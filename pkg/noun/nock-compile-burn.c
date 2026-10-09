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
_Static_assert(1, "");
#ifndef _nc_burn
#  include "nock-compile.c"
#  define _nc_mov_ws  (-999)
#  define _nc_burn    _nc_burn_ide
#endif

#if !__has_attribute(musttail) || !__has_attribute(preserve_none)
#  error "the u3nc interpreter needs clang's musttail and preserve_none"
#endif

#define _nc_cat_(a, b)  a##b
#define _nc_cat(a, b)   _nc_cat_(a, b)

//  OP(op): the function of an opcode, or of an internal entry point;
//  TAB: the dispatch table; OP_F: the type of its entries
//
#define OP(op)  _nc_cat(_nc_burn, _##op)
#define TAB     _nc_cat(_nc_burn, _tab)
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
#define NEXT  ip, reg, pog_u, rod_u, 0, 0, 0

typedef u3_noun (*OP_F)ARGS CONV;

#define Y(op)  static u3_noun OP(op) ARGS CONV;
OPCODES
#undef Y

static u3_noun OP(done) ARGS CONV;

#define Y(op)  OP(op),
static const OP_F TAB[] = { OPCODES };
#undef Y

#define RB()  (*ip++)
#define RS()  ({ c3_h _v = ip[0] | (ip[1] << 8); ip += 2; _v; })
#define RV()  ({                                                                \
    c3_y _n = *ip++;                                                            \
    c3_h _v = 0;                                                                \
    for ( c3_y _i = 0; _i < _n; _i++ ) {                                        \
      _v |= ((c3_h)*ip++) << (8 * _i);                                          \
    }                                                                           \
    _v;                                                                         \
  })

//  BURN(): dispatch the next opcode
//
#define BURN()  do {                                                            \
    c3_y _op = *ip++;                                                           \
    MUSTTAIL return TAB[_op](NEXT);                                             \
  } while ( 0 )

#define JUMP(t)  (ip = pog_u->byc_u.ops_y + (t))

#define PUSH(n)  _nc_push(rod_u, _nc_mov_ws, n)
#define POP(n)   _nc_pop(rod_u, _nc_mov_ws, n)
#define TOP(n)   _nc_top(rod_u, _nc_mov_ws, n)

//  Slots hold counted references, exactly while the register in them is
//  live (_nc_lives()):
//
//  PUT(d, v): the reference v into slot d, which is empty: the register
//             that held it was consumed or dropped
//  TAKE(s, K): the noun in slot s, with a reference: the slot's own if K,
//              consuming it (the slot is zeroed), else a new one
//  TAKEN(v, s, K): the same for v, already read from slot s: an op reading
//                  two slots that may be the same reads both before it
//                  takes either
//  DROP(s, K): release slot s after a borrowing read of it, if K
//  EMPTY(): when checking, assert that every slot of the activation is
//           zero: at a return or a tail call, they must be
//
#if defined(U3NC_CHECK) || defined(C3DBG)
#  define PUT(d, v)  do {                                                       \
    u3_assert( 0 == reg[d] );                                                   \
    reg[d] = (v);                                                               \
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
    if ( K ) { reg[s] = 0; } else { GAIN(v); }                                  \
  } while ( 0 )
#define TAKE(s, K)  ({ u3_noun _v = reg[s]; TAKEN(_v, s, K); _v; })
#define DROP(s, K)  do {                                                        \
    if ( K ) {                                                                  \
      u3_noun _v = reg[s];                                                      \
      reg[s] = 0;                                                               \
      LOSE(_v);                                                                 \
    }                                                                           \
  } while ( 0 )

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
//  a call.
//
#define BODY1(name, A, body, ...)                                               \
  static u3_noun OP(name##_in) ARGS CONV {                                      \
    const c3_h A = arg_a_w;                                                     \
    body(__VA_ARGS__)                                                           \
  }

#define BODY2(name, A, B, body, ...)                                            \
  static u3_noun OP(name##_in) ARGS CONV {                                      \
    const c3_h A = arg_a_w;                                                     \
    const c3_h B = arg_b_w;                                                     \
    body(__VA_ARGS__)                                                           \
  }

#define BODY3(name, A, B, C, body, ...)                                         \
  static u3_noun OP(name##_in) ARGS CONV {                                      \
    const c3_h A = arg_a_w;                                                     \
    const c3_h B = arg_b_w;                                                     \
    const c3_h C = arg_c_w;                                                     \
    body(__VA_ARGS__)                                                           \
  }

//  OP1/2/3(op, A.., body): an opcode with one, two or three operands and
//  no consume variants.  OP1K/2K/3K: with one consumable source, op_0
//  borrowing it and op_1 consuming it, the body taking K.  OP2KK/3KK:
//  with two, bit 0 of the variant for the first and bit 1 for the second,
//  the body taking KA and KB.
//
#define OP1(op, A, body)        DECODE1(op) BODY1(op, A, body)
#define OP2(op, A, B, body)     DECODE2(op) BODY2(op, A, B, body)
#define OP3(op, A, B, C, body)  DECODE3(op) BODY3(op, A, B, C, body)

#define OP1K(op, A, body)                                                       \
  DECODE1(op##_0) BODY1(op##_0, A, body, 0)                                     \
  DECODE1(op##_1) BODY1(op##_1, A, body, 1)

#define OP2K(op, A, B, body)                                                    \
  DECODE2(op##_0) BODY2(op##_0, A, B, body, 0)                                  \
  DECODE2(op##_1) BODY2(op##_1, A, B, body, 1)

#define OP3K(op, A, B, C, body)                                                 \
  DECODE3(op##_0) BODY3(op##_0, A, B, C, body, 0)                               \
  DECODE3(op##_1) BODY3(op##_1, A, B, C, body, 1)

#define OP2KK(op, A, B, body)                                                   \
  DECODE2(op##_0) BODY2(op##_0, A, B, body, 0, 0)                               \
  DECODE2(op##_1) BODY2(op##_1, A, B, body, 1, 0)                               \
  DECODE2(op##_2) BODY2(op##_2, A, B, body, 0, 1)                               \
  DECODE2(op##_3) BODY2(op##_3, A, B, body, 1, 1)

#define OP3KK(op, A, B, C, body)                                                \
  DECODE3(op##_0) BODY3(op##_0, A, B, C, body, 0, 0)                            \
  DECODE3(op##_1) BODY3(op##_1, A, B, C, body, 1, 0)                            \
  DECODE3(op##_2) BODY3(op##_2, A, B, C, body, 0, 1)                            \
  DECODE3(op##_3) BODY3(op##_3, A, B, C, body, 1, 1)

//  FRAME(des): push a frame recording this activation, its product to
//              slot des; produces the frame
//
#define FRAME(des)  ({                                                          \
    nc_frame* _f = PUSH(_nc_frame_w);                                           \
    _f->pog_u = pog_u;                                                          \
    _f->reg   = reg;                                                            \
    _f->ip_h  = ip - pog_u->byc_u.ops_y;                                        \
    _f->des_h = (des);                                                          \
    _f->key   = u3_none;                                                        \
    _f->cid_h = 0;                                                              \
    _f;                                                                         \
  })

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
      gop_u = _nc_callee(dir_u);                                                \
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
      if ( kon_y[_i] ) { reg[sot_h[_i]] = 0; } else { GAIN(nex[_i]); }          \
    }                                                                           \
  } while ( 0 )

//  LEAVE(): release the consumed arguments of the site, after a jet that
//           retained them
//
#define LEAVE()  do {                                                           \
    for ( c3_h _i = 0; _i < len_h; _i++ ) {                                     \
      if ( kon_y[_i] ) {                                                        \
        u3_noun _v = reg[sot_h[_i]];                                            \
        reg[sot_h[_i]] = 0;                                                     \
        LOSE(_v);                                                               \
      }                                                                         \
    }                                                                           \
  } while ( 0 )

//  ENTER(len): run pog_u, its len arguments in its slots at reg
//
#define ENTER(len)  do {                                                        \
    for ( c3_h _i = (len); _i < pog_u->tot_h; _i++ ) {                          \
      reg[_i] = 0;                                                              \
    }                                                                           \
    ip = pog_u->byc_u.ops_y;                                                    \
    BURN();                                                                     \
  } while ( 0 )

//  CALL(gop, arg, des): call gop on its one argument arg, transferred,
//                       its product to slot des
//
#define CALL(gop, arg, des)  do {                                               \
    u3_noun* _nex = PUSH((gop)->tot_h);                                         \
    _nex[0] = (arg);                                                            \
    (void)FRAME(des);                                                           \
    pog_u = (gop);                                                              \
    reg   = _nex;                                                               \
    ENTER(1);                                                                   \
  } while ( 0 )

//  TAIL(gop, arg): call gop on its one argument arg, transferred, in
//                  place of this activation, whose slots are all empty
//
#define TAIL(gop, arg)  do {                                                    \
    nc_frame _fam;                                                              \
    EMPTY();                                                                    \
    _fam = *(nc_frame*)TOP(_nc_frame_w);                                        \
    POP(_nc_frame_w + pog_u->tot_h);                                            \
    pog_u  = (gop);                                                             \
    reg    = PUSH(pog_u->tot_h);                                                \
    reg[0] = (arg);                                                             \
    *(nc_frame*)PUSH(_nc_frame_w) = _fam;                                       \
    ENTER(1);                                                                   \
  } while ( 0 )

//  DONE(pro): return pro, transferred, from this activation
//
#define DONE(pro)  MUSTTAIL return OP(done)(ip, reg, pog_u, rod_u, (pro), 0, 0)

/* _nc_burn(): run a program on its arguments.  TRANSFERS the arguments.
**
**   An activation is the callee's slots, then a frame holding the
**   caller's state: a call site copies its arguments straight into the
**   slots of the callee's new activation, consumed or gained.  A total
**   array jet gets its arguments copied above the activation instead,
**   uncounted, and leaves no other trace.  A tail call copies its
**   arguments to a temporary above its own activation, which is empty by
**   then (every other slot was consumed or dropped, or the call sweeps
**   them), and moves the temporary down in its place, under the same
**   frame.
*/
static u3_noun
_nc_burn(u3nc_prog* pog_u, u3_noun* arg, c3_h len_h)
{
  u3a_road* rod_u = u3R;
  u3_post   emp_p = rod_u->cap_p;
  u3_noun*  reg   = PUSH(pog_u->tot_h);
  nc_frame* fam_u;
  c3_y*     ip;
  c3_y      op_y;
  c3_h      i_h;
  u3_noun   pro;

  for ( i_h = 0; i_h < len_h; i_h++ ) {
    reg[i_h] = arg[i_h];
  }
  for ( i_h = len_h; i_h < pog_u->tot_h; i_h++ ) {
    reg[i_h] = 0;
  }

  fam_u = PUSH(_nc_frame_w);
  fam_u->pog_u = NULL;

  ip   = pog_u->byc_u.ops_y;
  op_y = *ip++;
  pro  = TAB[op_y](ip, reg, pog_u, rod_u, 0, 0, 0);

  u3_assert( emp_p == rod_u->cap_p );
  return pro;
}

//  done: arg_a_h is the product, transferred; the slots are all empty
//
static u3_noun
OP(done) ARGS CONV
{
  u3_noun   pro = arg_a_w;
  nc_frame* fam_u;
  c3_h      d_h;

  EMPTY();

  fam_u = TOP(_nc_frame_w);

  if ( !fam_u->pog_u ) {
    POP(_nc_frame_w + pog_u->tot_h);
    return pro;
  }

  if ( u3_none != fam_u->key ) {
    pro = _nc_save(fam_u->cid_h, fam_u->key, pro, fam_u->all_o);
    LOSE(fam_u->key);
  }

  POP(_nc_frame_w + pog_u->tot_h);
  pog_u = fam_u->pog_u;
  reg   = fam_u->reg;
  ip    = pog_u->byc_u.ops_y + fam_u->ip_h;
  d_h   = fam_u->des_h;
  PUT(d_h, pro);
  BURN();
}

static u3_noun
OP(IMM_0) ARGS CONV
{
  c3_h d_h = RB();
  PUT(d_h, 0);
  BURN();
}

static u3_noun
OP(IMM_1) ARGS CONV
{
  c3_h d_h = RB();
  PUT(d_h, 1);
  BURN();
}

static u3_noun
OP(IMM_B) ARGS CONV
{
  c3_h a_h = RB();
  c3_h d_h = RB();
  PUT(d_h, a_h);
  BURN();
}

static u3_noun
OP(IMM_S) ARGS CONV
{
  c3_h a_h = RS();
  c3_h d_h = RB();
  PUT(d_h, a_h);
  BURN();
}

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
  _nc_stat(jet_d);                                                              \
  if ( c3n == u3ud(reg[a_h]) ) {                                                \
    u3m_bail(c3__fail);                                                         \
  }                                                                             \
  PUT(d_h, u3qa_dec(reg[a_h]));                                                 \
  DROP(a_h, K);                                                                 \
  BURN();
OP2K(DEC, a_h, d_h, BODY_DEC)

#define BODY_ADD(KA, KB)                                                        \
  _nc_stat(jet_d);                                                              \
  if ( (c3n == u3ud(reg[a_h])) || (c3n == u3ud(reg[b_h])) ) {                   \
    u3m_bail(c3__fail);                                                         \
  }                                                                             \
  PUT(d_h, u3qa_add(reg[a_h], reg[b_h]));                                       \
  DROP(a_h, KA);                                                                \
  DROP(b_h, KB);                                                                \
  BURN();
OP3KK(ADD, a_h, b_h, d_h, BODY_ADD)

#define BODY_CON(KA, KB)                                                        \
  u3_noun h = reg[a_h], t = reg[b_h];                                           \
  TAKEN(h, a_h, KA);                                                            \
  TAKEN(t, b_h, KB);                                                            \
  _nc_stat(con_d);                                                              \
  PUT(d_h, u3nc(h, t));                                                         \
  BURN();
OP3KK(CON, a_h, b_h, d_h, BODY_CON)

//  UNIQUE(x, K): for the statistics, whether consuming x frees a cell
//
#define UNIQUE(x, K)                                                            \
  (  (K) && (c3y == u3a_is_pom(x)) && (c3y == _nc_normal(rod_u, x))             \
  && (1 == ((u3a_noun*)u3a_to_ptr(x))->use_w) )

#define BODY_HED(K)                                                             \
  u3_noun x = reg[a_h];                                                         \
  _nc_stat_if(UNIQUE(x, K), uni_d);                                             \
  PUT(d_h, ( c3y == u3du(x) ) ? GAIN(u3h(x)) : 0);                              \
  DROP(a_h, K);                                                                 \
  BURN();
OP2K(HED, a_h, d_h, BODY_HED)

#define BODY_TAL(K)                                                             \
  u3_noun x = reg[a_h];                                                         \
  _nc_stat_if(UNIQUE(x, K), uni_d);                                             \
  PUT(d_h, ( c3y == u3du(x) ) ? GAIN(u3t(x)) : 0);                              \
  DROP(a_h, K);                                                                 \
  BURN();
OP2K(TAL, a_h, d_h, BODY_TAL)

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
  DROP(a_h, K);                                                                 \
  BURN();
OP1K(LOB, a_h, BODY_LOB)

#define BODY_EQU(KA, KB)                                                        \
  (void)u3r_sing(reg[a_h], reg[b_h]);                                           \
  DROP(a_h, KA);                                                                \
  DROP(b_h, KB);                                                                \
  BURN();
OP2KK(EQU, a_h, b_h, BODY_EQU)

#define BODY_HSP()                                                              \
  u3_noun x = _nc_hilt_fore(u3h(pog_u->lit_u.non[a_h]));                        \
  *(u3_noun*)PUSH(1) = x;                                                       \
  BURN();
OP1(HSP, a_h, BODY_HSP)

#define BODY_HSE()                                                              \
  u3_noun x = *(u3_noun*)TOP(1);                                                \
  POP(1);                                                                       \
  _nc_hilt_hind(u3h(pog_u->lit_u.non[a_h]), x);                                 \
  BURN();
OP1(HSE, a_h, BODY_HSE)

#define BODY_HDP(K)                                                             \
  u3_noun x = _nc_hint_fore(u3h(pog_u->lit_u.non[a_h]), TAKE(b_h, K));          \
  *(u3_noun*)PUSH(1) = x;                                                       \
  BURN();
OP2K(HDP, a_h, b_h, BODY_HDP)

#define BODY_HDE(K)                                                             \
  u3_noun x = *(u3_noun*)TOP(1);                                                \
  POP(1);                                                                       \
  _nc_hint_hind(u3h(pog_u->lit_u.non[a_h]), x);                                 \
  DROP(b_h, K);                                                                 \
  BURN();
OP2K(HDE, a_h, b_h, BODY_HDE)

#define BODY_SPY(KA, KB)                                                        \
  u3_noun x = u3m_soft_esc(GAIN(reg[a_h]), GAIN(reg[b_h]));                     \
  if ( c3n == u3du(x) ) {                                                       \
    u3m_bail(u3nc(1, GAIN(reg[b_h])));                                          \
  }                                                                             \
  else if ( c3n == u3du(u3t(x)) ) {                                             \
    u3t_push(u3nt(c3__hunk, GAIN(reg[a_h]), GAIN(reg[b_h])));                   \
    u3m_bail(c3__exit);                                                         \
  }                                                                             \
  PUT(d_h, GAIN(u3t(u3t(x))));                                                  \
  LOSE(x);                                                                      \
  DROP(a_h, KA);                                                                \
  DROP(b_h, KB);                                                                \
  BURN();
OP3KK(SPY, a_h, b_h, d_h, BODY_SPY)

#define BODY_NOK(KA, KB)                                                        \
  u3nc_prog* gop_u;                                                             \
  u3_noun    x = reg[a_h];                                                      \
  u3t_off(noc_o);                                                               \
  gop_u = _nc_entry(x, reg[b_h]);                                               \
  u3t_on(noc_o);                                                                \
  TAKEN(x, a_h, KA);                                                            \
  DROP(b_h, KB);                                                                \
  CALL(gop_u, x, d_h);
OP3KK(NOK, a_h, b_h, d_h, BODY_NOK)

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
  MOVE();                                                                       \
  (void)FRAME(dir_u->des_h);                                                    \
  _nc_stat(dir_d);                                                              \
  pog_u = gop_u;                                                                \
  reg   = nex;                                                                  \
  ENTER(len_h);
OP1(CAL, a_h, BODY_CAL)

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
  pro = dir_u->arm_u->arg_f(nex);                                               \
  POP(len_h);                                                                   \
  LEAVE();                                                                      \
  PUT(dir_u->des_h, pro);                                                       \
  BURN();
OP1(CAF, a_h, BODY_CAF)

//  a call by subject: the u3w jet of the site, if it has one, on the
//  subject; the unary program of the bell if it punts.  The jet takes
//  the subject's reference only if it doesn't punt.
//
#define BODY_CAP()                                                              \
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[a_h]);                                \
  u3nc_prog* gop_u;                                                             \
  u3_noun    x = TAKE(SUB(), KON());                                            \
  u3_noun    pro;                                                               \
  if (  dir_u->ham_u                                                            \
     && (u3_none != (pro = u3j_kick_arm(x, dir_u->ham_u, u3t(dir_u->ring)))) )  \
  {                                                                             \
    _nc_stat(jet_d);                                                            \
    PUT(dir_u->des_h, pro);                                                     \
    BURN();                                                                     \
  }                                                                             \
  _nc_stat(sub_d);                                                              \
  CALLEE();                                                                     \
  CALL(gop_u, x, dir_u->des_h);
OP1(CAP, a_h, BODY_CAP)

#define BODY_CAM()                                                              \
  u3nc_dire* dir_u;                                                             \
  u3nc_prog* gop_u;                                                             \
  nc_frame*  fam_u;                                                             \
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
  x     = u3nc(_nc_knit(gop_u->ned, nex, &kni_h), GAIN(u3t(dir_u->bell)));      \
  o     = _nc_find(dir_u->cid_h, x, c3n);                                       \
  if ( u3_none != o ) {                                                         \
    POP(gop_u->tot_h);                                                          \
    LOSE(x);                                                                    \
    LEAVE();                                                                    \
    PUT(dir_u->des_h, o);                                                       \
    BURN();                                                                     \
  }                                                                             \
  MOVE();                                                                       \
  fam_u = FRAME(dir_u->des_h);                                                  \
  fam_u->key   = x;                                                             \
  fam_u->cid_h = dir_u->cid_h;                                                  \
  fam_u->all_o = c3n;                                                           \
  _nc_stat(dir_d);                                                              \
  pog_u = gop_u;                                                                \
  reg   = nex;                                                                  \
  ENTER(len_h);
OP1(CAM, a_h, BODY_CAM)

#define BODY_CSM()                                                              \
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[a_h]);                                \
  u3nc_prog* gop_u;                                                             \
  nc_frame*  fam_u;                                                             \
  u3_noun*   nex;                                                               \
  c3_h       s_h = SUB();                                                       \
  u3_noun    x   = reg[s_h];                                                    \
  u3_noun    o   = u3nc(GAIN(x), GAIN(u3t(dir_u->bell)));                       \
  u3_weak    pro = _nc_find(dir_u->cid_h, o, c3y);                              \
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
  fam_u  = FRAME(dir_u->des_h);                                                 \
  fam_u->key   = o;                                                             \
  fam_u->cid_h = dir_u->cid_h;                                                  \
  fam_u->all_o = c3y;                                                           \
  pog_u = gop_u;                                                                \
  reg   = nex;                                                                  \
  ENTER(1);
OP1(CSM, a_h, BODY_CSM)

//  branches release a consumed source before they jump
//
#define BODY_CLQ(K)                                                             \
  c3_t c = (c3y == u3du(reg[a_h]));                                             \
  DROP(a_h, K);                                                                 \
  if ( !c ) {                                                                   \
    JUMP(b_h);                                                                  \
  }                                                                             \
  BURN();
OP2K(CLQ, a_h, b_h, BODY_CLQ)

#define BODY_EQQ(KA, KB)                                                        \
  c3_o e = u3r_sing(reg[a_h], reg[b_h]);                                        \
  DROP(a_h, KA);                                                                \
  DROP(b_h, KB);                                                                \
  if ( c3n == e ) {                                                             \
    JUMP(c_h);                                                                  \
  }                                                                             \
  BURN();
OP3KK(EQQ, a_h, b_h, c_h, BODY_EQQ)

#define BODY_EQI(K)                                                             \
  c3_t e = (reg[b_h] == a_h);                                                   \
  DROP(b_h, K);                                                                 \
  if ( !e ) {                                                                   \
    JUMP(c_h);                                                                  \
  }                                                                             \
  BURN();
OP3K(EQI, a_h, b_h, c_h, BODY_EQI)

#define BODY_EQL(K)                                                             \
  c3_o e = u3r_sing(pog_u->lit_u.non[a_h], reg[b_h]);                           \
  DROP(b_h, K);                                                                 \
  if ( c3n == e ) {                                                             \
    JUMP(c_h);                                                                  \
  }                                                                             \
  BURN();
OP3K(EQL, a_h, b_h, c_h, BODY_EQL)

#define BODY_BRN(K)                                                             \
  u3_noun x = reg[a_h];                                                         \
  if ( x > 1 ) {                                                                \
    u3m_bail(c3__exit);                                                         \
  }                                                                             \
  DROP(a_h, K);                                                                 \
  if ( 1 == x ) {                                                               \
    JUMP(b_h);                                                                  \
  }                                                                             \
  BURN();
OP2K(BRN, a_h, b_h, BODY_BRN)

#define BODY_BRZ(K)                                                             \
  u3_noun x = reg[a_h];                                                         \
  DROP(a_h, K);                                                                 \
  if ( 0 != x ) {                                                               \
    JUMP(b_h);                                                                  \
  }                                                                             \
  BURN();
OP2K(BRZ, a_h, b_h, BODY_BRZ)

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
  nc_frame   fam;                                                               \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  SITE();                                                                       \
  CALLEE();                                                                     \
  GATHER(len_h);                                                                \
  _nc_stat(dir_d);                                                              \
  MOVE();                                                                       \
  SWEEP(S);                                                                     \
  EMPTY();                                                                      \
  POP(len_h);                                                                   \
  fam = *(nc_frame*)TOP(_nc_frame_w);                                           \
  POP(_nc_frame_w + pog_u->tot_h);                                              \
  pog_u = gop_u;                                                                \
  reg   = PUSH(pog_u->tot_h);                                                   \
  memmove(reg, nex, len_h * sizeof(u3_noun));                                   \
  *(nc_frame*)PUSH(_nc_frame_w) = fam;                                          \
  ENTER(len_h);
OP1K(JMP, a_h, BODY_JMP)

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
  pro = dir_u->arm_u->arg_f(nex);                                               \
  POP(len_h);                                                                   \
  LEAVE();                                                                      \
  SWEEP(S);                                                                     \
  DONE(pro);
OP1K(JMF, a_h, BODY_JMF)

//  the subject's slot gives up its reference, to the jet or the callee
//
#define BODY_JSP(S)                                                             \
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[a_h]);                                \
  u3nc_prog* gop_u;                                                             \
  u3_noun*   sot = &(reg[SUB()]);                                               \
  u3_noun    x   = *sot;                                                        \
  u3_noun    pro;                                                               \
  *sot = 0;                                                                     \
  SWEEP(S);                                                                     \
  if (  dir_u->ham_u                                                            \
     && (u3_none != (pro = u3j_kick_arm(x, dir_u->ham_u, u3t(dir_u->ring)))) )  \
  {                                                                             \
    _nc_stat(jet_d);                                                            \
    DONE(pro);                                                                  \
  }                                                                             \
  _nc_stat(sub_d);                                                              \
  CALLEE();                                                                     \
  TAIL(gop_u, x);
OP1K(JSP, a_h, BODY_JSP)

#define BODY_DON(S)                                                             \
  u3_noun pro = reg[a_h];                                                       \
  reg[a_h] = 0;                                                                 \
  SWEEP(S);                                                                     \
  DONE(pro);
OP1K(DON, a_h, BODY_DON)

#define BODY_DRO()                                                              \
  u3_noun x = reg[a_h];                                                         \
  _nc_stat_if(c3n == u3a_is_cat(x), dro_d);                                     \
  reg[a_h] = 0;                                                                 \
  LOSE(x);                                                                      \
  BURN();
OP1(DRO, a_h, BODY_DRO)

//  a sweep that keeps the registers live into the block: they are set
//  aside above the activation while the slots are swept
//
#define BODY_SWK()                                                              \
  u3nc_dire* dir_u;                                                             \
  c3_h*      sot_h;                                                             \
  c3_y*      kon_y;                                                             \
  c3_h       len_h;                                                             \
  u3_noun*   nex;                                                               \
  SITE();                                                                       \
  GATHER(len_h);                                                                \
  for ( c3_h i_h = 0; i_h < len_h; i_h++ ) {                                    \
    reg[sot_h[i_h]] = 0;                                                        \
  }                                                                             \
  SWEEP(1);                                                                     \
  for ( c3_h i_h = 0; i_h < len_h; i_h++ ) {                                    \
    reg[sot_h[i_h]] = nex[i_h];                                                 \
  }                                                                             \
  POP(len_h);                                                                   \
  (void)kon_y;                                                                  \
  BURN();
OP1(SWK, a_h, BODY_SWK)

static u3_noun
OP(BOM) ARGS CONV
{
  u3m_bail(c3__exit);
}

#undef _nc_cat_
#undef _nc_cat
#undef OP
#undef TAB
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
#undef UNIQUE
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
#undef BODY_IML
#undef BODY_MOV
#undef BODY_INC
#undef BODY_DEC
#undef BODY_ADD
#undef BODY_CON
#undef BODY_HED
#undef BODY_TAL
#undef BODY_CEL
#undef BODY_LOB
#undef BODY_EQU
#undef BODY_HSP
#undef BODY_HSE
#undef BODY_HDP
#undef BODY_HDE
#undef BODY_SPY
#undef BODY_NOK
#undef BODY_CAL
#undef BODY_CAF
#undef BODY_CAP
#undef BODY_CAM
#undef BODY_CSM
#undef BODY_CLQ
#undef BODY_EQQ
#undef BODY_EQI
#undef BODY_EQL
#undef BODY_BRN
#undef BODY_BRZ
#undef BODY_HOP
#undef BODY_JMP
#undef BODY_JMF
#undef BODY_JSP
#undef BODY_DON
#undef BODY_DRO
#undef BODY_SWK
