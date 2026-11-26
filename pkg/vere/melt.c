#include "allocate.h"
#include "canon.h"
#include "events.h"
#include "hashtable.h"
#include "jets.h"
#include "options.h"
#include "retrieve.h"
#include "vortex.h"
#include "vere.h"

/* _canon_hash(): table hash, from the (cached) mug.
*/
static inline c3_d
_canon_hash(u3_noun som)
{
  return u3r_mug(som) * 11400714819323198485ULL;
}

/* the tables of canonical atoms and cells (see canon.h).
*/
#define NAME    _canon_atoms
#define KEY_TY  u3_atom
#define HASH_FN _canon_hash
#define CMPR_FN _canon_cmp_atoms
#include "verstable.h"

#define NAME    _canon_cells
#define KEY_TY  u3_cell
#define HASH_FN _canon_hash
#define CMPR_FN _canon_cmp_cells
#include "verstable.h"

/* the report's table of nouns that have duplicates: the copy kept, to
** the label last charged for one of its own (low bits, 0 for none) and
** whether that label has been charged for having an equal elsewhere.
*/
static inline c3_d
_melt_hash_ptr(u3_noun som)
{
  return (c3_d)som * 11400714819323198485ULL;
}

static inline c3_i
_melt_cmp_ptr(u3_noun a, u3_noun b)
{
  return a == b;
}

#define NAME    _melt_twins
#define KEY_TY  u3_noun
#define VAL_TY  c3_h
#define HASH_FN _melt_hash_ptr
#define CMPR_FN _melt_cmp_ptr
#include "verstable.h"

#define _melt_mask  ((c3_s)0x7fff)  //  owner: a root's index plus one
#define _melt_seen  ((c3_s)0x8000)  //  a duplicate, counted
#define _melt_away  ((c3_h)0x10000) //  twin: label charged for an equal

/* _melt_root: something melt walks, and its line in the report.
*/
typedef struct {
  c3_c*         nam_c;   //  label path
  c3_y          kin_y;   //  0: a line only, 1: noun, 2: arvo's rest, 3: hamt
  c3_t          hit_t;   //  has a line
  c3_t          pik_t;   //  to be walked
  u3_noun         dat;   //  kin 1 (borrowed)
  u3p(u3h_root) har_p;   //  kin 3
  c3_ds        fre_ds;   //  bytes freed, in the order walked
  c3_d          wit_d;   //  bytes: duplicates among its own nouns
  c3_d          acr_d;   //  bytes: its own nouns with an equal elsewhere
  c3_d          dup_d;   //  bytes: duplicate cells met
  c3_d          cor_d;   //  bytes:   of them under a core's type
} _melt_root;

typedef struct {
  _canon_atoms vat_u;
  _canon_cells cel_u;
  c3_w    len_w;
  c3_w    siz_w;
  u3_noun *tac;
  c3_w    cor_w;        //  report: core types on the stack (_melt_core())
  _melt_root*  rot_u;   //  report: roots, in the order walked
  c3_w         rot_w;   //    how many
  c3_w         rom_w;   //    room for
  c3_s*        own_s;   //  report: owner of each allocation: 0, or a
                        //    root's index plus one
  c3_w         own_w;   //    entries
  _melt_twins  twi_u;   //  report: nouns that have duplicates
  c3_d         dup_d;   //  report: bytes, duplicate cells since the last line
  c3_d         cor_d;   //    of them under a core's type
} _melt_ctx;

/* _melt_size(): bytes held by an allocation of [len_w] words.
*/
static inline c3_d
_melt_size(c3_w len_w)
{
  if ( len_w > (((c3_w)1) << (u3a_page - 1)) ) {
    len_w  += (((c3_w)1) << u3a_page) - 1;
    len_w >>= u3a_page;
    len_w <<= u3a_page;
  }
  else {
    len_w = ((c3_w)1) << c3_bits_word(c3_max(len_w, u3a_minimum) - 1);
  }
  return (c3_d)len_w * sizeof(c3_w);
}

/* _melt_own(): the owner entry of the allocation [som].
*/
static inline c3_s*
_melt_own(_melt_ctx *can_u, u3_noun som)
{
  c3_w idx_w = u3a_to_off(som) >> u3a_min_log;

  u3_assert( idx_w < can_u->own_w );
  return can_u->own_s + idx_w;
}

/* _melt_core(): the cell [som] has the shape of a core's type:
**
**                 [%core p=type q=[p=[(unit term) ?(%wet %dry) vair] ...]]
**
**               with vair one of %gold, %iron, %lead and %zinc. the
**               report takes everything under such a cell for part of
**               a core's type: the type of its context, the hoon of
**               its arms, its seminoun. a test of shape, not a proof;
**               it holds as well for a noun equal to [som], so it
**               gives one answer before and after canonicalization.
*/
static inline c3_t
_melt_core(u3_noun som)
{
  u3_noun tal, coi, gab, pov, pol, var;

  if ( c3__core != u3h(som) ) return 0;

  tal = u3t(som);
  if ( c3n == u3du(tal) ) return 0;

  coi = u3t(tal);
  if ( c3n == u3du(coi) ) return 0;

  gab = u3h(coi);
  if ( c3n == u3du(gab) ) return 0;

  pov = u3t(gab);
  if ( c3n == u3du(pov) ) return 0;

  pol = u3h(pov);
  var = u3t(pov);

  return ((c3__wet == pol) || (c3__dry == pol))
      && (  (c3__gold == var) || (c3__iron == var)
         || (c3__lead == var) || (c3__zinc == var) );
}

/* _melt_twin(): account for [som], a duplicate of the kept copy [can],
**               [byt_d] bytes, met for the first time.
**
**   a noun belongs to the first label that reaches it, as in |mass,
**   and is met in that label's walk; the labels are walked one after
**   another. so of a label's copies of one noun, all but one are
**   duplicates within the label, whatever else has an equal noun; and
**   the one left is a duplicate across labels if another label has a
**   copy.
**
**   a partial melt can reach nouns of labels it doesn't walk, at any
**   point in a walk: those are left out, so a label's duplicates
**   within are the same in a partial melt as in a whole one.
*/
static void
_melt_twin(_melt_ctx *can_u, u3_noun som, u3_noun can, c3_d byt_d)
{
  c3_s own_s = *_melt_own(can_u, som) & _melt_mask;
  _melt_twins_itr tit_u = vt_get(&can_u->twi_u, can);
  c3_h lab_h;

  if ( !own_s || !can_u->rot_u[own_s - 1].pik_t ) return;

  if ( vt_is_end(tit_u) ) {
    c3_s cow_s = *_melt_own(can_u, can) & _melt_mask;

    if ( cow_s && !can_u->rot_u[cow_s - 1].pik_t ) cow_s = 0;

    tit_u = vt_insert(&can_u->twi_u, can, (c3_h)cow_s);
    u3_assert( !vt_is_end(tit_u) ); // OOM
  }

  lab_h = tit_u.data->val & _melt_mask;

  if ( lab_h == own_s ) {
    can_u->rot_u[own_s - 1].wit_d += byt_d;
    return;
  }

  //  the label last charged had the only copies so far: now one of
  //  them has an equal in another label
  //
  if ( lab_h && !(tit_u.data->val & _melt_away) ) {
    can_u->rot_u[lab_h - 1].acr_d += byt_d;
  }

  can_u->rot_u[own_s - 1].acr_d += byt_d;
  tit_u.data->val = (c3_h)own_s | _melt_away;
}

/* _melt_twin_cell(): account for the cell [som], a duplicate of [can],
**                    under a core's type or not [cor_t], unless it's been
**                    met before.
*/
static inline void
_melt_twin_cell(_melt_ctx *can_u, u3_noun som, u3_noun can, c3_t cor_t)
{
  c3_s* own_s = _melt_own(can_u, som);

  if ( !(*own_s & _melt_seen) ) {
    c3_d byt_d = _melt_size(c3_wiseof(u3a_cell));

    *own_s |= _melt_seen;
    can_u->dup_d += byt_d;
    if ( cor_t ) can_u->cor_d += byt_d;
    _melt_twin(can_u, som, can, byt_d);
  }
}

static u3_noun
_melt_canon_next(_melt_ctx *can_u, u3_noun som)
{
  _canon_atoms_itr vit_u;
  _canon_cells_itr cit_u;

  while ( 1 ) {
    if ( c3y == u3a_is_cat(som) ) return som;

    if ( c3n == u3a_is_cell(som) ) {
      vit_u  = vt_get_or_insert(&can_u->vat_u, som);
      u3_assert( !vt_is_end(vit_u) ); // OOM

      if ( can_u->own_s && (vit_u.data->key != som) ) {
        c3_s* own_s = _melt_own(can_u, som);

        if ( !(*own_s & _melt_seen) ) {
          u3a_atom* vat_u = u3a_to_ptr(som);

          *own_s |= _melt_seen;
          _melt_twin(can_u, som, vit_u.data->key,
                     _melt_size(vat_u->len_w + c3_wiseof(u3a_atom)));
        }
      }
      return vit_u.data->key;
    }

    cit_u = vt_get(&can_u->cel_u, som);
    if ( !vt_is_end(cit_u) ) {
      //  a duplicate whose children were canonical already
      //
      if ( can_u->own_s && (cit_u.data->key != som) ) {
        _melt_twin_cell(can_u, som, cit_u.data->key,
                        can_u->cor_w || _melt_core(som));
      }
      return cit_u.data->key;
    }

    if ( can_u->len_w == can_u->siz_w ) {
      can_u->siz_w += c3_min(can_u->siz_w, 1024);
      can_u->tac    = c3_realloc(can_u->tac, sizeof(*can_u->tac) * can_u->siz_w);
    }

    can_u->tac[can_u->len_w++] = u3a_to_off(som) >> u3a_vits;
    can_u->cor_w += _melt_core(som);
    som = u3h(som);
    continue;
  }
}

static inline void __attribute__((always_inline))
_melt_xchange(u3_noun new, u3_noun *som)
{
  u3_noun old = *som;

  if ( old != new ) {
    *som = u3k(new);
    u3z(old);
  }
}

static u3_noun
_melt_canon(_melt_ctx *can_u, u3_noun can)
{
  _canon_cells_itr cit_u;
  u3a_cell  *cel_u;
  u3_noun     *top;

  can_u->len_w = 0;

  can = _melt_canon_next(can_u, can);

  while ( can_u->len_w ) {
    top = &(can_u->tac[can_u->len_w - 1]);

    if ( !(*top >> (u3a_word_bits - 1)) ) {                       // head frame
      cel_u = u3to(u3a_cell, *top << u3a_vits);
      _melt_xchange(can, &cel_u->hed);
      *top |= ((c3_w)1) << (u3a_word_bits - 1);                          // tail frame
      can   = _melt_canon_next(can_u, cel_u->tel);
    }
    else {
      *top  &= (((c3_w)1) << (u3a_word_bits - 1)) - 1;
      *top <<= u3a_vits;
      cel_u  = u3to(u3a_cell, *top);
      _melt_xchange(can, &cel_u->tel);
      cit_u  = vt_get_or_insert(&can_u->cel_u, u3a_to_pom(*top));
      u3_assert( !vt_is_end(cit_u) ); // OOM
      can    = cit_u.data->key;

      if ( can_u->own_s && (can != u3a_to_pom(*top)) ) {
        _melt_twin_cell(can_u, u3a_to_pom(*top), can, !!can_u->cor_w);
      }
      can_u->cor_w -= _melt_core(u3a_to_pom(*top));

      can_u->len_w--;
    }
  }

  return can;
}

static void
_melt_canon_ptr(_melt_ctx *can_u, u3_noun *som)
{
  u3_noun can = _melt_canon(can_u, *som);
  _melt_xchange(can, som);
}

static void
_melt_walk_hamt(u3_noun kev, void* ptr_v)
{
  _melt_ctx *can_u = ptr_v;
  (void)_melt_canon(can_u, kev);
}

static c3_c*        _melt_rep_c;  //  the report
static size_t       _melt_len_i;  //    its length
static size_t       _melt_cap_i;  //    its room
static c3_c* const* _melt_pax_c;  //  label paths picked
static c3_w         _melt_pax_w;  //    how many
static c3_w         _melt_hit_w;  //    roots they matched

/* u3_melt_told(): the last u3_melt_all()'s report (u3_melt_tell).
*/
const c3_c*
u3_melt_told(void)
{
  return _melt_rep_c ? _melt_rep_c : "";
}

/* u3_melt_pick(): limit the next u3_melt_all() to the roots at or
**                 under the [len_w] label paths [pax_c].
*/
void
u3_melt_pick(c3_c* const* pax_c, c3_w len_w)
{
  _melt_pax_c = pax_c;
  _melt_pax_w = len_w;
}

/* _melt_pick(): the root labeled [nam_c] is to be walked: nothing is
**               picked, or it's at or under a picked path.
*/
static c3_t
_melt_pick(const c3_c *nam_c)
{
  c3_w i_w;

  if ( !_melt_pax_w ) {
    return 1;
  }

  for ( i_w = 0; i_w < _melt_pax_w; i_w++ ) {
    size_t len_i = strlen(_melt_pax_c[i_w]);

    if (  (0 == strncmp(nam_c, _melt_pax_c[i_w], len_i))
       && (('/' == nam_c[len_i]) || (0 == nam_c[len_i])) )
    {
      _melt_hit_w++;
      return 1;
    }
  }
  return 0;
}

/* _melt_putf(): add formatted text to the report.
*/
static void
_melt_putf(const c3_c *fmt_c, ...)
{
  c3_c    lin_c[1024];
  c3_i    len_i;
  va_list arg_u;

  va_start(arg_u, fmt_c);
  len_i = vsnprintf(lin_c, sizeof(lin_c), fmt_c, arg_u);
  va_end(arg_u);

  if ( len_i <= 0 ) return;
  if ( (size_t)len_i >= sizeof(lin_c) ) len_i = sizeof(lin_c) - 1;

  if ( (_melt_len_i + (size_t)len_i + 1) > _melt_cap_i ) {
    _melt_cap_i = c3_max(2 * _melt_cap_i, (size_t)1 << 16);
    _melt_rep_c = c3_realloc(_melt_rep_c, _melt_cap_i);
  }
  memcpy(_melt_rep_c + _melt_len_i, lin_c, (size_t)len_i);
  _melt_len_i += (size_t)len_i;
  _melt_rep_c[_melt_len_i] = 0;
}

/* _melt_mem(): [byt_ds] bytes as text in [buf_c] (16 bytes): binary
**              units, one letter, at most four digits. nothing for zero.
*/
static void
_melt_mem(c3_c *buf_c, c3_ds byt_ds)
{
  const c3_c* sig_c = (byt_ds < 0) ? "-" : "";
  c3_d        byt_d = (byt_ds < 0) ? (c3_d)-byt_ds : (c3_d)byt_ds;

  if ( !byt_d ) {
    buf_c[0] = 0;
  }
  else if ( byt_d < 1000ULL ) {
    snprintf(buf_c, 16, "%s%uB", sig_c, (c3_h)byt_d);
  }
  else if ( byt_d < (1000ULL << 10) ) {
    snprintf(buf_c, 16, "%s%.1fK", sig_c, (double)byt_d / (double)(1ULL << 10));
  }
  else if ( byt_d < (1000ULL << 20) ) {
    snprintf(buf_c, 16, "%s%.1fM", sig_c, (double)byt_d / (double)(1ULL << 20));
  }
  else {
    snprintf(buf_c, 16, "%s%.2fG", sig_c, (double)byt_d / (double)(1ULL << 30));
  }
}

/* _melt_root_add(): a root named [nam_c] of kind [kin_y], last in the
**                   order walked.
*/
static _melt_root*
_melt_root_add(_melt_ctx *can_u, const c3_c *nam_c, c3_y kin_y)
{
  _melt_root* rot_u;

  if ( can_u->rot_w == can_u->rom_w ) {
    can_u->rom_w = c3_max(2 * can_u->rom_w, 64);
    can_u->rot_u = c3_realloc(can_u->rot_u,
                              sizeof(*can_u->rot_u) * can_u->rom_w);
  }

  rot_u = &can_u->rot_u[can_u->rot_w++];
  memset(rot_u, 0, sizeof(*rot_u));
  rot_u->nam_c = strdup(nam_c);
  rot_u->kin_y = kin_y;
  return rot_u;
}

/* _melt_claim(): record [lab_s] as the owner of everything under [som]
**                that has none. linear: everything under a noun with
**                an owner has one.
*/
static void
_melt_claim(_melt_ctx *can_u, u3_noun som, c3_s lab_s)
{
  c3_w len_w = 0;

  while ( 1 ) {
    while ( c3n == u3a_is_cat(som) ) {
      c3_s* own_s = _melt_own(can_u, som);

      if ( *own_s ) break;
      *own_s = lab_s;

      if ( c3n == u3a_is_cell(som) ) break;

      if ( len_w == can_u->siz_w ) {
        can_u->siz_w += c3_min(can_u->siz_w, 1024);
        can_u->tac    = c3_realloc(can_u->tac,
                                   sizeof(*can_u->tac) * can_u->siz_w);
      }
      can_u->tac[len_w++] = u3t(som);
      som = u3h(som);
    }

    if ( !len_w ) return;
    som = can_u->tac[--len_w];
  }
}

typedef struct {
  _melt_ctx* can_u;
  c3_s       lab_s;
} _melt_claim_ctx;

static void
_melt_claim_hamt(u3_noun kev, void* ptr_v)
{
  _melt_claim_ctx* cla_u = ptr_v;
  _melt_claim(cla_u->can_u, kev, cla_u->lab_s);
}

/* _melt_claims(): find the owner of every noun under the roots: the
**                 first root to reach it, in the order walked, which
**                 is how |mass attributes memory. if it can't,
**                 produces the reason, and the report does without.
*/
static const c3_c*
_melt_claims(_melt_ctx *can_u)
{
  c3_w i_w;

  if ( c3n == u3a_is_north(u3R) ) {
    return "the home road isn't north";
  }
  if ( can_u->rot_w >= _melt_mask ) {
    return "too many labels";
  }

  can_u->own_w = (u3R->hat_p >> u3a_min_log) + 1;
  can_u->own_s = calloc(can_u->own_w, sizeof(*can_u->own_s));

  if ( !can_u->own_s ) {
    return "no memory for the owner table";
  }

  for ( i_w = 0; i_w < can_u->rot_w; i_w++ ) {
    _melt_root* rot_u = &can_u->rot_u[i_w];
    c3_s        lab_s = (c3_s)(i_w + 1);

    switch ( rot_u->kin_y ) {
      default: break;

      case 1: _melt_claim(can_u, rot_u->dat, lab_s); break;
      case 2: _melt_claim(can_u, u3A->roc, lab_s);   break;

      case 3: {
        _melt_claim_ctx cla_u = { can_u, lab_s };
        u3h_walk_with(rot_u->har_p, _melt_claim_hamt, &cla_u);
      } break;
    }
  }

  return 0;
}

/* _melt_sum: a line of the report.
*/
typedef struct {
  c3_ds fre_ds;
  c3_d   wit_d;
  c3_d   acr_d;
  c3_d   dup_d;
  c3_d   cor_d;
  c3_t   any_t;
} _melt_sum;

/* _melt_show_line(): the report line labeled [lab_c], [wid_w] wide.
*/
static void
_melt_show_line(const c3_c *lab_c, c3_w wid_w, const _melt_sum *sum_u,
                c3_t own_t, c3_t acr_t)
{
  c3_c fre_c[16], wit_c[16], acr_c[16], typ_c[16];

  _melt_mem(fre_c, sum_u->fre_ds);
  _melt_mem(wit_c, (c3_ds)sum_u->wit_d);
  _melt_mem(acr_c, (c3_ds)sum_u->acr_d);

  typ_c[0] = 0;
  if ( sum_u->dup_d ) {
    snprintf(typ_c, sizeof(typ_c), "%u%%",
             (c3_h)((100 * sum_u->cor_d) / sum_u->dup_d));
  }

  _melt_putf("%-*s %9s", (c3_i)wid_w, lab_c, fre_c);
  if ( own_t ) {
    _melt_putf(" %9s", wit_c);
    if ( acr_t ) _melt_putf(" %9s", acr_c);
    _melt_putf(" %6s", typ_c);
  }

  //  no trailing blanks
  //
  while ( _melt_len_i && (' ' == _melt_rep_c[_melt_len_i - 1]) ) {
    _melt_rep_c[--_melt_len_i] = 0;
  }
  _melt_putf("\n");
}

/* _melt_show(): the lines of roots [i_w, end_w), which share the first
**               [pre_i] bytes of their paths, as a tree [den_w] deep:
**               |mass's layout, with a total under each group.
*/
static void
_melt_show(_melt_ctx *can_u, c3_w i_w, c3_w end_w, size_t pre_i,
           c3_w den_w, c3_w wid_w, c3_t acr_t, _melt_sum *tot_u)
{
  c3_c lab_c[600];

  while ( i_w < end_w ) {
    const c3_c* nam_c = can_u->rot_u[i_w].nam_c + pre_i;
    size_t      len_i = strcspn(nam_c, "/");
    c3_t        lef_t = !nam_c[len_i];
    c3_w        j_w   = i_w + 1;
    _melt_sum   sum_u = {0};

    if ( !lef_t ) {
      while (  (j_w < end_w)
            && (0 == strncmp(can_u->rot_u[j_w].nam_c + pre_i, nam_c, len_i + 1)) )
      {
        j_w++;
      }
    }

    {
      c3_w k_w;

      for ( k_w = i_w; k_w < j_w; k_w++ ) {
        _melt_root* rot_u = &can_u->rot_u[k_w];

        if ( !rot_u->hit_t ) continue;

        sum_u.fre_ds += rot_u->fre_ds;
        sum_u.wit_d  += rot_u->wit_d;
        sum_u.acr_d  += rot_u->acr_d;
        sum_u.dup_d  += rot_u->dup_d;
        sum_u.cor_d  += rot_u->cor_d;
        sum_u.any_t  |= rot_u->fre_ds || rot_u->wit_d || rot_u->acr_d;
      }
    }

    if ( sum_u.any_t ) {
      snprintf(lab_c, sizeof(lab_c), "%*s%.*s:",
               (c3_i)(2 * den_w), "", (c3_i)len_i, nam_c);

      if ( lef_t ) {
        _melt_show_line(lab_c, wid_w, &sum_u, !!can_u->own_s, acr_t);
      }
      else {
        _melt_putf("%s\n", lab_c);
        _melt_show(can_u, i_w, j_w, pre_i + len_i + 1, den_w + 1,
                   wid_w, acr_t, 0);
        snprintf(lab_c, sizeof(lab_c), "%*s--", (c3_i)(2 * den_w), "");
        _melt_show_line(lab_c, wid_w, &sum_u, !!can_u->own_s, acr_t);
      }

      if ( tot_u ) {
        tot_u->fre_ds += sum_u.fre_ds;
        tot_u->wit_d  += sum_u.wit_d;
        tot_u->acr_d  += sum_u.acr_d;
        tot_u->dup_d  += sum_u.dup_d;
        tot_u->cor_d  += sum_u.cor_d;
      }
    }

    i_w = j_w;
  }
}

/* _melt_tell(): write the report, and print it to [fil_u] if given.
**               [par_t]: only some roots were walked, so nothing is
**               known of equals across labels.
*/
static void
_melt_tell(FILE *fil_u, _melt_ctx *can_u, c3_t par_t)
{
  c3_w wid_w = 24;
  c3_w i_w;

  //  the label column: as wide as the deepest label
  //
  for ( i_w = 0; i_w < can_u->rot_w; i_w++ ) {
    const c3_c* nam_c = can_u->rot_u[i_w].nam_c;
    c3_w        den_w = 0;

    if ( !can_u->rot_u[i_w].hit_t ) continue;

    while ( 1 ) {
      size_t len_i = strcspn(nam_c, "/");

      wid_w = c3_max(wid_w, (c3_w)((2 * den_w) + len_i + 1));
      if ( !nam_c[len_i] ) break;
      nam_c += len_i + 1;
      den_w++;
    }
  }
  wid_w = c3_min(wid_w, 56);

  _melt_putf("%-*s %9s", (c3_i)wid_w, "melt: duplicates by label", "freed");
  if ( can_u->own_s ) {
    _melt_putf(" %9s", "within");
    if ( !par_t ) _melt_putf(" %9s", "across");
    _melt_putf(" %6s", "cores");
  }
  _melt_putf("\n");

  _melt_show(can_u, 0, can_u->rot_w, 0, 0, wid_w, !par_t, 0);

  _melt_putf("\n"
             "  freed:  by this run, in the order above. a duplicate is freed\n"
             "          from the last label that holds it. rebuild/ is what\n"
             "          starting up freed from the runtime's caches.\n");
  if ( can_u->own_s ) {
    _melt_putf(
             "  within: duplicates among the label's own nouns: those |mass\n"
             "          counts for it, the first label to reach them.\n");
    if ( !par_t ) {
      _melt_putf(
             "  across: the label's own nouns that have an equal in another\n"
             "          label. each of those labels counts it, so these add\n"
             "          up to more than is freed.\n");
    }
    _melt_putf(
             "  cores:  the share of duplicate cells inside a core's type:\n"
             "          the type of its context, the hoon of its arms, its\n"
             "          seminoun. told by shape, so approximate.\n");
  }

  if ( fil_u ) {
    const c3_c* lin_c = _melt_rep_c;

    while ( *lin_c ) {
      size_t len_i = strcspn(lin_c, "\n");

      fprintf(fil_u, "%.*s\r\n", (c3_i)len_i, lin_c);
      lin_c += len_i + !!lin_c[len_i];
    }
  }
}

/* _melt_whey(): arvo's |mass report, as u3_mars_grab() asks for it:
**               a list of [label %.y noun] and [label %.n list].
**               null if arvo gives none.
*/
static u3_noun
_melt_whey(void)
{
  u3_noun sac = u3_nul;
  u3_noun sam, gon;

  {
    u3_noun pax = u3nc(c3__whey, u3_nul);
    u3_noun lyc = u3nc(u3_nul, u3_nul);
    sam = u3nt(lyc, c3n, u3nq(c3__once, u3_blip, u3_blip, pax));
  }

  gon = u3m_soft(0, u3v_peek, sam);

  {
    u3_noun tag, dat, val;

    if (  (c3y == u3r_cell(gon, &tag, &dat))
       && (u3_blip == tag)
       && (c3y == u3du(dat))
       && (c3y == u3r_pq(u3t(dat), c3__omen, 0, &val))
       && (c3y == u3r_p(val, c3__mass, &sac)) )
    {
      u3k(sac);
    }
    else {
      sac = u3_nul;
    }
  }

  u3z(gon);
  return sac;
}

/* _melt_mass(): add the leaves of the |mass node [mas] as roots, each
**               under its label path, below [pre_c]. a leaf's noun is
**               canonicalized in place; its root cell stays, as the
**               state holds it.
*/
static void
_melt_mass(_melt_ctx *can_u, u3_noun mas, const c3_c *pre_c)
{
  u3_noun lab, tal, yes, dat;
  c3_c    nam_c[512];

  if (  (c3n == u3r_cell(mas, &lab, &tal))
     || (c3y == u3du(lab))
     || (c3n == u3r_cell(tal, &yes, &dat)) )
  {
    return;
  }

  {
    c3_c* lab_c = u3r_string(lab);
    c3_c* c;

    for ( c = lab_c; *c; c++ ) {
      if ( (*c <= ' ') || ('=' == *c) || ('/' == *c) || (*c > '~') ) *c = '_';
    }
    snprintf(nam_c, sizeof(nam_c), "%s/%s", pre_c, lab_c);
    c3_free(lab_c);
  }

  if ( c3y == yes ) {
    _melt_root_add(can_u, nam_c, 1)->dat = dat;
  }
  else if ( c3n == yes ) {
    while ( c3y == u3du(dat) ) {
      _melt_mass(can_u, u3h(dat), nam_c);
      dat = u3t(dat);
    }
  }
}

c3_w
u3_melt_all(FILE *fil_u, c3_w opt_w)
{
  c3_w     pre_w = u3a_idle(u3R);
  c3_w     las_w;
  u3_noun    sac = u3_nul;
  _melt_ctx can_u = {0};

  // Verify that we're on the main road.
  //
  u3_assert( &(u3H->rod_u) == u3R );

  if ( opt_w & u3_melt_memo ) {
    u3h_free(u3R->cax.per_p);
    u3R->cax.per_p = u3h_new_cache(u3C.per_w);
  }

  if ( opt_w & u3_melt_ford ) {
    u3h_free(u3R->cax.for_p);
    u3R->cax.for_p = u3h_new_cache(u3C.per_w);
  }

  if ( (opt_w & u3_melt_memo) && (opt_w & u3_melt_ford) ) {
    u3h_free(u3R->dup_p);
    u3R->dup_p = u3h_new();
  }

  u3z(u3H->rod_u.bug.mer);

  can_u.siz_w = 32;
  can_u.tac   = c3_malloc(sizeof(*can_u.tac) * can_u.siz_w);

  vt_init(&can_u.vat_u);
  vt_init(&can_u.cel_u);
  vt_init(&can_u.twi_u);

  _melt_len_i = 0;
  _melt_hit_w = 0;
  if ( _melt_rep_c ) _melt_rep_c[0] = 0;

  if ( !(opt_w & u3_melt_mass) ) {
    _melt_pax_w = 0;
  }
  else {
    //  ask before the reclaim, which then frees what the asking cached
    //
    sac = _melt_whey();

    if ( (u3_nul == sac) && fil_u ) {
      fprintf(fil_u, "melt: no |mass report from arvo\r\n");
    }
  }

  //  the lines of the report, and the roots, in the order walked: what
  //  the startup rebuild freed, if it measured (the rebuild canonicalizes
  //  the runtime's roots before melt runs, so melt finds them clean); the
  //  reclaim; the arvo state label by label, then whatever no label
  //  reached; the runtime's tables
  //
  if ( u3m_Ream_tel_t ) {
    static const c3_c* nam_c[u3m_ream_roots] = {
      "rebuild/memo", "rebuild/ford", "rebuild/bytecode",
      "rebuild/warm", "rebuild/bash", "rebuild/hot", "rebuild/cold"
    };
    c3_w i_w;

    for ( i_w = 0; i_w < u3m_ream_roots; i_w++ ) {
      _melt_root* rot_u = _melt_root_add(&can_u, nam_c[i_w], 0);

      rot_u->fre_ds = u3m_Ream_fre_ds[i_w];
      rot_u->hit_t  = 1;
    }
  }

  {
    c3_w rec_w = can_u.rot_w;

    _melt_root_add(&can_u, "reclaim", 0);

    {
      u3_noun lis = sac;

      while ( c3y == u3du(lis) ) {
        _melt_mass(&can_u, u3h(lis), "arvo");
        lis = u3t(lis);
      }
    }

    _melt_root_add(&can_u, "arvo/rest", 2);
    _melt_root_add(&can_u, "runtime/cold", 3)->har_p = u3R->jed.cod_p;
    _melt_root_add(&can_u, "runtime/memo", 3)->har_p = u3R->cax.per_p;
    _melt_root_add(&can_u, "runtime/ford", 3)->har_p = u3R->cax.for_p;

    las_w = u3a_idle(u3R);
    u3m_reclaim();
    {
      c3_w now_w = u3a_idle(u3R);

      can_u.rot_u[rec_w].fre_ds = ((c3_ds)now_w - (c3_ds)las_w)
                                  * (c3_ds)sizeof(c3_w);
      can_u.rot_u[rec_w].hit_t  = 1;
      las_w = now_w;
    }
  }

  {
    c3_w i_w;

    for ( i_w = 0; i_w < can_u.rot_w; i_w++ ) {
      _melt_root* rot_u = &can_u.rot_u[i_w];

      rot_u->pik_t = rot_u->kin_y && _melt_pick(rot_u->nam_c);
    }
  }

  if ( opt_w & u3_melt_tell ) {
    const c3_c* why_c = _melt_claims(&can_u);

    if ( why_c && fil_u ) {
      fprintf(fil_u, "melt: reporting what's freed only, not each label's"
                     " own duplicates: %s\r\n", why_c);
    }
  }

  {
    c3_t par_t = !!_melt_pax_w;
    c3_w i_w;

    for ( i_w = 0; i_w < can_u.rot_w; i_w++ ) {
      _melt_root* rot_u = &can_u.rot_u[i_w];

      if ( !rot_u->pik_t ) continue;

      switch ( rot_u->kin_y ) {
        case 1: (void)_melt_canon(&can_u, rot_u->dat);         break;
        case 2: _melt_canon_ptr(&can_u, &(u3A->roc));          break;
        case 3: u3h_walk_with(rot_u->har_p, _melt_walk_hamt, &can_u); break;
      }

      {
        c3_w now_w = u3a_idle(u3R);

        rot_u->fre_ds = ((c3_ds)now_w - (c3_ds)las_w) * (c3_ds)sizeof(c3_w);
        rot_u->dup_d  = can_u.dup_d;
        rot_u->cor_d  = can_u.cor_d;
        rot_u->hit_t  = 1;
        can_u.dup_d   = 0;
        can_u.cor_d   = 0;
        las_w = now_w;
      }
    }

    u3z(sac);

    if ( opt_w & u3_melt_tell ) {
      _melt_tell(fil_u, &can_u, par_t);
    }
  }

  if ( _melt_pax_w && !_melt_hit_w && fil_u ) {
    fprintf(fil_u, "melt: no root at or under the paths given\r\n");
  }
  _melt_pax_w = 0;

  //  drop registrations of unreachable batteries, once everything is
  //  canonical. the home-road hashcons set holds references to them,
  //  and this melt has canonicalized anyway.
  //
  if ( opt_w & u3_melt_cold ) {
    c3_w tot_w, dea_w;
    c3_w num_w = u3j_prune(&tot_w, &dea_w);

    u3h_free(u3R->dup_p);
    u3R->dup_p = u3h_new();

    if ( fil_u ) {
      fprintf(fil_u, "melt: dropped %" PRIc3_w " of %" PRIc3_w
                     " batteries (%" PRIc3_w " unreachable, %" PRIc3_w
                     " kept as parents)\r\n",
                     num_w, tot_w, dea_w, dea_w - num_w);
    }
  }

  u3j_boot(c3n);
  u3j_ream();

  if ( fil_u ) {
    fprintf(fil_u, "atoms (%zu)", vt_size(&can_u.vat_u));
    u3a_print_memory(fil_u, "", (c3_w)vt_bucket_count(&can_u.vat_u));
    fprintf(fil_u, "cells (%zu)", vt_size(&can_u.cel_u));
    u3a_print_memory(fil_u, "", (c3_w)vt_bucket_count(&can_u.cel_u));
  }

  vt_cleanup(&can_u.vat_u);
  vt_cleanup(&can_u.cel_u);
  vt_cleanup(&can_u.twi_u);

  {
    c3_w i_w;

    for ( i_w = 0; i_w < can_u.rot_w; i_w++ ) {
      c3_free(can_u.rot_u[i_w].nam_c);
    }
    c3_free(can_u.rot_u);
  }
  free(can_u.own_s);
  c3_free(can_u.tac);

  u3H->rod_u.bug.mer = u3i_tape("emergency buffer");

  return u3a_idle(u3R) - pre_w;
}

c3_w
u3_meld_all(FILE *fil_u, c3_w opt_w)
{
  c3_w pre_w = u3a_open(u3R);
  c3_y pha_y = u3e_Phase;

  u3e_Phase = u3e_pha_pack;

  (void)u3_melt_all(fil_u, opt_w);
  (void)u3m_pack();

  u3e_Phase = pha_y;
  return u3a_open(u3R) - pre_w;
}
