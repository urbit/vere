/// @file

#include "urth.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "allocate.h"
#include "hashtable.h"
#include "imprison.h"
#include "jets.h"
#include "manage.h"
#include "options.h"
#include "retrieve.h"
#include "serial.h"
#include "ur/ur.h"
#include "vortex.h"

/* _cu_atom_to_ref(): allocate indirect atom off-loom.
*/
static inline ur_nref
_cu_atom_to_ref(ur_root_t* rot_u, u3a_atom* vat_u)
{
  ur_nref ref;
  c3_d  val_d;

  switch ( vat_u->len_w ) {
    case 2: {
      val_d = ((c3_d)vat_u->buf_w[1]) << 32
            | ((c3_d)vat_u->buf_w[0]);
      ref = ur_coin64(rot_u, val_d);
    } break;

    case 1: {
      val_d = (c3_d)vat_u->buf_w[0];
      ref = ur_coin64(rot_u, val_d);
    } break;

    default: {
      //  XX assumes little-endian
      //
      c3_y* byt_y = (c3_y*)vat_u->buf_w;
      c3_d  len_d = ((c3_d)vat_u->len_w) << 2;

      u3_assert( len_d );

      //  NB: this call will account for any trailing null bytes
      //  caused by an overestimate in [len_d]
      //
      ref = ur_coin_bytes(rot_u, len_d, byt_y);
    } break;
  }

  return ref;
}

/* _cu_box_check(): check loom allocation box for relocation pointer.
*/
static inline c3_o
_cu_box_check(u3a_noun* som_u, ur_nref* ref)
{
  u3a_box* box_u = u3a_botox(som_u);
  c3_w*    box_w = (void*)box_u;

  if ( 0xffffffff == box_w[0] ) {
    *ref = ( ((c3_d)box_w[2]) << 32
           | ((c3_d)box_w[1]) );
    return c3y;
  }

  return c3n;
}

/* _cu_box_stash(): overwrite an allocation box with relocation pointer.
*/
static inline void
_cu_box_stash(u3a_noun* som_u, ur_nref ref)
{
  u3a_box* box_u = u3a_botox(som_u);
  c3_w*    box_w = (void*)box_u;

  //  overwrite u3a_atom with reallocated reference
  //
  box_w[0] = 0xffffffff;
  box_w[1] = ref & 0xffffffff;
  box_w[2] = ref >> 32;
}

/*
**  stack frame for recording head vs tail iteration
**
**    $?  [LOM_HEAD cell=*]
**    [ref=* cell=*]
*/

#define LOM_HEAD 0xffffffffffffffffULL

typedef struct _cu_frame_s
{
  ur_nref     ref;
  u3a_cell* cel_u;
} _cu_frame;

typedef struct _cu_stack_s
{
  c3_w       pre_w;
  c3_w       siz_w;
  c3_w       fil_w;
  _cu_frame* fam_u;
} _cu_stack;

/* _cu_from_loom_next(): advance off-loom reallocation traversal.
*/
static inline ur_nref
_cu_from_loom_next(_cu_stack* tac_u, ur_root_t* rot_u, u3_noun a)
{
  while ( 1 ) {
    //  u3 direct == ur direct
    //
    if ( c3y == u3a_is_cat(a) ) {
      return (ur_nref)a;
    }
    else {
      u3a_noun* som_u = u3a_to_ptr(a);
      ur_nref   ref;

      //  check for relocation pointers
      //
      if ( c3y == _cu_box_check(som_u, &ref) ) {
        return ref;
      }
      //  reallocate indirect atoms, stashing relocation pointers
      //
      else if ( c3y == u3a_is_atom(a) ) {
        ref = _cu_atom_to_ref(rot_u, (u3a_atom*)som_u);
        _cu_box_stash(som_u, ref);
        return ref;
      }
      else {
        u3a_cell* cel_u = (u3a_cell*)som_u;

        //  reallocate the stack if full
        //
        if ( tac_u->fil_w == tac_u->siz_w ) {
          c3_w nex_w   = tac_u->pre_w + tac_u->siz_w;
          tac_u->fam_u = c3_realloc(tac_u->fam_u, nex_w * sizeof(*tac_u->fam_u));
          tac_u->pre_w = tac_u->siz_w;
          tac_u->siz_w = nex_w;
        }

        //  push a head-frame and continue into the head
        //
        {
          _cu_frame* fam_u = &(tac_u->fam_u[tac_u->fil_w++]);
          fam_u->ref   = LOM_HEAD;
          fam_u->cel_u = cel_u;
        }

        a = cel_u->hed;
        continue;
      }
    }
  }
}

/* _cu_from_loom(): reallocate [a] off loom, in [r].
*/
static ur_nref
_cu_from_loom(ur_root_t* rot_u, u3_noun a)
{
  _cu_stack tac_u = {0};
  ur_nref     ref;

  tac_u.pre_w = ur_fib10;
  tac_u.siz_w = ur_fib11;
  tac_u.fam_u = c3_malloc(tac_u.siz_w * sizeof(*tac_u.fam_u));

  ref = _cu_from_loom_next(&tac_u, rot_u, a);

  //  incorporate reallocated ref, accounting for cells
  //
  while ( tac_u.fil_w ) {
    //  peek at the top of the stack
    //
    _cu_frame* fam_u = &(tac_u.fam_u[tac_u.fil_w - 1]);

    //  [fam_u] is a head-frame; stash ref and continue into the tail
    //
    if ( LOM_HEAD == fam_u->ref ) {
      fam_u->ref = ref;
      ref        = _cu_from_loom_next(&tac_u, rot_u, fam_u->cel_u->tel);
    }
    //  [fam_u] is a tail-frame; cons refs and pop the stack
    //
    else {
      ref = ur_cons(rot_u, fam_u->ref, ref);
      _cu_box_stash((u3a_noun*)fam_u->cel_u, ref);
      tac_u.fil_w--;
    }
  }

  c3_free(tac_u.fam_u);

  return ref;
}

/* _cu_vec: parameters for cold-state hamt walk.
*/
typedef struct _cu_vec_s {
  ur_nvec_t* vec_u;
  ur_root_t* rot_u;
} _cu_vec;

/* _cu_hamt_walk(): reallocate key/value pair in hamt walk.
*/
static void
_cu_hamt_walk(u3_noun kev, void* ptr)
{
  _cu_vec*   dat_u = (_cu_vec*)ptr;
  ur_nvec_t* vec_u = dat_u->vec_u;
  ur_root_t* rot_u = dat_u->rot_u;

  vec_u->refs[vec_u->fill++] = _cu_from_loom(rot_u, kev);
}

/* _cu_all_from_loom(): reallocate essential persistent state off-loom.
**
**   NB: destroys the loom.
*/
static ur_nref
_cu_all_from_loom(ur_root_t* rot_u, ur_nvec_t* cod_u)
{
  ur_nref   ken = _cu_from_loom(rot_u, u3A->roc);
  c3_w    cod_w = u3h_wyt(u3R->jed.cod_p);
  _cu_vec dat_u = { .vec_u = cod_u, .rot_u = rot_u };

  ur_nvec_init(cod_u, cod_w);
  u3h_walk_with(u3R->jed.cod_p, _cu_hamt_walk, &dat_u);

  return ken;
}

typedef struct _cu_loom_s {
  ur_dict32_t map_u;  //  direct->indirect mapping
  u3_atom      *vat;  //  indirect atoms
  u3_noun      *cel;  //  cells
} _cu_loom;

/* _cu_ref_to_noun(): lookup/allocate [ref] on the loom.
*/
static u3_noun
_cu_ref_to_noun(ur_root_t* rot_u, ur_nref ref, _cu_loom* lom_u)
{
  switch ( ur_nref_tag(ref) ) {
    default: u3_assert(0);

    //  all ur indirect atoms have been pre-reallocated on the loom.
    //
    case ur_iatom:  return lom_u->vat[ur_nref_idx(ref)];


    //  cells were allocated off-loom in cons-order, and are traversed
    //  in the same order: we've already relocated any one we could need here.
    //
    case ur_icell:  return lom_u->cel[ur_nref_idx(ref)];

    //  u3 direct atoms are 31-bit, while ur direct atoms are 62-bit;
    //  we use a hashtable to deduplicate the non-overlapping space
    //
    case ur_direct: {
      u3_atom vat;

      if ( 0x7fffffffULL >= ref ) {
        return (u3_atom)ref;
      }
      else if ( ur_dict32_get(rot_u, &lom_u->map_u, ref, (c3_w*)&vat) ) {
        return vat;
      }
      else {
        {
          c3_w wor_w[2] = { ref & 0xffffffff, ref >> 32 };
          vat = (c3_w)u3i_words(2, wor_w);
        }

        ur_dict32_put(0, &lom_u->map_u, ref, (c3_w)vat);
        return vat;
      }
    } break;
  }
}

/* _cu_all_to_loom(): reallocate all of [rot_u] on the loom, restore roots.
**                NB: requires all roots to be cells
**                    does *not* track refcounts, which must be
**                    subsequently reconstructed via tracing.
*/
static void
_cu_all_to_loom(ur_root_t* rot_u, ur_nref ken, ur_nvec_t* cod_u)
{
  _cu_loom  lom_u = {0};
  c3_d i_d, fil_d;

  ur_dict32_grow(0, &lom_u.map_u, ur_fib11, ur_fib12);

  //  allocate all atoms on the loom.
  //
  {
    c3_d*  len_d = rot_u->atoms.lens;
    c3_y** byt_y = rot_u->atoms.bytes;

    fil_d = rot_u->atoms.fill;
    lom_u.vat = calloc(fil_d, sizeof(u3_atom));

    for ( i_d = 0; i_d < fil_d; i_d++ ) {
      lom_u.vat[i_d] = u3i_bytes(len_d[i_d], byt_y[i_d]);
    }
  }

  //  allocate all cells on the loom.
  //
  {
    ur_nref* hed = rot_u->cells.heads;
    ur_nref* tal = rot_u->cells.tails;
    u3_noun  cel;

    fil_d = rot_u->cells.fill;
    lom_u.cel = c3_calloc(fil_d * sizeof(u3_noun));

    for ( i_d = 0; i_d < fil_d; i_d++ ) {
      cel = u3nc(_cu_ref_to_noun(rot_u, hed[i_d], &lom_u),
                 _cu_ref_to_noun(rot_u, tal[i_d], &lom_u));
      lom_u.cel[i_d] = cel;
      u3r_mug(cel);
    }
  }

  //  restore kernel reference (always a cell)
  //
  u3A->roc = lom_u.cel[ur_nref_idx(ken)];

  //  restore cold jet state (always cells)
  //
  {
    c3_d  max_d = cod_u->fill;
    c3_d    i_d;
    ur_nref ref;
    u3_noun kev;

    for ( i_d = 0; i_d < max_d; i_d++) {
      ref = cod_u->refs[i_d];
      kev = lom_u.cel[ur_nref_idx(ref)];
      u3h_put(u3R->jed.cod_p, u3h(kev), u3k(u3t(kev)));
      u3z(kev);
    }
  }

  //  dispose of relocation pointers
  //
  c3_free(lom_u.cel);
  c3_free(lom_u.vat);
  ur_dict_free((ur_dict_t*)&lom_u.map_u);
}

/* _cu_realloc(): hash-cons roots off-loom, reallocate on loom.
*/
static ur_nref
_cu_realloc(FILE* fil_u, ur_root_t** tor_u, ur_nvec_t* doc_u)
{
#ifdef U3_MEMORY_DEBUG
  u3_assert(0);
#endif

  //  bypassing page tracking as an optimization
  //
  //    NB: u3m_foul() will mark all as dirty, and
  //    u3e_save() will reinstate protection flags
  //
  u3m_foul();

  //  stash event number
  //
  c3_d eve_d = u3A->eve_d;

  //  reallocate kernel and cold jet state
  //
  ur_root_t* rot_u = ur_root_init();
  ur_nvec_t  cod_u;
  ur_nref      ken = _cu_all_from_loom(rot_u, &cod_u);

  //  print [rot_u] measurements
  //
  if ( fil_u ) {
    ur_root_info(fil_u, rot_u);
    fprintf(fil_u, "\r\n");
  }

  //  reinitialize loom
  //
  //    NB: hot jet state is not yet re-established
  //
  u3m_pave(c3y);

  //  reallocate all nouns on the loom
  //
  _cu_all_to_loom(rot_u, ken, &cod_u);

  //  allocate new hot jet state
  //
  u3j_boot(c3y);

  //  establish correct refcounts via tracing
  //
  u3m_grab(u3_none);

  //  re-establish warm jet state
  //
  u3j_ream();

  //  restore event number
  //
  u3A->eve_d = eve_d;

  //  mark all pages dirty
  //
  u3m_foul();

  *tor_u = rot_u;
  *doc_u = cod_u;

  return ken;
}

/* u3u_meld(): globally deduplicate memory, returns u3a_open delta.
*/
#ifdef U3_MEMORY_DEBUG
c3_w
u3u_meld(void)
{
  fprintf(stderr, "u3: unable to meld under U3_MEMORY_DEBUG\r\n");
  return 0;
}
#else
c3_w
u3u_meld(void)
{
  c3_w       pre_w = u3a_open(u3R);
  ur_root_t* rot_u;
  ur_nvec_t  cod_u;

  u3_assert( &(u3H->rod_u) == u3R );

  _cu_realloc(stderr, &rot_u, &cod_u);

  //  dispose off-loom structures
  //
  ur_nvec_free(&cod_u);
  ur_root_free(rot_u);
  return (u3a_open(u3R) - pre_w);
}
#endif

/* BEGIN helper functions for u3u_melt
   -------------------------------------------------------------------
*/
/* _cj_warm_tap(): tap war_p to rel
*/
static void
_cj_warm_tap(u3_noun kev, void* wit)
{
  u3_noun* rel = wit;
  *rel = u3nc(u3k(kev), *rel);
}

static inline u3_weak
_cu_melt_get(u3p(u3h_root) set_p, u3_noun som)
{
  u3_post hav_p = u3h_git(set_p, som);

  if ( u3_none == hav_p ) {
    return u3_none;
  }

  //  restore tag bits from [som]
  //
  return (hav_p >> u3a_vits) | (som & 0xc0000000);
}

static inline void
_cu_melt_put(u3p(u3h_root) set_p, u3_noun som)
{
  //  strip tag bits from [som] to skip refcounts
  //
  u3_post hav_p = u3a_to_off(som);
  u3h_put(set_p, som, hav_p);
}

static void
_cu_melt_noun(u3p(u3h_root) set_p, u3_noun* mos)
{
  u3_noun som = *mos;
  u3_weak hav;

  //  skip direct atoms
  //
  if ( c3y == u3a_is_cat(som) ) {
    return;
  }

  //  [som] equals [hav], and [hav] is canonical
  //
  if ( u3_none != (hav = _cu_melt_get(set_p, som)) ) {
    if ( hav != som ) {
      u3z(som);
      *mos = u3k(hav);
    }
    return;
  }

  //  traverse subtrees
  //
  if ( c3y == u3a_is_cell(som) ) {
    u3a_cell *cel_u = u3a_to_ptr(som);
    _cu_melt_noun(set_p, &cel_u->hed);
    _cu_melt_noun(set_p, &cel_u->tel);
  }

  //  [som] is canonical
  //
  _cu_melt_put(set_p, som);
}

/* u3u_melt(): globally deduplicate memory and pack in-place.
*/
c3_w
u3u_melt(void)
{
  c3_w pre_w = u3a_open(u3R);

  // Verify that we're on the main road.
  //
  u3_assert( &(u3H->rod_u) == u3R );

  // Store a cons list of the cold jet registrations in `cod`
  //
  u3_noun cod = u3_nul;
  u3h_walk_with(u3R->jed.cod_p, _cj_warm_tap, &cod);

  u3m_reclaim();     // refresh the byte-code interpreter.

  u3h_free(u3R->cax.per_p);
  u3R->cax.per_p = u3h_new_cache(u3C.per_w);

  u3h_free(u3R->jed.cod_p);
  u3R->jed.cod_p = u3h_new();

  {
    u3p(u3h_root) set_p = u3h_new(); // temp hashtable

    _cu_melt_noun(set_p, &cod);      // melt the jets
    _cu_melt_noun(set_p, &u3A->roc); // melt the kernel

    u3h_free(set_p);  // release the temp hashtable
  }

  // re-initialize the jets
  //
  u3j_boot(c3y);

  // Put the jet registrations back. Loop over cod putting them back into the cold jet
  // dashboard. Then re-run the garbage collector.
  //
  {
    u3_noun codc = cod;

    while(u3_nul != cod) {
      u3_noun kev = u3h(cod);
      u3h_put(u3R->jed.cod_p, u3h(kev), u3k(u3t(kev)));
      cod = u3t(cod);
    }

    u3z(codc);
  }

  // remove free space
  //
  u3j_ream();
  u3m_pack();

  return (u3a_open(u3R) - pre_w);
}

/* _cu_rock_path(): format rock path.
*/
static c3_o
_cu_rock_path(c3_c* dir_c, c3_d eve_d, c3_c** out_c)
{
  c3_w  nam_w = 1 + snprintf(0, 0, "%s/.urb/roc/%" PRIu64 ".jam", dir_c, eve_d);
  c3_c* nam_c = c3_malloc(nam_w);
  c3_i ret_i;

  ret_i = snprintf(nam_c, nam_w, "%s/.urb/roc/%" PRIu64 ".jam", dir_c, eve_d);

  if ( ret_i < 0 ) {
    fprintf(stderr, "rock: path format failed (%s, %" PRIu64 "): %s\r\n",
                    dir_c, eve_d, strerror(errno));
    c3_free(nam_c);
    return c3n;
  }
  else if ( ret_i >= nam_w ) {
    fprintf(stderr, "rock: path format failed (%s, %" PRIu64 "): truncated\r\n",
                    dir_c, eve_d);
    c3_free(nam_c);
    return c3n;
  }

  *out_c = nam_c;
  return c3y;
}

/* _cu_rock_path_make(): format rock path, creating directory if necessary..
*/
static c3_o
_cu_rock_path_make(c3_c* dir_c, c3_d eve_d, c3_c** out_c)
{
  c3_w  nam_w = 1 + snprintf(0, 0, "%s/.urb/roc/%" PRIu64 ".jam", dir_c, eve_d);
  c3_c* nam_c = c3_malloc(nam_w);
  c3_i ret_i;

  //  create $pier/.urb/roc, if it doesn't exist
  //
  //    NB, $pier/.urb is guaranteed to already exist
  //
  {
    ret_i = snprintf(nam_c, nam_w, "%s/.urb/roc", dir_c);

    if ( ret_i < 0 ) {
      fprintf(stderr, "rock: path format failed (%s, %" PRIu64 "): %s\r\n",
                      dir_c, eve_d, strerror(errno));
      c3_free(nam_c);
      return c3n;
    }
    else if ( ret_i >= nam_w ) {
      fprintf(stderr, "rock: path format failed (%s, %" PRIu64 "): truncated\r\n",
                      dir_c, eve_d);
      c3_free(nam_c);
      return c3n;
    }

    if (  c3_mkdir(nam_c, 0700)
       && (EEXIST != errno) )
    {
      fprintf(stderr, "rock: directory create failed (%s, %" PRIu64 "): %s\r\n",
                      dir_c, eve_d, strerror(errno));
      c3_free(nam_c);
      return c3n;
    }
  }

  ret_i = snprintf(nam_c, nam_w, "%s/.urb/roc/%" PRIu64 ".jam", dir_c, eve_d);

  if ( ret_i < 0 ) {
    fprintf(stderr, "rock: path format failed (%s, %" PRIu64 "): %s\r\n",
                    dir_c, eve_d, strerror(errno));
    c3_free(nam_c);
    return c3n;
  }
  else if ( ret_i >= nam_w ) {
    fprintf(stderr, "rock: path format failed (%s, %" PRIu64 "): truncated\r\n",
                    dir_c, eve_d);
    c3_free(nam_c);
    return c3n;
  }

  *out_c = nam_c;
  return c3y;
}

static c3_o
_cu_rock_save(c3_c* dir_c, c3_d eve_d, c3_d len_d, c3_y* byt_y)
{
  c3_i fid_i;

  //  open rock file, creating the containing directory if necessary
  //
  {
    c3_c* nam_c;

    if ( c3n == _cu_rock_path_make(dir_c, eve_d, &nam_c) ) {
      return c3n;
    }

    if ( -1 == (fid_i = c3_open(nam_c, O_RDWR | O_CREAT | O_TRUNC, 0644)) ) {
      fprintf(stderr, "rock: c3_open failed (%s, %" PRIu64 "): %s\r\n",
                      dir_c, eve_d, strerror(errno));
      c3_free(nam_c);
      return c3n;
    }

    c3_free(nam_c);
  }

  //  write jam-buffer into [fid_i]
  //
  //    XX deduplicate with _write() wrapper in term.c
  //
  {
    ssize_t ret_i;

    while ( len_d > 0 ) {
      c3_w lop_w = 0;
      //  retry interrupt/async errors
      //
      do {
        //  abort pathological retry loop
        //
        if ( 100 == ++lop_w ) {
          fprintf(stderr, "rock: write loop: %s\r\n", strerror(errno));
          close(fid_i);
          //  XX unlink file?
          //
          return c3n;
        }

        ret_i = write(fid_i, byt_y, len_d);
      }
      while (  (ret_i < 0)
            && (  (errno == EINTR)
               || (errno == EAGAIN)
               || (errno == EWOULDBLOCK) ));

      //  assert on true errors
      //
      //    NB: can't call u3l_log here or we would re-enter _write()
      //
      if ( ret_i < 0 ) {
        fprintf(stderr, "rock: write failed %s\r\n", strerror(errno));
        close(fid_i);
        //  XX unlink file?
        //
        return c3n;
      }
      //  continue partial writes
      //
      else {
        len_d -= ret_i;
        byt_y += ret_i;
      }
    }
  }

  close(fid_i);

  return c3y;
}

/* u3u_cram(): globably deduplicate memory, and write a rock to disk.
*/
#ifdef U3_MEMORY_DEBUG
c3_o
u3u_cram(c3_c* dir_c, c3_d eve_d)
{
  fprintf(stderr, "u3: unable to cram under U3_MEMORY_DEBUG\r\n");
  return c3n;
}
#else
c3_o
u3u_cram(c3_c* dir_c, c3_d eve_d)
{
  c3_o  ret_o = c3y;
  c3_d  len_d;
  c3_y* byt_y;

  u3_assert( &(u3H->rod_u) == u3R );

  {
    ur_root_t* rot_u;
    ur_nvec_t  cod_u;
    ur_nref      ken = _cu_realloc(stderr, &rot_u, &cod_u);

    {
      ur_nref roc = u3_nul;
      c3_d  max_d = cod_u.fill;
      c3_d    i_d;

      //  cons vector of cold jet-state entries onto a list
      //
      for ( i_d = 0; i_d < max_d; i_d++) {
        roc = ur_cons(rot_u, cod_u.refs[i_d], roc);
      }

      {
        c3_c* has_c = "hashboard";
        ur_nref has = ur_coin_bytes(rot_u, strlen(has_c), (c3_y*)has_c);
        roc = ur_cons(rot_u, has, roc);
      }

      roc = ur_cons(rot_u, ur_coin64(rot_u, c3__arvo),
                           ur_cons(rot_u, ken, roc));

      ur_jam(rot_u, roc, &len_d, &byt_y);
    }

    //  dispose off-loom structures
    //
    ur_nvec_free(&cod_u);
    ur_root_free(rot_u);
  }

  //  write jam-buffer into pier
  //
  if ( c3n == _cu_rock_save(dir_c, eve_d, len_d, byt_y) ) {
    ret_o = c3n;
  }

  c3_free(byt_y);

  return ret_o;
}
#endif

/* u3u_mmap_read(): open and mmap the file at [pat_c] for reading.
*/
c3_o
u3u_mmap_read(c3_c* cap_c, c3_c* pat_c, c3_d* out_d, c3_y** out_y)
{
  c3_i fid_i;
  c3_d len_d;

  //  open file
  //
  if ( -1 == (fid_i = c3_open(pat_c, O_RDONLY, 0644)) ) {
    fprintf(stderr, "%s: c3_open failed (%s): %s\r\n",
                    cap_c, pat_c, strerror(errno));
    return c3n;
  }

  //  measure file
  //
  {
    struct stat buf_b;

    if ( -1 == fstat(fid_i, &buf_b) ) {
      fprintf(stderr, "%s: stat failed (%s): %s\r\n",
                      cap_c, pat_c, strerror(errno));
      close(fid_i);
      return c3n;
    }

    len_d = buf_b.st_size;
  }

  //  mmap file
  //
  {
    void* ptr_v;

    if ( MAP_FAILED == (ptr_v = mmap(0, len_d, PROT_READ, MAP_SHARED, fid_i, 0)) ) {
      fprintf(stderr, "%s: mmap failed (%s): %s\r\n",
                      cap_c, pat_c, strerror(errno));
      close(fid_i);
      return c3n;
    }

    *out_d = len_d;
    *out_y = (c3_y*)ptr_v;
  }

  //  close file
  //
  close(fid_i);

  return c3y;
}

/* u3u_mmap(): open/create file-backed mmap at [pat_c] for read/write.
*/
c3_o
u3u_mmap(c3_c* cap_c, c3_c* pat_c, c3_d len_d, c3_y** out_y)
{
  c3_i fid_i;

  //  open file
  //
  if ( -1 == (fid_i = c3_open(pat_c, O_RDWR | O_CREAT | O_TRUNC, 0644)) ) {
    fprintf(stderr, "%s: c3_open failed (%s): %s\r\n",
                    cap_c, pat_c, strerror(errno));
    return c3n;
  }

  //  grow [fid_i] to [len_w]
  //
  //    XX build with _FILE_OFFSET_BITS == 64 ?
  //
  if ( 0 != ftruncate(fid_i, len_d) ) {
    fprintf(stderr, "%s: ftruncate grow %s: %s\r\n",
                    cap_c, pat_c, strerror(errno));
    close(fid_i);
    return c3n;
  }

  //  mmap file
  //
  {
    void* ptr_v;

    if ( MAP_FAILED == (ptr_v = mmap(0, len_d, PROT_READ|PROT_WRITE, MAP_SHARED, fid_i, 0)) ) {
      fprintf(stderr, "%s: mmap failed (%s): %s\r\n",
                      cap_c, pat_c, strerror(errno));
      close(fid_i);
      return c3n;
    }

    *out_y = (c3_y*)ptr_v;
  }

  //  close file
  //
  close(fid_i);

  return c3y;
}

/* u3u_mmap_save(): sync file-backed mmap.
*/
c3_o
u3u_mmap_save(c3_c* cap_c, c3_c* pat_c, c3_d len_d, c3_y* byt_y)
{
  if ( 0 != msync(byt_y, len_d, MS_SYNC) ) {
    fprintf(stderr, "%s: msync %s: %s\r\n", cap_c, pat_c, strerror(errno));
    return c3n;
  }

  return c3y;
}

/* u3u_munmap(): unmap the region at [byt_y].
*/
c3_o
u3u_munmap(c3_d len_d, c3_y* byt_y)
{
  if ( 0 != munmap(byt_y, len_d) ) {
    return c3n;
  }

  return c3y;
}

/* u3u_uncram(): restore persistent state from a rock.
*/
c3_o
u3u_uncram(c3_c* dir_c, c3_d eve_d)
{
  c3_c* nam_c;
  c3_d  len_d;
  c3_y* byt_y;

  //  load rock file into buffer
  //
  if ( c3n == _cu_rock_path(dir_c, eve_d, &nam_c) ) {
    fprintf(stderr, "uncram: failed to make rock path (%s, %" PRIu64 ")\r\n",
                    dir_c, eve_d);
    return c3n;
  }
  else if ( c3n == u3u_mmap_read("rock", nam_c, &len_d, &byt_y) ) {
    c3_free(nam_c);
    return c3n;
  }

  //  bypassing page tracking as an optimization
  //
  //    NB: u3m_foul() will mark all as dirty, and
  //    u3e_save() will reinstate protection flags
  //
  u3m_foul();

  //  reinitialize loom
  //
  //    NB: hot jet state is not yet re-established
  //
  u3m_pave(c3y);

  //  cue rock, restore persistent state
  //
  //    XX errors are fatal, barring a full "u3m_reboot"-type operation.
  //
  {
    //  XX tune the initial dictionary size for less reallocation
    //
    u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib33, ur_fib34);
    u3_weak        ref = u3s_cue_xeno_with(sil_u, len_d, byt_y);
    u3_noun   roc, doc, tag, cod;

    u3s_cue_xeno_done(sil_u);

    if ( u3_none == ref ) {
      fprintf(stderr, "uncram: failed to cue rock\r\n");
      c3_free(nam_c);
      return c3n;
    }
    else if (  c3n == u3r_pq(ref, c3__arvo, &roc, &doc)
            || (c3n == u3r_cell(doc, &tag, &cod))
            || (c3n == u3r_sing_c("hashboard", tag)) )
   {
      fprintf(stderr, "uncram: failed: invalid rock format\r\n");
      u3z(ref);
      c3_free(nam_c);
      return c3n;
    }

    u3A->roc = u3k(roc);
    u3j_load(u3k(cod));

    u3z(ref);
  }

  u3u_munmap(len_d, byt_y);

  //  allocate new hot jet state; re-establish warm
  //
  u3j_boot(c3y);
  u3j_ream();

  //  restore event number
  //
  u3A->eve_d = eve_d;

  //  leave rocks on disk
  //
  // if ( 0 != c3_unlink(nam_c) ) {
  //   fprintf(stderr, "uncram: failed to delete rock (%s, %" PRIu64 "): %s\r\n",
  //                   dir_c, eve_d, strerror(errno));
  //   c3_free(nam_c);
  //   return c3n;
  // }

  c3_free(nam_c);

  return c3y;
}
