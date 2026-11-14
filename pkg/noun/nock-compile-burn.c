/// @file
///
/// Body of the u3nc interpreter, _nc_burn().  Included by nock-compile.c
/// once per road direction, with _nc_mov_ws defined as -1 (north road:
/// the stack grows down) or 1 (south road: it grows up) and _nc_burn
/// defined as the name of the entry function, so that the stack direction
/// and the reference-counting fast paths are compile-time constants.
/// Not a standalone header.
///
/// The interpreter is tail-call threaded: every opcode is a function that
/// ends by tail-calling the next opcode's function through the dispatch
/// table, and the interpreter state travels in the argument registers.
/// The preserve_none convention keeps the state out of the callee-saved
/// registers' way, and musttail makes the dispatch a jump.  Each opcode is
/// then a separate register-allocation problem, so an edit to one cannot
/// change the code of another.
///
/// An op that reads slots comes in a variant per subset of them consumed
/// (see the bytecode description in nock-compile.c); the variants share a
/// body, instantiated with the consume flags as compile-time constants.

// IDE support
#include "allocate.h"
static_assert(1, "Hack to end the preamble");
#ifndef _nc_burn
#  include "nock-compile.c"
#  define _nc_mov_ws  (-999)
#  define _nc_burn    _nc_burn_ide
#endif

#if !__has_attribute(musttail) || !__has_attribute(preserve_none)
#  error "the u3nc interpreter needs clang's musttail and preserve_none"
#endif

//  _nc_mode: what an inclusion defines:
//
//    0: the interpreter: the opcode functions and _nc_burn(), their entry;
//    1: the bodies native code shares rather than copies, of the ops too
//       big to copy at every site (OP1F.., below);
//    2: the stencils of native code (nock-compile-jit.c): the body of each
//       opcode as a function of its own, global, in a section of its own,
//       whose operands and continuations are undefined symbols, the holes
//       the lowering fills in a copy of its machine code; and the pieces
//       it makes calls of (CALB.., at the end).  Compiled apart, never
//       linked.
//
//  Native code is more opcode functions: it runs with the interpreter's
//  state in the interpreter's registers, on the interpreter's frames, and
//  the two call each other with a jump.  A program starts as bytecode,
//  and gets native code when it is hot (cold, below).
//
#ifndef _nc_mode
#  define _nc_mode  0
#endif

#define _nc_cat_(a, b)  a##b
#define _nc_cat(a, b)   _nc_cat_(a, b)

//  OP(op): the function of an opcode, or of an internal entry point;
//  TAB: the dispatch table; WTAB: that of the _V widths, behind WIDE;
//  OP_F: the type of their entries
//
#define OP(op)  _nc_cat(_nc_burn, _##op)
#define TAB     _nc_cat(_nc_burn, _tab)
#define WTAB    _nc_cat(_nc_burn, _wtab)
#define OP_F    _nc_cat(_nc_burn, _f)

#define CONV      __attribute__((preserve_none))
#define MUSTTAIL  __attribute__((musttail))

//  the interpreter state: the next byte of the bytecode, the slots of the
//  running activation, its program, the road; then the operands of the
//  opcode, decoded by the width variants for the shared body.  The state
//  proper travels in callee-saved registers and survives calls out; the
//  operands don't, so a dispatch passes zeros rather than preserve them.
//
//  The functions take c3_w so that u3_noun fits into argument A for DONE(pro)
//
#define SIG(A, B, C)                                                            \
  (c3_y* ip, u3_noun* reg, u3nc_prog* pog_u, u3a_road* rod_u,                   \
   c3_w A, c3_w B, c3_w C)
#define ARGS  SIG(arg_a_w, arg_b_w, arg_c_w)
#define PASS  ip, reg, pog_u, rod_u, arg_a_w, arg_b_w, arg_c_w

typedef u3_noun (*OP_F)ARGS CONV;

//  BASE(op): the function op of the interpreter of this road direction,
//  for the native code that goes with it
//
#ifndef _nc_base
#  define _nc_base  _nc_burn
#endif
#define BASE(op)  _nc_cat(_nc_base, _##op)

#if 0 == _nc_mode
#  define NEXT  ip, reg, pog_u, rod_u, 0, 0, 0

#  define Y(op)  static u3_noun OP(op) ARGS CONV;
OPCODES
WIDES
#  undef Y

static u3_noun OP(done) ARGS CONV;

#  define Y(op)  OP(op),
static const OP_F TAB[]  = { OPCODES };
static const OP_F WTAB[] = { WIDES };
#  undef Y

#elif 1 == _nc_mode
#  define NEXT  ip, reg, pog_u, rod_u, 0, 0, 0

#else
//  a stencil passes nothing in the operand registers: und_w, a variable
//  it never sets
//
#  define NEXT  ip, reg, pog_u, rod_u, und_w, und_w, und_w
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wuninitialized"
#  pragma clang diagnostic ignored "-Wmissing-prototypes"

//  the holes: _nj_s0.._nj_s2, the operand of that index when it is a slot,
//  the address of the symbol its offset in bytes from reg; _nj_v0.._nj_v2,
//  when it is anything else, the address its value; _nj_next, the code of
//  the next op; _nj_jump, of the target of a branch
//
extern u3_noun _nj_s0[], _nj_s1[], _nj_s2[];
extern c3_y    _nj_v0[], _nj_v1[], _nj_v2[];
u3_noun _nj_next ARGS CONV;
u3_noun _nj_jump ARGS CONV;

#  define SLT(i)  ((c3_d)(uintptr_t)_nj_s##i / sizeof(u3_noun))
#  define VAL(i)  ((c3_d)(c3_h)(uintptr_t)_nj_v##i)
#  define HOLE(fam, i)  ( (1 == _nj_k##i##_##fam) ? SLT(i) : VAL(i) )
#endif

#define FRAME_T  nc_frame
#define FRAME_W  c3_wiseof(FRAME_T)

//  _nc_dir: the index of the road direction, for the native code of a program
//
#define _nc_dir  ((_nc_mov_ws < 0) ? 0 : 1)

#define RB()  (*ip++)
#define RS()  ({ c3_h _v = ip[0] | (ip[1] << 8); ip += 2; _v; })
//  RV(): a length byte, then that many bytes, four at most, little-endian:
//  read as one word and masked (the bytecode is padded for it,
//  _nc_prog_lay())
//
#define RV()  ({                                                                \
    c3_y _n = *ip++;                                                            \
    c3_h _v;                                                                    \
    memcpy(&_v, ip, sizeof(_v));                                                \
    ip += _n;                                                                   \
    _v & (c3_h)(0xffffffffULL >> (8 * (4 - _n)));                               \
  })

//  BURN(): dispatch the next opcode; JUMP(t): the next is at t.  In a
//  stencil: go to the code of the next op, or of the target (JUMP()
//  leaves, and the BURN() after it is dead).  In a body native code
//  shares: go back to the stencil that called it, which goes on.
//
#if 2 == _nc_mode
#  define BURN()   MUSTTAIL return _nj_next(NEXT)
#  define JUMP(t)  MUSTTAIL return _nj_jump(NEXT)
#elif 1 == _nc_mode
#  define BURN()   return (nj_run){ 0, 0 }
#else
#  define BURN()  do {                                                          \
    c3_y _op = *ip++;                                                           \
    MUSTTAIL return TAB[_op](NEXT);                                             \
  } while ( 0 )

#  define JUMP(t)  (ip = pog_u->byc_u.ops_y + (t))
#endif

#define PUSH(n)  _nc_push(rod_u, _nc_mov_ws, n)
#define POP(n)   _nc_pop(rod_u, _nc_mov_ws, n)
#define TOP(n)   _nc_top(rod_u, _nc_mov_ws, n)

//  Slots hold counted references, exactly while the register in them is
//  live (_nc_lives()):
//
//  PUT(d, v): the reference v into slot d, which is empty: the register
//             that held it was consumed or dropped.  An op's product may
//             share the slot of a source it consumes, so an op releases
//             its consumed sources before it writes.  The exception is a
//             block that only crashes, where the registers that died on
//             the edge in were not dropped (_nc_lives()): what they left
//             in the slots leaks into the bail, and the check lets it
//             (_nc_doomed())
//  TAKE(s, K): the noun in slot s, with a reference: the slot's own if K,
//              consuming it (the slot is zeroed), else a new one
//  TAKEN(v, s, K): the same for v, already read from slot s: an op reading
//                  two slots that may be the same reads both before it
//                  takes either
//  DROP(s, K): release slot s after a borrowing read of it, if K
//  VOID(s, K): the same, where the op knows a direct atom is in it
//  EMPTY(): when checking, assert that every slot of the activation is
//           zero: at a return or a tail call, they must be
//
#if defined(U3NC_CHECK) || defined(C3DBG)
#  define PUT(d, v)  do {                                                       \
    u3_noun _val = (v);    /*  first: it may consume the slot it goes to  */    \
    if ( reg[d] && !_nc_doomed(pog_u, ip) ) {                                   \
      fprintf(stderr, "u3nc: slot %"PRIc3_w" not empty, written by the op "     \
        "before %"PRIc3_w" of program %p\r\n", (c3_w)(d),                       \
          (c3_w)(ip - pog_u->byc_u.ops_y), (void*)pog_u);                       \
      _nc_print(pog_u);                                                         \
      u3_assert( 0 == reg[d] );                                                 \
    }                                                                           \
    reg[d] = _val;                                                              \
  } while ( 0 )
#  define EMPTY()  do {                                                         \
    for ( c3_h _i = 0; _i < pog_u->tot_h; _i++ ) {                              \
      u3_assert( 0 == reg[_i] );                                                \
    }                                                                           \
  } while ( 0 )
#else
#  define PUT(d, v)  (reg[d] = (v))
#  define EMPTY()    ((void)0)
#endif

//  ZERO(s): empty slot s, zeroing it if the build reads empty slots
//  (_nc_zer_t): for a sweep, or for the checks
//
#define ZERO(s)  do { if ( _nc_zer_t ) { reg[s] = 0; } } while ( 0 )

//  SWEEP(S): if S, drop whatever every slot of the activation still holds:
//  the registers that died on the edge into this block, at an op that
//  ends the activation
//
#define SWEEP(S)  do {                                                          \
    if ( S ) {                                                                  \
      for ( c3_h _i = 0; _i < pog_u->tot_h; _i++ ) {                            \
        u3_noun _v = reg[_i];                                                   \
        reg[_i] = 0;                                                            \
        LOSE(_v);                                                               \
      }                                                                         \
    }                                                                           \
  } while ( 0 )

#define TAKEN(v, s, K)  do {                                                    \
    if ( K ) { ZERO(s); } else { GAIN(v); }                                    \
  } while ( 0 )
#define TAKE(s, K)  ({ u3_noun _v = reg[s]; TAKEN(_v, s, K); _v; })
#define DROP(s, K)  do {                                                        \
    if ( K ) {                                                                  \
      u3_noun _v = reg[s];                                                      \
      ZERO(s);                                                                  \
      LOSE(_v);                                                                 \
    }                                                                           \
  } while ( 0 )

//  VOID(s, K): DROP(s, K) of a slot the op has seen a direct atom in:
//  nothing to release
//
#define VOID(s, K)  do { if ( K ) { ZERO(s); } } while ( 0 )

#if 2 == _nc_mode
//  a stencil: no widths to decode, the operands are holes, by their kind
//  (_nj_kin()), and the function is named as the opcode of its _B width
//
#define DECODE1(name)
#define DECODE2(name)
#define DECODE3(name)

#define BODY0(name, body)                                                       \
  u3_noun OP(name) ARGS CONV {                                                  \
    c3_w und_w;                                                                 \
    body()                                                                      \
  }

#define BODY1(name, fam, A, body, ...)                                          \
  u3_noun OP(name##_B) ARGS CONV {                                              \
    c3_w und_w;                                                                 \
    const c3_d A = HOLE(fam, 0);                                                \
    body(__VA_ARGS__)                                                           \
  }

#define BODY2(name, fam, A, B, body, ...)                                       \
  u3_noun OP(name##_B) ARGS CONV {                                              \
    c3_w und_w;                                                                 \
    const c3_d A = HOLE(fam, 0);                                                \
    const c3_d B = HOLE(fam, 1);                                                \
    body(__VA_ARGS__)                                                           \
  }

#define BODY3(name, fam, A, B, C, body, ...)                                    \
  u3_noun OP(name##_B) ARGS CONV {                                              \
    c3_w und_w;                                                                 \
    const c3_d A = HOLE(fam, 0);                                                \
    const c3_d B = HOLE(fam, 1);                                                \
    const c3_d C = HOLE(fam, 2);                                                \
    body(__VA_ARGS__)                                                           \
  }
#elif 1 == _nc_mode
//  no bodies of the ops that have stencils
//
#define DECODE1(name)
#define DECODE2(name)
#define DECODE3(name)
#define BODY0(name, body)
#define BODY1(name, fam, A, body, ...)
#define BODY2(name, fam, A, B, body, ...)
#define BODY3(name, fam, A, B, C, body, ...)
#else
//  DECODE1/2/3(name): the width variants name_B, name_S and name_V of an
//  opcode with one, two or three immediates, each decoding them into the
//  operand registers and tail-calling the shared body, name_in
//
#define DECODE1(name)                                                           \
  static u3_noun OP(name##_in) ARGS CONV;                                       \
  static u3_noun OP(name##_B) ARGS CONV {                                       \
    arg_a_w = RB();                                                             \
    MUSTTAIL return OP(name##_in)(PASS);                                        \
  }                                                                             \
  static u3_noun OP(name##_S) ARGS CONV {                                       \
    arg_a_w = RS();                                                             \
    MUSTTAIL return OP(name##_in)(PASS);                                        \
  }                                                                             \
  static u3_noun OP(name##_V) ARGS CONV {                                       \
    arg_a_w = RV();                                                             \
    MUSTTAIL return OP(name##_in)(PASS);                                        \
  }

#define DECODE2(name)                                                           \
  static u3_noun OP(name##_in) ARGS CONV;                                       \
  static u3_noun OP(name##_B) ARGS CONV {                                       \
    arg_a_w = RB(); arg_b_w = RB();                                             \
    MUSTTAIL return OP(name##_in)(PASS);                                        \
  }                                                                             \
  static u3_noun OP(name##_S) ARGS CONV {                                       \
    arg_a_w = RS(); arg_b_w = RS();                                             \
    MUSTTAIL return OP(name##_in)(PASS);                                        \
  }                                                                             \
  static u3_noun OP(name##_V) ARGS CONV {                                       \
    arg_a_w = RV(); arg_b_w = RV();                                             \
    MUSTTAIL return OP(name##_in)(PASS);                                        \
  }

#define DECODE3(name)                                                           \
  static u3_noun OP(name##_in) ARGS CONV;                                       \
  static u3_noun OP(name##_B) ARGS CONV {                                       \
    arg_a_w = RB(); arg_b_w = RB(); arg_c_w = RB();                             \
    MUSTTAIL return OP(name##_in)(PASS);                                        \
  }                                                                             \
  static u3_noun OP(name##_S) ARGS CONV {                                       \
    arg_a_w = RS(); arg_b_w = RS(); arg_c_w = RS();                             \
    MUSTTAIL return OP(name##_in)(PASS);                                        \
  }                                                                             \
  static u3_noun OP(name##_V) ARGS CONV {                                       \
    arg_a_w = RV(); arg_b_w = RV(); arg_c_w = RV();                             \
    MUSTTAIL return OP(name##_in)(PASS);                                        \
  }

//  BODY1/2/3(name, A.., body, ...): the body name_in of an opcode, naming
//  its operands, from the body macro applied to the consume constants
//  that follow it, if any.  A body ends in a dispatch: BURN(), DONE(), or
//  a call.  BODY0(name, body): an opcode with no operands is its own
//  body, there being one encoding of it.
//
#define BODY0(name, body)                                                       \
  static u3_noun OP(name) ARGS CONV {                                           \
    body()                                                                      \
  }

#define BODY1(name, fam, A, body, ...)                                          \
  static u3_noun OP(name##_in) ARGS CONV {                                      \
    const c3_h A = arg_a_w;                                                     \
    body(__VA_ARGS__)                                                           \
  }

#define BODY2(name, fam, A, B, body, ...)                                       \
  static u3_noun OP(name##_in) ARGS CONV {                                      \
    const c3_h A = arg_a_w;                                                     \
    const c3_h B = arg_b_w;                                                     \
    body(__VA_ARGS__)                                                           \
  }

#define BODY3(name, fam, A, B, C, body, ...)                                    \
  static u3_noun OP(name##_in) ARGS CONV {                                      \
    const c3_h A = arg_a_w;                                                     \
    const c3_h B = arg_b_w;                                                     \
    const c3_h C = arg_c_w;                                                     \
    body(__VA_ARGS__)                                                           \
  }

#endif

//  OP1/2/3(op, A.., body): an opcode with one, two or three operands and
//  no consume variants.  OP1K/2K/3K: with one consumable source, op_0
//  borrowing it and op_1 consuming it, the body taking K.  OP2KK/3KK:
//  with two, bit 0 of the variant for the first and bit 1 for the second,
//  the body taking KA and KB.  OP3K4: a branch on two sources, with two
//  bits per source for the edge it is consumed on, 0 and 1 for the first
//  and 2 and 3 for the second, the body taking KAY, KAT, KBY and KBT.
//
#define OP0(op, body)           BODY0(op, body)
#define OP1(op, A, body)        DECODE1(op) BODY1(op, op, A, body)
#define OP2(op, A, B, body)     DECODE2(op) BODY2(op, op, A, B, body)
#define OP3(op, A, B, C, body)  DECODE3(op) BODY3(op, op, A, B, C, body)

#define OP1K(op, A, body)                                                       \
  DECODE1(op##_0) BODY1(op##_0, op, A, body, 0)                                 \
  DECODE1(op##_1) BODY1(op##_1, op, A, body, 1)

#define OP2K(op, A, B, body)                                                    \
  DECODE2(op##_0) BODY2(op##_0, op, A, B, body, 0)                              \
  DECODE2(op##_1) BODY2(op##_1, op, A, B, body, 1)

#define OP3K(op, A, B, C, body)                                                 \
  DECODE3(op##_0) BODY3(op##_0, op, A, B, C, body, 0)                           \
  DECODE3(op##_1) BODY3(op##_1, op, A, B, C, body, 1)

#define OP2KK(op, A, B, body)                                                   \
  DECODE2(op##_0) BODY2(op##_0, op, A, B, body, 0, 0)                           \
  DECODE2(op##_1) BODY2(op##_1, op, A, B, body, 1, 0)                           \
  DECODE2(op##_2) BODY2(op##_2, op, A, B, body, 0, 1)                           \
  DECODE2(op##_3) BODY2(op##_3, op, A, B, body, 1, 1)

#define OP3KK(op, A, B, C, body)                                                \
  DECODE3(op##_0) BODY3(op##_0, op, A, B, C, body, 0, 0)                        \
  DECODE3(op##_1) BODY3(op##_1, op, A, B, C, body, 1, 0)                        \
  DECODE3(op##_2) BODY3(op##_2, op, A, B, C, body, 0, 1)                        \
  DECODE3(op##_3) BODY3(op##_3, op, A, B, C, body, 1, 1)

#define OP3K4(op, A, B, C, body)                                                \
  DECODE3(op##_0)  BODY3(op##_0, op,  A, B, C, body, 0, 0, 0, 0)                \
  DECODE3(op##_1)  BODY3(op##_1, op,  A, B, C, body, 1, 0, 0, 0)                \
  DECODE3(op##_2)  BODY3(op##_2, op,  A, B, C, body, 0, 1, 0, 0)                \
  DECODE3(op##_3)  BODY3(op##_3, op,  A, B, C, body, 1, 1, 0, 0)                \
  DECODE3(op##_4)  BODY3(op##_4, op,  A, B, C, body, 0, 0, 1, 0)                \
  DECODE3(op##_5)  BODY3(op##_5, op,  A, B, C, body, 1, 0, 1, 0)                \
  DECODE3(op##_6)  BODY3(op##_6, op,  A, B, C, body, 0, 1, 1, 0)                \
  DECODE3(op##_7)  BODY3(op##_7, op,  A, B, C, body, 1, 1, 1, 0)                \
  DECODE3(op##_8)  BODY3(op##_8, op,  A, B, C, body, 0, 0, 0, 1)                \
  DECODE3(op##_9)  BODY3(op##_9, op,  A, B, C, body, 1, 0, 0, 1)                \
  DECODE3(op##_10) BODY3(op##_10, op, A, B, C, body, 0, 1, 0, 1)                \
  DECODE3(op##_11) BODY3(op##_11, op, A, B, C, body, 1, 1, 0, 1)                \
  DECODE3(op##_12) BODY3(op##_12, op, A, B, C, body, 0, 0, 1, 1)                \
  DECODE3(op##_13) BODY3(op##_13, op, A, B, C, body, 1, 0, 1, 1)                \
  DECODE3(op##_14) BODY3(op##_14, op, A, B, C, body, 0, 1, 1, 1)                \
  DECODE3(op##_15) BODY3(op##_15, op, A, B, C, body, 1, 1, 1, 1)

//  OP1F.., OP1I.., OP1KT: an op whose native code is not a copy of its
//  body: the stencil calls the one body all the sites of the op share, by
//  the C convention, which saves the stencil's registers.  The body gets
//  the state and the operands, and where the stencil goes on (ret_v), for
//  a frame; it produces what to run next: a program on its slots, or
//  nothing.  F: an op that never enters a program: the stencil goes on.
//  I: one that may: the stencil enters it, or goes on.  T: one that ends
//  the activation: the stencil enters the program, or is done with the
//  product, which comes in place of the slots.  To the interpreter they
//  are ops as the others.
//
#if 0 == _nc_mode
#  define OP1F(op, A, body)          OP1(op, A, body)
#  define OP1I(op, A, body)          OP1(op, A, body)
#  define OP2KF(op, A, B, body)      OP2K(op, A, B, body)
#  define OP3KKF(op, A, B, C, body)  OP3KK(op, A, B, C, body)
#  define OP3KKI(op, A, B, C, body)  OP3KK(op, A, B, C, body)
#  define OP1KT(op, A, body)         OP1K(op, A, body)
#elif 1 == _nc_mode
#  define FAR1(name, A, body, ...)                                              \
  static nj_run OP(name##_far)(u3_noun* reg, u3nc_prog* pog_u,                  \
                                u3a_road* rod_u, c3_d A, void* ret_v)           \
  {                                                                             \
    c3_y* ip = 0;                                                               \
    body(__VA_ARGS__)                                                           \
  }
#  define FAR2(name, A, B, body, ...)                                           \
  static nj_run OP(name##_far)(u3_noun* reg, u3nc_prog* pog_u,                  \
                                u3a_road* rod_u, c3_d A, c3_d B, void* ret_v)   \
  {                                                                             \
    c3_y* ip = 0;                                                               \
    body(__VA_ARGS__)                                                           \
  }
#  define FAR3(name, A, B, C, body, ...)                                        \
  static nj_run OP(name##_far)(u3_noun* reg, u3nc_prog* pog_u,                  \
                                u3a_road* rod_u, c3_d A, c3_d B, c3_d C,        \
                                void* ret_v)                                    \
  {                                                                             \
    c3_y* ip = 0;                                                               \
    body(__VA_ARGS__)                                                           \
  }
#  define OP1F(op, A, body)  FAR1(op, A, body)
#  define OP1I(op, A, body)  FAR1(op, A, body)
#  define OP2KF(op, A, B, body)                                                 \
  FAR2(op##_0, A, B, body, 0) FAR2(op##_1, A, B, body, 1)
#  define OP3KKF(op, A, B, C, body)                                             \
  FAR3(op##_0, A, B, C, body, 0, 0) FAR3(op##_1, A, B, C, body, 1, 0)           \
  FAR3(op##_2, A, B, C, body, 0, 1) FAR3(op##_3, A, B, C, body, 1, 1)
#  define OP3KKI(op, A, B, C, body)  OP3KKF(op, A, B, C, body)
#  define OP1KT(op, A, body)                                                    \
  FAR1(op##_0, A, body, 0) FAR1(op##_1, A, body, 1)
#else
//  GO(wha): run the program a shared body produced, on its slots
//
#  define GO(wha)  do {                                                         \
    pog_u = (wha).gop_u;                                                        \
    reg   = (wha).nex;                                                          \
    MUSTTAIL return ((OP_F)pog_u->nat_f[_nc_dir])(NEXT);                        \
  } while ( 0 )

#  define FAR1(name)                                                            \
  nj_run OP(name##_far)(u3_noun*, u3nc_prog*, u3a_road*, c3_d, void*);
#  define FAR2(name)                                                            \
  nj_run OP(name##_far)(u3_noun*, u3nc_prog*, u3a_road*, c3_d, c3_d, void*);
#  define FAR3(name)                                                            \
  nj_run OP(name##_far)(u3_noun*, u3nc_prog*, u3a_road*, c3_d, c3_d, c3_d,      \
                         void*);

#  define STF1(name)  FAR1(name)                                                \
  u3_noun OP(name##_B) ARGS CONV {                                              \
    c3_w und_w; void* und_v;                                                    \
    (void)OP(name##_far)(reg, pog_u, rod_u, VAL(0), und_v);                     \
    BURN();                                                                     \
  }
#  define STF2(name)  FAR2(name)                                                \
  u3_noun OP(name##_B) ARGS CONV {                                              \
    c3_w und_w; void* und_v;                                                    \
    (void)OP(name##_far)(reg, pog_u, rod_u, VAL(0), VAL(1), und_v);             \
    BURN();                                                                     \
  }
#  define STF3(name)  FAR3(name)                                                \
  u3_noun OP(name##_B) ARGS CONV {                                              \
    c3_w und_w; void* und_v;                                                    \
    (void)OP(name##_far)(reg, pog_u, rod_u, VAL(0), VAL(1), VAL(2), und_v);     \
    BURN();                                                                     \
  }
#  define STI1(name)  FAR1(name)                                                \
  u3_noun OP(name##_B) ARGS CONV {                                              \
    c3_w    und_w;                                                              \
    nj_run nex_u = OP(name##_far)(reg, pog_u, rod_u, VAL(0),                    \
                                   (void*)_nj_next);                            \
    if ( nex_u.gop_u ) { GO(nex_u); }                                           \
    BURN();                                                                     \
  }
#  define STI3(name)  FAR3(name)                                                \
  u3_noun OP(name##_B) ARGS CONV {                                              \
    c3_w    und_w;                                                              \
    nj_run nex_u = OP(name##_far)(reg, pog_u, rod_u, VAL(0), VAL(1), VAL(2),    \
                                   (void*)_nj_next);                            \
    if ( nex_u.gop_u ) { GO(nex_u); }                                           \
    BURN();                                                                     \
  }
#  define STT1(name)  FAR1(name)                                                \
  u3_noun OP(name##_B) ARGS CONV {                                              \
    c3_w    und_w; void* und_v;                                                 \
    nj_run nex_u = OP(name##_far)(reg, pog_u, rod_u, VAL(0), und_v);            \
    if ( nex_u.gop_u ) { GO(nex_u); }                                           \
    MUSTTAIL return BASE(done)(ip, reg, pog_u, rod_u,                           \
                               (u3_noun)(uintptr_t)nex_u.nex, und_w, und_w);    \
  }

#  define OP1F(op, A, body)          STF1(op)
#  define OP1I(op, A, body)          STI1(op)
#  define OP2KF(op, A, B, body)      STF2(op##_0) STF2(op##_1)
#  define OP3KKF(op, A, B, C, body)                                             \
  STF3(op##_0) STF3(op##_1) STF3(op##_2) STF3(op##_3)
#  define OP3KKI(op, A, B, C, body)                                             \
  STI3(op##_0) STI3(op##_1) STI3(op##_2) STI3(op##_3)
#  define OP1KT(op, A, body)         STT1(op##_0) STT1(op##_1)
#endif

//  FRAME(des): push a frame recording this activation, its product to
//              slot des; produces the frame.  It says where the activation
//              goes on: in the bytecode, or in native code (ret_f)
//
#if 2 != _nc_mode
#ifndef U3NC_JIT
#  define FRAME_BACK(f)  ((f)->ip_h = ip - pog_u->byc_u.ops_y)
#elif 0 == _nc_mode
#  define FRAME_BACK(f)                                                         \
     ((f)->ret_f = 0, (f)->ip_h = ip - pog_u->byc_u.ops_y)
#else
#  define FRAME_BACK(f)  ((f)->ret_f = ret_v)
#endif

#define FRAME(des)  ({                                                          \
    FRAME_T* _f = PUSH(FRAME_W);                                                \
    _f->pog_u = pog_u;                                                          \
    _f->reg   = reg;                                                            \
    FRAME_BACK(_f);                                                             \
    _f->des_h = (des);                                                          \
    _f->key   = u3_none;                                                        \
    _f->cid_h = 0;                                                              \
    _nc_stat_frame(_f);                                                         \
    _f;                                                                         \
  })
#endif

//  SITE(): the call site a_h, its argument slots sot_h, their consume
//          flags kon_y, and their number len_h
//
#define SITE()  do {                                                            \
    dir_u = &(pog_u->dir_u.dat_u[a_h]);                                         \
    sot_h = pog_u->sot_u.sot_h + dir_u->sot_h;                                  \
    kon_y = pog_u->sot_u.kon_y + dir_u->sot_h;                                  \
    len_h = dir_u->len_h;                                                       \
  } while ( 0 )

//  SUB(): the slot of the one argument of the site dir_u, the subject of
//         a call by subject; KON(): whether the call consumes it
//
#define SUB()  (pog_u->sot_u.sot_h[dir_u->sot_h])
#define KON()  (pog_u->sot_u.kon_y[dir_u->sot_h])

//  CALLEE(): the program gop_u of the site dir_u, linked on the first call
//
#define CALLEE()  do {                                                          \
    if ( dir_u->pog_p ) {                                                       \
      gop_u = u3to(u3nc_prog, dir_u->pog_p);                                    \
    }                                                                           \
    else {                                                                      \
      u3t_off(noc_o);                                                           \
      gop_u = _nc_callee(pog_u, dir_u);                                         \
      u3t_on(noc_o);                                                            \
    }                                                                           \
  } while ( 0 )

//  GATHER(num): push num words and copy the site's arguments into them,
//               uncounted
//
#define GATHER(num)  do {                                                       \
    nex = PUSH(num);                                                            \
    for ( c3_h _i = 0; _i < len_h; _i++ ) {                                     \
      nex[_i] = reg[sot_h[_i]];                                                 \
    }                                                                           \
  } while ( 0 )

//  MOVE(): the gathered arguments move to the callee: a consumed one
//          takes its slot's reference, a borrowed one gains a new one
//
#define MOVE()  do {                                                            \
    for ( c3_h _i = 0; _i < len_h; _i++ ) {                                     \
      if ( kon_y[_i] ) { ZERO(sot_h[_i]); } else { GAIN(nex[_i]); }             \
    }                                                                           \
  } while ( 0 )

//  LEAVE(): release the consumed arguments of the site, after a jet that
//           retained them
//
#define LEAVE()  do {                                                           \
    for ( c3_h _i = 0; _i < len_h; _i++ ) {                                     \
      if ( kon_y[_i] ) {                                                        \
        u3_noun _v = reg[sot_h[_i]];                                            \
        ZERO(sot_h[_i]);                                                        \
        LOSE(_v);                                                               \
      }                                                                         \
    }                                                                           \
  } while ( 0 )

//  GIVE(): the consumed arguments of the site leave their slots: their
//          references go with the gathered copies, to a transferring jet
//
#define GIVE()  do {                                                            \
    if ( _nc_zer_t ) {                                                          \
      for ( c3_h _i = 0; _i < len_h; _i++ ) {                                   \
        if ( kon_y[_i] ) { reg[sot_h[_i]] = 0; }                                \
      }                                                                         \
    }                                                                           \
  } while ( 0 )

//  ENTER(len): run pog_u, its len arguments in its slots at reg
//
#ifdef U3NC_STAT
#  define _nc_stat_pog(fel)  (pog_u->fel++)
#else
#  define _nc_stat_pog(fel)  ((void)0)
#endif

//  _nc_tsc_beg(), _nc_tsc_end(fel): TSC cycles of a stretch into a
//  counter (U3NC_STAT, x86_64)
//
#if defined(U3NC_STAT) && defined(__x86_64__)
#  include <x86intrin.h>
#  define _nc_tsc_beg()     c3_d _tsc_d = __rdtsc()
#  define _nc_tsc_end(fel)  (u3nc_Stat.fel += __rdtsc() - _tsc_d)
//  JT(x): x, a call of a jet, its cycles counted (nested ones twice)
#  define JT(x)  ({ c3_d _jt_d = __rdtsc(); u3_noun _jr = (x);                  \
                    u3nc_Stat.jet_c += __rdtsc() - _jt_d; _jr; })
#else
#  define _nc_tsc_beg()     ((void)0)
#  define _nc_tsc_end(fel)  ((void)0)
#  define JT(x)  (x)
#endif

//  START(): run pog_u from its first op.  With native code in the build:
//  through nat_f, the program's native code or the stub that runs its
//  bytecode (cold); a shared body leaves it to its stencil.
//  AGAIN(): the same for the running program, a loop
//
#if 1 == _nc_mode
#  define START()  return (nj_run){ pog_u, reg }
#elif defined(U3NC_JIT)
#  define START()  MUSTTAIL return ((OP_F)pog_u->nat_f[_nc_dir])(NEXT)
#else
#  define START()  do { ip = pog_u->byc_u.ops_y; BURN(); } while ( 0 )
#endif
#define AGAIN()  START()

//  REST(gop, nex, len): zero the slots of gop at nex past its len
//  arguments, if the build reads empty slots
//
#define REST(gop, nex, len)  do {                                               \
    if ( _nc_zer_t ) {                                                          \
      for ( c3_h _i = (len); _i < (gop)->tot_h; _i++ ) {                        \
        (nex)[_i] = 0;                                                          \
      }                                                                         \
    }                                                                           \
  } while ( 0 )

#define ENTER(len)  do {                                                        \
    _nc_stat_pog(ent_d);                                                        \
    REST(pog_u, reg, len);                                                      \
    _nc_stat_spa_set(0);                                                        \
    START();                                                                    \
  } while ( 0 )

//  INVOKE(gop, nex, len, des): call gop on its len arguments, in its slots
//    at nex, its product to slot des of this activation, which goes on
//    after it;
//  INVOKE_MEMO(gop, nex, len, des, kee, cid): the same, the product saved
//    under the key kee in the memo cache cid
//
#define INVOKE(gop, nex, len, des)  do {                                        \
    (void)FRAME(des);                                                           \
    pog_u = (gop);                                                              \
    reg   = (nex);                                                              \
    ENTER(len);                                                                 \
  } while ( 0 )

#define INVOKE_MEMO(gop, nex, len, des, kee, cid)  do {                         \
    FRAME_T* _fam = FRAME(des);                                                 \
    _fam->key   = (kee);                                                        \
    _fam->cid_h = (cid);                                                        \
    pog_u = (gop);                                                              \
    reg   = (nex);                                                              \
    ENTER(len);                                                                 \
  } while ( 0 )

//  CALL_MONO(gop, arg, des): call gop on its one argument arg, transferred,
//                       its product to slot des
//
#define CALL_MONO(gop, arg, des)  do {                                          \
    u3_noun* _nex = PUSH((gop)->tot_h);                                         \
    _nex[0] = (arg);                                                            \
    INVOKE(gop, _nex, 1, des);                                                  \
  } while ( 0 )

//  QUIT(): this activation, its slots all empty, makes way for one that
//          returns in its place: its frame comes off;
//  REJOIN(): and goes back over the slots of the new one
//
#define QUIT()                                                                  \
    FRAME_T _fam = *(FRAME_T*)TOP(FRAME_W);                                     \
    POP(FRAME_W + pog_u->tot_h)
#define REJOIN()  (*(FRAME_T*)PUSH(FRAME_W) = _fam)

//  TAIL_MONO(gop, arg): call gop on its one argument arg, transferred, in
//                  place of this activation, whose slots are all empty
//
#define TAIL_MONO(gop, arg)  do {                                               \
    EMPTY();                                                                    \
    QUIT();                                                                     \
    pog_u  = (gop);                                                             \
    reg    = PUSH(pog_u->tot_h);                                                \
    reg[0] = (arg);                                                             \
    REJOIN();                                                                   \
    ENTER(1);                                                                   \
  } while ( 0 )

//  DONE(pro): return pro, transferred, from this activation.  A shared
//  body leaves it to its stencil, which goes to the interpreter's done
//
#if 2 == _nc_mode
//  back to native code that takes no memo of the product, from here: a
//  jump of this site's, which predicts better than one all sites share
//
#  define DONE(pro)  do {                                                       \
    u3_noun  _pro = (pro);                                                      \
    FRAME_T* _fam = TOP(FRAME_W);                                               \
    if ( _fam->ret_f && (u3_none == _fam->key) ) {                              \
      POP(FRAME_W + pog_u->tot_h);                                              \
      pog_u = _fam->pog_u;                                                      \
      reg   = _fam->reg;                                                        \
      reg[_fam->des_h] = _pro;                                                  \
      MUSTTAIL return ((OP_F)_fam->ret_f)(NEXT);                                \
    }                                                                           \
    MUSTTAIL return BASE(done)(ip, reg, pog_u, rod_u, _pro, und_w, und_w);      \
  } while ( 0 )
#elif 1 == _nc_mode
#  define DONE(pro)  return (nj_run){ 0, (u3_noun*)(uintptr_t)(pro) }
#else
#  define DONE(pro)                                                             \
     MUSTTAIL return OP(done)(ip, reg, pog_u, rod_u, (pro), 0, 0)
#endif

/* _nc_burn(): run a program on its arguments.  TRANSFERS the arguments.
**
**   An activation is the callee's slots, then a frame holding the
**   caller's state: a call site copies its arguments straight into the
**   slots of the callee's new activation, consumed or gained.  A total
**   array jet gets its arguments copied above the activation instead,
**   uncounted, and leaves no other trace; a transferring jet gets them
**   the same way, with the consume flags of the site, and the consumed
**   slots are emptied as it takes their references.  A tail call copies its
**   arguments to a temporary above its own activation, which is empty by
**   then (every other slot was consumed or dropped, or the call sweeps
**   them), and moves the temporary down in its place, under the same
**   frame.  A tail call of the running program (JMS, a loop) moves them
**   back into its parameter slots and keeps the activation; the compiler
**   emits it sweeping only, lowering the others to moves and a jump to
**   the entry.
*/
#if 0 == _nc_mode
static u3_noun
_nc_burn(u3nc_prog* pog_u, u3_noun* arg, c3_h len_h)
{
  u3a_road* rod_u = u3R;
  u3_post   emp_p = rod_u->cap_p;
  u3_noun*  reg   = PUSH(pog_u->tot_h);
  FRAME_T*  fam_u;
  c3_y*     ip;
  c3_y      op_y;
  c3_h      i_h;
  u3_noun   pro;

  for ( i_h = 0; i_h < len_h; i_h++ ) {
    reg[i_h] = arg[i_h];
  }
  if ( _nc_zer_t ) {
    for ( i_h = len_h; i_h < pog_u->tot_h; i_h++ ) {
      reg[i_h] = 0;
    }
  }

  fam_u = PUSH(FRAME_W);
  fam_u->pog_u = NULL;
#ifdef U3NC_JIT
  fam_u->ret_f = 0;
#endif
  _nc_stat_frame(fam_u);
  _nc_stat_pog(ent_d);
  _nc_stat_spa_set(0);

#ifdef U3NC_JIT
  (void)ip; (void)op_y;
  pro  = ((OP_F)pog_u->nat_f[_nc_dir])(0, reg, pog_u, rod_u, 0, 0, 0);
#else
  ip   = pog_u->byc_u.ops_y;
  op_y = *ip++;
  pro  = TAB[op_y](ip, reg, pog_u, rod_u, 0, 0, 0);
#endif

  u3_assert( emp_p == rod_u->cap_p );
  return pro;
}

//  done: arg_a_h is the product, transferred; the slots are all empty
//
#ifdef U3NC_STENCIL
__attribute__((noinline))
#endif
static u3_noun
OP(done) ARGS CONV
{
  u3_noun   pro = arg_a_w;
  FRAME_T*  fam_u;
  c3_h      d_h;

  EMPTY();

  fam_u = TOP(FRAME_W);
  _nc_stat_unframe(fam_u);

  if ( !fam_u->pog_u ) {
    POP(FRAME_W + pog_u->tot_h);
    return pro;
  }

  if ( u3_none != fam_u->key ) {
    _nc_tsc_beg();
    pro = _nc_save(fam_u->cid_h, fam_u->key, pro);
    _nc_tsc_end(mes_c);
    _nc_stat(mes_d);
  }

  POP(FRAME_W + pog_u->tot_h);
  pog_u = fam_u->pog_u;
  reg   = fam_u->reg;
  d_h   = fam_u->des_h;
#ifdef U3NC_JIT
  //  the caller is native code
  //
  if ( fam_u->ret_f ) {
    reg[d_h] = pro;
    MUSTTAIL return ((OP_F)fam_u->ret_f)(NEXT);
  }
#endif
  ip    = pog_u->byc_u.ops_y + fam_u->ip_h;
  PUT(d_h, pro);
  BURN();
}

#ifdef U3NC_JIT
//  cold: what a program without native code for this road direction runs
//  as: its bytecode, until it is hot: it has started u3nc_Hot times within
//  one stretch of the clock (_nj_tick(); a start is a call, or a turn of a
//  loop that is a tail call of itself); then native code, made here.
//  Anything enters a program through nat_f, so the activation the count is
//  reached on is the first to run natively.
//
static u3_noun
OP(cold) ARGS CONV
{
  if ( pog_u->epo_h != _nj_epo_h ) {
    pog_u->epo_h = _nj_epo_h;
    pog_u->hot_h = 0;
  }
  if ( u3nc_Jit && (++pog_u->hot_h >= u3nc_Hot) ) {
    _nj_lower(pog_u, _nc_dir);
    MUSTTAIL return ((OP_F)pog_u->nat_f[_nc_dir])(PASS);
  }
  if ( !(++_nj_tic_h & 0xfff) ) {
    _nj_tick();
  }

  ip = pog_u->byc_u.ops_y;
  BURN();
}
#endif
#endif


#if 0 == _nc_mode
/* _nc_burn_check(): the product of the array driver of the site dir_u on
**   its arguments nex, tested against the unary program of its bell on the
**   core rebuilt from them, as _cj_kick_f() tests a u3w driver: the arm
**   trusted while the driver runs, lest it test itself, and off while the
**   program runs, lest the program use it (the checked sites in it read
**   the flags); a mismatch turns the arm off and fails.  RETAINS nex.
*/
static u3_noun
_nc_cat(_nc_burn, _check)(u3nc_dire* dir_u, u3_noun* nex, c3_h len_h)
{
  u3j_harm*  ham_u = dir_u->ham_u;
  u3nc_prog* gop_u;
  u3_noun    cor, pro, ame;

  ham_u->ice = c3y;
  pro = JT(dir_u->arm_u->arg_f(nex));
  ham_u->ice = c3n;

  cor   = _nc_core(dir_u, nex);
  gop_u = _nc_mono(dir_u);

  ham_u->liv = c3n;
  ame = _nc_burn(gop_u, &cor, 1);
  ham_u->liv = c3y;

  if ( c3n == u3r_sing(ame, pro) ) {
    u3l_log("test: %s %s: mismatch: good %x, bad %x",
            ham_u->cop_u->cos_c,
            (!strcmp(".2", ham_u->fcs_c)) ? "$" : ham_u->fcs_c,
            u3r_mug(ame),
            u3r_mug(pro));
    ham_u->liv = c3n;
    return u3m_bail(c3__fail);
  }

  u3z(ame);
  return pro;
}
#endif

#define BODY_IMM()                                                              \
  PUT(d_h, a_h);                                                                \
  BURN();
OP2(IMM, a_h, d_h, BODY_IMM)

#define BODY_IML()                                                              \
  PUT(d_h, GAIN(pog_u->lit_u.non[a_h]));                                        \
  BURN();
OP2(IML, a_h, d_h, BODY_IML)

#define BODY_MOV(K)                                                             \
  PUT(d_h, TAKE(a_h, K));                                                       \
  BURN();
OP2K(MOV, a_h, d_h, BODY_MOV)

#define BODY_INC(K)                                                             \
  PUT(d_h, u3i_vint(TAKE(a_h, K)));                                             \
  BURN();
OP2K(INC, a_h, d_h, BODY_INC)

//  jetted calls the interpreter knows: the jets retain their arguments
//
#define BODY_DEC(K)                                                             \
  u3_noun x = reg[a_h], pro;                                                    \
  _nc_stat(jet_d);                                                              \
  if ( c3n == u3ud(x) ) {                                                       \
    u3m_bail(c3__fail);                                                         \
  }                                                                             \
  pro = u3qa_dec(x);                                                            \
  DROP(a_h, K);                                                                 \
  PUT(d_h, pro);                                                                \
  BURN();
OP2K(DEC, a_h, d_h, BODY_DEC)

#define BODY_ADD(KA, KB)                                                        \
  u3_noun x = reg[a_h], y = reg[b_h], pro;                                      \
  _nc_stat(jet_d);                                                              \
  if ( (c3n == u3ud(x)) || (c3n == u3ud(y)) ) {                                 \
    u3m_bail(c3__fail);                                                         \
  }                                                                             \
  pro = u3qa_add(x, y);                                                         \
  DROP(a_h, KA);                                                                \
  DROP(b_h, KB);                                                                \
  PUT(d_h, pro);                                                                \
  BURN();
OP3KK(ADD, a_h, b_h, d_h, BODY_ADD)

#define BODY_CON(KA, KB)                                                        \
  u3_noun h = reg[a_h], t = reg[b_h];                                           \
  TAKEN(h, a_h, KA);                                                            \
  TAKEN(t, b_h, KB);                                                            \
  _nc_stat(con_d);                                                              \
  _nc_stat_con();                                                               \
  _nc_stat_pog(con_d);                                                          \
  PUT(d_h, u3nc(h, t));                                                         \
  BURN();
OP3KK(CON, a_h, b_h, d_h, BODY_CON)

//  CELL(x): x as the cell it is.  HED and TAL don't check: the IR
//  deconses a noun only past a CEL on it or on the yes edge of a CLQ
//  (+kern-need and +mono-try-call in nock-compilation.hoon), and no pass
//  of either compiler unguards them.  The checking build asserts it.
//
#if defined(U3NC_CHECK) || defined(C3DBG)
#  define CELL(x)  ({                                                           \
    u3_noun _x = (x);                                                           \
    u3_assert( c3y == u3du(_x) );                                               \
    (u3a_cell*)u3a_to_ptr(_x);                                                  \
  })
#else
#  define CELL(x)  ((u3a_cell*)u3a_to_ptr(x))
#endif

//  UNIQUE(x, K): for the statistics, whether consuming the cell x frees it
//
#define UNIQUE(x, K)                                                            \
  (  (K) && (c3y == _nc_normal(rod_u, x))                                       \
  && (1 == ((u3a_noun*)u3a_to_ptr(x))->use_w) )

#define BODY_HED(K)                                                             \
  u3_noun x = reg[a_h];                                                         \
  u3_noun h = GAIN(CELL(x)->hed);                                               \
  _nc_stat_if(K, dek_d);                                                        \
  _nc_stat_if(!(K), deb_d);                                                     \
  _nc_stat_uni(UNIQUE(x, K));                                                   \
  _nc_stat_ali(pog_u, ip, h, K);                                                \
  DROP(a_h, K);                                                                 \
  PUT(d_h, h);                                                                  \
  BURN();
OP2K(HED, a_h, d_h, BODY_HED)

#define BODY_TAL(K)                                                             \
  u3_noun x = reg[a_h];                                                         \
  u3_noun t = GAIN(CELL(x)->tel);                                               \
  _nc_stat_if(K, dek_d);                                                        \
  _nc_stat_if(!(K), deb_d);                                                     \
  _nc_stat_uni(UNIQUE(x, K));                                                   \
  _nc_stat_ali(pog_u, ip, t, K);                                                \
  DROP(a_h, K);                                                                 \
  PUT(d_h, t);                                                                  \
  BURN();
OP2K(TAL, a_h, d_h, BODY_TAL)

//  REUSE(x): whether a consuming HER or TAR may keep the cell x for its
//  COR: the slot's reference is the only one, and the cell is on this
//  road, not the home road, whose hash-cons set may hold it
//
#define REUSE(x)                                                                \
  (  (c3y == _nc_normal(rod_u, x))                                              \
  && (1 == ((u3a_noun*)u3a_to_ptr(x))->use_w)                                   \
  && (rod_u != &(u3H->rod_u)) )

//  HER and TAR: a consuming HED or TAL paired with a COR (_nc_reuse()).
//  The cell, when it would be freed, goes on the stack instead, as the
//  token the COR pops: it gives the product its reference to the part
//  taken and loses the other part.  Shared, it is read and released as
//  by HED_1 or TAL_1, and the token is 0.
//
#define BODY_HER()                                                              \
  u3_noun   x     = reg[a_h], tok, h;                                           \
  u3a_cell* cel_u = CELL(x);                                                    \
  ZERO(a_h);                                                                    \
  _nc_stat(dek_d);                                                              \
  _nc_stat(tok_d);                                                              \
  if ( REUSE(x) ) {                                                             \
    _nc_stat(uni_d);                                                            \
    tok = x;                                                                    \
    h   = cel_u->hed;                                                           \
    LOSE(cel_u->tel);                                                           \
  }                                                                             \
  else {                                                                        \
    tok = 0;                                                                    \
    h   = GAIN(cel_u->hed);                                                     \
    LOSE(x);                                                                    \
  }                                                                             \
  *(u3_noun*)PUSH(1) = tok;                                                     \
  PUT(d_h, h);                                                                  \
  BURN();
OP2(HER, a_h, d_h, BODY_HER)

#define BODY_TAR()                                                              \
  u3_noun   x     = reg[a_h], tok, t;                                           \
  u3a_cell* cel_u = CELL(x);                                                    \
  ZERO(a_h);                                                                    \
  _nc_stat(dek_d);                                                              \
  _nc_stat(tok_d);                                                              \
  if ( REUSE(x) ) {                                                             \
    _nc_stat(uni_d);                                                            \
    tok = x;                                                                    \
    t   = cel_u->tel;                                                           \
    LOSE(cel_u->hed);                                                           \
  }                                                                             \
  else {                                                                        \
    tok = 0;                                                                    \
    t   = GAIN(cel_u->tel);                                                     \
    LOSE(x);                                                                    \
  }                                                                             \
  *(u3_noun*)PUSH(1) = tok;                                                     \
  PUT(d_h, t);                                                                  \
  BURN();
OP2(TAR, a_h, d_h, BODY_TAR)

//  COR: CON into the cell its HER or TAR pushed, if it pushed one
//
#define BODY_COR(KA, KB)                                                        \
  u3_noun h   = reg[a_h], t = reg[b_h];                                         \
  u3_noun tok = *(u3_noun*)TOP(1);                                              \
  POP(1);                                                                       \
  TAKEN(h, a_h, KA);                                                            \
  TAKEN(t, b_h, KB);                                                            \
  _nc_stat(con_d);                                                              \
  _nc_stat_con();                                                               \
  _nc_stat_pog(con_d);                                                          \
  if ( tok ) {                                                                  \
    u3a_cell* cel_u = (u3a_cell*)u3a_to_ptr(tok);                               \
    _nc_stat(rus_d);                                                            \
    cel_u->mug_w = 0;                                                           \
    cel_u->hed   = h;                                                           \
    cel_u->tel   = t;                                                           \
    PUT(d_h, tok);                                                              \
  }                                                                             \
  else {                                                                        \
    PUT(d_h, u3nc(h, t));                                                       \
  }                                                                             \
  BURN();
OP3KK(COR, a_h, b_h, d_h, BODY_COR)

#define BODY_CEL(K)                                                             \
  if ( c3n == u3du(reg[a_h]) ) {                                                \
    u3m_bail(c3__exit);                                                         \
  }                                                                             \
  DROP(a_h, K);                                                                 \
  BURN();
OP1K(CEL, a_h, BODY_CEL)

#define BODY_LOB(K)                                                             \
  if ( reg[a_h] > 1 ) {                                                         \
    u3m_bail(c3__exit);                                                         \
  }                                                                             \
  VOID(a_h, K);                                                                 \
  BURN();
OP1K(LOB, a_h, BODY_LOB)

#define BODY_EQU(KA, KB)                                                        \
  (void)u3r_sing(reg[a_h], reg[b_h]);                                           \
  DROP(a_h, KA);                                                                \
  DROP(b_h, KB);                                                                \
  BURN();
OP2KK(EQU, a_h, b_h, BODY_EQU)

#define BODY_HSP()                                                              \
  u3_noun x = _nc_hilt_fore(pog_u->lit_u.non[a_h]);                             \
  *(u3_noun*)PUSH(1) = x;                                                       \
  BURN();
OP1F(HSP, a_h, BODY_HSP)

#define BODY_HSE()                                                              \
  u3_noun x = *(u3_noun*)TOP(1);                                                \
  POP(1);                                                                       \
  _nc_hilt_hind(pog_u->lit_u.non[a_h], x);                                      \
  BURN();
OP1F(HSE, a_h, BODY_HSE)

#define BODY_HDP(K)                                                             \
  u3_noun x = _nc_hint_fore(pog_u->lit_u.non[a_h], TAKE(b_h, K));               \
  *(u3_noun*)PUSH(1) = x;                                                       \
  BURN();
OP2KF(HDP, a_h, b_h, BODY_HDP)

#define BODY_HDE(K)                                                             \
  u3_noun x = *(u3_noun*)TOP(1);                                                \
  POP(1);                                                                       \
  _nc_hint_hind(pog_u->lit_u.non[a_h], x);                                      \
  DROP(b_h, K);                                                                 \
  BURN();
OP2KF(HDE, a_h, b_h, BODY_HDE)

//  a trace hint whose clue is a constant: push the literal [tag clue];
//  the epilogue is an HDE, which drops the clue's register
//
#define BODY_HLP()                                                              \
  u3t_push(GAIN(pog_u->lit_u.non[a_h]));                                        \
  *(u3_noun*)PUSH(1) = u3_nul;                                                  \
  BURN();
OP1F(HLP, a_h, BODY_HLP)

#define BODY_HMP()                                                              \
  u3nc_dire* dir_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun    lis = u3_nul;                                                      \
  SITE();                                                                       \
  for ( c3_h _i = len_h; _i-- > 0; ) {                                          \
    u3_noun _v = reg[sot_h[_i]];                                                \
    TAKEN(_v, sot_h[_i], kon_y[_i]);                                            \
    lis = u3nc(_v, lis);                                                        \
  }                                                                             \
  u3t_push(u3nt(c3__meaf, GAIN(dir_u->bell), lis));                             \
  BURN();
OP1F(HMP, a_h, BODY_HMP)

#define BODY_HME()                                                              \
  u3t_drop();                                                                   \
  BURN();
OP0(HME, BODY_HME)

//  a constant %mean: the known product of the arm, literal a_h
//
#define BODY_HCP()                                                              \
  u3t_push(u3nc(c3__meac, GAIN(pog_u->lit_u.non[a_h])));                        \
  BURN();
OP1F(HCP, a_h, BODY_HCP)

#define BODY_HCE()                                                              \
  u3t_drop();                                                                   \
  BURN();
OP0(HCE, BODY_HCE)

#define BODY_SPY(KA, KB)                                                        \
  u3_noun x = u3m_soft_esc(GAIN(reg[a_h]), GAIN(reg[b_h])), pro;                \
  if ( c3n == u3du(x) ) {                                                       \
    u3m_bail(u3nc(1, GAIN(reg[b_h])));                                          \
  }                                                                             \
  else if ( c3n == u3du(u3t(x)) ) {                                             \
    u3t_push(u3nt(c3__hunk, GAIN(reg[a_h]), GAIN(reg[b_h])));                   \
    u3m_bail(c3__exit);                                                         \
  }                                                                             \
  pro = GAIN(u3t(u3t(x)));                                                      \
  LOSE(x);                                                                      \
  DROP(a_h, KA);                                                                \
  DROP(b_h, KB);                                                                \
  PUT(d_h, pro);                                                                \
  BURN();
OP3KKF(SPY, a_h, b_h, d_h, BODY_SPY)

#define BODY_NOK(KA, KB)                                                        \
  u3nc_prog* gop_u;                                                             \
  u3_noun    x = reg[a_h];                                                      \
  _nc_tsc_beg();                                                                \
  u3t_off(noc_o);                                                               \
  gop_u = _nc_entry(x, reg[b_h]);                                               \
  u3t_on(noc_o);                                                                \
  _nc_tsc_end(nok_c);                                                           \
  _nc_stat(nok_d);                                                              \
  _nc_stat_if(_nc_here(pog_u, u3R), nkc_d);                                     \
  _nc_stat_if(_nc_home(reg[b_h]), nkf_d);                                       \
  _nc_stat_if(_nc_home(reg[b_h]) && (c3y == u3du(x)) && (c3y == u3du(u3t(x)))   \
              && _nc_home(u3t(u3t(x))), nkb_d);                                 \
  TAKEN(x, a_h, KA);                                                            \
  DROP(b_h, KB);                                                                \
  CALL_MONO(gop_u, x, d_h);
OP3KKI(NOK, a_h, b_h, d_h, BODY_NOK)

//  the call opcodes: the site a_h holds the argument slots, their consume
//  flags and the destination slot of the product
//
#define BODY_CAL()                                                              \
  u3nc_dire* dir_u;                                                             \
  u3nc_prog* gop_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  SITE();                                                                       \
  CALLEE();                                                                     \
  GATHER(gop_u->tot_h);                                                         \
  _nc_stat_move(gop_u, kon_y, len_h, 0);                                        \
  MOVE();                                                                       \
  _nc_stat(dir_d);                                                              \
  INVOKE(gop_u, nex, len_h, dir_u->des_h);
OP1I(CAL, a_h, BODY_CAL)

//  the total array jet of the site, on its arguments: retained, so the
//  consumed ones are released after
//
#define BODY_CAF()                                                              \
  u3nc_dire* dir_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  u3_noun    pro;                                                               \
  SITE();                                                                       \
  GATHER(len_h);                                                                \
  _nc_stat(jet_d);                                                              \
  pro = JT(dir_u->arm_u->arg_f(nex));                                               \
  POP(len_h);                                                                   \
  LEAVE();                                                                      \
  PUT(dir_u->des_h, pro);                                                       \
  BURN();
OP1F(CAF, a_h, BODY_CAF)

//  the transferring driver of the jet, on its arguments: it takes the
//  references of the consumed ones, and retains the others
//
#define BODY_CAX()                                                              \
  u3nc_dire* dir_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  u3_noun    pro;                                                               \
  SITE();                                                                       \
  GATHER(len_h);                                                                \
  GIVE();                                                                       \
  _nc_stat(jet_d);                                                              \
  _nc_stat(tra_d);                                                              \
  pro = JT(dir_u->arm_u->own_f(nex, kon_y));                                        \
  POP(len_h);                                                                   \
  PUT(dir_u->des_h, pro);                                                       \
  BURN();
OP1F(CAX, a_h, BODY_CAX)

#define CHECK(dir, nex, len)  BASE(check)(dir, nex, len)

//  the total array jet of the site under the flags of its arm, as u3n
//  kicks a jet (_cj_kick_f()): the arm off, the unary program of the
//  callee on the core rebuilt from the arguments, by subject, as CAP
//  without a driver; trusted, the driver alone, as CAF; else the driver
//  tested against the program (_nc_burn_check()).  Compiled where the arm
//  isn't trusted, and recompiled when its flags change (u3nc_stale()):
//  the flags are read here for the test, which turns the arm off while
//  the program runs, and for the sites compiled before a change.
//
#define BODY_CAC()                                                              \
  u3nc_dire* dir_u;                                                             \
  u3nc_prog* gop_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  u3_noun    pro;                                                               \
  SITE();                                                                       \
  GATHER(len_h);                                                                \
  if ( c3n == dir_u->ham_u->liv ) {                                             \
    u3_noun cor = _nc_core(dir_u, nex);                                         \
    POP(len_h);                                                                 \
    LEAVE();                                                                    \
    _nc_stat(sub_d);                                                            \
    gop_u = _nc_mono(dir_u);                                                    \
    CALL_MONO(gop_u, cor, dir_u->des_h);                                        \
  }                                                                             \
  _nc_stat(jet_d);                                                              \
  pro = ( c3y == dir_u->ham_u->ice )                                            \
      ? JT(dir_u->arm_u->arg_f(nex))                                                \
      : CHECK(dir_u, nex, len_h);                                               \
  POP(len_h);                                                                   \
  LEAVE();                                                                      \
  PUT(dir_u->des_h, pro);                                                       \
  BURN();
OP1I(CAC, a_h, BODY_CAC)

//  a call by subject: the u3w jet of the site, if it has one, on the
//  subject, under the flags of its arm; the unary program of the bell if
//  it punts.  The jet takes the subject's reference only if it doesn't
//  punt.
//
#define BODY_CAP()                                                              \
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[a_h]);                                \
  u3nc_prog* gop_u;                                                             \
  u3_noun    x = TAKE(SUB(), KON());                                            \
  u3_noun    pro;                                                               \
  if ( dir_u->arm_u && dir_u->arm_u->mon_f ) {                                  \
    if ( _nc_memo_jet(dir_u->arm_u) ) {                                         \
      CALLEE();                                                                 \
      u3nc_Gop = gop_u;                                                         \
    }                                                                           \
    pro = JT(u3j_kick_arm(x, dir_u->ham_u, dir_u->arm_u->mon_f,                    \
                       u3t(dir_u->ring)));                                       \
    u3nc_Gop = 0;                                                               \
  }                                                                             \
  else {                                                                        \
    pro = u3_none;                                                              \
  }                                                                             \
  if ( u3_none != pro )                                                         \
  {                                                                             \
    _nc_stat(jet_d);                                                            \
    PUT(dir_u->des_h, pro);                                                     \
    BURN();                                                                     \
  }                                                                             \
  _nc_stat(sub_d);                                                              \
  CALLEE();                                                                     \
  CALL_MONO(gop_u, x, dir_u->des_h);
OP1I(CAP, a_h, BODY_CAP)

#define BODY_CAM()                                                              \
  u3nc_dire* dir_u;                                                             \
  u3nc_prog* gop_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  c3_h       kni_h;       /*  argument cursor of _nc_knit(), address-taken  */  \
  u3_noun*   nex;                                                               \
  u3_noun    x, o;                                                              \
  SITE();                                                                       \
  CALLEE();                                                                     \
  GATHER(gop_u->tot_h);                                                         \
  kni_h = 0;                                                                    \
  {                                                                             \
    _nc_tsc_beg();                                                              \
    x = _nc_key(dir_u->cid_h, c3n,                                              \
                u3nc(_nc_knit(gop_u->ned, nex, &kni_h), GAIN(dir_u->bell)));    \
    o = _nc_find(dir_u->cid_h, x);                                              \
    _nc_tsc_end(mem_c);                                                         \
  }                                                                             \
  _nc_stat(mem_d);                                                              \
  _nc_stat_if(u3_none != o, meh_d);                                             \
  if ( u3_none != o ) {                                                         \
    POP(gop_u->tot_h);                                                          \
    LOSE(x);                                                                    \
    LEAVE();                                                                    \
    PUT(dir_u->des_h, o);                                                       \
    BURN();                                                                     \
  }                                                                             \
  _nc_stat_move(gop_u, kon_y, len_h, 0);                                        \
  MOVE();                                                                       \
  _nc_stat(dir_d);                                                              \
  INVOKE_MEMO(gop_u, nex, len_h, dir_u->des_h, x, dir_u->cid_h);
OP1I(CAM, a_h, BODY_CAM)

#define BODY_CSM()                                                              \
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[a_h]);                                \
  u3nc_prog* gop_u;                                                             \
  u3_noun*   nex;                                                               \
  c3_h       s_h = SUB();                                                       \
  u3_noun    x   = reg[s_h];                                                    \
  u3_noun    o;                                                                 \
  u3_weak    pro;                                                               \
  {                                                                             \
    _nc_tsc_beg();                                                              \
    o   = _nc_key(dir_u->cid_h, c3y,                                            \
                  u3nc(GAIN(x), GAIN(u3t(dir_u->bell))));                       \
    pro = _nc_find(dir_u->cid_h, o);                                            \
    _nc_tsc_end(mem_c);                                                         \
  }                                                                             \
  _nc_stat(mem_d);                                                              \
  _nc_stat_if(u3_none != pro, meh_d);                                           \
  if ( u3_none != pro ) {                                                       \
    LOSE(o);                                                                    \
    DROP(s_h, KON());                                                           \
    PUT(dir_u->des_h, pro);                                                     \
    BURN();                                                                     \
  }                                                                             \
  _nc_stat(sub_d);                                                              \
  CALLEE();                                                                     \
  nex = PUSH(gop_u->tot_h);                                                     \
  TAKEN(x, s_h, KON());                                                         \
  nex[0] = x;                                                                   \
  INVOKE_MEMO(gop_u, nex, 1, dir_u->des_h, o, dir_u->cid_h);
OP1I(CSM, a_h, BODY_CSM)

//  branches release a source on the edge that consumes it, before they
//  go: KY (KAY, KBY) on the fall-through to the yes block, KT (KAT, KBT)
//  on the jump
//
#define BODY_CLQ(KY, KT)                                                        \
  if ( c3y == u3du(reg[a_h]) ) {                                                \
    DROP(a_h, KY);                                                              \
  }                                                                             \
  else {                                                                        \
    DROP(a_h, KT);                                                              \
    JUMP(b_h);                                                                  \
  }                                                                             \
  BURN();
OP2KK(CLQ, a_h, b_h, BODY_CLQ)

#define BODY_EQQ(KAY, KAT, KBY, KBT)                                            \
  if ( c3y == u3r_sing(reg[a_h], reg[b_h]) ) {                                  \
    DROP(a_h, KAY);                                                             \
    DROP(b_h, KBY);                                                             \
  }                                                                             \
  else {                                                                        \
    DROP(a_h, KAT);                                                             \
    DROP(b_h, KBT);                                                             \
    JUMP(c_h);                                                                  \
  }                                                                             \
  BURN();
OP3K4(EQQ, a_h, b_h, c_h, BODY_EQQ)

#define BODY_EQI(KY, KT)                                                        \
  if ( reg[b_h] == a_h ) {                                                      \
    VOID(b_h, KY);                                                              \
  }                                                                             \
  else {                                                                        \
    DROP(b_h, KT);                                                              \
    JUMP(c_h);                                                                  \
  }                                                                             \
  BURN();
OP3KK(EQI, a_h, b_h, c_h, BODY_EQI)

//  cells and indirect atoms are above every direct atom
//
#define BODY_LTI(KY, KT)                                                        \
  if ( reg[b_h] < pog_u->lit_u.non[a_h] ) {                                     \
    DROP(b_h, KY);                                                              \
  }                                                                             \
  else {                                                                        \
    DROP(b_h, KT);                                                              \
    JUMP(c_h);                                                                  \
  }                                                                             \
  BURN();
OP3KK(LTI, a_h, b_h, c_h, BODY_LTI)

//  a direct atom equals only itself
//
#define BODY_EQL(KY, KT)                                                        \
  u3_noun l = pog_u->lit_u.non[a_h], x = reg[b_h];                              \
  if (  (l == x)                                                                \
     || (  (c3n == u3a_is_cat(l)) && (c3n == u3a_is_cat(x))                     \
        && (c3y == u3r_sing(l, x)) ) )                                          \
  {                                                                             \
    DROP(b_h, KY);                                                              \
  }                                                                             \
  else {                                                                        \
    DROP(b_h, KT);                                                              \
    JUMP(c_h);                                                                  \
  }                                                                             \
  BURN();
OP3KK(EQL, a_h, b_h, c_h, BODY_EQL)

#define BODY_BRN(KY, KT)                                                        \
  u3_noun x = reg[a_h];                                                         \
  if ( x > 1 ) {                                                                \
    u3m_bail(c3__exit);                                                         \
  }                                                                             \
  if ( 1 == x ) {                                                               \
    VOID(a_h, KT);                                                              \
    JUMP(b_h);                                                                  \
  }                                                                             \
  else {                                                                        \
    VOID(a_h, KY);                                                              \
  }                                                                             \
  BURN();
OP2KK(BRN, a_h, b_h, BODY_BRN)

#define BODY_BRZ(KY, KT)                                                        \
  if ( 0 == reg[a_h] ) {                                                        \
    VOID(a_h, KY);                                                              \
  }                                                                             \
  else {                                                                        \
    DROP(a_h, KT);                                                              \
    JUMP(b_h);                                                                  \
  }                                                                             \
  BURN();
OP2KK(BRZ, a_h, b_h, BODY_BRZ)

#define BODY_HOP()                                                              \
  JUMP(a_h);                                                                    \
  BURN();
OP1(HOP, a_h, BODY_HOP)

//  a tail call: the arguments are the last registers alive, so the
//  caller's slots are all empty once they have moved, unless the block
//  left its drops to the call: S
//
#define BODY_JMP(S)                                                             \
  u3nc_dire* dir_u;                                                             \
  u3nc_prog* gop_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  SITE();                                                                       \
  CALLEE();                                                                     \
  GATHER(len_h);                                                                \
  _nc_stat(dir_d);                                                              \
  _nc_stat_move(gop_u, kon_y, len_h, 1);                                        \
  MOVE();                                                                       \
  SWEEP(S);                                                                     \
  EMPTY();                                                                      \
  POP(len_h);                                                                   \
  QUIT();                                                                       \
  pog_u = gop_u;                                                                \
  reg   = PUSH(pog_u->tot_h);                                                   \
  memmove(reg, nex, len_h * sizeof(u3_noun));                                   \
  REJOIN();                                                                     \
  ENTER(len_h);
OP1KT(JMP, a_h, BODY_JMP)

//  a tail call to itself. Only the sweeping variant S == 1 is called, the other
//  gets lowered to a bunch of MOVs + HOP to entry point
//
#define BODY_JMS(S)                                                             \
  u3nc_dire* dir_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  SITE();                                                                       \
  GATHER(len_h);                                                                \
  _nc_stat(dir_d);                                                              \
  _nc_stat_move(pog_u, kon_y, len_h, 1);                                        \
  MOVE();                                                                       \
  SWEEP(S);                                                                     \
  EMPTY();                                                                      \
  for ( c3_h _i = 0; _i < len_h; _i++ ) {                                       \
    reg[_i] = nex[_i];                                                          \
  }                                                                             \
  POP(len_h);                                                                   \
  AGAIN();
OP1KT(JMS, a_h, BODY_JMS)

#define BODY_JMF(S)                                                             \
  u3nc_dire* dir_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  u3_noun    pro;                                                               \
  SITE();                                                                       \
  GATHER(len_h);                                                                \
  _nc_stat(jet_d);                                                              \
  pro = JT(dir_u->arm_u->arg_f(nex));                                               \
  POP(len_h);                                                                   \
  LEAVE();                                                                      \
  SWEEP(S);                                                                     \
  DONE(pro);
OP1KT(JMF, a_h, BODY_JMF)

#define BODY_JMX(S)                                                             \
  u3nc_dire* dir_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  u3_noun    pro;                                                               \
  SITE();                                                                       \
  GATHER(len_h);                                                                \
  GIVE();                                                                       \
  _nc_stat(jet_d);                                                              \
  _nc_stat(tra_d);                                                              \
  pro = JT(dir_u->arm_u->own_f(nex, kon_y));                                        \
  POP(len_h);                                                                   \
  SWEEP(S);                                                                     \
  DONE(pro);
OP1KT(JMX, a_h, BODY_JMX)

//  the subject's slot gives up its reference, to the jet or the callee
//
#define BODY_JSP(S)                                                             \
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[a_h]);                                \
  u3nc_prog* gop_u;                                                             \
  u3_noun*   sot = &(reg[SUB()]);                                               \
  u3_noun    x   = *sot;                                                        \
  u3_noun    pro;                                                               \
  if ( _nc_zer_t ) {                                                            \
    *sot = 0;                                                                   \
  }                                                                             \
  SWEEP(S);                                                                     \
  if ( dir_u->arm_u && dir_u->arm_u->mon_f ) {                                  \
    if ( _nc_memo_jet(dir_u->arm_u) ) {                                         \
      CALLEE();                                                                 \
      u3nc_Gop = gop_u;                                                         \
    }                                                                           \
    pro = JT(u3j_kick_arm(x, dir_u->ham_u, dir_u->arm_u->mon_f,                    \
                       u3t(dir_u->ring)));                                       \
    u3nc_Gop = 0;                                                               \
  }                                                                             \
  else {                                                                        \
    pro = u3_none;                                                              \
  }                                                                             \
  if ( u3_none != pro )                                                         \
  {                                                                             \
    _nc_stat(jet_d);                                                            \
    DONE(pro);                                                                  \
  }                                                                             \
  _nc_stat(sub_d);                                                              \
  CALLEE();                                                                     \
  TAIL_MONO(gop_u, x);
OP1KT(JSP, a_h, BODY_JSP)

//  CAC in tail position: JMF, or JSP without a driver when the arm is off
//
#define BODY_JMC(S)                                                             \
  u3nc_dire* dir_u;                                                             \
  u3nc_prog* gop_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  u3_noun    pro;                                                               \
  SITE();                                                                       \
  GATHER(len_h);                                                                \
  if ( c3n == dir_u->ham_u->liv ) {                                             \
    u3_noun cor = _nc_core(dir_u, nex);                                         \
    POP(len_h);                                                                 \
    LEAVE();                                                                    \
    SWEEP(S);                                                                   \
    _nc_stat(sub_d);                                                            \
    gop_u = _nc_mono(dir_u);                                                    \
    TAIL_MONO(gop_u, cor);                                                      \
  }                                                                             \
  _nc_stat(jet_d);                                                              \
  pro = ( c3y == dir_u->ham_u->ice )                                            \
      ? JT(dir_u->arm_u->arg_f(nex))                                                \
      : CHECK(dir_u, nex, len_h);                                               \
  POP(len_h);                                                                   \
  LEAVE();                                                                      \
  SWEEP(S);                                                                     \
  DONE(pro);
OP1KT(JMC, a_h, BODY_JMC)

#define BODY_DON(S)                                                             \
  u3_noun pro = reg[a_h];                                                       \
  ZERO(a_h);                                                                    \
  SWEEP(S);                                                                     \
  DONE(pro);
OP1K(DON, a_h, BODY_DON)

//  a product in parts: the first is the product of the activation, the
//  rest go through _nc_ret; where the caller takes one noun, they are consed
//
#define BODY_RTL()                                                              \
  PUT(d_h, _nc_ret[a_h]);                                                       \
  BURN();
OP2(RTL, a_h, d_h, BODY_RTL)

#define BODY_RTS(K)                                                             \
  _nc_ret[a_h] = TAKE(b_h, K);                                                  \
  BURN();
OP2K(RTS, a_h, b_h, BODY_RTS)

#define BODY_DOX(S)                                                             \
  u3_noun   pro = reg[b_h];                                                     \
  nc_frame* fam_u;                                                              \
  ZERO(b_h);                                                                    \
  SWEEP(S);                                                                     \
  fam_u = TOP(_nc_frame_w);                                                     \
  if ( !fam_u->pog_u || (u3_none != fam_u->key) ) {                             \
    pro = _nc_box(pog_u->lit_u.non[a_h], pro);                                  \
  }                                                                             \
  DONE(pro);
OP2K(DOX, a_h, b_h, BODY_DOX)

#define BODY_DRO()                                                              \
  u3_noun x = reg[a_h];                                                         \
  _nc_stat(drt_d);                                                              \
  _nc_stat_if(c3n == u3a_is_cat(x), dro_d);                                     \
  ZERO(a_h);                                                                    \
  LOSE(x);                                                                      \
  BURN();
OP1(DRO, a_h, BODY_DRO)


#if 1 != _nc_mode
#if 2 == _nc_mode
u3_noun
#else
static u3_noun
#endif
OP(BOM) ARGS CONV
{
  u3m_bail(c3__exit);
}
#endif

#if 0 == _nc_mode
//  WIDE: the _V width of the opcode in the next byte, from its own table
//
static u3_noun
OP(WIDE) ARGS CONV
{
  c3_y op_y = RB();
  MUSTTAIL return WTAB[op_y](NEXT);
}
#endif

#if 2 == _nc_mode
//  The stencils of a call, which native code makes of pieces, one per
//  argument, where the interpreter loops over the site: no loop, and the
//  slots are in the code.  Between the pieces the slots of the callee
//  travel in the instruction pointer's register, which native code has no
//  other use for, and the callee, by its loom offset, in the first
//  operand's.
//
//    CALB i     the callee of site i, linked; its slots pushed
//    ARGB s j   slot s to argument j, borrowed
//    ARGK s j   slot s to argument j, consumed
//    ARGC s j   slot s to argument j, uncounted: for a jet, which retains
//    CALE d n   the frame, the product to slot d; enter the callee, which
//               has n arguments
//    JETB n     room for the arguments of a total array jet, n words
//    JETE i     the jet of site i on them; the product waits in their
//               place for the drops of the arguments the site consumes,
//               which follow as DROs: it may go to the slot of one
//    JETP n d   the product to slot d; the n words popped
//    JMPB i n   the callee of site i, linked; room for its n arguments,
//               above this activation
//    JMPE n     this activation gives way to the callee's, the frame over
//               its slots, the n arguments moved into them; enter it.
//               JMPE1..JMPE4: the same for that many arguments, the move
//               in line
//
#define HAND  ip, reg, pog_u, rod_u, arg_a_w, und_w, und_w

u3_noun OP(CALB) ARGS CONV {
  c3_w       und_w;
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[VAL(0)]);
  u3nc_prog* gop_u;
  CALLEE();
  ip      = (c3_y*)PUSH(gop_u->tot_h);
  arg_a_w = u3of(u3nc_prog, gop_u);
  MUSTTAIL return _nj_next(HAND);
}

u3_noun OP(ARGB) ARGS CONV {
  c3_w und_w;
  ((u3_noun*)ip)[SLT(1)] = GAIN(reg[SLT(0)]);
  MUSTTAIL return _nj_next(HAND);
}

u3_noun OP(ARGK) ARGS CONV {
  c3_w und_w;
  ((u3_noun*)ip)[SLT(1)] = reg[SLT(0)];
  ZERO(SLT(0));
  MUSTTAIL return _nj_next(HAND);
}

u3_noun OP(ARGC) ARGS CONV {
  c3_w und_w;
  ((u3_noun*)ip)[SLT(1)] = reg[SLT(0)];
  MUSTTAIL return _nj_next(HAND);
}

u3_noun OP(CALE) ARGS CONV {
  c3_w       und_w;
  u3nc_prog* gop_u = u3to(u3nc_prog, arg_a_w);
  FRAME_T*   fam_u = PUSH(FRAME_W);
  fam_u->pog_u = pog_u;
  fam_u->reg   = reg;
  fam_u->ret_f = (void*)_nj_next;
  fam_u->des_h = VAL(0);
  fam_u->key   = u3_none;
  fam_u->cid_h = 0;
  pog_u = gop_u;
  reg   = (u3_noun*)ip;
  REST(pog_u, reg, VAL(1));
  MUSTTAIL return ((OP_F)pog_u->nat_f[_nc_dir])(NEXT);
}

u3_noun OP(JMPB) ARGS CONV {
  c3_w       und_w;
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[VAL(0)]);
  u3nc_prog* gop_u;
  CALLEE();
  ip      = (c3_y*)PUSH(VAL(1));
  arg_a_w = u3of(u3nc_prog, gop_u);
  MUSTTAIL return _nj_next(HAND);
}

#define JMPE(nam, num)                                                          \
  u3_noun OP(nam) ARGS CONV {                                                   \
    c3_w       und_w;                                                           \
    u3nc_prog* gop_u = u3to(u3nc_prog, arg_a_w);                                \
    u3_noun*   nex   = (u3_noun*)ip;                                            \
    POP(num);                                                                   \
    QUIT();                                                                     \
    pog_u = gop_u;                                                              \
    reg   = PUSH(pog_u->tot_h);                                                 \
    memmove(reg, nex, (num) * sizeof(u3_noun));                                 \
    REJOIN();                                                                   \
    REST(pog_u, reg, num);                                                      \
    MUSTTAIL return ((OP_F)pog_u->nat_f[_nc_dir])(NEXT);                        \
  }
JMPE(JMPE,  VAL(0))
JMPE(JMPE1, 1)
JMPE(JMPE2, 2)
JMPE(JMPE3, 3)
JMPE(JMPE4, 4)
#undef JMPE

u3_noun OP(JETB) ARGS CONV {
  c3_w und_w;
  ip = (c3_y*)PUSH(VAL(0));
  BURN();
}

u3_noun OP(JETE) ARGS CONV {
  c3_w       und_w;
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[VAL(0)]);
  u3_noun*   nex   = (u3_noun*)ip;
  nex[0] = dir_u->arm_u->arg_f(nex);
  BURN();
}

u3_noun OP(JETP) ARGS CONV {
  c3_w    und_w;
  u3_noun pro = ((u3_noun*)ip)[0];
  POP(VAL(0));
  reg[SLT(1)] = pro;
  BURN();
}

#undef HAND
#  pragma clang diagnostic pop
#  undef SLT
#  undef VAL
#  undef HOLE
#endif
#undef _nc_mode
#undef FAR1
#undef FAR2
#undef FAR3
#undef STF1
#undef STF2
#undef STF3
#undef STI1
#undef STI3
#undef STT1
#undef GO
#undef OP1F
#undef OP1I
#undef OP2KF
#undef OP3KKF
#undef OP3KKI
#undef OP1KT
#undef BASE
#undef _nc_base
#undef FRAME_T
#undef FRAME_W
#undef FRAME_BACK
#undef REST
#undef INVOKE
#undef INVOKE_MEMO
#undef CALL_MONO
#undef QUIT
#undef REJOIN
#undef TAIL_MONO
#undef START
#undef AGAIN
#undef _nc_dir
#undef BODY0
#undef OP0

#undef _nc_cat_
#undef _nc_cat
#undef OP
#undef TAB
#undef WTAB
#undef OP_F
#undef CONV
#undef MUSTTAIL
#undef SIG
#undef ARGS
#undef PASS
#undef NEXT
#undef RB
#undef RS
#undef RV
#undef BURN
#undef JUMP
#undef PUSH
#undef POP
#undef TOP
#undef PUT
#undef EMPTY
#undef SWEEP
#undef TAKEN
#undef TAKE
#undef DROP
#undef VOID
#undef UNIQUE
#undef CELL
#undef DECODE1
#undef DECODE2
#undef DECODE3
#undef BODY1
#undef BODY2
#undef BODY3
#undef OP1
#undef OP2
#undef OP3
#undef OP1K
#undef OP2K
#undef OP3K
#undef OP2KK
#undef OP3KK
#undef OP3K4
#undef FRAME
#undef SITE
#undef SUB
#undef KON
#undef CALLEE
#undef GATHER
#undef MOVE
#undef LEAVE
#undef ENTER
#undef CALL
#undef TAIL
#undef DONE
#undef BODY_IMM
#undef BODY_IML
#undef BODY_MOV
#undef BODY_INC
#undef BODY_DEC
#undef BODY_ADD
#undef BODY_CON
#undef BODY_HED
#undef BODY_TAL
#undef REUSE
#undef BODY_HER
#undef BODY_TAR
#undef BODY_COR
#undef BODY_CEL
#undef BODY_LOB
#undef BODY_EQU
#undef BODY_HSP
#undef BODY_HSE
#undef BODY_HDP
#undef BODY_HDE
#undef BODY_HLP
#undef BODY_HMP
#undef BODY_HME
#undef BODY_HCP
#undef BODY_HCE
#undef BODY_SPY
#undef BODY_NOK
#undef BODY_CAL
#undef BODY_CAF
#undef BODY_CAX
#undef BODY_CAP
#undef BODY_CAM
#undef BODY_CSM
#undef BODY_CLQ
#undef BODY_EQQ
#undef BODY_EQI
#undef BODY_LTI
#undef BODY_EQL
#undef BODY_BRN
#undef BODY_BRZ
#undef BODY_HOP
#undef BODY_JMP
#undef BODY_JMS
#undef BODY_JMF
#undef BODY_JMX
#undef BODY_JSP
#undef BODY_DON
#undef BODY_DRO
#undef BODY_RTL
#undef BODY_RTS
#undef BODY_DOX
