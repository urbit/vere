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

#ifdef U3NC_STAT
#include <time.h>
#endif

/*  Bytecode.
**
**  Every opcode with immediate arguments comes in three widths, for all
**  of its immediates at once: _B, one byte each; _S, two bytes each,
**  little-endian; _V, a length byte followed by that many bytes.  The
**  _B and _S widths are opcodes; the _V widths, which only a program
**  with over 65535 slots, literals, sites or bytes of code needs, are a
**  second table of them behind the prefix WIDE, for a dispatch more.
**  Slots, literal and call site indices, and jump targets (absolute byte
**  offsets) are immediates, and so is the atom of IMM and EQI, which
**  carry it rather than a literal index.  A call site holds its argument
**  slots and its destination slot d, so the call opcodes carry only the
**  site index.
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
**  sweeps, for the checks, or for the drops at the start of a block of
**  the registers that die on some edges into it only (_nc_zer_t).
**
**    IMM   n d      n -> d, n a direct atom
**    IML   i d      literal i -> d
**    MOV   s d      s -> d
**    INC   s d      +(s) -> d
**    DEC   s d      (dec s) -> d, by the jet
**    ADD   a b d    (add a b) -> d, by the jet
**    CON   h t d    [h t] -> d
**    HED   s d      -.s -> d, s is guaranteed to be a cell by Hoon IR emitter
**    TAL   s d      +.s -> d, s is guaranteed to be a cell by Hoon IR emitter
**    HER   s d      HED consuming s, paired with a COR: pushes the cell of
**                   s on the stack for it when the slot's reference was
**                   the only one, the cell giving d its reference to the
**                   head and losing the tail, else 0 (_nc_reuse())
**    TAR   s d      TAL the same way, for the tail
**    COR   h t d    CON into the cell a HER or TAR pushed, popped, if any
**    CEL   p        crash unless p is a cell
**    LOB   p        crash unless p is a loobean
**    EQU   l r      =(l r), for the side effect
**    HSP   i        static hint prologue, literal i is the hint
**    HSE   i        static hint epilogue
**    HDP   i p      dynamic hint prologue, clue in p
**    HDE   i p      dynamic hint epilogue
**    HLP   i        literal hint prologue: push literal i, the [tag clue]
**                   of a trace hint whose clue is a constant (_nc_fold())
**    HMP   i        deferred %mean prologue: push [%meaf bell args] of
**                   site i, the arm of the trap and the arguments it takes
**    HME            deferred %mean epilogue
**    HCP   i        constant %mean prologue: push [%meac literal i], the
**                   known product of the arm
**    HCE            constant %mean epilogue
**    SPY   e p d    .^(e p) -> d
**    NOK   u f d    .*(u f) -> d, through the SKA core
**    CAL   i        call site i with its arguments -> d
**    CAF   i        CAL, by the total array jet of the site
**    CAX   i        CAF, by the transferring driver of the jet, which
**                   takes the references of the arguments the site
**                   consumes and retains the others
**    CAC   i        CAF under the flags of the jet arm, which isn't trusted:
**                   the driver tested against the unary program of the
**                   callee on the core rebuilt from the arguments; the
**                   program alone if the arm is off, the driver alone if
**                   it is trusted (for sites compiled before a change)
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
**    JMX   i        CAX in tail position
**    JSP   i        CAP in tail position
**    DON   s        return s, consumed
**    DRO   s        drop s: lose its reference, 0 -> s
**    JMC   i        CAC in tail position
**    BOM            crash
**    WIDE  o        the _V width of an op, o its index in their table
**
**  DON, JMP, JMS, JMF, JMX, JSP and JMC end the activation, and come in a
**  sweeping variant (_1) that first drops every slot it doesn't take: it
**  stands for the drops at the start of a block that does nothing else.
*/

/*  Families of the IR ops: X(fam, OP, src, dst, imm, tar, sot, kon, swe):
**    fam - the family,
**    OP  - its opcode name,
**    src - the number of source slots in the op,
**    dst - whether it has a destination slot
**    imm - an immediate: a literal or call site index, or the atom itself
**          (IMM, EQI)
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
**  Other special families, nop and kil have no opcodes of their own: nop
**  encodes as nothing, and kil as IMM 0, or as nothing where consuming zeroes
**  the slot (_nc_zer_t).
**  kil defines a register as 0: a parameter of a block that a jump to it leaves
**  undefined, so that the register is not live on the way to that jump; and 0
**  drops as nothing where the register dies on a path that never defined it
**  (_nc_jump()).
*/
#define FAMILIES(X)                                                             \
  /*fam, OP,  src, dst, imm, tar, sot, kon, swe*/                               \
  X(imm, IMM, 0,   1,   1,   0,   0,   0,   0)                                  \
  X(iml, IML, 0,   1,   1,   0,   0,   0,   0)                                  \
  X(mov, MOV, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(inc, INC, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(dec, DEC, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(add, ADD, 2,   1,   0,   0,   0,   2,   0)                                  \
  X(con, CON, 2,   1,   0,   0,   0,   2,   0)                                  \
  X(hed, HED, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(tal, TAL, 1,   1,   0,   0,   0,   1,   0)                                  \
  X(her, HER, 1,   1,   0,   0,   0,   0,   0)                                  \
  X(tar, TAR, 1,   1,   0,   0,   0,   0,   0)                                  \
  X(cor, COR, 2,   1,   0,   0,   0,   2,   0)                                  \
  X(cel, CEL, 1,   0,   0,   0,   0,   1,   0)                                  \
  X(lob, LOB, 1,   0,   0,   0,   0,   1,   0)                                  \
  X(equ, EQU, 2,   0,   0,   0,   0,   2,   0)                                  \
  X(hsp, HSP, 0,   0,   1,   0,   0,   0,   0)                                  \
  X(hse, HSE, 0,   0,   1,   0,   0,   0,   0)                                  \
  X(hdp, HDP, 1,   0,   1,   0,   0,   1,   0)                                  \
  X(hde, HDE, 1,   0,   1,   0,   0,   1,   0)                                  \
  X(hlp, HLP, 0,   0,   1,   0,   0,   0,   0)                                  \
  X(hmp, HMP, 0,   0,   1,   0,   1,   0,   0)                                  \
  X(hcp, HCP, 0,   0,   1,   0,   0,   0,   0)                                  \
  X(spy, SPY, 2,   1,   0,   0,   0,   2,   0)                                  \
  X(nok, NOK, 2,   1,   0,   0,   0,   2,   0)                                  \
  X(cal, CAL, 0,   0,   1,   0,   1,   0,   0)                                  \
  X(caf, CAF, 0,   0,   1,   0,   1,   0,   0)                                  \
  X(cax, CAX, 0,   0,   1,   0,   1,   0,   0)                                  \
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
  X(jmx, JMX, 0,   0,   1,   0,   1,   1,   1)                                  \
  X(jsp, JSP, 0,   0,   1,   0,   1,   1,   1)                                  \
  X(don, DON, 1,   0,   0,   0,   0,   1,   1)                                  \
  X(dro, DRO, 1,   0,   0,   0,   0,   0,   0)                                  \
  X(cac, CAC, 0,   0,   1,   0,   1,   0,   0)                                  \
  X(jmc, JMC, 0,   0,   1,   0,   1,   1,   1)

/*  Families of ops without operands, X(fam, OP): one opcode each, with
**  nothing to widen or consume.  They follow the others in the opcode
**  space and in the family order the decoder searches.
*/
#define NULLARIES(X)                                                            \
  X(hme, HME)                                                                   \
  X(hce, HCE)

#define _nc_fam_enum(fam, OP, src, dst, imm, tar, sot, kon, swe)  _nc_##fam,
#define _nc_nul_enum(fam, OP)  _nc_##fam,
enum {
  FAMILIES(_nc_fam_enum) NULLARIES(_nc_nul_enum)
  _nc_bom, _nc_nop, _nc_kil
};
#undef _nc_fam_enum
#undef _nc_nul_enum

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

#define _nc_nul_ops(fam, OP)  Y(OP)

#define OPCODES  FAMILIES(_nc_fam_ops) NULLARIES(_nc_nul_ops) Y(BOM) Y(WIDE)
#define WIDES    FAMILIES(_nc_fam_wides)

#define Y(op) op,
enum { OPCODES LAST };
enum { WIDES LAST_V };
static_assert((LAST <= 256) && (LAST_V <= 256));
#undef Y

/* _nc_fam: operands of a family, materializes X table and adds special-cased
**          ops.
*/
#define _nc_fam_row(fam, OP, src, dst, imm, tar, sot, kon, swe)                 \
  [_nc_##fam] = { src, dst, imm, tar, sot, kon, swe },
#define _nc_nul_row(fam, OP)                                                    \
  [_nc_##fam] = { 0,   0,    0,  0,   0,   0,   0 },
static const struct {
  c3_y src_y, dst_y, imm_y, tar_y, sot_y, kon_y, swe_y;
} _nc_fam[] = {
  FAMILIES(_nc_fam_row)
  NULLARIES(_nc_nul_row)
  /*fam,        src, dst, imm, tar, sot, kon, swe*/
  [_nc_bom] = { 0,   0,    0,  0,   0,   0,   0 },
  [_nc_nop] = { 0,   0,    0,  0,   0,   0,   0 },
  [_nc_kil] = { 0,   1,    0,  0,   0,   0,   0 },
};
#undef _nc_fam_row
#undef _nc_nul_row

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
//  a nullary has no _V opcode: a base above every _V opcode keeps it out
//  of the decoder's search of WIDES
//
#define _nc_nul_bas(fam, OP)  [_nc_##fam] = OP,
#define _nc_nul_vas(fam, OP)  [_nc_##fam] = 0xff,
static const c3_y _nc_bas_y[] = { FAMILIES(_nc_fam_bas) NULLARIES(_nc_nul_bas) };
static const c3_y _nc_vas_y[] = { FAMILIES(_nc_fam_vas) NULLARIES(_nc_nul_vas) };
static_assert(LAST_V <= 0xff);
#undef _nc_fam_bas
#undef _nc_fam_vas
#undef _nc_nul_bas
#undef _nc_nul_vas

/* _nc_fam_c: names of the families, and of BOM, for _nc_print().
*/
#define _nc_fam_nam(fam, OP, src, dst, imm, tar, sot, kon, swe)                 \
  [_nc_##fam] = #OP,
#define _nc_nul_nam(fam, OP)  [_nc_##fam] = #OP,

static const c3_c* _nc_fam_c[] = {
  FAMILIES(_nc_fam_nam) NULLARIES(_nc_nul_nam) [_nc_bom] = "BOM"
};
#undef _nc_fam_nam
#undef _nc_nul_nam

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
  c3_y  ali_y;    //  a borrowing HED/TAL whose product could be an alias
                  //  of a part of its source, for the statistics
  c3_h  src_h[2]; //  source registers
  c3_h  dst_h;    //  destination register for non-call ops
  c3_h  imm_h;    //  index immediate, or the atom of an IMM
  c3_h  tar_h;    //  target block
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
  u3j_harm*       ham_u;  //  the arm in the hot state, for its flags
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
  c3_o    ice_o;  //  compiled trusting the jet arm (u3nc_dire)
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
  u3_noun       jet;      //  (list ring) of the jets ops stand in for
  u3p(u3h_root) lit_p;    //  literal -> index
  c3_h          lit_h;
  u3_noun*      lin;      //  index -> literal, borrowed from lit_p
  c3_h          lic_h;    //    and its capacity
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
#  define _nc_stat_add(fel, n)  (u3nc_Stat.fel += (n))

/*  models of cell reuse beyond the pairing, for the statistics: a spare
**  cell kept by the last unique free of an unpaired consuming HED/TAL and
**  taken by the next CON, globally (_nc_spg_t) or within an activation
**  (_nc_spa_t, saved in the frame across calls).
*/
static c3_t _nc_spg_t, _nc_spa_t;

#  define _nc_stat_uni(uni_t)  do {                                             \
    if ( uni_t ) {                                                              \
      u3nc_Stat.uni_d++;                                                        \
      if ( _nc_spg_t ) { u3nc_Stat.spw_d++; }                                   \
      _nc_spg_t = 1;                                                            \
      _nc_spa_t = 1;                                                            \
    }                                                                           \
  } while ( 0 )
#  define _nc_stat_con()  do {                                                  \
    if ( _nc_spg_t ) { u3nc_Stat.spg_d++; _nc_spg_t = 0; }                      \
    if ( _nc_spa_t ) { u3nc_Stat.spa_d++; _nc_spa_t = 0; }                      \
  } while ( 0 )
#  define _nc_stat_move(gop_u, kon_y, len_h, tai_t)  do {                       \
    for ( c3_h _j = 0; _j < (len_h); _j++ ) {                                   \
      c3_t _b = (_j < 64) && (((gop_u)->bor_d >> _j) & 1);                      \
      u3nc_Stat.kal_d++;                                                        \
      if ( !(kon_y)[_j] ) {                                                     \
        u3nc_Stat.gan_d++;                                                      \
        if ( _b ) { u3nc_Stat.bor_d++; }                                        \
      }                                                                         \
      else if ( _b ) {                                                          \
        if ( tai_t ) { u3nc_Stat.bot_d++; } else { u3nc_Stat.bok_d++; }         \
      }                                                                         \
    }                                                                           \
  } while ( 0 )
#  define _nc_stat_ali(pog_u, ip, pro, K)  do {                                 \
    if ( !(K) ) {                                                               \
      c3_t _ind = (c3n == u3a_is_cat(pro));                                     \
      if ( _ind ) { u3nc_Stat.dbi_d++; }                                        \
      if ( (pog_u)->ali_y[(ip) - (pog_u)->byc_u.ops_y] ) {                      \
        u3nc_Stat.ali_d++;                                                      \
        if ( _ind ) { u3nc_Stat.alx_d++; }                                      \
      }                                                                         \
    }                                                                           \
  } while ( 0 )
#  define _nc_stat_spa_set(v)  (_nc_spa_t = (v))
#  define _nc_stat_spa_get()   (_nc_spa_t)
#  define _nc_stat_frame(f)    ((f)->spa_y = _nc_spa_t)
#  define _nc_stat_unframe(f)  (_nc_spa_t = (f)->spa_y)
#else
#  define _nc_stat(fel)  ((void)0)
#  define _nc_stat_if(con, fel)  ((void)0)
#  define _nc_stat_add(fel, n)  ((void)0)
#  define _nc_stat_uni(uni_t)  ((void)0)
#  define _nc_stat_con()  ((void)0)
#  define _nc_stat_move(gop_u, kon_y, len_h, tai_t)  ((void)0)
#  define _nc_stat_ali(pog_u, ip, pro, K)  ((void)0)
#  define _nc_stat_spa_set(v)  ((void)0)
#  define _nc_stat_spa_get()   0
#  define _nc_stat_frame(f)    ((void)0)
#  define _nc_stat_unframe(f)  ((void)0)
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
**            holds a reference iff its register is live, and without
**            zeroing a drop never meets an empty slot (_nc_lives()), so
**            only a sweep, and the checks of U3NC_CHECK and C3DBG, read
**            empty slots.  With zeroing, _nc_lives() drops a register
**            that dies on some edges into a block only at the start of
**            the block, where its slot is zero on the other edges,
**            rather than in a block of drops made for each edge.
*/
#if defined(U3NC_SWEEP) || defined(U3NC_CHECK) || defined(C3DBG) \
    || defined(U3NC_ZERO)
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
    _nc_grow(gen_u->lin, got, gen_u->lic_h, u3_noun);
    gen_u->lin[got] = som;
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
  op_u->ali_y    = 0;
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
**             analysis's); of a total array driver on the arguments,
**             trusted (CAF) or checked under the flags of its arm (CAC);
**             or by subject, of a u3w driver that may punt, whose one
**             argument is the whole subject.  Bails if the arguments don't
**             fit the driver: the %jets table of the SKA core and u3u_Harm
**             then disagree.
**
**             The flags of the arm are compile-time: an arm that is off
**             has no driver here (u3j_ring()), and the SKA core isn't told
**             of it, so a call of it is plain; one that isn't trusted
**             compiles to a checked call.  A change of the flags drops the
**             programs compiled under the old ones (u3d_hot(), u3nc_stale()).
*/
typedef enum {
  _nc_kind_plain,
  _nc_kind_total,
  _nc_kind_check,
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

  if ( jet_u->arm_u->pun_t ) {
    return _nc_kind_punt;
  }

  return ( c3y == jet_u->ham_u->ice ) ? _nc_kind_total : _nc_kind_check;
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
  dir_u->ice_o = ( jet_u.ham_u ) ? jet_u.ham_u->ice : c3y;
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

/* _nc_opjet(): record the ring of a jet an op stands in for, once.
**              RETAINS.
*/
static void
_nc_opjet(nc_gen* gen_u, u3_noun ring)
{
  u3_noun i;

  for ( i = gen_u->jet; u3_nul != i; i = u3t(i) ) {
    if ( c3y == u3r_sing(u3h(i), ring) ) {
      return;
    }
  }

  gen_u->jet = u3nc(u3k(ring), gen_u->jet);
}

/* _nc_call(): translate a call with arguments through a ring, in tail
**             position if tal_t, as an op for its kind (_nc_kind()): a
**             plain call; a call of the total array jet, trusted (or the
**             op standing in for it, and transferring, CAX/JMX, if the jet
**             has a driver that takes the consumed arguments) or checked
**             (CAC/JMC); or a call by subject, of its one argument.
**             RETAINS.
**
**             The ring of an op standing in for a jet goes in the
**             program's list of them: the record of the jets it trusts,
**             for u3nc_stale().
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
        _nc_opjet(gen_u, ring);
        break;
      }
      op_u = _nc_op(gen_u, jet_u.arm_u->own_f
                           ? ( tal_t ? _nc_jmx : _nc_cax )
                           : ( tal_t ? _nc_jmf : _nc_caf ));
      op_u->imm_h = _nc_dir(gen_u, bell, jet_u, u3_none, arg, c3n);
    } break;

    case _nc_kind_check: {
      op_u = _nc_op(gen_u, tal_t ? _nc_jmc : _nc_cac);
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
      if ( c3y == u3a_is_cat(a) ) {
        op_u = _nc_op(gen_u, _nc_imm);
        op_u->imm_h = a;
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

    //  the hinted formula that the hint ops carry is not kept
    //
    case c3__hsp:
    case c3__hse: {
      u3x_cell(arg, &a, &b);
      op_u = _nc_op(gen_u, ( c3__hsp == tag ) ? _nc_hsp : _nc_hse);
      op_u->imm_h = _nc_lit(gen_u, a);
    } break;

    case c3__hdp:
    case c3__hde: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, ( c3__hdp == tag ) ? _nc_hdp : _nc_hde);
      op_u->imm_h    = _nc_lit(gen_u, a);
      op_u->src_h[0] = _nc_reg(gen_u, b);
    } break;

    case c3__hmp: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_hmp);
      op_u->imm_h = _nc_dir(gen_u, a, _nc_ring(u3_nul), u3_none, b, c3n);
    } break;

    case c3__hme:
    case c3__hce: {
      op_u = _nc_op(gen_u, ( c3__hme == tag ) ? _nc_hme : _nc_hce);
    } break;

    case c3__hcp: {
      u3x_cell(arg, &a, &b);
      op_u = _nc_op(gen_u, _nc_hcp);
      op_u->imm_h = _nc_lit(gen_u, a);
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
**             Likewise a trace hint (%hunk, %lose, %mean, %spot) whose
**             clue is an immediate, as every %spot of a !: is, pushes a
**             constant [tag clue]: it becomes a literal and the prologue
**             an HLP, which allocates nothing.  The epilogue still reads
**             the clue's register, to drop it.
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

  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op*  op_u = &(gen_u->ops_u[i_h]);
    nc_op*  def_u;
    u3_noun tag, clu, lit;

    if ( _nc_hdp != op_u->fam_y ) {
      continue;
    }

    tag = gen_u->lin[op_u->imm_h];
    if (  (c3__hunk != tag) && (c3__lose != tag)
       && (c3__mean != tag) && (c3__spot != tag) )
    {
      continue;
    }

    if ( _nc_none == def_h[op_u->src_h[0]] ) {
      continue;
    }

    def_u = &(gen_u->ops_u[def_h[op_u->src_h[0]]]);
    clu   = ( _nc_imm == def_u->fam_y ) ? def_u->imm_h
                                        : gen_u->lin[def_u->imm_h];
    lit   = u3nc(tag, u3k(clu));

    op_u->fam_y    = _nc_hlp;
    op_u->imm_h    = _nc_lit(gen_u, lit);
    op_u->src_h[0] = _nc_none;
    u3z(lit);
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

#define _nc_fam_sot(fam, OP, src, dst, imm, tar, sot, kon, swe)                 \
  ((sot) && (src)) |
//  XX check _nc_fam appending too
static_assert(!(FAMILIES(_nc_fam_sot) 0),
  "sot families have no own sources, see _nc_live_in()");
#undef _nc_fam_sot

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

      if ( _nc_dro != op_u->fam_y ) {
        _nc_srcs(gen_u, op_u, reg_h, {
          r_h = *reg_h;
          if (  (r_h >= lo_h) && (r_h < hi_h)
             && !_nc_bit_get(liv_d, r_h - lo_h) )
          {
            if ( mar_t ) {
              if ( _nc_fam[op_u->fam_y].sot_y ) {
                gen_u->pok_y[reg_h - gen_u->pol_h] = 1;
              }
              else if ( !_nc_fam[op_u->fam_y].swe_y ) {
                k_h = reg_h - op_u->src_h;
                op_u->kon_y |= _nc_edgy(op_u->fam_y)
                             ? (3 << (2 * k_h))
                             : (1 << k_h);
              }
            }
            _nc_bit_set(liv_d, r_h - lo_h);
          }
        });
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

#ifdef U3NC_STAT
/* _nc_ls: scratch of _nc_lives_stat(), for qsort.
*/
static c3_h        _nc_ls_wor_h;
static const c3_d* _nc_ls_set_d;

static int
_nc_ls_cmp_set(const void* a_v, const void* b_v)
{
  c3_h a_h = *(const c3_h*)a_v, b_h = *(const c3_h*)b_v;
  return memcmp(_nc_ls_set_d + (c3_d)a_h * _nc_ls_wor_h,
                _nc_ls_set_d + (c3_d)b_h * _nc_ls_wor_h,
                _nc_ls_wor_h * sizeof(c3_d));
}

static c3_h
_nc_ls_pop(const c3_d* set_d, c3_h wor_h)
{
  c3_h pop_h = 0;
  for ( c3_h w_h = 0; w_h < wor_h; w_h++ ) pop_h += __builtin_popcountll(set_d[w_h]);
  return pop_h;
}

static int
_nc_ls_cmp_pop(const void* a_v, const void* b_v)
{
  c3_h a_h = _nc_ls_pop(_nc_ls_set_d + (c3_d)(*(const c3_h*)a_v) * _nc_ls_wor_h, _nc_ls_wor_h);
  c3_h b_h = _nc_ls_pop(_nc_ls_set_d + (c3_d)(*(const c3_h*)b_v) * _nc_ls_wor_h, _nc_ls_wor_h);
  return ( a_h > b_h ) - ( a_h < b_h );
}

/* _nc_ls_chain(): drops of num_h distinct sets (indices idx_h into set_d),
**                 each falling through into the biggest smaller one it
**                 contains (sorted ascending by size first).  Sorts idx_h.
*/
static c3_d
_nc_ls_chain(const c3_d* set_d, c3_h wor_h, c3_h* idx_h, c3_h num_h)
{
  c3_d tot_d = 0;

  _nc_ls_wor_h = wor_h; _nc_ls_set_d = set_d;
  qsort(idx_h, num_h, sizeof(c3_h), _nc_ls_cmp_pop);

  for ( c3_h i_h = 0; i_h < num_h; i_h++ ) {
    const c3_d* s_d = set_d + (c3_d)idx_h[i_h] * wor_h;
    c3_h s_h = _nc_ls_pop(s_d, wor_h), best_h = 0;

    for ( c3_h j_h = 0; j_h < i_h; j_h++ ) {
      const c3_d* t_d = set_d + (c3_d)idx_h[j_h] * wor_h;
      c3_t sub_t = 1;
      for ( c3_h w_h = 0; w_h < wor_h; w_h++ ) {
        if ( t_d[w_h] & ~s_d[w_h] ) { sub_t = 0; break; }
      }
      if ( sub_t ) {
        c3_h t_h = _nc_ls_pop(t_d, wor_h);
        if ( t_h > best_h ) best_h = t_h;
      }
    }
    tot_d += s_h - best_h;
  }
  return tot_d;
}

/* _nc_ls_dedup(): sort the indices of num_h sets and keep one of each.
*/
static c3_h
_nc_ls_dedup(const c3_d* set_d, c3_h wor_h, c3_h* idx_h, c3_h num_h)
{
  c3_h dis_h = 0;

  _nc_ls_wor_h = wor_h; _nc_ls_set_d = set_d;
  qsort(idx_h, num_h, sizeof(c3_h), _nc_ls_cmp_set);
  for ( c3_h i_h = 0; i_h < num_h; i_h++ ) {
    if ( !dis_h || _nc_ls_cmp_set(&idx_h[dis_h - 1], &idx_h[i_h]) ) {
      idx_h[dis_h++] = idx_h[i_h];
    }
  }
  return dis_h;
}

/* _nc_lives_stat(): for each block with many blocks of drops on the
**   edges into it, how many drops two release-scheme alternatives would
**   leave: sharing the edge blocks (identical sets, then falling through
**   into a contained one), and dropping at the block start the registers
**   that are undefined, rather than consumed, on the edges in where they
**   don't die (zeroed at the entry instead).  Diagnostics only.
*/
static void
_nc_lives_stat(nc_gen* gen_u, const c3_d* edr_d, c3_h edr_h, c3_h reg_h,
               c3_h arg_h, const c3_h* pro_h, const c3_h* pre_h)
{
  c3_h  blk_h = gen_u->blk_h;
  c3_h  wor_h = (reg_h + 63) >> 6;
  c3_h* cnt_h = c3_calloc((c3_max(blk_h, 1)) * sizeof(c3_h));
  c3_h* def_h = NULL;
  c3_h  i_h, j_h, b_h;

  #define _NC_TARGET(edg) \
    ( gen_u->blk_u[gen_u->lay_h[(edg) >> 1]].kid_h[((edg) & 1) ? 1 : 0] )

  for ( i_h = 0; i_h < edr_h; i_h = j_h ) {
    c3_h edg_h = edr_d[i_h] >> 32;
    for ( j_h = i_h; (j_h < edr_h) && ((edr_d[j_h] >> 32) == edg_h); j_h++ );
    cnt_h[_NC_TARGET(edg_h)]++;
  }

  for ( b_h = 0; b_h < blk_h; b_h++ ) {
    c3_h  num_h = cnt_h[b_h], k_h = 0, dis_h, us_h = 0, uu_h = 0;
    c3_d* set_d;
    c3_h* edg_h;
    c3_h* idx_h;
    c3_d* uni_d;
    c3_d* saf_d;
    c3_d* res_d;
    c3_d  tot_d = 0, dis_d = 0, cha_d, res_tot_d = 0, res_cha_d;
    c3_h  pre_n = pro_h[b_h + 1] - pro_h[b_h];

    if ( num_h < 32 ) continue;

    set_d = c3_calloc(((c3_z)num_h * wor_h) * sizeof(c3_d));
    res_d = c3_calloc(((c3_z)num_h * wor_h) * sizeof(c3_d));
    edg_h = c3_malloc(num_h * sizeof(c3_h));
    idx_h = c3_malloc(num_h * sizeof(c3_h));
    uni_d = c3_calloc((wor_h) * sizeof(c3_d));
    saf_d = c3_calloc((wor_h) * sizeof(c3_d));

    for ( i_h = 0; i_h < edr_h; i_h = j_h ) {
      c3_h e_h = edr_d[i_h] >> 32;
      for ( j_h = i_h; (j_h < edr_h) && ((edr_d[j_h] >> 32) == e_h); j_h++ );
      if ( _NC_TARGET(e_h) != b_h ) continue;
      edg_h[k_h] = e_h;
      for ( c3_h r_i = i_h; r_i < j_h; r_i++ ) {
        c3_h r_h = (c3_h)edr_d[r_i];
        set_d[(c3_d)k_h * wor_h + (r_h >> 6)] |= 1ULL << (r_h & 63);
        uni_d[r_h >> 6] |= 1ULL << (r_h & 63);
      }
      tot_d += j_h - i_h;
      k_h++;
    }

    for ( i_h = 0; i_h < num_h; i_h++ ) idx_h[i_h] = i_h;
    dis_h = _nc_ls_dedup(set_d, wor_h, idx_h, num_h);
    for ( i_h = 0; i_h < dis_h; i_h++ ) {
      dis_d += _nc_ls_pop(set_d + (c3_d)idx_h[i_h] * wor_h, wor_h);
    }
    cha_d = _nc_ls_chain(set_d, wor_h, idx_h, dis_h);

    //  defining blocks, once per program
    //
    if ( !def_h ) {
      def_h = c3_malloc(c3_max(reg_h, 1) * sizeof(c3_h));
      for ( i_h = 0; i_h < reg_h; i_h++ ) {
        def_h[i_h] = ( i_h < arg_h ) ? gen_u->lay_h[0] : _nc_none;
      }
      for ( c3_h x_h = 0; x_h < blk_h; x_h++ ) {
        nc_blk* blk_u = &(gen_u->blk_u[x_h]);
        if ( _nc_none == blk_u->lay_h ) continue;
        if ( u3_none != blk_u->blob ) {
          for ( u3_noun par = u3h(blk_u->blob); u3_nul != par; par = u3t(par) ) {
            c3_h r_h = _nc_cat(u3h(par));
            if ( r_h < reg_h ) def_h[r_h] = x_h;
          }
        }
        for ( c3_h o_h = 0; o_h < blk_u->len_h; o_h++ ) {
          c3_h d_h = gen_u->ops_u[blk_u->fir_h + o_h].dst_h;
          if ( (_nc_none != d_h) && (d_h < reg_h) ) def_h[d_h] = x_h;
        }
      }
    }

    //  a register of the union is safe if on every edge in where it
    //  doesn't die its definition can't have run: its slot there is
    //  undefined (zeroed at the entry), never consumed
    //
    {
      c3_y* rea_y = c3_malloc(blk_h);
      c3_h* stk_h = c3_malloc(c3_max(blk_h, 1) * sizeof(c3_h));

      for ( c3_h r_h = 0; r_h < reg_h; r_h++ ) {
        c3_t ok_t = 1;
        c3_h top_h = 0;

        if ( !(uni_d[r_h >> 6] & (1ULL << (r_h & 63))) ) continue;
        uu_h++;
        if ( _nc_none == def_h[r_h] ) continue;

        memset(rea_y, 0, blk_h);
        stk_h[top_h++] = def_h[r_h];
        rea_y[def_h[r_h]] = 1;
        while ( top_h ) {
          nc_blk* x_u = &(gen_u->blk_u[stk_h[--top_h]]);
          for ( c3_y q_y = 0; q_y < x_u->kin_y; q_y++ ) {
            c3_h q_h = x_u->kid_h[q_y];
            if ( (_nc_none != q_h) && !rea_y[q_h] ) { rea_y[q_h] = 1; stk_h[top_h++] = q_h; }
          }
        }

        for ( j_h = pro_h[b_h]; ok_t && (j_h < pro_h[b_h + 1]); j_h++ ) {
          c3_h p_h = pre_h[j_h];
          c3_t die_t = 0;
          if ( gen_u->blk_u[p_h].kin_y > 1 ) {
            c3_h e_h = (2 * gen_u->blk_u[p_h].lay_h) + (_nc_edge_y(gen_u, p_h, b_h) & 1);
            for ( c3_h n_h = 0; n_h < num_h; n_h++ ) {
              if ( (edg_h[n_h] == e_h)
                && (set_d[(c3_d)n_h * wor_h + (r_h >> 6)] & (1ULL << (r_h & 63))) )
              {
                die_t = 1; break;
              }
            }
          }
          if ( !die_t && rea_y[p_h] ) ok_t = 0;
        }
        if ( ok_t ) {
          saf_d[r_h >> 6] |= 1ULL << (r_h & 63);
          us_h++;
        }
      }
      c3_free(rea_y);
      c3_free(stk_h);
    }

    for ( i_h = 0; i_h < num_h; i_h++ ) {
      for ( c3_h w_h = 0; w_h < wor_h; w_h++ ) {
        res_d[(c3_d)i_h * wor_h + w_h] = set_d[(c3_d)i_h * wor_h + w_h] & ~saf_d[w_h];
      }
      res_tot_d += _nc_ls_pop(res_d + (c3_d)i_h * wor_h, wor_h);
      idx_h[i_h] = i_h;
    }
    {
      c3_h rd_h = _nc_ls_dedup(res_d, wor_h, idx_h, num_h);
      res_cha_d = _nc_ls_chain(res_d, wor_h, idx_h, rd_h);
    }

    fprintf(stderr, "u3nc edge drops into block %u: %u preds, %u edge blocks, "
                    "%" PRIu64 " drops of %u registers, %u distinct sets | "
                    "shared: %" PRIu64 " drops, chained: %" PRIu64 " | "
                    "undefined-only: %u of %u registers, edges keep %" PRIu64
                    " drops, chained %" PRIu64 "\r\n",
            b_h, pre_n, num_h, tot_d, uu_h, dis_h, dis_d, cha_d,
            us_h, uu_h, res_tot_d, res_cha_d);

    c3_free(set_d); c3_free(res_d); c3_free(edg_h); c3_free(idx_h);
    c3_free(uni_d); c3_free(saf_d);
  }

  #undef _NC_TARGET
  c3_free(def_h);
  c3_free(cnt_h);
}
#endif

/* _nc_dt: scratch of _nc_drop_tree(), for qsort.
*/
static const c3_h* _nc_dt_edo_h;
static const c3_d* _nc_dt_edr_d;
static const c3_h* _nc_dt_tgt_h;

/* _nc_dt_len(): the number of drops on edge e.
*/
static inline c3_h
_nc_dt_len(c3_h e_h)
{
  return _nc_dt_edo_h[e_h + 1] - _nc_dt_edo_h[e_h];
}

/* _nc_dt_cmp(): edges by target, then drop sets by size and registers.
*/
static int
_nc_dt_cmp(const void* a_v, const void* b_v)
{
  c3_h a_h = *(const c3_h*)a_v, b_h = *(const c3_h*)b_v;
  c3_h l_h, i_h;

  if ( _nc_dt_tgt_h[a_h] != _nc_dt_tgt_h[b_h] ) {
    return ( _nc_dt_tgt_h[a_h] > _nc_dt_tgt_h[b_h] ) ? 1 : -1;
  }
  if ( _nc_dt_len(a_h) != _nc_dt_len(b_h) ) {
    return ( _nc_dt_len(a_h) > _nc_dt_len(b_h) ) ? 1 : -1;
  }
  for ( l_h = _nc_dt_len(a_h), i_h = 0; i_h < l_h; i_h++ ) {
    c3_h r_h = (c3_h)_nc_dt_edr_d[_nc_dt_edo_h[a_h] + i_h];
    c3_h q_h = (c3_h)_nc_dt_edr_d[_nc_dt_edo_h[b_h] + i_h];
    if ( r_h != q_h ) {
      return ( r_h > q_h ) ? 1 : -1;
    }
  }
  return ( a_h > b_h ) - ( a_h < b_h );
}

/* _nc_drop_tree(): share the blocks of drops of the edges into a block.
**
**   The edges with drops into one block get a node per distinct set of
**   drops, the edges with that set all leading to its block; a node's
**   parent is the biggest node into the same block whose set is a subset
**   of its own, so its block drops only the difference and goes on to the
**   parent's, the root's to the block itself.  The edges into a join that
**   many paths reach, a parser's failure, carry sets that mostly contain
**   one another: the drops are then about linear in the edges rather than
**   their product with the registers that die there.
**
**   edo_h: drops of edge e in edr_d[edo_h[e] .. edo_h[e + 1]), by
**   register; edn_h jump edges have drops.  The fall-through edges keep a
**   block of their own drops, after the predecessor: no jumps on a path
**   that may be hot.  Produces the number of nodes and,
**   for an edge with drops, nod_h[e], its node; for a node, nde_h an edge
**   with its set, ndp_h its parent or _nc_none, and ndt_h the target.
*/
static c3_h
_nc_drop_tree(nc_gen* gen_u, const c3_h* edo_h, const c3_d* edr_d,
              c3_h edn_h, c3_h reg_h,
              c3_h* nod_h, c3_h* nde_h, c3_h* ndp_h, c3_h* ndt_h)
{
  c3_h  egn_h = 2 * gen_u->lan_h;
  c3_h* eds_h = u3a_malloc(c3_max(edn_h, 1) * sizeof(c3_h));
  c3_h* tgt_h = u3a_malloc(c3_max(egn_h, 1) * sizeof(c3_h));
  c3_h* cix_h = u3a_malloc(c3_max(reg_h, 1) * sizeof(c3_h));
  c3_h* rgs_h = u3a_malloc(c3_max(reg_h, 1) * sizeof(c3_h));
  c3_h  non_h = 0, i_h, n_h = 0, e_h;

  for ( e_h = 0; e_h < egn_h; e_h++ ) {
    nod_h[e_h] = _nc_none;
    if ( (edo_h[e_h] != edo_h[e_h + 1]) && !(e_h & 1) ) {
      nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[e_h >> 1]]);
      tgt_h[e_h]    = blk_u->kid_h[(e_h & 1) ? 1 : 0];
      eds_h[n_h++]  = e_h;
    }
  }
  u3_assert( n_h == edn_h );

  _nc_dt_edo_h = edo_h;
  _nc_dt_edr_d = edr_d;
  _nc_dt_tgt_h = tgt_h;
  qsort(eds_h, edn_h, sizeof(c3_h), _nc_dt_cmp);

  //  one node per distinct set into a target, by size within the target;
  //  the keys of edr_d carry the edge above the register, so compare the
  //  registers
  //
  for ( i_h = 0; i_h < edn_h; i_h++ ) {
    e_h = eds_h[i_h];
    if ( non_h ) {
      c3_h f_h = nde_h[non_h - 1];
      c3_t eql_t = (tgt_h[f_h] == tgt_h[e_h])
                && (_nc_dt_len(f_h) == _nc_dt_len(e_h));

      for ( c3_h k_h = 0; eql_t && (k_h < _nc_dt_len(e_h)); k_h++ ) {
        eql_t = ( (c3_h)edr_d[edo_h[f_h] + k_h]
               == (c3_h)edr_d[edo_h[e_h] + k_h] );
      }
      if ( eql_t ) {
        nod_h[e_h] = non_h - 1;
        continue;
      }
    }
    nde_h[non_h] = e_h;
    ndt_h[non_h] = tgt_h[e_h];
    ndp_h[non_h] = _nc_none;
    nod_h[e_h]   = non_h++;
  }

  //  the parents, a target at a time: the sets as bitmaps of the
  //  registers dropped into it, the biggest smaller subset first
  //
  for ( i_h = 0; i_h < reg_h; i_h++ ) {
    cix_h[i_h] = _nc_none;
  }

  for ( c3_h g_h = 0, h_h; g_h < non_h; g_h = h_h ) {
    c3_h  uni_h = 0, wor_h, m_h;
    c3_d* bit_d;

    for ( h_h = g_h; (h_h < non_h) && (ndt_h[h_h] == ndt_h[g_h]); h_h++ );
    if ( (h_h - g_h) < 2 ) {
      continue;
    }

    for ( m_h = g_h; m_h < h_h; m_h++ ) {
      c3_h f_h = nde_h[m_h];
      for ( c3_h k_h = edo_h[f_h]; k_h < edo_h[f_h + 1]; k_h++ ) {
        c3_h r_h = (c3_h)edr_d[k_h];
        if ( _nc_none == cix_h[r_h] ) {
          cix_h[r_h] = uni_h;
          rgs_h[uni_h++] = r_h;
        }
      }
    }

    wor_h = (uni_h + 63) >> 6;
    bit_d = u3a_calloc((c3_z)(h_h - g_h) * wor_h, sizeof(c3_d));

    for ( m_h = g_h; m_h < h_h; m_h++ ) {
      c3_h  f_h = nde_h[m_h];
      c3_d* b_d = bit_d + ((c3_z)(m_h - g_h) * wor_h);
      for ( c3_h k_h = edo_h[f_h]; k_h < edo_h[f_h + 1]; k_h++ ) {
        c3_h x_h = cix_h[(c3_h)edr_d[k_h]];
        b_d[x_h >> 6] |= 1ULL << (x_h & 63);
      }
    }

    for ( m_h = g_h + 1; m_h < h_h; m_h++ ) {
      c3_d* s_d = bit_d + ((c3_z)(m_h - g_h) * wor_h);
      c3_h  s_h = _nc_dt_len(nde_h[m_h]);
      c3_h  j_h, ned_h = 0;

      for ( j_h = m_h; (j_h-- > g_h) && (ned_h < 4096); ned_h++ ) {
        c3_d* t_d = bit_d + ((c3_z)(j_h - g_h) * wor_h);
        c3_t  sub_t = 1;

        if ( _nc_dt_len(nde_h[j_h]) >= s_h ) {
          continue;
        }
        for ( c3_h w_h = 0; w_h < wor_h; w_h++ ) {
          if ( t_d[w_h] & ~s_d[w_h] ) {
            sub_t = 0;
            break;
          }
        }
        if ( sub_t ) {
          ndp_h[m_h] = j_h;
          break;
        }
      }
    }

    for ( m_h = 0; m_h < uni_h; m_h++ ) {
      cix_h[rgs_h[m_h]] = _nc_none;
    }
    u3a_free(bit_d);
  }

  u3a_free(eds_h);
  u3a_free(tgt_h);
  u3a_free(cix_h);
  u3a_free(rgs_h);
  return non_h;
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
**  the branches, _nc_edgy()), else into a block of drops for the edge:
**  for a jump, shared (_nc_drop_tree()), the jumps into a block with the
**  same drops sharing one and one whose drops contain another's dropping
**  the difference and going on to it; a block with a lone predecessor
**  lets its branch take its own sources the same way, over a drop at the
**  start.  So no drop ever
**  meets an empty slot, which a build without _nc_zer_t does not allow.
**  A build with it zeroes every empty slot, and a drop of a zero does
**  nothing: the drops left over for an edge go to the start of the block
**  with the common ones instead, and no block is made for the edge.
**
**  Then the ops are rebuilt in layout order: a block's drops first (or
**  left to its terminator's sweep, _nc_swe_t), then its ops, with a DRO
**  after an op whose product is dead on arrival; a block of drops for a
**  fall-through edge follows the predecessor, taking over its jump to
**  the yes block if it had one; those for jumps go at the end, the
**  branches retargeted to them, each falling through into the one it
**  goes on to when it comes right before it, else jumping there or to
**  its target.
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
  c3_d* sta_d;                           //  scratch: left over from the edges
                                         //  in, for the start (_nc_zer_t)
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
  //  on the edge, or at the start of the block too when empty slots are
  //  zero (_nc_zer_t)
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
  sta_d = ( _nc_zer_t ) ? u3a_malloc(chu_h * sizeof(c3_d)) : NULL;
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
      //
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

      //  dead on this edge in but not all: into the branch, or for a
      //  block on the edge; or, with empty slots zeroed, to the start of
      //  the block with the common drops: on the other edges in the
      //  register is dead already, its slot is zero, and the drop does
      //  nothing.  The branch takes what it can first either way, so
      //  that the two builds differ in the drops alone.
      //
      if ( pre_n > 1 ) {
        if ( _nc_zer_t ) {
          memset(sta_d, 0, chu_h * sizeof(c3_d));
        }

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

          if ( _nc_zer_t ) {
            for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
              sta_d[w_h] |= out_d[w_h];
            }
            continue;
          }

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

        if ( _nc_zer_t ) {
          for ( c3_h w_h = 0; w_h < chu_h; w_h++ ) {
            liv_d[w_h] |= sta_d[w_h];
          }
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
    }
  }

#undef _nc_out
#undef _nc_in

  //  rebuild the ops in layout order with the drops: those common to the
  //  edges into a block at its start, as drops or left to the terminator;
  //  that of a product never used right after its op; those of one edge
  //  in a block on the edge (none with _nc_zer_t): after the predecessor
  //  for its fall-through, taking over its jump to the yes block if it
  //  had one, else at the end, jumping on, with the branch retargeted to
  //  it.  The removed ops go.
  //
  _nc_key_sort(dro_d, dro_h);
  _nc_key_sort(edr_d, edr_h);
#ifdef U3NC_STAT
  //  U3NC_LIVES_STAT: what the alternatives to the drop blocks would save
  //
  {
    static c3_y lis_y = 2;
    if ( 2 == lis_y ) {
      lis_y = !!getenv("U3NC_LIVES_STAT");
    }
    if ( lis_y ) {
      _nc_lives_stat(gen_u, edr_d, edr_h, reg_h, arg_h, pro_h, pre_h);
    }
  }
#endif

  {
    c3_h*  edo_h = u3a_malloc(((2 * (c3_z)gen_u->lan_h) + 1) * sizeof(c3_h));
    c3_h   edn_h = 0, e_h;                     //  edges with drops
    c3_h   edj_h = 0;                          //    of them jumps
    c3_h   eft_h = 0;                          //    and fall-throughs
    c3_h   non_h = 0;                          //  blocks of drops (nodes)
    c3_h*  nod_h;                              //  edge -> node
    c3_h*  nde_h;                              //  node -> an edge with its set
    c3_h*  ndp_h;                              //  node -> parent, or _nc_none
    c3_h*  ndt_h;                              //  node -> target block
    c3_h*  ndb_h;                              //  node -> its block
    c3_h*  lay_h;                              //  the new layout
    c3_h   lan_h = 0;
    c3_d   cap_d;
    c3_h   cap_h;
    nc_op* new_u;
    c3_h   new_h = 0, dri_h = 0;

    for ( e_h = 0, edo_h[0] = 0; e_h < 2 * gen_u->lan_h; e_h++ ) {
      edo_h[e_h + 1] = edo_h[e_h] + edc_h[e_h];
      edn_h += !!edc_h[e_h];
      edj_h += !!edc_h[e_h] && !(e_h & 1);
    }
    eft_h = edn_h - edj_h;
    u3_assert( !_nc_zer_t || !edn_h );

    nod_h = u3a_malloc(c3_max(2 * (c3_z)gen_u->lan_h, 1) * sizeof(c3_h));
    nde_h = u3a_malloc(c3_max(edn_h, 1) * sizeof(c3_h));
    ndp_h = u3a_malloc(c3_max(edn_h, 1) * sizeof(c3_h));
    ndt_h = u3a_malloc(c3_max(edn_h, 1) * sizeof(c3_h));
    ndb_h = u3a_malloc(c3_max(edn_h, 1) * sizeof(c3_h));
    if ( edj_h ) {
      non_h = _nc_drop_tree(gen_u, edo_h, edr_d, edj_h, reg_h,
                            nod_h, nde_h, ndp_h, ndt_h);
    }
    else {
      for ( e_h = 0; e_h < 2 * gen_u->lan_h; e_h++ ) {
        nod_h[e_h] = _nc_none;
      }
    }
    _nc_stat_add(edb_d, non_h + eft_h);

    //  ops: the old ones twice over (a drop of a dead product each), the
    //  block drops, the edge drops (at most), a jump per node and one per
    //  fall-through edge
    //
    cap_d = (2 * (c3_d)gen_u->opn_h) + dro_h + edr_h + non_h + edn_h;
    if ( cap_d > c3_h_max ) {
      u3m_bail(c3__meme);
    }
    cap_h = cap_d;
    new_u = u3a_malloc(c3_max(cap_h, 1) * sizeof(nc_op));
    lay_h = u3a_malloc(((c3_z)gen_u->lan_h + non_h + eft_h) * sizeof(c3_h));

    if ( non_h + eft_h ) {
      gen_u->blk_u = u3a_realloc(gen_u->blk_u, blk_h * sizeof(nc_blk),
                                 ((c3_z)blk_h + non_h + eft_h)
                                 * sizeof(nc_blk));
    }
    if ( non_h ) {
      for ( c3_h n_h = 0; n_h < non_h; n_h++ ) {
        ndb_h[n_h] = gen_u->blk_h++;
      }
      for ( c3_h n_h = 0; n_h < non_h; n_h++ ) {
        nc_blk* nod_u = &(gen_u->blk_u[ndb_h[n_h]]);
        nod_u->blob     = u3_none;
        nod_u->fir_h    = 0;
        nod_u->len_h    = 0;
        nod_u->off_h    = 0;
        nod_u->lay_h    = _nc_none;
        nod_u->kin_y    = 1;
        nod_u->kid_h[0] = ( _nc_none == ndp_h[n_h] )
                        ? ndt_h[n_h]
                        : ndb_h[ndp_h[n_h]];
        nod_u->kid_h[1] = _nc_none;
      }
    }

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

      //  the edges out of this one with drops: the jump goes to the block
      //  of its node, its branch retargeted; the fall-through to a block
      //  of its drops that follows, which either falls through to the yes
      //  block in turn or takes over the jump to it
      //
      for ( k_y = 0; k_y < 2; k_y++ ) {
        c3_h kid_h;

        e_h = (2 * i_h) + k_y;

        if ( edo_h[e_h] == edo_h[e_h + 1] ) {
          continue;
        }

        //  if a block's edge has drops then it gotta be branchy
        u3_assert( bra_u && (blk_u->kin_y > 1) );

        kid_h = blk_u->kid_h[k_y];

        if ( k_y ) {
          c3_h    edg_h = gen_u->blk_h++;
          nc_blk* edg_u = &(gen_u->blk_u[edg_h]);

          edg_u->blob     = u3_none;
          edg_u->off_h    = 0;
          edg_u->lay_h    = _nc_none;
          edg_u->kin_y    = 1;
          edg_u->kid_h[0] = kid_h;
          edg_u->kid_h[1] = _nc_none;

          if ( hop_u ) {
            u3_assert( (hop_u->tar_h == kid_h)
                    && (hop_u == &(new_u[new_h - 1])) );
            new_h--;
            blk_u->len_h--;
          }
          edg_u->fir_h = new_h;
          for ( c3_h d_h = edo_h[e_h]; d_h < edo_h[e_h + 1]; d_h++ ) {
            nc_op* op_u = _nc_op_init(&(new_u[new_h++]), _nc_dro);
            op_u->src_h[0] = (c3_h)edr_d[d_h];
          }
          if ( hop_u ) {
            nc_op* op_u = _nc_op_init(&(new_u[new_h++]), _nc_hop);
            op_u->tar_h = kid_h;
          }
          edg_u->len_h = new_h - edg_u->fir_h;
          lay_h[lan_h++] = edg_h;
        }
        else {
          c3_h nob_h = ndb_h[nod_h[e_h]];

          u3_assert( gen_u->blk_u[nob_h].lay_h == _nc_none );
          u3_assert( bra_u->tar_h == kid_h );
          bra_u->tar_h = nob_h;
        }
      }
    }

    //  the blocks of the nodes, at the end: each drops what its set has
    //  beyond its parent's and goes on to the parent, falling through when
    //  it comes right before it; a chain at a time, from a node no other
    //  node precedes, up to a parent already laid out or the target
    //
    if ( non_h ) {
      c3_y* don_y = u3a_calloc(non_h, sizeof(c3_y));

      for ( c3_h n_h = non_h; n_h-- > 0; ) {
        c3_h m_h = n_h;

        while ( (_nc_none != m_h) && !don_y[m_h] ) {
          nc_blk* nod_u = &(gen_u->blk_u[ndb_h[m_h]]);
          c3_h    p_h   = ndp_h[m_h];
          c3_h    f_h   = nde_h[m_h];
          c3_h    k_h   = edo_h[f_h];
          c3_h    q_h, l_h;

          don_y[m_h]   = 1;
          nod_u->fir_h = new_h;

          //  the drops of the set but not of the parent's: both sorted
          //
          if ( _nc_none == p_h ) {
            q_h = l_h = 0;
          }
          else {
            q_h = edo_h[nde_h[p_h]];
            l_h = edo_h[nde_h[p_h] + 1];
          }
          for ( ; k_h < edo_h[f_h + 1]; k_h++ ) {
            c3_h r_h = (c3_h)edr_d[k_h];

            while ( (q_h < l_h) && ((c3_h)edr_d[q_h] < r_h) ) {
              q_h++;
            }
            if ( (q_h < l_h) && ((c3_h)edr_d[q_h] == r_h) ) {
              continue;
            }
            nc_op* op_u = _nc_op_init(&(new_u[new_h++]), _nc_dro);
            op_u->src_h[0] = r_h;
          }

          if ( (_nc_none == p_h) || don_y[p_h] ) {
            nc_op* op_u = _nc_op_init(&(new_u[new_h++]), _nc_hop);
            op_u->tar_h = nod_u->kid_h[0];
          }

          nod_u->len_h = new_h - nod_u->fir_h;
          lay_h[lan_h++] = ndb_h[m_h];
          m_h = p_h;
        }
      }
      u3a_free(don_y);
    }

    u3_assert( (dri_h == dro_h)
            && (new_h <= cap_h)
            && (lan_h == gen_u->lan_h + non_h + eft_h)
            && (gen_u->blk_h == blk_h + non_h + eft_h) );

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
    u3a_free(nod_h);
    u3a_free(nde_h);
    u3a_free(ndp_h);
    u3a_free(ndt_h);
    u3a_free(ndb_h);
  }

  u3a_free(use_h);
  u3a_free(pro_h);
  u3a_free(pre_h);
  u3a_free(inn_d);
  u3a_free(liv_d);
  u3a_free(out_d);
  u3a_free(sta_d);
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
**  With _nc_zer_t == 1 a drop at the start of a block may read a register that
**  is dead on another edge in (_nc_lives()): its ranges then cover that path
**  back to a definition, or to the entry, and keep the slot, zero there, from
**  other registers.
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

  //  the counts of the immediates encoded, for sizing their encodings
  //
  if ( buf_y ) {
    switch ( fam_y ) {
      case _nc_imm: {
        _nc_stat(imm_d);
        _nc_stat_if(0 == op_u->imm_h, imz_d);
        _nc_stat_if(1 == op_u->imm_h, imo_d);
        _nc_stat_if(op_u->dst_h >= 0x100, imw_d);
      } break;
      case _nc_kil: _nc_stat_if(!_nc_zer_t, kil_d); break;
      case _nc_iml: _nc_stat(iml_d); break;
      default: break;
    }
  }

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
      tmp_u       = *op_u;    //  IMM 0 when consuming does not zero
      tmp_u.fam_y = fam_y = _nc_imm;
      tmp_u.imm_h = 0;
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
**               Not for BOM, which is no family's.
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
  c3_c* tag_c = u3r_string(lit);

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

  fprintf(stderr, "program %p: %u slots, %u args, %u bytes (mug %08x), "
                  "%u literals, %u call sites\r\n",
          (void*)pog_u, pog_u->tot_h, pog_u->arg_h, pog_u->byc_u.len_h,
          u3r_mug_bytes(pog, pog_u->byc_u.len_h),
          pog_u->lit_u.len_h, pog_u->dir_u.len_h);

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[i_h]);
    c3_h       j_h;

    fprintf(stderr, "  site %u: bell %08x %s%s%s cid %u args",
            i_h, u3r_mug(dir_u->bell),
            ( c3y == dir_u->mon_o ) ? "subject" : "direct",
            !dir_u->arm_u ? ""
              : dir_u->arm_u->pun_t ? " punt-jet"
              : ( dir_u->arm_u->own_f && (c3y == dir_u->ice_o) )
              ? " owning-jet"
              : " array-jet",
            ( dir_u->arm_u && !dir_u->arm_u->pun_t && (c3n == dir_u->ice_o) )
              ? " checked" : "",
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

  //  literals: a small atom as text if it prints, else its mug
  //
  for ( i_h = 0; i_h < pog_u->lit_u.len_h; i_h++ ) {
    u3_noun lit = pog_u->lit_u.non[i_h];

    if ( (c3y == u3ud(lit)) && (u3r_met(3, lit) <= 16) ) {
      c3_y  buf_y[17] = {0};
      c3_w  len_w = u3r_met(3, lit), j_w;
      c3_t  txt_t = (len_w > 0);

      u3r_bytes(0, len_w, buf_y, lit);
      for ( j_w = 0; j_w < len_w; j_w++ ) {
        if ( (buf_y[j_w] < 32) || (buf_y[j_w] > 126) ) txt_t = 0;
      }
      if ( txt_t ) {
        fprintf(stderr, "  lit #%u: '%s'\r\n", i_h, (c3_c*)buf_y);
      }
      else {
        fprintf(stderr, "  lit #%u: atom 0x%" PRIx64 "%s\r\n", i_h,
                (len_w <= 8) ? u3r_chub(0, lit) : (c3_d)u3r_mug(lit),
                (len_w <= 8) ? "" : " (mug)");
      }
    }
    else {
      fprintf(stderr, "  lit #%u: %s mug %08x\r\n", i_h,
              (c3y == u3ud(lit)) ? "atom" : "cell", u3r_mug(lit));
    }
  }

  while ( ip_h < pog_u->byc_u.len_h ) {
    c3_y cod_y = pog[ip_h];
    c3_h off_h = ip_h;

    if ( BOM == cod_y ) {
      ip_h++;
      fprintf(stderr, "  %4u: %s", off_h, _nc_fam_c[_nc_bom]);
    }
    else {
      c3_y kon_y, wid_y, fam_y = _nc_decode(pog, &ip_h, &kon_y, &wid_y);

      if ( !(  _nc_fam[fam_y].imm_y || _nc_fam[fam_y].src_y
            || _nc_fam[fam_y].dst_y || _nc_fam[fam_y].tar_y) )
      {
        fprintf(stderr, "  %4u: %s", off_h, _nc_fam_c[fam_y]);
      }
      else {
        fprintf(stderr, "  %4u: %s_%c%s", off_h, _nc_fam_c[fam_y], "BSV"[wid_y],
                ( _nc_fam[fam_y].swe_y && kon_y ) ? " sweep" : " ");
      }

      if ( _nc_fam[fam_y].imm_y ) {
        c3_h imm_h = _nc_read(pog, &ip_h, wid_y);

        //  the atom itself, or an index
        //
        fprintf(stderr, " %s%u",
                ((_nc_imm == fam_y) || (_nc_eqi == fam_y)) ? "" : "#", imm_h);

        if (  (_nc_hsp == fam_y) || (_nc_hse == fam_y)
           || (_nc_hdp == fam_y) || (_nc_hde == fam_y) )
        {
          _nc_print_hint(pog_u->lit_u.non[imm_h]);
        }
        else if ( _nc_hlp == fam_y ) {
          _nc_print_hint(u3h(pog_u->lit_u.non[imm_h]));
          _nc_print_spot(u3t(pog_u->lit_u.non[imm_h]));
        }
        else if ( _nc_iml == fam_y ) {
          _nc_print_spot(pog_u->lit_u.non[imm_h]);
        }
      }
      for ( i_h = 0; i_h < _nc_fam[fam_y].src_y; i_h++ ) {
        static const c3_c* edg_c[] = { "", "!f", "!j", "!" };
        const c3_c*        kon_c   = _nc_edgy(fam_y)
                                   ? edg_c[(kon_y >> (2 * i_h)) & 3]
                                   : ( ((kon_y >> i_h) & 1)
                                     || (_nc_her == fam_y)
                                     || (_nc_tar == fam_y) ) ? "!" : "";
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
#ifdef U3NC_STAT
  c3_w ali_w;   //  aliasable HED/TAL by bytecode offset (byc + 1 bytes)
#endif
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

#ifdef U3NC_STAT
  lay_u.ali_w = len_w;
  len_w += byc_h + 1;
#endif

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
#ifdef U3NC_STAT
  pog_u->ali_y       = dat_y + lay_u.ali_w;
#endif
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
  pog_u->jet         = u3_nul;
  _nc_prog_fix(pog_u);
#ifdef U3NC_STAT
  pog_u->ent_d = 0;
  pog_u->con_d = 0;
  pog_u->bor_d = 0;
  memset(pog_u->ali_y, 0, byc_h + 1);
#endif

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

#ifdef U3NC_STAT
/* _nc_stat_borrow(): for the statistics, the drop-only parameters:
**                    argument registers, through the moves that rename
**                    them, that no op ever consumes into a product, a
**                    call, a hint or a return: every path only reads and
**                    drops them, so the callee could borrow them.
**                    Produces them as a bitmask.
*/
static c3_d
_nc_stat_borrow(nc_gen* gen_u, c3_h arg_h)
{
  c3_h  reg_h = c3_max(gen_u->reg_h, arg_h);
  c3_h* par_h = u3a_malloc(c3_max(reg_h, 1) * sizeof(c3_h));  //  union-find
  c3_y* con_y = u3a_calloc(c3_max(reg_h, 1), sizeof(c3_y));   //  consumed
  c3_d  bor_d = 0;
  c3_h  i_h, j_h, r_h;

#define _nc_root(r)  ({ c3_h _r = (r); while ( par_h[_r] != _r ) { _r = par_h[_r]; } _r; })

  for ( r_h = 0; r_h < reg_h; r_h++ ) {
    par_h[r_h] = r_h;
  }

  //  consuming moves rename: union source and destination
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);
    if ( (_nc_mov == op_u->fam_y) && (op_u->kon_y & 1) ) {
      par_h[_nc_root(op_u->src_h[0])] = _nc_root(op_u->dst_h);
    }
  }

  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);

    for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
      nc_op* op_u = &(gen_u->ops_u[blk_u->fir_h + j_h]);
      c3_y   fam_y = op_u->fam_y;

      //  consumption: an op that takes a source into its product or
      //  elsewhere; every other variant bit is a drop
      //
      switch ( fam_y ) {
        default: break;

        case _nc_inc: case _nc_con: case _nc_cor: {
          for ( c3_h k_h = 0; k_h < _nc_fam[fam_y].src_y; k_h++ ) {
            if ( (op_u->kon_y >> k_h) & 1 ) {
              con_y[_nc_root(op_u->src_h[k_h])] = 1;
            }
          }
        } break;

        case _nc_nok: case _nc_hdp: {
          if ( op_u->kon_y & 1 ) {
            con_y[_nc_root(op_u->src_h[0])] = 1;
          }
        } break;

        case _nc_don: {
          con_y[_nc_root(op_u->src_h[0])] = 1;
        } break;

        case _nc_cal: case _nc_cap: case _nc_cam: case _nc_csm:
        case _nc_jmp: case _nc_jms: case _nc_jsp: case _nc_hmp:
        case _nc_cax: case _nc_jmx: {
          nc_dir* dir_u = &(gen_u->dir_u[op_u->imm_h]);
          for ( c3_h k_h = 0; k_h < dir_u->len_h; k_h++ ) {
            if ( gen_u->pok_y[dir_u->sot_h + k_h] ) {
              con_y[_nc_root(gen_u->pol_h[dir_u->sot_h + k_h])] = 1;
            }
          }
        } break;
      }
    }
  }

  for ( r_h = 0; (r_h < arg_h) && (r_h < 64); r_h++ ) {
    if ( !con_y[_nc_root(r_h)] ) {
      bor_d |= (c3_d)1 << r_h;
    }
  }

#undef _nc_root
  u3a_free(par_h);
  u3a_free(con_y);
  return bor_d;
}
#endif

#ifdef U3NC_STAT
/* _nc_stat_consumes(): whether op consumes, as opposed to drops, its
**                      source register r read with its consume bit set:
**                      takes it into a product, a call, a hint or the
**                      return.  Call sites: by the flags of their args.
*/
static c3_t
_nc_stat_consumes(nc_gen* gen_u, nc_op* op_u, c3_h r_h)
{
  c3_y fam_y = op_u->fam_y;

  switch ( fam_y ) {
    default: return 0;

    case _nc_mov: case _nc_inc: case _nc_con: case _nc_cor: {
      for ( c3_h k_h = 0; k_h < _nc_fam[fam_y].src_y; k_h++ ) {
        if ( (op_u->src_h[k_h] == r_h) && ((op_u->kon_y >> k_h) & 1) ) {
          return 1;
        }
      }
      return 0;
    }

    case _nc_nok: case _nc_hdp: {
      return (op_u->src_h[0] == r_h) && (op_u->kon_y & 1);
    }

    case _nc_don: return 1;

    case _nc_cal: case _nc_cap: case _nc_cam: case _nc_csm:
    case _nc_jmp: case _nc_jms: case _nc_jsp: case _nc_hmp:
    case _nc_cax: case _nc_jmx: {
      nc_dir* dir_u = &(gen_u->dir_u[op_u->imm_h]);
      for ( c3_h k_h = 0; k_h < dir_u->len_h; k_h++ ) {
        if (  (gen_u->pol_h[dir_u->sot_h + k_h] == r_h)
           && gen_u->pok_y[dir_u->sot_h + k_h] )
        {
          return 1;
        }
      }
      return 0;
    }
  }
}

/* _nc_stat_reads(): whether op reads register r, and whether its read of
**                   it releases it (consume bit, or DRO), into *rel_t.
*/
static c3_t
_nc_stat_reads(nc_gen* gen_u, nc_op* op_u, c3_h r_h, c3_t* rel_t)
{
  c3_t red_t = 0;
  *rel_t = 0;
  _nc_srcs(gen_u, op_u, reg_h, {
    if ( *reg_h == r_h ) {
      red_t = 1;
      if ( _nc_fam[op_u->fam_y].sot_y ) {
        *rel_t |= gen_u->pok_y[reg_h - gen_u->pol_h];
      }
      else if ( _nc_dro == op_u->fam_y ) {
        *rel_t = 1;
      }
      else if ( !_nc_fam[op_u->fam_y].swe_y ) {
        c3_h k_h = reg_h - op_u->src_h;
        *rel_t |= _nc_edgy(op_u->fam_y)
                ? !!((op_u->kon_y >> (2 * k_h)) & 3)
                : ((op_u->kon_y >> k_h) & 1);
      }
    }
  });
  return red_t;
}

/* _nc_stat_alias(): for the statistics, mark each borrowing HED/TAL
**                   whose product r could be an uncounted alias of the
**                   part of its source p: every read of r is in its
**                   block, none consumes it (they only read and drop
**                   it), r dies in the block, and p is not released
**                   before the last read of r.  Then the gain at the
**                   HED/TAL and the drop of r could both go.
*/
static void
_nc_stat_alias(nc_gen* gen_u)
{
  c3_h i_h, j_h, k_h;

  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);

    for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
      nc_op* op_u = &(gen_u->ops_u[blk_u->fir_h + j_h]);
      c3_h   r_h, p_h;
      c3_t   die_t = 0, bad_t = 0;

      if (  ((_nc_hed != op_u->fam_y) && (_nc_tal != op_u->fam_y))
         || (op_u->kon_y & 1) )
      {
        continue;
      }
      r_h = op_u->dst_h;
      p_h = op_u->src_h[0];

      for ( k_h = j_h + 1; k_h < blk_u->len_h; k_h++ ) {
        nc_op* nex_u = &(gen_u->ops_u[blk_u->fir_h + k_h]);
        c3_t   rel_t, prl_t;
        c3_t   red_t = _nc_stat_reads(gen_u, nex_u, r_h, &rel_t);
        c3_t   pre_t = _nc_stat_reads(gen_u, nex_u, p_h, &prl_t);

        if ( red_t ) {
          if ( _nc_stat_consumes(gen_u, nex_u, r_h) ) {
            bad_t = 1;
            break;
          }
          if ( rel_t ) {
            die_t = 1;
            break;      //  r dies here; p may go with it
          }
        }
        if ( (pre_t && prl_t) || (nex_u->dst_h == p_h) ) {
          bad_t = 1;    //  p released or redefined while r lives
          break;
        }
        if ( _nc_fam[nex_u->fam_y].tar_y || _nc_fam[nex_u->fam_y].swe_y ) {
          break;        //  block ends: r live out, if not dead
        }
      }

      op_u->ali_y = die_t && !bad_t;
    }
  }
}
#endif

/* _nc_reuse(): pair each CON with a consuming HED or TAL before it in
**              its block, whose cell it reuses when the slot's reference
**              was the only one: the HED or TAL becomes a HER or TAR,
**              which pushes the cell it frees, or 0, on the road stack,
**              and the CON a COR, which pops it.  The tokens nest on the
**              stack, so a CON takes the latest pending one.  Nothing
**              else touches the stack between the ops of a block but a
**              hint prologue or epilogue, which pushes or pops its own
**              word, so those cut the pending tokens off.  Runs after
**              _nc_lives() and the slot assignment, when the consume
**              bits are final; a block runs through in order, so every
**              token pushed is popped before its block ends, and a bail
**              in between abandons the road, tokens and all.
*/
static void
_nc_reuse(nc_gen* gen_u)
{
  c3_h* pen_h = NULL;   //  pending tokens: the ops that push them
  c3_h  pen_n = 0, pec_h = 0;
  c3_h  i_h, j_h;

  for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);

    pen_n = 0;

    for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
      c3_h   o_h  = blk_u->fir_h + j_h;
      nc_op* op_u = &(gen_u->ops_u[o_h]);

      switch ( op_u->fam_y ) {
        default: break;

        case _nc_hed:
        case _nc_tal: {
          if ( op_u->kon_y & 1 ) {
            _nc_grow(pen_h, pen_n, pec_h, c3_h);
            pen_h[pen_n++] = o_h;
          }
        } break;

        case _nc_con: {
          if ( pen_n ) {
            nc_op* tok_u = &(gen_u->ops_u[pen_h[--pen_n]]);

            tok_u->fam_y = ( _nc_hed == tok_u->fam_y ) ? _nc_her : _nc_tar;
            tok_u->kon_y = 0;
            op_u->fam_y  = _nc_cor;
          }
        } break;

        case _nc_hsp: case _nc_hse:
        case _nc_hdp: case _nc_hde: case _nc_hlp: {
          pen_n = 0;
        } break;
      }
    }
  }

  if ( pen_h ) {
    u3a_free(pen_h);
  }
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

  _nc_reuse(gen_u);

#ifdef U3NC_STAT
  c3_d bor_d = _nc_stat_borrow(gen_u, arg_h);
  _nc_stat_alias(gen_u);
#endif

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
  _nc_stat_add(byc_d, byc_h);
  _nc_stat_add(lit_d, gen_u->lit_h);
  pog_u->tot_h = gen_u->tot_h;
  pog_u->arg_h = arg_h;
  pog_u->ned   = u3k(ned);
  pog_u->jet   = gen_u->jet;
  gen_u->jet   = u3_nul;

  {
    c3_y* buf_y = pog_u->byc_u.ops_y;
    c3_h  pos_h = 0;

    for ( i_h = 0; i_h < gen_u->lan_h; i_h++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_h[i_h]]);

      u3_assert( blk_u->off_h == pos_h );

      for ( j_h = 0; j_h < blk_u->len_h; j_h++ ) {
#ifdef U3NC_STAT
        c3_h was_h = pos_h;
#endif
        pos_h += _nc_encode(gen_u, &(gen_u->ops_u[blk_u->fir_h + j_h]),
                            buf_y + pos_h);
#ifdef U3NC_STAT
        if ( _nc_dro == gen_u->ops_u[blk_u->fir_h + j_h].fam_y ) {
          u3nc_Stat.dre_d++;
          u3nc_Stat.drb_d += pos_h - was_h;
        }
        pog_u->ali_y[pos_h] = gen_u->ops_u[blk_u->fir_h + j_h].ali_y;
#endif
      }
    }

    u3_assert( pos_h == byc_h );
#ifdef U3NC_STAT
    pog_u->bor_d = bor_d;
#endif
  }

  u3h_walk_with(gen_u->lit_p, _nc_lit_cb, pog_u->lit_u.non);

  //  the destination slot of a call is in its site
  //  not done in _nc_encode to avoid the destination slot's value affecting
  //  encoding size
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
    dat_u->ice_o = dir_u->ice_o;
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

#ifdef U3NC_STAT
  c3_d byc_d = u3nc_Stat.byc_d;
  c3_d drb_d = u3nc_Stat.drb_d;
  c3_d edb_d = u3nc_Stat.edb_d;
#endif

  _nc_blocks(&gen_u, map);
  _nc_translate(&gen_u);
  _nc_fold(&gen_u);
  _nc_sink(&gen_u);
  _nc_lives(&gen_u, arg_h);
  _nc_slots(&gen_u, arg_h);
  pog_u = _nc_emit(&gen_u, ned, arg_h);

  _nc_stat(com_d);

  //  a big program, or one with many blocks of drops for edges: its
  //  number, as _nc_print() counts them, and how much of it is drops
  //
#ifdef U3NC_STAT
  if (  (u3nc_Stat.byc_d - byc_d >= 16384)
     || (u3nc_Stat.edb_d - edb_d >= 64) )
  {
    fprintf(stderr, "u3nc: big program %" PRIu64 ": %" PRIu64 " bytes, "
                    "%" PRIu64 " of DRO, %" PRIu64 " edge blocks, "
                    "%" PRIc3_h " blocks, %" PRIc3_h " args\r\n",
            u3nc_Stat.com_d, u3nc_Stat.byc_d - byc_d,
            u3nc_Stat.drb_d - drb_d, u3nc_Stat.edb_d - edb_d,
            gen_u.blk_h, arg_h);
  }
#endif
  if ( _nc_verb_t ) {
    _nc_print(pog_u);
  }

  u3h_free(gen_u.idx_p);
  u3h_free(gen_u.lit_p);
  u3a_free(gen_u.lin);
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
  if ( gop_u->arg_h != dir_u->len_h ) {
    fprintf(stderr, "u3nc: site passes %u, program takes %u: bell %08x "
                    "(formula %08x, sock %08x) %s\r\n",
            dir_u->len_h, gop_u->arg_h, u3r_mug(dir_u->bell),
            u3r_mug(u3t(dir_u->bell)), u3r_mug(u3h(dir_u->bell)),
            ( c3y == dir_u->mon_o ) ? "mono" : "n-ary");
    u3_assert( gop_u->arg_h == dir_u->len_h );
  }
}

/* _nc_las_key(): the key, in this road's transient memo cache, of the
**   entry found last for a formula and a subject with sub's context: the
**   tail of its tail, what a gate keeps when its sample changes.  A rule
**   that a parser picks from a table is called over and over as the same
**   noun but for its sample, and many rules share a formula.  The cache
**   lives and dies with the road, as the subjects may.  RETAINS.
*/
static u3_noun
_nc_las_key(u3_noun sub, u3_noun fol)
{
  u3_noun con = ( (c3y == u3du(sub)) && (c3y == u3du(u3t(sub))) )
              ? u3t(u3t(sub))
              : u3_nul;

  return u3nt(c3__last, u3k(fol), u3k(con));
}

/* _nc_ent_get(): entry program for [sub fol], from any road: the most
**   specific whose sock sub matches.  Off the home road, which would keep
**   subjects alive from one event to the next, the entry that a subject
**   with the same context matched last is tried first, [entry subject]
**   in the cache: sub is checked against it only where it is not the
**   same noun as that subject.  RETAINS.
*/
static u3nc_prog*
_nc_ent_get(u3_noun sub, u3_noun fol)
{
  u3_weak got, lis;
  c3_t    hom_t = ( &(u3H->rod_u) == u3R );
  u3_noun key   = hom_t ? u3_nul : _nc_las_key(sub, fol);

  if ( !hom_t ) {
    u3_weak las = u3z_find(u3z_memo_toss, key);

    if ( u3_none != las ) {
      u3nc_prog* pog_u = ( c3y == u3d_fits_las(u3h(las), sub, u3t(las)) )
                       ? _nc_to(u3t(u3h(las)))
                       : NULL;
      u3z(las);
      if ( pog_u ) {
        u3z(key);
        return pog_u;
      }
    }
  }

  for (u3_road* rod_u = u3R; rod_u; rod_u = u3tn(u3_road, rod_u->par_p)) {
    if ( u3_none == (lis = u3h_git(rod_u->ska.ent_p, fol)) ) {
      continue;
    }
#ifdef U3NC_STAT
    //  u3d_match() scans every entry of the formula
    //
    for ( u3_noun l = lis; u3_nul != l; l = u3t(l) ) {
      _nc_stat(mat_d);
    }
#endif
    if ( u3_none != (got = u3d_match(sub, lis)) ) {
      if ( !hom_t ) {
        u3_noun las = u3nc(u3k(got), u3k(sub));

        u3z_save(u3z_memo_toss, key, las);
        u3z(las);
      }
      return _nc_to(u3t(got));
    }
  }

  u3z(key);
  return NULL;
}

/* _nc_link(): set the callee programs of the call sites that have been
**             compiled; the rest are linked when first called.  A call
**             with arguments of a total array jet never calls a program
**             by link: a checked one looks the unary program of its bell
**             up (_nc_mono()).
*/
static void
_nc_link(u3nc_prog* pog_u)
{
  c3_h i_h;

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[i_h]);
    u3nc_prog* gop_u;

    if ( dir_u->pog_p ) continue;

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
#ifdef U3NC_STAT
  struct timespec beg_u, end_u;
  clock_gettime(CLOCK_MONOTONIC, &beg_u);
#endif
  u3nc_prog* pog_u = _nc_build(straight, bell);
#ifdef U3NC_STAT
  clock_gettime(CLOCK_MONOTONIC, &end_u);
  u3nc_Stat.bld_d += ((c3_d)(end_u.tv_sec - beg_u.tv_sec) * 1000000000ULL
                      + end_u.tv_nsec - beg_u.tv_nsec) / 1000;
#endif
  _nc_link(pog_u);
  return pog_u;
}

/* _nc_here(): is a pointer on the given road's heap?
*/
static inline c3_t
_nc_here(void* ptr_v, u3_road* rod_u)
{
  u3_post pos_p = u3of(void, ptr_v);

  return ( c3y == u3a_is_north(rod_u) )
       ? ((pos_p >= rod_u->rut_p) && (pos_p < rod_u->hat_p))
       : ((pos_p >= rod_u->hat_p) && (pos_p < rod_u->rut_p));
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

  //  defer linking if the callee program is junior relative to the callsite,
  //  link otherwise
  for (u3_road* rod_u = u3R; rod_u; rod_u = u3tn(u3_road, rod_u->par_p)) {
    c3_t dir_t = _nc_here(dir_u, rod_u);
    c3_t gop_t = _nc_here(gop_u, rod_u);
    if ( dir_t ) {
      dir_u->pog_p = u3of(u3nc_prog, gop_u);
      return gop_u;
    }
    if ( gop_t ) {
      u3h_put(u3R->ska.def_p, _nc_of(pog_u), u3_nul);
      return gop_u;
    }
  }
  u3_assert(0);
}

/* _nc_edit(): som at axis axe_l of big, a cell made of an atom in the way.
**             TRANSFERS big and som.
*/
static u3_noun
_nc_edit(u3_noun big, c3_l axe_l, u3_noun som)
{
  u3_noun hed, tal;

  if ( 1 == axe_l ) {
    u3z(big);
    return som;
  }

  if ( c3y == u3du(big) ) {
    hed = u3k(u3h(big));
    tal = u3k(u3t(big));
  }
  else {
    hed = tal = 0;
  }
  u3z(big);

  return ( 2 == u3x_cap(axe_l) )
       ? u3nc(_nc_edit(hed, u3x_mas(axe_l), som), tal)
       : u3nc(hed, _nc_edit(tal, u3x_mas(axe_l), som));
}

/* _nc_core(): the core a call site of a total array jet calls the arm of,
**             rebuilt for a checked call: the parts of the subject the
**             sock of the bell knows, and the arguments, gained, at the
**             axes the driver takes them from (depth-first, like them).
**             What neither has is 0: the driver is the arm's function of
**             what it takes, so the arm doesn't read it.  RETAINS nex.
*/
static u3_noun
_nc_core(const u3nc_dire* dir_u, const u3_noun* nex)
{
  const u3u_harm* arm_u = dir_u->arm_u;
  u3_noun         cor   = u3k(u3t(u3h(dir_u->bell)));
  c3_w            i_w;

  for ( i_w = 0; i_w < arm_u->len_w; i_w++ ) {
    cor = _nc_edit(cor, arm_u->axe_l[i_w], u3k(nex[i_w]));
  }

  return cor;
}

/* _nc_mono(): the unary program of the bell of a call site, compiled if it
**             isn't yet, for a checked call, which rebuilds the core: a
**             call by subject (_nc_callee()), but looked up by bell every
**             time, never linked, so that the site keeps no pointer to
**             a program it isn't a caller of by kind.  The slow path of
**             an op that is slow by design.
*/
static u3nc_prog*
_nc_mono(const u3nc_dire* dir_u)
{
  u3_noun    key = _nc_dir_key(dir_u->bell, c3y);
  u3nc_prog* gop_u;

  u3t_off(noc_o);

  if ( !(gop_u = _nc_dir_get(key)) ) {
    gop_u = _nc_compile(u3d_dire(dir_u->bell, c3y), u3_none);
    u3h_put(u3R->ska.dir_p, key, _nc_of(gop_u));
  }

  u3t_on(noc_o);
  u3z(key);

  u3_assert( 1 == gop_u->arg_h );
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

#ifdef U3NC_STAT
    //  U3NC_ENT_LOG: each entry compiled, its formula and the depth of
    //  the poke it comes from (diagnostics)
    //
    {
      static c3_y log_y = 2;
      if ( 2 == log_y ) {
        log_y = !!getenv("U3NC_ENT_LOG");
      }
      if ( log_y ) {
        u3_noun jam = u3qe_jam(fol);
        c3_w    len_w = u3r_met(3, jam);
        fprintf(stderr, "u3nc entry miss: fol %08x (%u B jammed), sub %08x,"
                        " %u entries of the formula\r\n",
                u3r_mug(fol), len_w, u3r_mug(sub),
                (c3_w)(( u3_none == (old = u3h_git(u3R->ska.ent_p, fol)) )
                       ? 0 : u3qb_lent(old)));
        {
          static c3_w num_w = 0;
          if ( (len_w < 48) || ((len_w > 2100) && (len_w < 2200) && (num_w++ < 2)) ) {
            u3m_p("    fol", fol);
            //  the subject's shape, by mugs: printing it would take ages
            fprintf(stderr, "    sub: head %08x (%s), tail %08x\r\n",
                    u3r_mug(u3h(sub)), (c3y == u3du(u3h(sub))) ? "cell" : "atom",
                    u3r_mug(u3t(sub)));
          }
        }
        u3z(jam);
      }
    }
#endif
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

/* _nc_key(): the key of a memoized call over inn: seeded as nock.c's keys
**            are, or in our partition if the subject is partial (all_o
**            is c3n).  One cell, looked up and then saved: nock.c builds
**            one for the lookup and another for the save.  TRANSFERS inn.
*/
static u3_noun
_nc_key(c3_h cid_h, c3_o all_o, u3_noun inn)
{
  return u3nc(_nc_memo_seed(cid_h, all_o), inn);
}

/* _nc_save(): save the product of a memoized call under its key, as
**             nock.c does.  TRANSFERS key and pro.
*/
static u3_noun
_nc_save(c3_h cid_h, u3_noun key, u3_noun pro)
{
  if ( (u3z_memo_toss == cid_h)
       ? (&(u3H->rod_u) != u3R)
       : (0 == u3R->ski.gul) )
  {
    pro = u3z_save_dedup(cid_h, key, pro);
  }
  else {
    u3z(key);
  }
  return pro;
}

/* _nc_find(): look for a memoization entry by its key.  RETAINS key.
*/
static u3_weak
_nc_find(c3_h cid_h, u3_noun key)
{
  return u3z_find(cid_h, key);
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
#ifdef U3NC_STAT
  c3_y       spa_y;   //  the caller's activation-local spare (_nc_spa_t)
#endif
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

/* _nc_memo_jet(): is the driver of a call by subject a memoizing one, the
**                 +ut drivers: their misses run the callee's unary program,
**                 which the site then links before the kick and publishes in
**                 u3nc_Gop, so that a miss costs no entry search.
*/
static inline c3_t
_nc_memo_jet(const u3u_harm* arm_u)
{
  u3_noun (*mon_f)(u3_noun) = arm_u->mon_f;

  return (  (u3ufu_crop == mon_f) || (u3ufu_fish == mon_f)
         || (u3ufu_fuse == mon_f) || (u3ufu_mint == mon_f)
         || (u3ufu_mull == mon_f) || (u3ufu_nest_dext == mon_f)
         || (u3ufu_redo == mon_f) || (u3ufu_rest == mon_f) );
}

u3nc_prog* u3nc_Gop;

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

/* u3nc_gop_nock(): .*(bus fol) on a miss of a memoizing driver: the program
**                  of the call site, if it published one (consumed here,
**                  before the run, so that nested sites publish their own),
**                  else through the entry.  TRANSFERS.
*/
u3_noun
u3nc_gop_nock(u3_noun bus, u3_noun fol)
{
  u3nc_prog* gop_u = u3nc_Gop;

  if ( !gop_u ) {
    return u3nc_nock_on(bus, fol);
  }

  u3nc_Gop = 0;
  u3z(fol);
  return _nc_burn_out(gop_u, &bus, 1);
}

/* _nc_mean_run(): the arm of a deferred %mean trap on its arguments, a
**                 list: its n-ary program, compiled if it isn't yet, run
**                 on them.  TRANSFERS.
*/
static u3_noun
_nc_mean_run(u3_noun bell, u3_noun arg)
{
  u3_noun    key   = _nc_dir_key(bell, c3n);
  u3nc_prog* pog_u = _nc_dir_get(key);
  u3_noun    lis   = arg;
  u3_noun*   ary_u;
  u3_noun    pro;
  c3_h       i_h;

  if ( !pog_u ) {
    pog_u = _nc_compile(u3d_dire(bell, c3n), bell);
    u3h_put(u3R->ska.dir_p, key, _nc_of(pog_u));
  }
  u3z(key);

  c3_h arg_h = pog_u->arg_h;
  ary_u = u3a_malloc(arg_h * sizeof(u3_noun));
  for ( i_h = 0; i_h < arg_h; i_h++ ) {
    if ( c3n == u3du(lis) ) u3m_bail(c3__fail);
    ary_u[i_h] = u3k(u3h(lis));
    lis = u3t(lis);
  }
  if ( u3_nul != lis ) u3m_bail(c3__fail);
  u3z(arg);
  u3z(bell);

  pro = _nc_burn_out(pog_u, ary_u, arg_h);
  u3a_free(ary_u);
  return pro;
}

/* u3nc_mean(): the product of the arm of a deferred %mean trap, as
**              ++toon: _nc_mean_run() in a virtualization context
**              without a namespace, like +mack runs the trap.  The
**              compilation runs inside it too: a failing analysis is a
**              %fail, which passes through, where a crash of the arm is
**              the %2 of the toon.  RETAINS.
*/
u3_noun
u3nc_mean(u3_noun bell, u3_noun arg)
{
  return u3m_soft_run(u3_nul, _nc_mean_run, u3k(bell), u3k(arg));
}

c3_t u3nc_Self = 1;

/* u3nc_able(): can this road run compiled nock.
*/
c3_o
u3nc_able(void)
{
  //  in a poke of the SKA core made from inside one, the core runs in u3n:
  //  its %ska hints would ask it to compile its own code again, without end;
  //  without u3nc_Self, in any poke of it
  //
  return __(  (u3_nul != u3R->ska.cor)
           && !(u3R->how.fag_w & u3a_flag_mink)
           && (u3a_ska_depth(u3R->how.fag_w) < (u3nc_Self ? 2 : 1)) );
}

/* u3nc_slam_on(): produce (gat sam), compiled where it can be.
*/
u3_noun
u3nc_slam_on(u3_noun gat, u3_noun sam)
{
  u3_noun cor = u3nc(u3k(u3h(gat)), u3nc(sam, u3k(u3t(u3t(gat)))));

  u3z(gat);
  return u3nc_nock_on(cor, u3nq(9, 2, 0, 1));
}

/* u3nc_nock_on(): produce .*(bus fol); with u3n if this road can't.
*/
u3_noun
u3nc_nock_on(u3_noun bus, u3_noun fol)
{
  u3nc_prog* pog_u;
  u3_noun    pro;

  if ( c3n == u3nc_able() ) {
    return u3n_nock_on(bus, fol);
  }

  pog_u = _nc_entry(bus, fol);
  u3z(fol);
  pro = _nc_burn_out(pog_u, &bus, 1);

  if ( _nc_verb_t ) {
    fprintf(stderr, "u3nc: %" PRIu64 " entries, %" PRIu64 " compiled, "
                    "%" PRIu64 " jetted sites, %" PRIu64 " unresolved rings, "
                    "%" PRIu64 " direct calls, %" PRIu64 " subject calls, "
                    "%" PRIu64 " jet hits, %" PRIu64 " cons, "
                    "%" PRIu64 " unique decons, %" PRIu64 " drops, "
                    "%" PRIu64 " transferring calls, "
                    "%" PRIu64 " edits in place\r\n",
            u3nc_Stat.ent_d, u3nc_Stat.com_d, u3nc_Stat.arm_d,
            u3nc_Stat.rin_d, u3nc_Stat.dir_d, u3nc_Stat.sub_d,
            u3nc_Stat.jet_d, u3nc_Stat.con_d, u3nc_Stat.uni_d,
            u3nc_Stat.dro_d, u3nc_Stat.tra_d, u3nc_Stat.mut_d);
  }

  return pro;
}

/* _nc_regs_cb(): count the arguments of an n-ary program.
*/
static void
_nc_regs_cb(u3_noun kev, void* ptr_v)
{
  c3_w* his_w = ptr_v;

  if ( c3__mono != u3h(u3h(kev)) ) {
    c3_h arg_h = _nc_to(u3t(kev))->arg_h;

    his_w[c3_min(arg_h, 63)]++;
    his_w[64]++;
    his_w[65] += arg_h;
  }
}

/* u3nc_regs(): print a histogram of the argument counts of the n-ary
**              programs of this road and its seniors.
*/
void
u3nc_regs(void)
{
  c3_w      his_w[66] = {0};
  c3_w      i_w;
  u3a_road* rod_u;

  for ( rod_u = u3R; rod_u; rod_u = u3tn(u3a_road, rod_u->par_p) ) {
    u3h_walk_with(rod_u->ska.dir_p, _nc_regs_cb, his_w);
  }

  fprintf(stderr, "u3nc: %" PRIc3_w " n-ary programs, %" PRIc3_w
                  " arguments\r\n", his_w[64], his_w[65]);
  for ( i_w = 0; i_w < 64; i_w++ ) {
    if ( his_w[i_w] ) {
      fprintf(stderr, "  %" PRIc3_w "%s: %" PRIc3_w "\r\n",
              i_w, ( 63 == i_w ) ? "+" : "", his_w[i_w]);
    }
  }
}

#ifdef U3NC_STAT
/* _nc_hot: the programs u3nc_hot() collects, each once.
*/
typedef struct {
  u3nc_prog** pog_u;
  c3_w*       mug_w;              //  mug of its bell (as call sites print it)
  c3_w        len_w;
  c3_w        cap_w;
} _nc_hot;

static void
_nc_hot_add(_nc_hot* hot_u, u3nc_prog* pog_u, c3_w mug_w)
{
  c3_w i_w;

  for ( i_w = 0; i_w < hot_u->len_w; i_w++ ) {
    if ( hot_u->pog_u[i_w] == pog_u ) {
      if ( mug_w ) hot_u->mug_w[i_w] = mug_w;
      return;
    }
  }
  if ( hot_u->len_w == hot_u->cap_w ) {
    hot_u->cap_w = hot_u->cap_w ? 2 * hot_u->cap_w : 256;
    hot_u->pog_u = c3_realloc(hot_u->pog_u, hot_u->cap_w * sizeof(u3nc_prog*));
    hot_u->mug_w = c3_realloc(hot_u->mug_w, hot_u->cap_w * sizeof(c3_w));
  }
  hot_u->mug_w[hot_u->len_w] = mug_w;
  hot_u->pog_u[hot_u->len_w++] = pog_u;
}

static void
_nc_hot_dir_cb(u3_noun kev, void* ptr_v)
{
  u3_noun key = u3h(kev);
  u3_noun bel = ( (c3y == u3du(key)) && (c3__mono == u3h(key)) ) ? u3t(key) : key;

  _nc_hot_add(ptr_v, _nc_to(u3t(kev)), u3r_mug(bel));
}

static void
_nc_hot_ent_cb(u3_noun kev, void* ptr_v)
{
  u3_noun lis;

  for ( lis = u3t(kev); u3_nul != lis; lis = u3t(lis) ) {
    _nc_hot_add(ptr_v, _nc_to(u3t(u3h(lis))), 0);
  }
}

/* _nc_hof: the formulas of the entry programs, for u3nc_hot(): how many
**          programs each has, which an entry lookup scans every time, and
**          their activations.
*/
typedef struct {
  c3_w     len_w, cap_w;
  c3_w*    mug_w;
  c3_w*    num_w;
  c3_d*    act_d;
  u3_noun* kev;
} _nc_hof;

/* _nc_cape_size(): cells of a cape, the nodes u3d_match() walks; and its
**                  known leaves.
*/
static c3_d
_nc_cape_size(u3_noun cap, c3_d* yes_d)
{
  if ( c3y == u3ud(cap) ) {
    if ( 0 == cap ) (*yes_d)++;
    return 0;
  }
  return 1 + _nc_cape_size(u3h(cap), yes_d) + _nc_cape_size(u3t(cap), yes_d);
}

/* _nc_cape_axes(): the known leaves of a cape, by axis, the first num_w
**                  of them, in a list.  RETAINS cap.
*/
static void
_nc_cape_axes(u3_noun cap, u3_noun axe, c3_w* num_w, u3_noun* out)
{
  if ( !*num_w ) return;
  if ( c3y == u3ud(cap) ) {
    if ( 0 == cap ) {
      *out = u3nc(u3k(axe), *out);
      (*num_w)--;
    }
    return;
  }
  {
    u3_noun l = u3qc_peg(axe, 2), r = u3qc_peg(axe, 3);
    _nc_cape_axes(u3h(cap), l, num_w, out);
    _nc_cape_axes(u3t(cap), r, num_w, out);
    u3z(l); u3z(r);
  }
}

static void
_nc_hof_cb(u3_noun kev, void* ptr_v)
{
  _nc_hof* hof_u = ptr_v;
  c3_w     num_w = 0;
  c3_d     act_d = 0;
  u3_noun  lis;

  for ( lis = u3t(kev); u3_nul != lis; lis = u3t(lis) ) {
    num_w++;
    act_d += _nc_to(u3t(u3h(lis)))->ent_d;
  }
  if ( hof_u->len_w == hof_u->cap_w ) {
    hof_u->cap_w = hof_u->cap_w ? 2 * hof_u->cap_w : 64;
    hof_u->mug_w = c3_realloc(hof_u->mug_w, hof_u->cap_w * sizeof(c3_w));
    hof_u->num_w = c3_realloc(hof_u->num_w, hof_u->cap_w * sizeof(c3_w));
    hof_u->act_d = c3_realloc(hof_u->act_d, hof_u->cap_w * sizeof(c3_d));
    hof_u->kev   = c3_realloc(hof_u->kev, hof_u->cap_w * sizeof(u3_noun));
  }
  hof_u->mug_w[hof_u->len_w] = u3r_mug(u3h(kev));
  hof_u->num_w[hof_u->len_w] = num_w;
  hof_u->act_d[hof_u->len_w] = act_d;
  hof_u->kev[hof_u->len_w]   = kev;
  hof_u->len_w++;
}

/* _nc_hof_print(): the formulas whose entry programs ran the most, top
**                  num_w of them.
*/
static void
_nc_hof_print(c3_w num_w)
{
  _nc_hof   hof_u = {0};
  u3a_road* rod_u;
  c3_w      i_w, j_w, top_w;
  c3_d      act_d = 0, scn_d = 0;

  for ( rod_u = u3R; rod_u; rod_u = u3tn(u3a_road, rod_u->par_p) ) {
    u3h_walk_with(rod_u->ska.ent_p, _nc_hof_cb, &hof_u);
  }
  for ( i_w = 0; i_w < hof_u.len_w; i_w++ ) {
    act_d += hof_u.act_d[i_w];
    scn_d += hof_u.act_d[i_w] * hof_u.num_w[i_w];
  }
  fprintf(stderr, "u3nc entries: %" PRIc3_w " formulas, %" PRIu64
                  " activations of their programs, %" PRIu64
                  " programs scanned for them\r\n",
          hof_u.len_w, act_d, scn_d);

  //  selection of the top ones by activations
  //
  for ( top_w = 0; (top_w < num_w) && (top_w < hof_u.len_w); top_w++ ) {
    c3_w m_w = top_w;
    for ( j_w = top_w + 1; j_w < hof_u.len_w; j_w++ ) {
      if ( hof_u.act_d[j_w] > hof_u.act_d[m_w] ) m_w = j_w;
    }
    {
      c3_w mug_w = hof_u.mug_w[m_w], n_w = hof_u.num_w[m_w];
      c3_d a_d = hof_u.act_d[m_w];
      u3_noun k = hof_u.kev[m_w];
      hof_u.mug_w[m_w] = hof_u.mug_w[top_w];
      hof_u.num_w[m_w] = hof_u.num_w[top_w];
      hof_u.act_d[m_w] = hof_u.act_d[top_w];
      hof_u.kev[m_w]   = hof_u.kev[top_w];
      hof_u.mug_w[top_w] = mug_w;
      hof_u.num_w[top_w] = n_w;
      hof_u.act_d[top_w] = a_d;
      hof_u.kev[top_w]   = k;
    }
    fprintf(stderr, "u3nc entry formula %08x: %" PRIc3_w " programs, %" PRIu64
                    " activations\r\n",
            hof_u.mug_w[top_w], hof_u.num_w[top_w], hof_u.act_d[top_w]);
    //  the formula, when small, and the capes of its programs' socks: the
    //  nodes an entry lookup walks, and the first known leaves by axis
    //
    {
      u3_noun fol = u3h(hof_u.kev[top_w]), lis;
      u3_noun jam = u3qe_jam(fol);
      if ( u3r_met(3, jam) < 64 ) {
        u3m_p("    formula", fol);
      }
      else {
        fprintf(stderr, "    formula: %" PRIc3_w " bytes jammed\r\n",
                (c3_w)u3r_met(3, jam));
      }
      u3z(jam);
      for ( lis = u3t(hof_u.kev[top_w]); u3_nul != lis; lis = u3t(lis) ) {
        u3_noun cap = u3h(u3h(u3h(lis)));
        c3_d    yes_d = 0;
        c3_d    siz_d = _nc_cape_size(cap, &yes_d);
        c3_w    n_w = 40;
        u3_noun out = u3_nul;
        fprintf(stderr, "    sock cape: %" PRIu64 " cells, %" PRIu64
                        " known leaves\r\n", siz_d, yes_d);
        _nc_cape_axes(cap, 1, &n_w, &out);
        u3m_p("    first known axes (reversed)", out);
        u3z(out);
      }
    }
  }

  c3_free(hof_u.mug_w);
  c3_free(hof_u.num_w);
  c3_free(hof_u.act_d);
  c3_free(hof_u.kev);
}

static int
_nc_hot_cmp(const void* a_v, const void* b_v)
{
  const u3nc_prog* a_u = *(u3nc_prog* const*)a_v;
  const u3nc_prog* b_u = *(u3nc_prog* const*)b_v;

  return ( a_u->con_d < b_u->con_d ) - ( a_u->con_d > b_u->con_d );
}

/* u3nc_hot(): the num_w programs of this road and its seniors that made
**             the most cells since they were compiled, the first dum_w
**             of them in full (-Dska-stat).
*/
void
u3nc_hot(c3_w num_w, c3_w dum_w)
{
  _nc_hot   hot_u = {0};
  c3_d      con_d = 0, ent_d = 0;
  c3_w      i_w;
  u3a_road* rod_u;

  for ( rod_u = u3R; rod_u; rod_u = u3tn(u3a_road, rod_u->par_p) ) {
    u3h_walk_with(rod_u->ska.dir_p, _nc_hot_dir_cb, &hot_u);
    u3h_walk_with(rod_u->ska.ent_p, _nc_hot_ent_cb, &hot_u);
  }
  //  sort both arrays by the programs' cells: sort an index
  //
  {
    c3_w*       idx_w = c3_malloc(hot_u.len_w * sizeof(c3_w) + 1);
    u3nc_prog** pog_u = c3_malloc(hot_u.len_w * sizeof(u3nc_prog*) + 1);
    c3_w*       mug_w = c3_malloc(hot_u.len_w * sizeof(c3_w) + 1);

    for ( i_w = 0; i_w < hot_u.len_w; i_w++ ) {
      pog_u[i_w] = hot_u.pog_u[i_w];
    }
    qsort(pog_u, hot_u.len_w, sizeof(u3nc_prog*), _nc_hot_cmp);
    for ( i_w = 0; i_w < hot_u.len_w; i_w++ ) {
      c3_w j_w;
      for ( j_w = 0; hot_u.pog_u[j_w] != pog_u[i_w]; j_w++ );
      mug_w[i_w] = hot_u.mug_w[j_w];
    }
    c3_free(idx_w);
    c3_free(hot_u.pog_u);
    c3_free(hot_u.mug_w);
    hot_u.pog_u = pog_u;
    hot_u.mug_w = mug_w;
  }

  for ( i_w = 0; i_w < hot_u.len_w; i_w++ ) {
    con_d += hot_u.pog_u[i_w]->con_d;
    ent_d += hot_u.pog_u[i_w]->ent_d;
  }
  fprintf(stderr, "u3nc hot: %" PRIc3_w " programs, %" PRIu64 " activations, "
                  "%" PRIu64 " cells\r\n", hot_u.len_w, ent_d, con_d);
  _nc_hof_print(10);

  for ( i_w = 0; (i_w < num_w) && (i_w < hot_u.len_w); i_w++ ) {
    u3nc_prog* pog_u = hot_u.pog_u[i_w];

    fprintf(stderr, "u3nc hot %" PRIc3_w ": %p, bell %08x, %u bytes, %u slots, "
                    "%u args, "
                    "%" PRIu64 " activations, %" PRIu64 " cells (%.1f each), "
                    "%u call sites\r\n",
            i_w, (void*)pog_u, hot_u.mug_w[i_w],
            pog_u->byc_u.len_h, pog_u->tot_h, pog_u->arg_h,
            pog_u->ent_d, pog_u->con_d,
            pog_u->ent_d ? (double)pog_u->con_d / pog_u->ent_d : 0.0,
            pog_u->dir_u.len_h);
    if ( i_w < dum_w ) {
      _nc_print(pog_u);
    }
  }

  c3_free(hot_u.pog_u);
  c3_free(hot_u.mug_w);
}
#endif

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
  u3z(pog_u->jet);

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
  gop_u->jet   = u3a_take(pog_u->jet);

  memcpy(gop_u->byc_u.ops_y, pog_u->byc_u.ops_y, pog_u->byc_u.len_h);
  memcpy(gop_u->sot_u.sot_h, pog_u->sot_u.sot_h,
         pog_u->sot_u.len_h * sizeof(c3_h));
  memcpy(gop_u->sot_u.kon_y, pog_u->sot_u.kon_y,
         pog_u->sot_u.len_h * sizeof(c3_y));
#ifdef U3NC_STAT
  gop_u->bor_d = pog_u->bor_d;
  memcpy(gop_u->ali_y, pog_u->ali_y, pog_u->byc_u.len_h + 1);
#endif

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
  tot_w += u3a_mark_noun(pog_u->jet);

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

/* _nc_drop_dir(): free the program under a key of ska.dir_p, if there is
**                 one.  TRANSFERS.
*/
static void
_nc_drop_dir(u3_noun key)
{
  u3_weak pog = u3h_git(u3R->ska.dir_p, key);

  if ( u3_none != pog ) {
    _nc_prog_free(_nc_to(pog));
    u3h_del(u3R->ska.dir_p, key);
  }

  u3z(key);
}

/* _nc_drop_ent(): free the entry program of a bell, if there is one.  RETAINS.
*/
static void
_nc_drop_ent(u3_noun bell)
{
  u3_weak lis = u3h_git(u3R->ska.ent_p, u3t(bell));
  u3_noun out = u3_nul, i;

  if ( u3_none == lis ) {
    return;
  }

  for ( ; u3_nul != lis; lis = u3t(lis) ) {
    i = u3h(lis);

    if ( c3y == u3r_sing(u3h(i), u3h(bell)) ) {
      _nc_prog_free(_nc_to(u3t(i)));
    }
    else {
      out = u3nc(u3k(i), out);
    }
  }

  u3h_put(u3R->ska.ent_p, u3t(bell), out);
}

/* u3nc_drop(): free the programs of functions on the home road, lis a
**              (list bell) that holds the callers of all of them, so no
**              program is left linked to a dropped one.  RETAINS.
*/
void
u3nc_drop(u3_noun lis)
{
  u3_assert( u3R == &(u3H->rod_u) );

  for ( ; u3_nul != lis; lis = u3t(lis) ) {
    _nc_drop_dir(_nc_dir_key(u3h(lis), c3n));
    _nc_drop_dir(_nc_dir_key(u3h(lis), c3y));
    _nc_drop_ent(u3h(lis));
  }
}

/* _nc_stale_prog(): put in the set set_p the rings of the sites of a
**                   program compiled trusting a jet arm that isn't trusted
**                   now, or checking one that is; and of the ops standing
**                   in for jets, which trust them, that aren't trusted now.
*/
static void
_nc_stale_prog(u3p(u3h_root) set_p, u3nc_prog* pog_u)
{
  u3_noun lis;
  c3_h    i_h;

  for ( lis = pog_u->jet; u3_nul != lis; lis = u3t(lis) ) {
    u3j_harm*       ham_u;
    const u3u_harm* arm_u;

    if (  (c3y == u3j_ring(u3h(lis), &ham_u, &arm_u))
       && (c3n == ham_u->ice) )
    {
      u3h_put(set_p, u3h(lis), u3_nul);
    }
  }

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[i_h]);

    if (  dir_u->arm_u && !dir_u->arm_u->pun_t && (c3n == dir_u->mon_o)
       && (dir_u->ice_o != dir_u->ham_u->ice) )
    {
      u3h_put(set_p, dir_u->ring, u3_nul);
    }
  }
}

static void
_nc_stale_dir_cb(u3_noun kev, void* ptr_v)
{
  _nc_stale_prog(*(u3p(u3h_root)*)ptr_v, _nc_to(u3t(kev)));
}

static void
_nc_stale_ent_cb(u3_noun kev, void* ptr_v)
{
  u3_noun lis = u3t(kev), i;

  while ( u3_nul != lis ) {
    u3x_cell(lis, &i, &lis);
    _nc_stale_prog(*(u3p(u3h_root)*)ptr_v, _nc_to(u3t(i)));
  }
}

static void
_nc_stale_set_cb(u3_noun kev, void* ptr_v)
{
  u3_noun* lis = ptr_v;
  *lis = u3nc(u3k(u3h(kev)), *lis);
}

/* u3nc_stale(): the rings of the total array jets whose trust changed since
**               the programs on the home road compiled calls through them.
**               Produces a (list ring).
*/
u3_noun
u3nc_stale(void)
{
  u3p(u3h_root) set_p = u3h_new();
  u3_noun       lis   = u3_nul;

  u3_assert( u3R == &(u3H->rod_u) );

  u3h_walk_with(u3R->ska.dir_p, _nc_stale_dir_cb, &set_p);
  u3h_walk_with(u3R->ska.ent_p, _nc_stale_ent_cb, &set_p);
  u3h_walk_with(set_p, _nc_stale_set_cb, &lis);
  u3h_free(set_p);

  return lis;
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
  if ( !_nc_here(pog_u, u3R) ) return c3n;
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