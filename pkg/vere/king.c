/// @file
///
/// The main loop of the daemon process.

#include "vere.h"

#include "curl/curl.h"
#include "ivory.h"
#include "noun.h"
#include "pace.h"
#include "ur.h"
#include "uv.h"
#include "version.h"

// It's unclear which file owns this variable, so put it here for now.
u3_host u3_Host;

u3_king u3_King;

static const c3_c* ver_hos_c = "https://bootstrap.urbit.org/vere";

//  stash config flags for worker
//
static c3_w sag_w;

/*
::  skeleton client->king protocol
::
|%
::  +doom: daemon command
::
::    Should require auth to the daemon itself
::
+$  doom
  $%  ::  boot
      ::
      ::  p: boot procedure
      ::  q: pill specifier
      ::  r: path to pier
      ::
      [%boot p=boot q=pill r=@t]
      ::  end the daemon
      ::
      ::    XX not implemented
      ::
      [%exit ~]
      ::  acquire a pier
      ::
      ::    XX used for restart, may not be right
      ::
      [%pier p=(unit @t)]
      ::  admin ship actions
      ::
      ::    XX not implemented
      ::
      [%root p=ship q=wyrd]
  ==
::  +boot: boot procedures
::
+$  boot
  $%  ::  mine a comet
      ::
      ::  p: optionally under a specific star
      ::
      [%come p=(unit ship)]
      ::  boot with real keys
      ::
      ::    And perform pre-boot validation, retrieve snapshot, etc.
      ::
      [%dawn p=seed]
      ::  boot with fake keys
      ::
      ::  p: identity
      ::
      [%fake p=ship]
  ==
::  +pill: boot-sequence ingredients
::
::    p: jammed pill
::    q: optional %into ovum overriding that of .p
::
+$  pill  [p=@ q=(unit ovum)]
--
*/

void _king_doom(u3_noun doom);
  void _king_boot(u3_noun boot);
    void _king_come(u3_noun star, u3_noun pill, u3_noun path);
    void _king_dawn(u3_noun seed, u3_noun pill, u3_noun path);
    void _king_fake(u3_noun ship, u3_noun pill, u3_noun path);
  void _king_pier(u3_noun pier);

/* _king_defy_fate(): invalid fate
*/
void
_king_defy_fate()
{
  exit(1);
}

/* _king_doom(): doom parser
*/
void
_king_doom(u3_noun doom)
{
  u3_noun load;
  void (*next)(u3_noun);

  c3_assert(_(u3a_is_cell(doom)));
  c3_assert(_(u3a_is_cat(u3h(doom))));

  switch ( u3h(doom) ) {
    case c3__boot:
      next = _king_boot;
      break;
    case c3__pier:
      next = _king_pier;
      break;
    default:
      _king_defy_fate();
  }

  load = u3k(u3t(doom));
  u3z(doom);
  next(load);
}

/* _king_boot(): boot parser
*/
void
_king_boot(u3_noun bul)
{
  u3_noun boot, pill, path;
  void (*next)(u3_noun, u3_noun, u3_noun);

  c3_assert(_(u3a_is_cell(bul)));
  u3x_trel(bul, &boot, &pill, &path);
  c3_assert(_(u3a_is_cat(u3h(boot))));

  switch ( u3h(boot) ) {
    case c3__fake: {
      next = _king_fake;
      break;
    }
    case c3__come: {
      next = _king_come;
      break;
    }
    case c3__dawn: {
      next = _king_dawn;
      break;
    }
    default:
      return _king_defy_fate();
  }

  next(u3k(u3t(boot)), u3k(pill), u3k(path));
  u3z(bul);
}

/* _king_fake(): boot with fake keys
*/
void
_king_fake(u3_noun ship, u3_noun pill, u3_noun path)
{
  //  XX link properly
  //
  u3_noun vent = u3nc(c3__fake, u3k(ship));
  u3K.pir_u    = u3_pier_boot(sag_w, ship, vent, pill, path, u3_none);
}

/* _king_come(): mine a comet under star (unit)
**
**   XX revise to exclude star argument
*/
void
_king_come(u3_noun star, u3_noun pill, u3_noun path)
{
  _king_dawn(u3_dawn_come(), pill, path);
}

static void
_king_slog(u3_noun hod)
{
  u3_pier_tank(0, 0, u3k(u3t(hod)));
  u3z(hod);
}

/* _king_dawn(): boot from keys, validating
*/
void
_king_dawn(u3_noun feed, u3_noun pill, u3_noun path)
{
  // enable ivory slog printfs
  //
  u3C.slog_f = _king_slog;

  u3_noun ship = ( c3y == u3a_is_cell(u3h(feed)) )
                 ? u3h(u3t(feed))
                 : u3h(feed);
  u3_noun vent = u3_dawn_vent(u3k(ship), u3k(feed));
  //  XX link properly
  //
  u3K.pir_u    = u3_pier_boot(sag_w, u3k(ship), vent, pill, path, feed);

  // disable ivory slog printfs
  //
  u3C.slog_f = 0;
}

/* _king_pier(): pier parser
*/
void
_king_pier(u3_noun pier)
{
  if ( (c3n == u3du(pier)) ||
       (c3n == u3ud(u3t(pier))) ) {
    u3m_p("daemon: invalid pier", pier);
    exit(1);
  }

  u3K.pir_u = u3_pier_stay(sag_w, u3k(u3t(pier)));
  u3z(pier);
}

/* _king_curl_alloc(): allocate a response buffer for curl
**  XX deduplicate with dawn.c
*/
static size_t
_king_curl_alloc(void* dat_v, size_t uni_t, size_t mem_t, void* buf_v)
{
  uv_buf_t* buf_u = buf_v;

  size_t siz_t = uni_t * mem_t;
  buf_u->base = c3_realloc(buf_u->base, 1 + siz_t + buf_u->len);

  memcpy(buf_u->base + buf_u->len, dat_v, siz_t);
  buf_u->len += siz_t;
  buf_u->base[buf_u->len] = 0;

  return siz_t;
}

/* _king_curl_bytes(): HTTP GET url_c, produce response body bytes.
**  XX deduplicate with dawn.c
*/
static c3_i
_king_curl_bytes(c3_c* url_c, c3_w* len_w, c3_y** hun_y, c3_t veb_t)
{
  c3_i     ret_i = 0;
  CURL    *cul_u;
  CURLcode res_i;
  long     cod_i;
  uv_buf_t buf_u = uv_buf_init(c3_malloc(1), 0);

  if ( !(cul_u = curl_easy_init()) ) {
    u3l_log("failed to initialize libcurl\n");
    exit(1);
  }

  u3K.ssl_curl_f(cul_u);
  curl_easy_setopt(cul_u, CURLOPT_URL, url_c);
  curl_easy_setopt(cul_u, CURLOPT_WRITEFUNCTION, _king_curl_alloc);
  curl_easy_setopt(cul_u, CURLOPT_WRITEDATA, (void*)&buf_u);

  res_i = curl_easy_perform(cul_u);
  curl_easy_getinfo(cul_u, CURLINFO_RESPONSE_CODE, &cod_i);

  //  XX retry?
  //
  if ( CURLE_OK != res_i ) {
    if ( veb_t ) {
      u3l_log("curl: failed %s: %s\n", url_c, curl_easy_strerror(res_i));
    }
    ret_i = -1;
  }
  if ( 300 <= cod_i ) {
    if ( veb_t ) {
      u3l_log("curl: error %s: HTTP %ld\n", url_c, cod_i);
    }
    ret_i = -2;
  }

  curl_easy_cleanup(cul_u);

  *len_w = buf_u.len;
  *hun_y = (c3_y*)buf_u.base;

  return ret_i;
}

/* _king_get_atom(): HTTP GET url_c, produce response body as atom.
*/
static u3_noun
_king_get_atom(c3_c* url_c)
{
  c3_w  len_w;
  c3_y* hun_y;
  u3_noun pro;

  if ( _king_curl_bytes(url_c, &len_w, &hun_y, 1) ) {
    u3_king_bail();
    exit(1);
  }

  pro = u3i_bytes(len_w, hun_y);
  c3_free(hun_y);
  return pro;
}

/* _king_get_pace(): get "pace" (release channel name).
*/
static c3_c*
_king_get_pace(void)
{
  struct stat buf_u;
  c3_c*       pat_c;
  c3_w red_w, len_w;
  c3_i ret_i, fid_i;

  ret_i = asprintf(&pat_c, "%s/.bin/pace", u3_Host.dir_c);
  c3_assert( ret_i > 0 );

  fid_i = c3_open(pat_c, O_RDONLY, 0644);

  if ( (fid_i < 0) || (fstat(fid_i, &buf_u) < 0) ) {
    c3_free(pat_c);
    return strdup("live");
  }

  c3_free(pat_c);

  len_w = buf_u.st_size;
  pat_c = c3_malloc(len_w + 1);
  red_w = read(fid_i, pat_c, len_w);
  close(fid_i);

  if ( len_w != red_w ) {
    c3_free(pat_c);
    u3l_log("unable to read pace file, "
            "falling back to default (\"live\")\n");
    return strdup("live");
  }

  pat_c[len_w] = 0;

  while ( len_w-- && isspace(pat_c[len_w]) ) {
    pat_c[len_w] = 0;
  }

  return pat_c;
}

/* u3_king_next(): get next vere version string, if it exists.
**         return: 0 is success, -1 is no-op (same version), -2 is error
*/
c3_i
u3_king_next(c3_c* pac_c, c3_c** out_c)
{
  c3_c* ver_c;
  c3_c* url_c;
  c3_w  len_w;
  c3_y* hun_y;
  c3_i  ret_i;

  ret_i = asprintf(&url_c, "%s/%s/%s/next", ver_hos_c, pac_c, URBIT_VERSION);
  c3_assert( ret_i > 0 );

  //  skip printfs on failed requests (/next is usually not present)
  //
  if ( _king_curl_bytes(url_c, &len_w, &hun_y, 0) ) {
    c3_free(url_c);

    ret_i = asprintf(&url_c, "%s/%s/last", ver_hos_c, pac_c);
    c3_assert( ret_i > 0 );

    //  enable printfs on failed requests (/last must be present)
    //  XX support channel redirections
    //
    if ( _king_curl_bytes(url_c, &len_w, &hun_y, 1) )
    {
      c3_free(url_c);
      return -2;
    }
  }

  c3_free(url_c);

  //  null-terminate
  //
  hun_y = c3_realloc(hun_y, 1 + len_w);
  hun_y[len_w] = 0;
  c3_c* newline_c;
  while ( (newline_c = strrchr((c3_c*)hun_y, '\n')) ) {
    *newline_c = 0;
  }

  ver_c = (c3_c*)hun_y;

  //  XX trim ver_c ?
  //
  if ( 0 == strcmp(ver_c, URBIT_VERSION) ) {
    c3_free(ver_c);
    return -1;
  }

  *out_c = ver_c;
  return 0;
}

/* _get_cmd_output(): Run a shell command and capture its output.
   Exits with an error if the command fails or produces no output.
   The 'out_c' parameter should be an array of sufficient length to hold
   the command's output, up to a max of len_c characters.
*/
static void
_get_cmd_output(c3_c *cmd_c, c3_c *out_c, c3_w len_c)
{
  FILE *fp = popen(cmd_c, "r");
  if ( NULL == fp ) {
    u3l_log("'%s' failed\n", cmd_c);
    exit(1);
  }

  if ( NULL == fgets(out_c, len_c, fp) ) {
    u3l_log("'%s' produced no output\n", cmd_c);
    exit(1);
  }

  pclose(fp);
}

/* _arvo_hash(): get a shortened hash of the last git commit
   that modified the sys/ directory in arvo.
   hax_c must be an array with length >= 11.
*/
static void
_arvo_hash(c3_c *out_c, c3_c *arv_c)
{
  c3_c cmd_c[2048];

  sprintf(cmd_c, "git -C %s log -1 HEAD --format=%%H -- sys/", arv_c);
  _get_cmd_output(cmd_c, out_c, 11);

  out_c[10] = 0;  //  end with null-byte
}

/* _git_pill_url(): produce a URL from which to download a pill
   based on the location of an arvo git repository.
*/
static void
_git_pill_url(c3_c *out_c, c3_c *arv_c)
{
  c3_c hax_c[11];

  assert(NULL != arv_c);

  if ( 0 != system("which git >> /dev/null") ) {
    u3l_log("boot: could not find git executable\r\n");
    exit(1);
  }

  _arvo_hash(hax_c, arv_c);
  sprintf(out_c, "https://bootstrap.urbit.org/git-%s.pill", hax_c);
}

/* _boothack_pill(): parse CLI pill arguments into +pill specifier
*/
static u3_noun
_boothack_pill(void)
{
  u3_noun arv = u3_nul;
  u3_noun pil;

  if ( 0 != u3_Host.ops_u.pil_c ) {
    u3l_log("boot: loading pill %s\r\n", u3_Host.ops_u.pil_c);
    pil = u3m_file(u3_Host.ops_u.pil_c);
  }
  else {
    c3_c url_c[2048];

    if ( (c3y == u3_Host.ops_u.git) &&
       (0 != u3_Host.ops_u.arv_c) )
    {
      _git_pill_url(url_c, u3_Host.ops_u.arv_c);
    }
    else {
      c3_assert( 0 != u3_Host.ops_u.url_c );
      strcpy(url_c, u3_Host.ops_u.url_c);
    }

    u3l_log("boot: downloading pill %s\r\n", url_c);
    pil = _king_get_atom(url_c);
  }

  if ( 0 != u3_Host.ops_u.arv_c ) {
    u3l_log("boot: preparing filesystem from %s\r\n",
            u3_Host.ops_u.arv_c);
    arv = u3nc(u3_nul, u3_unix_initial_into_card(u3_Host.ops_u.arv_c));
  }

  return u3nc(pil, arv);
}

/* _boothack_key(): parse a private key file or value
*/
static u3_noun
_boothack_key(u3_noun kef)
{
  u3_noun seed;
  u3_weak ship = u3_none;

  {
    u3_noun des = u3dc("slaw", c3__uw, u3k(kef));

    if ( u3_nul == des ) {
      c3_c* kef_c = u3r_string(kef);
      u3l_log("dawn: invalid private keys: %s\r\n", kef_c);
      c3_free(kef_c);
      exit(1);
    }

    //  +feed:able:jael: keyfile
    //
    u3_noun pro = u3m_soft(0, u3ke_cue, u3k(u3t(des)));
    if ( u3_blip != u3h(pro) ) {
      u3l_log("dawn: unable to cue keyfile\r\n");
      exit(1);
    }
    seed = u3k(u3t(pro));
    u3z(pro);

    //  if it's a single seed, we can trivially sanity-check early
    //
    if ( c3y == u3ud(u3h(seed)) ) {
      //  local reference, not counted
      //
      ship = u3h(seed);
    }

    u3z(des);
    u3z(kef);
  }

  if ( 0 != u3_Host.ops_u.who_c ) {
    u3_noun woh = u3i_string(u3_Host.ops_u.who_c);
    u3_noun whu = u3dc("slaw", 'p', u3k(woh));

    if ( u3_nul == whu ) {
      u3l_log("dawn: invalid ship specified with -w %s\r\n",
              u3_Host.ops_u.who_c);
      exit(1);
    }

    if ( (u3_none != ship) &&
         (c3n == u3r_sing(ship, u3t(whu))) )
    {
      u3_noun how = u3dc("scot", 'p', u3k(ship));
      c3_c* how_c = u3r_string(u3k(how));
      u3l_log("dawn: mismatch between -w %s and -K %s\r\n",
              u3_Host.ops_u.who_c, how_c);

      u3z(how);
      c3_free(how_c);
      exit(1);
    }

    u3z(woh);
    u3z(whu);
  }

  return seed;
}

/* _boothack_doom(): parse CLI arguments into $doom
*/
static u3_noun
_boothack_doom(void)
{
  u3_noun pax = u3i_string(u3_Host.dir_c);
  u3_noun bot;

  if ( c3n == u3_Host.ops_u.nuu ) {
    return u3nt(c3__pier, u3_nul, pax);
  }
  else if ( 0 != u3_Host.ops_u.fak_c ) {
    u3_noun fak = u3i_string(u3_Host.ops_u.fak_c);
    u3_noun whu = u3dc("slaw", 'p', u3k(fak));

    if ( u3_nul == whu ) {
      u3l_log("boot: malformed -F ship %s\r\n", u3_Host.ops_u.fak_c);
      u3_king_bail();
    }

    bot = u3nc(c3__fake, u3k(u3t(whu)));

    u3z(whu);
    u3z(fak);
  }
  else if ( 0 != u3_Host.ops_u.who_c ) {
    u3_noun kef;

    if ( 0 != u3_Host.ops_u.key_c ) {
      kef = u3m_file(u3_Host.ops_u.key_c);

      // handle trailing newline
      //
      {
        c3_c* key_c = u3r_string(kef);
        c3_w  len_w = strlen(key_c);

        if (len_w && (key_c[len_w - 1] == '\n')) {
          key_c[len_w - 1] = '\0';
          u3z(kef);
          kef = u3i_string(key_c);
        }

        c3_free(key_c);
      }
    }
    else if ( 0 != u3_Host.ops_u.gen_c ) {
      kef = u3i_string(u3_Host.ops_u.gen_c);
    }
    else {
      u3l_log("boot: must specify a key with -k or -G\r\n");
      exit(1);
    }

    bot = u3nc(c3__dawn, _boothack_key(kef));
  }
  else {
    //  XX allow parent star to be specified?
    //
    bot = u3nc(c3__come, u3_nul);
  }

  return u3nq(c3__boot, bot, _boothack_pill(), pax);
}

/* _king_sign_init(): initialize daemon signal handlers
*/
static void
_king_sign_init(void)
{
  //  gracefully shutdown on SIGTERM
  //
  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->num_i = SIGTERM;
    sig_u->nex_u = u3_Host.sig_u;
    u3_Host.sig_u = sig_u;
  }

  //  forward SIGINT to worker
  //
  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->num_i = SIGINT;
    sig_u->nex_u = u3_Host.sig_u;
    u3_Host.sig_u = sig_u;
  }

  //  inject new dimensions after terminal resize
  //
  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->num_i = SIGWINCH;
    sig_u->nex_u = u3_Host.sig_u;
    u3_Host.sig_u = sig_u;
  }

  //  handle SIGINFO (if available)
  //
#ifdef SIGINFO
  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->num_i = SIGINFO;
    sig_u->nex_u = u3_Host.sig_u;
    u3_Host.sig_u = sig_u;
  }
#endif

  //  handle SIGUSR1 (fallback for SIGINFO)
  //
  {
    u3_usig* sig_u;

    sig_u = c3_malloc(sizeof(u3_usig));
    uv_signal_init(u3L, &sig_u->sil_u);

    sig_u->num_i = SIGUSR1;
    sig_u->nex_u = u3_Host.sig_u;
    u3_Host.sig_u = sig_u;
  }
}

/* _king_sign_cb: signal callback.
*/
static void
_king_sign_cb(uv_signal_t* sil_u, c3_i num_i)
{
  switch ( num_i ) {
    default: {
      u3l_log("\r\nmysterious signal %d\r\n", num_i);
      break;
    }

    case SIGTERM: {
      u3_king_exit();
      break;
    }

    case SIGINT: {
      u3l_log("\r\ninterrupt\r\n");
      u3_term_ef_ctlc();

#ifdef U3_OS_mingw
      PulseEvent(u3_Host.cev_u);
#endif
      break;
    }

    case SIGWINCH: {
      u3_term_ef_winc();
      break;
    }

    //  fallthru if defined
    //
#ifdef SIGINFO
    case SIGINFO:
#endif
    case SIGUSR1: {
      u3_king_slog();
      break;
    }
  }
}

/* _king_sign_move(): enable daemon signal handlers
*/
static void
_king_sign_move(void)
{
  u3_usig* sig_u;

  for ( sig_u = u3_Host.sig_u; sig_u; sig_u = sig_u->nex_u ) {
    uv_signal_start(&sig_u->sil_u, _king_sign_cb, sig_u->num_i);
  }
}

/* _king_sign_hold(): disable daemon signal handlers
*/
static void
_king_sign_hold(void)
{
  u3_usig* sig_u;

  for ( sig_u = u3_Host.sig_u; sig_u; sig_u = sig_u->nex_u ) {
    uv_signal_stop(&sig_u->sil_u);
  }
}

/* _king_sign_close(): dispose daemon signal handlers
*/
static void
_king_sign_close(void)
{
  u3_usig* sig_u;

  for ( sig_u = u3_Host.sig_u; sig_u; sig_u = sig_u->nex_u ) {
    uv_close((uv_handle_t*)&sig_u->sil_u, (uv_close_cb)free);
  }
}
/* _boothack_cb(): setup pier via message as if from client.
*/
void
_boothack_cb(uv_timer_t* tim_u)
{
  _king_doom(_boothack_doom());
}

/* _king_loop_init(): stuff that comes before the event loop
*/
void
_king_loop_init()
{
  //  initialize terminal/logging
  //
  u3_term_log_init();

  //  start signal handlers
  //
  _king_sign_init();
  _king_sign_move();

  //  async "boothack"
  // /
  uv_timer_start(&u3K.tim_u, _boothack_cb, 0, 0);
}

/* _king_loop_exit(): cleanup after event loop
*/
void
_king_loop_exit()
{
}

static void
_king_boot_ivory(void)
{
  c3_d  len_d;
  c3_y* byt_y;

  if ( u3_Host.ops_u.lit_c ) {
    if ( c3n == u3u_mmap_read("lite", u3_Host.ops_u.lit_c, &len_d, &byt_y) ) {
      u3l_log("lite: unable to load ivory pill at %s\n",
              u3_Host.ops_u.lit_c);
      exit(1);
    }
  }
  else {
    len_d = u3_Ivory_pill_len;
    byt_y = u3_Ivory_pill;
  }

  {
    u3_cue_xeno* sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
    u3_weak        pil;

    if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, len_d, byt_y)) ) {
      u3l_log("lite: unable to cue ivory pill\r\n");
      exit(1);
    }

    u3s_cue_xeno_done(sil_u);

    if ( c3n == u3v_boot_lite(pil)) {
      u3l_log("lite: boot failed\r\n");
      exit(1);
    }
  }

  if ( u3_Host.ops_u.lit_c ) {
    if ( c3n == u3u_munmap(len_d, byt_y) ) {
      u3l_log("lite: unable to unmap ivory pill at %s\n",
              u3_Host.ops_u.lit_c);
      exit(1);
    }
  }
}

/* u3_king_commence(): start the daemon
*/
void
u3_king_commence()
{
  u3_Host.lup_u = uv_default_loop();

  //  initialize top-level timer
  //
  uv_timer_init(u3L, &u3K.tim_u);

  //  start up a "fast-compile" arvo for internal use only
  //  (with hashboard and sample-profiling always disabled)
  //
  sag_w = u3C.wag_w;
  u3C.wag_w |= u3o_hashless;
  u3C.wag_w &= ~u3o_debug_cpu;

  //  wire up signal controls
  //
  u3C.sign_hold_f = _king_sign_hold;
  u3C.sign_move_f = _king_sign_move;

  //  Ignore SIGPIPE signals.
  #ifndef U3_OS_mingw
  {
    struct sigaction sig_s = {{0}};
    sigemptyset(&(sig_s.sa_mask));
    sig_s.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig_s, 0);
  }
  #endif

  //  boot the ivory pill
  //
  _king_boot_ivory();

  //  disable core dumps (due to lmdb size)
  //
  #ifndef U3_OS_mingw
  {
    struct rlimit rlm;

    getrlimit(RLIMIT_CORE, &rlm);
    rlm.rlim_cur = 0;

    if ( 0 != setrlimit(RLIMIT_CORE, &rlm) ) {
      u3l_log("king: unable to disable core dumps: %s\r\n", strerror(errno));
      exit(1);
    }
  }
  #endif

  //  run the loop
  //
  _king_loop_init();
  uv_run(u3L, UV_RUN_DEFAULT);
  _king_loop_exit();
  u3m_stop();
}

/* u3_king_stub(): get the One Pier for unreconstructed code.
*/
u3_pier*
u3_king_stub(void)
{
  if ( !u3K.pir_u ) {
    c3_assert(!"king: no pier");
  }
  else {
    return u3K.pir_u;
  }
}

/* _king_forall(): run on all piers
*/
static void
_king_forall(void (*pir_f)(u3_pier*))
{
  u3_pier* pir_u = u3K.pir_u;

  while ( pir_u ) {
    pir_f(pir_u);
    pir_u = pir_u->nex_u;
  }
}

/* u3_king_slog(): print status info.
*/
void
u3_king_slog(void)
{
  _king_forall(u3_pier_slog);
}

/* _king_forall_unlink(): run on all piers, unlinking from king.
*/
static void
_king_forall_unlink(void (*pir_f)(u3_pier*))
{
  u3_pier* pir_u = u3K.pir_u;

  while ( u3K.pir_u ) {
    u3_pier* pir_u = u3K.pir_u;
    u3K.pir_u = pir_u->nex_u;
    pir_f(pir_u);
  }
}

/* _king_curl_file(): HTTP GET [url_c], write response body to [fil_u].
*/
static c3_i
_king_save_file(c3_c* url_c, FILE* fil_u)
{
  c3_i     ret_i = 0;
  CURL    *cul_u;
  CURLcode res_i;
  long     cod_i;

  if ( !(cul_u = curl_easy_init()) ) {
    u3l_log("failed to initialize libcurl\n");
    exit(1);
  }

  u3K.ssl_curl_f(cul_u);
  curl_easy_setopt(cul_u, CURLOPT_URL, url_c);
  curl_easy_setopt(cul_u, CURLOPT_WRITEDATA, (void*)fil_u);

  res_i = curl_easy_perform(cul_u);
  curl_easy_getinfo(cul_u, CURLINFO_RESPONSE_CODE, &cod_i);

  //  XX retry?
  //
  if ( CURLE_OK != res_i ) {
    u3l_log("curl: failed %s: %s\n", url_c, curl_easy_strerror(res_i));
    ret_i = -1;
  }
  if ( 300 <= cod_i ) {
    u3l_log("curl: error %s: HTTP %ld\n", url_c, cod_i);
    ret_i = -2;
  }

  curl_easy_cleanup(cul_u);
  return ret_i;
}

/* _king_make_pace(): mkdir -p $pier/.bin/[pace]
*/
static c3_i
_king_make_pace(c3_c* pac_c)
{
  c3_c* bin_c;
  c3_i  ret_i;

  ret_i = asprintf(&bin_c, "%s/.bin", u3_Host.dir_c);
  c3_assert( ret_i > 0 );

  ret_i = c3_mkdir(bin_c, 0700);

  if ( ret_i && (EEXIST != errno) ) {
    fprintf(stderr, "vere: mkdir %s failed: %s\n", bin_c, strerror(errno));
    c3_free(bin_c);
    return -1;
  }

  c3_free(bin_c);

  ret_i = asprintf(&bin_c, "%s/.bin/%s/", u3_Host.dir_c, pac_c);
  c3_assert( ret_i > 0 );

  //  XX asserting wrapper conflicts here (and is bypassed for .urb)
  //
  ret_i = mkdir(bin_c, 0700);

  if ( ret_i && (EEXIST != errno) ) {
    fprintf(stderr, "vere: mkdir %s failed: %s\n", bin_c, strerror(errno));
    c3_free(bin_c);
    return -1;
  }

  c3_free(bin_c);
  return 0;
}

static c3_i
_king_write_raw(c3_i fid_i, c3_y* buf_y, size_t len_i);

/* _king_init_pace(): save pace file if not present
*/
static c3_i
_king_init_pace(c3_c* pac_c)
{
  c3_c* bin_c;
  c3_i  fid_i, ret_i = asprintf(&bin_c, "%s/.bin/pace", u3_Host.dir_c);
  c3_assert( ret_i > 0 );

  if ( (-1 == (fid_i = open(bin_c, O_WRONLY | O_CREAT | O_EXCL, 0644))) ) {
    if ( EEXIST == errno ) {
      c3_free(bin_c);
      //  XX print something here?
      //
      return 0;
    }
    else {
      u3l_log("dock: init pace (%s): open %s\n", pac_c, strerror(errno));
      c3_free(bin_c);
      return -1;
    }
  }

  if ( _king_write_raw(fid_i, (c3_y*)pac_c, strlen(pac_c)) ) {
    u3l_log("dock: init pace (%s): write %s\n", pac_c, strerror(errno));
    close(fid_i);
    c3_free(bin_c);
    return -1;
  }
  // XX sync first?
  //
  else if ( close(fid_i) ) {
    u3l_log("dock: init pace (%s): close %s\n", pac_c, strerror(errno));
    c3_free(bin_c);
    return 1;
  }

  u3l_log("dock: pace (%s): configured at %s/.bin/pace\r\n",
          pac_c, u3_Host.dir_c);

  return 0;
}

/* _king_link_run(): ln [bin_c] $pier/.run
*/
static c3_i
_king_link_run(c3_c* bin_c)
{
  c3_c* lin_c;
  c3_i  ret_i;

  ret_i = asprintf(&lin_c, "%s/%s", u3_Host.dir_c, U3_BIN_ALIAS);
  c3_assert( ret_i > 0 );

  ret_i = unlink(lin_c);

  if ( ret_i && (ENOENT != errno) ) {
    fprintf(stderr, "vere: unlink %s failed: %s\n", lin_c, strerror(errno));
    c3_free(lin_c);
    return -1;
  }

  ret_i = link(bin_c, lin_c);

  if ( ret_i ) {
    fprintf(stderr, "vere: link %s -> %s failed: %s\n",
                    lin_c, bin_c, strerror(errno));
    c3_free(lin_c);
    return -1;
  }

  c3_free(lin_c);
  return 0;
}

/* u3_king_vere(): download binary as specified.
*/
c3_i
u3_king_vere(c3_c* pac_c,  // pace
             c3_c* ver_c,  // version
             c3_c* arc_c,  // architecture
             c3_c* dir_c,  // output directory
             c3_t  lin_t)  // link to $pier/.run
{
  c3_c* bin_c;
  c3_c* url_c;
  FILE* fil_u;
  c3_i  fid_i, ret_i;

  ret_i = asprintf(&bin_c, "%s/vere-v%s-%s", dir_c, ver_c, arc_c);
  c3_assert( ret_i > 0 );

  if (   (-1 == (fid_i = open(bin_c, O_WRONLY | O_CREAT | O_EXCL, 0755)))
     || !(fil_u = fdopen(fid_i, "wb")) )
  {
    if ( EEXIST == errno ) {
      u3l_log("already installed\n");
      c3_free(bin_c);
      return 0;
    }
    else {
      u3l_log("unable to open %s: %s\r\n", bin_c, strerror(errno));
      c3_free(bin_c);
      return -1;
    }
  }

  ret_i = asprintf(&url_c, "%s/%s/v%s/vere-v%s-%s",
                   ver_hos_c, pac_c, ver_c, ver_c, arc_c);
  c3_assert( ret_i > 0 );

  if ( (ret_i = _king_save_file(url_c, fil_u)) ) {
    u3l_log("unable to save %s to %s: %d\r\n", url_c, bin_c, ret_i);
    c3_free(url_c);
    fclose(fil_u);
    unlink(bin_c);
    c3_free(bin_c);
    return -1; // XX
  }

  //  XX sync unnecessary here?
  //
  if ( fflush(fil_u) || c3_sync(fid_i) ) {
    fprintf(stderr, "vere: sync %s failed: %s\n", bin_c, strerror(errno));
    c3_free(url_c);
    fclose(fil_u);
    unlink(bin_c);
    c3_free(bin_c);
    return -1;
  }

  fclose(fil_u);

  //  XX if link fails wat do?
  //  XX set via cli option
  //
  if ( lin_t ) {
    if ( _king_link_run(bin_c) ) {
      fprintf(stderr, "vere: link %s/%s failed\n", u3_Host.dir_c, U3_BIN_ALIAS);
      c3_free(url_c);
      c3_free(bin_c);
      return -1;
    }
  }

  u3l_log("vere: saved to %s\n", bin_c);

  c3_free(url_c);
  c3_free(bin_c);

  return 0;
}

/* _king_do_upgrade(): get arch-appropriate binary at [ver_c].
*/
static void
_king_do_upgrade(c3_c* pac_c, c3_c* ver_c)
{
  c3_c* dir_c;
  c3_c* arc_c;

#ifdef U3_OS_ARCH
  arc_c = U3_OS_ARCH;
#else
  if ( u3_Host.arc_c ) {
    arc_c = u3_Host.arc_c;
  }
  else {
    u3l_log("vere: --arch required\r\n");
    return;
  }
#endif

  if ( _king_make_pace(pac_c) ) {
    u3l_log("vere: unable to make pace (%s) directory in pier\n", pac_c);
    u3_king_bail();
    exit(1);
  }

  {
    c3_i ret_i = asprintf(&dir_c, "%s/.bin/%s", u3_Host.dir_c, pac_c);
    c3_assert( ret_i > 0 );
  }

  //  XX get link option
  //
  if ( u3_king_vere(pac_c, ver_c, arc_c, dir_c, 1) ) {
    u3l_log("vere: upgrade failed\r\n");
    u3_king_bail();
    exit(1);
  }

  c3_free(dir_c);
  u3l_log("vere: upgrade succeeded\r\n");
  //  XX print restart instructions
}

/* _king_read_raw: read (up to) [len_i] from [fid_i] to [buf_y]
*/
static ssize_t
_king_read_raw(c3_i fid_i, c3_y* buf_y, size_t len_i)
{
  ssize_t ret_i;

  do {
    ret_i = read(fid_i, buf_y, len_i);
  }
  while ( (ret_i < 0) && (errno == EINTR) );

  return ret_i;
}

/* _king_read_raw: write [len_i] from [buf_y] to [fid_i].
*/
static c3_i
_king_write_raw(c3_i fid_i, c3_y* buf_y, size_t len_i)
{
  ssize_t ret_i;

  while ( len_i ) {

    do {
      ret_i = write(fid_i, buf_y, len_i);
    }
    while ( (ret_i < 0) && (errno == EINTR) );

    if ( ret_i < 0 ) {
      return -1;
    }
    else {
      len_i -= ret_i;
      buf_y += ret_i;
    }
  }

  return 0;
}

static c3_i
_king_copy_raw(c3_i src_i, c3_i dst_i, c3_y* buf_y, size_t pag_i)
{
  ssize_t red_i;

  do {
    if ( 0 > (red_i = _king_read_raw(src_i, buf_y, pag_i)) ) {
      return -1;
    }

    if ( _king_write_raw(dst_i, buf_y, (size_t)red_i) ) {
      return -1;
    }
  }
  while ( red_i );

  return 0;
}

#if defined(U3_OS_mingw)
int err_win_to_posix(DWORD winerr);
#endif

static c3_i
_king_copy_file(c3_c* src_c, c3_c* dst_c)
{
#if defined(U3_OS_mingw)
  //  XX try FSCTL_DUPLICATE_EXTENTS_TO_FILE
  //
  if ( CopyFileA(src_c, dst_c, TRUE) ) {
    return 0;
  }

  //  XX fallback on any?
  //
  errno = err_win_to_posix(GetLastError());
  return -1;
#elif defined(U3_OS_osx)
  if ( !clonefile(src_c, dst_c, 0) ) {
    return 0;
  }
  //  fallthru to copying bytes on some errors
  //
  else if ( (ENOTSUP != errno) && (EXDEV != errno) ) {
    return -1;
  }
#endif

  {
    c3_i src_i, dst_i, ret_i = 0, err_i = 0;

    if ( -1 == (src_i = open(src_c, O_RDONLY, 0644)) ) {
      err_i = errno;
      ret_i = -1;
      goto done1;
    }

    if ( -1 == (dst_i = open(dst_c, O_RDWR | O_CREAT, 0755)) ) {
      err_i = errno;
      ret_i = -1;
      goto done2;
    }

    //  XX try clone_file_range ?
    //
#if defined(U3_OS_linux)
  #if defined(FICLONE)
    if ( !ioctl(dst_i, FICLONE, src_i) ) {
      ret_i = 0;
      goto done3;
    }
    //  fallthru to copying bytes on some errors
    //
    else if ( (EOPNOTSUPP != errno) && (EXDEV != errno) ) {
      err_i = errno;
      ret_i = -1;
      goto done3;
    }
  #endif

    {
      off_t   off_i = 0;
      ssize_t sen_i;
      size_t  len_i;
      {
        struct stat sat_u;
        if ( -1 == fstat(src_i, &sat_u) ) {
          err_i = errno;
          ret_i = -1;
          goto done3;
        }
        len_i = sat_u.st_size;
      }

      do {
        //  XX fallback on any errors?
        //
        if ( 0 > (sen_i = sendfile64(dst_i, src_i, &off_i, len_i)) ) {
          err_i = errno;
          ret_i = -1;
          goto done3;
        }

        len_i -= off_i;
      }
      while ( len_i );

      ret_i = 0;
      goto done3;
    }
#elif defined(U3_OS_osx)
    if ( !fcopyfile(src_i, dst_i, NULL, COPYFILE_ALL) ) {
      ret_i = 0;
      goto done3;
    }

    //  XX fallback on any errors?
    //
#endif

    {
      size_t pag_i = 1 << 14;;
      c3_y*  buf_y = c3_malloc(pag_i);
      ret_i = _king_copy_raw(src_i, dst_i, buf_y, pag_i);
      err_i = errno;
      c3_free(buf_y);
    }

done3:
    close(dst_i);
done2:
    close(src_i);
done1:
    errno = err_i;
    return ret_i;
  }
}

/* _king_copy_vere(): copy current binary into $pier/.bin (COW if possible)
*/
static c3_i
_king_copy_vere(c3_c* pac_c, c3_c* ver_c, c3_c* arc_c, c3_t lin_t)
{
  c3_c* bin_c;
  c3_i  ret_i;

  if ( _king_make_pace(pac_c) ) {
    return -1; // XX
  }

  ret_i = asprintf(&bin_c, "%s/.bin/%s/vere-v%s-%s",
                           u3_Host.dir_c, pac_c, ver_c, arc_c);
  c3_assert( ret_i > 0 );

  ret_i = _king_copy_file(u3_Host.dem_c, bin_c);

  if ( ret_i ) {
    fprintf(stderr, "vere: copy %s -> %s failed: %s\r\n",
                    bin_c, u3_Host.dem_c, strerror(errno));
    c3_free(bin_c);
    return -1;
  }

  //  XX option
  //
  if ( lin_t ) {
    if ( _king_link_run(bin_c) ) {
      fprintf(stderr, "vere: link %s/%s failed\n", u3_Host.dir_c, U3_BIN_ALIAS);
      c3_free(bin_c);
      return -1;
    }
  }

  c3_free(bin_c);
  return 0;
}

/* u3_king_dock(): copy binary into pier on boot.
*/
void
u3_king_dock(c3_c* pac_c)
{
  c3_c* arc_c = "unknown";

#ifdef U3_OS_ARCH
  arc_c = U3_OS_ARCH;
#endif

  //  XX get link option
  //
  if ( _king_copy_vere(pac_c, URBIT_VERSION, arc_c, 1) ) {
    u3l_log("vere: binary copy failed\r\n");
    u3_king_bail();
    exit(1);
  }
  else {
    //  NB: failure ignored
    //
    _king_init_pace(pac_c);
    u3l_log("vere: binary copy succeeded\r\n");
    //  XX print restart instructions
  }
}

/* _king_done_cb():
*/
static void
_king_done_cb(uv_handle_t* han_u)
{
  if( UV_EBUSY == uv_loop_close(u3L) ) {
    //  XX uncomment to debug
    //
    // fprintf(stderr, "\r\nking: open libuv handles\r\n");
    // uv_print_all_handles(u3L, stderr);
    // fprintf(stderr, "\r\nking: force shutdown\r\n");

    uv_stop(u3L);
  }
}

/* u3_king_done(): all piers closed. s/b callback
*/
void
u3_king_done(void)
{
  uv_handle_t* han_u = (uv_handle_t*)&u3K.tim_u;

  if ( u3_Host.xit_i ) {
    if ( c3y == u3_Host.nex_o ) {
      u3l_log("vere: upgrade failed\r\n");
    }
    else if ( c3y == u3_Host.pep_o ) {
      u3l_log("vere: prep for upgrade failed\r\n");
    }
  }
  else {
    //  get next binary
    //
    if ( c3y == u3_Host.nex_o ) {
      c3_c* pac_c;
      c3_c* ver_c;

      //  hack to ensure we only try once
      //
      u3_Host.nex_o = c3n;

      pac_c = _king_get_pace();

      switch ( u3_king_next(pac_c, &ver_c) ) {
        case -2: {
          u3l_log("vere: unable to check for next version\n");
        } break;

        case -1: {
          u3l_log("vere: up to date\n");
        } break;

        case 0: {
          u3l_log("vere: next (%%%s): %s\n", pac_c, ver_c);
          _king_do_upgrade(pac_c, ver_c);
          c3_free(ver_c);
        } break;

        default: c3_assert(0);
      }

      c3_free(pac_c);
    }
    else if ( c3y == u3_Host.pep_o ) {
      u3l_log("vere: ready for upgrade\n");
    }

    //  copy binary into pier on boot
    //
    if (  (c3y == u3_Host.ops_u.nuu)
       && (c3y == u3_Host.ops_u.doc) )
    {
      //  hack to ensure we only try once
      //
      u3_Host.ops_u.nuu = c3n;
      u3_king_dock(U3_VERE_PACE);
    }
  }

  //  XX hack, if pier's are still linked, we're not actually done
  //
  if ( !u3K.pir_u && !uv_is_closing(han_u) ) {
    uv_close((uv_handle_t*)&u3K.tim_u, _king_done_cb);
    _king_sign_close();

    u3_term_log_exit();
    fflush(stdout);
  }

  //  XX remove move
  //
  exit(u3_Host.xit_i);
}

/* u3_king_exit(): shutdown gracefully
*/
void
u3_king_exit(void)
{
  _king_forall(u3_pier_exit);
}

/* u3_king_bail(): immediately shutdown.
*/
void
u3_king_bail(void)
{
  u3_Host.xit_i = 1;
  _king_forall_unlink(u3_pier_bail);
  _king_loop_exit();
  u3_king_done();
  exit(u3_Host.xit_i);
}

/* u3_king_grab(): gc the daemon
*/
void
u3_king_grab(void* vod_p)
{
  c3_w tot_w = 0;
  FILE* fil_u;

  c3_assert( u3R == &(u3H->rod_u) );

#ifdef U3_MEMORY_LOG
  {
    //  XX date will not match up with that of the worker
    //
    u3_noun wen = u3dc("scot", c3__da, u3k(u3A->now));
    c3_c* wen_c = u3r_string(wen);

    c3_c nam_c[2048];
    snprintf(nam_c, 2048, "%s/.urb/put/mass", u3_king_stub()->pax_c);

    struct stat st;
    if ( -1 == stat(nam_c, &st) ) {
      c3_mkdir(nam_c, 0700);
    }

    c3_c man_c[2048];
    snprintf(man_c, 2048, "%s/%s-daemon.txt", nam_c, wen_c);

    fil_u = c3_fopen(man_c, "w");
    fprintf(fil_u, "%s\r\n", wen_c);

    c3_free(wen_c);
    u3z(wen);
  }
#else
  {
    fil_u = u3_term_io_hija();
    fprintf(fil_u, "measuring daemon:\r\n");
  }
#endif

  tot_w += u3m_mark(fil_u);
  tot_w += u3_pier_mark(fil_u);

  u3a_print_memory(fil_u, "total marked", tot_w);
  u3a_print_memory(fil_u, "sweep", u3a_sweep());

#ifdef U3_MEMORY_LOG
  {
    fclose(fil_u);
  }
#else
  {
    u3_term_io_loja(0);
  }
#endif
}
