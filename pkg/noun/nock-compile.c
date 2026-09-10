/// @file

#include "nock-compile.h"

#include "allocate.h"
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
  X3(IML) X3(MOV) X3(INC) X3(CON) X3(HED) X3(TAL) X3(CEL) X3(LOB)         \
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

/*  Families of the IR ops, in the order of the widened opcodes.
*/
enum {
  _nc_iml, _nc_mov, _nc_inc, _nc_con, _nc_hed, _nc_tal, _nc_cel, _nc_lob,
  _nc_equ, _nc_hsp, _nc_hse, _nc_hdp, _nc_hde, _nc_spy, _nc_nok,
  _nc_cal, _nc_cam, _nc_csl, _nc_csm,
  _nc_clq, _nc_eqq, _nc_eqi, _nc_eql, _nc_brn, _nc_brz, _nc_hop, _nc_jmp,
  _nc_jsp, _nc_don,
  _nc_bom, _nc_imm, _nc_nop
};

#define _nc_base(fam)  (IML_B + (3 * (fam)))
#define _nc_none       ((c3_w)-1)

/* _nc_fam: operands of a family: number of source registers, whether
**          there is a destination register, an index immediate (literal
**          or call site), and a jump target.
*/
static const struct { c3_y src_y, dst_y, imm_y, tar_y; } _nc_fam[] = {
  [_nc_iml] = { 0, 1, 1, 0 },
  [_nc_mov] = { 1, 1, 0, 0 },
  [_nc_inc] = { 1, 1, 0, 0 },
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
  c3_y  fam_y;
  c3_y  hop_y;      //  moves a hop argument: dropped if the parameter is unused
  c3_w  src_w[2];
  c3_w  dst_w;
  c3_w  imm_w;      //  index immediate, or the atom of an inline immediate
  c3_w  tar_w;      //  target block, or the literal of an inline immediate
} nc_op;

/* nc_blk: a basic block.
*/
typedef struct {
  u3_noun blob;
  c3_w    fir_w;    //  first op
  c3_w    len_w;    //  number of ops
  c3_w    off_w;    //  byte offset
  c3_w    lay_w;    //  position in the layout, or _nc_none if unreachable
} nc_blk;

/* nc_dir: a call site, with registers until slots are assigned.
*/
typedef struct {
  u3_noun bell;
  u3_noun ring;
  c3_w    cid_w;
  c3_o    dir_o;
  c3_w    sot_w;
  c3_w    len_w;
} nc_dir;

/* nc_gen: state of a compilation.
*/
typedef struct {
  u3p(u3h_root) idx_p;    //  block id -> index
  nc_blk*       blk_u;
  c3_w          blk_w;
  c3_w*         lay_w;    //  block indices in layout order
  c3_w          lay_n;
  nc_op*        ops_u;
  c3_w          ops_w, ops_n;
  nc_dir*       dir_u;
  c3_w          dir_w, dir_n;
  c3_w*         pol_w;    //  argument registers of call sites
  c3_w          pol_n, pol_m;
  u3p(u3h_root) lit_p;    //  literal -> index
  c3_w          lit_w;
  c3_w          reg_w;    //  registers
  c3_w          tot_w;    //  slots
} nc_gen;

#define _nc_grow(arr, len, cap, typ)                                     \
  if ( (len) >= (cap) ) {                                                \
    c3_w _old = (cap);                                                   \
    (cap) = (cap) ? (2 * (cap)) : 64;                                     \
    (arr) = u3a_realloc((arr), _old * sizeof(typ), (cap) * sizeof(typ)); \
  }

/* _nc_cat(): direct atom, or fail.
*/
static inline c3_w
_nc_cat(u3_noun som)
{
  if ( c3n == u3a_is_cat(som) ) {
    u3m_bail(c3__fail);
  }
  return som;
}

/* _nc_reg(): register of an IR op.
*/
static c3_w
_nc_reg(nc_gen* gen_u, u3_noun som)
{
  c3_w reg_w = _nc_cat(som);
  gen_u->reg_w = c3_max(gen_u->reg_w, reg_w + 1);
  return reg_w;
}

/* _nc_lit(): index of a literal.  RETAINS.
*/
static c3_w
_nc_lit(nc_gen* gen_u, u3_noun som)
{
  u3_weak got = u3h_git(gen_u->lit_p, som);

  if ( u3_none == got ) {
    got = gen_u->lit_w++;
    u3h_put(gen_u->lit_p, som, got);
  }

  return got;
}

/* _nc_blk(): index of a block by id.  RETAINS.
*/
static c3_w
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

  _nc_grow(gen_u->ops_u, gen_u->ops_n, gen_u->ops_w, nc_op);
  op_u = &(gen_u->ops_u[gen_u->ops_n++]);

  op_u->fam_y    = fam_y;
  op_u->hop_y    = 0;
  op_u->src_w[0] = _nc_none;
  op_u->src_w[1] = _nc_none;
  op_u->dst_w    = _nc_none;
  op_u->imm_w    = 0;
  op_u->tar_w    = _nc_none;

  return op_u;
}

/* _nc_cid(): memo cache for a %memo clue, as in nock.c.  RETAINS.
*/
static c3_w
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

/* _nc_dir(): append a call site.  RETAINS.
**            bell: callee; ring: jet, or ~; clu: memo clue, or u3_none;
**            arg: (list register) or, for a call by subject, u3_none.
*/
static c3_w
_nc_dir(nc_gen* gen_u, u3_noun bell, u3_noun ring, u3_weak clu, u3_weak arg)
{
  nc_dir* dir_u;

  _nc_grow(gen_u->dir_u, gen_u->dir_n, gen_u->dir_w, nc_dir);
  dir_u = &(gen_u->dir_u[gen_u->dir_n]);

  dir_u->bell  = u3k(bell);
  dir_u->ring  = u3k(ring);
  dir_u->cid_w = ( u3_none == clu ) ? 0 : _nc_cid(clu);
  dir_u->dir_o = __(u3_none != arg);
  dir_u->sot_w = gen_u->pol_n;
  dir_u->len_w = 0;

  while ( (u3_none != arg) && (u3_nul != arg) ) {
    u3_noun i;
    u3x_cell(arg, &i, &arg);
    _nc_grow(gen_u->pol_w, gen_u->pol_n, gen_u->pol_m, c3_w);
    gen_u->pol_w[gen_u->pol_n++] = _nc_reg(gen_u, i);
    dir_u->len_w++;
  }

  return gen_u->dir_n++;
}

/* _nc_kids(): successor blocks of a block, in the order to visit them
**             so that the fall-through successor ends up next.
*/
static c3_w
_nc_kids(nc_gen* gen_u, u3_noun blob, c3_w* kid_w)
{
  u3_noun fin = u3t(u3t(blob));
  u3_noun tag, a, b, z, o;

  u3x_cell(fin, &tag, &a);

  switch ( tag ) {
    default: {
      return 0;
    }

    case c3__hop: {
      kid_w[0] = _nc_blk(gen_u, u3t(a));
      return 1;
    }

    case c3__eqq: {
      u3x_qual(a, &a, &b, &z, &o);
      break;
    }

    case c3__clq:
    case c3__brn:
    case c3__brz: {
      u3x_trel(a, &a, &z, &o);
      break;
    }
  }

  kid_w[0] = _nc_blk(gen_u, u3t(o));
  kid_w[1] = _nc_blk(gen_u, u3t(z));
  return 2;
}

/* _nc_blocks(): collect the blocks of a straight, and lay them out in
**               topological order from the entry block.
*/
static void
_nc_blocks(nc_gen* gen_u, u3_noun map)
{
  //  the treap of the map, in preorder
  //
  {
    u3_noun* sak = 0;
    c3_w     sak_n = 0, sak_m = 0, blk_m = 0;

    _nc_grow(sak, sak_n, sak_m, u3_noun);
    sak[sak_n++] = map;

    while ( sak_n ) {
      u3_noun nod = sak[--sak_n], n, l, r;

      if ( u3_nul == nod ) {
        continue;
      }

      u3x_trel(nod, &n, &l, &r);

      _nc_grow(gen_u->blk_u, gen_u->blk_w, blk_m, nc_blk);
      gen_u->blk_u[gen_u->blk_w].blob  = u3t(n);
      gen_u->blk_u[gen_u->blk_w].lay_w = _nc_none;
      u3h_put(gen_u->idx_p, u3h(n), gen_u->blk_w);
      gen_u->blk_w++;

      _nc_grow(sak, sak_n + 1, sak_m, u3_noun);
      sak[sak_n++] = l;
      sak[sak_n++] = r;
    }

    u3a_free(sak);
  }

  //  reverse postorder of a depth-first search from the entry block
  //
  {
    c3_w  blk_w = gen_u->blk_w;
    c3_w* sak_w = u3a_malloc(blk_w * sizeof(c3_w));
    c3_y* nex_y = u3a_calloc(blk_w, sizeof(c3_y));
    c3_y* saw_y = u3a_calloc(blk_w, sizeof(c3_y));
    c3_w  sak_n = 0, kid_w[2];

    gen_u->lay_w = u3a_malloc(blk_w * sizeof(c3_w));
    gen_u->lay_n = 0;

    sak_w[sak_n++] = _nc_blk(gen_u, 0);
    saw_y[sak_w[0]] = 1;

    while ( sak_n ) {
      c3_w top_w = sak_w[sak_n - 1];
      c3_w kin_w = _nc_kids(gen_u, gen_u->blk_u[top_w].blob, kid_w);

      if ( nex_y[top_w] < kin_w ) {
        c3_w kid = kid_w[nex_y[top_w]++];

        if ( !saw_y[kid] ) {
          saw_y[kid] = 1;
          sak_w[sak_n++] = kid;
        }
      }
      else {
        sak_n--;
        gen_u->lay_w[gen_u->lay_n++] = top_w;
      }
    }

    for ( c3_w i_w = 0; i_w < gen_u->lay_n / 2; i_w++ ) {
      c3_w j_w = gen_u->lay_n - 1 - i_w;
      c3_w tmp_w = gen_u->lay_w[i_w];
      gen_u->lay_w[i_w] = gen_u->lay_w[j_w];
      gen_u->lay_w[j_w] = tmp_w;
    }

    for ( c3_w i_w = 0; i_w < gen_u->lay_n; i_w++ ) {
      gen_u->blk_u[gen_u->lay_w[i_w]].lay_w = i_w;
    }

    u3a_free(sak_w);
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
        op_u->imm_w = a;
        op_u->tar_w = _nc_lit(gen_u, a);
      }
      else {
        op_u = _nc_op(gen_u, _nc_iml);
        op_u->imm_w = _nc_lit(gen_u, a);
      }
      op_u->dst_w = _nc_reg(gen_u, b);
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
      op_u->src_w[0] = _nc_reg(gen_u, a);
      op_u->dst_w    = _nc_reg(gen_u, b);
    } break;

    case c3__con: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_con);
      op_u->src_w[0] = _nc_reg(gen_u, a);
      op_u->src_w[1] = _nc_reg(gen_u, b);
      op_u->dst_w    = _nc_reg(gen_u, c);
    } break;

    case c3__cel:
    case c3__lob: {
      op_u = _nc_op(gen_u, ( c3__cel == tag ) ? _nc_cel : _nc_lob);
      op_u->src_w[0] = _nc_reg(gen_u, arg);
    } break;

    case c3__equ: {
      u3x_cell(arg, &a, &b);
      op_u = _nc_op(gen_u, _nc_equ);
      op_u->src_w[0] = _nc_reg(gen_u, a);
      op_u->src_w[1] = _nc_reg(gen_u, b);
    } break;

    case c3__hsp:
    case c3__hse: {
      op_u = _nc_op(gen_u, ( c3__hsp == tag ) ? _nc_hsp : _nc_hse);
      op_u->imm_w = _nc_lit(gen_u, arg);
    } break;

    case c3__hdp:
    case c3__hde: {
      u3_noun hin;
      u3x_trel(arg, &a, &b, &c);
      hin  = u3nc(u3k(a), u3k(c));
      op_u = _nc_op(gen_u, ( c3__hdp == tag ) ? _nc_hdp : _nc_hde);
      op_u->imm_w    = _nc_lit(gen_u, hin);
      op_u->src_w[0] = _nc_reg(gen_u, b);
      u3z(hin);
    } break;

    case c3__spy:
    case c3__nok: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, ( c3__spy == tag ) ? _nc_spy : _nc_nok);
      op_u->src_w[0] = _nc_reg(gen_u, a);
      op_u->src_w[1] = _nc_reg(gen_u, b);
      op_u->dst_w    = _nc_reg(gen_u, c);
    } break;

    case c3__cal: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_cal);
      op_u->imm_w = _nc_dir(gen_u, a, u3_nul, u3_none, b);
      op_u->dst_w = _nc_reg(gen_u, c);
    } break;

    case c3__caf: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_cal);
      op_u->imm_w = _nc_dir(gen_u, a, d, u3_none, b);
      op_u->dst_w = _nc_reg(gen_u, c);
    } break;

    case c3__cam: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_cam);
      op_u->imm_w = _nc_dir(gen_u, a, u3_nul, d, b);
      op_u->dst_w = _nc_reg(gen_u, c);
    } break;

    case c3__csl: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_csl);
      op_u->imm_w    = _nc_dir(gen_u, a, u3_nul, u3_none, u3_none);
      op_u->src_w[0] = _nc_reg(gen_u, b);
      op_u->dst_w    = _nc_reg(gen_u, c);
    } break;

    case c3__csf: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_csl);
      op_u->imm_w    = _nc_dir(gen_u, a, d, u3_none, u3_none);
      op_u->src_w[0] = _nc_reg(gen_u, b);
      op_u->dst_w    = _nc_reg(gen_u, c);
    } break;

    case c3__csm: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_csm);
      op_u->imm_w    = _nc_dir(gen_u, a, u3_nul, d, u3_none);
      op_u->src_w[0] = _nc_reg(gen_u, b);
      op_u->dst_w    = _nc_reg(gen_u, c);
    } break;
  }
}

/* _nc_jump(): translate a jump [args there] to a block, with its
**             arguments moved into the parameters of the target.
**             nex_w: the block laid out next, or _nc_none.
*/
static void
_nc_jump(nc_gen* gen_u, u3_noun jmp, c3_w nex_w)
{
  u3_noun arg, id, par, a, p;
  c3_w    tar_w;

  u3x_cell(jmp, &arg, &id);
  tar_w = _nc_blk(gen_u, id);
  par   = u3h(gen_u->blk_u[tar_w].blob);

  while ( u3_nul != arg ) {
    u3x_cell(arg, &a, &arg);
    u3x_cell(par, &p, &par);

    if ( c3y == u3du(a) ) {
      nc_op* op_u = _nc_op(gen_u, _nc_mov);
      op_u->hop_y    = 1;
      op_u->src_w[0] = _nc_reg(gen_u, u3t(a));
      op_u->dst_w    = _nc_reg(gen_u, p);
    }
  }

  if ( u3_nul != par ) {
    u3m_bail(c3__fail);
  }

  if ( tar_w != nex_w ) {
    nc_op* op_u = _nc_op(gen_u, _nc_hop);
    op_u->tar_w = tar_w;
  }
}

/* _nc_bare(): block of a jump that carries no arguments.
*/
static c3_w
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
_nc_fin(nc_gen* gen_u, u3_noun fin, c3_w nex_w)
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
      op_u->src_w[0] = _nc_reg(gen_u, a);
      op_u->tar_w    = _nc_bare(gen_u, c);
      c = b;
    } break;

    case c3__eqq: {
      u3x_qual(arg, &a, &b, &c, &d);
      op_u = _nc_op(gen_u, _nc_eqq);
      op_u->src_w[0] = _nc_reg(gen_u, a);
      op_u->src_w[1] = _nc_reg(gen_u, b);
      op_u->tar_w    = _nc_bare(gen_u, d);
    } break;

    case c3__hop: {
      _nc_jump(gen_u, arg, nex_w);
      return;
    }

    case c3__jmp: {
      u3x_cell(arg, &a, &b);
      op_u = _nc_op(gen_u, _nc_jmp);
      op_u->imm_w = _nc_dir(gen_u, a, u3_nul, u3_none, b);
      return;
    }

    case c3__jmf: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_jmp);
      op_u->imm_w = _nc_dir(gen_u, a, c, u3_none, b);
      return;
    }

    case c3__jsp: {
      u3x_cell(arg, &a, &b);
      op_u = _nc_op(gen_u, _nc_jsp);
      op_u->imm_w    = _nc_dir(gen_u, a, u3_nul, u3_none, u3_none);
      op_u->src_w[0] = _nc_reg(gen_u, b);
      return;
    }

    case c3__jsf: {
      u3x_trel(arg, &a, &b, &c);
      op_u = _nc_op(gen_u, _nc_jsp);
      op_u->imm_w    = _nc_dir(gen_u, a, c, u3_none, u3_none);
      op_u->src_w[0] = _nc_reg(gen_u, b);
      return;
    }

    case c3__don: {
      op_u = _nc_op(gen_u, _nc_don);
      op_u->src_w[0] = _nc_reg(gen_u, arg);
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
    c3_w yes_w = _nc_bare(gen_u, c);

    if ( yes_w != nex_w ) {
      op_u = _nc_op(gen_u, _nc_hop);
      op_u->tar_w = yes_w;
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
  c3_w* def_w = u3a_malloc(gen_u->reg_w * sizeof(c3_w));
  c3_w  i_w;

  for ( i_w = 0; i_w < gen_u->reg_w; i_w++ ) {
    def_w[i_w] = _nc_none;
  }

  for ( i_w = 0; i_w < gen_u->ops_n; i_w++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_w]);

    if ( (_nc_imm == op_u->fam_y) || (_nc_iml == op_u->fam_y) ) {
      def_w[op_u->dst_w] = i_w;
    }
  }

  for ( i_w = 0; i_w < gen_u->ops_n; i_w++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_w]);
    nc_op* def_u;

    if ( _nc_eqq != op_u->fam_y ) {
      continue;
    }

    if ( _nc_none == def_w[op_u->src_w[1]] ) {
      c3_w tmp_w;

      if ( _nc_none == def_w[op_u->src_w[0]] ) {
        continue;
      }
      tmp_w          = op_u->src_w[0];
      op_u->src_w[0] = op_u->src_w[1];
      op_u->src_w[1] = tmp_w;
    }

    def_u = &(gen_u->ops_u[def_w[op_u->src_w[1]]]);

    //  a wide slot makes an IMM op emit as IML: its literal is in tar_w
    //
    if ( _nc_imm == def_u->fam_y ) {
      op_u->fam_y = _nc_eqi;
      op_u->imm_w = def_u->imm_w;
    }
    else {
      op_u->fam_y = _nc_eql;
      op_u->imm_w = def_u->imm_w;
    }
    op_u->src_w[1] = _nc_none;
  }

  u3a_free(def_w);
}

/* _nc_translate(): translate the blocks in layout order.
*/
static void
_nc_translate(nc_gen* gen_u)
{
  c3_w i_w;

  for ( i_w = 0; i_w < gen_u->lay_n; i_w++ ) {
    nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_w[i_w]]);
    c3_w    nex_w = ( (i_w + 1) < gen_u->lay_n )
                  ? gen_u->lay_w[i_w + 1]
                  : _nc_none;
    u3_noun par, bod, fin, op;

    u3x_trel(blk_u->blob, &par, &bod, &fin);
    blk_u->fir_w = gen_u->ops_n;

    while ( u3_nul != bod ) {
      u3x_cell(bod, &op, &bod);
      _nc_pole(gen_u, op);
    }

    _nc_fin(gen_u, fin, nex_w);
    blk_u->len_w = gen_u->ops_n - blk_u->fir_w;
  }
}

/* _nc_srcs(): run body over the source registers of an op, reg_w
**             pointing at each in turn: its own, then those of its call
**             site, if any.
*/
#define _nc_srcs(gen_u, op_u, i_w, reg_w, body)                          \
  do {                                                                   \
    const c3_y _nfam = (op_u)->fam_y;                                    \
    c3_w _i;                                                             \
    for ( _i = 0; _i < _nc_fam[_nfam].src_y; _i++ ) {                    \
      c3_w* reg_w = &((op_u)->src_w[_i]);                                \
      body;                                                              \
    }                                                                    \
    if (  (_nc_cal == _nfam) || (_nc_cam == _nfam) || (_nc_jmp == _nfam) ) { \
      nc_dir* _dir = &((gen_u)->dir_u[(op_u)->imm_w]);                   \
      for ( _i = 0; _i < _dir->len_w; _i++ ) {                           \
        c3_w* reg_w = &((gen_u)->pol_w[_dir->sot_w + _i]);               \
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
_nc_slots(nc_gen* gen_u, c3_w arg_w)
{
  c3_w  reg_w = c3_max(gen_u->reg_w, arg_w);
  c3_w* sta_w = u3a_malloc(reg_w * sizeof(c3_w));
  c3_w* end_w = u3a_calloc(reg_w, sizeof(c3_w));
  c3_w* use_w = u3a_calloc(reg_w, sizeof(c3_w));
  c3_w* sot_w = u3a_malloc(reg_w * sizeof(c3_w));
  c3_d* key_d = u3a_malloc(reg_w * sizeof(c3_d));
  c3_w* act_w = u3a_malloc(reg_w * sizeof(c3_w));
  c3_w* fre_w = u3a_malloc(reg_w * sizeof(c3_w));
  c3_w  key_n = 0, act_n = 0, fre_n = 0, tot_w = 0;
  c3_w  i_w, r_w;

  for ( r_w = 0; r_w < reg_w; r_w++ ) {
    sta_w[r_w] = ( r_w < arg_w ) ? 0 : _nc_none;
    sot_w[r_w] = _nc_none;
  }

  //  uses; moves into unused parameters and unused immediates are dropped
  //
  for ( i_w = 0; i_w < gen_u->ops_n; i_w++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_w]);
    _nc_srcs(gen_u, op_u, i_w, reg_w, { use_w[*reg_w]++; });
  }

  for ( i_w = 0; i_w < gen_u->ops_n; i_w++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_w]);
    if (  ( op_u->hop_y
         || (_nc_imm == op_u->fam_y)
         || (_nc_iml == op_u->fam_y) )
       && !use_w[op_u->dst_w] )
    {
      op_u->fam_y = _nc_nop;
    }
  }

  //  intervals
  //
  for ( i_w = 0; i_w < gen_u->ops_n; i_w++ ) {
    nc_op* op_u  = &(gen_u->ops_u[i_w]);
    c3_w   pos_w = i_w + 1;

    if ( _nc_nop == op_u->fam_y ) {
      continue;
    }

    _nc_srcs(gen_u, op_u, i_w, reg_w, {
      if ( _nc_none == sta_w[*reg_w] ) {
        sta_w[*reg_w] = 0;
      }
      end_w[*reg_w] = c3_max(end_w[*reg_w], pos_w);
    });

    if ( _nc_none != op_u->dst_w ) {
      if ( _nc_none == sta_w[op_u->dst_w] ) {
        sta_w[op_u->dst_w] = pos_w;
      }
      end_w[op_u->dst_w] = c3_max(end_w[op_u->dst_w], pos_w);
    }
  }

  //  linear scan
  //
  for ( r_w = 0; r_w < reg_w; r_w++ ) {
    if ( _nc_none != sta_w[r_w] ) {
      key_d[key_n++] = ((c3_d)sta_w[r_w] << 32) | r_w;
    }
  }

  qsort(key_d, key_n, sizeof(c3_d), _nc_key_cmp);

  for ( i_w = 0; i_w < key_n; i_w++ ) {
    c3_w j_w;

    r_w = (c3_w)key_d[i_w];

    for ( j_w = 0; j_w < act_n; ) {
      if ( end_w[act_w[j_w]] < sta_w[r_w] ) {
        fre_w[fre_n++] = sot_w[act_w[j_w]];
        act_w[j_w]     = act_w[--act_n];
      }
      else {
        j_w++;
      }
    }

    sot_w[r_w]     = fre_n ? fre_w[--fre_n] : tot_w++;
    act_w[act_n++] = r_w;
  }

  for ( r_w = 0; r_w < arg_w; r_w++ ) {
    u3_assert( r_w == sot_w[r_w] );
  }

  //  rewrite
  //
  for ( i_w = 0; i_w < gen_u->ops_n; i_w++ ) {
    nc_op* op_u = &(gen_u->ops_u[i_w]);

    if ( _nc_nop == op_u->fam_y ) {
      continue;
    }

    _nc_srcs(gen_u, op_u, i_w, reg_w, { *reg_w = sot_w[*reg_w]; });

    if ( _nc_none != op_u->dst_w ) {
      op_u->dst_w = sot_w[op_u->dst_w];
    }
  }

  gen_u->tot_w = c3_max(tot_w, arg_w);

  u3a_free(sta_w);
  u3a_free(end_w);
  u3a_free(use_w);
  u3a_free(sot_w);
  u3a_free(key_d);
  u3a_free(act_w);
  u3a_free(fre_w);
}

/* _nc_put(): write an immediate of the given width, or measure it.
*/
static c3_w
_nc_put(c3_y* buf_y, c3_y wid_y, c3_w val_w)
{
  switch ( wid_y ) {
    case 0: {
      if ( buf_y ) {
        buf_y[0] = val_w;
      }
      return 1;
    }

    case 1: {
      if ( buf_y ) {
        buf_y[0] = val_w & 0xff;
        buf_y[1] = val_w >> 8;
      }
      return 2;
    }

    default: {
      c3_w len_w = 0, i_w;

      for ( c3_w tmp_w = val_w; tmp_w; tmp_w >>= 8 ) {
        len_w++;
      }
      len_w = c3_max(len_w, 1);

      if ( buf_y ) {
        buf_y[0] = len_w;
        for ( i_w = 0; i_w < len_w; i_w++ ) {
          buf_y[1 + i_w] = (val_w >> (8 * i_w)) & 0xff;
        }
      }
      return 1 + len_w;
    }
  }
}

/* _nc_encode(): write an op, or measure it.
*/
static c3_w
_nc_encode(nc_gen* gen_u, nc_op* op_u, c3_y* buf_y)
{
  nc_op tmp_u;
  c3_w  val_w[5], len_w = 0, max_w = 0, siz_w = 1, i_w;
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
      if ( op_u->dst_w < 0x100 ) {
        c3_w n_w = op_u->imm_w;
        c3_y cod_y;

        if ( n_w < 2 ) {
          cod_y = n_w ? IMM_1 : IMM_0;
          siz_w = 2;
        }
        else if ( n_w < 0x100 ) {
          cod_y = IMM_B;
          siz_w = 3;
        }
        else {
          cod_y = IMM_S;
          siz_w = 4;
        }

        if ( buf_y ) {
          buf_y[0] = cod_y;
          if ( 3 == siz_w ) {
            buf_y[1] = n_w;
          }
          else if ( 4 == siz_w ) {
            buf_y[1] = n_w & 0xff;
            buf_y[2] = n_w >> 8;
          }
          buf_y[siz_w - 1] = op_u->dst_w;
        }
        return siz_w;
      }

      //  a wide slot: use the literal
      //
      tmp_u       = *op_u;
      tmp_u.fam_y = fam_y = _nc_iml;
      tmp_u.imm_w = op_u->tar_w;
      op_u        = &tmp_u;
    } break;
  }

  if ( _nc_fam[fam_y].imm_y ) {
    val_w[len_w++] = op_u->imm_w;
  }
  for ( i_w = 0; i_w < _nc_fam[fam_y].src_y; i_w++ ) {
    val_w[len_w++] = op_u->src_w[i_w];
  }
  if ( _nc_fam[fam_y].dst_y ) {
    val_w[len_w++] = op_u->dst_w;
  }
  if ( _nc_fam[fam_y].tar_y ) {
    val_w[len_w++] = gen_u->blk_u[op_u->tar_w].off_w;
  }

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    max_w = c3_max(max_w, val_w[i_w]);
  }

  wid_y = ( max_w < 0x100 ) ? 0 : ( max_w < 0x10000 ) ? 1 : 2;

  if ( buf_y ) {
    buf_y[0] = _nc_base(fam_y) + wid_y;
  }

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    siz_w += _nc_put(buf_y ? (buf_y + siz_w) : 0, wid_y, val_w[i_w]);
  }

  return siz_w;
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

static c3_t _nc_verb_t;

/* _nc_read(): read an immediate of the given width.
*/
static c3_w
_nc_read(const c3_y* pog, c3_w* ip_w, c3_y wid_y)
{
  c3_w val_w = 0;

  switch ( wid_y ) {
    case 0: {
      val_w = pog[(*ip_w)++];
    } break;

    case 1: {
      val_w  = pog[(*ip_w)++];
      val_w |= pog[(*ip_w)++] << 8;
    } break;

    default: {
      c3_y len_y = pog[(*ip_w)++], i_y;
      for ( i_y = 0; i_y < len_y; i_y++ ) {
        val_w |= ((c3_w)pog[(*ip_w)++]) << (8 * i_y);
      }
    } break;
  }

  return val_w;
}

/* _nc_print(): print a program.
*/
static void
_nc_print(u3nc_prog* pog_u)
{
  const c3_y* pog = pog_u->byc_u.ops_y;
  c3_w        ip_w = 0, i_w;

  fprintf(stderr, "program %p: %u slots, %u args, %u bytes, %u literals, "
                  "%u call sites\r\n",
          (void*)pog_u, pog_u->tot_w, pog_u->arg_w, pog_u->byc_u.len_w,
          pog_u->lit_u.len_w, pog_u->dir_u.len_w);

  for ( i_w = 0; i_w < pog_u->dir_u.len_w; i_w++ ) {
    u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[i_w]);
    c3_w       j_w;

    fprintf(stderr, "  site %u: bell %08x %s%s%s cid %u args",
            i_w, u3r_mug(dir_u->bell),
            ( c3y == dir_u->dir_o ) ? "direct" : "subject",
            dir_u->ham_u ? " jet" : "",
            dir_u->arm_u ? " array-jet" : "",
            dir_u->cid_w);

    for ( j_w = 0; j_w < dir_u->len_w; j_w++ ) {
      fprintf(stderr, " %u", pog_u->sot_u.sot_w[dir_u->sot_w + j_w]);
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
      fprintf(stderr, "/%u", u3t(dir_u->ring));
    }
    fprintf(stderr, "\r\n");
  }

  while ( ip_w < pog_u->byc_u.len_w ) {
    c3_y cod_y = pog[ip_w];
    c3_w off_w = ip_w++;

    fprintf(stderr, "  %4u: %-6s", off_w, _nc_name_c[cod_y]);

    if ( cod_y < IML_B ) {
      if ( IMM_B == cod_y ) {
        fprintf(stderr, " %u", _nc_read(pog, &ip_w, 0));
      }
      else if ( IMM_S == cod_y ) {
        fprintf(stderr, " %u", _nc_read(pog, &ip_w, 1));
      }
      fprintf(stderr, " -> %u", _nc_read(pog, &ip_w, 0));
    }
    else if ( BOM != cod_y ) {
      c3_y fam_y = (cod_y - IML_B) / 3;
      c3_y wid_y = (cod_y - IML_B) % 3;

      if ( _nc_fam[fam_y].imm_y ) {
        fprintf(stderr, " #%u", _nc_read(pog, &ip_w, wid_y));
      }
      for ( i_w = 0; i_w < _nc_fam[fam_y].src_y; i_w++ ) {
        fprintf(stderr, " %u", _nc_read(pog, &ip_w, wid_y));
      }
      if ( _nc_fam[fam_y].dst_y ) {
        fprintf(stderr, " -> %u", _nc_read(pog, &ip_w, wid_y));
      }
      if ( _nc_fam[fam_y].tar_y ) {
        fprintf(stderr, " @%u", _nc_read(pog, &ip_w, wid_y));
      }
    }
    fprintf(stderr, "\r\n");
  }
}

/* _nc_prog_fix(): set the pointers of a program from its lengths.
*/
static void
_nc_prog_fix(u3nc_prog* pog_u)
{
  c3_y* dat_y = (c3_y*)pog_u;
  c3_w  len_w = sizeof(u3nc_prog);

  len_w = c3_align(len_w, 8, C3_ALGHI);
  pog_u->byc_u.ops_y = dat_y + len_w;
  len_w += pog_u->byc_u.len_w;

  len_w = c3_align(len_w, 8, C3_ALGHI);
  pog_u->lit_u.non = (u3_noun*)(dat_y + len_w);
  len_w += pog_u->lit_u.len_w * sizeof(u3_noun);

  len_w = c3_align(len_w, 8, C3_ALGHI);
  pog_u->dir_u.dat_u = (u3nc_dire*)(dat_y + len_w);
  len_w += pog_u->dir_u.len_w * sizeof(u3nc_dire);

  len_w = c3_align(len_w, 8, C3_ALGHI);
  pog_u->sot_u.sot_w = (c3_w*)(dat_y + len_w);
}

/* _nc_prog_new(): allocate a program.
*/
static u3nc_prog*
_nc_prog_new(c3_w byc_w, c3_w lit_w, c3_w dir_w, c3_w sot_w)
{
  c3_w len_w = sizeof(u3nc_prog);
  u3nc_prog* pog_u;

  len_w = c3_align(len_w, 8, C3_ALGHI) + byc_w;
  len_w = c3_align(len_w, 8, C3_ALGHI) + (lit_w * sizeof(u3_noun));
  len_w = c3_align(len_w, 8, C3_ALGHI) + (dir_w * sizeof(u3nc_dire));
  len_w = c3_align(len_w, 8, C3_ALGHI) + (sot_w * sizeof(c3_w));

  pog_u = u3a_malloc(len_w);
  pog_u->byc_u.len_w = byc_w;
  pog_u->lit_u.len_w = lit_w;
  pog_u->dir_u.len_w = dir_w;
  pog_u->sot_u.len_w = sot_w;
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
  dir_u->ham_u = 0;
  dir_u->arm_u = 0;

  if ( u3_nul == dir_u->ring ) {
    return;
  }

  if ( c3n == u3j_ring(dir_u->ring, &(dir_u->ham_u), &(dir_u->arm_u)) ) {
    u3nc_Stat.rin_d++;
    u3l_log("u3nc: no driver for ring %s", u3m_pretty_path(u3h(dir_u->ring)));
  }
  else {
    u3nc_Stat.arm_d++;
  }
}

/* _nc_emit(): lay the ops out and assemble the program.  RETAINS ned.
*/
static u3nc_prog*
_nc_emit(nc_gen* gen_u, u3_noun ned, c3_w arg_w)
{
  u3nc_prog* pog_u;
  c3_w       byc_w, i_w, j_w;

  //  block offsets: immediates only widen, so this converges
  //
  for ( i_w = 0; i_w < gen_u->blk_w; i_w++ ) {
    gen_u->blk_u[i_w].off_w = 0;
  }

  {
    c3_t chg_t;

    do {
      chg_t = 0;
      byc_w = 0;

      for ( i_w = 0; i_w < gen_u->lay_n; i_w++ ) {
        nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_w[i_w]]);

        if ( blk_u->off_w != byc_w ) {
          blk_u->off_w = byc_w;
          chg_t = 1;
        }

        for ( j_w = 0; j_w < blk_u->len_w; j_w++ ) {
          byc_w += _nc_encode(gen_u, &(gen_u->ops_u[blk_u->fir_w + j_w]), 0);
        }
      }
    } while ( chg_t );
  }

  pog_u = _nc_prog_new(byc_w, gen_u->lit_w, gen_u->dir_n, gen_u->pol_n);
  pog_u->tot_w = gen_u->tot_w;
  pog_u->arg_w = arg_w;
  pog_u->ned   = u3k(ned);

  {
    c3_y* buf_y = pog_u->byc_u.ops_y;
    c3_w  pos_w = 0;

    for ( i_w = 0; i_w < gen_u->lay_n; i_w++ ) {
      nc_blk* blk_u = &(gen_u->blk_u[gen_u->lay_w[i_w]]);

      u3_assert( blk_u->off_w == pos_w );

      for ( j_w = 0; j_w < blk_u->len_w; j_w++ ) {
        pos_w += _nc_encode(gen_u, &(gen_u->ops_u[blk_u->fir_w + j_w]),
                            buf_y + pos_w);
      }
    }

    u3_assert( pos_w == byc_w );
  }

  u3h_walk_with(gen_u->lit_p, _nc_lit_cb, pog_u->lit_u.non);

  for ( i_w = 0; i_w < gen_u->dir_n; i_w++ ) {
    nc_dir*    dir_u = &(gen_u->dir_u[i_w]);
    u3nc_dire* dat_u = &(pog_u->dir_u.dat_u[i_w]);

    dat_u->bell  = dir_u->bell;
    dat_u->ring  = dir_u->ring;
    dat_u->pog_p = 0;
    dat_u->sot_w = dir_u->sot_w;
    dat_u->len_w = dir_u->len_w;
    dat_u->cid_w = dir_u->cid_w;
    dat_u->dir_o = dir_u->dir_o;
    _nc_dire_jet(dat_u);
  }

  memcpy(pog_u->sot_u.sot_w, gen_u->pol_w, gen_u->pol_n * sizeof(c3_w));

  return pog_u;
}

/* _nc_build(): compile a straight [need n-args blocks].  TRANSFERS.
*/
static u3nc_prog*
_nc_build(u3_noun straight)
{
  nc_gen     gen_u = {0};
  u3nc_prog* pog_u;
  u3_noun    ned, arg, map;

  u3x_trel(straight, &ned, &arg, &map);

  gen_u.idx_p = u3h_new();
  gen_u.lit_p = u3h_new();

  _nc_blocks(&gen_u, map);
  _nc_translate(&gen_u);
  _nc_fold(&gen_u);
  _nc_slots(&gen_u, _nc_cat(arg));
  pog_u = _nc_emit(&gen_u, ned, _nc_cat(arg));

  u3nc_Stat.com_d++;
  if ( _nc_verb_t ) {
    _nc_print(pog_u);
  }

  u3h_free(gen_u.idx_p);
  u3h_free(gen_u.lit_p);
  u3a_free(gen_u.blk_u);
  u3a_free(gen_u.lay_w);
  u3a_free(gen_u.ops_u);
  u3a_free(gen_u.dir_u);
  u3a_free(gen_u.pol_w);
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
  u3a_road* rod_u = u3R;

  while ( 1 ) {
    u3_weak pog = u3h_git(rod_u->ska.dir_p, bell);

    if ( u3_none != pog ) {
      return _nc_to(pog);
    }
    if ( !rod_u->par_p ) {
      return 0;
    }
    rod_u = u3to(u3a_road, rod_u->par_p);
  }
}

/* _nc_ent_get(): entry program for [sub fol], from any road.  RETAINS.
*/
static u3nc_prog*
_nc_ent_get(u3_noun sub, u3_noun fol)
{
  u3a_road* rod_u = u3R;

  while ( 1 ) {
    u3_weak lis = u3h_git(rod_u->ska.ent_p, fol);
    u3_weak got;

    if (  (u3_none != lis)
       && (u3_none != (got = u3d_match(sub, lis))) )
    {
      return _nc_to(u3t(got));
    }
    if ( !rod_u->par_p ) {
      return 0;
    }
    rod_u = u3to(u3a_road, rod_u->par_p);
  }
}

/* _nc_link(): set the callee programs of the direct call sites.
*/
static void
_nc_link(u3nc_prog* pog_u)
{
  c3_w i_w;

  for ( i_w = 0; i_w < pog_u->dir_u.len_w; i_w++ ) {
    u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[i_w]);

    if ( c3y == dir_u->dir_o ) {
      u3nc_prog* gop_u = _nc_dir_get(dir_u->bell);
      u3_assert( gop_u );
      dir_u->pog_p = u3of(u3nc_prog, gop_u);
    }
  }
}

/* _nc_compile(): compile a straight, then its callees that haven't
**                been compiled, and link them all.  TRANSFERS.
*/
static u3nc_prog*
_nc_compile(u3_noun straight)
{
  u3nc_prog** fre_u = 0;
  c3_w        fre_n = 0, fre_m = 0, i_w, j_w;
  u3nc_prog*  pog_u = _nc_build(straight);

  _nc_grow(fre_u, fre_n, fre_m, u3nc_prog*);
  fre_u[fre_n++] = pog_u;

  for ( i_w = 0; i_w < fre_n; i_w++ ) {
    u3nc_prog* gop_u = fre_u[i_w];

    for ( j_w = 0; j_w < gop_u->dir_u.len_w; j_w++ ) {
      u3nc_dire* dir_u = &(gop_u->dir_u.dat_u[j_w]);

      if ( (c3y == dir_u->dir_o) && !_nc_dir_get(dir_u->bell) ) {
        u3nc_prog* new_u = _nc_build(u3d_dire(dir_u->bell));

        u3h_put(u3R->ska.dir_p, dir_u->bell, _nc_of(new_u));
        _nc_grow(fre_u, fre_n, fre_m, u3nc_prog*);
        fre_u[fre_n++] = new_u;
      }
    }
  }

  for ( i_w = 0; i_w < fre_n; i_w++ ) {
    _nc_link(fre_u[i_w]);
  }

  u3a_free(fre_u);
  return pog_u;
}

/* _nc_entry(): entry program for [sub fol], compiling it if needed.
**              RETAINS.
*/
static u3nc_prog*
_nc_entry(u3_noun sub, u3_noun fol)
{
  u3nc_prog* pog_u = _nc_ent_get(sub, fol);

  u3nc_Stat.ent_d++;

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
static c3_w
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
_nc_knit(u3_noun ned, u3_noun* arg, c3_w* i_w)
{
  u3_noun tag = u3h(ned);

  if ( c3y == u3du(tag) ) {
    u3_noun hed = _nc_knit(tag, arg, i_w);
    return u3nc(hed, _nc_knit(u3t(ned), arg, i_w));
  }

  switch ( tag ) {
    default:        return 0;
    case c3__this:  return u3k(arg[(*i_w)++]);
    case c3__both: {
      u3_noun rot = u3k(arg[(*i_w)++]);
      *i_w += _nc_count(u3h(u3t(ned))) + _nc_count(u3t(u3t(ned)));
      return rot;
    }
  }
}

/* _nc_save(): save the product of a memoized call, as nock.c does.
**             RETAINS.
*/
static void
_nc_save(c3_w cid_w, u3_noun key, u3_noun pro)
{
  if ( (u3z_memo_ford == cid_w) && (0 == u3R->ski.gul) ) {
    u3z_save_m(cid_w, 136 + c3__ford, key, pro);
  }
  else if ( (u3z_memo_toss == cid_w)
            ? (&(u3H->rod_u) != u3R)
            : (0 == u3R->ski.gul) )
  {
    u3z_save_m(cid_w, 144 + c3__nock, key, pro);
  }
}

/* nc_frame: the caller of an activation, below its slots on the stack.
*/
typedef struct __attribute__((__packed__)) {
  u3nc_prog* pog_u;   //  caller program, or 0 at the entry
  u3_noun*   reg;     //  caller slots
  c3_w       ip_w;    //  caller instruction pointer
  c3_w       des_w;   //  caller slot for the product
  u3_weak    key;     //  memo key to save the product under
  c3_w       cid_w;   //  memo cache
} nc_frame;

#define _nc_frame_w  c3_wiseof(nc_frame)

/* _nc_push(): push words on the stack.  mov_ws: -1 north, 1 south.
*/
static inline void*
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
static inline void*
_nc_top(u3a_road* rod_u, c3_ws mov_ws, c3_w len_w)
{
  return u3to(void, (mov_ws < 0) ? rod_u->cap_p : (rod_u->cap_p - len_w));
}

/* _nc_pop(): pop words off the stack.
*/
static inline void
_nc_pop(u3a_road* rod_u, c3_ws mov_ws, c3_w len_w)
{
  rod_u->cap_p -= mov_ws * (c3_ws)len_w;
}

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
_nc_burn(u3nc_prog* pog_u, u3_noun* arg, c3_w len_w, c3_ws mov_ws)
{
#define X(op) &&do_##op,
  static const void* lab[] = { OPCODES };
#undef X

  u3a_road*  rod_u = u3R;
  c3_y*      pog;
  c3_w       ip_w;
  u3_noun*   reg;
  u3_noun*   nex;
  nc_frame*  fam_u;
  nc_frame   fam;
  u3nc_prog* gop_u;
  u3nc_dire* dir_u;
  c3_w*      sot_w;
  c3_w       a_w, b_w, c_w, d_w, i_w;
  u3_noun    x, o, pro, hin;
  u3_post    emp_p = rod_u->cap_p;

#define RB()  (pog[ip_w++])
#define RS()  ({ c3_w _v = pog[ip_w] | (pog[ip_w + 1] << 8); ip_w += 2; _v; })
#define RV()  ({                                                         \
    c3_y _n = pog[ip_w++];                                               \
    c3_w _v = 0;                                                         \
    for ( c3_y _i = 0; _i < _n; _i++ ) {                                 \
      _v |= ((c3_w)pog[ip_w++]) << (8 * _i);                             \
    }                                                                    \
    _v;                                                                  \
  })
#define BURN()  goto *lab[pog[ip_w++]]
#define PUT(d, v)  do { u3_noun _o = reg[d]; reg[d] = (v); u3z(_o); } while ( 0 )
#define PUSH(n)  _nc_push(rod_u, mov_ws, n)
#define POP(n)   _nc_pop(rod_u, mov_ws, n)
#define TOP(n)   _nc_top(rod_u, mov_ws, n)

#define ARG1(op, a)                                                      \
  do_##op##_B: a = RB(); goto op##_in;                                   \
  do_##op##_S: a = RS(); goto op##_in;                                   \
  do_##op##_V: a = RV();                                                 \
  op##_in:
#define ARG2(op, a, b)                                                   \
  do_##op##_B: a = RB(); b = RB(); goto op##_in;                         \
  do_##op##_S: a = RS(); b = RS(); goto op##_in;                         \
  do_##op##_V: a = RV(); b = RV();                                       \
  op##_in:
#define ARG3(op, a, b, c)                                                \
  do_##op##_B: a = RB(); b = RB(); c = RB(); goto op##_in;               \
  do_##op##_S: a = RS(); b = RS(); c = RS(); goto op##_in;               \
  do_##op##_V: a = RV(); b = RV(); c = RV();                             \
  op##_in:

//  FRAME(): push a frame recording this activation, its product to d_w
//
#define FRAME()  do {                                                    \
    fam_u = PUSH(_nc_frame_w);                                           \
    fam_u->pog_u = pog_u;                                                \
    fam_u->reg   = reg;                                                  \
    fam_u->ip_w  = ip_w;                                                 \
    fam_u->des_w = d_w;                                                  \
    fam_u->key   = u3_none;                                              \
    fam_u->cid_w = 0;                                                    \
  } while ( 0 )

//  SITE(): the call site a_w, its callee gop_u, its argument slots
//          sot_w and their number len_w
//
#define SITE()  do {                                                     \
    dir_u = &(pog_u->dir_u.dat_u[a_w]);                                  \
    gop_u = u3to(u3nc_prog, dir_u->pog_p);                               \
    sot_w = pog_u->sot_u.sot_w + dir_u->sot_w;                           \
    len_w = dir_u->len_w;                                                \
  } while ( 0 )

//  GATHER(): push num words and copy the site's arguments into them
//
#define GATHER(num)  do {                                                \
    nex = PUSH(num);                                                     \
    for ( i_w = 0; i_w < len_w; i_w++ ) {                                \
      nex[i_w] = reg[sot_w[i_w]];                                        \
    }                                                                    \
  } while ( 0 )

  reg = PUSH(pog_u->tot_w);

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    reg[i_w] = arg[i_w];
  }

  fam_u = PUSH(_nc_frame_w);
  fam_u->pog_u = 0;

  enter:
    //  pog_u: the callee, its len_w arguments in its slots at reg
    //
    for ( i_w = len_w; i_w < pog_u->tot_w; i_w++ ) {
      reg[i_w] = 0;
    }

    pog  = pog_u->byc_u.ops_y;
    ip_w = 0;
    BURN();

  call:
    //  gop_u: the callee; x: its one argument, transferred;
    //  d_w: the slot for its product
    //
    nex    = PUSH(gop_u->tot_w);
    nex[0] = x;
    FRAME();
    pog_u  = gop_u;
    reg    = nex;
    len_w  = 1;
    goto enter;

  tail:
    //  gop_u: the callee; x: its one argument, transferred
    //
    for ( i_w = 0; i_w < pog_u->tot_w; i_w++ ) {
      u3z(reg[i_w]);
    }
    fam = *(nc_frame*)TOP(_nc_frame_w);
    POP(_nc_frame_w + pog_u->tot_w);

    pog_u  = gop_u;
    reg    = PUSH(pog_u->tot_w);
    reg[0] = x;
    *(nc_frame*)PUSH(_nc_frame_w) = fam;
    len_w  = 1;
    goto enter;

  done:
    //  pro: the product, transferred
    //
    for ( i_w = 0; i_w < pog_u->tot_w; i_w++ ) {
      u3z(reg[i_w]);
    }

    fam_u = TOP(_nc_frame_w);

    if ( !fam_u->pog_u ) {
      POP(_nc_frame_w + pog_u->tot_w);
      u3_assert( emp_p == rod_u->cap_p );
      return pro;
    }

    if ( u3_none != fam_u->key ) {
      _nc_save(fam_u->cid_w, fam_u->key, pro);
      u3z(fam_u->key);
    }

    POP(_nc_frame_w + pog_u->tot_w);
    pog_u = fam_u->pog_u;
    reg   = fam_u->reg;
    ip_w  = fam_u->ip_w;
    d_w   = fam_u->des_w;
    pog   = pog_u->byc_u.ops_y;
    PUT(d_w, pro);
    BURN();

  {
    do_IMM_0:
      d_w = RB();
      PUT(d_w, 0);
      BURN();

    do_IMM_1:
      d_w = RB();
      PUT(d_w, 1);
      BURN();

    do_IMM_B:
      a_w = RB();
      d_w = RB();
      PUT(d_w, a_w);
      BURN();

    do_IMM_S:
      a_w = RS();
      d_w = RB();
      PUT(d_w, a_w);
      BURN();

    ARG2(IML, a_w, d_w)
      PUT(d_w, u3k(pog_u->lit_u.non[a_w]));
      BURN();

    ARG2(MOV, a_w, d_w)
      PUT(d_w, u3k(reg[a_w]));
      BURN();

    ARG2(INC, a_w, d_w)
      PUT(d_w, u3i_vint(u3k(reg[a_w])));
      BURN();

    ARG3(CON, a_w, b_w, d_w)
      PUT(d_w, u3nc(u3k(reg[a_w]), u3k(reg[b_w])));
      BURN();

    ARG2(HED, a_w, d_w)
      x = reg[a_w];
      PUT(d_w, ( c3y == u3du(x) ) ? u3k(u3h(x)) : 0);
      BURN();

    ARG2(TAL, a_w, d_w)
      x = reg[a_w];
      PUT(d_w, ( c3y == u3du(x) ) ? u3k(u3t(x)) : 0);
      BURN();

    ARG1(CEL, a_w)
      if ( c3n == u3du(reg[a_w]) ) {
        u3m_bail(c3__exit);
      }
      BURN();

    ARG1(LOB, a_w)
      if ( reg[a_w] > 1 ) {
        u3m_bail(c3__exit);
      }
      BURN();

    ARG2(EQU, a_w, b_w)
      u3r_sing(reg[a_w], reg[b_w]);
      BURN();

    ARG1(HSP, a_w)
      u3n_hilt_fore(u3k(pog_u->lit_u.non[a_w]), 0, &x);
      *(u3_noun*)PUSH(1) = x;
      BURN();

    ARG1(HSE, a_w)
      x = *(u3_noun*)TOP(1);
      POP(1);
      u3n_hilt_hind(x, 0);
      BURN();

    ARG2(HDP, a_w, b_w)
      hin = pog_u->lit_u.non[a_w];
      o   = u3k(reg[b_w]);

      switch ( u3h(hin) ) {
        case c3__hunk:
        case c3__lose:
        case c3__mean:
        case c3__spot: {
          u3t_push(u3nc(u3k(u3h(hin)), o));
          x = u3_nul;
        } break;

        case c3__live: {
          if ( c3y == u3ud(o) ) {
            u3t_heck(o);
          }
          else {
            u3z(o);
          }
          x = u3_nul;
        } break;

        case c3__slog: {
          if ( !(u3C.wag_h & u3o_quiet) ) {
            u3t_slog(o);
          }
          else {
            u3z(o);
          }
          x = u3_nul;
        } break;

        //  %loop needs the subject, which isn't at hand; %hand is unknown
        //
        case c3__loop:
        case c3__hand: {
          u3z(o);
          x = u3_nul;
        } break;

        default: {
          u3n_hint_fore(u3k(hin), 0, &o);
          x = o;
        } break;
      }

      *(u3_noun*)PUSH(1) = x;
      BURN();

    ARG2(HDE, a_w, b_w)
      hin = pog_u->lit_u.non[a_w];
      x   = *(u3_noun*)TOP(1);
      POP(1);

      switch ( u3h(hin) ) {
        case c3__hunk:
        case c3__lose:
        case c3__mean:
        case c3__spot: {
          u3t_drop();
        } break;

        case c3__live:
        case c3__slog:
        case c3__loop:
        case c3__hand: {
        } break;

        default: {
          u3n_hint_hind(x, 0);
        } break;
      }
      BURN();

    ARG3(SPY, a_w, b_w, d_w)
      x = u3m_soft_esc(u3k(reg[a_w]), u3k(reg[b_w]));

      if ( c3n == u3du(x) ) {
        u3m_bail(u3nc(1, u3k(reg[b_w])));
      }
      else if ( c3n == u3du(u3t(x)) ) {
        u3t_push(u3nt(c3__hunk, u3k(reg[a_w]), u3k(reg[b_w])));
        u3m_bail(c3__exit);
      }

      PUT(d_w, u3k(u3t(u3t(x))));
      u3z(x);
      BURN();

    ARG3(NOK, a_w, b_w, d_w)
      x     = reg[a_w];
      gop_u = _nc_entry(x, reg[b_w]);
      u3k(x);
      goto call;

    ARG2(CAL, a_w, d_w)
      SITE();
      GATHER(gop_u->tot_w);

      if ( dir_u->arm_u && (u3_none != (pro = dir_u->arm_u->arg_f(nex))) ) {
        _nc_stat(jet_d);
        POP(gop_u->tot_w);
        PUT(d_w, pro);
        BURN();
      }
      goto cal_go;

    ARG2(CAM, a_w, d_w)
      SITE();
      GATHER(gop_u->tot_w);

      i_w = 0;
      x   = u3nc(_nc_knit(gop_u->ned, nex, &i_w), u3k(u3t(dir_u->bell)));
      o   = u3z_find_m(dir_u->cid_w, 144 + c3__nock, x);

      if ( u3_none != o ) {
        POP(gop_u->tot_w);
        u3z(x);
        PUT(d_w, o);
        BURN();
      }

      for ( i_w = 0; i_w < len_w; i_w++ ) {
        u3k(nex[i_w]);
      }
      FRAME();
      fam_u->key   = x;
      fam_u->cid_w = dir_u->cid_w;
      goto cal_in;

    cal_go:
      for ( i_w = 0; i_w < len_w; i_w++ ) {
        u3k(nex[i_w]);
      }
      FRAME();

    cal_in:
      _nc_stat(dir_d);
      pog_u = gop_u;
      reg   = nex;
      goto enter;

    ARG3(CSL, a_w, b_w, d_w)
      dir_u = &(pog_u->dir_u.dat_u[a_w]);
      x     = reg[b_w];

      if ( dir_u->ham_u ) {
        pro = u3j_kick_arm(u3k(x), dir_u->ham_u, u3t(dir_u->ring));

        if ( u3_none != pro ) {
          _nc_stat(jet_d);
          PUT(d_w, pro);
          BURN();
        }
        u3z(x);
      }
      goto csl_go;

    ARG3(CSM, a_w, b_w, d_w)
      dir_u = &(pog_u->dir_u.dat_u[a_w]);
      x     = reg[b_w];
      o     = u3nc(u3k(x), u3k(u3t(dir_u->bell)));
      pro   = u3z_find_m(dir_u->cid_w, 144 + c3__nock, o);

      if ( u3_none != pro ) {
        u3z(o);
        PUT(d_w, pro);
        BURN();
      }

      _nc_stat(sub_d);
      gop_u  = _nc_entry(x, u3t(dir_u->bell));
      nex    = PUSH(gop_u->tot_w);
      nex[0] = u3k(x);
      FRAME();
      fam_u->key   = o;
      fam_u->cid_w = dir_u->cid_w;
      pog_u  = gop_u;
      reg    = nex;
      len_w  = 1;
      goto enter;

    csl_go:
      _nc_stat(sub_d);
      gop_u = _nc_entry(x, u3t(dir_u->bell));
      u3k(x);
      goto call;

    ARG2(CLQ, a_w, b_w)
      if ( c3n == u3du(reg[a_w]) ) {
        ip_w = b_w;
      }
      BURN();

    ARG3(EQQ, a_w, b_w, c_w)
      if ( c3n == u3r_sing(reg[a_w], reg[b_w]) ) {
        ip_w = c_w;
      }
      BURN();

    ARG3(EQI, a_w, b_w, c_w)
      if ( reg[b_w] != a_w ) {
        ip_w = c_w;
      }
      BURN();

    ARG3(EQL, a_w, b_w, c_w)
      if ( c3n == u3r_sing(pog_u->lit_u.non[a_w], reg[b_w]) ) {
        ip_w = c_w;
      }
      BURN();

    ARG2(BRN, a_w, b_w)
      x = reg[a_w];
      if ( 1 == x ) {
        ip_w = b_w;
      }
      else if ( 0 != x ) {
        u3m_bail(c3__exit);
      }
      BURN();

    ARG2(BRZ, a_w, b_w)
      if ( 0 != reg[a_w] ) {
        ip_w = b_w;
      }
      BURN();

    ARG1(HOP, a_w)
      ip_w = a_w;
      BURN();

    ARG1(JMP, a_w)
      SITE();
      GATHER(len_w);

      if ( dir_u->arm_u && (u3_none != (pro = dir_u->arm_u->arg_f(nex))) ) {
        _nc_stat(jet_d);
        POP(len_w);
        goto done;
      }

      //  the caller's references move to the callee: the first argument
      //  from a slot takes its reference, any other from the same slot
      //  finds it gone and gains one
      //
      _nc_stat(dir_d);
      for ( i_w = 0; i_w < len_w; i_w++ ) {
        u3_noun* sot = &(reg[sot_w[i_w]]);

        if ( *sot ) {
          *sot = 0;
        }
        else {
          u3k(nex[i_w]);
        }
      }
      for ( i_w = 0; i_w < pog_u->tot_w; i_w++ ) {
        u3z(reg[i_w]);
      }
      POP(len_w);
      fam = *(nc_frame*)TOP(_nc_frame_w);
      POP(_nc_frame_w + pog_u->tot_w);
      pog_u = gop_u;
      reg   = PUSH(pog_u->tot_w);
      memmove(reg, nex, len_w * sizeof(u3_noun));
      *(nc_frame*)PUSH(_nc_frame_w) = fam;
      goto enter;

    ARG2(JSP, a_w, b_w)
      dir_u = &(pog_u->dir_u.dat_u[a_w]);
      x     = reg[b_w];

      if ( dir_u->ham_u ) {
        pro = u3j_kick_arm(u3k(x), dir_u->ham_u, u3t(dir_u->ring));

        if ( u3_none != pro ) {
          _nc_stat(jet_d);
          goto done;
        }
        u3z(x);
      }

      _nc_stat(sub_d);
      gop_u    = _nc_entry(x, u3t(dir_u->bell));
      reg[b_w] = 0;
      goto tail;

    ARG1(DON, a_w)
      pro      = reg[a_w];
      reg[a_w] = 0;
      goto done;

    do_BOM:
      u3m_bail(c3__exit);
  }

#undef RB
#undef RS
#undef RV
#undef BURN
#undef PUT
#undef PUSH
#undef POP
#undef TOP
#undef ARG1
#undef ARG2
#undef ARG3
#undef FRAME
#undef SITE
#undef GATHER
}

/* _nc_burn_out(): run a program on its arguments.  TRANSFERS.
*/
static u3_noun
_nc_burn_out(u3nc_prog* pog_u, u3_noun* arg, c3_w len_w)
{
  c3_ws   mov_ws = ( c3y == u3a_is_north(u3R) ) ? -1 : 1;
  u3_noun pro;

  u3t_on(noc_o);
  pro = _nc_burn(pog_u, arg, len_w, mov_ws);
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

  _nc_verb_t = !!getenv("U3NC_VERBOSE");

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

/* _nc_prog_free(): free a program.
*/
static void
_nc_prog_free(u3nc_prog* pog_u)
{
  c3_w i_w;

  u3z(pog_u->ned);

  for ( i_w = 0; i_w < pog_u->lit_u.len_w; i_w++ ) {
    u3z(pog_u->lit_u.non[i_w]);
  }

  for ( i_w = 0; i_w < pog_u->dir_u.len_w; i_w++ ) {
    u3z(pog_u->dir_u.dat_u[i_w].bell);
    u3z(pog_u->dir_u.dat_u[i_w].ring);
  }

  u3a_free(pog_u);
}

/* _nc_prog_take(): copy a junior program to the current road.
*/
static u3nc_prog*
_nc_prog_take(u3nc_prog* pog_u)
{
  u3nc_prog* gop_u = _nc_prog_new(pog_u->byc_u.len_w,
                                  pog_u->lit_u.len_w,
                                  pog_u->dir_u.len_w,
                                  pog_u->sot_u.len_w);
  c3_w i_w;

  gop_u->tot_w = pog_u->tot_w;
  gop_u->arg_w = pog_u->arg_w;
  gop_u->ned   = u3a_take(pog_u->ned);

  memcpy(gop_u->byc_u.ops_y, pog_u->byc_u.ops_y, pog_u->byc_u.len_w);
  memcpy(gop_u->sot_u.sot_w, pog_u->sot_u.sot_w,
         pog_u->sot_u.len_w * sizeof(c3_w));

  for ( i_w = 0; i_w < pog_u->lit_u.len_w; i_w++ ) {
    gop_u->lit_u.non[i_w] = u3a_take(pog_u->lit_u.non[i_w]);
  }

  for ( i_w = 0; i_w < pog_u->dir_u.len_w; i_w++ ) {
    u3nc_dire* dir_u = &(gop_u->dir_u.dat_u[i_w]);

    *dir_u       = pog_u->dir_u.dat_u[i_w];
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
  u3_noun out = u3_nul, i, sock, pog;

  while ( u3_nul != lis ) {
    u3x_cell(lis, &i, &lis);
    u3x_cell(i, &sock, &pog);
    out = u3nc(u3nc(u3a_take(sock), _nc_take_dir_cb(pog)), out);
  }

  return u3kb_flop(out);
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
  u3h_walk_with(dir_p, _nc_reap_dir_cb, 0);
  u3h_walk_with(ent_p, _nc_reap_ent_cb, 0);
  u3h_walk_with(dir_p, _nc_link_dir_cb, 0);
  u3h_walk_with(ent_p, _nc_link_ent_cb, 0);
  u3h_free(dir_p);
  u3h_free(ent_p);
}

/* _nc_prog_mark(): mark a program for gc.
*/
static c3_w
_nc_prog_mark(u3nc_prog* pog_u)
{
  c3_w i_w, tot_w = u3a_mark_mptr(pog_u);

  tot_w += u3a_mark_noun(pog_u->ned);

  for ( i_w = 0; i_w < pog_u->lit_u.len_w; i_w++ ) {
    tot_w += u3a_mark_noun(pog_u->lit_u.non[i_w]);
  }

  for ( i_w = 0; i_w < pog_u->dir_u.len_w; i_w++ ) {
    tot_w += u3a_mark_noun(pog_u->dir_u.dat_u[i_w].bell);
    tot_w += u3a_mark_noun(pog_u->dir_u.dat_u[i_w].ring);
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
  c3_w       i_w;

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

  qua_u[5] = 0;

  tot_u->nam_c = strdup("total compiled nock");
  tot_u->qua_u = qua_u;
  for ( i_w = 0; i_w < 5; i_w++ ) {
    tot_u->siz_w += qua_u[i_w]->siz_w;
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
  c3_w i_w;

  _nc_prog_fix(pog_u);

  for ( i_w = 0; i_w < pog_u->dir_u.len_w; i_w++ ) {
    _nc_dire_jet(&(pog_u->dir_u.dat_u[i_w]));
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
