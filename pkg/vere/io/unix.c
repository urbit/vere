/// @file

/*
**  this file is responsible for maintaining a bidirectional
**  mapping between the contents of a clay desk and a directory
**  in a unix filesystem.
**
**  TODO  this driver is crufty and overdue for a rewrite.
**  aspirationally, the rewrite should do sanity checking and
**  transformations at the noun level to convert messages from
**  arvo into sets of fs operations on trusted inputs, and
**  inverse transformations and checks for fs contents to arvo
**  messages.
**
**  the two relevant transformations to apply are:
**
**  1. bidirectionally map file contents to atoms
**  2. bidirectionally map arvo $path <-> unix relative paths
**
**  the first transform is trivial. the second poses some
**  challenges: an arvo $path is a list of $knot, and the $knot
**  space intersects with invalid unix paths in the three cases
**  of: %$ (the empty knot), '.', and '..'. we escape these by
**  prepending a '!' to the filename corresponding to the $knot,
**  yielding unix files named '!', '!.', and '!..'.
**
**  there is also the case of the empty path. we elide empty
**  paths from this wrapper, which always uses the last path
**  component as the file extension/mime-type.
**
**  these transforms are implemented, but they ought to be
**  implemented in one place, prior to any fs calls; as-is, they
**  are sprinkled throughout the file updating code.
**
*/

#include "vere.h"

#include <ftw.h>

#include "noun.h"

struct _u3_umon;
struct _u3_udir;
struct _u3_ufil;
struct _u3_unix;

/* auto-sync heuristics, tuned for editor save patterns (write-temp-
** then-rename, delete-then-write, truncate-then-write):
**
**   SYNC_QUIET_MS:   scan only after the filesystem has been quiet
**                    this long, so multi-step saves coalesce
**   SYNC_CAP_MS:     but never delay a scan longer than this past
**                    the first change, so a busy writer can't
**                    starve sync entirely
**   SYNC_GRACE_MS:   a missing file must stay missing this long
**                    before its deletion is synced, so a file
**                    deleted moments before being rewritten never
**                    propagates a transient deletion
**   SYNC_RECHECK_MS: how soon to recheck files awaiting the grace
**                    period (must exceed SYNC_GRACE_MS)
**   SYNC_SWEEP_MS:   backstop: rescan auto-synced mounts at least
**                    this often, in case an fs event was dropped
**                    (inotify queue overflow, unwatchable edge
**                    cases, etc)
*/
#define SYNC_QUIET_MS    100
#define SYNC_CAP_MS     1000
#define SYNC_GRACE_MS    300
#define SYNC_RECHECK_MS  350
#define SYNC_SWEEP_MS  30000

/* u3_unod: file or directory.
*/
  typedef struct _u3_unod {
    c3_o              dir;              //  c3y if dir, c3n if file
    c3_o              dry;              //  ie, unmodified
    c3_c*             pax_c;            //  absolute path
    struct _u3_udir*  par_u;            //  parent
    struct _u3_unod*  nex_u;            //  internal list
  } u3_unod;

/* u3_ufil: synchronized file.
*/
  typedef struct _u3_ufil {
    c3_o              dir;              //  c3y if dir, c3n if file
    c3_o              dry;              //  ie, unmodified
    c3_c*             pax_c;            //  absolute path
    struct _u3_udir*  par_u;            //  parent
    struct _u3_unod*  nex_u;            //  internal list
    c3_w              gum_w;            //  mug of last %ergo
    c3_d              dum_d;            //  first seen missing (ms), or 0
  } u3_ufil;

/* u3_ufil: synchronized directory.
*/
  typedef struct _u3_udir {
    c3_o              dir;              //  c3y if dir, c3n if file
    c3_o              dry;              //  ie, unmodified
    c3_c*             pax_c;            //  absolute path
    struct _u3_udir*  par_u;            //  parent
    struct _u3_unod*  nex_u;            //  internal list
    u3_unod*          kid_u;            //  subnodes
    struct _u3_uwat*  wat_u;            //  fs-event watcher, if any
  } u3_udir;

/* u3_uwat: fs-event watcher on a directory.
*/
  typedef struct _u3_uwat {
    uv_fs_event_t     eve_u;            //  libuv handle, must be first
    struct _u3_unix*  unx_u;            //  driver backpointer
    struct _u3_udir*  dir_u;            //  watched directory, 0 once freed
  } u3_uwat;

/* u3_usyc: files included in an injected %into event, used to
**          update mug state once the event commits.
*/
  typedef struct _u3_usyc {
    struct _u3_unix*  unx_u;            //  driver backpointer
    c3_c*             nam_c;            //  mount point name
    c3_w              len_w;            //  entries used
    c3_w              siz_w;            //  entries allocated
    struct _u3_usye {
      c3_c*           pax_c;            //  absolute unix path
      c3_w            mug_w;            //  mug of content sent
    }* ent_u;                           //  entry array
  } u3_usyc;

/* u3_umug: a persisted mug-cache entry.
*/
  typedef struct _u3_umug {
    c3_c*             pax_c;            //  absolute unix path
    c3_w              mug_w;            //  mug of content at last sync
  } u3_umug;

/* u3_ufil: synchronized mount point.
*/
  typedef struct _u3_umon {
    u3_udir          dir_u;             //  root directory, must be first
    c3_c*            nam_c;             //  mount point name
    c3_o             syn_o;             //  auto-sync (fs-event watch) on
    u3_umug*         lod_u;             //  mug cache loaded from disk
    c3_w             lod_w;             //  entries in lod_u
    struct _u3_umon* nex_u;             //  internal list
  } u3_umon;

/* u3_unix: clay support system, also
*/
  typedef struct _u3_unix {
    u3_auto     car_u;
    c3_l        sev_l;                  //  instance number
    u3_umon*    mon_u;                  //  mount points
    c3_c*       pax_c;                  //  pier directory
    c3_o        alm;                    //  timer set
    c3_o        dyr;                    //  ready to update
    u3_noun     sat;                    //  (sane %ta) handle
    uv_timer_t* syt_u;                  //  auto-sync debounce timer
    uv_timer_t* swt_u;                  //  auto-sync backstop sweep timer
    c3_d        fir_d;                  //  debounce window start (ms)
    c3_o        dum_o;                  //  doomed files await recheck
    u3_usyc*    pen_u;                  //  scan accumulator, if scanning
#ifdef SYNCLOG
    c3_w         lot_w;                 //  sync-slot
    struct _u3_sylo {
      c3_o     unx;                     //  from unix
      c3_m     wer_m;                   //  mote saying where
      c3_m     wot_m;                   //  mote saying what
      c3_c*    pax_c;                   //  path
    } sylo[1024];
#endif
  } u3_unix;

void
u3_unix_ef_look(u3_unix* unx_u, u3_noun mon, u3_noun all);

static void
_unix_update_mount(u3_unix* unx_u, u3_umon* mon_u, u3_noun all);

static void
_unix_mark_wet(u3_udir* dir_u);

static void
_unix_save_mugs(u3_unix* unx_u, u3_umon* mon_u);

static void
_unix_drop_mugs(u3_unix* unx_u, u3_umon* mon_u);

static u3_umon*
_unix_node_mount(u3_unix* unx_u, u3_unod* nod_u);

static void
_unix_seed_mug(struct _u3_unix* unx_u, u3_ufil* fil_u);

static void
_unix_free_lod(u3_umon* mon_u);

static void
_unix_lod_del(struct _u3_unix* unx_u, u3_ufil* fil_u);

static c3_i
_unix_lod_cmp(const void* lef_v, const void* rit_v);

/* u3_unix_cane(): true iff (unix) path is canonical.
*/
c3_t
u3_unix_cane(const c3_c* pax_c)
{
  if ( 0 == pax_c ) {
    return 0;
  }
  //  allow absolute paths.
  //
  if ( '/' == *pax_c ) {
    pax_c++;
    //  allow root.
    //
    if ( 0 == *pax_c ) {
      return 1;
    }
  }
  do {
    if (  0 == *pax_c
       || 0 == strcmp(".",    pax_c)
       || 0 == strcmp("..",   pax_c)
       || 0 == strncmp("/",   pax_c, 1)
       || 0 == strncmp("./",  pax_c, 2)
       || 0 == strncmp("../", pax_c, 3) )
    {
      return 0;
    }
    pax_c = strchr(pax_c, '/');
  } while ( 0 != pax_c++ );
  return 1;
}

/* _unix_sane_ta(): true iff pat is a valid @ta
**
**  %ta is parsed by:
**      (star ;~(pose nud low hep dot sig cab))
*/
static c3_t
_unix_sane_ta(u3_unix* unx_u, u3_atom pat)
{
  return _(u3n_slam_on(u3k(unx_u->sat), pat));
}

/* u3_readdir_r():
*/
c3_w
u3_readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
  errno = 0;
  struct dirent * tmp_u = readdir(dirp);

  if (NULL == tmp_u){
    *result = NULL;
    return (errno);  // either success or error code
  } else {
    memcpy(entry, tmp_u, tmp_u->d_reclen);
    *result = entry;
  }

  return(0);
}

/* _unix_string_to_knot(): convert c unix path component to $knot
*/
static u3_atom
_unix_string_to_knot(c3_c* pax_c)
{
  u3_assert(pax_c);
  //  XX  this can happen if we encounter a file without an extension.
  //
  // u3_assert(*pax_c);
  u3_assert(!strchr(pax_c, '/'));
  //  XX  horrible
  //
# ifdef _WIN32
  u3_assert(!strchr(pax_c, '\\'));
# endif
  if ( '!' == *pax_c ) {
    pax_c++;
  }
  return u3i_string(pax_c);
}

/* _unix_knot_to_string(): convert $knot to c unix path component. RETAIN.
*/
static c3_c*
_unix_knot_to_string(u3_atom pon)
{
  c3_c* ret_c;

  if (  u3_nul != pon
     && c3_s1('.') != pon
     && c3_s2('.','.') != pon
     && '!' != u3r_byte(0, pon) )
  {
    ret_c = u3r_string(pon);
  }
  else {
    c3_w  met_w = u3r_met(3, pon);

    ret_c = c3_malloc(met_w + 2);
    *ret_c = '!';
    u3r_bytes(0, met_w, (c3_y*)ret_c + 1, pon);
    ret_c[met_w + 1] = 0;
  }
  u3_assert(!strchr(ret_c, '/'));
# ifdef _WIN32
  u3_assert(!strchr(ret_c, '\\'));
# endif
  return ret_c;
}

/* _unix_down(): descend path.
*/
static c3_c*
_unix_down(c3_c* pax_c, c3_c* sub_c)
{
  c3_w pax_w = strlen(pax_c);
  c3_w sub_w = strlen(sub_c);
  c3_c* don_c = c3_malloc(pax_w + sub_w + 2);

  strcpy(don_c, pax_c);
  don_c[pax_w] = '/';
  strcpy(don_c + pax_w + 1, sub_c);
  don_c[pax_w + 1 + sub_w] = '\0';

  return don_c;
}

/* _unix_string_to_path(): convert c string to u3_noun $path
**
**  c string must begin with the pier path plus mountpoint
*/
static u3_noun
_unix_string_to_path_helper(c3_c* pax_c)
{
  u3_noun not;

  u3_assert(pax_c[-1] == '/');
  c3_c* end_c = strchr(pax_c, '/');
  if ( !end_c ) {
    end_c = strrchr(pax_c, '.');
    if ( !end_c ) {
      return u3nc(_unix_string_to_knot(pax_c), u3_nul);
    }
    else {
      *end_c = 0;
      not = _unix_string_to_knot(pax_c);
      *end_c = '.';
      return u3nt(not, _unix_string_to_knot(end_c + 1), u3_nul);
    }
  }
  else {
    *end_c = 0;
    not = _unix_string_to_knot(pax_c);
    *end_c = '/';
    return u3nc(not, _unix_string_to_path_helper(end_c + 1));
  }
}
static u3_noun
_unix_string_to_path(u3_unix* unx_u, c3_c* pax_c)
{
  pax_c += strlen(unx_u->pax_c) + 1;
  c3_c* pox_c = strchr(pax_c, '/');
  if ( !pox_c ) {
    pox_c = strchr(pax_c, '.');
    if ( !pox_c ) {
      return u3_nul;
    }
    else {
      return u3nc(_unix_string_to_knot(pox_c + 1), u3_nul);
    }
  }
  else {
    return _unix_string_to_path_helper(pox_c + 1);
  }
}

/* _unix_mkdirp(): recursive mkdir of dirname of pax_c.
*/
static void
_unix_mkdirp(c3_c* pax_c)
{
  c3_c* fas_c = strchr(pax_c + 1, '/');

  while ( fas_c ) {
    *fas_c = 0;
    if ( 0 != mkdir(pax_c, 0777) && EEXIST != errno ) {
      u3l_log("unix: mkdir %s: %s", pax_c, strerror(errno));
      u3m_bail(c3__fail);
    }
    *fas_c++ = '/';
    fas_c = strchr(fas_c, '/');
  }
}

/* u3_unix_save(): save file under .../.urb/put or bail.
**
**  XX this is quite bad, and doesn't share much in common with
**  the rest of unix.c. a refactor would probably share common
**  logic with _unix_sync_change, perhaps using openat, making
**  unx_u optional, and/or having a flag to not track the file
**  for future changes.
*/
void
u3_unix_save(c3_c* pax_c, u3_atom pad)
{
  c3_i  fid_i;
  c3_w  lod_w, len_w, fln_w, rit_w;
  c3_y* pad_y;
  c3_c* ful_c;

  if ( !u3_unix_cane(pax_c) ) {
    u3l_log("%s: non-canonical path", pax_c);
    u3z(pad); u3m_bail(c3__fail);
  }
  if ( '/' == *pax_c) {
    pax_c++;
  }
  lod_w = strlen(u3_Host.dir_c);
  len_w = lod_w + sizeof("/.urb/put/") + strlen(pax_c);
  ful_c = c3_malloc(len_w);
  rit_w = snprintf(ful_c, len_w, "%s/.urb/put/%s", u3_Host.dir_c, pax_c);
  u3_assert(len_w == rit_w + 1);

  _unix_mkdirp(ful_c);
  fid_i = c3_open(ful_c, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if ( fid_i < 0 ) {
    u3l_log("%s: %s", ful_c, strerror(errno));
    c3_free(ful_c);
    u3z(pad); u3m_bail(c3__fail);
  }

  fln_w = u3r_met(3, pad);
  pad_y = c3_malloc(fln_w);
  u3r_bytes(0, fln_w, pad_y, pad);
  u3z(pad);
  rit_w = write(fid_i, pad_y, fln_w);
  close(fid_i);
  c3_free(pad_y);

  if ( rit_w != fln_w ) {
    u3l_log("%s: %s", ful_c, strerror(errno));
    c3_free(ful_c);
    u3m_bail(c3__fail);
  }
  c3_free(ful_c);
}

/* _unix_rm_r_cb(): callback to delete individual files/directories
*/
static c3_i
_unix_rm_r_cb(const c3_c* pax_c,
              const struct stat* buf_u,
              c3_i typeflag,
              struct FTW* ftw_u)
{
  switch ( typeflag ) {
    default:
      u3l_log("bad file type in rm_r: %s", pax_c);
      break;
    case FTW_F:
      if ( 0 != c3_unlink(pax_c) && ENOENT != errno ) {
        u3l_log("error unlinking (in rm_r) %s: %s",
                pax_c, strerror(errno));
        u3_assert(0);
      }
      break;
    case FTW_D:
      u3l_log("shouldn't have gotten pure directory: %s", pax_c);
      break;
    case FTW_DNR:
      u3l_log("couldn't read directory: %s", pax_c);
      break;
    case FTW_NS:
      u3l_log("couldn't stat path: %s", pax_c);
      break;
    case FTW_DP:
      if ( 0 != c3_rmdir(pax_c) && ENOENT != errno ) {
        u3l_log("error rmdiring %s: %s", pax_c, strerror(errno));
        u3_assert(0);
      }
      break;
    case FTW_SL:
      u3l_log("got symbolic link: %s", pax_c);
      break;
    case FTW_SLN:
      u3l_log("got nonexistent symbolic link: %s", pax_c);
      break;
  }

  return 0;
}

/* _unix_rm_r(): rm -r directory
*/
static void
_unix_rm_r(c3_c* pax_c)
{
  if ( 0 > nftw(pax_c, _unix_rm_r_cb, 100, FTW_DEPTH | FTW_PHYS )
       && ENOENT != errno) {
    u3l_log("rm_r error on %s: %s", pax_c, strerror(errno));
  }
}

/* _unix_mkdir(): mkdir, asserting.
*/
static void
_unix_mkdir(c3_c* pax_c)
{
  if ( 0 != c3_mkdir(pax_c, 0755) && EEXIST != errno) {
    u3l_log("error mkdiring %s: %s", pax_c, strerror(errno));
    u3_assert(0);
  }
}

/* _unix_write_file_hard(): write to a file, overwriting what's there
*/
static c3_w
_unix_write_file_hard(c3_c* pax_c, u3_noun mim)
{
  c3_i  fid_i = c3_open(pax_c, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  c3_w  len_w, rit_w, siz_w, mug_w = 0;
  c3_y* dat_y;

  u3_noun dat = u3t(u3t(mim));

  if ( fid_i < 0 ) {
    u3l_log("error opening %s for writing: %s",
            pax_c, strerror(errno));
    u3z(mim);
    return 0;
  }

  siz_w = u3h(u3t(mim));
  len_w = u3r_met(3, dat);
  dat_y = c3_calloc(siz_w);

  u3r_bytes(0, c3_min(len_w, siz_w), dat_y, dat);
  u3z(mim);

  rit_w = write(fid_i, dat_y, siz_w);

  if ( rit_w != siz_w ) {
    u3l_log("error writing %s: %s",
            pax_c, strerror(errno));
    mug_w = 0;
  }
  else {
    //  mug all [siz_w] bytes, matching a later scan of the file
    //
    mug_w = u3r_mug_bytes(dat_y, siz_w);
  }

  close(fid_i);
  c3_free(dat_y);

  return mug_w;
}

/* _unix_mim_mug(): mug of a mime cell's octet-stream, as bytes at its
**                  declared length (a noun mug would drop trailing zeros).
**                  retains [mim].
*/
static c3_w
_unix_mim_mug(u3_noun mim)
{
  u3_noun dat = u3t(u3t(mim));
  c3_w  siz_w = u3h(u3t(mim));
  c3_w  len_w = u3r_met(3, dat);
  c3_y* dat_y = c3_calloc((siz_w ? siz_w : 1));
  c3_w  mug_w;

  u3r_bytes(0, c3_min(len_w, siz_w), dat_y, dat);
  mug_w = u3r_mug_bytes(dat_y, siz_w);
  c3_free(dat_y);

  return mug_w;
}

/* _unix_write_file_soft(): write to a file, not overwriting if it's changed
*/
static void
_unix_write_file_soft(u3_ufil* fil_u, u3_noun mim)
{
  struct stat buf_u;
  c3_i  fid_i = c3_open(fil_u->pax_c, O_RDONLY, 0644);
  c3_ws len_ws, red_ws;
  c3_w  old_w;
  c3_y* old_y;

  if ( fid_i < 0 || fstat(fid_i, &buf_u) < 0 ) {
    if ( ENOENT == errno ) {
      goto _unix_write_file_soft_go;
    }
    else {
      u3l_log("error opening file (soft) %s: %s",
              fil_u->pax_c, strerror(errno));
      u3z(mim);
      return;
    }
  }

  len_ws = buf_u.st_size;
  old_y = c3_malloc(len_ws);

  red_ws = read(fid_i, old_y, len_ws);

  if ( close(fid_i) < 0 ) {
    u3l_log("error closing file (soft) %s: %s",
            fil_u->pax_c, strerror(errno));
  }

  if ( len_ws != red_ws ) {
    if ( red_ws < 0 ) {
      u3l_log("error reading file (soft) %s: %s",
              fil_u->pax_c, strerror(errno));
    }
    else {
      u3l_log("wrong # of bytes read in file %s: %d %d",
              fil_u->pax_c, len_ws, red_ws);
    }
    c3_free(old_y);
    u3z(mim);
    return;
  }

  old_w = u3r_mug_bytes(old_y, len_ws);
  c3_free(old_y);

  if ( old_w == _unix_mim_mug(mim) ) {
    //  the disk file already has clay's contents
    //
    fil_u->gum_w = old_w;
    u3z(mim);
    return;
  }

  if ( old_w != fil_u->gum_w ) {
    //  the file changed on disk since we last synced it; leave the
    //  edit alone, but record the mug of clay's contents so the
    //  disk file is seen as changed and synced back in
    //
    fil_u->gum_w = _unix_mim_mug(mim);
    u3z(mim);
    return;
  }

_unix_write_file_soft_go:
  fil_u->gum_w = _unix_write_file_hard(fil_u->pax_c, mim);
}

static void
_unix_watch_dir(u3_udir* dir_u, u3_udir* par_u, c3_c* pax_c);
static void
_unix_watch_file(u3_unix* unx_u, u3_ufil* fil_u, u3_udir* par_u, c3_c* pax_c);

/* _unix_get_mount_point(): retrieve or create mount point
*/
static u3_umon*
_unix_get_mount_point(u3_unix* unx_u, u3_noun mon)
{
  if ( c3n == u3ud(mon) ) {
    u3_assert(!"mount point must be an atom");
    u3z(mon);
    return NULL;
  }

  c3_c* nam_c = _unix_knot_to_string(mon);
  u3_umon* mon_u;

  for ( mon_u = unx_u->mon_u;
        mon_u && 0 != strcmp(nam_c, mon_u->nam_c);
        mon_u = mon_u->nex_u )
  {
  }

  if ( !mon_u ) {
    mon_u = c3_malloc(sizeof(u3_umon));
    mon_u->nam_c = nam_c;
    mon_u->syn_o = c3n;
    mon_u->lod_u = NULL;
    mon_u->lod_w = 0;
    mon_u->dir_u.dir = c3y;
    mon_u->dir_u.dry = c3n;
    mon_u->dir_u.pax_c = strdup(unx_u->pax_c);
    mon_u->dir_u.par_u = NULL;
    mon_u->dir_u.nex_u = NULL;
    mon_u->dir_u.kid_u = NULL;
    mon_u->dir_u.wat_u = NULL;
    mon_u->nex_u = unx_u->mon_u;
    unx_u->mon_u = mon_u;
  }
  else {
    c3_free(nam_c);
  }

  u3z(mon);

  return mon_u;
}

/* _unix_scan_mount_point(): scan unix for already-existing mount point
*/
static void
_unix_scan_mount_point(u3_unix* unx_u, u3_umon* mon_u)
{
  DIR* rid_u = c3_opendir(mon_u->dir_u.pax_c);
  if ( !rid_u ) {
    u3l_log("error opening pier directory: %s: %s",
            mon_u->dir_u.pax_c, strerror(errno));
    return;
  }

  c3_w len_w = strlen(mon_u->nam_c);

  while ( 1 ) {
    struct dirent  ent_u;
    struct dirent* out_u;
    c3_w err_w;

    if ( 0 != (err_w = u3_readdir_r(rid_u, &ent_u, &out_u)) ) {
      u3l_log("erroring loading pier directory %s: %s",
              mon_u->dir_u.pax_c, strerror(errno));

      u3_assert(0);
    }
    else if ( !out_u ) {
      break;
    }
    else if ( '.' == out_u->d_name[0] ) { // unnecessary, but consistency
      continue;
    }
    else if ( 0 != strncmp(mon_u->nam_c, out_u->d_name, len_w) ) {
      continue;
    }
    else {
      c3_c* pax_c = _unix_down(mon_u->dir_u.pax_c, out_u->d_name);

      struct stat buf_u;

      if ( 0 != stat(pax_c, &buf_u) ) {
        u3l_log("can't stat pier directory %s: %s",
                mon_u->dir_u.pax_c, strerror(errno));
        c3_free(pax_c);
        continue;
      }
      if ( S_ISDIR(buf_u.st_mode) ) {
        if ( out_u->d_name[len_w] != '\0' ) {
          c3_free(pax_c);
          continue;
        }
        else {
          u3_udir* dir_u = c3_malloc(sizeof(u3_udir));
          _unix_watch_dir(dir_u, &mon_u->dir_u, pax_c);
        }
      }
      else {
        if (  '.'  != out_u->d_name[len_w]
           || '\0' == out_u->d_name[len_w + 1]
           || '~'  == out_u->d_name[strlen(out_u->d_name) - 1]
           || !_unix_sane_ta(unx_u, _unix_string_to_knot(out_u->d_name)) )
        {
          c3_free(pax_c);
          continue;
        }
        else {
          u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));
          _unix_watch_file(unx_u, fil_u, &mon_u->dir_u, pax_c);
        }
      }

      c3_free(pax_c);
    }
  }
}

static u3_noun _unix_free_node(u3_unix* unx_u, u3_unod* nod_u, c3_t del_t);

/* _unix_watch_close_cb(): free watcher handle after libuv lets go.
*/
static void
_unix_watch_close_cb(uv_handle_t* han_u)
{
  //  frees the containing u3_uwat; the handle is its first member
  //
  c3_free(han_u);
}

/* _unix_watch_disarm(): stop watching a directory.
*/
static void
_unix_watch_disarm(u3_udir* dir_u)
{
  if ( !dir_u->wat_u ) {
    return;
  }

  dir_u->wat_u->dir_u = NULL;
  uv_fs_event_stop(&dir_u->wat_u->eve_u);
  uv_close((uv_handle_t*)&dir_u->wat_u->eve_u, _unix_watch_close_cb);
  dir_u->wat_u = NULL;
}

/* _unix_watch_disarm_deep(): stop watching a directory tree.
*/
static void
_unix_watch_disarm_deep(u3_udir* dir_u)
{
  u3_unod* nod_u;

  _unix_watch_disarm(dir_u);

  for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
    if ( c3y == nod_u->dir ) {
      _unix_watch_disarm_deep((u3_udir*)nod_u);
    }
  }
}

/* _unix_free_file(): free file, unlinking it
*/
static void
_unix_free_file(u3_ufil *fil_u, c3_t del_t)
{
  if ( del_t ) {
    if ( 0 != c3_unlink(fil_u->pax_c) && ENOENT != errno ) {
      u3l_log("error unlinking %s: %s", fil_u->pax_c, strerror(errno));
      u3_assert(0);
    }
  }

  c3_free(fil_u->pax_c);
  c3_free(fil_u);
}

/* _unix_free_dir(): free directory, deleting everything within
*/
static void
_unix_free_dir(u3_udir *dir_u, c3_t del_t)
{
  _unix_watch_disarm(dir_u);

  if (del_t) _unix_rm_r(dir_u->pax_c);

  if ( dir_u->kid_u ) {
    fprintf(stderr, "don't kill me, i've got a family %s\r\n", dir_u->pax_c);
  }
  else {
    // fprintf(stderr, "i'm a lone, lonely loner %s\r\n", dir_u->pax_c);
  }
  c3_free(dir_u->pax_c);
  c3_free(dir_u); // XXX this might be too early, how do we
               //     know we've freed all the children?
               //     i suspect we should do this only if
               //     our kid list is empty
}

/* _unix_free_node(): free node, deleting everything within
**
**  also deletes from parent list if in it
*/
static u3_noun
_unix_free_node(u3_unix* unx_u, u3_unod* nod_u, c3_t del_t)
{
  u3_noun can;
  if ( nod_u->par_u ) {
    u3_unod* don_u = nod_u->par_u->kid_u;

    if ( !don_u ) {
    }
    else if ( nod_u == don_u ) {
      nod_u->par_u->kid_u = nod_u->par_u->kid_u->nex_u;
    }
    else {
      for ( ; don_u->nex_u && nod_u != don_u->nex_u; don_u = don_u->nex_u ) {
      }
      if ( don_u->nex_u ) {
        don_u->nex_u = don_u->nex_u->nex_u;
      }
    }
  }

  if ( c3y == nod_u->dir ) {
    can = u3_nul;
    u3_unod* nud_u = ((u3_udir*) nod_u)->kid_u;
    while ( nud_u ) {
      u3_unod* nex_u = nud_u->nex_u;
      can = u3kb_weld(_unix_free_node(unx_u, nud_u, del_t), can);
      nud_u = nex_u;
    }
    _unix_free_dir((u3_udir *)nod_u, del_t);
  }
  else {
    can = u3nc(u3nc(_unix_string_to_path(unx_u, nod_u->pax_c), u3_nul),
               u3_nul);
    _unix_lod_del(unx_u, (u3_ufil *)nod_u);
    _unix_free_file((u3_ufil *)nod_u, del_t);
  }

  return can;
}

/* _unix_free_mount_point(): free mount point
**
**  this process needs to happen in a very careful order. in
**  particular, we must recurse before we get to the callback, so
**  that libuv does all the child directories before it does us.
**
**  tread carefully
*/
static void
_unix_free_mount_point(u3_unix* unx_u, u3_umon* mon_u, c3_t del_t)
{
  u3_unod* nod_u;
  for ( nod_u = mon_u->dir_u.kid_u; nod_u; ) {
    u3_unod* nex_u = nod_u->nex_u;
    u3z(_unix_free_node(unx_u, nod_u, del_t));
    nod_u = nex_u;
  }

  _unix_watch_disarm(&mon_u->dir_u);
  _unix_free_lod(mon_u);

  c3_free(mon_u->dir_u.pax_c);
  c3_free(mon_u->nam_c);
  c3_free(mon_u);
}

/* _unix_delete_mount_point(): remove mount point from list and free
*/
static void
_unix_delete_mount_point(u3_unix* unx_u, u3_noun mon, c3_t del_t)
{
  if ( c3n == u3ud(mon) ) {
    u3_assert(!"mount point must be an atom");
    u3z(mon);
    return;
  }

  c3_c* nam_c = _unix_knot_to_string(mon);
  u3_umon* mon_u;
  u3_umon* tem_u;

  mon_u = unx_u->mon_u;
  if ( !mon_u ) {
    u3l_log("mount point already gone: %s", nam_c);
    goto _delete_mount_point_out;
  }
  if ( 0 == strcmp(nam_c, mon_u->nam_c) ) {
    unx_u->mon_u = mon_u->nex_u;
    _unix_drop_mugs(unx_u, mon_u);
    _unix_free_mount_point(unx_u, mon_u, del_t);
    goto _delete_mount_point_out;
  }

  for ( ;
        mon_u->nex_u && 0 != strcmp(nam_c, mon_u->nex_u->nam_c);
        mon_u = mon_u->nex_u )
  {
  }

  if ( !mon_u->nex_u ) {
    u3l_log("mount point already gone: %s", nam_c);
    goto _delete_mount_point_out;
  }

  tem_u = mon_u->nex_u;
  mon_u->nex_u = mon_u->nex_u->nex_u;
  _unix_drop_mugs(unx_u, tem_u);
  _unix_free_mount_point(unx_u, tem_u, del_t);

_delete_mount_point_out:
  c3_free(nam_c);
  u3z(mon);
}

/* _unix_commit_mount_point: commit from mount point
*/
static void
_unix_commit_mount_point(u3_unix* unx_u, u3_noun mon)
{
  unx_u->dyr = c3y;
  u3_unix_ef_look(unx_u, mon, c3n);
  return;
}

/* _unix_watch_file(): initialize file
*/
static void
_unix_watch_file(u3_unix* unx_u, u3_ufil* fil_u, u3_udir* par_u, c3_c* pax_c)
{
  // initialize fil_u

  fil_u->dir = c3n;
  fil_u->dry = c3n;
  fil_u->pax_c = c3_malloc(1 + strlen(pax_c));
  strcpy(fil_u->pax_c, pax_c);
  fil_u->par_u = par_u;
  fil_u->nex_u = NULL;
  fil_u->gum_w = 0;
  fil_u->dum_d = 0;

  if ( par_u ) {
    fil_u->nex_u = par_u->kid_u;
    par_u->kid_u = (u3_unod*) fil_u;
    _unix_seed_mug(unx_u, fil_u);
  }
}

/* _unix_watch_dir(): initialize directory
*/
static void
_unix_watch_dir(u3_udir* dir_u, u3_udir* par_u, c3_c* pax_c)
{
  // initialize dir_u

  dir_u->dir = c3y;
  dir_u->dry = c3n;
  dir_u->pax_c = c3_malloc(1 + strlen(pax_c));
  strcpy(dir_u->pax_c, pax_c);
  dir_u->par_u = par_u;
  dir_u->nex_u = NULL;
  dir_u->kid_u = NULL;
  dir_u->wat_u = NULL;

  if ( par_u ) {
    dir_u->nex_u = par_u->kid_u;
    par_u->kid_u = (u3_unod*) dir_u;
  }
}

/* _unix_create_dir(): create unix directory and watch it
*/
static void
_unix_create_dir(u3_udir* dir_u, u3_udir* par_u, u3_noun nam)
{
  c3_c* nam_c = _unix_knot_to_string(nam);
  c3_w  nam_w = strlen(nam_c);
  c3_w  pax_w = strlen(par_u->pax_c);
  c3_c* pax_c = c3_malloc(pax_w + 1 + nam_w + 1);

  strcpy(pax_c, par_u->pax_c);
  pax_c[pax_w] = '/';
  strcpy(pax_c + pax_w + 1, nam_c);
  pax_c[pax_w + 1 + nam_w] = '\0';

  c3_free(nam_c);
  u3z(nam);

  _unix_mkdir(pax_c);
  _unix_watch_dir(dir_u, par_u, pax_c);

  c3_free(pax_c);
}

/* _unix_mark_wet(): mark a directory tree as modified, so that a
**                   scan re-examines all of it.
*/
static void
_unix_mark_wet(u3_udir* dir_u)
{
  u3_unod* nod_u;

  dir_u->dry = c3n;

  for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
    if ( c3y == nod_u->dir ) {
      _unix_mark_wet((u3_udir*)nod_u);
    }
    else {
      nod_u->dry = c3n;
    }
  }
}

/* _unix_node_mount(): find the mount point owning a node.
*/
static u3_umon*
_unix_node_mount(u3_unix* unx_u, u3_unod* nod_u)
{
  u3_umon* mon_u;

  while ( nod_u->par_u ) {
    nod_u = (u3_unod*)nod_u->par_u;
  }

  for ( mon_u = unx_u->mon_u; mon_u; mon_u = mon_u->nex_u ) {
    if ( (u3_unod*)&mon_u->dir_u == nod_u ) {
      return mon_u;
    }
  }

  return NULL;
}

static void
_unix_time_cb(uv_timer_t* tim_u);

/* _unix_doom_recheck(): if a scan deferred any deletions, schedule
**                       a follow-up scan to confirm them.
*/
static void
_unix_doom_recheck(u3_unix* unx_u)
{
  if ( c3y == unx_u->dum_o ) {
    unx_u->dum_o = c3n;

    if ( !uv_is_active((uv_handle_t*)unx_u->syt_u) ) {
      unx_u->fir_d = uv_now(u3L);
      uv_timer_start(unx_u->syt_u, _unix_time_cb, SYNC_RECHECK_MS, 0);
    }
  }
}

/* _unix_time_cb(): auto-sync debounce timer fired.
**
**  scans every mount: on watched mounts, wet subtrees are pending
**  fs events; on unwatched mounts nothing is ever wet between
**  commits except files awaiting a deletion-grace recheck, so the
**  scan is surgical either way.
*/
static void
_unix_time_cb(uv_timer_t* tim_u)
{
  u3_unix* unx_u = tim_u->data;
  u3_umon* mon_u;

  for ( mon_u = unx_u->mon_u; mon_u; mon_u = mon_u->nex_u ) {
    _unix_update_mount(unx_u, mon_u, c3n);
  }

  _unix_doom_recheck(unx_u);
}

/* _unix_sweep_cb(): periodic backstop sweep.
**
**  fs events can be dropped (inotify queue overflow, exhausted
**  watch descriptors, platform edge cases), so periodically treat
**  every auto-synced mount as if an event had fired for its whole
**  tree.  the scan re-reads wet files but only syncs those whose
**  contents changed, so a quiescent tree produces no event.
*/
static void
_unix_sweep_cb(uv_timer_t* tim_u)
{
  u3_unix* unx_u = tim_u->data;
  u3_umon* mon_u;
  c3_o     wet_o = c3n;

  for ( mon_u = unx_u->mon_u; mon_u; mon_u = mon_u->nex_u ) {
    if ( c3y == mon_u->syn_o ) {
      _unix_mark_wet(&mon_u->dir_u);
      wet_o = c3y;
    }
  }

  //  schedule the scan through the debounce path, so a sweep
  //  landing mid-save coalesces with the save's own events
  //
  if ( (c3y == wet_o) && !uv_is_active((uv_handle_t*)unx_u->syt_u) ) {
    unx_u->fir_d = uv_now(u3L);
    uv_timer_start(unx_u->syt_u, _unix_time_cb, SYNC_QUIET_MS, 0);
  }
}

/* _unix_event_cb(): fs event on a watched directory.
*/
static void
_unix_event_cb(uv_fs_event_t* eve_u,
               const c3_c*    fil_c,
               c3_i           sev_i,
               c3_i           sas_i)
{
  u3_uwat* wat_u = (u3_uwat*)eve_u;
  u3_udir* dir_u = wat_u->dir_u;
  u3_unix* unx_u = wat_u->unx_u;

  if ( !dir_u ) {  //  watcher is pending close
    return;
  }

  //  ignore hidden and backup files
  //
  if ( fil_c ) {
    c3_w len_w = strlen(fil_c);

    if ( !len_w || ('.' == fil_c[0]) || ('~' == fil_c[len_w - 1]) ) {
      return;
    }
  }

  {
    c3_t fon_t = 0;

    if ( fil_c ) {
      c3_w     dir_w = strlen(dir_u->pax_c);
      u3_unod* nod_u;

      for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
        if ( 0 == strcmp(nod_u->pax_c + dir_w + 1, fil_c) ) {
          if ( c3y == nod_u->dir ) {
            _unix_mark_wet((u3_udir*)nod_u);
          }
          else {
            nod_u->dry = c3n;
          }
          fon_t = 1;
          break;
        }
      }
    }
    else {
      //  no filename from the platform: re-examine the whole subtree
      //
      _unix_mark_wet(dir_u);
    }

    //  the mount root is the pier directory; ignore entries there
    //  that we aren't already tracking
    //
    if ( !fon_t && !dir_u->par_u && fil_c ) {
      return;
    }
  }

  //  wet the directory and its ancestors so a scan descends to it
  //
  {
    u3_udir* par_u = dir_u;

    while ( par_u ) {
      par_u->dry = c3n;
      par_u = par_u->par_u;
    }
  }

  //  debounce until quiescence, capped so a busy writer can't
  //  starve sync (see SYNC_QUIET_MS/SYNC_CAP_MS)
  //
  {
    c3_d now_d = uv_now(u3L);

    if ( !uv_is_active((uv_handle_t*)unx_u->syt_u) ) {
      unx_u->fir_d = now_d;
      uv_timer_start(unx_u->syt_u, _unix_time_cb, SYNC_QUIET_MS, 0);
    }
    else if ( (now_d - unx_u->fir_d) < SYNC_CAP_MS ) {
      uv_timer_start(unx_u->syt_u, _unix_time_cb, SYNC_QUIET_MS, 0);
    }
    //  else: cap reached, let the pending scan fire
  }
}

/* _unix_watch_arm(): watch a directory for fs events.
*/
static void
_unix_watch_arm(u3_unix* unx_u, u3_udir* dir_u)
{
  u3_uwat* wat_u;

  if ( dir_u->wat_u ) {
    return;
  }

  wat_u = c3_malloc(sizeof(u3_uwat));
  wat_u->unx_u = unx_u;
  wat_u->dir_u = dir_u;

  if ( 0 != uv_fs_event_init(u3L, &wat_u->eve_u) ) {
    u3l_log("unix: can't init watcher for %s", dir_u->pax_c);
    c3_free(wat_u);
    return;
  }

  if ( 0 != uv_fs_event_start(&wat_u->eve_u, _unix_event_cb,
                              dir_u->pax_c, 0) )
  {
    u3l_log("unix: can't watch %s", dir_u->pax_c);
    wat_u->dir_u = NULL;
    uv_close((uv_handle_t*)&wat_u->eve_u, _unix_watch_close_cb);
    return;
  }

  dir_u->wat_u = wat_u;
}

/* _unix_watch_arm_deep(): watch a directory tree for fs events.
*/
static void
_unix_watch_arm_deep(u3_unix* unx_u, u3_udir* dir_u)
{
  u3_unod* nod_u;

  _unix_watch_arm(unx_u, dir_u);

  for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
    if ( c3y == nod_u->dir ) {
      _unix_watch_arm_deep(unx_u, (u3_udir*)nod_u);
    }
  }
}

/* _unix_maybe_watch(): watch a new directory if its mount is auto-synced.
*/
static void
_unix_maybe_watch(u3_unix* unx_u, u3_udir* dir_u)
{
  u3_umon* mon_u = _unix_node_mount(unx_u, (u3_unod*)dir_u);

  if ( mon_u && (c3y == mon_u->syn_o) ) {
    _unix_watch_arm(unx_u, dir_u);
  }
}

/* _unix_find_node(): find a node by absolute path, under a directory.
*/
static u3_unod*
_unix_find_node(u3_udir* dir_u, const c3_c* pax_c)
{
  u3_unod* nod_u;

  for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
    c3_w len_w = strlen(nod_u->pax_c);

    if ( 0 == strncmp(nod_u->pax_c, pax_c, len_w) ) {
      if ( '\0' == pax_c[len_w] ) {
        return nod_u;
      }
      if ( ('/' == pax_c[len_w]) && (c3y == nod_u->dir) ) {
        return _unix_find_node((u3_udir*)nod_u, pax_c);
      }
    }
  }

  return NULL;
}

/* _unix_pend(): record a file included in a pending %into event.
*/
static void
_unix_pend(u3_unix* unx_u, const c3_c* pax_c, c3_w mug_w)
{
  u3_usyc* syc_u = unx_u->pen_u;

  if ( !syc_u ) {
    return;
  }

  if ( syc_u->len_w == syc_u->siz_w ) {
    syc_u->siz_w = syc_u->siz_w ? (2 * syc_u->siz_w) : 64;
    syc_u->ent_u = c3_realloc(syc_u->ent_u,
                              syc_u->siz_w * sizeof(*syc_u->ent_u));
  }

  syc_u->ent_u[syc_u->len_w].pax_c = strdup(pax_c);
  syc_u->ent_u[syc_u->len_w].mug_w = mug_w;
  syc_u->len_w++;
}

/* _unix_sync_free(): free a pending sync record.
*/
static void
_unix_sync_free(u3_usyc* syc_u)
{
  c3_w i_w;

  for ( i_w = 0; i_w < syc_u->len_w; i_w++ ) {
    c3_free(syc_u->ent_u[i_w].pax_c);
  }
  c3_free(syc_u->nam_c);
  c3_free(syc_u->ent_u);
  c3_free(syc_u);
}

/* _unix_sync_done(): a %into event committed; record synced mugs
**                    so the files aren't re-sent, and persist them.
*/
static void
_unix_sync_done(u3_usyc* syc_u)
{
  u3_unix* unx_u = syc_u->unx_u;
  u3_umon* mon_u;
  c3_w     i_w;

  //  the mount may have been deleted while the event was in flight
  //
  for ( mon_u = unx_u->mon_u;
        mon_u && (0 != strcmp(mon_u->nam_c, syc_u->nam_c));
        mon_u = mon_u->nex_u )
  { }

  if ( !mon_u ) {
    return;
  }

  for ( i_w = 0; i_w < syc_u->len_w; i_w++ ) {
    u3_unod* nod_u = _unix_find_node(&mon_u->dir_u,
                                     syc_u->ent_u[i_w].pax_c);
    if ( nod_u && (c3n == nod_u->dir) ) {
      ((u3_ufil*)nod_u)->gum_w = syc_u->ent_u[i_w].mug_w;
    }
  }

  _unix_save_mugs(unx_u, mon_u);
}

/* _unix_sync_news(): notification of %into event status.
*/
static void
_unix_sync_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  if ( u3_ovum_done == new_e ) {
    if ( egg_u->ptr_v ) {
      _unix_sync_done(egg_u->ptr_v);
      _unix_sync_free(egg_u->ptr_v);
      egg_u->ptr_v = NULL;
    }
  }
  else if ( u3_ovum_drop == new_e ) {
    if ( egg_u->ptr_v ) {
      _unix_sync_free(egg_u->ptr_v);
      egg_u->ptr_v = NULL;
    }
  }
}

/* _unix_sync_bail(): a %into event failed; changes will be retried
**                    by a later scan, since mugs were not updated.
*/
static void
_unix_sync_bail(u3_ovum* egg_u, u3_noun lud)
{
  if ( egg_u->ptr_v ) {
    _unix_sync_free(egg_u->ptr_v);
    egg_u->ptr_v = NULL;
  }

  u3_auto_bail_slog(egg_u, lud);
  u3_ovum_free(egg_u);
}

/* _unix_mug_pax(): path of the persisted mug cache for a mount point.
**                  caller frees.
*/
static c3_c*
_unix_mug_pax(u3_unix* unx_u, u3_umon* mon_u, const c3_c* suf_c)
{
  c3_w  len_w = strlen(unx_u->pax_c) + strlen(mon_u->nam_c)
              + strlen(suf_c) + 32;
  c3_c* pax_c = c3_malloc(len_w);

  snprintf(pax_c, len_w, "%s/.urb/syn", unx_u->pax_c);
  c3_mkdir(pax_c, 0700);
  snprintf(pax_c, len_w, "%s/.urb/syn/%s.mug%s",
           unx_u->pax_c, mon_u->nam_c, suf_c);

  return pax_c;
}

#define SYNC_MUG_HEAD "vere-sync-mug-v1"

/* _unix_grab_mugs_dir(): collect a directory tree's mugs, unsorted.
*/
static void
_unix_grab_mugs_dir(u3_udir* dir_u, u3_umug** ent_u, c3_w* len_w, c3_w* siz_w)
{
  u3_unod* nod_u;

  for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
    if ( c3y == nod_u->dir ) {
      _unix_grab_mugs_dir((u3_udir*)nod_u, ent_u, len_w, siz_w);
    }
    else {
      u3_ufil* fic_u = (u3_ufil*)nod_u;

      if ( fic_u->gum_w ) {
        if ( *len_w == *siz_w ) {
          *siz_w = *siz_w ? (2 * *siz_w) : 256;
          *ent_u = c3_realloc(*ent_u, *siz_w * sizeof(**ent_u));
        }
        (*ent_u)[*len_w].pax_c = strdup(fic_u->pax_c);
        (*ent_u)[*len_w].mug_w = fic_u->gum_w;
        (*len_w)++;
      }
    }
  }
}

/* _unix_fold_mugs(): merge the node tree's mugs into the cache.
**
**  the tree is built lazily, so it may cover only part of the mount;
**  tree state takes precedence over cache entries for the same path.
*/
static void
_unix_fold_mugs(u3_umon* mon_u)
{
  u3_umug* tre_u = NULL;
  c3_w     tre_w = 0, sat_w = 0;

  _unix_grab_mugs_dir(&mon_u->dir_u, &tre_u, &tre_w, &sat_w);

  if ( !tre_w ) {
    return;
  }

  qsort(tre_u, tre_w, sizeof(u3_umug), _unix_lod_cmp);

  {
    u3_umug* new_u = c3_malloc((tre_w + mon_u->lod_w) * sizeof(u3_umug));
    c3_w     new_w = 0, t_w = 0, l_w = 0;

    while ( (t_w < tre_w) || (l_w < mon_u->lod_w) ) {
      c3_i cmp_i = (t_w == tre_w)        ?  1
                 : (l_w == mon_u->lod_w) ? -1
                 : strcmp(tre_u[t_w].pax_c, mon_u->lod_u[l_w].pax_c);

      if ( cmp_i < 0 ) {
        new_u[new_w++] = tre_u[t_w++];
      }
      else if ( cmp_i > 0 ) {
        new_u[new_w++] = mon_u->lod_u[l_w++];
      }
      else {
        new_u[new_w++] = tre_u[t_w++];
        c3_free(mon_u->lod_u[l_w].pax_c);
        l_w++;
      }
    }

    c3_free(tre_u);
    c3_free(mon_u->lod_u);
    mon_u->lod_u = new_u;
    mon_u->lod_w = new_w;
  }
}

/* _unix_save_mugs(): persist a mount point's synced mugs, so that
**                    a post-restart scan only sends real changes.
*/
static void
_unix_save_mugs(u3_unix* unx_u, u3_umon* mon_u)
{
  c3_c* pax_c = _unix_mug_pax(unx_u, mon_u, "");
  c3_c* tmp_c = _unix_mug_pax(unx_u, mon_u, ".tmp");
  FILE* fil_u = fopen(tmp_c, "w");
  c3_w  bas_w = strlen(unx_u->pax_c);
  c3_w  i_w;

  if ( !fil_u ) {
    u3l_log("unix: can't write mug cache %s: %s", tmp_c, strerror(errno));
    c3_free(pax_c);
    c3_free(tmp_c);
    return;
  }

  _unix_fold_mugs(mon_u);

  fprintf(fil_u, "%s\n", SYNC_MUG_HEAD);

  for ( i_w = 0; i_w < mon_u->lod_w; i_w++ ) {
    fprintf(fil_u, "%08x %s\n",
            mon_u->lod_u[i_w].mug_w,
            mon_u->lod_u[i_w].pax_c + bas_w + 1);
  }

  //  atomically replace the previous cache.  mingw's rename() fails
  //  if the destination exists, so use MoveFileEx on windows.
  //
#ifdef U3_OS_windows
  if ( fclose(fil_u)
    || !MoveFileExA(tmp_c, pax_c, MOVEFILE_REPLACE_EXISTING) )
#else
  if ( fclose(fil_u) || (0 != rename(tmp_c, pax_c)) )
#endif
  {
    u3l_log("unix: can't save mug cache %s: %s", pax_c, strerror(errno));
    c3_unlink(tmp_c);
  }

  c3_free(pax_c);
  c3_free(tmp_c);
}

/* _unix_drop_mugs(): delete a mount point's persisted mug cache.
*/
static void
_unix_drop_mugs(u3_unix* unx_u, u3_umon* mon_u)
{
  c3_c* pax_c = _unix_mug_pax(unx_u, mon_u, "");

  c3_unlink(pax_c);
  c3_free(pax_c);
}

/* _unix_lod_cmp(): mug cache comparator, by path.
*/
static c3_i
_unix_lod_cmp(const void* lef_v, const void* rit_v)
{
  return strcmp(((const u3_umug*)lef_v)->pax_c,
                ((const u3_umug*)rit_v)->pax_c);
}

/* _unix_free_lod(): free a mount point's loaded mug cache.
*/
static void
_unix_free_lod(u3_umon* mon_u)
{
  c3_w i_w;

  for ( i_w = 0; i_w < mon_u->lod_w; i_w++ ) {
    c3_free(mon_u->lod_u[i_w].pax_c);
  }
  c3_free(mon_u->lod_u);
  mon_u->lod_u = NULL;
  mon_u->lod_w = 0;
}

/* _unix_load_mugs(): load the persisted mug cache for a mount point.
**
**  the node tree is built lazily by scans, so entries can't be
**  applied here; they're kept on the mount point and consulted as
**  file nodes are created (_unix_watch_file).
*/
static void
_unix_load_mugs(u3_unix* unx_u, u3_umon* mon_u)
{
  c3_c* pax_c = _unix_mug_pax(unx_u, mon_u, "");
  FILE* fil_u = fopen(pax_c, "r");
  c3_c  lin_c[8192];
  c3_w  bas_w = strlen(unx_u->pax_c);
  c3_w  siz_w = 0;

  _unix_free_lod(mon_u);

  if ( !fil_u ) {
    c3_free(pax_c);
    return;
  }

  if (  !fgets(lin_c, sizeof(lin_c), fil_u)
     || 0 != strncmp(lin_c, SYNC_MUG_HEAD, strlen(SYNC_MUG_HEAD)) )
  {
    u3l_log("unix: discarding unrecognized mug cache %s", pax_c);
    fclose(fil_u);
    c3_free(pax_c);
    return;
  }

  while ( fgets(lin_c, sizeof(lin_c), fil_u) ) {
    c3_w  len_w = strlen(lin_c);
    c3_w  mug_w;
    c3_c* rel_c;

    if ( len_w && ('\n' == lin_c[len_w - 1]) ) {
      lin_c[--len_w] = '\0';
    }

    //  "%08x <path>": a malformed line discards the whole cache,
    //  which safely degrades to a full reconciliation
    //
    if ( (len_w < 10) || (' ' != lin_c[8])
       || (1 != sscanf(lin_c, "%8x", &mug_w)) )
    {
      u3l_log("unix: discarding malformed mug cache %s", pax_c);
      _unix_free_lod(mon_u);
      break;
    }

    rel_c = lin_c + 9;

    if ( mon_u->lod_w == siz_w ) {
      siz_w = siz_w ? (2 * siz_w) : 256;
      mon_u->lod_u = c3_realloc(mon_u->lod_u, siz_w * sizeof(u3_umug));
    }

    {
      c3_w  abs_w = bas_w + 1 + strlen(rel_c) + 1;
      c3_c* abs_c = c3_malloc(abs_w);

      snprintf(abs_c, abs_w, "%s/%s", unx_u->pax_c, rel_c);
      mon_u->lod_u[mon_u->lod_w].pax_c = abs_c;
      mon_u->lod_u[mon_u->lod_w].mug_w = mug_w;
      mon_u->lod_w++;
    }
  }

  if ( mon_u->lod_w ) {
    qsort(mon_u->lod_u, mon_u->lod_w, sizeof(u3_umug), _unix_lod_cmp);
  }

  fclose(fil_u);
  c3_free(pax_c);
}

/* _unix_seed_mug(): seed a new file node's mug from the loaded cache.
*/
static void
_unix_seed_mug(u3_unix* unx_u, u3_ufil* fil_u)
{
  u3_umon* mon_u = _unix_node_mount(unx_u, (u3_unod*)fil_u);
  u3_umug  key_u;
  u3_umug* fon_u;

  if ( !mon_u || !mon_u->lod_w ) {
    return;
  }

  key_u.pax_c = fil_u->pax_c;
  fon_u = bsearch(&key_u, mon_u->lod_u, mon_u->lod_w,
                  sizeof(u3_umug), _unix_lod_cmp);

  if ( fon_u ) {
    fil_u->gum_w = fon_u->mug_w;
  }
}

/* _unix_lod_del(): remove a deleted file from its mount's mug cache.
*/
static void
_unix_lod_del(u3_unix* unx_u, u3_ufil* fil_u)
{
  u3_umon* mon_u = _unix_node_mount(unx_u, (u3_unod*)fil_u);
  u3_umug  key_u;
  u3_umug* fon_u;

  if ( !mon_u || !mon_u->lod_w ) {
    return;
  }

  key_u.pax_c = fil_u->pax_c;
  fon_u = bsearch(&key_u, mon_u->lod_u, mon_u->lod_w,
                  sizeof(u3_umug), _unix_lod_cmp);

  if ( fon_u ) {
    c3_w i_w = fon_u - mon_u->lod_u;

    c3_free(fon_u->pax_c);
    memmove(fon_u, fon_u + 1,
            (mon_u->lod_w - i_w - 1) * sizeof(u3_umug));
    mon_u->lod_w--;
  }
}

static u3_noun _unix_update_node(u3_unix* unx_u, u3_unod* nod_u);

/* _unix_doom_hold(): a file is missing; hold its deletion until it
**                     has stayed missing through the grace period.
**
**  editors often delete a file moments before rewriting it, so a
**  deletion only syncs once it survives a recheck.
*/
static c3_t
_unix_doom_hold(u3_ufil* fil_u)
{
  c3_d now_d = uv_now(u3L);

  if ( !fil_u->dum_d ) {
    fil_u->dum_d = now_d;
    return 1;
  }

  return (now_d - fil_u->dum_d) < SYNC_GRACE_MS;
}

/* _unix_update_file(): update file, producing list of changes
**
**  when scanning through files, if dry, do nothing. otherwise,
**  mark as dry, then check if file exists. if not, remove
**  self from node list and add path plus sig to %into event.
**  otherwise, read the file and get a mug checksum. if same as
**  gum_w, move on. otherwise, overwrite add path plus data to
**  %into event.
*/
static u3_noun
_unix_update_file(u3_unix* unx_u, u3_ufil* fil_u)
{
  u3_assert( c3n == fil_u->dir );

  if ( c3y == fil_u->dry ) {
    return u3_nul;
  }

  fil_u->dry = c3y;

  struct stat buf_u;
  c3_i  fid_i = c3_open(fil_u->pax_c, O_RDONLY, 0644);
  c3_ws len_ws, red_ws;
  c3_y* dat_y;

  if ( fid_i < 0 || fstat(fid_i, &buf_u) < 0 ) {
    if ( ENOENT == errno ) {
      if ( _unix_doom_hold(fil_u) ) {
        u3_udir* par_u;

        //  stay wet, and keep ancestors wet, so the recheck
        //  scan descends back to this node
        //
        fil_u->dry = c3n;
        for ( par_u = fil_u->par_u; par_u; par_u = par_u->par_u ) {
          par_u->dry = c3n;
        }

        unx_u->dum_o = c3y;
        return u3_nul;
      }

      fil_u->dum_d = 0;
      return u3nc(u3nc(_unix_string_to_path(unx_u, fil_u->pax_c), u3_nul), u3_nul);
    }
    else {
      u3l_log("error opening file %s: %s",
              fil_u->pax_c, strerror(errno));
      return u3_nul;
    }
  }

  fil_u->dum_d = 0;

  len_ws = buf_u.st_size;
  dat_y = c3_malloc(len_ws);

  red_ws = read(fid_i, dat_y, len_ws);

  if ( close(fid_i) < 0 ) {
    u3l_log("error closing file %s: %s",
            fil_u->pax_c, strerror(errno));
  }

  if ( len_ws != red_ws ) {
    if ( red_ws < 0 ) {
      u3l_log("error reading file %s: %s",
              fil_u->pax_c, strerror(errno));
    }
    else {
      u3l_log("wrong # of bytes read in file %s: %d %d",
              fil_u->pax_c, len_ws, red_ws);
    }
    c3_free(dat_y);
    return u3_nul;
  }
  else {
    c3_w mug_w = u3r_mug_bytes(dat_y, len_ws);
    if ( mug_w == fil_u->gum_w ) {
      c3_free(dat_y);
      return u3_nul;
    }
    else {
      u3_noun pax = _unix_string_to_path(unx_u, fil_u->pax_c);
      u3_noun mim = u3nt(c3__text, u3i_string("plain"), u3_nul);
      u3_noun dat = u3nt(mim, len_ws, u3i_bytes(len_ws, dat_y));

      _unix_pend(unx_u, fil_u->pax_c, mug_w);

      c3_free(dat_y);
      return u3nc(u3nt(pax, u3_nul, dat), u3_nul);
    }
  }
}

/* _unix_update_dir(): update directory, producing list of changes
**
**  when changing this, consider whether to also change
**  _unix_initial_update_dir()
*/
static u3_noun
_unix_update_dir(u3_unix* unx_u, u3_udir* dir_u)
{
  u3_noun can = u3_nul;

  u3_assert( c3y == dir_u->dir );

  if ( c3y == dir_u->dry ) {
    return u3_nul;
  }

  dir_u->dry = c3y;

  // Check that old nodes are still there

  u3_unod* nod_u = dir_u->kid_u;

  if ( nod_u ) {
    while ( nod_u ) {
      if ( c3y == nod_u->dry ) {
        nod_u = nod_u->nex_u;
      }
      else {
        if ( c3y == nod_u->dir ) {
          DIR* red_u = c3_opendir(nod_u->pax_c);
          if ( 0 == red_u ) {
            u3_unod* nex_u = nod_u->nex_u;
            can = u3kb_weld(_unix_free_node(unx_u, nod_u, true), can);
            nod_u = nex_u;
          }
          else {
            closedir(red_u);
            nod_u = nod_u->nex_u;
          }
        }
        else {
          struct stat buf_u;
          c3_i  fid_i = c3_open(nod_u->pax_c, O_RDONLY, 0644);

          if ( (fid_i < 0) || (fstat(fid_i, &buf_u) < 0) ) {
            if ( ENOENT != errno ) {
              u3l_log("_unix_update_dir: error opening file %s: %s",
                      nod_u->pax_c, strerror(errno));
            }
            else if ( _unix_doom_hold((u3_ufil*)nod_u) ) {
              unx_u->dum_o = c3y;
              nod_u = nod_u->nex_u;
              continue;
            }

            u3_unod* nex_u = nod_u->nex_u;
            can = u3kb_weld(_unix_free_node(unx_u, nod_u, true), can);
            nod_u = nex_u;
          }
          else {
            if ( close(fid_i) < 0 ) {
              u3l_log("_unix_update_dir: error closing file %s: %s",
                      nod_u->pax_c, strerror(errno));
            }

            nod_u = nod_u->nex_u;
          }
        }
      }
    }
  }

  // Check for new nodes

  DIR* rid_u = c3_opendir(dir_u->pax_c);
  if ( !rid_u ) {
    u3l_log("error opening directory %s: %s",
            dir_u->pax_c, strerror(errno));
    u3_assert(0);
  }

  while ( 1 ) {
    struct dirent  ent_u;
    struct dirent* out_u;
    c3_w err_w;


    if ( (err_w = u3_readdir_r(rid_u, &ent_u, &out_u)) != 0 ) {
      u3l_log("error loading directory %s: %s",
              dir_u->pax_c, strerror(err_w));
      u3_assert(0);
    }
    else if ( !out_u ) {
      break;
    }
    else if ( '.' == out_u->d_name[0] ) {
      continue;
    }
    else {
      c3_c* pax_c = _unix_down(dir_u->pax_c, out_u->d_name);

      struct stat buf_u;

      if ( 0 != stat(pax_c, &buf_u) ) {
        u3l_log("can't stat %s: %s", pax_c, strerror(errno));
        c3_free(pax_c);
        continue;
      }
      else {
        u3_unod* nod_u;
        for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
          if ( 0 == strcmp(pax_c, nod_u->pax_c) ) {
            if ( S_ISDIR(buf_u.st_mode) ) {
              if ( c3n == nod_u->dir ) {
                u3l_log("not a directory: %s", nod_u->pax_c);
                u3_assert(0);
              }
            }
            else {
              if ( c3y == nod_u->dir ) {
                u3l_log("not a file: %s", nod_u->pax_c);
                u3_assert(0);
              }
            }
            break;
          }
        }

        if ( !nod_u ) {
          if ( !S_ISDIR(buf_u.st_mode) ) {
            if (  !strchr(out_u->d_name,'.')
               || '~' == out_u->d_name[strlen(out_u->d_name) - 1]
               || !_unix_sane_ta(unx_u, _unix_string_to_knot(out_u->d_name)) )
            {
              c3_free(pax_c);
              continue;
            }

            u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));
            _unix_watch_file(unx_u, fil_u, dir_u, pax_c);
          }
          else {
            u3_udir* dis_u = c3_malloc(sizeof(u3_udir));
            _unix_watch_dir(dis_u, dir_u, pax_c);
            _unix_maybe_watch(unx_u, dis_u);
            can = u3kb_weld(_unix_update_dir(unx_u, dis_u), can); // XXX unnecessary?
          }
        }
      }

      c3_free(pax_c);
    }
  }

  if ( closedir(rid_u) < 0 ) {
    u3l_log("error closing directory %s: %s",
            dir_u->pax_c, strerror(errno));
  }

  if ( !dir_u->kid_u ) {
    return u3kb_weld(_unix_free_node(unx_u, (u3_unod*) dir_u, true), can);
  }

  // get change list

  for ( nod_u = dir_u->kid_u; nod_u; nod_u = nod_u->nex_u ) {
    can = u3kb_weld(_unix_update_node(unx_u, nod_u), can);
  }

  return can;
}

/* _unix_update_node(): update node, producing list of changes
*/
static u3_noun
_unix_update_node(u3_unix* unx_u, u3_unod* nod_u)
{
  if ( c3y == nod_u->dir ) {
    return _unix_update_dir(unx_u, (void*)nod_u);
  }
  else {
    return _unix_update_file(unx_u, (void*)nod_u);
  }
}

/* _unix_update_mount(): update mount point
*/
static void
_unix_update_mount(u3_unix* unx_u, u3_umon* mon_u, u3_noun all)
{
  if ( c3n == mon_u->dir_u.dry ) {
    u3_noun  can = u3_nul;
    u3_unod* nod_u;
    u3_usyc* syc_u;

    //  accumulate (path, mug) for files included in the event,
    //  to be recorded if and when the event commits
    //
    u3_assert( !unx_u->pen_u );
    syc_u = c3_calloc(sizeof(*syc_u));
    syc_u->unx_u = unx_u;
    syc_u->nam_c = strdup(mon_u->nam_c);
    unx_u->pen_u = syc_u;

    for ( nod_u = mon_u->dir_u.kid_u; nod_u; nod_u = nod_u->nex_u ) {
      can = u3kb_weld(_unix_update_node(unx_u, nod_u), can);
    }

    unx_u->pen_u = NULL;

    //  if nothing changed, don't inject an empty event
    //
    if ( (u3_nul == can) && (c3n == all) ) {
      _unix_sync_free(syc_u);
      u3z(all);
      return;
    }

    {
      //  XX remove u3A->sen
      //
      u3_noun wir = u3nt(c3__sync,
                        u3dc("scot", c3__uv, unx_u->sev_l),
                        u3_nul);
      u3_noun cad = u3nq(c3__into, _unix_string_to_knot(mon_u->nam_c), all,
                         can);

      u3_auto_peer(
        u3_auto_plan(&unx_u->car_u, u3_ovum_init(0, c3__c, wir, cad)),
        syc_u, _unix_sync_news, _unix_sync_bail);
    }
  }
}

/* _unix_initial_update_file(): read file, but don't watch
**  XX deduplicate with _unix_update_file()
*/
static u3_noun
_unix_initial_update_file(c3_c* pax_c, c3_c* bas_c)
{
  struct stat buf_u;
  c3_i  fid_i = c3_open(pax_c, O_RDONLY, 0644);
  c3_ws len_ws, red_ws;
  c3_y* dat_y;

  if ( fid_i < 0 || fstat(fid_i, &buf_u) < 0 ) {
    if ( ENOENT == errno ) {
      return u3_nul;
    }
    else {
      u3l_log("error opening initial file %s: %s",
              pax_c, strerror(errno));
      return u3_nul;
    }
  }

  len_ws = buf_u.st_size;
  dat_y = c3_malloc(len_ws);

  red_ws = read(fid_i, dat_y, len_ws);

  if ( close(fid_i) < 0 ) {
    u3l_log("error closing initial file %s: %s",
            pax_c, strerror(errno));
  }

  if ( len_ws != red_ws ) {
    if ( red_ws < 0 ) {
      u3l_log("error reading initial file %s: %s",
              pax_c, strerror(errno));
    }
    else {
      u3l_log("wrong # of bytes read in initial file %s: %d %d",
              pax_c, len_ws, red_ws);
    }
    c3_free(dat_y);
    return u3_nul;
  }
  else {
    u3_noun pax = _unix_string_to_path_helper(pax_c
                   + strlen(bas_c)
                   + 1); /* XX slightly less VERY BAD than before*/
    u3_noun mim = u3nt(c3__text, u3i_string("plain"), u3_nul);
    u3_noun dat = u3nt(mim, len_ws, u3i_bytes(len_ws, dat_y));

    c3_free(dat_y);
    return u3nc(u3nt(pax, u3_nul, dat), u3_nul);
  }
}

/* _unix_initial_update_dir(): read directory, but don't watch
**  XX deduplicate with _unix_update_dir()
*/
static u3_noun
_unix_initial_update_dir(c3_c* pax_c, c3_c* bas_c)
{
  u3_noun can = u3_nul;

  DIR* rid_u = c3_opendir(pax_c);
  if ( !rid_u ) {
    u3l_log("error opening initial directory: %s: %s",
            pax_c, strerror(errno));
    return u3_nul;
  }

  while ( 1 ) {
    struct dirent  ent_u;
    struct dirent* out_u;
    c3_w err_w;

    if ( 0 != (err_w = u3_readdir_r(rid_u, &ent_u, &out_u)) ) {
      u3l_log("error loading initial directory %s: %s",
              pax_c, strerror(errno));

      u3_assert(0);
    }
    else if ( !out_u ) {
      break;
    }
    else if ( '.' == out_u->d_name[0] ) {
      continue;
    }
    else {
      c3_c* pox_c = _unix_down(pax_c, out_u->d_name);

      struct stat buf_u;

      if ( 0 != stat(pox_c, &buf_u) ) {
        u3l_log("initial can't stat %s: %s",
                pox_c, strerror(errno));
        c3_free(pox_c);
        continue;
      }
      else {
        if ( S_ISDIR(buf_u.st_mode) ) {
          can = u3kb_weld(_unix_initial_update_dir(pox_c, bas_c), can);
        }
        else {
          can = u3kb_weld(_unix_initial_update_file(pox_c, bas_c), can);
        }
        c3_free(pox_c);
      }
    }
  }

  if ( closedir(rid_u) < 0 ) {
    u3l_log("error closing initial directory %s: %s",
            pax_c, strerror(errno));
  }

  return can;
}

/* u3_unix_initial_into_card(): create initial filesystem sync card.
*/
u3_noun
u3_unix_initial_into_card(c3_c* arv_c)
{
  u3_noun can = _unix_initial_update_dir(arv_c, arv_c);

  return u3nc(u3nt(c3__c, c3__sync, u3_nul),
              u3nq(c3__into, u3_nul, c3y, can));
}

/* _unix_sync_file(): sync file to unix
*/
static void
_unix_sync_file(u3_unix* unx_u, u3_udir* par_u, u3_noun nam, u3_noun ext, u3_noun mim)
{
  u3_assert( par_u );
  u3_assert( c3y == par_u->dir );

  // form file path

  c3_c* nam_c = _unix_knot_to_string(nam);
  c3_c* ext_c = _unix_knot_to_string(ext);
  c3_w  par_w = strlen(par_u->pax_c);
  c3_w  nam_w = strlen(nam_c);
  c3_w  ext_w = strlen(ext_c);
  c3_c* pax_c = c3_malloc(par_w + 1 + nam_w + 1 + ext_w + 1);

  strcpy(pax_c, par_u->pax_c);
  pax_c[par_w] = '/';
  strcpy(pax_c + par_w + 1, nam_c);
  pax_c[par_w + 1 + nam_w] = '.';
  strcpy(pax_c + par_w + 1 + nam_w + 1, ext_c);
  pax_c[par_w + 1 + nam_w + 1 + ext_w] = '\0';

  c3_free(nam_c); c3_free(ext_c);
  u3z(nam); u3z(ext);

  // check whether we already know about this file

  u3_unod* nod_u;
  for ( nod_u = par_u->kid_u;
        ( nod_u &&
          ( c3y == nod_u->dir ||
            0 != strcmp(nod_u->pax_c, pax_c) ) );
        nod_u = nod_u->nex_u )
  { }

  // apply change

  if ( u3_nul == mim ) {
    if ( nod_u ) {
      u3z(_unix_free_node(unx_u, nod_u, true));
    }
  }
  else {

    if ( !nod_u ) {
      c3_w gum_w = _unix_write_file_hard(pax_c, u3k(u3t(mim)));
      u3_ufil* fil_u = c3_malloc(sizeof(u3_ufil));
      _unix_watch_file(unx_u, fil_u, par_u, pax_c);
      fil_u->gum_w = gum_w;
    }
    else {
      _unix_write_file_soft((u3_ufil*) nod_u, u3k(u3t(mim)));
    }
  }

  c3_free(pax_c);
  u3z(mim);
}

/* _unix_sync_change(): sync single change to unix
*/
static void
_unix_sync_change(u3_unix* unx_u, u3_udir* dir_u, u3_noun pax, u3_noun mim)
{
  u3_assert( c3y == dir_u->dir );

  if ( c3n == u3du(pax) ) {
    if ( u3_nul == pax ) {
      u3l_log("can't sync out file as top-level, strange");
    }
    else {
      u3l_log("sync out: bad path");
    }
    u3z(pax); u3z(mim);
    return;
  }
  else if ( c3n == u3du(u3t(pax)) ) {
    u3l_log("can't sync out file as top-level, strangely");
    u3z(pax); u3z(mim);
  }
  else {
    u3_noun i_pax = u3h(pax);
    u3_noun t_pax = u3t(pax);
    u3_noun it_pax = u3h(t_pax);
    u3_noun tt_pax = u3t(t_pax);

    if ( u3_nul == tt_pax ) {
      _unix_sync_file(unx_u, dir_u, u3k(i_pax), u3k(it_pax), mim);
    }
    else {
      c3_c* nam_c = _unix_knot_to_string(i_pax);
      c3_w pax_w = strlen(dir_u->pax_c);
      u3_unod* nod_u;

      for ( nod_u = dir_u->kid_u;
            ( nod_u &&
              ( c3n == nod_u->dir ||
                0 != strcmp(nod_u->pax_c + pax_w + 1, nam_c) ) );
            nod_u = nod_u->nex_u )
      { }

      if ( !nod_u ) {
        nod_u = c3_malloc(sizeof(u3_udir));
        _unix_create_dir((u3_udir*) nod_u, dir_u, u3k(i_pax));
        _unix_maybe_watch(unx_u, (u3_udir*) nod_u);
      }

      if ( c3n == nod_u->dir ) {
        u3l_log("weird, we got a file when we weren't expecting to");
        u3_assert(0);
      }

      _unix_sync_change(unx_u, (u3_udir*) nod_u, u3k(t_pax), mim);
      
      c3_free(nam_c);
    }
  }
  u3z(pax);
}

/* _unix_sync_ergo(): sync list of changes to unix
*/
static void
_unix_sync_ergo(u3_unix* unx_u, u3_umon* mon_u, u3_noun can)
{
  u3_noun nac = can;
  u3_noun nam = _unix_string_to_knot(mon_u->nam_c);

  while ( u3_nul != nac) {
    _unix_sync_change(unx_u, &mon_u->dir_u,
                      u3nc(u3k(nam), u3k(u3h(u3h(nac)))),
                      u3k(u3t(u3h(nac))));
    nac = u3t(nac);
  }

  //  %ergo confirms a commit: persist the synced mugs
  //
  _unix_save_mugs(unx_u, mon_u);

  u3z(nam);
  u3z(can);
}

/* u3_unix_ef_dirk(): commit mount point
*/
void
u3_unix_ef_dirk(u3_unix* unx_u, u3_noun mon)
{
  _unix_commit_mount_point(unx_u, mon);
}

/* u3_unix_ef_ergo(): update filesystem from urbit
*/
void
u3_unix_ef_ergo(u3_unix* unx_u, u3_noun mon, u3_noun can)
{
  u3_umon* mon_u = _unix_get_mount_point(unx_u, mon);

  _unix_sync_ergo(unx_u, mon_u, can);
}

/* u3_unix_ef_ogre(): delete mount point
*/
void
u3_unix_ef_ogre(u3_unix* unx_u, u3_noun mon)
{
  _unix_delete_mount_point(unx_u, mon, true);
}

/* u3_unix_ef_wath(): start auto-syncing a mount point.
*/
void
u3_unix_ef_wath(u3_unix* unx_u, u3_noun mon)
{
  u3_umon* mon_u = _unix_get_mount_point(unx_u, mon);

  if ( c3n == mon_u->syn_o ) {
    mon_u->syn_o = c3y;
    _unix_watch_arm_deep(unx_u, &mon_u->dir_u);
  }

  //  reconciliation scan: sync any changes made while not watching
  //
  _unix_mark_wet(&mon_u->dir_u);
  _unix_update_mount(unx_u, mon_u, c3n);
  _unix_doom_recheck(unx_u);
}

/* u3_unix_ef_wend(): stop auto-syncing a mount point.
*/
void
u3_unix_ef_wend(u3_unix* unx_u, u3_noun mon)
{
  u3_umon* mon_u = _unix_get_mount_point(unx_u, mon);

  if ( c3y == mon_u->syn_o ) {
    mon_u->syn_o = c3n;
    _unix_watch_disarm_deep(&mon_u->dir_u);
  }
}

/* u3_unix_ef_hill(): enumerate mount points
*/
void
u3_unix_ef_hill(u3_unix* unx_u, u3_noun hil)
{
  u3_noun mon;

  for ( mon = hil; c3y == u3du(mon); mon = u3t(mon) ) {
    u3_umon* mon_u = _unix_get_mount_point(unx_u, u3k(u3h(mon)));
    _unix_load_mugs(unx_u, mon_u);
    _unix_scan_mount_point(unx_u, mon_u);
  }

  unx_u->car_u.liv_o = c3y;

  u3z(hil);
}

/* u3_unix_ef_look(): update the root of a specific mount point.
*/
void
u3_unix_ef_look(u3_unix* unx_u, u3_noun mon, u3_noun all)
{
  if ( c3y == unx_u->dyr ) {
    c3_c* nam_c = _unix_knot_to_string(mon);

    unx_u->dyr = c3n;
    u3_umon* mon_u = unx_u->mon_u;
    while ( mon_u && 0 != strcmp(nam_c, mon_u->nam_c) ) {
      mon_u = mon_u->nex_u;
    }
    c3_free(nam_c);
    if ( mon_u ) {
      _unix_mark_wet(&mon_u->dir_u);
      _unix_update_mount(unx_u, mon_u, all);
      _unix_doom_recheck(unx_u);
    }
  }
  u3z(mon);
}

/* _unix_io_talk(): start listening for fs events.
*/
static void
_unix_io_talk(u3_auto* car_u)
{
  //  XX review wire
  //
  u3_noun wir = u3nc(c3__boat, u3_nul);
  u3_noun cad = u3nc(c3__boat, u3_nul);

  u3_auto_plan(car_u, u3_ovum_init(0, c3__c, wir, cad));
}

/* _unix_io_kick(): apply effects.
*/
static c3_o
_unix_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_unix* unx_u = (u3_unix*)car_u;

  u3_noun tag, dat, i_wir;
  c3_o ret_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3n == u3r_cell(cad, &tag, &dat))
     || (  (c3__clay != i_wir)
        && (c3__boat != i_wir)
        && (c3__sync != i_wir) )  )
  {
    ret_o = c3n;
  }
  else {
    switch ( tag ) {
      default: {
        ret_o = c3n;
      } break;

      case c3__dirk: {
        u3_unix_ef_dirk(unx_u, u3k(dat));
        ret_o = c3y;
      } break;

      case c3__ergo: {
        u3_noun mon = u3k(u3h(dat));
        u3_noun can = u3k(u3t(dat));
        u3_unix_ef_ergo(unx_u, mon, can);

        ret_o = c3y;
      } break;

      case c3__ogre: {
        u3_unix_ef_ogre(unx_u, u3k(dat));
        ret_o = c3y;
      } break;

      case c3__hill: {
        u3_unix_ef_hill(unx_u, u3k(dat));
        ret_o = c3y;
      } break;

      case c3__wath: {
        u3_unix_ef_wath(unx_u, u3k(dat));
        ret_o = c3y;
      } break;

      case c3__wend: {
        u3_unix_ef_wend(unx_u, u3k(dat));
        ret_o = c3y;
      } break;
    }
  }

  u3z(wir); u3z(cad);
  return ret_o;
}

static u3m_quac**
_unix_io_mark(u3_auto* car_u, c3_w *out_w)
{
  u3m_quac** all_u = c3_malloc(2 * sizeof(*all_u));

  all_u[0] = c3_malloc(sizeof(**all_u));
  all_u[0]->nam_c = strdup("+sane handle");
  all_u[0]->siz_w = 4 * u3a_mark_noun(((u3_unix*)car_u)->sat);
  all_u[0]->qua_u = 0;

  all_u[1] = 0;

  *out_w = all_u[0]->siz_w;

  return all_u;
}

/* _unix_io_exit(): terminate unix I/O.
*/
static void
_unix_timer_close_cb(uv_handle_t* han_u)
{
  c3_free(han_u);
}

static void
_unix_io_exit(u3_auto* car_u)
{
  u3_unix* unx_u = (u3_unix*)car_u;

  u3_umon* mon_u = unx_u->mon_u;
  u3_umon* nex_u;
  while ( mon_u ) {
    nex_u = mon_u->nex_u;
    _unix_save_mugs(unx_u, mon_u);
    _unix_free_mount_point(unx_u, mon_u, false);
    mon_u = nex_u;
  }

  uv_close((uv_handle_t*)unx_u->syt_u, _unix_timer_close_cb);
  uv_close((uv_handle_t*)unx_u->swt_u, _unix_timer_close_cb);

  u3z(unx_u->sat);
  c3_free(unx_u->pax_c);
  c3_free(unx_u);
}

/* u3_unix_io_init(): initialize unix sync.
*/
u3_auto*
u3_unix_io_init(u3_pier* pir_u)
{
  u3_unix* unx_u = c3_calloc(sizeof(*unx_u));
  unx_u->mon_u = 0;
  unx_u->pax_c = strdup(pir_u->pax_c);
  unx_u->alm = c3n;
  unx_u->dyr = c3n;
  unx_u->sat = u3do("sane", c3__ta);
  unx_u->pen_u = NULL;
  unx_u->fir_d = 0;
  unx_u->dum_o = c3n;

  unx_u->syt_u = c3_malloc(sizeof(*unx_u->syt_u));
  uv_timer_init(u3L, unx_u->syt_u);
  unx_u->syt_u->data = unx_u;

  unx_u->swt_u = c3_malloc(sizeof(*unx_u->swt_u));
  uv_timer_init(u3L, unx_u->swt_u);
  unx_u->swt_u->data = unx_u;
  uv_timer_start(unx_u->swt_u, _unix_sweep_cb, SYNC_SWEEP_MS, SYNC_SWEEP_MS);

  u3_auto* car_u = &unx_u->car_u;
  car_u->nam_m = c3__unix;
  car_u->liv_o = c3n;
  car_u->io.talk_f = _unix_io_talk;
  car_u->io.kick_f = _unix_io_kick;
  car_u->io.mark_f = _unix_io_mark;
  car_u->io.exit_f = _unix_io_exit;
  //  XX wat do
  //
  // car_u->ev.bail_f = ...l;

  {
    u3_noun now;
    struct timeval tim_u;
    gettimeofday(&tim_u, 0);

    now = u3m_time_in_tv(&tim_u);
    unx_u->sev_l = u3r_mug(now);
    u3z(now);
  }

  return car_u;
}
