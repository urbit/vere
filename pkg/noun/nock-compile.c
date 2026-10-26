/// @file

#include "nock-compile.h"

#include "allocate.h"
#include "c3/defs.h"
#include "c3/motes.h"
#include "direct.h"
#include "error.h"
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
#include <limits.h>

/*  Bytecode.
**
**  Every opcode with immediate arguments comes in three widths, for all
**  of its immediates at once: _B, one byte each; _S, two bytes each,
**  little-endian; _V, a length byte followed by that many bytes.  The
**  _B and _S widths are opcodes; the _V widths, which only a program
**  with over 65535 slots, literals, sites or bytes of code needs, are a
**  second table of them behind the prefix WIDE, for a dispatch more.
**  Slots, literal and call site indices, and jump targets (absolute byte
**  offsets) are immediates.  The IMM opcodes are the exception: IMM_B
**  and IMM_S carry the atom itself, and all of them write to a one-byte
**  slot.  A call site holds its argument slots and its destination slot
**  d, so the call opcodes carry only the site index.
**
**  Slots hold counted references.  An op reads a source slot either
**  borrowed, gaining a reference for its product and leaving the slot as
**  it was, or consumed: the slot gives up its reference to the op and is
**  empty.  An op with sources comes in a variant per subset of its
**  sources consumed, _0 (none) to _3 (both), bit i for source i; a call
**  site holds a consume flag per argument instead; a branch (CLQ, EQQ,
**  EQI, EQL, BRN, BRZ) consumes a source per edge, bit 2i for source i
**  on the fall-through and bit 2i+1 on the jump.  The compiler consumes
**  a register at its last use and drops (DRO) the registers that die
**  anywhere else (see _nc_lives()), so a slot holds a reference exactly
**  while the register in it is live, a destination slot holds none when
**  written, and an activation ends with every slot empty but the one it
**  returns.
**  An empty slot is zeroed only in a build that reads empty slots: for
**  sweeps, or for the checks (_nc_zer_t).
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
**    HED   s d      -.s -> d, s is guaranteed to be a cell by Hoon IR emitter
**    TAL   s d      +.s -> d, s is guaranteed to be a cell by Hoon IR emitter
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
**                   the parameter slots and it starts over (a loop);
**                   emitted sweeping only, the others being moves and a
**                   HOP to the entry (_nc_shuffle())
**    JMF   i        CAF in tail position
**    JSP   i        CAP in tail position
**    DON   s        return s, consumed
**    DRO   s        drop s: lose its reference, 0 -> s
**    BOM            crash
**    WIDE  o        the _V width of an op, o its index in their table
**
**  DON, JMP, JMS, JMF and JSP end the activation, and come in a sweeping
**  variant (_1) that first drops every slot it doesn't take: it stands
**  for the drops at the start of a block that does nothing else.
*/

/*  Families of the IR ops: X(fam, OP, src, dst, imm, tar, sot, kon, swe):
**    fam - the family,
**    OP  - its opcode name,
**    src - the number of source slots in the op,
**    dst - whether it has a destination slot
**    imm - an index immediate (literal or call site)
**    tar - has a jump target
**    sot - has a call site (whose argument slots are sources and which holds
**          the destination slot, if any)
**    kon - the number of variant bits in the opcode
**    swe - whether the variant bit means a sweep rather than a consumed source
**
**  A consume bit per source, except for a call (the flags are in the site), for
**  DON and DRO, which always consume, and for a branch, whose two bits per
**  source consume it on the fall-through (bit 2i) and on the jump (bit 2i+1)
**  (_nc_edgy()).
**
**  BOM has no variants.
**
**  Other special families, imm, nop and kil have no opcodes of their own: imm
**  encodes as an IMM opcode or as IML, nop as nothing, and kil as an imm of 0.
**  kil defines a register as 0: a parameter of a block that a jump to it leaves
**  undefined, so that the register is not live on the way to that jump; and 0
**  drops as nothing where the register dies on a path that never defined it
**  (_nc_jump()).
*/
#define FAMILIES(X)                                                             \
  /*fam, OP,  src, dst, imm, tar, sot, kon, swe*/                               \
  X(iml, IML, 0,   1,   1,   0,   0,   0,   0)                                  \
  X(mov, MOV, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(inc, INC, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(dec, DEC, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(add, ADD, 2,   1,   0,   0,   0,   2,   0)                                  \
  X(con, CON, 2,   1,   0,   0,   0,   2,   0)                                  \
  X(hed, HED, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(tal, TAL, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(cel, CEL, 1,   0,   0,   0,   0,   1,   0)                                  \
  X(lob, LOB, 1,   0,   0,   0,   0,   1,   0)                                  \
  X(equ, EQU, 2,   0,   0,   0,   0,   2,   0)                                  \
  X(hsp, HSP, 0,   0,   1,   0,   0,   0,   0)                                  \
  X(hse, HSE, 0,   0,   1,   0,   0,   0,   0)                                  \
  X(hdp, HDP, 1,   0,   1,   0,   0,   1,   0)                                  \
  X(hde, HDE, 1,   0,   1,   0,   0,   1,   0)                                  \
  X(spy, SPY, 2,   1,   0,   0,   0,   2,   0)                                  \
  X(nok, NOK, 2,   1,   0,   0,   0,   2,   0)                                  \
  X(cal, CAL, 0,   0,   1,   0,   1,   0,   0)                                  \
  X(caf, CAF, 0,   0,   1,   0,   1,   0,   0)                                  \
  X(cap, CAP, 0,   0,   1,   0,   1,   0,   0)                                  \
  X(cam, CAM, 0,   0,   1,   0,   1,   0,   0)                                  \
  X(csm, CSM, 0,   0,   1,   0,   1,   0,   0)                                  \
  X(clq, CLQ, 1,   0,   0,   1,   0,   2,   0)                                  \
  X(eqq, EQQ, 2,   0,   0,   1,   0,   4,   0)                                  \
  X(eqi, EQI, 1,   0,   1,   1,   0,   2,   0)                                  \
  X(eql, EQL, 1,   0,   1,   1,   0,   2,   0)                                  \
  X(brn, BRN, 1,   0,   0,   1,   0,   2,   0)                                  \
  X(brz, BRZ, 1,   0,   0,   1,   0,   2,   0)                                  \
  X(hop, HOP, 0,   0,   0,   1,   0,   0,   0)                                  \
  X(jmp, JMP, 0,   0,   1,   0,   1,   1,   1)                                  \
  X(jms, JMS, 0,   0,   1,   0,   1,   1,   1)                                  \
  X(jmf, JMF, 0,   0,   1,   0,   1,   1,   1)                                  \
  X(jsp, JSP, 0,   0,   1,   0,   1,   1,   1)                                  \
  X(don, DON, 1,   0,   0,   0,   0,   1,   1)                                  \
  X(dro, DRO, 1,   0,   0,   0,   0,   0,   0)

#define _nc_fam_enum(fam, OP, src, dst, imm, tar, sot, kon, swe)  _nc_##fam,
enum { FAMILIES(_nc_fam_enum) _nc_bom, _nc_imm, _nc_nop, _nc_kil };
#undef _nc_fam_enum

/*  The opcodes of a family, in the order of the consume variants and,
**  within a variant, of the widths: OPCODES lists the _B and _S widths
**  of them all, through Y, and WIDES the _V widths, behind WIDE.
*/
#define _nc_wid(op)        Y(op##_B) Y(op##_S)
#define _nc_vid(op)        Y(op##_V)
#define _nc_kon_0(W, op)   W(op)
#define _nc_kon_1(W, op)   W(op##_0) W(op##_1)
#define _nc_kon_2(W, op)   W(op##_0) W(op##_1) W(op##_2) W(op##_3)
#define _nc_kon_4(W, op)   W(op##_0)  W(op##_1)  W(op##_2)  W(op##_3)           \
                           W(op##_4)  W(op##_5)  W(op##_6)  W(op##_7)           \
                           W(op##_8)  W(op##_9)  W(op##_10) W(op##_11)          \
                           W(op##_12) W(op##_13) W(op##_14) W(op##_15)
#define _nc_fam_ops(fam, OP, src, dst, imm, tar, sot, kon, swe)                 \
  _nc_kon_##kon(_nc_wid, OP)
#define _nc_fam_wides(fam, OP, src, dst, imm, tar, sot, kon, swe)               \
  _nc_kon_##kon(_nc_vid, OP)

#define OPCODES  Y(IMM_0) Y(IMM_1) Y(IMM_B) Y(IMM_S)                            \
                 FAMILIES(_nc_fam_ops)                                          \
                 Y(BOM) Y(WIDE)
#define WIDES    FAMILIES(_nc_fam_wides)

#define Y(op) op,
enum { OPCODES LAST };
enum { WIDES LAST_V };
static_assert((LAST <= 256) && (LAST_V <= 256));
#undef Y

#define Y(op) #op,
static const c3_c* _nc_name_c[] = { OPCODES };
#undef Y

/* _nc_fam: operands of a family, materializes X table and adds special-cased
**          ops.
*/
#define _nc_fam_row(fam, OP, src, dst, imm, tar, sot, kon, swe)                 \
  [_nc_##fam] = { src, dst, imm, tar, sot, kon, swe },
static const struct {
  c3_y src_y, dst_y, imm_y, tar_y, sot_y, kon_y, swe_y;
} _nc_fam[] = {
  FAMILIES(_nc_fam_row)
  /*fam,        src, dst, imm, tar, sot, kon, swe*/
  [_nc_bom] = { 0,   0,    0,  0,   0,   0,   0 },
  [_nc_imm] = { 0,   1,    0,  0,   0,   0,   0 },
  [_nc_nop] = { 0,   0,    0,  0,   0,   0,   0 },
  [_nc_kil] = { 0,   1,    0,  0,   0,   0,   0 },
};
#undef _nc_fam_row

/* _nc_edgy(): whether a family is a branch on sources, whose variant
**             bits consume them per edge: for source i, bit 2i on the
**             fall-through (the yes block), bit 2i+1 on the jump.
*/
#define _nc_edgy(fam)  (_nc_fam[fam].tar_y && _nc_fam[fam].src_y)

/* _nc_bas_y: first opcode of a family in OPCODES: the opcode of an op is
**            _nc_bas_y[fam] + 2 * variant bits + width, for the widths
**            _B (0) and _S (1).  _nc_vas_y: the first in WIDES: the _V
**            width is WIDE, then _nc_vas_y[fam] + variant bits.
*/
#define _nc_first_0(op, W)  op##_##W
#define _nc_first_1(op, W)  op##_0_##W
#define _nc_first_2(op, W)  op##_0_##W
#define _nc_first_4(op, W)  op##_0_##W
#define _nc_fam_bas(fam, OP, src, dst, imm, tar, sot, kon, swe)                 \
  [_nc_##fam] = _nc_first_##kon(OP, B),
#define _nc_fam_vas(fam, OP, src, dst, imm, tar, sot, kon, swe)                 \
  [_nc_##fam] = _nc_first_##kon(OP, V),
static const c3_y _nc_bas_y[] = { FAMILIES(_nc_fam_bas) };
static const c3_y _nc_vas_y[] = { FAMILIES(_nc_fam_vas) };
#undef _nc_fam_bas
#undef _nc_fam_vas

/* _nc_fam_c: names of the families, for _nc_print().
*/
#define _nc_fam_nam(fam, OP, src, dst, imm, tar, sot, kon, swe)                 \
  [_nc_##fam] = #OP,

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
  c3_h  dst_h;    //  destination register for non-call ops
  c3_h  imm_h;    //  index immediate, or the atom of an inline immediate
  c3_h  tar_h;    //  target block, or the literal of an inline immediate
} nc_op;

/* nc_blk: a basic block.
*/
typedef struct {
  u3_noun blob;     //  [params body fin] of the block, or none if made
                    //  by _nc_lives()
  c3_h    fir_h;    //  first op in the nc_op array
  c3_h    len_h;    //  number of ops in the nc_op array
  c3_h    off_h;    //  byte offset
  c3_h    lay_h;    //  position in the layout, or _nc_none if unreachable
  c3_h    kid_h[2]; //  successors: the jump target first, then the yes
                    //  block of a branch (_nc_kids())
  c3_y    kin_y;    //    and their number
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
  c3_h          poc_h;    //    and its capacity
  c3_h          pon_h;    //    and its length
  c3_y*         pok_y;    //  consume flags of the argument registers
  c3_h          pkc_h;    //    and its capacity (grows with pol_h)
  u3p(u3h_root) lit_p;    //  literal -> index
  c3_h          lit_h;
  c3_h          reg_h;    //  registers
  c3_h          tot_h;    //  slots
  u3_weak       bell;     //  bell of the program, if a tail call to it can
                          //  be a self call (a direct program), else none
} nc_gen;

#define _nc_grow(arr, len, cap, typ)                                            \
  if ( (len) >= (cap) ) {                                                       \
    c3_h _old = (cap);                                                          \
    if ( _old >= 0x80000000 ) {                                                 \
      u3m_bail(c3__meme);                                                       \
    }                                                                           \
    (cap) = (cap) ? (2 * (cap)) : 64;                                           \
    (arr) = u3a_realloc((arr), _old * sizeof(typ), (cap) * sizeof(typ));        \
  }

/* u3nc_Stat: cumulative counters, printed after each u3nc_nock_on()
**            when U3NC_VERBOSE is set.  The ones in the interpreter
**            loop are only kept when compiled with U3NC_STAT.
*/
u3nc_stat u3nc_Stat;

#ifdef U3NC_STAT
#  define _nc_stat(fel)  (u3nc_Stat.fel++)
#  define _nc_stat_if(con, fel)  do { if ( con ) { u3nc_Stat.fel++; } } while(0)
#else
#  define _nc_stat(fel)  ((void)0)
#  define _nc_stat_if(con, fel)  ((void)0)
#endif

#ifdef U3NC_VERBOSE
static const c3_t _nc_verb_t = 1;
#else
static const c3_t _nc_verb_t = 0;
#endif

/* _nc_swe_t: whether a block of only drops and a terminator leaves the
**            drops to the terminator's sweeping variant, which drops
**            whatever every slot of the activation holds (U3NC_SWEEP).
**            Off by default: a sweep needs every empty slot zeroed.
*/
#ifdef U3NC_SWEEP
static const c3_t _nc_swe_t = 1;
#else
static const c3_t _nc_swe_t = 0;
#endif

/* _nc_zer_t: whether the interpreter zeroes a slot it empties.  A slot
**            holds a reference iff its register is live, and a drop
**            never meets an empty slot (_nc_lives()), so only a sweep,
**            and the checks of U3NC_CHECK and C3DBG, read empty slots.
*/
#if defined(U3NC_SWEEP) || defined(U3NC_CHECK) || defined(C3DBG)
static const c3_t _nc_zer_t = 1;
#else
static const c3_t _nc_zer_t = 0;
#endif

static_assert(!_nc_swe_t || _nc_zer_t,
  "sweeping requires zeroing out of empty slots");

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

  u3_assert(got == (c3_h)got);

  return (c3_h)got;
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
  static_assert(u3a_cells <= c3_w_max);
  c3_w len_w = 0;

  for ( ; u3_nul != lis; lis = u3t(lis) ) {
    len_w++;
  }

  if ( len_w != (c3_h)len_w ) {
    u3m_bail(c3__fail);
  }

  return (c3_h)len_w;
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
typedef enum {
  _nc_kind_plain,
  _nc_kind_total,
  _nc_kind_punt
} _nc_kind_e ;

static _nc_kind_e
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
    if ( c3n == u3r_cell(arg, &i, &arg) ) u3m_bail(c3__fail);
    _nc_grow(gen_u->pol_h, gen_u->pon_h, gen_u->poc_h, c3_h);
    _nc_grow(gen_u->pok_y, gen_u->pon_h, gen_u->pkc_h, c3_y);
    gen_u->pok_y[gen_u->pon_h]   = 0;
    gen_u->pol_h[gen_u->pon_h++] = _nc_reg(gen_u, i);
    dir_u->len_h++;
  }

  u3_assert( (c3n == mon_o) || (1 == dir_u->len_h) );

  return gen_u->din_h++;
}

/* _nc_dir_sub(): append a call site by subject, in register sub.  RETAINS.
*/
static c3_h
_nc_dir_sub(nc_gen* gen_u, u3_noun bell, nc_jet jet_u, u3_weak clu, u3_noun sub)
{
  u3_noun arg   = u3nc(u3k(sub), u3_nul);
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
  //  flatten the map into an array of values gen_u->blk_u, construct a hamt
  //  gen_u->idx_p: @uwoo -> array
  //  sak: stack of nlr nodes
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

      if ( c3n == u3r_trel(nod, &n, &l, &r) ) u3m_bail(c3__fail);

      _nc_grow(gen_u->blk_u, gen_u->blk_h, blk_m, nc_blk);
      //  blk_u[i].blob holds an uncounted reference, but it's fine as long
      //  as we don't run u3r_sing on nouns that contain blob during compilation
      //  and `straight` noun outlives this reference
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

  //  successors of each block
  //
  for ( c3_h i_h = 0; i_h < gen_u->blk_h; i_h++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[i_h]);
    blk_u->kin_y = _nc_kids(gen_u, blk_u->blob, blk_u->kid_h);
  }

  //  reverse postorder (i.e. topological order) of a depth-first search from
  //  the entry block
  //  first get a postorder, then reverse it
  //
  {
    c3_h  blk_h = gen_u->blk_h;
    c3_h* sak_h = u3a_malloc(blk_h * sizeof(c3_h));
    c3_y* nex_y = u3a_calloc(blk_h, sizeof(c3_y));
    c3_y* saw_y = u3a_calloc(blk_h, sizeof(c3_y));
    c3_h  dep_h = 0;  // stack depth

    gen_u->lay_h = u3a_malloc(blk_h * sizeof(c3_h));
    gen_u->lan_h = 0;

    sak_h[dep_h++] = _nc_blk(gen_u, 0);
    saw_y[sak_h[0]] = 1;

    while ( dep_h ) {
      c3_h top_h = sak_h[dep_h - 1];
      c3_h kin_h = gen_u->blk_u[top_h].kin_y;

      if ( nex_y[top_h] < kin_h ) {
        c3_h kid = gen_u->blk_u[top_h].kid_h[nex_y[top_h]++];

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
**             records for the liveness analysis (_nc_lives()).  The kil
**             puts 0 in the slot: the parameter is live from there, and
**             the drop where it dies would meet whatever the slot held.
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
      if ( !_nc_zer_t ) {
        //  KIL can become IML[0] when encoding
        op_u->tar_h = _nc_lit(gen_u, 0);
      }
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
**             the immediate can stand in for it; a comparison with 0 is
**             a BRZ.
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

    if ( _nc_imm != def_u->fam_y ) {
      op_u->fam_y = _nc_eql;
      op_u->imm_h = def_u->imm_h;
    }
    else if ( 0 == def_u->imm_h ) {
      op_u->fam_y = _nc_brz;
      op_u->imm_h = 0;
    }
    else {
      op_u->fam_y = _nc_eqi;
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
#define _nc_srcs(gen_u, op_u, reg_h, body)                                      \
  do {                                                                          \
    const c3_y _nfam = (op_u)->fam_y;                                           \
    c3_h _i;                                                                    \
    for ( _i = 0; _i < _nc_fam[_nfam].src_y; _i++ ) {                           \
      c3_h* reg_h = &((op_u)->src_h[_i]);                                       \
      body;                                                                     \
    }                                                                           \
    if ( _nc_fam[_nfam].sot_y ) {                                               \
      nc_dir* _dir = &((gen_u)->dir_u[(op_u)->imm_h]);                          \
      for ( _i = 0; _i < _dir->len_h; _i++ ) {                                  \
        c3_h* reg_h = &((gen_u)->pol_h[_dir->sot_h + _i]);                      \
        body;                                                                   \
      }                                                                         \
    }                                                                           \
  } while ( 0 )

/* _nc_key_sort(): quicksort key_d. The inputs are typically almost-sorted so
**  middle element is used as a pivot. Small arrays are insertion-sorted.
*/
static void
_nc_key_sort(c3_d* key_d, c3_h len_h)
{
  while ( len_h > 16 ) {
    c3_d piv_d = key_d[len_h / 2];
    c3_h i_h = 0, j_h = len_h - 1;

    for (;;) {
      while ( key_d[i_h] < piv_d ) i_h++;
      while ( key_d[j_h] > piv_d ) j_h--;
      if ( i_h >= j_h ) break;
      c3_d tmp_d = key_d[i_h];
      key_d[i_h++] = key_d[j_h];
      key_d[j_h--] = tmp_d;
    }

    j_h++;

    if ( j_h < len_h - j_h ) {
      _nc_key_sort(key_d, j_h);
      key_d += j_h;
      len_h -= j_h;
    }
    else {
      _nc_key_sort(key_d + j_h, len_h - j_h);
      len_h = j_h;
    }
  }

  for ( c3_h i_h = 1; i_h < len_h; i_h++ ) {
    c3_d tmp_d = key_d[i_h];
    c3_h j_h   = i_h;
    while ( j_h > 0 && key_d[j_h - 1] > tmp_d ) {
      key_d[j_h] = key_d[j_h - 1];
      j_h--;
    }
    key_d[j_h] = tmp_d;
  }
}

/* _nc_bit_get(), _nc_bit_set(), _nc_bit_clr(): bitsets of registers.
*/
#define _nc_bit_get(set, r)  ((set)[(r) >> 6] & (1ULL << ((r) & 63)))
#define _nc_bit_set(set, r)  ((set)[(r) >> 6] |= (1ULL << ((r) & 63)))
#define _nc_bit_clr(set, r)  ((set)[(r) >> 6] &= ~(1ULL << ((r) & 63)))

/*  _nc_live_in(): lifetime analysis.
**  Since CFG is DAG one pass over the blocks in reversed topological order with
**  a backward iteration over the ops in each block is enough. 
**  This function is called in two modes:
**    - with mar_t set to true and lvo_d/lva_d set to NULL, to set dead flags
**      and consumption bits,
**    - and with mar_t set to false and lvo_d/lva_d allocated to record which
**      registers were live before and after an op.
**
**  In both passes inn_d is set to record which regs were live on entry into a
**  given block.
**
**  The bit array inn_d has size (number of blocks * regs) and the before/after
**  arrays are (number of ops * regs). To limit the amount of used memory the
**  registers are analyzed in chunks [lo, hi) which is fine since the liveness
**  of regs is independent. liv_d is scratch memory reused on each iteration
**  over a block
*/
static void
_nc_live_in(nc_gen* gen_u, c3_h lo_h, c3_h hi_h, c3_h chu_h,
            c3_d* inn_d, c3_d* liv_d, c3_d* lvo_d, c3_d* lva_d, c3_t mar_t)
{
  c3_h i_h, j_h, k_h, r_h;

  memset(inn_d, 0, (c3_z)gen_u->blk_h * chu_h * sizeof(c3_d));

  for ( i_h = gen_u->lan_h; i_h-- > 0; ) {
    nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);

    //  live out of the block: live into its successors
    //
    memset(liv_d, 0, chu_h * sizeof(c3_d));
    for ( j_h = 0; j_h < blk_u->kin_y; j_h++ ) {
      c3_d* kin_d = inn_d + ((c3_z)blk_u->kid_h[j_h] * chu_h);
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

      if ( lva_d ) {
        memcpy(lva_d + ((c3_z)(blk_u->fir_h + j_h) * chu_h), liv_d,
               chu_h * sizeof(c3_d));
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
        if ( _nc_dro == op_u->fam_y ) {
          //  the register is already dead at this point, nothing to do
          break;
        }
        r_h = op_u->src_h[k_h];
        if ( (r_h >= lo_h) && (r_h < hi_h) ) {
          if ( !_nc_bit_get(liv_d, r_h - lo_h) ) {
            if ( mar_t && !_nc_fam[op_u->fam_y].swe_y ) {
              op_u->kon_y |= _nc_edgy(op_u->fam_y)
                           ? (3 << (2 * k_h))
                           : (1 << k_h);
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

/* _nc_sink(): move an immediate (IMM, IML) read in one block only, other
**             than its own, to that block, right before its first read
**             there.  It is then live in no other block, where it would
**             be dropped on every path that doesn't read it: SKA defines
**             the ~ of a ?~ before the test, say, and the cons arm would
**             drop it each time around.  The ops are rebuilt in layout
**             order; nothing else changes.
*/
static void
_nc_sink(nc_gen* gen_u)
{
  static const c3_h _nc_many = (c3_h)-2;
  c3_h   reg_h = gen_u->reg_h;
  c3_h   opn_h = gen_u->opn_h;

  // block reading the register, none, or many
  c3_h*  use_h = u3a_malloc(c3_max(reg_h, 1) * sizeof(c3_h));

  // and the first op reading it there
  c3_h*  pos_h = u3a_malloc(c3_max(reg_h, 1) * sizeof(c3_h));

  //  the ops sunk into a block, chained
  c3_h*  nex_h = u3a_malloc(c3_max(opn_h, 1) * sizeof(c3_h));

  //  whether an op sinks
  c3_y*  sun_y = u3a_calloc(c3_max(opn_h, 1), sizeof(c3_y));

  //  the chain of a block
  c3_h*  fir_h = u3a_malloc(c3_max(gen_u->blk_h, 1) * sizeof(c3_h));
  c3_h*  las_h = u3a_malloc(c3_max(gen_u->blk_h, 1) * sizeof(c3_h));

  c3_h   i_h, j_h, r_h, b_h, sun_h = 0;

  for ( r_h = 0; r_h < reg_h; r_h++ ) {
    use_h[r_h] = _nc_none;
  }
  for ( b_h = 0; b_h < gen_u->blk_h; b_h++ ) {
    fir_h[b_h] = las_h[b_h] = _nc_none;
  }

  //  where each register is read
  //
  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[b_h = gen_u->lay_h[i_h]]);

    for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
      _nc_srcs(gen_u, &(gen_u->ops_u[blk_u->fir_h + j_h]), src_h, {
        if ( _nc_none == use_h[*src_h] ) {
          use_h[*src_h] = b_h;
          pos_h[*src_h] = j_h;
        }
        else if ( b_h != use_h[*src_h] ) {
          use_h[*src_h] = _nc_many;
        }
      });
    }
  }

  //  the immediates read in one other block, chained to it
  //
  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[b_h = gen_u->lay_h[i_h]]);

    for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
      c3_h   o_h  = blk_u->fir_h + j_h;
      nc_op* op_u = &(gen_u->ops_u[o_h]);
      c3_h   t_h;

      if ( (_nc_imm != op_u->fam_y) && (_nc_iml != op_u->fam_y) ) {
        continue;
      }
      t_h = use_h[op_u->dst_h];
      if ( (_nc_none == t_h) || (_nc_many == t_h) || (b_h == t_h) ) {
        continue;
      }

      sun_y[o_h] = 1;
      nex_h[o_h] = _nc_none;
      if ( _nc_none == fir_h[t_h] ) {
        fir_h[t_h] = o_h;
      }
      else {
        nex_h[las_h[t_h]] = o_h;
      }
      las_h[t_h] = o_h;
      sun_h++;
    }
  }

  if ( sun_h ) {
    nc_op* new_u = u3a_malloc(c3_max(opn_h, 1) * sizeof(nc_op));
    c3_h   new_h = 0;

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[b_h = gen_u->lay_h[i_h]]);
      c3_h    beg_h = new_h;

      for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
        c3_h o_h = blk_u->fir_h + j_h;
        c3_h s_h;

        for ( s_h = fir_h[b_h]; _nc_none != s_h; s_h = nex_h[s_h] ) {
          if ( pos_h[gen_u->ops_u[s_h].dst_h] == j_h ) {
            new_u[new_h++] = gen_u->ops_u[s_h];
          }
        }
        if ( !sun_y[o_h] ) {
          new_u[new_h++] = gen_u->ops_u[o_h];
        }
      }

      blk_u->fir_h = beg_h;
      blk_u->len_h = new_h - beg_h;
    }

    u3_assert( new_h == opn_h );

    u3a_free(gen_u->ops_u);
    gen_u->ops_u = new_u;
    gen_u->opc_h = opn_h;
  }

  u3a_free(use_h);
  u3a_free(pos_h);
  u3a_free(nex_h);
  u3a_free(sun_y);
  u3a_free(fir_h);
  u3a_free(las_h);
}

/* _nc_branch(): nc_op* to the terminator if the block b_h ends with a branching
**               op, NULL otherwise
*/
static nc_op*
_nc_branch(nc_gen* gen_u, c3_h b_h)
{
  nc_blk* blk_u = &(gen_u->blk_u[b_h]);
  c3_h    j_h;

  for ( j_h = blk_u->len_h; j_h-- > 0; ) {
    nc_op* op_u = &(gen_u->ops_u[blk_u->fir_h + j_h]);
    //  HOP might've been emitted if the op can't fall through to the yes block
    //
    if ( (_nc_nop == op_u->fam_y) || (_nc_hop == op_u->fam_y) ) {
      continue;
    }
    return ( _nc_edgy(op_u->fam_y) ) ? op_u : NULL;
  }

  return NULL;
}

/* _nc_edge_y(): the edge from block p to its kid b, as the variant bits
**               of a branch consuming its first source there: 1 for the
**               fall-through to the yes block, 2 for the jump; those for
**               source i are shifted by 2i (_nc_consume()).
*/
static c3_y
_nc_edge_y(nc_gen* gen_u, c3_h p_h, c3_h b_h)
{
  nc_blk* blk_u = &(gen_u->blk_u[p_h]);
  u3_assert(blk_u->kin_y > 1);
  if ( blk_u->kid_h[1] == b_h ) {
    return 1;
  }
  u3_assert(blk_u->kid_h[0] == b_h);
  return 2;
}

/* _nc_consume(): the branch bra consumes on edge e
**                (_nc_edge_y()) each of its sources among the registers
**                in ded, a chunk from lo to hi of those that die on the
**                edge, and takes them out of it.
*/
static void
_nc_consume(nc_op* bra_u, c3_y edg_y, c3_d* ded_d, c3_h lo_h, c3_h hi_h)
{
  c3_h k_h, r_h;

  for ( k_h = 0; k_h < _nc_fam[bra_u->fam_y].src_y; k_h++ ) {
    r_h = bra_u->src_h[k_h];
    if ( (r_h >= lo_h) && (r_h < hi_h) && _nc_bit_get(ded_d, r_h - lo_h) ) {
      _nc_bit_clr(ded_d, r_h - lo_h);
      bra_u->kon_y |= edg_y << (2 * k_h);
    }
  }
}

/* _nc_lives(): code transformations to respect the lifetimes of the registers.
**
**  First pass is dead code elimination: non-crashing ops whose products are not
**  used can be dropped, and their sources won't used by them anymore.
**
**  Next the reversed CFG is constructed with `pro_h` and `pre_h`. Predecessors
**  of a block `b` are in [ pre_h[pro_h[b]], pre_h[pro_h[b + 1]] ).
**
**  Then for each block we record whether it can drop the dead registers with
**  its terminating op (no writes in between in the body of the block)
**
**  Then we run liveness analysis.  A register dies on an edge into a block
**  when it is live out of the predecessor (a cousin block still needs it)
**  and not live into the block; an unused argument dies at the entry.
**  The drops common to every edge into a block are collected for its
**  start.  A drop on one edge but not all goes into the branch that ends
**  the predecessor, if that is one of its sources (the edge variants of
**  the branches, _nc_edgy()), else into a block of drops made for the
**  edge; a block with a lone predecessor lets its branch take its own
**  sources the same way, over a drop at the start.  So no drop ever
**  meets an empty slot, which _nc_zer_t does not allow.
**
**  Then the ops are rebuilt in layout order: a block's drops first (or
**  left to its terminator's sweep, _nc_swe_t), then its ops, with a DRO
**  after an op whose product is dead on arrival; a block of drops for a
**  fall-through edge follows the predecessor, taking over its jump to
**  the yes block if it had one; those for jump edges go at the end,
**  jumping on to their targets, and the branches are retargeted to them.
*/
static void
_nc_lives(nc_gen* gen_u, c3_h arg_h)
{
  static_assert(64 == CHAR_BIT * sizeof(c3_d));
  
  c3_h  reg_h = c3_max(gen_u->reg_h, arg_h);
  c3_h  blk_h = gen_u->blk_h;
  c3_h  wor_h = (reg_h + 63) >> 6;       //  doubles of a full bitset
  c3_h  chu_h;                           //  doubles of a chunk
  c3_h* use_h = u3a_calloc(reg_h, sizeof(c3_h));
  c3_h* pro_h = u3a_calloc(blk_h + 1, sizeof(c3_h));  //  predecessor offsets
  c3_h* pre_h;                           //  predecessors
  c3_d* inn_d;                           //  live into each block
  c3_d* liv_d;                           //  scratch
  c3_d* out_d;                           //  scratch: live out of a block
  c3_d* dro_d = NULL;                    //  drops: layout << 32 | reg
  c3_h  dro_h = 0, drc_h = 0;
  c3_d* edr_d = NULL;                    //  edge drops: edge << 32 | reg
  c3_h  edr_h = 0, erc_h = 0;
  c3_h* edc_h;                           //  drops of each edge: 2 * layout + k

  //  kind, by layout; always dro when not sweeping
  c3_y* kin_y = ( _nc_swe_t ) ? u3a_calloc(gen_u->lan_h, sizeof(c3_y)) : NULL;

  c3_y* cnt_y = u3a_calloc(gen_u->lan_h, sizeof(c3_y));  //  drops, by layout
  c3_h  i_h, j_h, k_h, r_h, b_h;

  //  kind of block: whether the registers will be freed with DROs vs by the
  //  terminator
  enum { _nc_kin_dro, _nc_kin_ter };

  //  Dead code elimination
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    _nc_srcs(gen_u, &(gen_u->ops_u[i_h]), reg_h, { use_h[*reg_h]++; });
  }
  //  one iteration backwards: works because our CFG is (at this point) acyclic
  //
  for ( i_h = gen_u->opn_h; i_h-- > 0; ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);

    switch ( op_u->fam_y ) {
      default: break;

      case _nc_imm: case _nc_iml: case _nc_mov: case _nc_kil:
      case _nc_con: case _nc_hed: case _nc_tal: {
        if ( !use_h[op_u->dst_h] ) {
          _nc_srcs(gen_u, op_u, reg_h, { use_h[*reg_h]--; });
          op_u->fam_y = _nc_nop;
        }
      } break;
    }
  }

  //  predecessors of the laid-out blocks
  //
  //  First, get the number of the incoming edges
  //
  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);
    for ( j_h = 0; j_h < blk_u->kin_y; j_h++ ) {
      pro_h[blk_u->kid_h[j_h] + 1]++;
    }
  }

  //  cumulative sum: pro_h[i] shows where the starting index for the edge array
  //
  for ( b_h = 0; b_h < blk_h; b_h++ ) {
    pro_h[b_h + 1] += pro_h[b_h];
  }

  pre_h = u3a_malloc(c3_max(pro_h[blk_h], 1) * sizeof(c3_h));
  //  fill the edge array
  //
  {
    c3_h* fil_h = u3a_calloc(blk_h, sizeof(c3_h));

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[b_h = gen_u->lay_h[i_h]]);
      for ( j_h = 0; j_h < blk_u->kin_y; j_h++ ) {
        c3_h kid = blk_u->kid_h[j_h];
        pre_h[pro_h[kid] + fil_h[kid]++] = b_h;
      }
    }

    u3a_free(fil_h);
  }

  //  how each block drops, if sweeps are enabled: a block that only ends the
  //  activation, by its terminator; otherwise by drops. With sweeps disabled:
  //  always by drops
  //
  if ( kin_y ) {
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

      kin_y[i_h] = ( (j_h == blk_u->len_h)
                   && las_u
                   && _nc_fam[las_u->fam_y].swe_y )
                 ? _nc_kin_ter
                 : _nc_kin_dro;
    }
  }

  //  liveness, a chunk of the registers at a time: the drops common to
  //  the edges into each block, and of the unused arguments at the
  //  entry; the drops of one edge in but not all, into the branch that
  //  ends the predecessor when it reads the register, else for a block
  //  on the edge
  //
  //  live-in array is bounded to be about (1 << 17) doubles == a megabyte
  //
  //  If there are more than (1 << 17) blocks (~130K) the chunk is set to 1, so
  //  the live-in array is number of blocks * sizeof(c3_d), which can be bigger
  //  than a megabyte.
  //
  chu_h = c3_max(1, c3_min(wor_h, (1 << 17) / c3_max(blk_h, 1)));
  inn_d = u3a_malloc((c3_z)blk_h * chu_h * sizeof(c3_d));
  liv_d = u3a_malloc(chu_h * sizeof(c3_d));
  out_d = u3a_malloc(chu_h * sizeof(c3_d));
  edc_h = u3a_calloc((2 * (c3_z)gen_u->lan_h) + 1, sizeof(c3_h));

#define _nc_in(b)  (inn_d + ((c3_z)(b) * chu_h))

  //  _nc_out(): live out of block b, into out_d
  //
#define _nc_out(b)  do {                                                 \
    nc_blk* _blk = &(gen_u->blk_u[b]);                                   \
    memset(out_d, 0, chu_h * sizeof(c3_d));                              \
    for ( k_h = 0; k_h < _blk->kin_y; k_h++ ) {                          \
      c3_d* _kin = _nc_in(_blk->kid_h[k_h]);                             \
      for ( c3_h _w = 0; _w < chu_h; _w++ ) {                            \
        out_d[_w] |= _kin[_w];                                           \
      }                                                                  \
    }                                                                    \
  } while ( 0 )

  for ( c3_d lo_d = 0; lo_d < reg_h; lo_d += (c3_d)chu_h << 6 ) {
    c3_h lo_h = lo_d;
    c3_h hi_h = c3_min(reg_h, lo_d + ((c3_d)chu_h << 6));

    _nc_live_in(gen_u, lo_h, hi_h, chu_h, inn_d, liv_d, NULL, NULL, 1);

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[b_h = gen_u->lay_h[i_h]]);
      c3_d*   bin_d = _nc_in(b_h);
      c3_h    pre_n = pro_h[b_h + 1] - pro_h[b_h];

      //  a layed out block is either the entry block or has some predecessors
      u3_assert(0 == i_h || pre_n > 0);

      //  crashing blocks don't need DROps
      //  may cause a leak. see _nc_doomed()
      //
      if (  blk_u->len_h
         && (_nc_bom == gen_u->ops_u[blk_u->fir_h + blk_u->len_h - 1].fam_y) )
      {
        continue;
      }

      //  dead on every edge in: live out of every predecessor (the
      //  arguments, before the entry) and not live into the block
      //  the name is a bit misleading: liv_d will become a set of regs that die
      //  in b_h on all incoming edges
      //
      if ( 0 == i_h ) {
        memset(liv_d, 0, chu_h * sizeof(c3_d));
        for ( r_h = lo_h; (r_h < hi_h) && (r_h < arg_h); r_h++ ) {
          _nc_bit_set(liv_d, r_h - lo_h);
        }
      }
      else {
        memset(liv_d, 0xff, chu_h * sizeof(c3_d));
        for ( j_h = pro_h[b_h]; j_h < pro_h[b_h + 1]; j_h++ ) {
          _nc_out(pre_h[j_h]);
          for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
            liv_d[w_h] &= out_d[w_h];
          }
        }
      }
      for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
        liv_d[w_h] &= ~bin_d[w_h];
      }

      //  a lone predecessor's branch takes its own sources instead
      //
      if ( 1 == pre_n ) {
        c3_h p_h = pre_h[pro_h[b_h]];
        nc_op* bra_u = _nc_branch(gen_u, p_h);
        if ( NULL != bra_u ) {
          _nc_consume(bra_u, _nc_edge_y(gen_u, p_h, b_h), liv_d, lo_h, hi_h);
        }
      }

      //  emit DROs
      for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
        c3_d ded_d = liv_d[w_h];

        while ( ded_d ) {
          c3_h bit_h = c3_tz_d(ded_d);
          ded_d &= ded_d - 1;
          _nc_grow(dro_d, dro_h, drc_h, c3_d);
          dro_d[dro_h++] = ((c3_d)i_h << 32) | (lo_h + (w_h << 6) + bit_h);
          cnt_y[i_h] |= 1;
        }
      }

      //  dead on this edge in but not all: into the branch, or for a
      //  block on the edge
      //
      if ( pre_n > 1 ) {
        for ( j_h = pro_h[b_h]; j_h < pro_h[b_h + 1]; j_h++ ) {
          c3_h p_h   = pre_h[j_h];
          nc_op* bra_u = _nc_branch(gen_u, p_h);
          if ( NULL == bra_u ) {
            //  predecessor with a single successor has no deaths on the edge;
            //  its live-out == successor's live-in
            continue;
          }
          c3_y edg_y = _nc_edge_y(gen_u, p_h, b_h);

          // edge identifier
          c3_h edg_h = (2 * gen_u->blk_u[p_h].lay_h) + (edg_y & 1);

          _nc_out(p_h);
          //  out_d: live out of p_h but dead in b_h and not in liv_d: set of
          //  registers that die on this edge
          for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
            out_d[w_h] &= ~bin_d[w_h] & ~liv_d[w_h];
          }
          //  branching op can consume its sources itself
          _nc_consume(bra_u, edg_y, out_d, lo_h, hi_h);

          //  and the remainder will get emitted into a block that will get
          //  spliced into the edge
          for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
            c3_d ded_d = out_d[w_h];

            while ( ded_d ) {
              c3_h bit_h = c3_tz_d(ded_d);
              ded_d &= ded_d - 1;
              r_h = lo_h + (w_h << 6) + bit_h;

              _nc_grow(edr_d, edr_h, erc_h, c3_d);
              edr_d[edr_h++] = ((c3_d)edg_h << 32) | r_h;
              edc_h[edg_h]++;
            }
          }
        }
      }
    }
  }

#undef _nc_out
#undef _nc_in

  //  rebuild the ops in layout order with the drops: those common to the
  //  edges into a block at its start, as drops or left to the terminator;
  //  that of a product never used right after its op; those of one edge
  //  in a block on the edge: after the predecessor for its fall-through,
  //  taking over its jump to the yes block if it had one, else at the
  //  end, jumping on, with the branch retargeted to it.  The removed
  //  ops go.
  //
  _nc_key_sort(dro_d, dro_h);
  _nc_key_sort(edr_d, edr_h);

  {
    c3_h*  edo_h = u3a_malloc(((2 * (c3_z)gen_u->lan_h) + 1) * sizeof(c3_h));
    c3_h   edn_h = 0, e_h;                     //  edge blocks
    c3_h*  lay_h;                              //  the new layout
    c3_h   lan_h = 0;
    c3_h*  def_h;                              //  edge blocks for the end
    c3_h*  dfe_h;                              //    and their edges
    c3_h   dfn_h = 0;
    c3_d   cap_d;
    c3_h   cap_h;
    nc_op* new_u;
    c3_h   new_h = 0, dri_h = 0;

    for ( e_h = 0, edo_h[0] = 0; e_h < 2 * gen_u->lan_h; e_h++ ) {
      edo_h[e_h + 1] = edo_h[e_h] + edc_h[e_h];
      edn_h += !!edc_h[e_h];
    }

    cap_d = (2 * (c3_d)gen_u->opn_h) + dro_h + edr_h + edn_h;
    if ( cap_d > c3_h_max ) {
      u3m_bail(c3__meme);
    }
    cap_h = cap_d;
    new_u = u3a_malloc(c3_max(cap_h, 1) * sizeof(nc_op));
    lay_h = u3a_malloc(((c3_z)gen_u->lan_h + edn_h) * sizeof(c3_h));
    def_h = u3a_malloc(c3_max(edn_h, 1) * sizeof(c3_h));
    dfe_h = u3a_malloc(c3_max(edn_h, 1) * sizeof(c3_h));

    if ( edn_h ) {
      gen_u->blk_u = u3a_realloc(gen_u->blk_u, blk_h * sizeof(nc_blk),
                                 ((c3_z)blk_h + edn_h) * sizeof(nc_blk));
    }

    //  _nc_edrops(): the drops of edge e
    //
#define _nc_edrops(e)  do {                                                     \
    for ( c3_h _i = edo_h[e]; _i < edo_h[(e) + 1]; _i++ ) {                     \
      nc_op* _op = _nc_op_init(&(new_u[new_h++]), _nc_dro);                     \
      _op->src_h[0] = (c3_h)edr_d[_i];                                          \
    }                                                                           \
  } while ( 0 )

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);
      c3_h    fir_h = new_h;
      nc_op*  bra_u = NULL;               //  its branch, copied
      nc_op*  hop_u = NULL;               //  its jump to the yes block, copied
      c3_y    k_y;

      lay_h[lan_h++] = gen_u->lay_h[i_h];

      //  emit DROs in the beginning of the block, or skip them if the control
      //  flow op will sweep
      switch ( ( kin_y ) ? kin_y[i_h] : _nc_kin_dro ) {
        default: u3_assert(0);

        case _nc_kin_dro: {
          while ( (dri_h < dro_h) && ((c3_h)(dro_d[dri_h] >> 32) == i_h) ) {
            nc_op* op_u = _nc_op_init(&(new_u[new_h++]), _nc_dro);
            op_u->src_h[0] = (c3_h)dro_d[dri_h++];
          }
        } break;

        case _nc_kin_ter: {
          while ( (dri_h < dro_h) && ((c3_h)(dro_d[dri_h] >> 32) == i_h) ) {
            dri_h++;
          }
        } break;
      }
      //  copy ops
      for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
        nc_op* op_u = &(gen_u->ops_u[blk_u->fir_h + j_h]);

        if ( _nc_nop == op_u->fam_y ) {
          continue;
        }

        new_u[new_h] = *op_u;

        if ( _nc_fam[op_u->fam_y].tar_y ) {
          if ( _nc_fam[op_u->fam_y].src_y ) {
            bra_u = &(new_u[new_h]);
          }
          else {
            hop_u = &(new_u[new_h]);
          }
        }
        new_h++;

        if ( kin_y && _nc_kin_ter == kin_y[i_h] && cnt_y[i_h]
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

      //  the blocks of drops on the edges out of this one
      //
      for ( k_y = 0; k_y < 2; k_y++ ) {
        c3_h    kid_h, edg_h;
        nc_blk* edg_u;

        e_h = (2 * i_h) + k_y;

        if ( edo_h[e_h] == edo_h[e_h + 1] ) {
          continue;
        }

        //  if a block's edge has drops then it gotta be branchy
        u3_assert( bra_u && (blk_u->kin_y > 1) );

        //  insert a block with drops
        kid_h = blk_u->kid_h[k_y];
        edg_h = gen_u->blk_h++;
        edg_u = &(gen_u->blk_u[edg_h]);
        edg_u->blob     = u3_none;
        edg_u->off_h    = 0;
        edg_u->lay_h    = _nc_none;
        edg_u->kin_y    = 1;
        edg_u->kid_h[0] = kid_h;
        edg_u->kid_h[1] = _nc_none;

        if ( k_y ) {
          //  the fall-through: the block follows, and either falls
          //  through to the yes block in turn or takes over the jump
          //
          if ( hop_u ) {
            u3_assert( (hop_u->tar_h == kid_h)
                    && (hop_u == &(new_u[new_h - 1])) );
            new_h--;
            blk_u->len_h--;
          }
          edg_u->fir_h = new_h;
          _nc_edrops(e_h);
          if ( hop_u ) {
            nc_op* op_u = _nc_op_init(&(new_u[new_h++]), _nc_hop);
            op_u->tar_h = kid_h;
          }
          edg_u->len_h = new_h - edg_u->fir_h;
          lay_h[lan_h++] = edg_h;
        }
        else {
          //  the jump: retargeted to the block, which goes at the end
          //
          u3_assert( bra_u->tar_h == kid_h );
          bra_u->tar_h = edg_h;
          def_h[dfn_h] = edg_h;
          dfe_h[dfn_h] = e_h;
          dfn_h++;
        }
      }
    }

    for ( i_h = 0; i_h < dfn_h; i_h++ ) {
      nc_blk* edg_u = &(gen_u->blk_u[def_h[i_h]]);
      nc_op*  op_u;

      edg_u->fir_h = new_h;
      _nc_edrops(dfe_h[i_h]);
      op_u = _nc_op_init(&(new_u[new_h++]), _nc_hop);
      op_u->tar_h = edg_u->kid_h[0];
      edg_u->len_h = new_h - edg_u->fir_h;
      lay_h[lan_h++] = def_h[i_h];
    }

#undef _nc_edrops

    u3_assert( (dri_h == dro_h)
            && (new_h <= cap_h)
            && (lan_h == gen_u->lan_h + edn_h)
            && (gen_u->blk_h == blk_h + edn_h) );

    for ( i_h = 0; i_h < lan_h; i_h++ ) {
      gen_u->blk_u[lay_h[i_h]].lay_h = i_h;
    }

    u3a_free(gen_u->ops_u);
    gen_u->ops_u = new_u;
    gen_u->opn_h = new_h;
    gen_u->opc_h = cap_h;
    u3a_free(gen_u->lay_h);
    gen_u->lay_h = lay_h;
    gen_u->lan_h = lan_h;
    u3a_free(edo_h);
    u3a_free(def_h);
    u3a_free(dfe_h);
  }

  u3a_free(use_h);
  u3a_free(pro_h);
  u3a_free(pre_h);
  u3a_free(inn_d);
  u3a_free(liv_d);
  u3a_free(out_d);
  u3a_free(dro_d);
  u3a_free(edr_d);
  u3a_free(edc_h);
  u3a_free(kin_y);
  u3a_free(cnt_y);
}

/* _nc_range(): append an occupancy range [sta, end] of a register.
*/
typedef struct {
  c3_h* sta_h;    //  first half-position
  c3_h* end_h;    //  last half-position
  c3_h* nex_h;    //  next range of the register, or _nc_none
  c3_h  len_h, cap_h;
} nc_ran;

static void
_nc_range(nc_ran* ran_u, c3_h* fir_h, c3_h* las_h, c3_h r_h,
          c3_h sta_h, c3_h end_h)
{
  c3_h i_h = ran_u->len_h;

  if ( ran_u->len_h >= ran_u->cap_h ) {
    c3_h cap_h = ran_u->cap_h ? (2 * ran_u->cap_h) : 256;

    if ( ran_u->cap_h >= 0x80000000 ) {
      u3m_bail(c3__meme);
    }
    ran_u->sta_h = u3a_realloc(ran_u->sta_h, ran_u->cap_h * sizeof(c3_h),
                               cap_h * sizeof(c3_h));
    ran_u->end_h = u3a_realloc(ran_u->end_h, ran_u->cap_h * sizeof(c3_h),
                               cap_h * sizeof(c3_h));
    ran_u->nex_h = u3a_realloc(ran_u->nex_h, ran_u->cap_h * sizeof(c3_h),
                               cap_h * sizeof(c3_h));
    ran_u->cap_h = cap_h;
  }

  ran_u->sta_h[i_h] = sta_h;
  ran_u->end_h[i_h] = end_h;
  ran_u->nex_h[i_h] = _nc_none;
  ran_u->len_h++;

  if ( _nc_none == fir_h[r_h] ) {
    fir_h[r_h] = i_h;
  }
  else {
    ran_u->nex_h[las_h[r_h]] = i_h;
  }
  las_h[r_h] = i_h;
}

/* _nc_fits(): whether the ranges of a register, from fir_h, are free in
**             the occupancy bitmap of a slot; _nc_take(): mark them.
*/
static_assert(64 == sizeof(c3_d) * CHAR_BIT);

static c3_t
_nc_fits(const nc_ran* ran_u, c3_h fir_h, const c3_d* occ_d)
{
  for ( ; _nc_none != fir_h; fir_h = ran_u->nex_h[fir_h] ) {
    c3_h a_h = ran_u->sta_h[fir_h], b_h = ran_u->end_h[fir_h];
    c3_h w_h;

    for ( w_h = a_h >> 6; w_h <= (b_h >> 6); w_h++ ) {
      c3_d msk_d = ~(c3_d)0;

      //  first double: remove preceding, least significant bits
      if ( w_h == (a_h >> 6) ) {
        msk_d &= ~(c3_d)0 << (a_h & 63);
      }
      //  last double: remove following, most significant bits
      if ( w_h == (b_h >> 6) ) {
        msk_d &= ~(c3_d)0 >> (63 - (b_h & 63));
      }
      if ( occ_d[w_h] & msk_d ) {
        return 0;
      }
    }
  }

  return 1;
}

static void
_nc_take(const nc_ran* ran_u, c3_h fir_h, c3_d* occ_d)
{
  for ( ; _nc_none != fir_h; fir_h = ran_u->nex_h[fir_h] ) {
    c3_h a_h = ran_u->sta_h[fir_h], b_h = ran_u->end_h[fir_h];
    c3_h w_h;

    for ( w_h = a_h >> 6; w_h <= (b_h >> 6); w_h++ ) {
      c3_d msk_d = ~(c3_d)0;

      if ( w_h == (a_h >> 6) ) {
        msk_d &= ~(c3_d)0 << (a_h & 63);
      }
      if ( w_h == (b_h >> 6) ) {
        msk_d &= ~(c3_d)0 >> (63 - (b_h & 63));
      }
      occ_d[w_h] |= msk_d;
    }
  }
}

/* _nc_slots(): assign the registers to slots, rewriting the ops.
**
**  A register occupies its slot wherever it holds a reference: from its
**  definition to its last use site, which consumes it, and, if _nc_zer_t == 1,
**  zeroes it. If _nc_zer_t == 0 KIL zeroes out the slot, otherwise a slot with
**  a potentially poisoned register (control flow join where only one side sets
**  a register) could contain either a live value or an already freed garbage.
**  Occupancy of slots and liveness of registers is expressed in terms of half-
**  positions: before and after a given op.
**  Input registers get first N slots. Then the registers get slots in this
**  order of preference: first a parameter slot it wants for an argument of a
**  self tail-call JMS, then slots above the input, then the input slots, then
**  a new one. Later the self-calls will be lowered into moves of the misplaced
**  arguments plus a HOP to the entry point (_nc_shuffle).
*/
static void
_nc_slots(nc_gen* gen_u, c3_h arg_h)
{
  c3_h   reg_h = c3_max(gen_u->reg_h, arg_h);
  c3_h   opn_h = gen_u->opn_h;
  c3_h   hap_h = 2 * opn_h;                    //  half-positions
  c3_h   hwo_h = (hap_h + 63) >> 6;            //  doubles of a slot bitmap
  c3_h   wor_h = (reg_h + 63) >> 6;            //  doubles of a register bitset
  c3_h   chu_h;                                //  doubles of a chunk
  c3_h*  fir_h = u3a_malloc(reg_h * sizeof(c3_h));  //  first range
  c3_h*  las_h = u3a_malloc(reg_h * sizeof(c3_h));  //  last range
  c3_h*  wan_h = u3a_malloc(reg_h * sizeof(c3_h));  //  wanted slot
  c3_y*  won_y = u3a_calloc(reg_h, sizeof(c3_y));   //  0 none, 1 one, 2 many
  c3_h*  sot_h = u3a_malloc(reg_h * sizeof(c3_h));
  c3_d*  key_d = u3a_malloc(reg_h * sizeof(c3_d));
  c3_d*  occ_d = NULL;                         //  slot bitmaps
  c3_h   occ_h = 0;                            //  slots with a bitmap
  nc_ran ran_u = {0};
  c3_h   key_n_h = 0, tot_h = 0;
  c3_h   i_h, r_h;

  for ( r_h = 0; r_h < reg_h; r_h++ ) {
    fir_h[r_h] = _nc_none;
    las_h[r_h] = _nc_none;
    wan_h[r_h] = _nc_none;
    sot_h[r_h] = _nc_none;
  }

  //  the wants of the arguments of self calls; a parameter is in its
  //  own slot, and a register wanted in two places wants neither
  //
  for ( i_h = 0; i_h < opn_h; i_h++ ) {
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
        }
        else if ( wan_h[r_h] != k_h ) {
          won_y[r_h] = 2;
          wan_h[r_h] = _nc_none;
        }
      }
    }
  }

  //  occupancy ranges, a chunk of the registers at a time
  //
  chu_h = c3_max(1, c3_min(wor_h, (1 << 16) / c3_max(opn_h, 1)));

  {
    c3_d* inn_d = u3a_malloc((c3_z)gen_u->blk_h * chu_h * sizeof(c3_d));
    c3_d* liv_d = u3a_malloc(chu_h * sizeof(c3_d));
    c3_d* lvo_d = u3a_calloc((c3_z)opn_h * chu_h, sizeof(c3_d));
    c3_d* lva_d = u3a_calloc((c3_z)opn_h * chu_h, sizeof(c3_d));
    c3_d* cur_d = u3a_malloc(chu_h * sizeof(c3_d));
    c3_d* now_d = u3a_malloc(chu_h * sizeof(c3_d));
    c3_h* ope_h = u3a_malloc((chu_h << 6) * sizeof(c3_h));

    for ( c3_d lo_d = 0; lo_d < reg_h; lo_d += (c3_d)chu_h << 6 ) {
      c3_h lo_h = lo_d;
      c3_h hi_h = c3_min(reg_h, lo_d + ((c3_d)chu_h << 6));
      c3_h h_h;

      _nc_live_in(gen_u, lo_h, hi_h, chu_h, inn_d, liv_d, lvo_d, lva_d, 0);

      //  cur_d: occupancy bitmap at the previous half-position
      //  ope_h: beginning of the open range for a given register, or _nc_none
      //         if the range is closed
      memset(cur_d, 0, chu_h * sizeof(c3_d));
      for ( i_h = 0; i_h < (chu_h << 6); i_h++ ) {
        ope_h[i_h] = _nc_none;
      }

      for ( h_h = 0; h_h < hap_h; h_h++ ) {
        nc_op* op_u = &(gen_u->ops_u[h_h >> 1]);
        c3_h   w_h;

        //  _nc_live_in gave us liveness but not the occupancy:
        //    - dead products occupy their slot
        //    - DROpped registers occupy their slot when they get dropped
        //    - input parameters may be dead (no one reads them) but they occupy
        //      slots
        if ( h_h & 1 ) {
          memcpy(now_d, lva_d + ((c3_z)(h_h >> 1) * chu_h),
                 chu_h * sizeof(c3_d));
          r_h = op_u->dst_h;
          if ( (_nc_none != r_h) && (r_h >= lo_h) && (r_h < hi_h) ) {
            _nc_bit_set(now_d, r_h - lo_h);
          }
        }
        else {
          memcpy(now_d, lvo_d + ((c3_z)(h_h >> 1) * chu_h),
                 chu_h * sizeof(c3_d));
          if ( _nc_dro == op_u->fam_y ) {
            r_h = op_u->src_h[0];
            if ( (r_h >= lo_h) && (r_h < hi_h) ) {
              _nc_bit_set(now_d, r_h - lo_h);
            }
          }
          if ( !h_h ) {
            for ( r_h = lo_h; (r_h < hi_h) && (r_h < arg_h); r_h++ ) {
              _nc_bit_set(now_d, r_h - lo_h);
            }
          }
        }

        //  ranges open where occupancy begins and close where it ends
        //
        for ( w_h = 0; w_h < chu_h; w_h++ ) {
          c3_d dif_d = cur_d[w_h] ^ now_d[w_h];

          while ( dif_d ) {
            c3_h bit_h = (w_h << 6) + c3_tz_d(dif_d);
            dif_d &= dif_d - 1;

            if ( (now_d[w_h] >> (bit_h & 63)) & 1 ) {
              ope_h[bit_h] = h_h;
            }
            else {
              _nc_range(&ran_u, fir_h, las_h,
                lo_h + bit_h, ope_h[bit_h], h_h - 1);

              ope_h[bit_h] = _nc_none;
            }
          }
          cur_d[w_h] = now_d[w_h];
        }
      }

      for ( i_h = 0; i_h < (hi_h - lo_h); i_h++ ) {
        if ( _nc_none != ope_h[i_h] ) {
          _nc_range(&ran_u, fir_h, las_h, lo_h + i_h, ope_h[i_h], hap_h - 1);
        }
      }
    }

    u3a_free(inn_d);
    u3a_free(liv_d);
    u3a_free(lvo_d);
    u3a_free(lva_d);
    u3a_free(cur_d);
    u3a_free(now_d);
    u3a_free(ope_h);
  }

  //  the parameters, then the other registers by their first point
  //
#define _nc_occ(sot)  (occ_d + ((c3_z)(sot) * hwo_h))
#define _nc_slot_new()  do {                                                    \
    if ( tot_h >= occ_h ) {                                                     \
      c3_h _new = c3_max(2 * occ_h, c3_max(arg_h, 8));                          \
      occ_d = u3a_realloc(occ_d, (c3_z)occ_h * hwo_h * sizeof(c3_d),            \
                                 (c3_z)_new * hwo_h * sizeof(c3_d));            \
      memset(_nc_occ(occ_h), 0, (c3_z)(_new - occ_h) * hwo_h * sizeof(c3_d));   \
      occ_h = _new;                                                             \
    }                                                                           \
    tot_h++;                                                                    \
  } while ( 0 )

  for ( r_h = 0; r_h < arg_h; r_h++ ) {
    _nc_slot_new();
    sot_h[r_h] = r_h;
    _nc_take(&ran_u, fir_h[r_h], _nc_occ(r_h));
  }

  for ( r_h = arg_h; r_h < reg_h; r_h++ ) {
    if ( _nc_none != fir_h[r_h] ) {
      key_d[key_n_h++] = ((c3_d)ran_u.sta_h[fir_h[r_h]] << 32) | r_h;
    }
  }

  _nc_key_sort(key_d, key_n_h);

  for ( i_h = 0; i_h < key_n_h; i_h++ ) {
    c3_h sot = _nc_none, s_h;

    r_h = (c3_h)key_d[i_h];

    if (  (_nc_none != wan_h[r_h])
       && _nc_fits(&ran_u, fir_h[r_h], _nc_occ(wan_h[r_h])) )
    {
      sot = wan_h[r_h];
    }

    for ( s_h = arg_h; (_nc_none == sot) && (s_h < tot_h); s_h++ ) {
      if ( _nc_fits(&ran_u, fir_h[r_h], _nc_occ(s_h)) ) {
        sot = s_h;
      }
    }

    for ( s_h = 0; (_nc_none == sot) && (s_h < arg_h); s_h++ ) {
      if ( _nc_fits(&ran_u, fir_h[r_h], _nc_occ(s_h)) ) {
        sot = s_h;
      }
    }

    if ( _nc_none == sot ) {
      sot = tot_h;
      _nc_slot_new();
    }

    sot_h[r_h] = sot;
    _nc_take(&ran_u, fir_h[r_h], _nc_occ(sot));
  }

#undef _nc_occ
#undef _nc_slot_new

  //  rewrite
  //
  for ( i_h = 0; i_h < opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);

    _nc_srcs(gen_u, op_u, reg_h, {
      c3_h s_h = sot_h[*reg_h];
      //  live ops' registers should've gotten their slots
      u3_assert(_nc_none != s_h);
      *reg_h = s_h;
    });

    if ( _nc_none != op_u->dst_h ) {
      c3_h s_h = sot_h[op_u->dst_h];
      u3_assert(_nc_none != s_h);
      op_u->dst_h = s_h;
    }
  }

  gen_u->tot_h = c3_max(tot_h, arg_h);

  u3a_free(fir_h);
  u3a_free(las_h);
  u3a_free(wan_h);
  u3a_free(won_y);
  u3a_free(sot_h);
  u3a_free(key_d);
  u3a_free(occ_d);
  u3a_free(ran_u.sta_h);
  u3a_free(ran_u.end_h);
  u3a_free(ran_u.nex_h);
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
    case _nc_nop: {
      return 0;
    }

    case _nc_bom: {
      if ( buf_y ) {
        buf_y[0] = BOM;
      }
      return 1;
    }

    case _nc_kil: {
      if ( _nc_zer_t ) {
        return 0;       //  noop when consuming ops zero out the consumed slot
      }
      [[fallthrough]];  //  becomes IMM_0 or IML[0] when consuming does not zero
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
  siz_h = 1 + (2 == wid_y);  //  op byte + WIDE extension

  if ( buf_y ) {
    c3_y kon_y = _nc_fam[fam_y].kon_y ? op_u->kon_y : 0;

    u3_assert( kon_y < (1 << _nc_fam[fam_y].kon_y) );
    if ( 2 == wid_y ) {
      buf_y[0] = WIDE;
      buf_y[1] = _nc_vas_y[fam_y] + kon_y;
    }
    else {
      buf_y[0] = _nc_bas_y[fam_y] + (2 * kon_y) + wid_y;
    }
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

/* _nc_decode(): the family of the op at ip, past its opcode (a byte, or
**               two behind WIDE), with its variant bits and its width.
**               Not for IMM_* or BOM, which are no family's.
*/
static c3_y
_nc_decode(const c3_y* pog, c3_h* ip_h, c3_y* kon_y, c3_y* wid_y)
{
  c3_y        cod_y = pog[(*ip_h)++];
  c3_t        wid_t = (WIDE == cod_y);
  const c3_y* bas_y = wid_t ? _nc_vas_y : _nc_bas_y;
  c3_y        fam_y = 0;

  if ( wid_t ) {
    cod_y = pog[(*ip_h)++];
  }
  while ( ((fam_y + 1) < _nc_bom) && (bas_y[fam_y + 1] <= cod_y) ) {
    fam_y++;
  }
  cod_y -= bas_y[fam_y];
  *wid_y = wid_t ? 2 : (cod_y & 1);
  *kon_y = wid_t ? cod_y : (cod_y >> 1);
  return fam_y;
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

    fprintf(stderr, "  site %u: bell %08x %s%s%s cid %u args",
            i_h, u3r_mug(dir_u->bell),
            ( c3y == dir_u->mon_o ) ? "subject" : "direct",
            dir_u->ham_u ? " jet" : "",
            !dir_u->arm_u ? "" : dir_u->arm_u->pun_t ? " punt-jet" : " array-jet",
            dir_u->cid_h);

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
    c3_h off_h = ip_h;

    if ( (cod_y < IML_B) || (BOM == cod_y) ) {
      ip_h++;
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
      c3_y kon_y, wid_y, fam_y = _nc_decode(pog, &ip_h, &kon_y, &wid_y);

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
        static const c3_c* edg_c[] = { "", "!f", "!j", "!" };
        const c3_c*        kon_c   = _nc_edgy(fam_y)
                                   ? edg_c[(kon_y >> (2 * i_h)) & 3]
                                   : ( ((kon_y >> i_h) & 1) ? "!" : "" );
        fprintf(stderr, " %u%s", _nc_read(pog, &ip_h, wid_y), kon_c);
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

/* _nc_shuffle(): lower a self call to a jump to the entry: the moves that
**                put its arguments into the parameter slots, then the HOP.
**                The arguments are the only registers alive, so every
**                other slot is empty.  An argument already in its
**                parameter's slot stays; the others move, each once its
**                parameter's slot is empty: not an argument's, or moved
**                out of already.  A slot read by several arguments (one
**                register passed twice) is borrowed by all reads but the
**                last, which takes its reference; a slot whose argument
**                stays is borrowed by every other read.  When every
**                argument left is in another's slot (a cycle), the one in
**                the first's slot steps aside into an empty slot above the
**                parameters, a new one if there is none: the cycle unwinds
**                from the emptied slot, and the scratch slot is drained
**                before another cycle can be stuck, so one suffices.
**                Appends to ops_u.
*/
static void
_nc_shuffle(nc_gen* gen_u, nc_dir* dir_u, c3_h ent_h,
            nc_op** ops_u, c3_h* opn_h, c3_h* opc_h)
{
  c3_h   len_h = dir_u->len_h;
  c3_h   tot_h = gen_u->tot_h;
  c3_h*  arg_h = u3a_malloc(c3_max(len_h, 1) * sizeof(c3_h));  //  slot of each
  c3_h*  red_h = u3a_calloc(tot_h + 1, sizeof(c3_h));  //  reads left of a slot
  c3_y*  sta_y = u3a_calloc(tot_h + 1, sizeof(c3_y));  //  its argument stays
  c3_h   pen_h = 0, k_h;
  nc_op* op_u;

  //  arg_h[k_h]: slot that holds a value that needs to be in k_h
  //  sta_y[k_h]: k_h is already in place, it needs to stay
  //  red_h[k_h]: k_h has a pending move from it, can't overwrite if nonzero
  //
  for ( k_h = 0; k_h < len_h; k_h++ ) {
    arg_h[k_h] = gen_u->pol_h[dir_u->sot_h + k_h];

    if ( arg_h[k_h] == k_h ) {
      sta_y[k_h] = 1;
    }
    else {
      red_h[arg_h[k_h]]++;
      pen_h++;
    }
  }

  while ( pen_h ) {
    c3_t did_t = 0;

    for ( k_h = 0; k_h < len_h; k_h++ ) {
      //  first try to move a value whose target slot is free to write
      //
      c3_h s_h = arg_h[k_h];

      if ( (s_h == k_h) || red_h[k_h] ) {
        continue;
      }

      _nc_grow(*ops_u, *opn_h, *opc_h, nc_op);
      op_u = _nc_op_init(&((*ops_u)[(*opn_h)++]), _nc_mov);
      op_u->src_h[0] = s_h;
      op_u->dst_h    = k_h;
      op_u->kon_y    = ( (1 == red_h[s_h]) && !sta_y[s_h] );
      red_h[s_h]--;
      arg_h[k_h] = k_h;
      pen_h--;
      did_t = 1;
    }

    if ( !did_t ) {
      //  couldn't move anything: a cycle
      //  get a scratch slot for a swap, allocating if needed
      c3_h t_h, j_h;

      for ( k_h = 0; arg_h[k_h] == k_h; k_h++ );
      for ( t_h = len_h; (t_h < tot_h) && red_h[t_h]; t_h++ );

      //  a slot out of place has a pending read due to a cycle
      u3_assert( red_h[k_h] );

      if ( t_h == tot_h ) {
        u3_assert( tot_h == gen_u->tot_h );
        tot_h++;
      }

      _nc_grow(*ops_u, *opn_h, *opc_h, nc_op);
      op_u = _nc_op_init(&((*ops_u)[(*opn_h)++]), _nc_mov);
      op_u->src_h[0] = k_h;
      op_u->dst_h    = t_h;
      op_u->kon_y    = 1;

      for ( j_h = 0; j_h < len_h; j_h++ ) {
        if ( arg_h[j_h] == k_h ) {
          arg_h[j_h] = t_h;
        }
      }
      red_h[t_h] = red_h[k_h];
      red_h[k_h] = 0;
    }
  }

  _nc_grow(*ops_u, *opn_h, *opc_h, nc_op);
  op_u = _nc_op_init(&((*ops_u)[(*opn_h)++]), _nc_hop);
  op_u->tar_h = ent_h;

  gen_u->tot_h = tot_h;

  u3a_free(arg_h);
  u3a_free(red_h);
  u3a_free(sta_y);
}

/* _nc_emit(): lay the ops out and assemble the program.  RETAINS ned.
*/
static u3nc_prog*
_nc_emit(nc_gen* gen_u, u3_noun ned, c3_h arg_h)
{
  u3nc_prog* pog_u;
  c3_h       byc_h, i_h, j_h;

  //  a self call that doesn't sweep starts over: a jump to the entry,
  //  after the moves that put its arguments in their parameter slots
  //  (_nc_shuffle()).  A move of a slot onto itself (an argument
  //  coalesced with the block parameter it is passed to) does nothing,
  //  and goes.
  //  NB: this adds a backedge into an otherwise DAG. It's OK because the passes
  //  that depended on acyclity of the CFG happened before this point.
  //
  {
    c3_h   ent_h = _nc_blk(gen_u, 0);
    c3_h   cap_h = gen_u->opn_h, new_h = 0;
    nc_op* new_u = u3a_malloc(c3_max(cap_h, 1) * sizeof(nc_op));

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);
      c3_h    fir_h = new_h;

      for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
        nc_op* op_u = &(gen_u->ops_u[blk_u->fir_h + j_h]);

        if ( (_nc_jms == op_u->fam_y) && !op_u->kon_y ) {
          nc_dir* dir_u = &(gen_u->dir_u[op_u->imm_h]);

          u3_assert( dir_u->len_h == arg_h );
          _nc_shuffle(gen_u, dir_u, ent_h, &new_u, &new_h, &cap_h);
        }
        else if (  (_nc_mov == op_u->fam_y)
                && (op_u->src_h[0] == op_u->dst_h) )
        {
          u3_assert( op_u->kon_y );
        }
        else {
          _nc_grow(new_u, new_h, cap_h, nc_op);
          new_u[new_h++] = *op_u;
        }
      }

      blk_u->fir_h = fir_h;
      blk_u->len_h = new_h - fir_h;
    }

    u3a_free(gen_u->ops_u);
    gen_u->ops_u = new_u;
    gen_u->opc_h = cap_h;
    gen_u->opn_h = new_h;
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
  _nc_sink(&gen_u);
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

    if ( (c3n == dir_u->mon_o) && dir_u->arm_u && !dir_u->arm_u->pun_t ) {
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
_nc_callee(u3nc_prog* pog_u, u3nc_dire* dir_u)
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
  else {
    u3h_put(u3R->ska.def_p, _nc_of(pog_u), u3_nul);
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

#if defined(U3NC_CHECK) || defined(C3DBG)
/* _nc_doomed(): whether the code from ip runs into BOM before any other
**               end of a block: a branch, a hop, or the end of the
**               activation.  A block that only crashes gets no drops for
**               the registers that died on the edge into it (_nc_lives()),
**               so its slots may still hold references when its ops write
**               them; the checking interpreter allows that there, since
**               the bail abandons the road.  Only the ops of the crash
**               block itself, and of blocks that fall through into it,
**               are doomed this way: a block that reaches it by a jump
**               has had its drops.
*/
static c3_t
_nc_doomed(const u3nc_prog* pog_u, const c3_y* ip)
{
  const c3_y* pog  = pog_u->byc_u.ops_y;
  c3_h        ip_h = ip - pog;

  while ( ip_h < pog_u->byc_u.len_h ) {
    c3_y cod_y = pog[ip_h];

    if ( BOM == cod_y ) {
      return 1;
    }
    else if ( cod_y < IML_B ) {   //  IMM_0, IMM_1, IMM_B, IMM_S
      ip_h += ( IMM_S == cod_y ) ? 4 : ( IMM_B == cod_y ) ? 3 : 2;
    }
    else {
      c3_y kon_y, wid_y, n_y, fam_y = _nc_decode(pog, &ip_h, &kon_y, &wid_y);

      if ( _nc_fam[fam_y].tar_y || _nc_fam[fam_y].swe_y ) {
        return 0;
      }
      n_y = _nc_fam[fam_y].imm_y + _nc_fam[fam_y].src_y
          + _nc_fam[fam_y].dst_y;
      while ( n_y-- ) {
        _nc_read(pog, &ip_h, wid_y);
      }
    }
  }

  return 0;
}
#endif

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
  u3m_quac** qua_u = c3_malloc(sizeof(*qua_u) * 7);
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
  
  qua_u[5] = c3_calloc(sizeof(*qua_u[5]));
  qua_u[5]->nam_c = strdup("deffered linking table");
  qua_u[5]->siz_w = u3h_mark_tot(u3R->ska.def_p) * sizeof(c3_w);

  qua_u[6] = NULL;

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
  u3h_free(u3R->ska.def_p);
}

/* u3nc_reclaim(): free the programs to reclaim memory.
*/
void
u3nc_reclaim(void)
{
  u3nc_free();
  u3R->ska.dir_p = u3h_new();
  u3R->ska.ent_p = u3h_new();
  u3R->ska.def_p = u3h_new();
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
  u3h_relocate(&(u3R->ska.def_p));
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

static c3_o
_nc_link_deferred_cb(u3_noun kev, void* tox_v)
{
  u3nc_prog* pog_u = _nc_to(u3h(kev));
  if ( !_nc_here(pog_u) ) return c3n;
  _nc_link(pog_u);
  return c3y;
}

/* u3nc_link_deferred(): relink programs from the deferred worklist
*/
void
u3nc_link_deferred(u3p(u3h_root) def_p)
{
  u3h_prune_with(def_p, _nc_link_deferred_cb, NULL);
}