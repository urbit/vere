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

  u3h_free(u3R->cax.for_p);
  u3R->cax.for_p = u3h_new_cache(u3C.per_w);

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

/* u3u_cram(): jam persistent state (rock) and write it to disk.
**
**   serializes on-loom state directly through the blob-aware jam:
**   bob atoms are expanded into the rock as regular atoms, their
**   bytes mmap'd straight from the blob store.  rocks stay portable
**   (standard jam; no blob refs), and the loom — including the blob
**   bank — is untouched.  formerly routed through an off-loom ur
**   rebuild, which repaved the home road (wiping blb_p) and read bob
**   atoms' len_w raw; deduplication is |meld's job, not cram's.
*/
c3_o
u3u_cram(c3_c* dir_c, c3_d eve_d)
{
  c3_o  ret_o = c3y;
  c3_d  len_d;
  c3_y* byt_y;

  u3_assert( &(u3H->rod_u) == u3R );

  {
    u3_noun cod = u3_nul;
    u3h_walk_with(u3R->jed.cod_p, _cj_warm_tap, &cod);

    u3_noun roc = u3nc(c3__arvo,
                       u3nc(u3k(u3A->roc),
                            u3nc(u3i_string("hashboard"), cod)));

    u3s_jam_xeno(roc, &len_d, &byt_y);
    u3z(roc);
  }

  //  write jam-buffer into pier
  //
  if ( c3n == _cu_rock_save(dir_c, eve_d, len_d, byt_y) ) {
    ret_o = c3n;
  }

  c3_free(byt_y);

  return ret_o;
}

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
