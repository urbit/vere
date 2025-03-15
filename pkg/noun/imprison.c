/// @file

#include "imprison.h"

#include "jets/k.h"
#include "jets/q.h"
#include "manage.h"
#include "retrieve.h"
#include "trace.h"
#include "xtract.h"

/* _ci_slab_size(): calculate slab bloq-size, checking for overflow.
*/
static c3_n
_ci_slab_size(c3_g met_g, c3_d len_d)
{
  c3_d bit_d = len_d << met_g;
  c3_d wor_d = (bit_d + (u3a_note_bits - 1)) >> u3a_note_bits_log;
  c3_n wor_n = (c3_n)wor_d;

  if (  (wor_n != wor_d)
     || (len_d != (bit_d >> met_g)) )
  {
    return (c3_n)u3m_bail(c3__fail);
  }
  return wor_n;
}

/* _ci_slab_init(): initialize slab with heap allocation.
**              NB: callers must ensure [len_w] >0
*/
static void
_ci_slab_init(u3i_slab* sab_u, c3_n len_n)
{
  c3_n*     nov_w = u3a_walloc(len_n + c3_wiseof(u3a_atom));
  u3a_atom* vat_u = (void *)nov_w;

  vat_u->mug_w = 0;
  vat_u->len_n = len_n;

#ifdef U3_MEMORY_DEBUG
  u3_assert( len_n );
#endif

  sab_u->_._vat_u = vat_u;
  sab_u->buf_n    = vat_u->buf_n;
  sab_u->len_n    = len_n;
}

/* _ci_slab_grow(): update slab with heap reallocation.
*/
static void
_ci_slab_grow(u3i_slab* sab_u, c3_n len_w)
{
  c3_n*     old_w = (void*)sab_u->_._vat_u;
  //    XX implement a more efficient u3a_wealloc()
  //
  c3_n*     nov_w = u3a_wealloc(old_w, len_w + c3_wiseof(u3a_atom));
  u3a_atom* vat_u = (void *)nov_w;

  vat_u->len_n = len_w;

  sab_u->_._vat_u = vat_u;
  sab_u->buf_n    = vat_u->buf_n;
  sab_u->len_n    = len_w;
}

/* _ci_atom_mint(): finalize a heap-allocated atom at specified length.
*/
static u3_atom
_ci_atom_mint(u3a_atom* vat_u, c3_n len_w)
{
  c3_n* nov_w = (void*)vat_u;

  if ( 0 == len_w ) {
    u3a_wfree(nov_w);
    return (u3_atom)0;
  }
  else if ( 1 == len_w ) {
    c3_n dat_w = *vat_u->buf_n;

    if ( c3y == u3a_is_cat(dat_w) ) {
      u3a_wfree(nov_w);
      return (u3_atom)dat_w;
    }
  }

  //  try to strip a block off the end
  //
  {
    c3_n old_w = vat_u->len_n;

    if ( old_w > len_w ) {
      c3_y wiz_y = c3_wiseof(u3a_atom);
      u3a_wtrim(nov_w, old_w + wiz_y, len_w + wiz_y);
    }
  }

  vat_u->len_n = len_w;

  return u3a_to_pug(u3a_outa(nov_w));
}

/* u3i_slab_init(): configure bloq-length slab, zero-initialize.
*/
void
u3i_slab_init(u3i_slab* sab_u, c3_g met_g, c3_d len_d)
{
  u3i_slab_bare(sab_u, met_g, len_d);

  u3t_on(mal_o);
  memset(sab_u->buf_y, 0, (size_t)sab_u->len_n * u3a_note_bytes);
  u3t_off(mal_o);
}

/* u3i_slab_bare(): configure bloq-length slab, uninitialized.
*/
void
u3i_slab_bare(u3i_slab* sab_u, c3_g met_g, c3_d len_d)
{
  u3t_on(mal_o);
  {
    c3_n wor_w = _ci_slab_size(met_g, len_d);

    //  if we only need one word, use the static storage in [sab_u]
    //
    if ( (0 == wor_w) || (1 == wor_w) ) {
      sab_u->_._vat_u = 0;
      sab_u->buf_n    = &sab_u->_._sat_n;
      sab_u->len_n    = 1;
    }
    //  allocate an indirect atom
    //
    else {
      _ci_slab_init(sab_u, wor_w);
    }
  }
  u3t_off(mal_o);
}

/* u3i_slab_from(): configure bloq-length slab, initialize with [a].
*/
void
u3i_slab_from(u3i_slab* sab_u, u3_atom a, c3_g met_g, c3_d len_d)
{
  u3i_slab_bare(sab_u, met_g, len_d);

  //  copies [a], zero-initializes any additional space
  //
  u3r_notes(0, sab_u->len_n, sab_u->buf_n, a);

  //  if necessary, mask off extra most-significant bits
  //  from most-significant word
  //
  if ( (u3a_note_bits_log > met_g) && (u3r_met(u3a_note_bits_log, a) >= sab_u->len_n) ) {
    //  NB: overflow already checked in _ci_slab_size()
    //
    c3_d bit_d = len_d << met_g;
    c3_n bit_w = bit_d & (u3a_note_bits-1);

    if ( bit_w ) {
      c3_n wor_w = bit_d >> u3a_note_bits_log;
      sab_u->buf_n[wor_w] &= ((c3_n)1 << bit_w) - 1;
    }
  }
}

/* u3i_slab_grow(): resize slab, zero-initializing new space.
*/
void
u3i_slab_grow(u3i_slab* sab_u, c3_g met_g, c3_d len_d)
{
  c3_n old_w = sab_u->len_n;

  u3t_on(mal_o);
  {
    c3_n wor_w = _ci_slab_size(met_g, len_d);

    //  XX actually shrink?
    //
    if ( wor_w <= old_w ) {
      sab_u->len_n = wor_w;
    }
    else {
      //  upgrade from static storage
      //
      if ( 1 == old_w ) {
        c3_n dat_w = *sab_u->buf_n;

        _ci_slab_init(sab_u, wor_w);
        sab_u->buf_n[0] = dat_w;
      }
      //  reallocate
      //
      else {
        _ci_slab_grow(sab_u, wor_w);
      }

      {
        c3_y*  buf_y = (void*)(sab_u->buf_n + old_w);
        size_t dif_i = wor_w - old_w;
        memset(buf_y, 0, dif_i * u3a_note_bytes);
      }
    }
  }
  u3t_off(mal_o);
}

/* u3i_slab_free(): dispose memory backing slab.
*/
void
u3i_slab_free(u3i_slab* sab_u)
{
  c3_n      len_w = sab_u->len_n;
  u3a_atom* vat_u = sab_u->_._vat_u;

  u3t_on(mal_o);

  if ( 1 == len_w ) {
    u3_assert( !vat_u );
  }
  else {
    c3_n* tav_n = (sab_u->buf_n - c3_wiseof(u3a_atom));
    u3_assert( tav_n == (c3_n*)vat_u );
    u3a_wfree(vat_u);
  }

  u3t_off(mal_o);
}

/* u3i_slab_mint(): produce atom from slab, trimming.
*/
u3_atom
u3i_slab_mint(u3i_slab* sab_u)
{
  c3_n      len_w = sab_u->len_n;
  u3a_atom* vat_u = sab_u->_._vat_u;
  u3_atom     pro;

  u3t_on(mal_o);

  if ( 1 == len_w ) {
    c3_n dat_w = *sab_u->buf_n;

    u3_assert( !vat_u );

    u3t_off(mal_o);
    pro = u3i_note(dat_w);
    u3t_on(mal_o);
  }
  else {
    u3a_atom* vat_u = sab_u->_._vat_u;
    c3_n* tav_w = (sab_u->buf_n - c3_wiseof(u3a_atom));
    u3_assert( tav_w == (c3_n*)vat_u );

    //  trim trailing zeros
    //
    while ( len_w && !(sab_u->buf_n[len_w - 1]) ) {
      len_w--;
    }

    pro = _ci_atom_mint(vat_u, len_w);
  }

  u3t_off(mal_o);

  return pro;
}

/* u3i_slab_moot(): produce atom from slab, no trimming.
*/
u3_atom
u3i_slab_moot(u3i_slab* sab_u)
{
  c3_n      len_w = sab_u->len_n;
  u3_atom     pro;

  u3t_on(mal_o);

  if ( 1 == len_w) {
    c3_n dat_w = *sab_u->buf_n;

    u3_assert( !sab_u->_._vat_u );

    u3t_off(mal_o);
    pro = u3i_note(dat_w);
    u3t_on(mal_o);
  }
  else {
    u3a_atom* vat_u = sab_u->_._vat_u;
    c3_n* tav_w = (sab_u->buf_n - c3_wiseof(u3a_atom));
    u3_assert( tav_w == (c3_n*)vat_u );

    pro = _ci_atom_mint(vat_u, len_w);
  }

  u3t_off(mal_o);

  return pro;
}

/* u3i_word(): construct u3_atom from c3_w.
*/
u3_atom
u3i_word_new(c3_w_new dat_w)
{
  u3_atom pro;

  u3t_on(mal_o);

  if ( c3y == u3a_is_cat((c3_n)dat_w) ) {
    pro = (u3_atom)dat_w;
  }
  else {
    c3_n*     nov_w = u3a_walloc(1 + c3_wiseof(u3a_atom));
    u3a_atom* vat_u = (void *)nov_w;

    vat_u->mug_w = 0;
    vat_u->len_n = 1;
    vat_u->buf_n[0] = dat_w;

    pro = u3a_to_pug(u3a_outa(nov_w));
  }

  u3t_off(mal_o);

  return pro;
}

/* u3i_word(): construct u3_atom from c3_w.
*/
u3_atom
u3i_word_tmp(c3_w_new dat_w)
{
  return u3i_word_new(dat_w);
}

/* u3i_chub(): construct u3_atom from c3_d.
*/
u3_atom
u3i_chub(c3_d dat_d)
{
#ifndef VERE64
  if ( c3y == u3a_is_cat(dat_d) ) {
    return (u3_atom)dat_d;
  }
  else {
    c3_w_new dat_w[2] = {
      dat_d & 0xffffffffULL,
      dat_d >> 32
    };

    return u3i_words_new(2, dat_w);
  }
#else
  u3_atom pro;

  u3t_on(mal_o);

  if ( c3y == u3a_is_cat(dat_d) ) {
    pro = (u3_atom)dat_d;
  }
  else {
    c3_n*     nov_w = u3a_walloc(1 + c3_wiseof(u3a_atom));
    u3a_atom* vat_u = (void *)nov_w;

    vat_u->mug_w = 0;
    vat_u->len_n = 1;
    vat_u->buf_n[0] = dat_d;

    pro = u3a_to_pug(u3a_outa(nov_w));
  }

  u3t_off(mal_o);

  return pro;
#endif
}

/* u3i_bytes(): Copy [a] bytes from [b] to an LSB first atom.
*/
u3_atom
u3i_bytes(c3_n        a_w,
          const c3_y* b_y)
{
  //  strip trailing zeroes.
  //
  while ( a_w && !b_y[a_w - 1] ) {
    a_w--;
  }

  if ( !a_w ) {
    return (u3_atom)0;
  }
  else {
    u3i_slab sab_u;

    u3i_slab_bare(&sab_u, 3, a_w);

    u3t_on(mal_o);
    {
      //  zero-initialize last word
      //
      sab_u.buf_n[sab_u.len_n - 1] = 0;
      memcpy(sab_u.buf_y, b_y, a_w);
    }
    u3t_off(mal_o);

    return u3i_slab_moot_bytes(&sab_u);
  }
}

/* u3i_words(): Copy [a] words from [b] into an atom.
*/
u3_atom
u3i_words_new(c3_n        a_w,
          const c3_w_new* b_w)
{
  //  strip trailing zeroes.
  //
  while ( a_w && !b_w[a_w - 1] ) {
    a_w--;
  }

  if ( !a_w ) {
    return (u3_atom)0;
  }
  else if ( 1 == a_w ) {
    return u3i_word_new(b_w[0]);
  }
  else {
    u3i_slab sab_u;
    u3i_slab_bare(&sab_u, 5, a_w);

    u3t_on(mal_o);
    memcpy(sab_u.buf_w, b_w, (size_t)4 * a_w);
    u3t_off(mal_o);

    return u3i_slab_moot(&sab_u);
  }
}

/* u3i_words(): Copy [a] words from [b] into an atom.
*/
u3_atom
u3i_words_tmp(c3_n        a_w,
          const c3_w_new* b_w)
{
  return u3i_words_new(a_w, b_w);
}


/* u3i_chubs(): Copy [a] chubs from [b] into an atom.
*/
u3_atom
u3i_chubs(c3_n        a_w,
          const c3_d* b_d)
{
  //  strip trailing zeroes.
  //
  while ( a_w && !b_d[a_w - 1] ) {
    a_w--;
  }

  if ( !a_w ) {
    return (u3_atom)0;
  }
  else if ( 1 == a_w ) {
    return u3i_chub(b_d[0]);
  }
  else {
    u3i_slab sab_u;
    u3i_slab_bare(&sab_u, 6, a_w);

    u3t_on(mal_o);
#ifndef VERE64
// XX: why exactly different?
    {
      c3_w_tmp* buf_w = sab_u.buf_w;
      c3_w_tmp    i_w;
      c3_d    i_d;

      for ( i_w = 0; i_w < a_w; i_w++ ) {
        i_d = b_d[i_w];
        *buf_w++ = i_d & 0xffffffffULL;
        *buf_w++ = i_d >> 32;
      }
    }
#else
    memcpy(sab_u.buf_d, b_d, (size_t)8 * a_w);
#endif
    u3t_off(mal_o);

    return u3i_slab_mint(&sab_u);
  }
}

u3_atom
u3i_note(c3_n dat_n) {
  return
#ifndef VERE64
    u3i_word_new(dat_n);
#else
    u3i_chub(dat_n);
#endif
}

u3_atom
u3i_notes(c3_n        a_n,
          const c3_n* b_n) {
  return
#ifndef VERE64
    u3i_words_new(a_n, b_n);
#else
    u3i_chubs(a_n, b_n);
#endif
}

/* u3i_mp(): Copy the GMP integer [a] into an atom, and clear it.
*/
u3_atom
u3i_mp(mpz_t a_mp)
{
  size_t   siz_i = mpz_sizeinbase(a_mp, 2);
  u3i_slab sab_u;
  u3i_slab_init(&sab_u, 0, siz_i);

  mpz_export(sab_u.buf_n, 0, -1, sizeof(c3_n), 0, 0, a_mp);
  mpz_clear(a_mp);

  //  per the mpz_export() docs:
  //
  //    > If op is non-zero then the most significant word produced
  //    >  will be non-zero.
  //
  return u3i_slab_moot(&sab_u);
}

/* u3i_vint(): increment [a].
*/
u3_atom
u3i_vint(u3_noun a)
{
  u3_assert(u3_none != a);

  if ( _(u3a_is_cat(a)) ) {
    return ( a == u3a_direct_max ) ? u3i_note(a + 1) : (a + 1);
  }
  else if ( _(u3a_is_cell(a)) ) {
    return u3m_bail(c3__exit);
  }
  else {
    mpz_t a_mp;

    u3r_mp(a_mp, a);
    u3z(a);

    mpz_add_ui(a_mp, a_mp, 1);
    return u3i_mp(a_mp);
  }
}

/* u3i_defcons(): allocate cell for deferred construction.
**            NB: [hed] and [tel] pointers MUST be filled.
*/
u3_cell
u3i_defcons(u3_noun** hed, u3_noun** tel)
{
  u3_noun pro;

  u3t_on(mal_o);
  {
    c3_n*     nov_w = u3a_celloc();
    u3a_cell* nov_u = (void *)nov_w;

    nov_u->mug_w = 0;

#ifdef U3_MEMORY_DEBUG
    nov_u->hed = u3_none;
    nov_u->tel = u3_none;
#endif

    *hed = &nov_u->hed;
    *tel = &nov_u->tel;

    pro = u3a_to_pom(u3a_outa(nov_w));
  }
  u3t_off(mal_o);

  return pro;
}

/* u3i_cell(): Produce the cell `[a b]`.
*/
u3_noun
u3i_cell(u3_noun a, u3_noun b)
{
  u3_noun pro;

  u3t_on(mal_o);
  {
    c3_n*     nov_w = u3a_celloc();
    u3a_cell* nov_u = (void *)nov_w;

    nov_u->mug_w = 0;
    nov_u->hed = a;
    nov_u->tel = b;

    pro = u3a_to_pom(u3a_outa(nov_w));
  }
  u3t_off(mal_o);

  return pro;
}

/* u3i_trel(): Produce the triple `[a b c]`.
*/
u3_noun
u3i_trel(u3_noun a, u3_noun b, u3_noun c)
{
  return u3i_cell(a, u3i_cell(b, c));
}

/* u3i_qual(): Produce the cell `[a b c d]`.
*/
u3_noun
u3i_qual(u3_noun a, u3_noun b, u3_noun c, u3_noun d)
{
  return u3i_cell(a, u3i_trel(b, c, d));
}

/* u3i_string(): Produce an LSB-first atom from the C string [a].
*/
u3_atom
u3i_string(const c3_c* a_c)
{
  return u3i_bytes(strlen(a_c), (c3_y *)a_c);
}

/* u3i_tape(): from a C string, to a list of bytes.
*/
u3_noun
u3i_tape(const c3_c* txt_c)
{
  if ( !*txt_c ) {
    return u3_nul;
  } else return u3i_cell(*txt_c, u3i_tape(txt_c + 1));
}

/* u3i_list(): list from `u3_none`-terminated varargs.
*/
u3_noun
u3i_list(u3_weak som, ...)
{
  u3_noun lit = u3_nul;
  va_list  ap;

  if ( u3_none == som ) {
    return lit;
  }
  else {
    lit = u3nc(som, lit);
  }

  {
    u3_noun tem;

    va_start(ap, som);
    while ( 1 ) {
      if ( u3_none == (tem = va_arg(ap, u3_weak)) ) {
        break;
      }
      else {
        lit = u3nc(tem, lit);
      }
    }
    va_end(ap);
  }

  return u3kb_flop(lit);
}

/* u3i_edit():
**
**   Mutate `big` at axis `axe` with new value `som`.
**   `axe` is RETAINED.
*/
u3_noun
u3i_edit(u3_noun big, u3_noun axe, u3_noun som)
{
  u3_noun  pro;
  u3_noun* out = &pro;

  switch ( axe ) {
    case 0: return u3m_bail(c3__exit);
    case 1: break;

    default: {
      c3_w_tmp        dep_w = u3r_met(0, u3x_atom(axe)) - 2;
      const c3_n* axe_w = ( c3y == u3a_is_cat(axe) )
                        ? &axe
                        : ((u3a_atom*)u3a_to_ptr(axe))->buf_n;

      do {
        u3a_cell*  big_u = u3a_to_ptr(big);
        u3_noun*     old = (u3_noun*)&(big_u->hed);
        const c3_y bit_y =
          1 & ( axe_w[dep_w >> u3a_note_bits_log] >> 
                (dep_w & (u3a_note_bits - 1)) );

        if ( c3n == u3a_is_cell(big) ) {
          return u3m_bail(c3__exit);
        }
        else if ( c3y == u3a_is_mutable(u3R, big) ) {
          *out = big;
          out  = &(old[bit_y]);
          big  = *out;
          big_u->mug_w = 0;
        }
        else  {
          u3_noun  luz = big;
          u3_noun* new[2];

          *out = u3i_defcons(&new[0], &new[1]);
          out  = new[bit_y];
          big  = u3k(old[bit_y]);
          *(new[!bit_y]) = u3k(old[!bit_y]);

          u3z(luz);
        }
      }
      while ( dep_w-- );
    }
  }

  u3z(big);
  *out = som;
  return pro;
}

/* u3i_molt():
**
**   Mutate `som` with a 0-terminated list of axis, noun pairs.
**   Axes must be cats (31 bit).
*/
  struct _molt_pair {
    c3_w_tmp    axe_w;
    u3_noun som;
  };

  static c3_w_tmp
  _molt_cut(c3_w_tmp               len_w,
            struct _molt_pair* pms_m)
  {
    c3_w_tmp i_w, cut_t, cut_w;

    cut_t = 0;
    cut_w = 0;
    for ( i_w = 0; i_w < len_w; i_w++ ) {
      c3_w_tmp axe_w = pms_m[i_w].axe_w;

      if ( (cut_t == 0) && (3 == u3x_cap(axe_w)) ) {
        cut_t = 1;
        cut_w = i_w;
      }
      pms_m[i_w].axe_w = u3x_mas(axe_w);
    }
    return cut_t ? cut_w : i_w;
  }

  static u3_noun                            //  transfer
  _molt_apply(u3_noun            som,       //  retain
              c3_w_tmp               len_w,
              struct _molt_pair* pms_m)     //  transfer
  {
    if ( len_w == 0 ) {
      return u3k(som);
    }
    else if ( (len_w == 1) && (1 == pms_m[0].axe_w) ) {
      return pms_m[0].som;
    }
    else {
      c3_w_tmp cut_w = _molt_cut(len_w, pms_m);

      if ( c3n == u3a_is_cell(som) ) {
        return u3m_bail(c3__exit);
      }
      else {
        return u3i_cell
           (_molt_apply(u3a_h(som), cut_w, pms_m),
            _molt_apply(u3a_t(som), (len_w - cut_w), (pms_m + cut_w)));
      }
    }
  }

u3_noun
u3i_molt(u3_noun som, ...)
{
  va_list            ap;
  c3_w_tmp               len_w;
  struct _molt_pair* pms_m;
  u3_noun            pro;

  //  Count.
  //
  len_w = 0;
  {
    va_start(ap, som);
    while ( 1 ) {
      if ( 0 == va_arg(ap, c3_w_tmp) ) {
        break;
      }
      va_arg(ap, u3_weak*);
      len_w++;
    }
    va_end(ap);
  }

  u3_assert( 0 != len_w );
  pms_m = alloca(len_w * sizeof(struct _molt_pair));

  //  Install.
  //
  {
    c3_w_tmp i_w;

    va_start(ap, som);
    for ( i_w = 0; i_w < len_w; i_w++ ) {
      pms_m[i_w].axe_w = va_arg(ap, c3_w_tmp);
      pms_m[i_w].som = va_arg(ap, u3_noun);
    }
    va_end(ap);
  }

  //  Apply.
  //
  pro = _molt_apply(som, len_w, pms_m);
  u3z(som);
  return pro;
}
