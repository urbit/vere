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
#define SIG(a, b, c)                                                     \
  (c3_y* ip, u3_noun* reg, u3nc_prog* pog_u, u3a_road* rod_u,            \
   c3_w a, c3_w b, c3_w c)
#define ARGS  SIG(a_w, b_w, c_w)
#define PASS  ip, reg, pog_u, rod_u, a_w, b_w, c_w
#define NEXT  ip, reg, pog_u, rod_u, 0, 0, 0

typedef u3_noun (*OP_F)ARGS CONV;

#define X(op)  static u3_noun OP(op) ARGS CONV;
OPCODES
#undef X

static u3_noun OP(done) ARGS CONV;

#define X(op)  OP(op),
static const OP_F TAB[] = { OPCODES };
#undef X

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
#define PUT(d, v)  do {                                                         \
    u3_noun _o = reg[d];                                                        \
    reg[d] = (v);                                                               \
    LOSE(_o);                                                                   \
  } while ( 0 )

#define PUSH(n)  _nc_push(rod_u, _nc_mov_ws, n)
#define POP(n)   _nc_pop(rod_u, _nc_mov_ws, n)
#define TOP(n)   _nc_top(rod_u, _nc_mov_ws, n)

//  OP0(op): an opcode without operands.  OP1/2/3(op, a, ...): an opcode
//  in its three widths, each decoding its operands and tail-calling the
//  shared body, which names them.  All open a function body: close with
//  OP_END.
//
#define OP0(op)                                                          \
  static u3_noun OP(op) ARGS CONV {

#define OP1(op, a)                                                       \
  static u3_noun OP(op##_in) ARGS CONV;                                  \
  static u3_noun OP(op##_B) ARGS CONV {                                  \
    a_w = RB();                                                          \
    MUSTTAIL return OP(op##_in)(PASS);                                   \
  }                                                                      \
  static u3_noun OP(op##_S) ARGS CONV {                                  \
    a_w = RS();                                                          \
    MUSTTAIL return OP(op##_in)(PASS);                                   \
  }                                                                      \
  static u3_noun OP(op##_V) ARGS CONV {                                  \
    a_w = RV();                                                          \
    MUSTTAIL return OP(op##_in)(PASS);                                   \
  }                                                                      \
  static u3_noun OP(op##_in) ARGS CONV {                                 \
    const c3_h a = a_w;

#define OP2(op, a, b)                                                    \
  static u3_noun OP(op##_in) ARGS CONV;                                  \
  static u3_noun OP(op##_B) ARGS CONV {                                  \
    a_w = RB(); b_w = RB();                                              \
    MUSTTAIL return OP(op##_in)(PASS);                                   \
  }                                                                      \
  static u3_noun OP(op##_S) ARGS CONV {                                  \
    a_w = RS(); b_w = RS();                                              \
    MUSTTAIL return OP(op##_in)(PASS);                                   \
  }                                                                      \
  static u3_noun OP(op##_V) ARGS CONV {                                  \
    a_w = RV(); b_w = RV();                                              \
    MUSTTAIL return OP(op##_in)(PASS);                                   \
  }                                                                      \
  static u3_noun OP(op##_in) ARGS CONV {                                 \
    const c3_h a = a_w;                                                  \
    const c3_h b = b_w;

#define OP3(op, a, b, c)                                                 \
  static u3_noun OP(op##_in) ARGS CONV;                                  \
  static u3_noun OP(op##_B) ARGS CONV {                                  \
    a_w = RB(); b_w = RB(); c_w = RB();                                  \
    MUSTTAIL return OP(op##_in)(PASS);                                   \
  }                                                                      \
  static u3_noun OP(op##_S) ARGS CONV {                                  \
    a_w = RS(); b_w = RS(); c_w = RS();                                  \
    MUSTTAIL return OP(op##_in)(PASS);                                   \
  }                                                                      \
  static u3_noun OP(op##_V) ARGS CONV {                                  \
    a_w = RV(); b_w = RV(); c_w = RV();                                  \
    MUSTTAIL return OP(op##_in)(PASS);                                   \
  }                                                                      \
  static u3_noun OP(op##_in) ARGS CONV {                                 \
    const c3_h a = a_w;                                                  \
    const c3_h b = b_w;                                                  \
    const c3_h c = c_w;

#define OP_END  }

//  FRAME(des): push a frame recording this activation, its product to
//              slot des; produces the frame
//
#define FRAME(des)  ({                                                   \
    nc_frame* _f = PUSH(_nc_frame_w);                                    \
    _f->pog_u = pog_u;                                                   \
    _f->reg   = reg;                                                     \
    _f->ip_h  = ip - pog_u->byc_u.ops_y;                                 \
    _f->des_h = (des);                                                   \
    _f->key   = u3_none;                                                 \
    _f->cid_h = 0;                                                       \
    _f;                                                                  \
  })

//  SITE(): the call site a_h, its callee gop_u, its argument slots
//          sot_h and their number len_h
//
#define SITE()  do {                                                     \
    dir_u = &(pog_u->dir_u.dat_u[a_h]);                                  \
    if ( dir_u->pog_p ) {                                                \
      gop_u = u3to(u3nc_prog, dir_u->pog_p);                             \
    }                                                                    \
    else {                                                               \
      u3t_off(noc_o);                                                    \
      gop_u = _nc_callee(dir_u);                                         \
      u3t_on(noc_o);                                                     \
    }                                                                    \
    sot_h = pog_u->sot_u.sot_h + dir_u->sot_h;                           \
    len_h = dir_u->len_h;                                                \
  } while ( 0 )

//  GATHER(): push num words and copy the site's arguments into them
//
#define GATHER(num)  do {                                                \
    nex = PUSH(num);                                                     \
    if ( 1 == len_h ) {                                                  \
      nex[0] = reg[sot_h[0]];                                            \
    }                                                                    \
    else if ( 2 == len_h ) {                                             \
      nex[0] = reg[sot_h[0]];                                            \
      nex[1] = reg[sot_h[1]];                                            \
    }                                                                    \
    else {                                                               \
      for ( c3_h _i = 0; _i < len_h; _i++ ) {                            \
        nex[_i] = reg[sot_h[_i]];                                        \
      }                                                                  \
    }                                                                    \
  } while ( 0 )

//  ENTER(len): run pog_u, its len arguments in its slots at reg
//
#define ENTER(len)  do {                                                 \
    for ( c3_h _i = (len); _i < pog_u->tot_h; _i++ ) {                   \
      reg[_i] = 0;                                                       \
    }                                                                    \
    ip = pog_u->byc_u.ops_y;                                             \
    BURN();                                                              \
  } while ( 0 )

//  CALL(gop, arg, des): call gop on its one argument arg, transferred,
//                       its product to slot des
//
#define CALL(gop, arg, des)  do {                                        \
    u3_noun* _nex = PUSH((gop)->tot_h);                                  \
    _nex[0] = (arg);                                                     \
    (void)FRAME(des);                                                    \
    pog_u = (gop);                                                       \
    reg   = _nex;                                                        \
    ENTER(1);                                                            \
  } while ( 0 )

//  TAIL(gop, arg): call gop on its one argument arg, transferred, in
//                  place of this activation
//
#define TAIL(gop, arg)  do {                                             \
    nc_frame _fam;                                                       \
    for ( c3_h _i = 0; _i < pog_u->tot_h; _i++ ) {                       \
      LOSE(reg[_i]);                                                     \
    }                                                                    \
    _fam = *(nc_frame*)TOP(_nc_frame_w);                                 \
    POP(_nc_frame_w + pog_u->tot_h);                                     \
    pog_u  = (gop);                                                      \
    reg    = PUSH(pog_u->tot_h);                                         \
    reg[0] = (arg);                                                      \
    *(nc_frame*)PUSH(_nc_frame_w) = _fam;                                \
    ENTER(1);                                                            \
  } while ( 0 )

//  DONE(pro): return pro, transferred, from this activation
//
#define DONE(pro)  MUSTTAIL return OP(done)(ip, reg, pog_u, rod_u, (pro), 0, 0)

/* _nc_burn(): run a program on its arguments.  TRANSFERS the arguments.
**
**   An activation is the callee's slots, then a frame holding the
**   caller's state, pushed only once the call is known to run: a call
**   site copies its arguments straight into the slots of the callee's
**   new activation, retained, and tries the array jet on them there;
**   a hit pops the slots and leaves no other trace.  A tail call copies
**   its arguments to a temporary above its own activation, moves the
**   caller's references into it, drops what the caller had left, and
**   moves the temporary down in its place, under the same frame.
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

//  done: a_w is the product, transferred
//
static u3_noun
OP(done) ARGS CONV
{
  const u3_noun pro = a_w;
  nc_frame*     fam_u;
  c3_h          d_h;

  for ( c3_h i_h = 0; i_h < pog_u->tot_h; i_h++ ) {
    LOSE(reg[i_h]);
  }

  fam_u = TOP(_nc_frame_w);

  if ( !fam_u->pog_u ) {
    POP(_nc_frame_w + pog_u->tot_h);
    return pro;
  }

  if ( u3_none != fam_u->key ) {
    _nc_save(fam_u->cid_h, fam_u->key, pro);
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

OP0(IMM_0)
  c3_h d_h = RB();
  PUT(d_h, 0);
  BURN();
OP_END

OP0(IMM_1)
  c3_h d_h = RB();
  PUT(d_h, 1);
  BURN();
OP_END

OP0(IMM_B)
  c3_h a_h = RB();
  c3_h d_h = RB();
  PUT(d_h, a_h);
  BURN();
OP_END

OP0(IMM_S)
  c3_h a_h = RS();
  c3_h d_h = RB();
  PUT(d_h, a_h);
  BURN();
OP_END

OP2(IML, a_h, d_h)
  PUT(d_h, GAIN(pog_u->lit_u.non[a_h]));
  BURN();
OP_END

OP2(MOV, a_h, d_h)
  PUT(d_h, GAIN(reg[a_h]));
  BURN();
OP_END

OP2(INC, a_h, d_h)
  PUT(d_h, u3i_vint(GAIN(reg[a_h])));
  BURN();
OP_END

//  jetted calls the interpreter knows: the jets retain their arguments
//
OP2(DEC, a_h, d_h)
  _nc_stat(jet_d);
  if ( c3n == u3ud(reg[a_h]) ) {
    u3m_bail(c3__fail);
  }
  PUT(d_h, u3qa_dec(reg[a_h]));
  BURN();
OP_END

OP3(ADD, a_h, b_h, d_h)
  _nc_stat(jet_d);
  if ( c3n == u3ud(reg[a_h]) || c3n == u3ud(reg[b_h]) ) {
    u3m_bail(c3__fail);
  }
  PUT(d_h, u3qa_add(reg[a_h], reg[b_h]));
  BURN();
OP_END

OP3(CON, a_h, b_h, d_h)
  PUT(d_h, u3nc(GAIN(reg[a_h]), GAIN(reg[b_h])));
  BURN();
OP_END

OP2(HED, a_h, d_h)
  u3_noun x = reg[a_h];
  PUT(d_h, ( c3y == u3du(x) ) ? GAIN(u3h(x)) : 0);
  BURN();
OP_END

OP2(TAL, a_h, d_h)
  u3_noun x = reg[a_h];
  PUT(d_h, ( c3y == u3du(x) ) ? GAIN(u3t(x)) : 0);
  BURN();
OP_END

OP1(CEL, a_h)
  if ( c3n == u3du(reg[a_h]) ) {
    u3m_bail(c3__exit);
  }
  BURN();
OP_END

OP1(LOB, a_h)
  if ( reg[a_h] > 1 ) {
    u3m_bail(c3__exit);
  }
  BURN();
OP_END

OP2(EQU, a_h, b_h)
  (void)u3r_sing(reg[a_h], reg[b_h]);
  BURN();
OP_END

OP1(HSP, a_h)
  u3_noun x = _nc_hilt_fore(u3h(pog_u->lit_u.non[a_h]));
  *(u3_noun*)PUSH(1) = x;
  BURN();
OP_END

OP1(HSE, a_h)
  u3_noun x = *(u3_noun*)TOP(1);
  POP(1);
  _nc_hilt_hind(u3h(pog_u->lit_u.non[a_h]), x);
  BURN();
OP_END

OP2(HDP, a_h, b_h)
  u3_noun x = _nc_hint_fore(u3h(pog_u->lit_u.non[a_h]), GAIN(reg[b_h]));
  *(u3_noun*)PUSH(1) = x;
  BURN();
OP_END

OP2(HDE, a_h, b_h)
  u3_noun x = *(u3_noun*)TOP(1);
  POP(1);
  _nc_hint_hind(u3h(pog_u->lit_u.non[a_h]), x);
  BURN();
OP_END

OP3(SPY, a_h, b_h, d_h)
  u3_noun x = u3m_soft_esc(GAIN(reg[a_h]), GAIN(reg[b_h]));

  if ( c3n == u3du(x) ) {
    u3m_bail(u3nc(1, GAIN(reg[b_h])));
  }
  else if ( c3n == u3du(u3t(x)) ) {
    u3t_push(u3nt(c3__hunk, GAIN(reg[a_h]), GAIN(reg[b_h])));
    u3m_bail(c3__exit);
  }

  PUT(d_h, GAIN(u3t(u3t(x))));
  LOSE(x);
  BURN();
OP_END

OP3(NOK, a_h, b_h, d_h)
  u3nc_prog* gop_u;
  u3_noun    x = reg[a_h];

  u3t_off(noc_o);
  gop_u = _nc_entry(x, reg[b_h]);
  u3t_on(noc_o);
  GAIN(x);
  CALL(gop_u, x, d_h);
OP_END

OP2(CAL, a_h, d_h)
  u3nc_dire* dir_u;
  u3nc_prog* gop_u;
  c3_h*      sot_h;
  c3_h       len_h;
  u3_noun*   nex;
  u3_noun    pro;

  SITE();
  GATHER(gop_u->tot_h);

  if ( dir_u->arm_u && (u3_none != (pro = dir_u->arm_u->arg_f(nex))) ) {
    _nc_stat(jet_d);
    POP(gop_u->tot_h);
    PUT(d_h, pro);
    BURN();
  }

  for ( c3_h i_h = 0; i_h < len_h; i_h++ ) {
    GAIN(nex[i_h]);
  }
  (void)FRAME(d_h);
  _nc_stat(dir_d);
  pog_u = gop_u;
  reg   = nex;
  ENTER(len_h);
OP_END

OP2(CAM, a_h, d_h)
  u3nc_dire* dir_u;
  u3nc_prog* gop_u;
  nc_frame*  fam_u;
  c3_h*      sot_h;
  c3_h       len_h;
  c3_h       kni_h;       //  argument cursor of _nc_knit(), address-taken
  u3_noun*   nex;
  u3_noun    x, o;

  SITE();
  GATHER(gop_u->tot_h);

  kni_h = 0;
  x     = u3nc(_nc_knit(gop_u->ned, nex, &kni_h), GAIN(u3t(dir_u->bell)));
  o     = u3z_find_m(dir_u->cid_h, 144 + c3__nock, x);

  if ( u3_none != o ) {
    POP(gop_u->tot_h);
    LOSE(x);
    PUT(d_h, o);
    BURN();
  }

  for ( c3_h i_h = 0; i_h < len_h; i_h++ ) {
    GAIN(nex[i_h]);
  }
  fam_u = FRAME(d_h);
  fam_u->key   = x;
  fam_u->cid_h = dir_u->cid_h;
  _nc_stat(dir_d);
  pog_u = gop_u;
  reg   = nex;
  ENTER(len_h);
OP_END

OP3(CSL, a_h, b_h, d_h)
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[a_h]);
  u3nc_prog* gop_u;
  u3_noun    x = reg[b_h];
  u3_noun    pro;

  if ( dir_u->ham_u ) {
    pro = u3j_kick_arm(GAIN(x), dir_u->ham_u, u3t(dir_u->ring));

    if ( u3_none != pro ) {
      _nc_stat(jet_d);
      PUT(d_h, pro);
      BURN();
    }
    LOSE(x);
  }

  _nc_stat(sub_d);
  u3t_off(noc_o);
  gop_u = _nc_entry(x, u3t(dir_u->bell));
  u3t_on(noc_o);
  GAIN(x);
  CALL(gop_u, x, d_h);
OP_END

OP3(CSM, a_h, b_h, d_h)
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[a_h]);
  u3nc_prog* gop_u;
  nc_frame*  fam_u;
  u3_noun*   nex;
  u3_noun    x = reg[b_h];
  u3_noun    o = u3nc(GAIN(x), GAIN(u3t(dir_u->bell)));
  u3_noun    pro = u3z_find_m(dir_u->cid_h, 144 + c3__nock, o);

  if ( u3_none != pro ) {
    LOSE(o);
    PUT(d_h, pro);
    BURN();
  }

  _nc_stat(sub_d);
  u3t_off(noc_o);
  gop_u = _nc_entry(x, u3t(dir_u->bell));
  u3t_on(noc_o);
  nex    = PUSH(gop_u->tot_h);
  nex[0] = GAIN(x);
  fam_u  = FRAME(d_h);
  fam_u->key   = o;
  fam_u->cid_h = dir_u->cid_h;
  pog_u = gop_u;
  reg   = nex;
  ENTER(1);
OP_END

OP2(CLQ, a_h, b_h)
  if ( c3n == u3du(reg[a_h]) ) {
    JUMP(b_h);
  }
  BURN();
OP_END

OP3(EQQ, a_h, b_h, c_h)
  if ( c3n == u3r_sing(reg[a_h], reg[b_h]) ) {
    JUMP(c_h);
  }
  BURN();
OP_END

OP3(EQI, a_h, b_h, c_h)
  if ( reg[b_h] != a_h ) {
    JUMP(c_h);
  }
  BURN();
OP_END

OP3(EQL, a_h, b_h, c_h)
  if ( c3n == u3r_sing(pog_u->lit_u.non[a_h], reg[b_h]) ) {
    JUMP(c_h);
  }
  BURN();
OP_END

OP2(BRN, a_h, b_h)
  u3_noun x = reg[a_h];
  if ( 1 == x ) {
    JUMP(b_h);
  }
  else if ( 0 != x ) {
    u3m_bail(c3__exit);
  }
  BURN();
OP_END

OP2(BRZ, a_h, b_h)
  if ( 0 != reg[a_h] ) {
    JUMP(b_h);
  }
  BURN();
OP_END

OP1(HOP, a_h)
  JUMP(a_h);
  BURN();
OP_END

OP1(JMP, a_h)
  u3nc_dire* dir_u;
  u3nc_prog* gop_u;
  nc_frame   fam;
  c3_h*      sot_h;
  c3_h       len_h;
  u3_noun*   nex;
  u3_noun    pro;

  SITE();

  //  without a jet to try first, distinct argument slots move to the
  //  callee in one pass
  //
  if ( !dir_u->arm_u && (c3y == dir_u->uni_o) ) {
    nex = PUSH(len_h);
    for ( c3_h i_h = 0; i_h < len_h; i_h++ ) {
      u3_noun* sot = &(reg[sot_h[i_h]]);
      nex[i_h] = *sot;
      *sot     = 0;
    }
    _nc_stat(dir_d);
    goto jmp_go;
  }

  GATHER(len_h);

  if ( dir_u->arm_u && (u3_none != (pro = dir_u->arm_u->arg_f(nex))) ) {
    _nc_stat(jet_d);
    POP(len_h);
    DONE(pro);
  }

  //  the caller's references move to the callee: the first argument
  //  from a slot takes its reference, any other from the same slot
  //  finds it gone and gains one
  //
  _nc_stat(dir_d);
  for ( c3_h i_h = 0; i_h < len_h; i_h++ ) {
    u3_noun* sot = &(reg[sot_h[i_h]]);

    if ( *sot ) {
      *sot = 0;
    }
    else {
      GAIN(nex[i_h]);
    }
  }

jmp_go:
  for ( c3_h i_h = 0; i_h < pog_u->tot_h; i_h++ ) {
    LOSE(reg[i_h]);
  }
  POP(len_h);
  fam = *(nc_frame*)TOP(_nc_frame_w);
  POP(_nc_frame_w + pog_u->tot_h);

  pog_u = gop_u;
  reg   = PUSH(pog_u->tot_h);
  memmove(reg, nex, len_h * sizeof(u3_noun));
  *(nc_frame*)PUSH(_nc_frame_w) = fam;
  ENTER(len_h);
OP_END

OP2(JSP, a_h, b_h)
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[a_h]);
  u3nc_prog* gop_u;
  u3_noun    x = reg[b_h];
  u3_noun    pro;

  if ( dir_u->ham_u ) {
    pro = u3j_kick_arm(GAIN(x), dir_u->ham_u, u3t(dir_u->ring));

    if ( u3_none != pro ) {
      _nc_stat(jet_d);
      DONE(pro);
    }
    LOSE(x);
  }

  _nc_stat(sub_d);
  u3t_off(noc_o);
  gop_u = _nc_entry(x, u3t(dir_u->bell));
  u3t_on(noc_o);
  reg[b_h] = 0;
  TAIL(gop_u, x);
OP_END

OP1(DON, a_h)
  u3_noun pro = reg[a_h];
  reg[a_h] = 0;
  DONE(pro);
OP_END

OP0(BOM)
  u3m_bail(c3__exit);
OP_END

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
#undef PUT
#undef PUSH
#undef POP
#undef TOP
#undef OP0
#undef OP1
#undef OP2
#undef OP3
#undef OP_END
#undef FRAME
#undef SITE
#undef GATHER
#undef ENTER
#undef CALL
#undef TAIL
#undef DONE
