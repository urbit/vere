/// @file

#include "nock-compile.h"

#include "allocate.h"
#include "c3/defs.h"
#include "c3/motes.h"
#include "direct.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "jets/k.h"
#include "jets/q.h"
#include "jets/u.h"
#include "log.h"
#include "manage.h"
#include "nock.h"
#include "options.h"
#include "retrieve.h"
#include "trace.h"
#include "vortex.h"
#include "xtract.h"
#include "zave.h"

/*  Bytecode.
**
**  Every opcode with immediate arguments comes in three widths, for all
**  of its immediates at once: _B, one byte each; _S, two bytes each,
**  little-endian; _V, a length byte followed by that many bytes.
**  Slots, literal and call site indices, and jump targets (absolute byte
**  offsets) are immediates.  The IMM opcodes are the exception: IMM_B
**  and IMM_S carry the atom itself, and all of them write to a one-byte
**  slot.  A call site holds its argument slots and its destination slot
**  d, so the call opcodes carry only the site index.
**
**  Slots hold counted references.  An op reads a source slot either
**  borrowed, gaining a reference for its product and leaving the slot as
**  it was, or consumed: the slot gives up its reference to the op and is
**  zeroed.  An op with sources comes in a variant per subset of its
**  sources consumed, _0 (none) to _3 (both), bit i for source i; a call
**  site holds a consume flag per argument instead.  The compiler consumes
**  a register at its last use and drops (DRO) the registers that die
**  anywhere else (see _nc_lives()), so a slot holds a reference exactly
**  while the register in it is live, a destination slot is always zero
**  when written, and an activation ends with every slot zero but the one
**  it returns.
**
**    IMM_0 d        0 -> d
**    IMM_1 d        1 -> d
**    IMM_B n d      n -> d
**    IMM_S n d      n -> d
**    IML   i d      literal i -> d
**    MOV   s d      s -> d
**    INC   s d      +(s) -> d
**    DEC   s d      (dec s) -> d, by the jet
**    ADD   a b d    (add a b) -> d, by the jet
**    CON   h t d    [h t] -> d
**    HED   s d      -.s -> d, 0 if s is an atom
**    TAL   s d      +.s -> d, 0 if s is an atom
**    CEL   p        crash unless p is a cell
**    LOB   p        crash unless p is a loobean
**    EQU   l r      =(l r), for the side effect
**    HSP   i        static hint prologue, literal i is [hint formula]
**    HSE   i        static hint epilogue
**    HDP   i p      dynamic hint prologue, clue in p
**    HDE   i p      dynamic hint epilogue
**    SPY   e p d    .^(e p) -> d
**    NOK   u f d    .*(u f) -> d, through the SKA core
**    CAL   i        call site i with its arguments -> d
**    CAF   i        CAL, by the total array jet of the site
**    CAP   i        call site i with the whole subject, its one argument,
**                   -> d: by the u3w jet of the site if it has one and
**                   doesn't punt, else by the unary program of the callee
**    CAM   i        CAL, memoized
**    CSM   i        CAP, memoized, without a jet
**    CLQ   s t      goto t unless s is a cell
**    EQQ   l r t    goto t unless =(l r)
**    EQI   n s t    goto t unless =(n s), n a direct atom
**    EQL   i s t    goto t unless =(literal i, s)
**    BRN   s t      goto t if s is 1, crash unless s is a loobean
**    BRZ   s t      goto t unless s is 0
**    HOP   t        goto t
**    JMP   i        CAL in tail position
**    JMS   i        JMP of the running program: its arguments move into
**                   the parameter slots and it starts over (a loop)
**    JMF   i        CAF in tail position
**    JSP   i        CAP in tail position
**    DON   s        return s, consumed
**    DRO   s        drop s: lose its reference, 0 -> s
**    SWK   i        sweep: drop every slot but the argument slots of
**                   site i, the registers live into the block
**    BOM            crash
**
**  DON, JMP, JMS, JMF and JSP end the activation, and come in a sweeping
**  variant (_1) that first drops every slot it doesn't take: it stands
**  for the drops at the start of a block that does nothing else.
*/

/*  Families of the IR ops: X(fam, OP, src, dst, imm, tar, sot, kon, swe):
**  the family, its opcode name, the number of source slots in the op,
**  whether it has a destination slot, an index immediate (literal or
**  call site), a jump target, a call site (whose argument slots are
**  sources and which holds the destination slot, if any), the number of
**  variant bits in the opcode, and whether the variant bit means a sweep
**  rather than a consumed source.  A consume bit per source, except for
**  a call (the flags are in the site) and for DON and DRO, which always
**  consume.  The special families bom, imm, nop and kil have no opcodes
**  of their own: imm encodes as an IMM opcode or as IML, nop and kil as
**  nothing.  kil defines a register without code: a parameter of a block
**  that a jump to it leaves undefined, so that the register is not live
**  on the way to that jump.
*/
#define FAMILIES(X)                                                      \
  X(iml, IML, 0, 1, 1, 0, 0, 0, 0)                                       \
  X(mov, MOV, 1, 1, 0, 0, 0, 1, 0)                                       \
  X(inc, INC, 1, 1, 0, 0, 0, 1, 0)                                       \
  X(dec, DEC, 1, 1, 0, 0, 0, 1, 0)                                       \
  X(add, ADD, 2, 1, 0, 0, 0, 2, 0)                                       \
  X(con, CON, 2, 1, 0, 0, 0, 2, 0)                                       \
  X(hed, HED, 1, 1, 0, 0, 0, 1, 0)                                       \
  X(tal, TAL, 1, 1, 0, 0, 0, 1, 0)                                       \
  X(cel, CEL, 1, 0, 0, 0, 0, 1, 0)                                       \
  X(lob, LOB, 1, 0, 0, 0, 0, 1, 0)                                       \
  X(equ, EQU, 2, 0, 0, 0, 0, 2, 0)                                       \
  X(hsp, HSP, 0, 0, 1, 0, 0, 0, 0)                                       \
  X(hse, HSE, 0, 0, 1, 0, 0, 0, 0)                                       \
  X(hdp, HDP, 1, 0, 1, 0, 0, 1, 0)                                       \
  X(hde, HDE, 1, 0, 1, 0, 0, 1, 0)                                       \
  X(spy, SPY, 2, 1, 0, 0, 0, 2, 0)                                       \
  X(nok, NOK, 2, 1, 0, 0, 0, 2, 0)                                       \
  X(cal, CAL, 0, 0, 1, 0, 1, 0, 0)                                       \
  X(caf, CAF, 0, 0, 1, 0, 1, 0, 0)                                       \
  X(cap, CAP, 0, 0, 1, 0, 1, 0, 0)                                       \
  X(cam, CAM, 0, 0, 1, 0, 1, 0, 0)                                       \
  X(csm, CSM, 0, 0, 1, 0, 1, 0, 0)                                       \
  X(clq, CLQ, 1, 0, 0, 1, 0, 1, 0)                                       \
  X(eqq, EQQ, 2, 0, 0, 1, 0, 2, 0)                                       \
  X(eqi, EQI, 1, 0, 1, 1, 0, 1, 0)                                       \
  X(eql, EQL, 1, 0, 1, 1, 0, 1, 0)                                       \
  X(brn, BRN, 1, 0, 0, 1, 0, 1, 0)                                       \
  X(brz, BRZ, 1, 0, 0, 1, 0, 1, 0)                                       \
  X(hop, HOP, 0, 0, 0, 1, 0, 0, 0)                                       \
  X(jmp, JMP, 0, 0, 1, 0, 1, 1, 1)                                       \
  X(jms, JMS, 0, 0, 1, 0, 1, 1, 1)                                       \
  X(jmf, JMF, 0, 0, 1, 0, 1, 1, 1)                                       \
  X(jsp, JSP, 0, 0, 1, 0, 1, 1, 1)                                       \
  X(don, DON, 1, 0, 0, 0, 0, 1, 1)                                       \
  X(dro, DRO, 1, 0, 0, 0, 0, 0, 0)                                       \
  X(swk, SWK, 0, 0, 1, 0, 1, 0, 0)

#define _nc_fam_enum(fam, OP, src, dst, imm, tar, sot, kon, swe)  _nc_##fam,
enum { FAMILIES(_nc_fam_enum) _nc_bom, _nc_imm, _nc_nop, _nc_kil };
#undef _nc_fam_enum

/*  The opcodes of a family, in the order of the consume variants and,
**  within a variant, of the widths; OPCODES lists them all, through Y.
*/
#define _nc_wid(op)        Y(op##_B) Y(op##_S) Y(op##_V)
#define _nc_kon_0(op)      _nc_wid(op)
#define _nc_kon_1(op)      _nc_wid(op##_0) _nc_wid(op##_1)
#define _nc_kon_2(op)      _nc_wid(op##_0) _nc_wid(op##_1)                 \
                           _nc_wid(op##_2) _nc_wid(op##_3)
#define _nc_fam_ops(fam, OP, src, dst, imm, tar, sot, kon, swe)  _nc_kon_##kon(OP)

#define OPCODES  Y(IMM_0) Y(IMM_1) Y(IMM_B) Y(IMM_S)                     \
                 FAMILIES(_nc_fam_ops)                                   \
                 Y(BOM)

#define Y(op) op,
enum { OPCODES LAST };
#undef Y

#define Y(op) #op,
static const c3_c* _nc_name_c[] = { OPCODES };
#undef Y

/* _nc_fam: operands of a family: number of source registers, whether
**          there is a destination register in the op, an index immediate
**          (literal or call site), a jump target, whether the op has a
**          call site, whose argument slots are sources and which holds the
**          destination register, if any, the number of variant bits, and
**          whether they mean a sweep rather than consumed sources.
*/
#define _nc_fam_row(fam, OP, src, dst, imm, tar, sot, kon, swe)          \
  [_nc_##fam] = { src, dst, imm, tar, sot, kon, swe },
static const struct {
  c3_y src_y, dst_y, imm_y, tar_y, sot_y, kon_y, swe_y;
} _nc_fam[] = {
  FAMILIES(_nc_fam_row)
  [_nc_bom] = { 0, 0, 0, 0, 0, 0, 0 },
  [_nc_imm] = { 0, 1, 0, 0, 0, 0, 0 },
  [_nc_nop] = { 0, 0, 0, 0, 0, 0, 0 },
  [_nc_kil] = { 0, 1, 0, 0, 0, 0, 0 },
};
#undef _nc_fam_row

/* _nc_bas_y: first opcode of a family; the opcode of an op is
**            _nc_bas_y[fam] + 3 * variant bits + width.
*/
#define _nc_first_0(op)  op##_B
#define _nc_first_1(op)  op##_0_B
#define _nc_first_2(op)  op##_0_B
#define _nc_fam_bas(fam, OP, src, dst, imm, tar, sot, kon, swe)          \
  [_nc_##fam] = _nc_first_##kon(OP),
static const c3_y _nc_bas_y[] = { FAMILIES(_nc_fam_bas) };
#undef _nc_fam_bas

/* _nc_fam_c: names of the families, for _nc_print().
*/
#define _nc_fam_nam(fam, OP, src, dst, imm, tar, sot, kon, swe)  [_nc_##fam] = #OP,
static const c3_c* _nc_fam_c[] = { FAMILIES(_nc_fam_nam) };
#undef _nc_fam_nam

#define _nc_none       ((c3_h)-1)

//  layout of an op:
//  [OP][source_registers?][destination_register?][index?][jump_target?]

/* nc_op: an IR op, with registers until slots are assigned.
*/
typedef struct {
  c3_y  fam_y;    //  family of the op
  c3_y  kon_y;    //  variant bits: consumed sources, bit i for src_h[i],
                  //  or a sweep (_nc_lives())
  c3_y  ded_y;    //  the product is never used: drop it (_nc_lives())
  c3_h  src_h[2]; //  source registers
  c3_h  dst_h;    //  destination register (a call's moves to its site)
  c3_h  imm_h;    //  index immediate, or the atom of an inline immediate
  c3_h  tar_h;    //  target block, or the literal of an inline immediate
  c3_h  pos_h;    //  position of the op for the live intervals of
                  //  _nc_slots(), or 0 for its place in the layout
} nc_op;

/* nc_blk: a basic block.
*/
typedef struct {
  u3_noun blob;
  c3_h    fir_h;    //  first op in the nc_op array
  c3_h    len_h;    //  number of ops in the nc_op array
  c3_h    off_h;    //  byte offset
  c3_h    lay_h;    //  position in the layout, or _nc_none if unreachable
} nc_blk;

/* nc_jet: a ring [path axis], resolved to its drivers.
*/
typedef struct {
  u3_noun         ring;   //  [path axis] of the jet, or ~
  u3j_harm*       ham_u;  //  u3w driver, taking the core, nullable
  const u3u_harm* arm_u;  //  array driver, taking the arguments, nullable
} nc_jet;

/* nc_dir: a call site, with registers until slots are assigned.
*/
typedef struct {
  u3_noun bell;
  nc_jet  jet_u;
  c3_h    cid_h;
  c3_h    des_h;  //  destination slot, set by _nc_emit() from the op
  c3_o    mon_o;  //  call by subject: the one argument is the whole subject
  c3_h    sot_h;
  c3_h    len_h;
} nc_dir;

/* nc_gen: state of a compilation.
*/
typedef struct {
  u3p(u3h_root) idx_p;    //  block id -> index
  nc_blk*       blk_u;    //  blocks
  c3_h          blk_h;    //  block index generator
  c3_h*         lay_h;    //  block indices in layout order
  c3_h          lan_h;    //    and its length
  nc_op*        ops_u;    //  op array
  c3_h          opc_h;    //    and its capacity
  c3_h          opn_h;    //    and its length
  nc_dir*       dir_u;    //  callsite array
  c3_h          dir_h;    //    and its capacity
  c3_h          din_h;    //    and its length
  c3_h*         pol_h;    //  argument registers of call sites
  c3_h          pon_h;    //    and its length
  c3_h          poc_h;    //    and its capacity
  c3_y*         pok_y;    //  consume flags of the argument registers
  c3_h          pkc_h;    //    and its capacity (grows with pol_h)
  u3p(u3h_root) lit_p;    //  literal -> index
  c3_h          lit_h;
  c3_h          reg_h;    //  registers
  c3_h          tot_h;    //  slots
  u3_weak       bell;     //  bell of the program, if a tail call to it can
                          //  be a self call (a direct program), else none
} nc_gen;

#define _nc_grow(arr, len, cap, typ)                                     \
  if ( (len) >= (cap) ) {                                                \
    c3_h _old = (cap);                                                   \
    if ( _old >= 0x80000000 ) {                                          \
      u3m_bail(c3__meme);                                                \
    }                                                                    \
    (cap) = (cap) ? (2 * (cap)) : 64;                                     \
    (arr) = u3a_realloc((arr), _old * sizeof(typ), (cap) * sizeof(typ)); \
  }

/* u3nc_Stat: cumulative counters, printed after each u3nc_nock_on()
**            when U3NC_VERBOSE is set.  The ones in the interpreter
**            loop are only kept when compiled with U3NC_STAT.
*/
u3nc_stat u3nc_Stat;

#ifdef U3NC_STAT
#  define _nc_stat(fel)  (u3nc_Stat.fel++)
#  define _nc_stat_if(con, fel)  do { if ( con ) { u3nc_Stat.fel++; } } while ( 0 )
#else
#  define _nc_stat(fel)  ((void)0)
#  define _nc_stat_if(con, fel)  ((void)0)
#endif

#ifdef U3NC_VERBOSE
static const c3_t _nc_verb_t = 1;
#else
static const c3_t _nc_verb_t = 0;
#endif

/* _nc_cat(): direct atom below _nc_none, or fail.
*/
static inline c3_h
_nc_cat(u3_noun som)
{
  if ( (c3n == u3a_is_cat(som)) || (som >= _nc_none) ) {
    u3m_bail(c3__fail);
  }
  return som;
}

/* _nc_reg(): register of an IR op.
*/
static c3_h
_nc_reg(nc_gen* gen_u, u3_noun som)
{
  c3_h reg_h = _nc_cat(som);
  gen_u->reg_h = c3_max(gen_u->reg_h, reg_h + 1);
  return reg_h;
}

/* _nc_lit(): index of a literal.  RETAINS.
*/
static c3_h
_nc_lit(nc_gen* gen_u, u3_noun som)
{
  u3_weak got = u3h_git(gen_u->lit_p, som);

  if ( u3_none == got ) {
    got = gen_u->lit_h++;
    u3h_put(gen_u->lit_p, som, got);
  }

  return got;
}

/* _nc_blk(): index of a block by id.  RETAINS.
*/
static c3_h
_nc_blk(nc_gen* gen_u, u3_noun id)
{
  u3_weak got = u3h_git(gen_u->idx_p, id);

  if ( u3_none == got ) {
    u3m_bail(c3__fail);
  }

  return got;
}

/* _nc_op_init(): initialize an op of a family, without operands.
*/
static nc_op*
_nc_op_init(nc_op* op_u, c3_y fam_y)
{
  op_u->fam_y    = fam_y;
  op_u->kon_y    = 0;
  op_u->ded_y    = 0;
  op_u->src_h[0] = _nc_none;
  op_u->src_h[1] = _nc_none;
  op_u->dst_h    = _nc_none;
  op_u->imm_h    = 0;
  op_u->tar_h    = _nc_none;
  op_u->pos_h    = 0;

  return op_u;
}

/* _nc_op(): append an op.
*/
static nc_op*
_nc_op(nc_gen* gen_u, c3_y fam_y)
{
  _nc_grow(gen_u->ops_u, gen_u->opn_h, gen_u->opc_h, nc_op);
  return _nc_op_init(&(gen_u->ops_u[gen_u->opn_h++]), fam_y);
}

/* _nc_cid(): memo cache for a %memo clue, as in nock.c.  RETAINS.
*/
static c3_h
_nc_cid(u3_noun clu)
{
  if ( c3n == u3du(clu) ) {
    return u3z_memo_toss;
  }
  else if (  (c3y == u3du(u3t(clu)))
          && (c3__clay == u3h(clu))
          && (c3__ford == u3h(u3t(clu))) )
  {
    return u3z_memo_ford;
  }
  else {
    return u3z_memo_keep;
  }
}

/* _nc_lent(): length of a (list register).  RETAINS.
*/
static c3_h
_nc_lent(u3_noun lis)
{
  c3_h len_h = 0;

  for ( ; u3_nul != lis; lis = u3t(lis) ) {
    len_h++;
  }

  return len_h;
}

/* _nc_ring(): resolve a ring [path axis], or ~, to its drivers.  RETAINS.
*/
static nc_jet
_nc_ring(u3_noun ring)
{
  nc_jet jet_u = { ring, NULL, NULL };

  if ( u3_nul == ring ) {
    return jet_u;
  }

  if ( c3n == u3j_ring(ring, &(jet_u.ham_u), &(jet_u.arm_u)) ) {
    _nc_stat(rin_d);
    if ( _nc_verb_t ) {
      c3_c* pax_c = u3m_pretty_path_road(u3h(ring));
      u3l_log("u3nc: no driver for ring %s", pax_c);
      u3a_free(pax_c);
    }
  }
  else {
    _nc_stat(arm_d);
  }

  return jet_u;
}

/* _nc_kind(): the kind of a call with len_h arguments through a resolved
**             ring: plain, with no array driver (the arguments are the
**             analysis's); of a total array driver on the arguments; or
**             by subject, of a u3w driver that may punt, whose one argument
**             is the whole subject.  Bails if the arguments don't fit the
**             driver: the %jets table of the SKA core and u3u_Harm then
**             disagree.
*/
enum { _nc_kind_plain, _nc_kind_total, _nc_kind_punt };

static c3_y
_nc_kind(const nc_jet* jet_u, c3_h len_h)
{
  if ( !jet_u->arm_u ) {
    return _nc_kind_plain;
  }

  if ( len_h != jet_u->arm_u->len_w ) {
    u3m_bail(c3__fail);
  }

  return ( jet_u->arm_u->pun_t ) ? _nc_kind_punt : _nc_kind_total;
}

/* _nc_jet_op(): an op standing in for a call of a total array jet, for
**               the jets the interpreter knows as ops: +dec and +add.
**               arg: (list register), fitting the jet.  RETAINS.
**               Produces NULL if there is no such op.
*/
static nc_op*
_nc_jet_op(nc_gen* gen_u, const u3u_harm* arm_u, u3_noun arg)
{
  nc_op* op_u = NULL;

  if ( u3ua_dec == arm_u->arg_f ) {
    op_u = _nc_op(gen_u, _nc_dec);
    op_u->src_h[0] = _nc_reg(gen_u, u3h(arg));
  }
  else if ( u3ua_add == arm_u->arg_f ) {
    op_u = _nc_op(gen_u, _nc_add);
    op_u->src_h[0] = _nc_reg(gen_u, u3h(arg));
    op_u->src_h[1] = _nc_reg(gen_u, u3h(u3t(arg)));
  }

  return op_u;
}

/* _nc_dir(): append a call site.  RETAINS.
**            bell: callee; jet_u: its ring, resolved; clu: memo clue, or
**            u3_none; arg: (list register); mon_o: a call by subject, its
**            one argument the whole subject, of the unary program of the
**            bell.
*/
static c3_h
_nc_dir(nc_gen* gen_u, u3_noun bell, nc_jet jet_u, u3_weak clu, u3_noun arg,
        c3_o mon_o)
{
  nc_dir* dir_u;

  _nc_grow(gen_u->dir_u, gen_u->din_h, gen_u->dir_h, nc_dir);
  dir_u = &(gen_u->dir_u[gen_u->din_h]);

  dir_u->bell  = u3k(bell);
  dir_u->jet_u = jet_u;
  dir_u->cid_h = ( u3_none == clu ) ? 0 : _nc_cid(clu);
  dir_u->des_h = _nc_none;
  dir_u->mon_o = mon_o;
  dir_u->sot_h = gen_u->pon_h;
  dir_u->len_h = 0;
  u3k(jet_u.ring);

  while ( u3_nul != arg ) {
    u3_noun i;
    u3x_cell(arg, &i, &arg);
    _nc_grow(gen_u->pol_h, gen_u->pon_h, gen_u->poc_h, c3_h);
    _nc_grow(gen_u->pok_y, gen_u->pon_h, gen_u->pkc_h, c3_y);
    gen_u->pok_y[gen_u->pon_h]   = 0;
    gen_u->pol_h[gen_u->pon_h++] = _nc_reg(gen_u, i);
    dir_u->len_h++;
  }

  u3_assert( (c3n == mon_o) || (1 == dir_u->len_h) );

  return gen_u->din_h++;
}

/* _nc_dir_kept(): append the keep set of a sweep (SWK) as a pseudo call
**                 site: no bell (0, never a bell), no jet, and the
**                 registers as its arguments.
*/
static c3_h
_nc_dir_kept(nc_gen* gen_u, const c3_h* reg_h, c3_h len_h)
{
  nc_dir* dir_u;
  c3_h    i_h;

  _nc_grow(gen_u->dir_u, gen_u->din_h, gen_u->dir_h, nc_dir);
  dir_u = &(gen_u->dir_u[gen_u->din_h]);

  dir_u->bell  = 0;
  dir_u->jet_u = _nc_ring(u3_nul);
  dir_u->cid_h = 0;
  dir_u->des_h = _nc_none;
  dir_u->mon_o = c3n;
  dir_u->sot_h = gen_u->pon_h;
  dir_u->len_h = len_h;

  for ( i_h = 0; i_h < len_h; i_h++ ) {
    _nc_grow(gen_u->pol_h, gen_u->pon_h, gen_u->poc_h, c3_h);
    _nc_grow(gen_u->pok_y, gen_u->pon_h, gen_u->pkc_h, c3_y);
    gen_u->pok_y[gen_u->pon_h]   = 0;
    gen_u->pol_h[gen_u->pon_h++] = reg_h[i_h];
  }

  return gen_u->din_h++;
}

/* _nc_dir_sub(): append a call site by subject, in register sub.  RETAINS.
*/
static c3_h
_nc_dir_sub(nc_gen* gen_u, u3_noun bell, nc_jet jet_u, u3_weak clu, u3_noun sub)
{
  u3_noun arg   = u3nc(sub, u3_nul);
  c3_h    dir_h = _nc_dir(gen_u, bell, jet_u, clu, arg, c3y);

  u3z(arg);
  return dir_h;
}

/* _nc_call(): translate a call with arguments through a ring, in tail
**             position if tal_t, as an op for its kind (_nc_kind()): a
**             plain call; a call of the total array jet, or the op standing
**             in for it; or a call by subject, of its one argument.
**             RETAINS.
*/
static nc_op*
_nc_call(nc_gen* gen_u, u3_noun bell, u3_noun ring, u3_noun arg, c3_t tal_t)
{
  nc_jet jet_u = _nc_ring(ring);
  nc_op* op_u;

  switch ( _nc_kind(&jet_u, _nc_lent(arg)) ) {
    default:  u3_assert(0);

    case _nc_kind_plain: {
      op_u = _nc_op(gen_u, tal_t ? _nc_jmp : _nc_cal);
      op_u->imm_h = _nc_dir(gen_u, bell, jet_u, u3_none, arg, c3n);
    } break;

    case _nc_kind_total: {
      if ( !tal_t && (op_u = _nc_jet_op(gen_u, jet_u.arm_u, arg)) ) {
        break;
      }
      op_u = _nc_op(gen_u, tal_t ? _nc_jmf : _nc_caf);
      op_u->imm_h = _nc_dir(gen_u, bell, jet_u, u3_none, arg, c3n);
    } break;

    case _nc_kind_punt: {
      op_u = _nc_op(gen_u, tal_t ? _nc_jsp : _nc_cap);
      op_u->imm_h = _nc_dir(gen_u, bell, jet_u, u3_none, arg, c3y);
    } break;
  }

  return op_u;
}

/* _nc_kids(): successor blocks of a block, in the order to visit them
**             so that the fall-through successor ends up next.
*/
static c3_h
_nc_kids(nc_gen* gen_u, u3_noun blob, c3_h* kid_h)
{
  u3_noun fin = u3t(u3t(blob));
  u3_noun tag, a, z, o;

  u3x_cell(fin, &tag, &a);

  switch ( tag ) {
    default: {
      return 0;
    }

    case c3__hop: {
      kid_h[0] = _nc_blk(gen_u, u3t(a));
      return 1;
    }

    case c3__eqq: {
      u3x_qual(a, NULL, NULL, &z, &o);
      break;
    }

    case c3__clq:
    case c3__brn:
    case c3__brz: {
      u3x_trel(a, NULL, &z, &o);
      break;
    }
  }

  kid_h[0] = _nc_blk(gen_u, u3t(o));
  kid_h[1] = _nc_blk(gen_u, u3t(z));
  return 2;
}

/* _nc_blocks(): collect the blocks of a straight, and lay them out in
**               topological order from the entry block. RETAINS
*/
static void
_nc_blocks(nc_gen* gen_u, u3_noun map)
{
  //  the treap of the map, in preorder
  //
  {
    u3_noun* sak = NULL;
    //  size, capacity of sac array, capacity of blk_u array
    c3_h     sak_h = 0, sac_h = 0, blk_m = 0;

    _nc_grow(sak, sak_h, sac_h, u3_noun);
    sak[sak_h++] = map;

    while ( sak_h ) {
      u3_noun nod = sak[--sak_h], n, l, r;

      if ( u3_nul == nod ) {
        continue;
      }

      u3x_trel(nod, &n, &l, &r);

      _nc_grow(gen_u->blk_u, gen_u->blk_h, blk_m, nc_blk);
      //  blk_u[i].blob holds an uncounted reference, but it's fine as long
      //  as we don't run u3r_sing during compilation and `straight` noun
      //  outlives this reference
      gen_u->blk_u[gen_u->blk_h].blob  = u3t(n);
      gen_u->blk_u[gen_u->blk_h].lay_h = _nc_none;
      u3h_put(gen_u->idx_p, u3h(n), _nc_cat(gen_u->blk_h));
      gen_u->blk_h++;

      _nc_grow(sak, sak_h + 1, sac_h, u3_noun);
      sak[sak_h++] = l;
      sak[sak_h++] = r;
    }

    u3a_free(sak);
  }

  //  reverse postorder of a depth-first search from the entry block
  //
  {
    c3_h  blk_h = gen_u->blk_h;
    c3_h* sak_h = u3a_malloc(blk_h * sizeof(c3_h));
    c3_y* nex_y = u3a_calloc(blk_h, sizeof(c3_y));
    c3_y* saw_y = u3a_calloc(blk_h, sizeof(c3_y));
    c3_h  dep_h = 0;  // stack depth
    c3_h  kid_h[2];

    gen_u->lay_h = u3a_malloc(blk_h * sizeof(c3_h));
    gen_u->lan_h = 0;

    sak_h[dep_h++] = _nc_blk(gen_u, 0);
    saw_y[sak_h[0]] = 1;

    while ( dep_h ) {
      c3_h top_h = sak_h[dep_h - 1];
      c3_h kin_h = _nc_kids(gen_u, gen_u->blk_u[top_h].blob, kid_h);

      if ( nex_y[top_h] < kin_h ) {
        c3_h kid = kid_h[nex_y[top_h]++];

        if ( !saw_y[kid] ) {
          saw_y[kid] = 1;
          sak_h[dep_h++] = kid;
        }
      }
      else {
        dep_h--;
        gen_u->lay_h[gen_u->lan_h++] = top_h;
      }
    }

    for ( c3_h i_h = 0; i_h < gen_u->lan_h / 2; i_h++ ) {
      c3_h j_h = gen_u->lan_h - 1 - i_h;
      c3_h tmp_h = gen_u->lay_h[i_h];
      gen_u->lay_h[i_h] = gen_u->lay_h[j_h];
      gen_u->lay_h[j_h] = tmp_h;
    }

    for ( c3_h i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      gen_u->blk_u[gen_u->lay_h[i_h]].lay_h = i_h;
    }

    u3a_free(sak_h);
    u3a_free(nex_y);
    u3a_free(saw_y);
  }
}

/* _nc_pole(): translate a non-control-flow op.  RETAINS.
*/
static void
_nc_pole(nc_gen* gen_u, u3_noun pole)
{
  u3_noun tag, arg, a, b, c, d;
  nc_op*  op_u;

  u3x_cell(pole, &tag, &arg);

  switch ( tag ) {
    default: {
      u3m_bail(c3__fail);
    }

    case c3__imm: {
      u3x_cell(arg, &a, &b);
      if ( (c3y == u3a_is_cat(a)) && (a < 0x10000) ) {
        op_u = _nc_op(gen_u, _nc_imm);
        op_u->imm_h = a;
        op_u->tar_h = _nc_lit(gen_u, a);
      }
      else {
        op_u = _nc_op(gen_u, _nc_iml);
        op_u->imm_h = _nc_lit(gen_u, a);
      }
      op_u->dst_h = _nc_reg(gen_u, b);
    } break;

    case c3__mov:
    case c3__inc:
    case c3__hed:
    case c3__tal: {
      c3_y fam_y = ( c3__mov == tag ) ? _nc_mov
                 : ( c3__inc == tag ) ? _nc_inc
                 : ( c3__hed == tag ) ? _nc_hed
                 : _nc_tal;
      u3x_cell(arg, &a, &b);
      op_u = _nc_op(gen_u, fam_y);
      op_u->src_h[0] = _nc_reg(gen_u, a);
      op_u->dst_h    = _nc_reg(gen_u, b);
    } break;

    case c3__con: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_con);
      op_u->src_h[0] = _nc_reg(gen_u, a);
      op_u->src_h[1] = _nc_reg(gen_u, b);
      op_u->dst_h    = _nc_reg(gen_u, c);
    } break;

    case c3__cel:
    case c3__lob: {
      op_u = _nc_op(gen_u, ( c3__cel == tag ) ? _nc_cel : _nc_lob);
      op_u->src_h[0] = _nc_reg(gen_u, arg);
    } break;

    case c3__equ: {
      u3x_cell(arg, &a, &b);
      op_u = _nc_op(gen_u, _nc_equ);
      op_u->src_h[0] = _nc_reg(gen_u, a);
      op_u->src_h[1] = _nc_reg(gen_u, b);
    } break;

    case c3__hsp:
    case c3__hse: {
      op_u = _nc_op(gen_u, ( c3__hsp == tag ) ? _nc_hsp : _nc_hse);
      op_u->imm_h = _nc_lit(gen_u, arg);
    } break;

    case c3__hdp:
    case c3__hde: {
      u3_noun hin;
      u3x_trel(arg, &a, &b, &c);
      hin  = u3nc(u3k(a), u3k(c));
      op_u = _nc_op(gen_u, ( c3__hdp == tag ) ? _nc_hdp : _nc_hde);
      op_u->imm_h    = _nc_lit(gen_u, hin);
      op_u->src_h[0] = _nc_reg(gen_u, b);
      u3z(hin);
    } break;

    case c3__spy:
    case c3__nok: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, ( c3__spy == tag ) ? _nc_spy : _nc_nok);
      op_u->src_h[0] = _nc_reg(gen_u, a);
      op_u->src_h[1] = _nc_reg(gen_u, b);
      op_u->dst_h    = _nc_reg(gen_u, c);
    } break;

    case c3__cal: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_cal);
      op_u->imm_h = _nc_dir(gen_u, a, _nc_ring(u3_nul), u3_none, b, c3n);
      op_u->dst_h = _nc_reg(gen_u, c);
    } break;

    case c3__caf: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_call(gen_u, a, d, b, 0);
      op_u->dst_h = _nc_reg(gen_u, c);
    } break;

    case c3__cam: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_cam);
      op_u->imm_h = _nc_dir(gen_u, a, _nc_ring(u3_nul), d, b, c3n);
      op_u->dst_h = _nc_reg(gen_u, c);
    } break;

    //  calls by subject: the u3w driver of the ring, if any, tries first
    //
    case c3__csl:
    case c3__csf: {
      d = u3_nul;
      if ( c3__csf == tag ) {
        u3x_qual(arg, &a, &b, &c, &d);
      }
      else {
        u3x_trel(arg, &a, &b, &c);
      }
      op_u = _nc_op(gen_u, _nc_cap);
      op_u->imm_h = _nc_dir_sub(gen_u, a, _nc_ring(d), u3_none, b);
      op_u->dst_h = _nc_reg(gen_u, c);
    } break;

    case c3__csm: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_csm);
      op_u->imm_h = _nc_dir_sub(gen_u, a, _nc_ring(u3_nul), d, b);
      op_u->dst_h = _nc_reg(gen_u, c);
    } break;
  }
}

/* _nc_jump(): translate a jump [args there] to a block, with its
**             arguments moved into the parameters of the target; a ~
**             argument leaves its parameter undefined, which a kil op
**             records for the liveness analysis (_nc_lives()).
**             nex_h: the block laid out next, or _nc_none.
*/
static void
_nc_jump(nc_gen* gen_u, u3_noun jmp, c3_h nex_h)
{
  u3_noun arg, id, par, a, p;
  c3_h    tar_h;

  u3x_cell(jmp, &arg, &id);
  tar_h = _nc_blk(gen_u, id);
  par   = u3h(gen_u->blk_u[tar_h].blob);

  while ( u3_nul != arg ) {
    u3x_cell(arg, &a, &arg);
    u3x_cell(par, &p, &par);

    if ( c3y == u3du(a) ) {
      nc_op* op_u = _nc_op(gen_u, _nc_mov);
      op_u->src_h[0] = _nc_reg(gen_u, u3t(a));
      op_u->dst_h    = _nc_reg(gen_u, p);
    }
    else {
      nc_op* op_u = _nc_op(gen_u, _nc_kil);
      op_u->dst_h = _nc_reg(gen_u, p);
    }
  }

  if ( u3_nul != par ) {
    u3m_bail(c3__fail);
  }

  if ( tar_h != nex_h ) {
    nc_op* op_u = _nc_op(gen_u, _nc_hop);
    op_u->tar_h = tar_h;
  }
}

/* _nc_bare(): block of a jump that carries no arguments.
*/
static c3_h
_nc_bare(nc_gen* gen_u, u3_noun jmp)
{
  if ( u3_nul != u3h(jmp) ) {
    u3m_bail(c3__fail);
  }
  return _nc_blk(gen_u, u3t(jmp));
}

/* _nc_fin(): translate a control-flow op.  RETAINS.
*/
static void
_nc_fin(nc_gen* gen_u, u3_noun fin, c3_h nex_h)
{
  u3_noun tag, arg, a, b, c, d;
  nc_op*  op_u;

  u3x_cell(fin, &tag, &arg);

  switch ( tag ) {
    default: {
      u3m_bail(c3__fail);
    }

    case c3__clq:
    case c3__brn:
    case c3__brz: {
      c3_y fam_y = ( c3__clq == tag ) ? _nc_clq
                 : ( c3__brn == tag ) ? _nc_brn
                 : _nc_brz;
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, fam_y);
      op_u->src_h[0] = _nc_reg(gen_u, a);
      op_u->tar_h    = _nc_bare(gen_u, c);
      c = b;
    } break;

    case c3__eqq: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_eqq);
      op_u->src_h[0] = _nc_reg(gen_u, a);
      op_u->src_h[1] = _nc_reg(gen_u, b);
      op_u->tar_h    = _nc_bare(gen_u, d);
    } break;

    case c3__hop: {
      _nc_jump(gen_u, arg, nex_h);
      return;
    }

    case c3__jmp: {
      c3_y fam_y;

      u3x_cell(arg, &a, &b);
      fam_y = ( (u3_none != gen_u->bell) && (c3y == u3r_sing(a, gen_u->bell)) )
            ? _nc_jms
            : _nc_jmp;
      op_u = _nc_op(gen_u, fam_y);
      op_u->imm_h = _nc_dir(gen_u, a, _nc_ring(u3_nul), u3_none, b, c3n);
      return;
    }

    case c3__jmf: {
      u3x_trel(arg, &a, &b, &c);
      _nc_call(gen_u, a, c, b, 1);
      return;
    }

    case c3__jsp:
    case c3__jsf: {
      c = u3_nul;
      if ( c3__jsf == tag ) {
        u3x_trel(arg, &a, &b, &c);
      }
      else {
        u3x_cell(arg, &a, &b);
      }
      op_u = _nc_op(gen_u, _nc_jsp);
      op_u->imm_h = _nc_dir_sub(gen_u, a, _nc_ring(c), u3_none, b);
      return;
    }

    case c3__don: {
      op_u = _nc_op(gen_u, _nc_don);
      op_u->src_h[0] = _nc_reg(gen_u, arg);
      return;
    }

    case c3__bom: {
      _nc_op(gen_u, _nc_bom);
      return;
    }
  }

  //  a branch falls through to its yes block: jump there if it's not next
  //
  {
    c3_h yes_h = _nc_bare(gen_u, c);

    if ( yes_h != nex_h ) {
      op_u = _nc_op(gen_u, _nc_hop);
      op_u->tar_h = yes_h;
    }
  }
}

/* _nc_fold(): compare against immediates.  A register defined by an
**             immediate is defined once, so wherever it is compared,
**             the immediate can stand in for it.
*/
static void
_nc_fold(nc_gen* gen_u)
{
  c3_h* def_h = u3a_malloc(gen_u->reg_h * sizeof(c3_h));
  c3_h  i_h;

  for ( i_h = 0; i_h < gen_u->reg_h; i_h++ ) {
    def_h[i_h] = _nc_none;
  }

  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);

    if ( (_nc_imm == op_u->fam_y) || (_nc_iml == op_u->fam_y) ) {
      def_h[op_u->dst_h] = i_h;
    }
  }

  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);
    nc_op* def_u;

    if ( _nc_eqq != op_u->fam_y ) {
      continue;
    }

    if ( _nc_none == def_h[op_u->src_h[1]] ) {
      c3_h tmp_h;

      if ( _nc_none == def_h[op_u->src_h[0]] ) {
        continue;
      }
      tmp_h          = op_u->src_h[0];
      op_u->src_h[0] = op_u->src_h[1];
      op_u->src_h[1] = tmp_h;
    }

    def_u = &(gen_u->ops_u[def_h[op_u->src_h[1]]]);

    if ( _nc_imm == def_u->fam_y ) {
      op_u->fam_y = _nc_eqi;
      op_u->imm_h = def_u->imm_h;
    }
    else {
      op_u->fam_y = _nc_eql;
      op_u->imm_h = def_u->imm_h;
    }
    op_u->src_h[1] = _nc_none;
  }

  u3a_free(def_h);
}

/* _nc_translate(): translate the blocks in layout order.
*/
static void
_nc_translate(nc_gen* gen_u)
{
  c3_h i_h;

  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);
    c3_h    nex_h = ( (i_h + 1) < gen_u->lan_h )
                  ? gen_u->lay_h[i_h + 1]
                  : _nc_none;
    u3_noun par, bod, fin, op;

    u3x_trel(blk_u->blob, &par, &bod, &fin);
    blk_u->fir_h = gen_u->opn_h;

    while ( u3_nul != bod ) {
      u3x_cell(bod, &op, &bod);
      _nc_pole(gen_u, op);
    }

    _nc_fin(gen_u, fin, nex_h);
    blk_u->len_h = gen_u->opn_h - blk_u->fir_h;
  }
}

/* _nc_srcs(): run body over the source registers of an op, reg_h
**             pointing at each in turn: its own, then those of its call
**             site, if any.
*/
#define _nc_srcs(gen_u, op_u, reg_h, body)                               \
  do {                                                                   \
    const c3_y _nfam = (op_u)->fam_y;                                    \
    c3_h _i;                                                             \
    for ( _i = 0; _i < _nc_fam[_nfam].src_y; _i++ ) {                    \
      c3_h* reg_h = &((op_u)->src_h[_i]);                                \
      body;                                                              \
    }                                                                    \
    if ( _nc_fam[_nfam].sot_y ) {                                        \
      nc_dir* _dir = &((gen_u)->dir_u[(op_u)->imm_h]);                   \
      for ( _i = 0; _i < _dir->len_h; _i++ ) {                           \
        c3_h* reg_h = &((gen_u)->pol_h[_dir->sot_h + _i]);               \
        body;                                                            \
      }                                                                  \
    }                                                                    \
  } while ( 0 )

/* _nc_key_cmp(): sort registers by the start of their live interval.
*/
static int
_nc_key_cmp(const void* a_v, const void* b_v)
{
  c3_d a_d = *(const c3_d*)a_v, b_d = *(const c3_d*)b_v;
  return ( a_d < b_d ) ? -1 : ( a_d > b_d ) ? 1 : 0;
}

/* _nc_bit_get(), _nc_bit_set(), _nc_bit_clr(): bitsets of registers.
*/
#define _nc_bit_get(set, r)  ((set)[(r) >> 6] & (1ULL << ((r) & 63)))
#define _nc_bit_set(set, r)  ((set)[(r) >> 6] |= (1ULL << ((r) & 63)))
#define _nc_bit_clr(set, r)  ((set)[(r) >> 6] &= ~(1ULL << ((r) & 63)))

/* _nc_live_in(): the registers of a chunk [lo, hi) live into each
**                laid-out block, as bitsets of chu_h words in inn_d; liv_d
**                is scratch.  If lvo_d, also the registers live before
**                each op, a bitset per op index.  If mar_t, mark the last
**                use of each register on its paths as consumed (in the
**                op's consume bits, or in the flags of its call site; a
**                register read twice by an op is consumed by one read
**                only), and the ops whose product is never used.
**
**   The CFG is a DAG and the IR is in SSA form (a parameter of a merging
**   block is defined by a move in each predecessor, or left undefined by
**   a kil where the jump passes ~), so one backward pass over the blocks
**   in reverse layout order suffices, with a backward walk of the ops of
**   each block.
*/
static void
_nc_live_in(nc_gen* gen_u, c3_h lo_h, c3_h hi_h, c3_h chu_h,
            c3_d* inn_d, c3_d* liv_d, c3_d* lvo_d, c3_t mar_t)
{
  c3_h i_h, j_h, k_h, r_h, kin_h, kid_h[2];

  memset(inn_d, 0, (c3_z)gen_u->blk_h * chu_h * sizeof(c3_d));

  for ( i_h = gen_u->lan_h; i_h-- > 0; ) {
    nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);

    //  live out of the block: live into its successors
    //
    memset(liv_d, 0, chu_h * sizeof(c3_d));
    kin_h = _nc_kids(gen_u, blk_u->blob, kid_h);
    for ( j_h = 0; j_h < kin_h; j_h++ ) {
      c3_d* kin_d = inn_d + ((c3_z)kid_h[j_h] * chu_h);
      for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
        liv_d[w_h] |= kin_d[w_h];
      }
    }

    //  backward through the ops: a product that is not live is never
    //  used; a source that is not live is read for the last time
    //
    for ( j_h = blk_u->len_h; j_h-- > 0; ) {
      nc_op* op_u = &(gen_u->ops_u[blk_u->fir_h + j_h]);

      if ( _nc_nop == op_u->fam_y ) {
        continue;
      }

      r_h = op_u->dst_h;
      if ( (_nc_none != r_h) && (r_h >= lo_h) && (r_h < hi_h) ) {
        if (  mar_t && (_nc_kil != op_u->fam_y)
           && !_nc_bit_get(liv_d, r_h - lo_h) )
        {
          op_u->ded_y = 1;
        }
        _nc_bit_clr(liv_d, r_h - lo_h);
      }

      for ( k_h = 0; k_h < _nc_fam[op_u->fam_y].src_y; k_h++ ) {
        r_h = op_u->src_h[k_h];
        if ( (r_h >= lo_h) && (r_h < hi_h) ) {
          if ( !_nc_bit_get(liv_d, r_h - lo_h) ) {
            if ( mar_t && !_nc_fam[op_u->fam_y].swe_y ) {
              op_u->kon_y |= 1 << k_h;
            }
            _nc_bit_set(liv_d, r_h - lo_h);
          }
        }
      }

      if ( _nc_fam[op_u->fam_y].sot_y ) {
        nc_dir* dir_u = &(gen_u->dir_u[op_u->imm_h]);

        for ( k_h = 0; k_h < dir_u->len_h; k_h++ ) {
          r_h = gen_u->pol_h[dir_u->sot_h + k_h];
          if ( (r_h >= lo_h) && (r_h < hi_h) ) {
            if ( !_nc_bit_get(liv_d, r_h - lo_h) ) {
              if ( mar_t ) {
                gen_u->pok_y[dir_u->sot_h + k_h] = 1;
              }
              _nc_bit_set(liv_d, r_h - lo_h);
            }
          }
        }
      }

      if ( lvo_d ) {
        memcpy(lvo_d + ((c3_z)(blk_u->fir_h + j_h) * chu_h), liv_d,
               chu_h * sizeof(c3_d));
      }
    }

    memcpy(inn_d + ((c3_z)gen_u->lay_h[i_h] * chu_h), liv_d,
           chu_h * sizeof(c3_d));
  }
}

/* _nc_lives(): respect the lifetimes of the registers: consume a register
**              at its last use, and drop it where it dies otherwise.
**
**   The last uses are found by _nc_live_in().  A register live out of a
**   predecessor of a block but not live into the block died on the edge,
**   and is dropped at the start of the block.  That is sound at a block
**   with several predecessors too: by construction a slot is zero
**   wherever the register in it is dead, and dropping zero does nothing.
**   An argument never used is dropped at the entry; the product of an op
**   never used, right after the op, unless the op has no effect and is
**   removed instead.  Nothing is dropped at a block that only crashes.
**
**   Then a slot holds a reference exactly while the register in it is
**   live or being dropped, so a destination slot is zero when written,
**   and an activation returns with every slot zero but the returned one.
**
**   The slot assignment (_nc_slots()) must keep the dropped register's
**   slot from any register that could be in it at the drop: one held at
**   the end of a predecessor of the block and live into the block.  Such
**   a register's interval runs from before the end of that predecessor to
**   past the start of the block, so it suffices that the drop count as a
**   use of its register at a position just after the last predecessor in
**   the layout (pos_h).  That is where the register would have been
**   released had the drop been on the edge; counting the drop where it
**   is, in the block, could reserve the slot across all the code laid out
**   in between.
**
**   A block whose predecessors reach back over the whole function, with
**   many drops, still reserves many slots, so such a block sweeps
**   instead: it drops whatever its slots hold at its start, but for the
**   registers live into it, without naming the dead.  A block that ends
**   the activation (DON or a tail call) after ops that write no slot
**   leaves the sweep to its terminator; another block starts with SWK,
**   keeping the registers listed in a pseudo call site.  A sweep costs a
**   pass over all the slots where a drop costs a dispatch, and a few
**   drops cost a few slots at worst, so a block sweeps only when the
**   dead are at least _nc_swk_min and a quarter of what it holds.
**
**   Live sets are bitsets over a chunk of the registers, the chunk sized
**   to bound the memory for all the blocks; the ops are walked once per
**   chunk, twice if there are sweeps to collect keep sets for.
*/
#define _nc_swk_min  3    //  drops at a block below which it never sweeps

static void
_nc_lives(nc_gen* gen_u, c3_h arg_h)
{
  c3_h  reg_h = c3_max(gen_u->reg_h, arg_h);
  c3_h  blk_h = gen_u->blk_h;
  c3_h  wor_h = (reg_h + 63) >> 6;               //  words of a full bitset
  c3_h  chu_h;                                   //  words of a chunk
  c3_h* use_h = u3a_calloc(reg_h, sizeof(c3_h));
  c3_h* pro_h = u3a_calloc(blk_h + 1, sizeof(c3_h));  //  predecessor offsets
  c3_h* pre_h;                                        //  predecessors
  c3_d* inn_d;                                   //  live into each block
  c3_d* liv_d;                                   //  scratch
  c3_d* dro_d = NULL;                            //  drops: layout << 32 | reg
  c3_d* kep_d = NULL;                            //  keeps: layout << 32 | reg
  c3_h  dro_h = 0, drc_h = 0, kep_h = 0, kec_h = 0;
  c3_y* kin_y = u3a_calloc(gen_u->lan_h, sizeof(c3_y));  //  kind, by layout
  c3_h* cnt_h = u3a_calloc(gen_u->lan_h, sizeof(c3_h));  //  drops, by layout
  c3_h* liv_h = u3a_calloc(gen_u->lan_h, sizeof(c3_h));  //  live in, by layout
  c3_t  swk_t = 0;
  c3_h  i_h, j_h, k_h, r_h, b_h, kin_h, kid_h[2];

  enum { _nc_kin_dro, _nc_kin_ter, _nc_kin_swk };

  //  products never used: an op without effect is removed, which may
  //  leave its sources unused in turn
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    _nc_srcs(gen_u, &(gen_u->ops_u[i_h]), reg_h, { use_h[*reg_h]++; });
  }

  {
    c3_t chg_t;

    do {
      chg_t = 0;

      for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
        nc_op* op_u = &(gen_u->ops_u[i_h]);

        switch ( op_u->fam_y ) {
          default: break;

          case _nc_imm: case _nc_iml: case _nc_mov: case _nc_kil:
          case _nc_con: case _nc_hed: case _nc_tal: {
            if ( !use_h[op_u->dst_h] ) {
              _nc_srcs(gen_u, op_u, reg_h, { use_h[*reg_h]--; });
              op_u->fam_y = _nc_nop;
              chg_t = 1;
            }
          } break;
        }
      }
    } while ( chg_t );
  }

  //  predecessors of the laid-out blocks
  //
  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    kin_h = _nc_kids(gen_u, gen_u->blk_u[gen_u->lay_h[i_h]].blob, kid_h);
    for ( j_h = 0; j_h < kin_h; j_h++ ) {
      pro_h[kid_h[j_h] + 1]++;
    }
  }

  for ( b_h = 0; b_h < blk_h; b_h++ ) {
    pro_h[b_h + 1] += pro_h[b_h];
  }

  pre_h = u3a_malloc(c3_max(pro_h[blk_h], 1) * sizeof(c3_h));

  {
    c3_h* fil_h = u3a_calloc(blk_h, sizeof(c3_h));

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      b_h   = gen_u->lay_h[i_h];
      kin_h = _nc_kids(gen_u, gen_u->blk_u[b_h].blob, kid_h);
      for ( j_h = 0; j_h < kin_h; j_h++ ) {
        c3_h kid = kid_h[j_h];
        pre_h[pro_h[kid] + fil_h[kid]++] = b_h;
      }
    }

    u3a_free(fil_h);
  }

  //  how each block drops: a block that only ends the activation, by its
  //  terminator; otherwise by drops, or a sweep if there are many
  //
  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);
    nc_op*  las_u = NULL;

    for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
      nc_op* op_u = &(gen_u->ops_u[blk_u->fir_h + j_h]);

      if ( _nc_nop != op_u->fam_y ) {
        if ( _nc_none != op_u->dst_h ) {
          break;
        }
        las_u = op_u;
      }
    }

    kin_y[i_h] = ( (j_h == blk_u->len_h) && las_u && _nc_fam[las_u->fam_y].swe_y )
               ? _nc_kin_ter
               : _nc_kin_dro;
  }

  //  liveness, a chunk of the registers at a time: deaths on the edges
  //  into each block, and of the unused arguments at the entry, are live
  //  out of a predecessor and not live into the block
  //
  chu_h = c3_max(1, c3_min(wor_h, (1 << 17) / c3_max(blk_h, 1)));
  inn_d = u3a_malloc((c3_z)blk_h * chu_h * sizeof(c3_d));
  liv_d = u3a_malloc(chu_h * sizeof(c3_d));

#define _nc_in(b)  (inn_d + ((c3_z)(b) * chu_h))

  for ( c3_d lo_d = 0; lo_d < reg_h; lo_d += (c3_d)chu_h << 6 ) {
    c3_h lo_h = lo_d;
    c3_h hi_h = c3_min(reg_h, lo_d + ((c3_d)chu_h << 6));

    _nc_live_in(gen_u, lo_h, hi_h, chu_h, inn_d, liv_d, NULL, 1);

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[b_h = gen_u->lay_h[i_h]]);
      c3_d*   bin_d = _nc_in(b_h);

      if (  blk_u->len_h
         && (_nc_bom == gen_u->ops_u[blk_u->fir_h + blk_u->len_h - 1].fam_y) )
      {
        continue;
      }

      memset(liv_d, 0, chu_h * sizeof(c3_d));

      if ( !i_h ) {
        for ( r_h = lo_h; (r_h < hi_h) && (r_h < arg_h); r_h++ ) {
          _nc_bit_set(liv_d, r_h - lo_h);
        }
      }

      for ( j_h = pro_h[b_h]; j_h < pro_h[b_h + 1]; j_h++ ) {
        kin_h = _nc_kids(gen_u, gen_u->blk_u[pre_h[j_h]].blob, kid_h);
        for ( k_h = 0; k_h < kin_h; k_h++ ) {
          c3_d* kin_d = _nc_in(kid_h[k_h]);
          for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
            liv_d[w_h] |= kin_d[w_h];
          }
        }
      }

      for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
        c3_d ded_d = liv_d[w_h] & ~bin_d[w_h];

        liv_h[i_h] += __builtin_popcountll(bin_d[w_h]);

        while ( ded_d ) {
          c3_h bit_h = __builtin_ctzll(ded_d);
          ded_d &= ded_d - 1;
          _nc_grow(dro_d, dro_h, drc_h, c3_d);
          dro_d[dro_h++] = ((c3_d)i_h << 32) | (lo_h + (w_h << 6) + bit_h);
          cnt_h[i_h]++;
        }
      }
    }
  }

  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    if (  (_nc_kin_dro == kin_y[i_h])
       && (cnt_h[i_h] >= _nc_swk_min)
       && ((4 * cnt_h[i_h]) >= (cnt_h[i_h] + liv_h[i_h])) )
    {
      kin_y[i_h] = _nc_kin_swk;
      swk_t = 1;
    }
  }

  //  the keep sets of the sweeps: the registers live into their blocks
  //
  if ( swk_t ) {
    for ( c3_d lo_d = 0; lo_d < reg_h; lo_d += (c3_d)chu_h << 6 ) {
      c3_h lo_h = lo_d;
      c3_h hi_h = c3_min(reg_h, lo_d + ((c3_d)chu_h << 6));

      _nc_live_in(gen_u, lo_h, hi_h, chu_h, inn_d, liv_d, NULL, 0);

      for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
        c3_d* bin_d = _nc_in(gen_u->lay_h[i_h]);

        if ( _nc_kin_swk != kin_y[i_h] ) {
          continue;
        }

        for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
          c3_d set_d = bin_d[w_h];

          while ( set_d ) {
            c3_h bit_h = __builtin_ctzll(set_d);
            set_d &= set_d - 1;
            _nc_grow(kep_d, kep_h, kec_h, c3_d);
            kep_d[kep_h++] = ((c3_d)i_h << 32) | (lo_h + (w_h << 6) + bit_h);
          }
        }
      }
    }
  }

#undef _nc_in

  //  rebuild the ops in layout order with the drops: those of the edges
  //  into a block at its start, as drops, a sweep, or left to the
  //  terminator; that of a product never used right after its op; the
  //  removed ops go
  //
  qsort(dro_d, dro_h, sizeof(c3_d), _nc_key_cmp);
  qsort(kep_d, kep_h, sizeof(c3_d), _nc_key_cmp);

  {
    c3_h   cap_h = (2 * gen_u->opn_h) + dro_h;
    nc_op* new_u = u3a_malloc(c3_max(cap_h, 1) * sizeof(nc_op));
    c3_h*  reg_h = u3a_malloc(c3_max(kep_h, 1) * sizeof(c3_h));
    c3_h*  end_h = u3a_calloc(gen_u->lan_h, sizeof(c3_h));  //  ops, by layout
    c3_h   new_h = 0, dri_h = 0, kei_h = 0;

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[b_h = gen_u->lay_h[i_h]]);
      c3_h    fir_h = new_h;

      switch ( kin_y[i_h] ) {
        case _nc_kin_dro: {
          c3_h pos_h = 0;

          //  the predecessors precede the block in the layout
          //
          for ( j_h = pro_h[b_h]; j_h < pro_h[b_h + 1]; j_h++ ) {
            pos_h = c3_max(pos_h, end_h[gen_u->blk_u[pre_h[j_h]].lay_h]);
          }

          while ( (dri_h < dro_h) && ((c3_h)(dro_d[dri_h] >> 32) == i_h) ) {
            nc_op* op_u = _nc_op_init(&(new_u[new_h++]), _nc_dro);
            op_u->src_h[0] = (c3_h)dro_d[dri_h++];
            op_u->pos_h    = pos_h;
          }
        } break;

        case _nc_kin_swk: {
          c3_h   len_h = 0;
          nc_op* op_u;

          while ( (kei_h < kep_h) && ((c3_h)(kep_d[kei_h] >> 32) == i_h) ) {
            reg_h[len_h++] = (c3_h)kep_d[kei_h++];
          }

          op_u = _nc_op_init(&(new_u[new_h++]), _nc_swk);
          op_u->imm_h = _nc_dir_kept(gen_u, reg_h, len_h);
        } [[fallthrough]];

        case _nc_kin_ter: {
          while ( (dri_h < dro_h) && ((c3_h)(dro_d[dri_h] >> 32) == i_h) ) {
            dri_h++;
          }
        } break;
      }

      for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
        nc_op* op_u = &(gen_u->ops_u[blk_u->fir_h + j_h]);

        if ( _nc_nop == op_u->fam_y ) {
          continue;
        }

        new_u[new_h++] = *op_u;

        if ( (_nc_kin_ter == kin_y[i_h]) && cnt_h[i_h]
           && _nc_fam[op_u->fam_y].swe_y )
        {
          new_u[new_h - 1].kon_y = 1;
        }

        if ( op_u->ded_y ) {
          nc_op* dro_u = _nc_op_init(&(new_u[new_h++]), _nc_dro);
          dro_u->src_h[0] = op_u->dst_h;
        }
      }

      blk_u->fir_h = fir_h;
      blk_u->len_h = new_h - fir_h;
      end_h[i_h]   = new_h;    //  the position just after the block
    }

    u3_assert( (dri_h == dro_h) && (kei_h == kep_h) && (new_h <= cap_h) );

    u3a_free(gen_u->ops_u);
    u3a_free(reg_h);
    u3a_free(end_h);
    gen_u->ops_u = new_u;
    gen_u->opn_h = new_h;
    gen_u->opc_h = cap_h;
  }

  u3a_free(use_h);
  u3a_free(pro_h);
  u3a_free(pre_h);
  u3a_free(inn_d);
  u3a_free(liv_d);
  u3a_free(dro_d);
  u3a_free(kep_d);
  u3a_free(kin_y);
  u3a_free(cnt_h);
  u3a_free(liv_h);
}

/* _nc_consumes(): whether the op consumes register r: as one of its own
**                 sources, or as an argument of its call site.
*/
static c3_t
_nc_consumes(nc_gen* gen_u, nc_op* op_u, c3_h r_h)
{
  c3_h k_h;

  for ( k_h = 0; k_h < _nc_fam[op_u->fam_y].src_y; k_h++ ) {
    if ( (op_u->src_h[k_h] == r_h) && ((op_u->kon_y >> k_h) & 1) ) {
      return 1;
    }
  }

  if ( _nc_fam[op_u->fam_y].sot_y ) {
    nc_dir* dir_u = &(gen_u->dir_u[op_u->imm_h]);

    for ( k_h = 0; k_h < dir_u->len_h; k_h++ ) {
      if (  (gen_u->pol_h[dir_u->sot_h + k_h] == r_h)
         && gen_u->pok_y[dir_u->sot_h + k_h] )
      {
        return 1;
      }
    }
  }

  return 0;
}

/* _nc_slots(): assign the registers to slots, rewriting the ops.
**
**   The CFG is a DAG, so the live range of a register lies within its
**   interval of ops in layout order, from its definition to its last use.
**   A parameter of a merging block is defined by a move at each of its
**   predecessors, so its interval starts at the first of them; then no
**   move into a parameter aliases a register read by a later one.
**   Intervals are assigned slots by linear scan; two intervals share a
**   slot only if one ends strictly before the other starts, so an op
**   never writes the slot it reads, except that the product of an op may
**   take the slot of a source the op consumes: every op releases its
**   consumed sources before it writes.  A drop (DRO) is a use at the
**   position _nc_lives() gives it, so the interval covers every point
**   where the slot may still hold the register.
**
**   The arguments of a self call (JMS) want the parameter slots they go
**   to: a register feeding parameter i takes slot i when it is free, or
**   when parameter i is dead before every op of its interval (or consumed
**   by the op defining it) and any other register in the slot is consumed
**   by that op too; the parameter's liveness is per op, so its uses in
**   other branches don't stand in the way, and its drops do (a drop reads
**   the slot where it is).  A parameter slot freed early is handed out
**   last otherwise.  A self call whose arguments are all in place has
**   nothing to move (_nc_emit()).
*/
static void
_nc_slots(nc_gen* gen_u, c3_h arg_h)
{
  c3_h  reg_h = c3_max(gen_u->reg_h, arg_h);
  c3_h* sta_h = u3a_malloc(reg_h * sizeof(c3_h));
  c3_h* end_h = u3a_calloc(reg_h, sizeof(c3_h));
  c3_h* def_h = u3a_malloc(reg_h * sizeof(c3_h));  //  op at the start
  c3_h* wan_h = u3a_malloc(reg_h * sizeof(c3_h));  //  wanted slot
  c3_y* won_y = u3a_calloc(reg_h, sizeof(c3_y));   //  0 none, 1 one, 2 many
  c3_h* sot_h = u3a_malloc(reg_h * sizeof(c3_h));
  c3_d* key_d = u3a_malloc(reg_h * sizeof(c3_d));
  c3_h* act_h = u3a_malloc(reg_h * sizeof(c3_h));
  c3_h* fre_h = u3a_malloc(reg_h * sizeof(c3_h));
  c3_h  key_n_h = 0, act_n_h = 0, fre_n_h = 0, tot_h = 0;
  c3_h  pch_h = (arg_h + 63) >> 6;                 //  words of parameters
  c3_d* plv_d = NULL;                              //  parameters live before each op
  c3_t  wan_t = 0;
  c3_h  i_h, r_h;

  for ( r_h = 0; r_h < reg_h; r_h++ ) {
    sta_h[r_h] = ( r_h < arg_h ) ? 0 : _nc_none;
    sot_h[r_h] = _nc_none;
    def_h[r_h] = _nc_none;
    wan_h[r_h] = _nc_none;
  }

  //  the wants of the arguments of self calls; a parameter is in its
  //  own slot, and a register wanted in two places wants neither
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);

    if ( _nc_jms == op_u->fam_y ) {
      nc_dir* dir_u = &(gen_u->dir_u[op_u->imm_h]);
      c3_h    k_h;

      for ( k_h = 0; k_h < dir_u->len_h; k_h++ ) {
        r_h = gen_u->pol_h[dir_u->sot_h + k_h];

        if ( r_h < arg_h ) {
          continue;
        }
        else if ( !won_y[r_h] ) {
          won_y[r_h] = 1;
          wan_h[r_h] = k_h;
          wan_t = 1;
        }
        else if ( wan_h[r_h] != k_h ) {
          won_y[r_h] = 2;
          wan_h[r_h] = _nc_none;
        }
      }
    }
  }

  if ( wan_t ) {
    c3_d* pin_d = u3a_malloc((c3_z)gen_u->blk_h * pch_h * sizeof(c3_d));
    c3_d* pli_d = u3a_malloc(pch_h * sizeof(c3_d));

    plv_d = u3a_calloc((c3_z)gen_u->opn_h * pch_h, sizeof(c3_d));
    _nc_live_in(gen_u, 0, arg_h, pch_h, pin_d, pli_d, plv_d, 0);
    u3a_free(pin_d);
    u3a_free(pli_d);
  }

  //  intervals; a drop reads its register at the position _nc_lives()
  //  gave it, just after the predecessors of its block
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u  = &(gen_u->ops_u[i_h]);
    c3_h   pos_h = i_h + 1;
    c3_h   use_h = op_u->pos_h ? op_u->pos_h : pos_h;

    if ( _nc_nop == op_u->fam_y ) {
      continue;
    }

    _nc_srcs(gen_u, op_u, reg_h, {
      if ( _nc_none == sta_h[*reg_h] ) {
        sta_h[*reg_h] = 0;
      }
      end_h[*reg_h] = c3_max(end_h[*reg_h], use_h);
    });

    if ( _nc_none != op_u->dst_h ) {
      if ( _nc_none == sta_h[op_u->dst_h] ) {
        sta_h[op_u->dst_h] = pos_h;
        def_h[op_u->dst_h] = i_h;
      }
      end_h[op_u->dst_h] = c3_max(end_h[op_u->dst_h], pos_h);
    }
  }

  //  linear scan
  //
  for ( r_h = 0; r_h < reg_h; r_h++ ) {
    if ( _nc_none != sta_h[r_h] ) {
      key_d[key_n_h++] = ((c3_d)sta_h[r_h] << 32) | r_h;
    }
  }

  //  this feels highly illegal but should be fine because we sort by
  //  simple arithmetic comparison, i.e. by the start of the lifetime
  //  with @uvre value as a tiebreaker, and the tie can happen only
  //  with sta_h == 0. So the sorted array should always be the same
  //  no matter the implementation of qsort (as long as it is not
  //  completely broken)
  qsort(key_d, key_n_h, sizeof(c3_d), _nc_key_cmp);

  for ( i_h = 0; i_h < key_n_h; i_h++ ) {
    c3_h j_h, wan = _nc_none;

    r_h = (c3_h)key_d[i_h];

    //  iterate over all active slots, freeing the stale ones by
    //  putting them in the free list and rewriting the slot in act_h
    //  with the item from the end. The replacing item is not yet examined
    //  so we advance j_h in the else branch only.  A slot still held by
    //  another active register (a parameter's, taken by an argument of a
    //  self call) stays out of the free list; a parameter slot goes to
    //  the bottom of it, to be handed out last.
    for ( j_h = 0; j_h < act_n_h; ) {
      if ( end_h[act_h[j_h]] < sta_h[r_h] ) {
        c3_h sot = sot_h[act_h[j_h]];
        c3_h k_h;

        act_h[j_h] = act_h[--act_n_h];

        for ( k_h = 0; k_h < act_n_h; k_h++ ) {
          if ( sot_h[act_h[k_h]] == sot ) {
            break;
          }
        }

        if ( k_h < act_n_h ) {
          continue;
        }
        else if ( sot < arg_h ) {
          memmove(fre_h + 1, fre_h, fre_n_h * sizeof(c3_h));
          fre_h[0] = sot;
          fre_n_h++;
        }
        else {
          fre_h[fre_n_h++] = sot;
        }
      }
      else {
        j_h++;
      }
    }

    //  the wanted slot, if it is free; or if its parameter is dead before
    //  every op of this register's interval, except consumed by the op
    //  defining it, and so is any other register holding the slot
    //
    if ( _nc_none != wan_h[r_h] ) {
      for ( j_h = 0; j_h < fre_n_h; j_h++ ) {
        if ( fre_h[j_h] == wan_h[r_h] ) {
          wan = fre_h[j_h];
          fre_h[j_h] = fre_h[--fre_n_h];
          break;
        }
      }

      if ( (_nc_none == wan) && (_nc_none != def_h[r_h]) && plv_d ) {
        nc_op* def_u = &(gen_u->ops_u[def_h[r_h]]);
        c3_h   par_h = wan_h[r_h];
        c3_t   fit_t = 1;
        c3_h   k_h;

        for ( k_h = sta_h[r_h]; fit_t && (k_h <= end_h[r_h]); k_h++ ) {
          if ( _nc_bit_get(plv_d + ((c3_z)(k_h - 1) * pch_h), par_h) ) {
            fit_t = (k_h == sta_h[r_h]) && _nc_consumes(gen_u, def_u, par_h);
          }
        }

        for ( j_h = 0; fit_t && (j_h < act_n_h); j_h++ ) {
          c3_h act = act_h[j_h];

          if ( (sot_h[act] == par_h) && (act != par_h) ) {
            fit_t = (end_h[act] == sta_h[r_h])
                 && _nc_consumes(gen_u, def_u, act);
          }
        }

        if ( fit_t ) {
          for ( j_h = 0; j_h < act_n_h; ) {
            c3_h act = act_h[j_h];

            if ( (sot_h[act] == par_h) && (act != par_h) ) {
              act_h[j_h] = act_h[--act_n_h];
            }
            else {
              j_h++;
            }
          }
          wan = par_h;
        }
      }
    }

    sot_h[r_h] = ( _nc_none != wan ) ? wan
               : fre_n_h ? fre_h[--fre_n_h]
               : tot_h++;
    act_h[act_n_h++] = r_h;
  }

  //  Calling convention assertion
  for ( r_h = 0; r_h < arg_h; r_h++ ) {
    u3_assert( r_h == sot_h[r_h] );
  }

  //  rewrite
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);

    if ( _nc_nop == op_u->fam_y ) {
      continue;
    }

    _nc_srcs(gen_u, op_u, reg_h, { *reg_h = sot_h[*reg_h]; });

    if ( _nc_none != op_u->dst_h ) {
      op_u->dst_h = sot_h[op_u->dst_h];
    }
  }

  gen_u->tot_h = c3_max(tot_h, arg_h);

  u3a_free(sta_h);
  u3a_free(end_h);
  u3a_free(def_h);
  u3a_free(wan_h);
  u3a_free(won_y);
  u3a_free(plv_d);
  u3a_free(sot_h);
  u3a_free(key_d);
  u3a_free(act_h);
  u3a_free(fre_h);
}

/* _nc_put(): write an immediate of the given width, or measure it.
*/
static c3_h
_nc_put(c3_y* buf_y, c3_y wid_y, c3_h val_h)
{
  switch ( wid_y ) {
    case 0: {
      if ( buf_y ) {
        buf_y[0] = val_h;
      }
      return 1;
    }

    case 1: {
      if ( buf_y ) {
        buf_y[0] = val_h & 0xff;
        buf_y[1] = val_h >> 8;
      }
      return 2;
    }

    default: {
      c3_h len_h = 0, i_h;

      for ( c3_h tmp_h = val_h; tmp_h; tmp_h >>= 8 ) {
        len_h++;
      }
      len_h = c3_max(len_h, 1);

      if ( buf_y ) {
        buf_y[0] = len_h;
        for ( i_h = 0; i_h < len_h; i_h++ ) {
          buf_y[1 + i_h] = (val_h >> (8 * i_h)) & 0xff;
        }
      }
      return 1 + len_h;
    }
  }
}

/* _nc_encode(): write an op, or measure it.
*/
static c3_h
_nc_encode(nc_gen* gen_u, nc_op* op_u, c3_y* buf_y)
{
  nc_op tmp_u;
  c3_h  val_h[5], len_h = 0, max_h = 0, siz_h = 1, i_h;
  c3_y  wid_y, fam_y = op_u->fam_y;

  switch ( fam_y ) {
    case _nc_nop:
    case _nc_kil: {
      return 0;
    }

    case _nc_bom: {
      if ( buf_y ) {
        buf_y[0] = BOM;
      }
      return 1;
    }

    case _nc_imm: {
      if ( op_u->dst_h < 0x100 ) {
        c3_h n_h = op_u->imm_h;
        c3_y cod_y;

        if ( n_h < 2 ) {
          cod_y = n_h ? IMM_1 : IMM_0;
          siz_h = 2;
        }
        else if ( n_h < 0x100 ) {
          cod_y = IMM_B;
          siz_h = 3;
        }
        else {
          cod_y = IMM_S;
          siz_h = 4;
        }

        if ( buf_y ) {
          buf_y[0] = cod_y;
          if ( 3 == siz_h ) {
            buf_y[1] = n_h;
          }
          else if ( 4 == siz_h ) {
            buf_y[1] = n_h & 0xff;
            buf_y[2] = n_h >> 8;
          }
          buf_y[siz_h - 1] = op_u->dst_h;
        }
        return siz_h;
      }

      //  a wide slot: use the literal
      //
      tmp_u       = *op_u;
      tmp_u.fam_y = fam_y = _nc_iml;
      tmp_u.imm_h = op_u->tar_h;
      op_u        = &tmp_u;
    } break;
  }

  if ( _nc_fam[fam_y].imm_y ) {
    val_h[len_h++] = op_u->imm_h;
  }
  for ( i_h = 0; i_h < _nc_fam[fam_y].src_y; i_h++ ) {
    val_h[len_h++] = op_u->src_h[i_h];
  }
  if ( _nc_fam[fam_y].dst_y ) {
    val_h[len_h++] = op_u->dst_h;
  }
  if ( _nc_fam[fam_y].tar_y ) {
    val_h[len_h++] = gen_u->blk_u[op_u->tar_h].off_h;
  }

  for ( i_h = 0; i_h < len_h; i_h++ ) {
    max_h = c3_max(max_h, val_h[i_h]);
  }

  wid_y = ( max_h < 0x100 ) ? 0 : ( max_h < 0x10000 ) ? 1 : 2;

  if ( buf_y ) {
    c3_y kon_y = _nc_fam[fam_y].kon_y ? op_u->kon_y : 0;

    u3_assert( kon_y < (1 << _nc_fam[fam_y].kon_y) );
    buf_y[0] = _nc_bas_y[fam_y] + (3 * kon_y) + wid_y;
  }

  for ( i_h = 0; i_h < len_h; i_h++ ) {
    siz_h += _nc_put(buf_y ? (buf_y + siz_h) : NULL, wid_y, val_h[i_h]);
  }

  return siz_h;
}

/* _nc_read(): read an immediate of the given width.
*/
static c3_h
_nc_read(const c3_y* pog, c3_h* ip_h, c3_y wid_y)
{
  c3_h val_h = 0;

  switch ( wid_y ) {
    case 0: {
      val_h = pog[(*ip_h)++];
    } break;

    case 1: {
      val_h  = pog[(*ip_h)++];
      val_h |= pog[(*ip_h)++] << 8;
    } break;

    default: {
      c3_y len_y = pog[(*ip_h)++], i_y;
      for ( i_y = 0; i_y < len_y; i_y++ ) {
        val_h |= ((c3_h)pog[(*ip_h)++]) << (8 * i_y);
      }
    } break;
  }

  return val_h;
}

/* _nc_print_hint(): print the tag of a hint literal [tag formula].
*/
static void
_nc_print_hint(u3_noun lit)
{
  c3_c* tag_c = u3r_string(u3h(lit));

  fprintf(stderr, " %%%s", tag_c);
  c3_free(tag_c);
}

/* _nc_print_spot(): print a literal that looks like a spot
**                   [path [line col] [line col]], the clue of a %spot
**                   hint, as path:line:col.
*/
static void
_nc_print_spot(u3_noun lit)
{
  u3_noun pax, pin, pen, lin, col;

  if (  (c3n == u3r_trel(lit, &pax, &pin, &pen))
     || (c3n == u3r_cell(pin, &lin, &col))
     || (c3n == u3a_is_cat(lin)) || (c3n == u3a_is_cat(col))
     || (c3n == u3du(pen)) || (c3n == u3ud(u3h(pen))) )
  {
    return;
  }

  for ( ; c3y == u3du(pax); pax = u3t(pax) ) {
    if ( c3n == u3ud(u3h(pax)) ) {
      return;
    }
  }

  fprintf(stderr, "  ");
  for ( pax = u3h(lit); c3y == u3du(pax); pax = u3t(pax) ) {
    c3_c* nam_c = u3r_string(u3h(pax));
    fprintf(stderr, "/%s", nam_c);
    c3_free(nam_c);
  }
  fprintf(stderr, ":%" PRIc3_w ":%" PRIc3_w, lin, col);
}

/* _nc_print(): print a program.
*/
static void
_nc_print(u3nc_prog* pog_u)
{
  const c3_y* pog = pog_u->byc_u.ops_y;
  c3_h        ip_h = 0, i_h;

  fprintf(stderr, "program %p: %u slots, %u args, %u bytes, %u literals, "
                  "%u call sites\r\n",
          (void*)pog_u, pog_u->tot_h, pog_u->arg_h, pog_u->byc_u.len_h,
          pog_u->lit_u.len_h, pog_u->dir_u.len_h);

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[i_h]);
    c3_h       j_h;

    if ( !dir_u->bell ) {
      fprintf(stderr, "  site %u: keep set of a sweep, slots", i_h);
    }
    else {
      fprintf(stderr, "  site %u: bell %08x %s%s%s cid %u args",
              i_h, u3r_mug(dir_u->bell),
              ( c3y == dir_u->mon_o ) ? "subject" : "direct",
              dir_u->ham_u ? " jet" : "",
              !dir_u->arm_u ? "" : dir_u->arm_u->pun_t ? " punt-jet" : " array-jet",
              dir_u->cid_h);
    }

    for ( j_h = 0; j_h < dir_u->len_h; j_h++ ) {
      fprintf(stderr, " %u%s", pog_u->sot_u.sot_h[dir_u->sot_h + j_h],
              pog_u->sot_u.kon_y[dir_u->sot_h + j_h] ? "!" : "");
    }

    if ( _nc_none != dir_u->des_h ) {
      fprintf(stderr, " -> %u", dir_u->des_h);
    }

    if ( u3_nul != dir_u->ring ) {
      u3_noun pax = u3h(dir_u->ring);
      fprintf(stderr, " ring");
      while ( u3_nul != pax ) {
        c3_c* nam_c = u3r_string(u3h(pax));
        fprintf(stderr, "/%s", nam_c);
        c3_free(nam_c);
        pax = u3t(pax);
      }
      fprintf(stderr, "/%" PRIc3_w, u3t(dir_u->ring));
    }
    fprintf(stderr, "\r\n");
  }

  while ( ip_h < pog_u->byc_u.len_h ) {
    c3_y cod_y = pog[ip_h];
    c3_h off_h = ip_h++;

    if ( (cod_y < IML_B) || (BOM == cod_y) ) {
      fprintf(stderr, "  %4u: %-6s", off_h, _nc_name_c[cod_y]);

      if ( IMM_B == cod_y ) {
        fprintf(stderr, " %u", _nc_read(pog, &ip_h, 0));
      }
      else if ( IMM_S == cod_y ) {
        fprintf(stderr, " %u", _nc_read(pog, &ip_h, 1));
      }
      if ( BOM != cod_y ) {
        fprintf(stderr, " -> %u", _nc_read(pog, &ip_h, 0));
      }
    }
    else {
      c3_y fam_y = 0, wid_y, kon_y;

      while ( ((fam_y + 1) < _nc_bom) && (_nc_bas_y[fam_y + 1] <= cod_y) ) {
        fam_y++;
      }
      wid_y = (cod_y - _nc_bas_y[fam_y]) % 3;
      kon_y = (cod_y - _nc_bas_y[fam_y]) / 3;

      fprintf(stderr, "  %4u: %s_%c%s", off_h, _nc_fam_c[fam_y], "BSV"[wid_y],
              ( _nc_fam[fam_y].swe_y && kon_y ) ? " sweep" : " ");

      if ( _nc_fam[fam_y].imm_y ) {
        c3_h imm_h = _nc_read(pog, &ip_h, wid_y);

        fprintf(stderr, " #%u", imm_h);

        if (  (_nc_hsp == fam_y) || (_nc_hse == fam_y)
           || (_nc_hdp == fam_y) || (_nc_hde == fam_y) )
        {
          _nc_print_hint(pog_u->lit_u.non[imm_h]);
        }
        else if ( _nc_iml == fam_y ) {
          _nc_print_spot(pog_u->lit_u.non[imm_h]);
        }
      }
      for ( i_h = 0; i_h < _nc_fam[fam_y].src_y; i_h++ ) {
        fprintf(stderr, " %u%s", _nc_read(pog, &ip_h, wid_y),
                ( (kon_y >> i_h) & 1 ) ? "!" : "");
      }
      if ( _nc_fam[fam_y].dst_y ) {
        fprintf(stderr, " -> %u", _nc_read(pog, &ip_h, wid_y));
      }
      if ( _nc_fam[fam_y].tar_y ) {
        fprintf(stderr, " @%u", _nc_read(pog, &ip_h, wid_y));
      }
    }
    fprintf(stderr, "\r\n");
  }
}

/* nc_lay: byte offsets of the sections of a program, and its size.
*/
typedef struct {
  c3_w byc_w;   //  bytecode
  c3_w lit_w;   //  literals
  c3_w dir_w;   //  call sites
  c3_w sot_w;   //  argument slots
  c3_w kon_w;   //  their consume flags
  c3_w len_w;   //  total
} nc_lay;

/* _nc_prog_lay(): lay out a program from the lengths of its sections,
**                 each aligned for its element type.
*/
static nc_lay
_nc_prog_lay(c3_h byc_h, c3_h lit_h, c3_h dir_h, c3_h sot_h)
{
  nc_lay lay_u;
  c3_w   len_w = sizeof(u3nc_prog);

  lay_u.byc_w = len_w = c3_align(len_w, alignof(c3_y), C3_ALGHI);
  len_w += byc_h;

  lay_u.lit_w = len_w = c3_align(len_w, alignof(u3_noun), C3_ALGHI);
  len_w += lit_h * sizeof(u3_noun);

  lay_u.dir_w = len_w = c3_align(len_w, alignof(u3nc_dire), C3_ALGHI);
  len_w += dir_h * sizeof(u3nc_dire);

  lay_u.sot_w = len_w = c3_align(len_w, alignof(c3_h), C3_ALGHI);
  len_w += sot_h * sizeof(c3_h);

  lay_u.kon_w = len_w = c3_align(len_w, alignof(c3_y), C3_ALGHI);
  len_w += sot_h * sizeof(c3_y);

  lay_u.len_w = len_w;
  return lay_u;
}

/* _nc_prog_fix(): set the pointers of a program from its lengths.
*/
static void
_nc_prog_fix(u3nc_prog* pog_u)
{
  c3_y*  dat_y = (c3_y*)pog_u;
  nc_lay lay_u = _nc_prog_lay(pog_u->byc_u.len_h, pog_u->lit_u.len_h,
                              pog_u->dir_u.len_h, pog_u->sot_u.len_h);

  pog_u->byc_u.ops_y = dat_y + lay_u.byc_w;
  pog_u->lit_u.non   = (u3_noun*)(dat_y + lay_u.lit_w);
  pog_u->dir_u.dat_u = (u3nc_dire*)(dat_y + lay_u.dir_w);
  pog_u->sot_u.sot_h = (c3_h*)(dat_y + lay_u.sot_w);
  pog_u->sot_u.kon_y = dat_y + lay_u.kon_w;
}

/* _nc_prog_new(): allocate a program.
*/
static u3nc_prog*
_nc_prog_new(c3_h byc_h, c3_h lit_h, c3_h dir_h, c3_h sot_h)
{
  nc_lay     lay_u = _nc_prog_lay(byc_h, lit_h, dir_h, sot_h);
  u3nc_prog* pog_u = u3a_malloc(lay_u.len_w);

  pog_u->byc_u.len_h = byc_h;
  pog_u->lit_u.len_h = lit_h;
  pog_u->dir_u.len_h = dir_h;
  pog_u->sot_u.len_h = sot_h;
  _nc_prog_fix(pog_u);

  return pog_u;
}

/* _nc_lit_cb(): put a literal in its place.
*/
static void
_nc_lit_cb(u3_noun kev, void* ptr_v)
{
  u3_noun* non = ptr_v;
  non[u3t(kev)] = u3k(u3h(kev));
}

/* _nc_dire_jet(): resolve the jet of a restored call site.
*/
static void
_nc_dire_jet(u3nc_dire* dir_u)
{
  nc_jet jet_u = _nc_ring(dir_u->ring);

  dir_u->ham_u = jet_u.ham_u;
  dir_u->arm_u = jet_u.arm_u;
}

/* _nc_emit(): lay the ops out and assemble the program.  RETAINS ned.
*/
static u3nc_prog*
_nc_emit(nc_gen* gen_u, u3_noun ned, c3_h arg_h)
{
  u3nc_prog* pog_u;
  c3_h       byc_h, i_h, j_h;

  //  a self call that doesn't sweep and finds every argument in its
  //  parameter slot has nothing to do but start over: a jump to the entry
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);

    if ( (_nc_jms == op_u->fam_y) && !op_u->kon_y ) {
      nc_dir* dir_u = &(gen_u->dir_u[op_u->imm_h]);

      for ( j_h = 0; j_h < dir_u->len_h; j_h++ ) {
        if (  (gen_u->pol_h[dir_u->sot_h + j_h] != j_h)
           || !gen_u->pok_y[dir_u->sot_h + j_h] )
        {
          break;
        }
      }

      if ( j_h == dir_u->len_h ) {
        op_u->fam_y = _nc_hop;
        op_u->imm_h = 0;
        op_u->tar_h = _nc_blk(gen_u, 0);
      }
    }
  }

  //  Fixed-point loop on the block offsets. Immediate arguments only widen
  //  and their size is capped, so this converges. 
  //
  for ( i_h = 0; i_h < gen_u->blk_h; i_h++ ) {
    gen_u->blk_u[i_h].off_h = 0;
  }

  {
    c3_t chg_t;

    do {
      chg_t = 0;
      byc_h = 0;

      for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
        nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);

        if ( blk_u->off_h != byc_h ) {
          blk_u->off_h = byc_h;
          chg_t = 1;
        }

        for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
          byc_h += _nc_encode(gen_u, &(gen_u->ops_u[blk_u->fir_h + j_h]), NULL);
        }
      }
    } while ( chg_t );
  }

  pog_u = _nc_prog_new(byc_h, gen_u->lit_h, gen_u->din_h, gen_u->pon_h);
  pog_u->tot_h = gen_u->tot_h;
  pog_u->arg_h = arg_h;
  pog_u->ned   = u3k(ned);

  {
    c3_y* buf_y = pog_u->byc_u.ops_y;
    c3_h  pos_h = 0;

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);

      u3_assert( blk_u->off_h == pos_h );

      for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
        pos_h += _nc_encode(gen_u, &(gen_u->ops_u[blk_u->fir_h + j_h]),
                            buf_y + pos_h);
      }
    }

    u3_assert( pos_h == byc_h );
  }

  u3h_walk_with(gen_u->lit_p, _nc_lit_cb, pog_u->lit_u.non);

  //  the destination slot of a call is in its site
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);

    if ( _nc_fam[op_u->fam_y].sot_y ) {
      gen_u->dir_u[op_u->imm_h].des_h = op_u->dst_h;
    }
  }

  for ( i_h = 0; i_h < gen_u->din_h; i_h++ ) {
    nc_dir*    dir_u = &(gen_u->dir_u[i_h]);
    u3nc_dire* dat_u = &(pog_u->dir_u.dat_u[i_h]);

    dat_u->bell  = dir_u->bell;
    dat_u->ring  = dir_u->jet_u.ring;
    dat_u->pog_p = 0;
    dat_u->sot_h = dir_u->sot_h;
    dat_u->len_h = dir_u->len_h;
    dat_u->cid_h = dir_u->cid_h;
    dat_u->des_h = dir_u->des_h;
    dat_u->mon_o = dir_u->mon_o;
    dat_u->ham_u = dir_u->jet_u.ham_u;
    dat_u->arm_u = dir_u->jet_u.arm_u;
  }

  memcpy(pog_u->sot_u.sot_h, gen_u->pol_h, gen_u->pon_h * sizeof(c3_h));
  memcpy(pog_u->sot_u.kon_y, gen_u->pok_y, gen_u->pon_h * sizeof(c3_y));

  return pog_u;
}

/* _nc_build(): compile a straight [need n-args blocks]; bell: the bell of
**              the program if a tail call to it is a self call (a direct
**              program), else u3_none.  RETAINS bell.
*/
static u3nc_prog*
_nc_build(u3_noun straight, u3_weak bell)
{
  nc_gen     gen_u = {0};
  u3nc_prog* pog_u;
  u3_noun    ned, arg, map;

  u3x_trel(straight, &ned, &arg, &map);

  c3_h arg_h = _nc_cat(arg);

  gen_u.idx_p = u3h_new();
  gen_u.lit_p = u3h_new();
  gen_u.bell  = bell;

  _nc_blocks(&gen_u, map);
  _nc_translate(&gen_u);
  _nc_fold(&gen_u);
  _nc_lives(&gen_u, arg_h);
  _nc_slots(&gen_u, arg_h);
  pog_u = _nc_emit(&gen_u, ned, arg_h);

  _nc_stat(com_d);
  if ( _nc_verb_t ) {
    _nc_print(pog_u);
  }

  u3h_free(gen_u.idx_p);
  u3h_free(gen_u.lit_p);
  u3a_free(gen_u.blk_u);
  u3a_free(gen_u.lay_h);
  u3a_free(gen_u.ops_u);
  u3a_free(gen_u.dir_u);
  u3a_free(gen_u.pol_h);
  u3a_free(gen_u.pok_y);
  u3z(straight);

  return pog_u;
}

/* _nc_of(): loom offset of a program, as a direct atom.
*/
static inline c3_w
_nc_of(u3nc_prog* pog_u)
{
  return u3of(u3nc_prog, pog_u) >> u3a_vits;
}

/* _nc_to(): program from its loom offset.
*/
static inline u3nc_prog*
_nc_to(c3_w pog_w)
{
  return u3to(u3nc_prog, pog_w << u3a_vits);
}

/* _nc_dir_key(): key of a program in ska.dir_p: its bell, or, for the
**                unary program of the bell's whole subject, [%mono bell].
**                RETAINS bell; TRANSFERS the key.
*/
static u3_noun
_nc_dir_key(u3_noun bell, c3_o mon_o)
{
  return ( c3y == mon_o ) ? u3nc(c3__mono, u3k(bell)) : u3k(bell);
}

/* _nc_dir_get(): program under a key of ska.dir_p, from any road.  RETAINS.
*/
static u3nc_prog*
_nc_dir_get(u3_noun key)
{
  u3_weak pog;
  for (u3_road* rod_u = u3R; rod_u; rod_u = u3tn(u3_road, rod_u->par_p)) {
    if ( u3_none != (pog = u3h_git(rod_u->ska.dir_p, key)) ) {
      return _nc_to(pog);
    }
  }

  return NULL;
}

/* _nc_dire_get(): program of a call site, if it has been compiled.
*/
static u3nc_prog*
_nc_dire_get(u3nc_dire* dir_u)
{
  u3_noun    key   = _nc_dir_key(dir_u->bell, dir_u->mon_o);
  u3nc_prog* gop_u = _nc_dir_get(key);

  u3z(key);
  return gop_u;
}

/* _nc_dire_fit(): assert that a program takes the arguments of a call site.
*/
static void
_nc_dire_fit(const u3nc_dire* dir_u, const u3nc_prog* gop_u)
{
  u3_assert( gop_u->arg_h == dir_u->len_h );
}

/* _nc_ent_get(): entry program for [sub fol], from any road.  RETAINS.
*/
static u3nc_prog*
_nc_ent_get(u3_noun sub, u3_noun fol)
{
  u3_weak got, lis;
  for (u3_road* rod_u = u3R; rod_u; rod_u = u3tn(u3_road, rod_u->par_p)) {
    if (  (u3_none != (lis = u3h_git(rod_u->ska.ent_p, fol)))
       && (u3_none != (got = u3d_match(sub, lis))) )
    {
      return _nc_to(u3t(got));
    }
  }

  return NULL;
}

/* _nc_link(): set the callee programs of the call sites that have been
**             compiled; the rest are linked when first called.  A call
**             with arguments of a total array jet never calls a program.
*/
static void
_nc_link(u3nc_prog* pog_u)
{
  c3_h i_h;

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[i_h]);
    u3nc_prog* gop_u;

    if (  !dir_u->bell
       || ((c3n == dir_u->mon_o) && dir_u->arm_u && !dir_u->arm_u->pun_t) )
    {
      continue;
    }

    if ( (gop_u = _nc_dire_get(dir_u)) ) {
      _nc_dire_fit(dir_u, gop_u);
      dir_u->pog_p = u3of(u3nc_prog, gop_u);
    }
  }
}

/* _nc_compile(): compile a straight and link it.  TRANSFERS straight;
**                RETAINS bell, the program's own if it is direct (see
**                _nc_build()), else u3_none.
*/
static u3nc_prog*
_nc_compile(u3_noun straight, u3_weak bell)
{
  u3nc_prog* pog_u = _nc_build(straight, bell);
  _nc_link(pog_u);
  return pog_u;
}

/* _nc_here(): is a pointer on the current road?
*/
static inline c3_t
_nc_here(void* ptr_v)
{
  u3_post pos_p = u3of(void, ptr_v);

  return ( c3y == u3a_is_north(u3R) )
       ? ((pos_p >= u3R->rut_p) && (pos_p < u3R->hat_p))
       : ((pos_p >= u3R->hat_p) && (pos_p < u3R->rut_p));
}

/* _nc_gain_slow(), _nc_lose_slow(): out-of-line reference counting for
**   the interpreter loop.  preserve_most makes the callee save every
**   register, so the inline fast paths of GAIN() and LOSE() don't force
**   the interpreter state to be spilled around them.
*/
#if defined(__clang__) && (defined(__x86_64__) || defined(__aarch64__))
#  define _nc_cold  __attribute__((preserve_most, noinline))
#else
#  define _nc_cold  __attribute__((noinline))
#endif

static _nc_cold u3_noun
_nc_gain_slow(u3_noun som)
{
  return u3a_gain(som);
}

static _nc_cold void
_nc_lose_slow(u3_noun som)
{
  u3a_lose(som);
}

/* _nc_callee(): the program of a call site, compiling it on the first
**               call: the program of its bell or, for a call by subject,
**               the unary program of the bell's whole subject.  The site
**               remembers it if the site is on the current road; a senior
**               site can't point at junior memory.  The interpreter checks
**               the linked case inline; this is the slow path, and doesn't
**               clobber the caller's registers.
*/
static _nc_cold u3nc_prog*
_nc_callee(u3nc_dire* dir_u)
{
  u3nc_prog* gop_u;
  u3_noun    key;

  if ( dir_u->pog_p ) {
    return u3to(u3nc_prog, dir_u->pog_p);
  }

  key = _nc_dir_key(dir_u->bell, dir_u->mon_o);

  if ( !(gop_u = _nc_dir_get(key)) ) {
    gop_u = _nc_compile(u3d_dire(dir_u->bell, dir_u->mon_o),
                        ( c3n == dir_u->mon_o ) ? dir_u->bell : u3_none);
    u3h_put(u3R->ska.dir_p, key, _nc_of(gop_u));
  }

  u3z(key);
  _nc_dire_fit(dir_u, gop_u);

  if ( _nc_here(dir_u) ) {
    dir_u->pog_p = u3of(u3nc_prog, gop_u);
  }

  return gop_u;
}

/* _nc_entry(): entry program for [sub fol], compiling it if needed.
**              RETAINS.
*/
static u3nc_prog*
_nc_entry(u3_noun sub, u3_noun fol)
{
  u3nc_prog* pog_u = _nc_ent_get(sub, fol);

  _nc_stat(ent_d);

  if ( !pog_u ) {
    u3_noun bell, old, lis;

    pog_u = _nc_compile(u3d_full(sub, fol, &bell), u3_none);

    old = u3h_get(u3R->ska.ent_p, fol);
    lis = u3nc(u3nc(u3k(u3h(bell)), _nc_of(pog_u)),
               ( u3_none == old ) ? u3_nul : old);
    u3h_put(u3R->ska.ent_p, fol, lis);
    u3z(bell);
  }

  return pog_u;
}

/* _nc_count(): number of arguments in a need-ordered shape.
*/
static c3_h
_nc_count(u3_noun ned)
{
  u3_noun tag = u3h(ned);

  if ( c3y == u3du(tag) ) {
    return _nc_count(tag) + _nc_count(u3t(ned));
  }

  switch ( tag ) {
    default:        return 0;
    case c3__this:  return 1;
    case c3__both:  return 1 + _nc_count(u3h(u3t(ned)))
                             + _nc_count(u3t(u3t(ned)));
  }
}

/* _nc_knit(): the subject of a callee from its arguments, with 0 for
**             the parts it doesn't use.  RETAINS the arguments.
*/
static u3_noun
_nc_knit(u3_noun ned, u3_noun* arg, c3_h* i_h)
{
  u3_noun tag = u3h(ned);

  if ( c3y == u3du(tag) ) {
    u3_noun hed = _nc_knit(tag, arg, i_h);
    return u3nc(hed, _nc_knit(u3t(ned), arg, i_h));
  }

  switch ( tag ) {
    default:        return 0;
    case c3__this:  return u3k(arg[(*i_h)++]);
    case c3__both: {
      u3_noun rot = u3k(arg[(*i_h)++]);
      *i_h += _nc_count(u3h(u3t(ned))) + _nc_count(u3t(u3t(ned)));
      return rot;
    }
  }
}

static c3_m
_nc_memo_seed(c3_h cid_h, c3_o all_o)
{
  c3_m fun_m = ( c3n == all_o ) ? c3__ska
             : ( u3z_memo_ford == cid_h ) ? 136 + c3__ford
             : 144 + c3__nock;
  //  currently calls to persistently memoized functions are pessimized so their
  //  entire subjects are captured. That way only transient memoization is
  //  partial, so it does not require migration or clearing on upgrade
  u3_assert(u3z_memo_toss == cid_h || c3__ska != fun_m);
  return fun_m;
}

/* _nc_save(): save the product of a memoized call, as nock.c does, but use our
**             key partition if the subject is partial (all_o is c3n)
**             RETAINS key, TRANSFERS pro
*/
static u3_noun
_nc_save(c3_h cid_h, u3_noun key, u3_noun pro, c3_o all_o)
{
  if ( (u3z_memo_toss == cid_h)
       ? (&(u3H->rod_u) != u3R)
       : (0 == u3R->ski.gul) )
  {
    pro = u3z_save_m_dedup(cid_h, _nc_memo_seed(cid_h, all_o), key, pro);
  }
  return pro;
}

/* _nc_find(): look for a memoization entry. RETAINS key
*/
static u3_weak
_nc_find(c3_h cid_h, u3_noun key, c3_o all_o)
{
  return u3z_find_m(cid_h, _nc_memo_seed(cid_h, all_o), key);
}

/* _nc_normal(), _nc_senior(): where an indirect noun lives relative to
**   the road, with the direction known at compile time.
*/
#define _nc_normal(rod_u, dog)                                           \
  ( ( _nc_mov_ws < 0 ) ? u3a_north_is_normal(rod_u, dog)                 \
                       : u3a_south_is_normal(rod_u, dog) )
#define _nc_senior(rod_u, dog)                                           \
  ( ( _nc_mov_ws < 0 ) ? u3a_north_is_senior(rod_u, dog)                 \
                       : u3a_south_is_senior(rod_u, dog) )

/* GAIN(), LOSE(): u3k() and u3z() for the interpreter loop.  The common
**   cases (direct atom, senior noun, normal noun whose count stays
**   positive) are handled inline; the rest call out.  rod_u and
**   _nc_mov_ws must be in scope.
*/
#if defined(U3_MEMORY_DEBUG) || defined(U3_CPU_DEBUG) || defined(C3DBG)
#define GAIN(som)  ({                                                    \
    u3_noun _s = (som);                                                  \
    ( c3y == u3a_is_cat(_s) ) ? _s : _nc_gain_slow(_s);                  \
  })
#define LOSE(som)  ({                                                    \
    u3_noun _s = (som);                                                  \
    if ( c3n == u3a_is_cat(_s) ) { _nc_lose_slow(_s); }                  \
    (void)0;                                                             \
  })
#else
#define GAIN(som)  ({                                                    \
    u3_noun _s = (som);                                                  \
    if ( c3n == u3a_is_cat(_s) ) {                                       \
      if ( c3y == _nc_normal(rod_u, _s) ) {                              \
        u3a_noun* _b = u3a_to_ptr(_s);                                   \
        if ( (_b->use_w - 1) < (u3a_direct_max - 1) ) {                  \
          _b->use_w++;                                                   \
        }                                                                \
        else {                                                           \
          _nc_gain_slow(_s);                                             \
        }                                                                \
      }                                                                  \
      else if ( c3n == _nc_senior(rod_u, _s) ) {                         \
        _nc_gain_slow(_s);                                               \
      }                                                                  \
    }                                                                    \
    _s;                                                                  \
  })
#define LOSE(som)  ({                                                    \
    u3_noun _s = (som);                                                  \
    if ( (c3n == u3a_is_cat(_s)) && (c3y == _nc_normal(rod_u, _s)) ) {   \
      u3a_noun* _b = u3a_to_ptr(_s);                                     \
      if ( _b->use_w > 1 ) {                                             \
        _b->use_w--;                                                     \
      }                                                                  \
      else {                                                             \
        _nc_lose_slow(_s);                                               \
      }                                                                  \
    }                                                                    \
    (void)0;                                                             \
  })
#endif

/* nc_frame: the caller of an activation, above the callee's slots.
*/
typedef struct __attribute__((__packed__)) {
  u3nc_prog* pog_u;   //  caller program, or 0 at the entry
  u3_noun*   reg;     //  caller slots
  c3_h       ip_h;    //  caller instruction pointer
  c3_h       des_h;   //  caller slot for the product
  u3_weak    key;     //  memo key to save the product under
  c3_h       cid_h;   //  memo cache
  c3_o       all_o;   //  does memo cache capture the entire subject?
} nc_frame;

#define _nc_frame_w  c3_wiseof(nc_frame)

/* _nc_push(): push words on the stack.  mov_ws: -1 north, 1 south.
*/
static inline __attribute__((always_inline)) void*
_nc_push(u3a_road* rod_u, c3_ws mov_ws, c3_w len_w)
{
  u3_post bas_p;

  if ( mov_ws < 0 ) {
    rod_u->cap_p -= len_w;
    bas_p = rod_u->cap_p;
  }
  else {
    bas_p = rod_u->cap_p;
    rod_u->cap_p += len_w;
  }

#ifndef U3_GUARD_PAGE
  if ( (mov_ws < 0) ? !(rod_u->cap_p > rod_u->hat_p)
                    : !(rod_u->cap_p < rod_u->hat_p) )
  {
    u3m_bail(c3__meme);
  }
#endif

  return u3to(void, bas_p);
}

/* _nc_top(): the top words of the stack.
*/
static inline __attribute__((always_inline)) void*
_nc_top(u3a_road* rod_u, c3_ws mov_ws, c3_w len_w)
{
  return u3to(void, (mov_ws < 0) ? rod_u->cap_p : (rod_u->cap_p - len_w));
}

/* _nc_pop(): pop words off the stack.
*/
static inline __attribute__((always_inline)) void
_nc_pop(u3a_road* rod_u, c3_ws mov_ws, c3_w len_w)
{
  rod_u->cap_p -= mov_ws * (c3_ws)len_w;
}

/* _nc_hilt_fore(): static hint prologue.  tag: the hint atom, RETAIN.
**   Produces the token for _nc_hilt_hind(): what the epilogue needs, or ~.
**   nock-compilation.hoon passes only %bout and %xray; %xray is not
**   implemented yet.
*/
static u3_noun
_nc_hilt_fore(u3_atom tag)
{
  switch ( tag ) {
    case c3__bout: {
      return u3i_chub(u3t_trace_time());
    }

    default: {
      return u3_nul;
    }
  }
}

/* _nc_hilt_hind(): static hint epilogue.  tag: RETAIN, tok: TRANSFER.
*/
static void
_nc_hilt_hind(u3_atom tag, u3_noun tok)
{
  switch ( tag ) {
    case c3__bout: {
      u3_atom delta = u3ka_sub(u3i_chub(u3t_trace_time()), tok);
      c3_c    str_c[64];

      u3a_print_time(str_c, "took", u3r_chub(0, delta));
      u3t_slog(u3nc(0, u3i_string(str_c)));
      u3z(delta);
    } break;

    default: {
      u3z(tok);
    } break;
  }
}

/* _nc_hint_fore(): dynamic hint prologue.  tag: the hint atom, RETAIN;
**   clu: the clue, TRANSFER.  Produces the token for _nc_hint_hind().
**   nock-compilation.hoon passes %bout, %xray, %spin, %jinx, %live,
**   %hunk, %hand, %lose, %mean, %spot and %slog; %xray is not implemented
**   yet, %hand is unknown, and both just drop the clue.
*/
static u3_noun
_nc_hint_fore(u3_atom tag, u3_noun clu)
{
  switch ( tag ) {
    case c3__hunk:
    case c3__lose:
    case c3__mean:
    case c3__spot: {
      u3t_push(u3nc(tag, clu));
      return u3_nul;
    }

    case c3__live: {
      if ( c3y == u3ud(clu) ) {
        u3t_heck(clu);
      }
      u3z(clu);
      return u3_nul;
    }

    case c3__slog: {
      if ( !(u3C.wag_h & u3o_quiet) ) {
        u3t_slog(clu);
      }
      else {
        u3z(clu);
      }
      return u3_nul;
    }

    case c3__jinx: {
      if ( (c3n == u3ud(clu)) || (u3_nul == clu) ) {
        u3z(clu);
        return u3_nul;
      }
      u3m_timer_set(clu);
      return c3__jinx;
    }

    case c3__spin: {
      u3t_sstack_push(clu);
      return u3_nul;
    }

    case c3__bout: {
      return u3nc(clu, u3i_chub(u3t_trace_time()));
    }

    default: {
      u3z(clu);
      return u3_nul;
    }
  }
}

/* _nc_hint_hind(): dynamic hint epilogue.  tag: RETAIN, tok: TRANSFER.
*/
static void
_nc_hint_hind(u3_atom tag, u3_noun tok)
{
  switch ( tag ) {
    case c3__hunk:
    case c3__lose:
    case c3__mean:
    case c3__spot: {
      u3t_drop();
    } break;

    case c3__jinx: {
      if ( c3__jinx == tok ) {
        u3m_timer_pop();
      }
    } break;

    case c3__spin: {
      u3t_sstack_pop();
    } break;

    case c3__bout: {
      u3_noun clu, now, pri, tan;
      u3_atom delta;
      c3_c    str_c[64];

      u3x_cell(tok, &clu, &now);
      delta = u3ka_sub(u3i_chub(u3t_trace_time()), u3k(now));
      u3a_print_time(str_c, "took", u3r_chub(0, delta));

      //  caption the report with the tank of the clue, if it has one
      //
      if ( c3y == u3r_cell(clu, &pri, &tan) ) {
        c3_h pri_h = ( c3y == u3a_is_cat(pri) ) ? pri : 0;
        u3t_slog_cap(pri_h, u3k(tan), u3i_string(str_c));
      }
      else {
        u3t_slog(u3nc(0, u3i_string(str_c)));
      }
      u3z(delta);
    } break;

    default: break;
  }

  u3z(tok);
}

/* _nc_burn_north(), _nc_burn_south(): run a program on its arguments.
**   TRANSFERS the arguments.  Body in nock-compile-burn.c, instantiated
**   once per road direction.
*/
#define _nc_mov_ws   (-1)
#define _nc_burn  _nc_burn_north
#include "nock-compile-burn.c"
#undef _nc_mov_ws
#undef _nc_burn

#define _nc_mov_ws   1
#define _nc_burn  _nc_burn_south
#include "nock-compile-burn.c"
#undef _nc_mov_ws
#undef _nc_burn

/* _nc_burn_out(): run a program on its arguments.  TRANSFERS.
*/
static u3_noun
_nc_burn_out(u3nc_prog* pog_u, u3_noun* arg, c3_h len_h)
{
  u3_noun pro;

  u3t_on(noc_o);
  pro = ( c3y == u3a_is_north(u3R) )
      ? _nc_burn_north(pog_u, arg, len_h)
      : _nc_burn_south(pog_u, arg, len_h);
  u3t_off(noc_o);

  return pro;
}

/* u3nc_nock_on(): produce .*(bus fol).
*/
u3_noun
u3nc_nock_on(u3_noun bus, u3_noun fol)
{
  u3nc_prog* pog_u;
  u3_noun    pro;


  pog_u = _nc_entry(bus, fol);
  u3z(fol);
  pro = _nc_burn_out(pog_u, &bus, 1);

  if ( _nc_verb_t ) {
    fprintf(stderr, "u3nc: %" PRIu64 " entries, %" PRIu64 " compiled, "
                    "%" PRIu64 " jetted sites, %" PRIu64 " unresolved rings, "
                    "%" PRIu64 " direct calls, %" PRIu64 " subject calls, "
                    "%" PRIu64 " jet hits, %" PRIu64 " cons, "
                    "%" PRIu64 " unique decons, %" PRIu64 " drops\r\n",
            u3nc_Stat.ent_d, u3nc_Stat.com_d, u3nc_Stat.arm_d,
            u3nc_Stat.rin_d, u3nc_Stat.dir_d, u3nc_Stat.sub_d,
            u3nc_Stat.jet_d, u3nc_Stat.con_d, u3nc_Stat.uni_d,
            u3nc_Stat.dro_d);
  }

  return pro;
}

/* u3nc_scan(): analyze and compile [bus fol] without running it.
*/
void
u3nc_scan(u3_noun bus, u3_noun fol)
{
  _nc_entry(bus, fol);
  u3z(bus);
  u3z(fol);
}

/* _nc_prog_free(): free a program.
*/
static void
_nc_prog_free(u3nc_prog* pog_u)
{
  c3_h i_h;

  u3z(pog_u->ned);

  for ( i_h = 0; i_h < pog_u->lit_u.len_h; i_h++ ) {
    u3z(pog_u->lit_u.non[i_h]);
  }

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3z(pog_u->dir_u.dat_u[i_h].bell);
    u3z(pog_u->dir_u.dat_u[i_h].ring);
  }

  u3a_free(pog_u);
}

/* _nc_prog_take(): copy a junior program to the current road.
*/
static u3nc_prog*
_nc_prog_take(u3nc_prog* pog_u)
{
  u3nc_prog* gop_u = _nc_prog_new(pog_u->byc_u.len_h,
                                  pog_u->lit_u.len_h,
                                  pog_u->dir_u.len_h,
                                  pog_u->sot_u.len_h);
  c3_h i_h;

  gop_u->tot_h = pog_u->tot_h;
  gop_u->arg_h = pog_u->arg_h;
  gop_u->ned   = u3a_take(pog_u->ned);

  memcpy(gop_u->byc_u.ops_y, pog_u->byc_u.ops_y, pog_u->byc_u.len_h);
  memcpy(gop_u->sot_u.sot_h, pog_u->sot_u.sot_h,
         pog_u->sot_u.len_h * sizeof(c3_h));
  memcpy(gop_u->sot_u.kon_y, pog_u->sot_u.kon_y,
         pog_u->sot_u.len_h * sizeof(c3_y));

  for ( i_h = 0; i_h < pog_u->lit_u.len_h; i_h++ ) {
    gop_u->lit_u.non[i_h] = u3a_take(pog_u->lit_u.non[i_h]);
  }

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3nc_dire* dir_u = &(gop_u->dir_u.dat_u[i_h]);

    *dir_u       = pog_u->dir_u.dat_u[i_h];
    dir_u->bell  = u3a_take(dir_u->bell);
    dir_u->ring  = u3a_take(dir_u->ring);
    dir_u->pog_p = 0;
  }

  return gop_u;
}

/* _nc_take_dir_cb(): take a direct program.
*/
static u3_noun
_nc_take_dir_cb(u3_noun pog)
{
  return _nc_of(_nc_prog_take(_nc_to(pog)));
}

/* _nc_take_ent_cb(): take a list of entry programs [sock program].
*/
static u3_noun
_nc_take_ent_cb(u3_noun lis)
{
  u3_noun out, i, sock, pog;
  u3_noun *h, *nex, *t = &out;

  while ( u3_nul != lis ) {
    u3x_cell(lis, &i, &lis);
    u3x_cell(i, &sock, &pog);
    *t = u3i_defcons(&h, &nex);
    t  = nex;
    *h = u3nc(u3a_take(sock), _nc_take_dir_cb(pog));
  }

  *t = u3_nul;

  return out;
}

/* u3nc_take(): copy junior program tables; sets *dir_p and *ent_p
**              to the copies.
*/
void
u3nc_take(u3p(u3h_root)* dir_p, u3p(u3h_root)* ent_p)
{
  *dir_p = u3h_take_with(*dir_p, _nc_take_dir_cb);
  *ent_p = u3h_take_with(*ent_p, _nc_take_ent_cb);
}

/* _nc_reap_dir_cb(): promote a direct program, unless there is one.
*/
static void
_nc_reap_dir_cb(u3_noun kev, void* ptr_v)
{
  u3_noun bell, pog;

  u3x_cell(kev, &bell, &pog);

  if ( u3_none == u3h_git(u3R->ska.dir_p, bell) ) {
    u3h_put(u3R->ska.dir_p, bell, pog);
  }
  else {
    _nc_prog_free(_nc_to(pog));
  }
}

/* _nc_reap_ent_cb(): promote entry programs.
*/
static void
_nc_reap_ent_cb(u3_noun kev, void* ptr_v)
{
  u3_noun fol, lis, old;

  u3x_cell(kev, &fol, &lis);

  old = u3h_get(u3R->ska.ent_p, fol);
  lis = u3kb_weld(u3k(lis), ( u3_none == old ) ? u3_nul : old);
  u3h_put(u3R->ska.ent_p, fol, lis);
}

/* _nc_link_dir_cb(): link a promoted direct program.
*/
static void
_nc_link_dir_cb(u3_noun kev, void* ptr_v)
{
  _nc_link(_nc_dir_get(u3h(kev)));
}

/* _nc_link_ent_cb(): link promoted entry programs.
*/
static void
_nc_link_ent_cb(u3_noun kev, void* ptr_v)
{
  u3_noun lis = u3t(kev), i;

  while ( u3_nul != lis ) {
    u3x_cell(lis, &i, &lis);
    _nc_link(_nc_to(u3t(i)));
  }
}

/* u3nc_reap(): promote taken program tables.
*/
void
u3nc_reap(u3p(u3h_root) dir_p, u3p(u3h_root) ent_p)
{
  u3h_walk_with(dir_p, _nc_reap_dir_cb, NULL);
  u3h_walk_with(ent_p, _nc_reap_ent_cb, NULL);
  u3h_walk_with(dir_p, _nc_link_dir_cb, NULL);
  u3h_walk_with(ent_p, _nc_link_ent_cb, NULL);
  u3h_free(dir_p);
  u3h_free(ent_p);
}

/* _nc_prog_mark(): mark a program for gc.
*/
static c3_w
_nc_prog_mark(u3nc_prog* pog_u)
{
  c3_h i_h;
  c3_w tot_w = u3a_mark_mptr(pog_u);

  tot_w += u3a_mark_noun(pog_u->ned);

  for ( i_h = 0; i_h < pog_u->lit_u.len_h; i_h++ ) {
    tot_w += u3a_mark_noun(pog_u->lit_u.non[i_h]);
  }

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    tot_w += u3a_mark_noun(pog_u->dir_u.dat_u[i_h].bell);
    tot_w += u3a_mark_noun(pog_u->dir_u.dat_u[i_h].ring);
  }

  return tot_w;
}

/* _nc_mark_dir_cb(): mark a direct program.
*/
static void
_nc_mark_dir_cb(u3_noun kev, void* ptr_v)
{
  *(c3_w*)ptr_v += _nc_prog_mark(_nc_to(u3t(kev)));
}

/* _nc_mark_ent_cb(): mark entry programs.
*/
static void
_nc_mark_ent_cb(u3_noun kev, void* ptr_v)
{
  u3_noun lis = u3t(kev), i;

  while ( u3_nul != lis ) {
    u3x_cell(lis, &i, &lis);
    *(c3_w*)ptr_v += _nc_prog_mark(_nc_to(u3t(i)));
  }
}

/* u3nc_mark(): mark the SKA core and program tables for gc.
*/
u3m_quac*
u3nc_mark(void)
{
  u3m_quac** qua_u = c3_malloc(sizeof(*qua_u) * 6);
  u3m_quac*  tot_u = c3_calloc(sizeof(*tot_u));
  c3_h       i_h;

  qua_u[0] = c3_calloc(sizeof(*qua_u[0]));
  qua_u[0]->nam_c = strdup("SKA core");
  qua_u[0]->siz_w = u3a_mark_noun(u3R->ska.cor) * sizeof(c3_w);

  qua_u[1] = c3_calloc(sizeof(*qua_u[1]));
  qua_u[1]->nam_c = strdup("direct programs");
  u3h_walk_with(u3R->ska.dir_p, _nc_mark_dir_cb, &qua_u[1]->siz_w);
  qua_u[1]->siz_w *= sizeof(c3_w);

  qua_u[2] = c3_calloc(sizeof(*qua_u[2]));
  qua_u[2]->nam_c = strdup("direct table");
  qua_u[2]->siz_w = u3h_mark_tot(u3R->ska.dir_p) * sizeof(c3_w);

  qua_u[3] = c3_calloc(sizeof(*qua_u[3]));
  qua_u[3]->nam_c = strdup("entry programs");
  u3h_walk_with(u3R->ska.ent_p, _nc_mark_ent_cb, &qua_u[3]->siz_w);
  qua_u[3]->siz_w *= sizeof(c3_w);

  qua_u[4] = c3_calloc(sizeof(*qua_u[4]));
  qua_u[4]->nam_c = strdup("entry table");
  qua_u[4]->siz_w = u3h_mark_tot(u3R->ska.ent_p) * sizeof(c3_w);

  qua_u[5] = NULL;

  tot_u->nam_c = strdup("total compiled nock");
  tot_u->qua_u = qua_u;
  for ( i_h = 0; i_h < 5; i_h++ ) {
    tot_u->siz_w += qua_u[i_h]->siz_w;
  }

  return tot_u;
}

/* _nc_free_dir_cb(): free a direct program.
*/
static void
_nc_free_dir_cb(u3_noun kev)
{
  _nc_prog_free(_nc_to(u3t(kev)));
}

/* _nc_free_ent_cb(): free entry programs.
*/
static void
_nc_free_ent_cb(u3_noun kev)
{
  u3_noun lis = u3t(kev), i;

  while ( u3_nul != lis ) {
    u3x_cell(lis, &i, &lis);
    _nc_prog_free(_nc_to(u3t(i)));
  }
}

/* u3nc_free(): free the programs and their tables.
*/
void
u3nc_free(void)
{
  u3h_walk(u3R->ska.dir_p, _nc_free_dir_cb);
  u3h_walk(u3R->ska.ent_p, _nc_free_ent_cb);
  u3h_free(u3R->ska.dir_p);
  u3h_free(u3R->ska.ent_p);
}

/* u3nc_reclaim(): free the programs to reclaim memory.
*/
void
u3nc_reclaim(void)
{
  u3nc_free();
  u3R->ska.dir_p = u3h_new();
  u3R->ska.ent_p = u3h_new();
}

/* u3nc_rewrite_compact(): rewrite the SKA state for compaction.
**
** NB: the program tables must have been cleared by u3nc_reclaim():
** programs are not nouns but hold loom pointers.
*/
void
u3nc_rewrite_compact(void)
{
  u3a_relocate_noun(&(u3R->ska.cor));
  u3h_relocate(&(u3R->ska.dir_p));
  u3h_relocate(&(u3R->ska.ent_p));
}

/* _nc_prog_ream(): fix the pointers of a restored program.
*/
static void
_nc_prog_ream(u3nc_prog* pog_u)
{
  c3_h i_h;

  _nc_prog_fix(pog_u);

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    _nc_dire_jet(&(pog_u->dir_u.dat_u[i_h]));
  }
}

/* _nc_ream_dir_cb(): ream a direct program.
*/
static void
_nc_ream_dir_cb(u3_noun kev)
{
  _nc_prog_ream(_nc_to(u3t(kev)));
}

/* _nc_ream_ent_cb(): ream entry programs.
*/
static void
_nc_ream_ent_cb(u3_noun kev)
{
  u3_noun lis = u3t(kev), i;

  while ( u3_nul != lis ) {
    u3x_cell(lis, &i, &lis);
    _nc_prog_ream(_nc_to(u3t(i)));
  }
}

/* u3nc_ream(): refresh after restoring from checkpoint.
*/
void
u3nc_ream(void)
{
  u3_assert( u3R == &(u3H->rod_u) );
  u3h_walk(u3R->ska.dir_p, _nc_ream_dir_cb);
  u3h_walk(u3R->ska.ent_p, _nc_ream_ent_cb);
}
