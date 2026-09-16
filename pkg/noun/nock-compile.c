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
**  slot.
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
**    CAL   i d      call site i with its arguments -> d
**    CAM   i d      CAL, memoized
**    CSL   i s d    call site i with the whole subject in s -> d
**    CSM   i s d    CSL, memoized
**    CLQ   s t      goto t unless s is a cell
**    EQQ   l r t    goto t unless =(l r)
**    EQI   n s t    goto t unless =(n s), n a direct atom
**    EQL   i s t    goto t unless =(literal i, s)
**    BRN   s t      goto t if s is 1, crash unless s is a loobean
**    BRZ   s t      goto t unless s is 0
**    HOP   t        goto t
**    JMP   i        CAL in tail position
**    JSP   i s      CSL in tail position
**    DON   s        return s
**    BOM            crash
*/
#define X3(op) X(op##_B) X(op##_S) X(op##_V)
#define OPCODES                                                          \
  X(IMM_0) X(IMM_1) X(IMM_B) X(IMM_S)                                    \
  X3(IML) X3(MOV) X3(INC) X3(DEC) X3(ADD) X3(CON) X3(HED) X3(TAL)         \
  X3(CEL) X3(LOB)                                                        \
  X3(EQU) X3(HSP) X3(HSE) X3(HDP) X3(HDE) X3(SPY) X3(NOK)                 \
  X3(CAL) X3(CAM) X3(CSL) X3(CSM)                                        \
  X3(CLQ) X3(EQQ) X3(EQI) X3(EQL) X3(BRN) X3(BRZ) X3(HOP) X3(JMP) X3(JSP) \
  X3(DON)                                                                \
  X(BOM)

#define X(op) op,
enum { OPCODES LAST };
#undef X

#define X(op) #op,
static const c3_c* _nc_name_c[] = { OPCODES };
#undef X

/*  Families of the IR ops, in the order of the widened opcodes. Must be in the 
**  same order as X3 definitions in OPCODES macro.
**  Final families are bom and imm for special-cased ops and nop for marking
**  deleted ops.
*/
enum {
  _nc_iml, _nc_mov, _nc_inc, _nc_dec, _nc_add, _nc_con, _nc_hed, _nc_tal,
  _nc_cel, _nc_lob,
  _nc_equ, _nc_hsp, _nc_hse, _nc_hdp, _nc_hde, _nc_spy, _nc_nok,
  _nc_cal, _nc_cam, _nc_csl, _nc_csm,
  _nc_clq, _nc_eqq, _nc_eqi, _nc_eql, _nc_brn, _nc_brz, _nc_hop, _nc_jmp,
  _nc_jsp, _nc_don,
  _nc_bom, _nc_imm, _nc_nop
};

//  maps an IR bytecode family to its first opcode. Valid because X3-defined
//  opcodes are kept together.
#define _nc_base(fam)  (IML_B + (3 * (fam)))

#define _nc_none       ((c3_h)-1)

//  layout of an op:
//  [OP][source_registers?][destination_register?][index?][jump_target?]

/* _nc_fam: operands of a family: number of source registers, whether
**          there is a destination register, an index immediate (literal
**          or call site), and a jump target.
*/
static const struct { c3_y src_y, dst_y, imm_y, tar_y; } _nc_fam[] = {
  [_nc_iml] = { 0, 1, 1, 0 },
  [_nc_mov] = { 1, 1, 0, 0 },
  [_nc_inc] = { 1, 1, 0, 0 },
  [_nc_dec] = { 1, 1, 0, 0 },
  [_nc_add] = { 2, 1, 0, 0 },
  [_nc_con] = { 2, 1, 0, 0 },
  [_nc_hed] = { 1, 1, 0, 0 },
  [_nc_tal] = { 1, 1, 0, 0 },
  [_nc_cel] = { 1, 0, 0, 0 },
  [_nc_lob] = { 1, 0, 0, 0 },
  [_nc_equ] = { 2, 0, 0, 0 },
  [_nc_hsp] = { 0, 0, 1, 0 },
  [_nc_hse] = { 0, 0, 1, 0 },
  [_nc_hdp] = { 1, 0, 1, 0 },
  [_nc_hde] = { 1, 0, 1, 0 },
  [_nc_spy] = { 2, 1, 0, 0 },
  [_nc_nok] = { 2, 1, 0, 0 },
  [_nc_cal] = { 0, 1, 1, 0 },
  [_nc_cam] = { 0, 1, 1, 0 },
  [_nc_csl] = { 1, 1, 1, 0 },
  [_nc_csm] = { 1, 1, 1, 0 },
  [_nc_clq] = { 1, 0, 0, 1 },
  [_nc_eqq] = { 2, 0, 0, 1 },
  [_nc_eqi] = { 1, 0, 1, 1 },
  [_nc_eql] = { 1, 0, 1, 1 },
  [_nc_brn] = { 1, 0, 0, 1 },
  [_nc_brz] = { 1, 0, 0, 1 },
  [_nc_hop] = { 0, 0, 0, 1 },
  [_nc_jmp] = { 0, 0, 1, 0 },
  [_nc_jsp] = { 1, 0, 1, 0 },
  [_nc_don] = { 1, 0, 0, 0 },
  [_nc_bom] = { 0, 0, 0, 0 },
  [_nc_imm] = { 0, 1, 0, 0 },
  [_nc_nop] = { 0, 0, 0, 0 },
};

/* nc_op: an IR op, with registers until slots are assigned.
*/
typedef struct {
  c3_y  fam_y;    //  family of the op
  c3_h  src_h[2]; //  source registers
  c3_h  dst_h;    //  destination register
  c3_h  imm_h;    //  index immediate, or the atom of an inline immediate
  c3_h  tar_h;    //  target block, or the literal of an inline immediate
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

/* nc_dir: a call site, with registers until slots are assigned.
*/
typedef struct {
  u3_noun bell;
  u3_noun ring;
  c3_h    cid_h;
  c3_o    dir_o;  //  call with args or with the whole subject
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
  u3p(u3h_root) lit_p;    //  literal -> index
  c3_h          lit_h;
  c3_h          reg_h;    //  registers
  c3_h          tot_h;    //  slots
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
#else
#  define _nc_stat(fel)  ((void)0)
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

/* _nc_op(): append an op.
*/
static nc_op*
_nc_op(nc_gen* gen_u, c3_y fam_y)
{
  nc_op* op_u;

  _nc_grow(gen_u->ops_u, gen_u->opn_h, gen_u->opc_h, nc_op);
  op_u = &(gen_u->ops_u[gen_u->opn_h++]);

  op_u->fam_y    = fam_y;
  op_u->src_h[0] = _nc_none;
  op_u->src_h[1] = _nc_none;
  op_u->dst_h    = _nc_none;
  op_u->imm_h    = 0;
  op_u->tar_h    = _nc_none;

  return op_u;
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

/* _nc_jet_op(): an op standing in for a jetted call with arguments,
**               for the jets the interpreter knows as ops: +dec and +add.
**               ring: [path axis] of the jet; arg: (list register).
**               RETAINS.  Produces NULL if there is no such op.
*/
static nc_op*
_nc_jet_op(nc_gen* gen_u, u3_noun ring, u3_noun arg)
{
  u3j_harm*       ham_u;
  const u3u_harm* arm_u;
  nc_op*          op_u;
  c3_h            len_h = 0;

  for ( u3_noun l = arg; u3_nul != l; l = u3t(l) ) {
    len_h++;
  }

  if (  (c3n == u3j_ring(ring, &ham_u, &arm_u))
     || !arm_u
     || (len_h != arm_u->len_w) )
  {
    return NULL;
  }

  if ( (u3ua_dec == arm_u->arg_f) && (1 == len_h) ) {
    op_u = _nc_op(gen_u, _nc_dec);
    op_u->src_h[0] = _nc_reg(gen_u, u3h(arg));
  }
  else if ( (u3ua_add == arm_u->arg_f) && (2 == len_h) ) {
    op_u = _nc_op(gen_u, _nc_add);
    op_u->src_h[0] = _nc_reg(gen_u, u3h(arg));
    op_u->src_h[1] = _nc_reg(gen_u, u3h(u3t(arg)));
  }
  else {
    return NULL;
  }

  _nc_stat(arm_d);
  return op_u;
}

/* _nc_dir(): append a call site.  RETAINS.
**            bell: callee; ring: jet, or ~; clu: memo clue, or u3_none;
**            arg: (list register) or, for a call by subject, u3_none.
*/
static c3_h
_nc_dir(nc_gen* gen_u, u3_noun bell, u3_noun ring, u3_weak clu, u3_weak arg)
{
  nc_dir* dir_u;

  _nc_grow(gen_u->dir_u, gen_u->din_h, gen_u->dir_h, nc_dir);
  dir_u = &(gen_u->dir_u[gen_u->din_h]);

  dir_u->bell  = u3k(bell);
  dir_u->ring  = u3k(ring);
  dir_u->cid_h = ( u3_none == clu ) ? 0 : _nc_cid(clu);
  dir_u->dir_o = __(u3_none != arg);
  dir_u->sot_h = gen_u->pon_h;
  dir_u->len_h = 0;

  while ( (u3_none != arg) && (u3_nul != arg) ) {
    u3_noun i;
    u3x_cell(arg, &i, &arg);
    _nc_grow(gen_u->pol_h, gen_u->pon_h, gen_u->poc_h, c3_h);
    gen_u->pol_h[gen_u->pon_h++] = _nc_reg(gen_u, i);
    dir_u->len_h++;
  }

  return gen_u->din_h++;
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
      op_u->imm_h = _nc_dir(gen_u, a, u3_nul, u3_none, b);
      op_u->dst_h = _nc_reg(gen_u, c);
    } break;

    case c3__caf: {
      u3x_qual(arg, &a, &b, &c, &d);
      if ( !(op_u = _nc_jet_op(gen_u, d, b)) ) {
        op_u = _nc_op(gen_u, _nc_cal);
        op_u->imm_h = _nc_dir(gen_u, a, d, u3_none, b);
      }
      op_u->dst_h = _nc_reg(gen_u, c);
    } break;

    case c3__cam: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_cam);
      op_u->imm_h = _nc_dir(gen_u, a, u3_nul, d, b);
      op_u->dst_h = _nc_reg(gen_u, c);
    } break;

    case c3__csl: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_csl);
      op_u->imm_h    = _nc_dir(gen_u, a, u3_nul, u3_none, u3_none);
      op_u->src_h[0] = _nc_reg(gen_u, b);
      op_u->dst_h    = _nc_reg(gen_u, c);
    } break;

    case c3__csf: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_csl);
      op_u->imm_h    = _nc_dir(gen_u, a, d, u3_none, u3_none);
      op_u->src_h[0] = _nc_reg(gen_u, b);
      op_u->dst_h    = _nc_reg(gen_u, c);
    } break;

    case c3__csm: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_csm);
      op_u->imm_h    = _nc_dir(gen_u, a, u3_nul, d, u3_none);
      op_u->src_h[0] = _nc_reg(gen_u, b);
      op_u->dst_h    = _nc_reg(gen_u, c);
    } break;
  }
}

/* _nc_jump(): translate a jump [args there] to a block, with its
**             arguments moved into the parameters of the target.
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
      u3x_cell(arg, &a, &b);
      op_u = _nc_op(gen_u, _nc_jmp);
      op_u->imm_h = _nc_dir(gen_u, a, u3_nul, u3_none, b);
      return;
    }

    case c3__jmf: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_jmp);
      op_u->imm_h = _nc_dir(gen_u, a, c, u3_none, b);
      return;
    }

    case c3__jsp: {
      u3x_cell(arg, &a, &b);
      op_u = _nc_op(gen_u, _nc_jsp);
      op_u->imm_h    = _nc_dir(gen_u, a, u3_nul, u3_none, u3_none);
      op_u->src_h[0] = _nc_reg(gen_u, b);
      return;
    }

    case c3__jsf: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_jsp);
      op_u->imm_h    = _nc_dir(gen_u, a, c, u3_none, u3_none);
      op_u->src_h[0] = _nc_reg(gen_u, b);
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
    if (  (_nc_cal == _nfam) || (_nc_cam == _nfam) || (_nc_jmp == _nfam) ) { \
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

/* _nc_slots(): assign the registers to slots, rewriting the ops.
**
**   The CFG is a DAG, so the live range of a register lies within its
**   interval of ops in layout order, from its definition to its last use.
**   A parameter of a merging block is defined by a move at each of its
**   predecessors, so its interval starts at the first of them; then no
**   move into a parameter aliases a register read by a later one.
**   Intervals are assigned slots by linear scan; two intervals share a
**   slot only if one ends strictly before the other starts, so an op
**   never writes the slot it reads.
*/
static void
_nc_slots(nc_gen* gen_u, c3_h arg_h)
{
  c3_h  reg_h = c3_max(gen_u->reg_h, arg_h);
  c3_h* sta_h = u3a_malloc(reg_h * sizeof(c3_h));
  c3_h* end_h = u3a_calloc(reg_h, sizeof(c3_h));
  c3_h* use_h = u3a_calloc(reg_h, sizeof(c3_h));
  c3_h* sot_h = u3a_malloc(reg_h * sizeof(c3_h));
  c3_d* key_d = u3a_malloc(reg_h * sizeof(c3_d));
  c3_h* act_h = u3a_malloc(reg_h * sizeof(c3_h));
  c3_h* fre_h = u3a_malloc(reg_h * sizeof(c3_h));
  c3_h  key_n_h = 0, act_n_h = 0, fre_n_h = 0, tot_h = 0;
  c3_h  i_h, r_h;

  for ( r_h = 0; r_h < reg_h; r_h++ ) {
    sta_h[r_h] = ( r_h < arg_h ) ? 0 : _nc_none;
    sot_h[r_h] = _nc_none;
  }

  //  uses; moves into unused parameters and unused immediates are dropped
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);
    _nc_srcs(gen_u, op_u, reg_h, { use_h[*reg_h]++; });
  }

  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_h]);
    if (  ( (_nc_mov == op_u->fam_y)
         || (_nc_imm == op_u->fam_y)
         || (_nc_iml == op_u->fam_y) )
       && !use_h[op_u->dst_h] )
    {
      op_u->fam_y = _nc_nop;
    }
  }

  //  intervals
  //
  for ( i_h = 0; i_h < gen_u->opn_h; i_h++ ) {
    nc_op* op_u  = &(gen_u->ops_u[i_h]);
    c3_h   pos_h = i_h + 1;

    if ( _nc_nop == op_u->fam_y ) {
      continue;
    }

    _nc_srcs(gen_u, op_u, reg_h, {
      if ( _nc_none == sta_h[*reg_h] ) {
        sta_h[*reg_h] = 0;
      }
      end_h[*reg_h] = c3_max(end_h[*reg_h], pos_h);
    });

    if ( _nc_none != op_u->dst_h ) {
      if ( _nc_none == sta_h[op_u->dst_h] ) {
        sta_h[op_u->dst_h] = pos_h;
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
    c3_h j_h;

    r_h = (c3_h)key_d[i_h];

    //  iterate over all active slots, freeing the stale ones by
    //  putting them in the free list and rewriting the slot in act_h
    //  with the item from the end. The replacing item is not yet examined
    //  so we advance j_h in the else branch only
    for ( j_h = 0; j_h < act_n_h; ) {
      if ( end_h[act_h[j_h]] < sta_h[r_h] ) {
        fre_h[fre_n_h++] = sot_h[act_h[j_h]];
        act_h[j_h]     = act_h[--act_n_h];
      }
      else {
        j_h++;
      }
    }

    sot_h[r_h]     = fre_n_h ? fre_h[--fre_n_h] : tot_h++;
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
  u3a_free(use_h);
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
    case _nc_nop: {
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
    buf_y[0] = _nc_base(fam_y) + wid_y;
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
            ( c3y == dir_u->dir_o ) ? "direct" : "subject",
            dir_u->ham_u ? " jet" : "",
            dir_u->arm_u ? " array-jet" : "",
            dir_u->cid_h);

    for ( j_h = 0; j_h < dir_u->len_h; j_h++ ) {
      fprintf(stderr, " %u", pog_u->sot_u.sot_h[dir_u->sot_h + j_h]);
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

    fprintf(stderr, "  %4u: %-6s", off_h, _nc_name_c[cod_y]);

    if ( cod_y < IML_B ) {
      if ( IMM_B == cod_y ) {
        fprintf(stderr, " %u", _nc_read(pog, &ip_h, 0));
      }
      else if ( IMM_S == cod_y ) {
        fprintf(stderr, " %u", _nc_read(pog, &ip_h, 1));
      }
      fprintf(stderr, " -> %u", _nc_read(pog, &ip_h, 0));
    }
    else if ( BOM != cod_y ) {
      c3_y fam_y = (cod_y - IML_B) / 3;
      c3_y wid_y = (cod_y - IML_B) % 3;

      if ( _nc_fam[fam_y].imm_y ) {
        fprintf(stderr, " #%u", _nc_read(pog, &ip_h, wid_y));
      }
      for ( i_h = 0; i_h < _nc_fam[fam_y].src_y; i_h++ ) {
        fprintf(stderr, " %u", _nc_read(pog, &ip_h, wid_y));
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
}

/* _nc_uniq(): yes if the len_h slots in sot_h are all distinct.
*/
static c3_o
_nc_uniq(const c3_h* sot_h, c3_h len_h)
{
  for ( c3_h i_h = 1; i_h < len_h; i_h++ ) {
    for ( c3_h j_h = 0; j_h < i_h; j_h++ ) {
      if ( sot_h[i_h] == sot_h[j_h] ) {
        return c3n;
      }
    }
  }
  return c3y;
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

/* _nc_dire_jet(): find the jet arms of a call site.
*/
static void
_nc_dire_jet(u3nc_dire* dir_u)
{
  dir_u->ham_u = NULL;
  dir_u->arm_u = NULL;

  if ( u3_nul == dir_u->ring ) {
    return;
  }

  if ( c3n == u3j_ring(dir_u->ring, &(dir_u->ham_u), &(dir_u->arm_u)) ) {
    _nc_stat(rin_d);
    if ( _nc_verb_t ) {
      u3l_log("u3nc: no driver for ring %s", u3m_pretty_path(u3h(dir_u->ring)));
    }
  }
  else {
    _nc_stat(arm_d);
  }
}

/* _nc_emit(): lay the ops out and assemble the program.  RETAINS ned.
*/
static u3nc_prog*
_nc_emit(nc_gen* gen_u, u3_noun ned, c3_h arg_h)
{
  u3nc_prog* pog_u;
  c3_h       byc_h, i_h, j_h;

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

  for ( i_h = 0; i_h < gen_u->din_h; i_h++ ) {
    nc_dir*    dir_u = &(gen_u->dir_u[i_h]);
    u3nc_dire* dat_u = &(pog_u->dir_u.dat_u[i_h]);

    dat_u->bell  = dir_u->bell;
    dat_u->ring  = dir_u->ring;
    dat_u->pog_p = 0;
    dat_u->sot_h = dir_u->sot_h;
    dat_u->len_h = dir_u->len_h;
    dat_u->cid_h = dir_u->cid_h;
    dat_u->dir_o = dir_u->dir_o;
    dat_u->uni_o = _nc_uniq(gen_u->pol_h + dir_u->sot_h, dir_u->len_h);
    _nc_dire_jet(dat_u);
  }

  memcpy(pog_u->sot_u.sot_h, gen_u->pol_h, gen_u->pon_h * sizeof(c3_h));

  return pog_u;
}

/* _nc_build(): compile a straight [need n-args blocks].
*/
static u3nc_prog*
_nc_build(u3_noun straight)
{
  nc_gen     gen_u = {0};
  u3nc_prog* pog_u;
  u3_noun    ned, arg, map;

  u3x_trel(straight, &ned, &arg, &map);

  c3_h arg_h = _nc_cat(arg);

  gen_u.idx_p = u3h_new();
  gen_u.lit_p = u3h_new();

  _nc_blocks(&gen_u, map);
  _nc_translate(&gen_u);
  _nc_fold(&gen_u);
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

/* _nc_dir_get(): direct program of a bell, from any road.  RETAINS.
*/
static u3nc_prog*
_nc_dir_get(u3_noun bell)
{
  u3_weak pog;
  for (u3_road* rod_u = u3R; rod_u; rod_u = u3tn(u3_road, rod_u->par_p)) {
    if ( u3_none != (pog = u3h_git(rod_u->ska.dir_p, bell)) ) {
      return _nc_to(pog);
    }
  }

  return NULL;
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

/* _nc_link(): set the callee programs of the direct call sites that
**             have been compiled; the rest are linked when first called.
*/
static void
_nc_link(u3nc_prog* pog_u)
{
  c3_h i_h;

  for ( i_h = 0; i_h < pog_u->dir_u.len_h; i_h++ ) {
    u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[i_h]);
    u3nc_prog* gop_u;

    if ( (c3y == dir_u->dir_o) && (gop_u = _nc_dir_get(dir_u->bell)) ) {
      dir_u->pog_p = u3of(u3nc_prog, gop_u);
    }
  }
}

/* _nc_compile(): compile a straight and link it.  TRANSFERS.
*/
static u3nc_prog*
_nc_compile(u3_noun straight)
{
  u3nc_prog* pog_u = _nc_build(straight);
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

/* _nc_callee(): the program of a direct call site, compiling it on the
**               first call.  The site remembers it if the site is on the
**               current road; a senior site can't point at junior memory.
**               The interpreter checks the linked case inline; this is
**               the slow path, and doesn't clobber the caller's registers.
*/
static _nc_cold u3nc_prog*
_nc_callee(u3nc_dire* dir_u)
{
  u3nc_prog* gop_u;

  if ( dir_u->pog_p ) {
    return u3to(u3nc_prog, dir_u->pog_p);
  }

  if ( !(gop_u = _nc_dir_get(dir_u->bell)) ) {
    gop_u = _nc_compile(u3d_dire(dir_u->bell));
    u3h_put(u3R->ska.dir_p, dir_u->bell, _nc_of(gop_u));
  }

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

    pog_u = _nc_compile(u3d_full(sub, fol, &bell));

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

/* _nc_save(): save the product of a memoized call, as nock.c does.
**             RETAINS.
*/
static void
_nc_save(c3_h cid_h, u3_noun key, u3_noun pro)
{
  if ( (u3z_memo_ford == cid_h) && (0 == u3R->ski.gul) ) {
    u3z_save_m(cid_h, 136 + c3__ford, key, pro);
  }
  else if ( (u3z_memo_toss == cid_h)
            ? (&(u3H->rod_u) != u3R)
            : (0 == u3R->ski.gul) )
  {
    u3z_save_m(cid_h, 144 + c3__nock, key, pro);
  }
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

/* _nc_burn_north(), _nc_burn_south(): run a program on its arguments.
**   TRANSFERS the arguments.  Body in nock-compile-burn.h, instantiated
**   once per road direction.
*/
#define _nc_mov_ws   (-1)
#define _nc_burn  _nc_burn_north
#include "nock-compile-burn.h"
#undef _nc_mov_ws
#undef _nc_burn

#define _nc_mov_ws   1
#define _nc_burn  _nc_burn_south
#include "nock-compile-burn.h"
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
                    "%" PRIu64 " jet hits\r\n",
            u3nc_Stat.ent_d, u3nc_Stat.com_d, u3nc_Stat.arm_d,
            u3nc_Stat.rin_d, u3nc_Stat.dir_d, u3nc_Stat.sub_d,
            u3nc_Stat.jet_d);
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
