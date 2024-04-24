/// @file

#define U3_GLOBAL
#define C3_GLOBAL
#include "noun.h"
#include "events.h" // XX remove, see full replay in _cw_play()
#include "ivory.h"
#include "ur.h"
#include "platform/rsignal.h"
#include "vere.h"
#include "sigsegv.h"
#include "openssl/conf.h"
#include "openssl/engine.h"
#include "openssl/err.h"
#include "openssl/ssl.h"
#include "h2o.h"
#include "curl/curl.h"
#include "db/lmdb.h"
#include "getopt.h"
#include "libgen.h"

#include "ca_bundle.h"
#include "pace.h"
#include "version.h"
#include "whereami.h"
#include "mars.h"

//  serf module state
//
static u3_serf        u3V;             //  one serf per process
static u3_moat      inn_u;             //  input stream
static u3_mojo      out_u;             //  output stream
static u3_cue_xeno* sil_u;             //  cue handle

#undef SERF_TRACE_JAM
#undef SERF_TRACE_CUE

/* Require unsigned char
 */
//STATIC_ASSERT(( 0 == CHAR_MIN && UCHAR_MAX == CHAR_MAX ),
              //"unsigned char required");

/* _main_self_path(): get binary self-path.
*/
static void
_main_self_path(void)
{
  c3_c* pat_c;
  c3_i  len_i, pat_i;

  if ( 0 < (len_i = wai_getExecutablePath(NULL, 0, &pat_i)) ) {
    pat_c = c3_malloc( 1 + len_i );
    wai_getExecutablePath(pat_c, len_i, &pat_i);
    pat_c[len_i] = 0;

    u3_Host.dem_c = pat_c;
  }
  else {
    fprintf(stderr, "unable to get binary self path\r\n");
    exit(1);

    //  XX continue?
    //
    // u3_Host.dem_c = strdup(bin_c);
  }
}

/* _main_readw(): parse a word from a string.
*/
static c3_o
_main_readw(const c3_c* str_c, c3_w max_w, c3_w* out_w)
{
  c3_c* end_c;
  c3_w  par_w = strtoul(str_c, &end_c, 0);

  if ( *str_c != '\0' && *end_c == '\0' && par_w < max_w ) {
    *out_w = par_w;
    return c3y;
  }
  else return c3n;
}

/* _main_readw_loom(): parse loom pointer bit size from a string.
*/
static c3_i
_main_readw_loom(const c3_c* arg_c, c3_y* out_y)
{
  c3_w lom_w;
  c3_o res_o = _main_readw(optarg, u3a_bits_max + 1, &lom_w);
  if ( res_o == c3n || (lom_w < 20) ) {
    fprintf(stderr, "error: --%s must be >= 20 and <= %zu\r\n", arg_c, u3a_bits_max);
    return -1;
  }
  *out_y = lom_w;
  return 0;
}

/* _main_presig(): prefix optional sig.
*/
c3_c*
_main_presig(c3_c* txt_c)
{
  c3_c* new_c = c3_malloc(2 + strlen(txt_c));

  if ( '~' == *txt_c ) {
    strcpy(new_c, txt_c);
  } else {
    new_c[0] = '~';
    strcpy(new_c + 1, txt_c);
  }
  return new_c;
}

/* _main_repath(): canonicalize path, using dirname if needed.
*/
c3_c*
_main_repath(c3_c* pax_c)
{
  c3_c* rel_c;
  c3_c* fas_c;
  c3_c* dir_c;
  c3_w  len_w;
  c3_i  wit_i;

  u3_assert(pax_c);
  if ( 0 != (rel_c = realpath(pax_c, 0)) ) {
    return rel_c;
  }
  fas_c = strrchr(pax_c, '/');
  if ( !fas_c ) {
    c3_c rec_c[2048];

    wit_i = snprintf(rec_c, sizeof(rec_c), "./%s", pax_c);
    u3_assert(sizeof(rec_c) > wit_i);
    return _main_repath(rec_c);
  }
  u3_assert(u3_unix_cane(fas_c + 1));
  *fas_c = 0;
  dir_c = realpath(pax_c, 0);
  *fas_c = '/';
  if ( 0 == dir_c ) {
    return 0;
  }
  len_w = strlen(dir_c) + strlen(fas_c) + 1;
  rel_c = c3_malloc(len_w);
  wit_i = snprintf(rel_c, len_w, "%s%s", dir_c, fas_c);
  u3_assert(len_w == wit_i + 1);
  c3_free(dir_c);
  return rel_c;
}

/* _main_init(): initialize globals
*/
static void
_main_init(void)
{
  u3_Host.nex_o = c3n;
  u3_Host.pep_o = c3n;

  u3_Host.ops_u.abo = c3n;
  u3_Host.ops_u.dem = c3n;
  u3_Host.ops_u.dry = c3n;
  u3_Host.ops_u.gab = c3n;
  u3_Host.ops_u.git = c3n;

  //  always disable hashboard
  //  XX temporary, remove once hashes are added
  //
  u3_Host.ops_u.has = c3y;

  u3_Host.ops_u.map = c3y;
  u3_Host.ops_u.net = c3y;
  u3_Host.ops_u.lit = c3n;
  u3_Host.ops_u.nuu = c3n;
  u3_Host.ops_u.pro = c3n;
  u3_Host.ops_u.qui = c3n;
  u3_Host.ops_u.rep = c3n;
  u3_Host.ops_u.eph = c3n;
  u3_Host.ops_u.tos = c3n;
  u3_Host.ops_u.beb = c3n;
  u3_Host.ops_u.tem = c3n;
  u3_Host.ops_u.tex = c3n;
  u3_Host.ops_u.tra = c3n;
  u3_Host.ops_u.veb = c3n;
  u3_Host.ops_u.puf_c = "jam";
  u3_Host.ops_u.hap_w = 50000;
  u3C.hap_w = u3_Host.ops_u.hap_w;
  u3_Host.ops_u.per_w = 50000;
  u3C.per_w = u3_Host.ops_u.per_w;
  u3_Host.ops_u.kno_w = DefaultKernel;

  u3_Host.ops_u.sap_w = 120;    /* aka 2 minutes */
  u3_Host.ops_u.lut_y = 31;     /* aka 2G */
  u3_Host.ops_u.lom_y = 31;

  u3C.eph_c = 0;
  u3C.tos_w = 0;
}

/* _main_pier_run(): get pier from binary path (argv[0]), if appropriate
*/
static c3_c*
_main_pier_run(c3_c* bin_c)
{
  c3_c* dir_c = 0;
  c3_w  bin_w = strlen(bin_c);
  c3_w  len_w = strlen(U3_BIN_ALIAS);

  //  no args, argv[0] == $pier/.run
  //
  if (  (len_w <= bin_w)
     && (0 == strcmp(bin_c + (bin_w - len_w), U3_BIN_ALIAS)) )
  {
    bin_c = strdup(bin_c); // dirname can modify
    dir_c = _main_repath(dirname(bin_c));
    c3_free(bin_c);
  }

  return dir_c;
}

/* _main_add_prop(): add a boot prop to u3_Host.ops_u.vex_u.
*/
u3_even*
_main_add_prop(c3_i kin_i, c3_c* loc_c)
{
  u3_even* nex_u = c3_calloc(sizeof(*nex_u));
  nex_u->kin_i = kin_i;
  nex_u->loc_c = loc_c;
  nex_u->pre_u = u3_Host.ops_u.vex_u;
  u3_Host.ops_u.vex_u = nex_u;
  return nex_u;
}

/* _main_getopt(): extract option map from command line.
*/
static u3_noun
_main_getopt(c3_i argc, c3_c** argv)
{
  c3_i ch_i, lid_i;
  c3_w arg_w;
  c3_o want_creat_o = c3n;

  static struct option lop_u[] = {
    { "arvo",                required_argument, NULL, 'A' },
    { "abort",               no_argument,       NULL, 'a' },
    { "bootstrap",           required_argument, NULL, 'B' },
    { "http-ip",             required_argument, NULL, 'b' },
    { "memo-cache-limit",    required_argument, NULL, 'C' },
    { "pier",                required_argument, NULL, 'c' },
    { "replay",              no_argument,       NULL, 'D' },
    { "daemon",              no_argument,       NULL, 'd' },
    { "ethereum",            required_argument, NULL, 'e' },
    { "fake",                required_argument, NULL, 'F' },
    { "key-string",          required_argument, NULL, 'G' },
    { "gc",                  no_argument,       NULL, 'g' },
    { "dns-root",            required_argument, NULL, 'H' },
    { "inject",              required_argument, NULL, 'I' },
    { "import",              required_argument, NULL, 'i' },
    { "ivory-pill",          required_argument, NULL, 'J' },
    { "json-trace",          no_argument,       NULL, 'j' },
    { "kernel-stage",        required_argument, NULL, 'K' },
    { "key-file",            required_argument, NULL, 'k' },
    { "loom",                required_argument, NULL, c3__loom },
    { "local",               no_argument,       NULL, 'L' },
    { "lite-boot",           no_argument,       NULL, 'l' },
    { "keep-cache-limit",    required_argument, NULL, 'M' },
    { "replay-to",           required_argument, NULL, 'n' },
    { "profile",             no_argument,       NULL, 'P' },
    { "ames-port",           required_argument, NULL, 'p' },
    { "http-port",           required_argument, NULL, c3__http },
    { "https-port",          required_argument, NULL, c3__htls },
    { "snap-time",           required_argument, NULL, c3__snap },
    { "no-conn",             no_argument,       NULL, c3__noco },
    { "no-dock",             no_argument,       NULL, c3__nodo },
    { "quiet",               no_argument,       NULL, 'q' },
    { "versions",            no_argument,       NULL, 'R' },
    { "replay-from",         required_argument, NULL, 'r' },
    { "skip-battery-hashes", no_argument,       NULL, 'S' },
    { "autoselect-pill",     no_argument,       NULL, 's' },
    { "no-tty",              no_argument,       NULL, 't' },
    { "bootstrap-url",       required_argument, NULL, 'u' },
    { "verbose",             no_argument,       NULL, 'v' },
    { "name",                required_argument, NULL, 'w' },
    { "scry",                required_argument, NULL, 'X' },
    { "exit",                no_argument,       NULL, 'x' },
    { "scry-into",           required_argument, NULL, 'Y' },
    { "scry-format",         required_argument, NULL, 'Z' },
    //
    { "prop-file",           required_argument, NULL, 1 },
    { "prop-url",            required_argument, NULL, 2 },
    { "prop-name",           required_argument, NULL, 3 },
    //
    { "urth-loom",           required_argument, NULL, 5 },
    { "no-demand",           no_argument,       NULL, 6 },
    { "swap",                no_argument,       NULL, 7 },
    { "swap-to",             required_argument, NULL, 8 },
    { "toss",                required_argument, NULL, 9 },
    { "behn-allow-blocked",  no_argument,       NULL, 10 },
    //
    { NULL, 0, NULL, 0 },
  };

  while ( -1 != (ch_i=getopt_long(argc, argv,
                 "A:B:C:DF:G:H:I:J:K:LM:PRSX:Y:Z:ab:c:de:gi:jk:ln:p:qr:stu:vw:x",
                 lop_u, &lid_i)) )
  {
    switch ( ch_i ) {
      case 1: case 2: case 3: {  //  prop-*
        _main_add_prop(ch_i, strdup(optarg));
        break;
      }
      case 5: {  //  urth-loom
        if (_main_readw_loom("urth-loom", &u3_Host.ops_u.lut_y)) {
          return c3n;
        }
        break;
      }
      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        break;
      }
      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        break;
      }
      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.eph_c = strdup(optarg);
        break;
      }
      case 9: {  //  toss
        u3_Host.ops_u.tos = c3y;
        if ( 1 != sscanf(optarg, "%" SCNu32, &u3C.tos_w) ) {
          return c3n;
        }
        break;
      }
      case 10: { //  behn-allow-blocked
        u3_Host.ops_u.beb = c3y;
        break;
      }
      //  special args
      //
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          return c3n;
        }
        break;
      }
      case c3__http: {
        if ( c3n == _main_readw(optarg, 65536, &arg_w) ) {
          return c3n;
        } else u3_Host.ops_u.per_s = arg_w;
        break;
      }
      case c3__htls: {
        if ( c3n == _main_readw(optarg, 65536, &arg_w) ) {
          return c3n;
        } else u3_Host.ops_u.pes_s = arg_w;
        break;
      }
      case c3__noco: {
        u3_Host.ops_u.con = c3n;
        break;
      }
      case c3__nodo: {
        u3_Host.ops_u.doc = c3n;
        break;
      }
      case c3__snap: {
        if ( c3n == _main_readw(optarg, 65536, &arg_w) ) {
          return c3n;
        } else {
          u3_Host.ops_u.sap_w = arg_w * 60;
          if ( 0 == u3_Host.ops_u.sap_w )
            return c3n;
        }
        break;
      }
      //  opts with args
      //
      case 'A': {
        u3_Host.ops_u.arv_c = _main_repath(optarg);
        break;
      }
      case 'B': {
        u3_Host.ops_u.pil_c = _main_repath(optarg);
        break;
      }
      case 'b': {
        u3_Host.ops_u.bin_c = strdup(optarg);
        break;
      }
      case 'C': {
        if ( c3n == _main_readw(optarg, 1000000000, &u3_Host.ops_u.hap_w) ) {
          return c3n;
        }
        u3C.hap_w = u3_Host.ops_u.hap_w;
        break;
      }
      case 'c': {
        u3_Host.dir_c     = _main_repath(optarg);
        u3_Host.ops_u.nuu = c3y;
        break;
      }
      case 'e': {
        u3_Host.ops_u.eth_c = strdup(optarg);
        break;
      }
      case 'F': {
        u3_Host.ops_u.fak_c = _main_presig(optarg);
        u3_Host.ops_u.net   = c3n;
        u3_Host.ops_u.nuu   = c3y;
        break;
      }
      case 'G': {
        u3_Host.ops_u.gen_c = strdup(optarg);
        break;
      }
      case 'H': {
        u3_Host.ops_u.dns_c = strdup(optarg);
        break;
      }
      case 'I': {
        u3_Host.ops_u.jin_c = _main_repath(optarg);
        break;
      }
      case 'i': {
        u3_Host.ops_u.imp_c = _main_repath(optarg);
        break;
      }
      case 'J': {
        u3_Host.ops_u.lit_c = _main_repath(optarg);
        break;
      }
      case 'K': {
        if ( c3n == _main_readw(optarg, 256, &u3_Host.ops_u.kno_w) ) {
          return c3n;
        }
        break;
      }
      case 'k': {
        u3_Host.ops_u.key_c = _main_repath(optarg);
        break;
      }
      case 'M': {
        if ( c3n == _main_readw(optarg, 1000000000, &u3_Host.ops_u.per_w) ) {
          return c3n;
        }
        u3C.per_w = u3_Host.ops_u.per_w;
        break;
      }
      case 'n': {
        u3_Host.ops_u.til_c = strdup(optarg);
        break;
      }
      case 'p': {
        if ( c3n == _main_readw(optarg, 65536, &arg_w) ) {
          return c3n;
        } else u3_Host.ops_u.por_s = arg_w;
        break;
      }
      case 'R': {
        u3_Host.ops_u.rep = c3y;
        return c3y;
      }
      case 'r': {
        u3_Host.ops_u.roc_c = strdup(optarg);
        break;
      }
      case 'u': {
        u3_Host.ops_u.url_c = strdup(optarg);
        break;
      }
      case 'w': {
        u3_Host.ops_u.who_c = _main_presig(optarg);
        u3_Host.ops_u.nuu = c3y;
        break;
      }
      case 'X': {
        u3_Host.ops_u.pek_c = strdup(optarg);
        break;
      }
      case 'x': {
        u3_Host.ops_u.tex = c3y;
        break;
      }
      case 'Y': {
        u3_Host.ops_u.puk_c = strdup(optarg);
        break;
      }
      case 'Z': {
        u3_Host.ops_u.puf_c = strdup(optarg);
        break;
      }
      //  opts without args
      //
      case 'a': { u3_Host.ops_u.abo = c3y; break; }
      case 'D': { u3_Host.ops_u.dry = c3y; break; }
      case 'd': { u3_Host.ops_u.dem = c3y; break; }
      case 'g': { u3_Host.ops_u.gab = c3y; break; }
      case 'j': { u3_Host.ops_u.tra = c3y; break; }
      case 'L': { u3_Host.ops_u.net = c3n; break; }
      case 'l': { u3_Host.ops_u.lit = c3y; break; }
      case 'P': { u3_Host.ops_u.pro = c3y; break; }
      case 'q': { u3_Host.ops_u.qui = c3y; break; }
      case 's': { u3_Host.ops_u.git = c3y; break; }
      case 'S': { u3_Host.ops_u.has = c3y; break; }
      case 't': { u3_Host.ops_u.tem = c3y; break; }
      case 'v': { u3_Host.ops_u.veb = c3y; break; }
      //  unknown opt
      //
      case '?': default: {
        return c3n;
      }
    }
  }

#if !defined(U3_OS_PROF)
  if (u3_Host.ops_u.pro == c3y) {
    fprintf(stderr, "profiling isn't yet supported on your OS\r\n");
    return c3n;
  }
#endif

  if ( 0 != u3_Host.ops_u.fak_c ) {
    if ( 28 < strlen(u3_Host.ops_u.fak_c) ) {
      fprintf(stderr, "fake comets are forbidden\r\n");
      return c3n;
    }
    if ( 0 != u3_Host.ops_u.who_c ) {
      fprintf(stderr, "-F and -w cannot be used together\r\n");
      return c3n;
    }

    u3_Host.ops_u.who_c = strdup(u3_Host.ops_u.fak_c);
    u3_Host.ops_u.has = c3y;  /* no battery hashing on fake ships. */
    u3_Host.ops_u.net = c3n;  /* no networking on fake ships. */
  }

  if ( argc == optind && u3_Host.ops_u.nuu != c3y ) {
    if ( !(u3_Host.dir_c = _main_pier_run(argv[0])) ) {
      //  no trailing arg, argv[0] != $pier/.run, not making new pier: invalid command
      fprintf(stderr, "no pier provided\n");
      return c3n;
    }
  }
  else if ( argc != optind ) {
    if ( u3_Host.ops_u.nuu == c3y || argc > (optind + 1) ) {
      //  path with new pier or multiple paths: invalid command
      fprintf(stderr, "too many arguments\n");
      return c3n;
    }
    u3_Host.dir_c = _main_repath(argv[optind]);
  }

  if ( 0 == u3_Host.dir_c ) {
    u3_Host.dir_c = strdup(1 + u3_Host.ops_u.who_c);
  }

  //  daemon mode (-d) implies disabling terminal assumptions (-t)
  //
  if ( c3y == u3_Host.ops_u.dem ) {
    u3_Host.ops_u.tem = c3y;
  }

  {
    struct stat s;
    //  catch invalid boot
    if ( 0 != stat(u3_Host.dir_c, &s) ) {
      if ( u3_Host.ops_u.nuu != c3y ) {
        fprintf(stderr, "couldn't find pier %s\n", u3_Host.dir_c);
        exit(1);
      }
    }
    //  catch invalid boot of existing pier
    else {
      if ( u3_Host.ops_u.nuu == c3y ) {
        fprintf(stderr, "tried to create pier %s but it already exists\n", u3_Host.dir_c);
        fprintf(stderr, "normal usage: %s %s\n", argv[0], u3_Host.dir_c);
        exit(1);
      }
      else if ( 0 != access(u3_Host.dir_c, W_OK) ) {
        fprintf(stderr, "urbit: write permissions are required for %s\n", u3_Host.dir_c);
        exit(1);
      }
    }
  }

  if ( u3_Host.ops_u.nuu != c3y && u3_Host.ops_u.pil_c != 0 ) {
    fprintf(stderr, "-B only makes sense when creating a new ship\n");
    return c3n;
  }

  if ( u3_Host.ops_u.nuu != c3y && u3_Host.ops_u.gen_c != 0 ) {
    fprintf(stderr, "-G only makes sense when creating a new ship\n");
    return c3n;
  }

  if ( u3_Host.ops_u.nuu != c3y && u3_Host.ops_u.dns_c != 0 ) {
    fprintf(stderr, "-H only makes sense when creating a new ship\n");
    return c3n;
  }

  if ( u3_Host.ops_u.nuu != c3y && u3_Host.ops_u.key_c != 0 ) {
    fprintf(stderr, "-k only makes sense when creating a new ship\n");
    return c3n;
  }

  if ( u3_Host.ops_u.nuu != c3y && u3_Host.ops_u.url_c != 0 ) {
    fprintf(stderr, "-u only makes sense when creating a new ship\n");
    return c3n;
  }

  if ( u3_Host.ops_u.url_c != 0 && u3_Host.ops_u.pil_c != 0 ) {
    fprintf(stderr, "-B and -u cannot be used together\n");
    return c3n;
  }
  else if ( u3_Host.ops_u.nuu == c3y
           && u3_Host.ops_u.url_c == 0
           && u3_Host.ops_u.git == c3n ) {

    c3_c version_c[strlen(URBIT_VERSION) + 1];
    strcpy(version_c, URBIT_VERSION);
    c3_c* hyphen_c = strchr(version_c, '-');
    // URBIT_VERSION has the form {version}-{commit_sha} when built on
    // non-"live" channels, which means we need to strip off the trailing commit
    // SHA in those cases.
    if ( hyphen_c ) {
      *hyphen_c = '\0';
    }
    //TODO  use brass pill from b.u.org/props/etc eventually
    c3_i res_i = asprintf(&u3_Host.ops_u.url_c,
                          "https://bootstrap.urbit.org/urbit-v%s.pill",
                          version_c);
    if ( res_i < 0 ) {
      fprintf(stderr, "failed to construct pill URL\n");
      return c3n;
    }
  }
  else if ( u3_Host.ops_u.nuu == c3y
           && u3_Host.ops_u.url_c == 0
           && u3_Host.ops_u.arv_c == 0 ) {

    // implicitly: u3_Host.ops_u.git == c3y
    fprintf(stderr, "-s only makes sense with -A\n");
    return c3n;
  }

  if ( u3_Host.ops_u.pil_c != 0 ) {
    struct stat s;
    if ( stat(u3_Host.ops_u.pil_c, &s) != 0 ) {
      fprintf(stderr, "pill %s not found\n", u3_Host.ops_u.pil_c);
      return c3n;
    }
  }

  if ( u3_Host.ops_u.vex_u != 0 ) {
    struct stat s;
    u3_even* vex_u = u3_Host.ops_u.vex_u;
    while ( vex_u != 0 ) {
      if ( vex_u->kin_i == 1 && stat(vex_u->loc_c, &s) != 0 ) {
        fprintf(stderr, "events file %s not found\n", vex_u->loc_c);
        return c3n;
      }
      vex_u = vex_u->pre_u;
    }
  }

  struct sockaddr_in t;
  if ( u3_Host.ops_u.bin_c != 0 && inet_pton(AF_INET, u3_Host.ops_u.bin_c, &t.sin_addr) == 0 ) {
    fprintf(stderr, "-b invalid IP address\n");
    return c3n;
  }

  if ( u3_Host.ops_u.key_c != 0 ) {
    struct stat s;
    if ( stat(u3_Host.ops_u.key_c, &s) != 0 ) {
      fprintf(stderr, "keyfile %s not found\n", u3_Host.ops_u.key_c);
      return c3n;
    }
  }

  //TODO  split up "default distribution" packages eventually
  // //  if we're not in lite mode, include the default props
  // //
  // if ( u3_Host.ops_u.lit == c3n ) {
  //   _main_add_prop(3, "landscape");
  //   _main_add_prop(3, "webterm");
  //   _main_add_prop(3, "groups");
  // }

  return c3y;
}

/* _cert_store: decoded CA certificates
 */
static STACK_OF(X509_INFO)* _cert_store;

/* _setup_cert_store(): decodes embedded CA certificates
 */
static void
_setup_cert_store()
{
  BIO* cbio = BIO_new_mem_buf(include_ca_bundle_crt, include_ca_bundle_crt_len);
  if ( !cbio || !(_cert_store = PEM_X509_INFO_read_bio(cbio, NULL, NULL, NULL)) ) {
    u3l_log("boot: failed to decode embedded CA certificates");
    exit(1);
  }

  BIO_free(cbio);
}

/* _setup_ssl_x509(): adds embedded CA certificates to a X509_STORE
 */
static void
_setup_ssl_x509(void* arg)
{
  X509_STORE* cts = arg;
  int i;
  for ( i = 0; i < sk_X509_INFO_num(_cert_store); i++ ) {
    X509_INFO *itmp = sk_X509_INFO_value(_cert_store, i);
    if(itmp->x509) {
      X509_STORE_add_cert(cts, itmp->x509);
    }
    if(itmp->crl) {
      X509_STORE_add_crl(cts, itmp->crl);
    }
  }
}

/* _curl_ssl_ctx_cb(): curl SSL context callback
 */
static CURLcode
_curl_ssl_ctx_cb(CURL* curl, SSL_CTX* sslctx, void* param)
{
  X509_STORE* cts = SSL_CTX_get_cert_store(sslctx);
  if (!cts || !_cert_store)
    return CURLE_ABORTED_BY_CALLBACK;

  _setup_ssl_x509(cts);
  return CURLE_OK;
}

/* _setup_ssl_curl(): adds embedded CA certificates to a curl context
 */
static void
_setup_ssl_curl(void* arg)
{
  CURL* curl = arg;
  curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
  curl_easy_setopt(curl, CURLOPT_CAPATH, NULL);
  curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, _curl_ssl_ctx_cb);
}

/* _cw_usage(): print utility usage.
*/
static void
_cw_usage(c3_c* bin_c)
{
  c3_c *use_c[] = {
    "utilities:\n",
    "  %s eval                      evaluate hoon from stdin:\n",
    "  %s cram %.*s              jam state:\n",
    "  %s dock %.*s              copy binary:\n",
    "  %s grab %.*s              measure memory usage:\n",
    "  %s info %.*s              print pier info:\n",
    "  %s meld %.*s              deduplicate snapshot:\n",
    "  %s pack %.*s              defragment snapshot:\n",
    "  %s play %.*s              recompute events:\n",
    "  %s prep %.*s              prepare for upgrade:\n",
    "  %s next %.*s              request upgrade:\n",
    "  %s queu %.*s<at-event>    cue state:\n",
    "  %s chop %.*s              truncate event log:\n",
    "  %s roll %.*s              rollover to new epoch:\n",
    "  %s vere ARGS <output dir>    download binary:\n",
    "\n  run as a 'serf':\n",
    "    %s serf <pier> <key> <flags> <cache-size> <at-event>"
    "\n",
    0
  };

  c3_c* d = _main_pier_run(bin_c);
  c3_i  i;

  for ( i=0; use_c[i]; i++ ) {
    fprintf(stderr, use_c[i], bin_c, d ? 0 : 7, "<pier> ");
  }

  c3_free(d);
}

/* u3_ve_usage(): print usage and exit.
*/
static void
u3_ve_usage(c3_i argc, c3_c** argv)
{
  c3_c *use_c[] = {
    "Urbit: a personal server operating function\n",
    "https://urbit.org\n",
    "Version " URBIT_VERSION "\n",
    "\n",
    "Usage: %s [options...] ship_name\n",
    "where ship_name is a @p phonetic representation of an urbit address\n",
    "without the leading '~', and options is some subset of the following:\n",
    "\n",
    "-A, --arvo DIR                Use dir for initial clay sync\n",
    "-a, --abort                   Abort aggressively\n",
    "-B, --bootstrap PILL          Bootstrap from this pill\n",
    "-b, --http-ip IP              Bind HTTP server to this IP address\n",
    "-C, --memo-cache-limit LIMIT  Set memo cache max size; 0 means uncapped\n",
    "-c, --pier PIER               Create a new urbit in <pier>/\n",
    "-D, --replay                  Recompute from events\n",
    "-d, --daemon                  Daemon mode; implies -t\n",
    "-e, --ethereum URL            Ethereum gateway\n",
    "-F, --fake SHIP               Boot fake urbit; also disables networking\n",
    "-G, --key-string STRING       Private key string (@uw, see also -k)\n"
    "-g, --gc                      Set GC flag\n",
    "-I, --inject FILE             Inject event from jamfile\n",
    "-i, --import FILE             Import pier state from jamfile\n",
    "-J, --ivory-pill PILL         Use custom ivory pill\n",
    "-j, --json-trace              Create json trace file in .urb/put/trace\n",
    "-K, --kernel-stage STAGE      Start at Hoon kernel version stage\n",
    "-k, --key-file KEYS           Private key file (see also -G)\n",
    "-L, --local                   Local networking only\n",
    "    --loom                    Set loom to binary exponent (31 == 2GB)\n"
    "-l, --lite-boot               Most-minimal startup\n",
    "-M, --keep-cache-limit LIMIT  Set persistent memo cache max size; 0 means default\n",
    "-n, --replay-to NUMBER        Replay up to event\n",
    "-P, --profile                 Profiling\n",
    "-p, --ames-port PORT          Set the ames port to bind to\n",
    "    --http-port PORT          Set the http port to bind to\n",
    "    --https-port PORT         Set the https port to bind to\n",
    "    --snap-time TIME          Set the snapshotting rate in minutes (> 0)\n",
    "-q, --quiet                   Quiet\n",
    "-R, --versions                Report urbit build info\n",
    "-r, --replay-from NUMBER      Load snapshot from event\n",
    "-S, --skip-battery-hashes     Disable battery hashing\n",
    // XX find a way to re-enable
    // "-s, --autoselect-pill      Pill URL from arvo git hash\n",
    "-t, --no-tty                  Disable terminal/tty assumptions\n",
    "-u, --bootstrap-url URL       URL from which to download pill\n",
    "-v, --verbose                 Verbose\n",
    "-w, --name NAME               Initial boot as ~name (with ticket)\n",
    "-X, --scry PATH               Scry, write to file, then exit\n",
    "-x, --exit                    Exit immediately\n",
    "-Y, --scry-into FILE          Optional name of file (for -X)\n",
    "-Z, --scry-format FORMAT      Optional file format ('jam', or aura, for -X)\n",
    "    --no-demand               Skip demand paging\n"
    "    --no-conn                 Do not run control plane\n",
    "    --no-dock                 Skip binary \"docking\" on boot\n",
    "    --swap                    Use an explicit ephemeral (swap-like) file\n",
    "    --swap-to FILE            Specify ephemeral file location\n",
    "    --prop-file FILE          Add a prop into the boot sequence\n"
    "    --prop-url URL            Download a prop into the boot sequence\n",
    "    --prop-name NAME          Download a prop from bootstrap.urbit.org\n",
    "\n",
    "Development Usage:\n",
    "   To create a development ship, use a fakezod:\n",
    "   %s -F zod -A /path/to/arvo/folder -B /path/to/pill -c zod\n",
    "\n",
    "   For more information about developing on urbit, see:\n",
    "   https://github.com/urbit/urbit/blob/master/CONTRIBUTING.md\n",
    "\n",
    "Simple Usage: \n",
    "   %s -c <my-comet> to create a comet (anonymous urbit)\n",
    "   %s -w <my-planet> -k <my-key-file> if you own a planet\n",
    "   %s <my-planet or my-comet> to restart an existing urbit\n",
    0
  };

  c3_i i;
  for ( i=0; use_c[i]; i++ ) {
    fprintf(stderr, use_c[i], argv[0]);
  }
  _cw_usage(argv[0]);
  exit(1);
}

#if 0
/* u3_ve_panic(): panic and exit.
*/
static void
u3_ve_panic(c3_i argc, c3_c** argv)
{
  fprintf(stderr, "%s: gross system failure\n", argv[0]);
  exit(1);
}
#endif

static void
report(void)
{
  printf("urbit %s\n", URBIT_VERSION);
  printf("gmp: %s\n", gmp_version);
  printf("sigsegv: %d.%d\n",
         (libsigsegv_version >> 8) & 0xff,
         libsigsegv_version & 0xff);
  printf("openssl: %s\n", SSLeay_version(SSLEAY_VERSION));
  printf("libuv: %s\n", uv_version_string());
  printf("libh2o: %d.%d.%d\n",
         H2O_LIBRARY_VERSION_MAJOR,
         H2O_LIBRARY_VERSION_MINOR,
         H2O_LIBRARY_VERSION_PATCH);
  printf("lmdb: %d.%d.%d\n",
         MDB_VERSION_MAJOR,
         MDB_VERSION_MINOR,
         MDB_VERSION_PATCH);
  printf("curl: %d.%d.%d\n",
         LIBCURL_VERSION_MAJOR,
         LIBCURL_VERSION_MINOR,
         LIBCURL_VERSION_PATCH);
}

/* _stop_exit(): exit immediately.
*/
static void
_stop_exit(c3_i int_i)
{
  //  explicit fprintf to avoid allocation in u3l_log
  //
  fprintf(stderr, "\r\n[received keyboard stop signal, exiting]\r\n");
  u3_king_bail();
}

/* _stop_on_boot_completed_cb(): exit gracefully after boot is complete
*/
static void
_stop_on_boot_completed_cb()
{
  u3_king_exit();
}

/* _cw_serf_fail(): failure stub.
*/
static void
_cw_serf_fail(void* ptr_v, ssize_t err_i, const c3_c* err_c)
{
  if ( UV_EOF == err_i ) {
    fprintf(stderr, "serf: pier unexpectedly shut down\r\n");
  }
  else {
    fprintf(stderr, "serf: pier error: %s\r\n", err_c);
  }

  exit(1);
}

/* _cw_king_fail(): local failure stub.
*/
static void
_cw_king_fail(void* ptr_v, ssize_t err_i, const c3_c* err_c)
{
  fprintf(stderr, "king: eval error: %s\r\n", err_c);
  exit(1);
}

/* _cw_serf_send(): send plea back to daemon.
*/
static void
_cw_serf_send(u3_noun pel)
{
  c3_d  len_d;
  c3_y* byt_y;

#ifdef SERF_TRACE_JAM
  u3t_event_trace("serf ipc jam", 'B');
#endif

  u3s_jam_xeno(pel, &len_d, &byt_y);

#ifdef SERF_TRACE_JAM
  u3t_event_trace("serf ipc jam", 'E');
#endif

  u3_newt_send(&out_u, len_d, byt_y);
  u3z(pel);
}

/* _cw_serf_send_slog(): send hint output (hod is [priority tank]).
*/
static void
_cw_serf_send_slog(u3_noun hod)
{
  _cw_serf_send(u3nc(c3__slog, hod));
}

/* _cw_serf_send_stdr(): send stderr output (%flog)
*/
static void
_cw_serf_send_stdr(c3_c* str_c)
{
  _cw_serf_send(u3nc(c3__flog, u3i_string(str_c)));
}

/* _cw_serf_step_trace(): initialize or rotate trace file.
*/
static void
_cw_serf_step_trace(void)
{
  if ( u3C.wag_w & u3o_trace ) {
    c3_w trace_cnt_w = u3t_trace_cnt();
    if ( trace_cnt_w == 0  && u3t_file_cnt() == 0 ) {
      u3t_trace_open(u3V.dir_c);
    }
    else if ( trace_cnt_w >= 100000 ) {
      u3t_trace_close();
      u3t_trace_open(u3V.dir_c);
    }
  }
}

/* _cw_serf_writ(): process a command from the king.
*/
static void
_cw_serf_writ(void* vod_p, c3_d len_d, c3_y* byt_y)
{
  u3_weak jar;
  u3_noun ret;

  _cw_serf_step_trace();

#ifdef SERF_TRACE_CUE
  u3t_event_trace("serf ipc cue", 'B');
#endif

  jar = u3s_cue_xeno_with(sil_u, len_d, byt_y);

#ifdef SERF_TRACE_CUE
  u3t_event_trace("serf ipc cue", 'E');
#endif

  if (  (u3_none == jar)
     || (c3n == u3_serf_writ(&u3V, jar, &ret)) )
  {
    _cw_serf_fail(0, -1, "bad jar");
  }
  else {
    _cw_serf_send(ret);

    //  all references must now be counted, and all roots recorded
    //
    u3_serf_post(&u3V);
  }
}

/* _cw_serf_stdio(): fix up std io handles
*/
static void
_cw_serf_stdio(c3_i* inn_i, c3_i* out_i)
{
  //  the serf is spawned with [FD 0] = events and [FD 1] = effects
  //  we dup [FD 0 & 1] so we don't accidentally use them for something else
  //  we replace [FD 0] (stdin) with a fd pointing to /dev/null
  //  we replace [FD 1] (stdout) with a dup of [FD 2] (stderr)
  //
  c3_i nul_i = c3_open(c3_dev_null, O_RDWR, 0);

  *inn_i = dup(0);
  *out_i = dup(1);

  dup2(nul_i, 0);
  dup2(2, 1);

  close(nul_i);

  //  set stream I/O to unbuffered because it's now a pipe not a console
  //
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
}

/* _cw_serf_stdio(): cleanup on serf exit.
*/
static void
_cw_serf_exit(void)
{
  u3s_cue_xeno_done(sil_u);
  u3t_trace_close();
  u3m_stop();
}

/* _cw_init_io(): initialize i/o streams.
*/
static void
_cw_init_io(uv_loop_t* lup_u)
{
  //  mars is spawned with [FD 0] = events and [FD 1] = effects
  //  we dup [FD 0 & 1] so we don't accidentally use them for something else
  //  we replace [FD 0] (stdin) with a fd pointing to /dev/null
  //  we replace [FD 1] (stdout) with a dup of [FD 2] (stderr)
  //
  c3_i nul_i = c3_open(c3_dev_null, O_RDWR, 0);
  c3_i inn_i = dup(0);
  c3_i out_i = dup(1);

  dup2(nul_i, 0);
  dup2(2, 1);

  close(nul_i);

  //  set stream I/O to unbuffered because it's now a pipe not a console
  //
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  //  Ignore SIGPIPE signals.
  //
  {
    struct sigaction sig_s = {{0}};
    sigemptyset(&(sig_s.sa_mask));
    sig_s.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig_s, 0);
  }

  //  configure pipe to daemon process
  //
  {
    c3_i err_i;
    err_i = uv_timer_init(lup_u, &inn_u.tim_u);
    u3_assert(!err_i);
    err_i = uv_pipe_init(lup_u, &inn_u.pyp_u, 0);
    u3_assert(!err_i);
    uv_pipe_open(&inn_u.pyp_u, inn_i);
    err_i = uv_pipe_init(lup_u, &out_u.pyp_u, 0);
    u3_assert(!err_i);
    uv_pipe_open(&out_u.pyp_u, out_i);

    uv_stream_set_blocking((uv_stream_t*)&out_u.pyp_u, 1);
  }
}

/* _cw_serf_commence(): initialize and run serf
*/
static void
_cw_serf_commence(c3_i argc, c3_c* argv[])
{
  if ( 9 > argc ) {
    fprintf(stderr, "serf: missing args\n");
    exit(1);
  }
  //  XX use named arguments and getopt

  c3_d       eve_d = 0;
  uv_loop_t* lup_u = u3_Host.lup_u = uv_default_loop();
  c3_c*      dir_c = argv[2];
  c3_c*      key_c = argv[3]; // XX use passkey
  c3_c*      wag_c = argv[4];
  c3_c*      hap_c = argv[5];
  c3_c*      lom_c = argv[6];
  c3_w       lom_w;
  c3_c*      eve_c = argv[7];
  c3_c*      eph_c = argv[8];
  c3_c*      tos_c = argv[9];
  c3_c*      per_c = argv[10];
  c3_w       tos_w;

  _cw_init_io(lup_u);

  memset(&u3V, 0, sizeof(u3V));

  //  load passkey
  //
  //    XX and then ... use passkey
  //
  {
    sscanf(key_c, "%" PRIx64 ":%" PRIx64 ":%" PRIx64 ":%" PRIx64,
                  &u3V.key_d[0],
                  &u3V.key_d[1],
                  &u3V.key_d[2],
                  &u3V.key_d[3]);
  }

  //  load runtime config
  //
  {
    //  XX check return
    //
    sscanf(wag_c, "%" SCNu32, &u3C.wag_w);
    sscanf(hap_c, "%" SCNu32, &u3C.hap_w);
    sscanf(per_c, "%" SCNu32, &u3C.per_w);
    sscanf(lom_c, "%" SCNu32, &lom_w);

    if ( 1 != sscanf(tos_c, "%" SCNu32, &u3C.tos_w) ) {
      fprintf(stderr, "serf: toss: invalid number '%s'\r\n", tos_c);
    }

    if ( 1 != sscanf(eve_c, "%" PRIu64, &eve_d) ) {
      fprintf(stderr, "serf: rock: invalid number '%s'\r\n", eve_c);
    }
  }

  sil_u = u3s_cue_xeno_init();

  //  set up writing
  //
  out_u.ptr_v = &u3V;
  out_u.bal_f = _cw_serf_fail;

  //  set up reading
  //
  inn_u.ptr_v = &u3V;
  inn_u.pok_f = _cw_serf_writ;
  inn_u.bal_f = _cw_serf_fail;

  //  setup loom
  //
  {
    u3C.eph_c = (strcmp(eph_c, "0") == 0 ? 0 : strdup(eph_c));

    u3V.dir_c = strdup(dir_c);
    u3V.sen_d = u3V.dun_d = u3m_boot(dir_c, (size_t)1 << lom_w);

    if ( eve_d ) {
      //  XX need not be fatal, need a u3m_reboot equivalent
      //  XX can spuriously fail do to corrupt memory-image checkpoint,
      //  need a u3m_half_boot equivalent
      //  workaround is to delete/move the checkpoint in case of corruption
      //
      if ( c3n == u3u_uncram(u3V.dir_c, eve_d) ) {
        fprintf(stderr, "serf (%" PRIu64 "): rock load failed\r\n", eve_d);
        exit(1);
      }
    }
  }

  //  set up logging
  //
  //    XX must be after u3m_boot due to u3l_log
  //
  {
    u3C.stderr_log_f = _cw_serf_send_stdr;
    u3C.slog_f = _cw_serf_send_slog;
  }

  u3V.xit_f = _cw_serf_exit;

#if defined(SERF_TRACE_JAM) || defined(SERF_TRACE_CUE)
  u3t_trace_open(u3V.dir_c);
#endif

  //  start serf
  //
  {
    _cw_serf_send(u3_serf_init(&u3V));
  }

  //  start reading
  //
  u3_newt_read_sync(&inn_u);

  //  enter loop
  //
  uv_run(lup_u, UV_RUN_DEFAULT);
  u3m_stop();
}

/* _cw_disk_init(): open event log
*/
static u3_disk*
_cw_disk_init(c3_c* dir_c)
{
  u3_disk_cb cb_u = {0};
  u3_disk*  log_u = u3_disk_init(dir_c, cb_u);

  if ( !log_u ) {
    fprintf(stderr, "unable to open event log\n");
    exit(1);
  }

  return log_u;
}

/* _cw_dock(): copy binary into pier
*/
static void
_cw_dock(c3_i argc, c3_c* argv[])
{
  switch ( argc ) {
    case 2: {
      if ( !(u3_Host.dir_c = _main_pier_run(argv[0])) ) {
        fprintf(stderr, "unable to find pier\r\n");
        exit (1);
      }
    } break;

    case 3: {
      u3_Host.dir_c = argv[2];
    } break;

    default: {
      fprintf(stderr, "invalid command\r\n");
      exit(1);
    } break;
  }

  _main_self_path();

  u3_king_dock(U3_VERE_PACE);
}

/* _cw_eval_get_string(): read file til EOF and return a malloc'd string
*/
c3_c*
_cw_eval_get_string(FILE* fil_u, size_t siz_i)
{
  c3_i   car_i;
  size_t len_i = 0;
  c3_c*  str_c = c3_malloc(siz_i); //  size is start size

  while( EOF != (car_i = fgetc(fil_u)) ){
    str_c[len_i++] = car_i;
    if( len_i == siz_i ){
      siz_i += 16;
      str_c = c3_realloc(str_c, siz_i);
    }
  }

  str_c[len_i++]='\0';

  return c3_realloc(str_c, len_i);
}

/* _cw_eval_get_newt(): read a newt-encoded jammed noun from file and return a
**                      malloc'd byte buffer
*/
c3_y*
_cw_eval_get_newt(FILE* fil_u, c3_d* len_d)
{
  //  TODO: can't reuse u3_newt_decode; coupled to reading from pipe
  c3_d  i;
  c3_y  hed_y = sizeof(((u3_mess*)NULL)->hed_u.hed_y);
  c3_y* byt_y = c3_malloc(hed_y);

  for ( i = 0; i < hed_y; ++i ) {
    byt_y[i] = fgetc(fil_u);
  }

  if ( 0x0 != byt_y[0] ) {
    fprintf(stderr, "corrupted newt passed to cue\n");
    exit(1);
  }

  *len_d = (((c3_d)byt_y[1]) <<  0)
         | (((c3_d)byt_y[2]) <<  8)
         | (((c3_d)byt_y[3]) << 16)
         | (((c3_d)byt_y[4]) << 24);
  byt_y = c3_realloc(byt_y, *len_d);

  for ( i = 0; i < *len_d; ++i ) {
    byt_y[i] = fgetc(fil_u);
  }

  return byt_y;
}

/* _cw_eval(): initialize and run the hoon evaluator
*/
static void
_cw_eval(c3_i argc, c3_c* argv[])
{
  u3_mojo std_u;
  c3_i    ch_i, lid_i;
  c3_w    arg_w;
  c3_o    cue_o = c3n;
  c3_o    jam_o = c3n;
  c3_o    kan_o = c3n;
  c3_o    new_o = c3n;

  static struct option lop_u[] = {
    { "loom", required_argument,  NULL, c3__loom },
    { "cue",  no_argument,        NULL, 'c'},
    { "jam",  no_argument,        NULL, 'j' },
    { "newt", no_argument,        NULL, 'n' },
    //
    { NULL, 0, NULL, 0 }
  };

  while ( -1 != (ch_i=getopt_long(argc, argv, "cjkn", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 'c': {
        cue_o = c3y;
      } break;

      case 'j': {
        jam_o = c3y;
      } break;

      case 'k': {
        kan_o = c3y;
      } break;

      case 'n': {
        new_o = c3y;
      } break;

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  cannot have both jam and cue set
  //
  if ( ( c3y == cue_o ) && ( c3y == jam_o ) ) {
    fprintf(stderr, "cannot enable both jam and cue\r\n");
    exit(1);
  }
  //  newt meaningless without jam or cue set
  //
  if ( ( c3y == new_o ) && ( c3n == cue_o ) && ( c3n == jam_o) ) {
    fprintf(stderr, "newt meaningless w/o jam or cue; ignoring\r\n");
    new_o = c3n;
  }
  //  argv[optind] is always "eval"
  //
  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  //  configure stdout as u3_mojo
  //
  {
    c3_i err_i;
    err_i = uv_pipe_init(uv_default_loop(), &std_u.pyp_u, 0);
    u3_assert(!err_i);
    uv_pipe_open(&std_u.pyp_u, 1);

    std_u.ptr_v = NULL;
    std_u.bal_f = _cw_king_fail;
  }

  //  initialize the Loom and load the Ivory Pill
  //
  {
    c3_d         len_d = u3_Ivory_pill_len;
    c3_y*        byt_y = u3_Ivory_pill;
    u3_cue_xeno* sil_u;
    u3_weak      pil;

    u3C.wag_w |= u3o_hashless;
    u3m_boot_lite((size_t)1 << u3_Host.ops_u.lom_y);
    sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
    if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, len_d, byt_y)) ) {
      fprintf(stderr, "lite: unable to cue ivory pill\r\n");
      exit(1);
    }
    u3s_cue_xeno_done(sil_u);
    if ( c3n == u3v_boot_lite(pil) ) {
      u3l_log("lite: boot failed");
      exit(1);
    }
  }

  fprintf(stderr, "eval (");
  if ( c3y == cue_o ) {
    fprintf(stderr, "cue");
  } else if ( c3y == jam_o ) {
    fprintf(stderr, "jam");
  } else {
    fprintf(stderr, "run");
  }
  if ( c3y == new_o ) {
    fprintf(stderr, ", newt");
  }
  fprintf(stderr,"):\n");

  //  cue input and pretty-print on stdout
  //
  if ( c3y == cue_o ) {
    c3_d    len_d;
    c3_y*   byt_y;
    u3_weak som;
    if ( c3n == new_o ) {
      fprintf(stderr, "cue only supports newt encoding (for now)\n");
      exit(1);
    }
    byt_y = _cw_eval_get_newt(stdin, &len_d);
    som = u3s_cue_xeno(len_d, byt_y);
    if ( u3_none == som ) {
      fprintf(stderr, "cue failed\n");
      exit(1);
    }
    c3_c* pre_c;
    u3k(som);
    //  if input is jammed khan output
    if ( c3y == kan_o ) {
      u3_noun cop, uid, mar, res, tan;
      u3x_qual(som, &uid, &mar, &res, &tan);
      //  and if result is a goof
      if ( c3n == res ) {
        //  pretty-print tang to stderr and output only header
        u3_Host.ops_u.dem = c3y;
        u3_pier_punt_goof("eval", tan);
        cop = som;
        som = u3i_trel(uid, mar, res);
        u3k(som);
        u3z(cop);
      }
    }
    pre_c = u3m_pretty(som);
    fprintf(stdout, "%s\n", pre_c);
    c3_free(pre_c);
    u3z(som);
    free(byt_y);
  }
  //  jam input and return on stdout
  //
  else if ( c3y == jam_o ) {
    c3_d    bits = 0;
    c3_d    len_d = 0;
    c3_c*   evl_c = _cw_eval_get_string(stdin, 10);
    c3_y*   byt_y;
    u3_noun sam = u3i_string(evl_c);
    u3_noun res = u3m_soft(0, u3v_wish_n, sam);
    if ( 0 == u3h(res) ) {                //  successful execution, print output
      bits = u3s_jam_xeno(u3t(res), &len_d, &byt_y);
      if ( c3y == new_o ) {
        u3_newt_send(&std_u, len_d, byt_y);
      } else {
        for ( size_t p=0; p < len_d; p++ ) {
          fprintf(stdout,"\\x%2x", byt_y[p++]);
        }
      }
    } else {                              //  error, print stack trace
      u3_pier_punt_goof("eval", u3k(res));
    }
    u3z(res);
    free(evl_c);
  }
  //  slam eval gate with input
  //
  else {
    c3_c*   evl_c = _cw_eval_get_string(stdin, 10);
    //  +wish for an eval gate (virtualized twice for pretty-printing)
    u3_noun gat = u3v_wish("|=(a=@t (sell (slap !>(+>.$) (rain /eval a))))");
    u3_noun res;
    {
      u3_noun sam = u3i_string(evl_c);
      u3_noun cor = u3nc(u3k(u3h(gat)), u3nc(sam, u3k(u3t(u3t(gat)))));
      res = u3m_soft(0, u3n_kick_on, cor);
    }
    if ( 0 == u3h(res) ) {                //  successful execution, print output
      u3_pier_tank(0, 0, u3k(u3t(res)));
    } else {                              //  error, print stack trace
      u3_pier_punt_goof("eval", u3k(res));
    }
    u3z(res);
    u3z(gat);
    free(evl_c);
  }
}

/* _cw_info(): print pier info
*/
static void
_cw_info(c3_i argc, c3_c* argv[])
{
  c3_i lid_i, ch_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "info"
  //
  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  u3_Host.eve_d = u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);
  u3_disk* log_u = _cw_disk_init(u3_Host.dir_c);

  fprintf(stderr, "\r\nurbit: %s at event %" PRIu64 "\r\n",
                  u3_Host.dir_c, u3_Host.eve_d);

  u3_disk_slog(log_u);
  printf("\n");


  {
    c3_z  len_z = u3_disk_epoc_list(log_u, 0);
    c3_d* sot_d = c3_malloc(len_z * sizeof(c3_d));
    u3_disk_epoc_list(log_u, sot_d);

    fprintf(stderr, "epocs:\r\n");

    while ( len_z-- ) {
      fprintf(stderr, "  0i%" PRIu64 "\r\n", sot_d[len_z]);
    }

    c3_free(sot_d);
    fprintf(stderr, "\r\n");
  }

  u3_lmdb_stat(log_u->mdb_u, stdout);
  u3_disk_exit(log_u);

  u3m_stop();
}

/* _cw_grab(): gc pier.
*/
static void
_cw_grab(c3_i argc, c3_c* argv[])
{
  c3_i lid_i, ch_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "grab"
  //
  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);
  u3C.wag_w |= u3o_hashless;
  u3_serf_grab();
  u3m_stop();
}

/* _cw_cram(): jam persistent state (rock), and exit.
*/
static void
_cw_cram(c3_i argc, c3_c* argv[])
{
  c3_i ch_i, lid_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "cram"
  //

  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  u3_Host.eve_d = u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);
  u3_disk* log_u = _cw_disk_init(u3_Host.dir_c); // XX s/b try_aquire lock
  c3_o  ret_o;

  fprintf(stderr, "urbit: cram: preparing\r\n");

  if ( c3n == (ret_o = u3u_cram(u3_Host.dir_c, u3_Host.eve_d)) ) {
    fprintf(stderr, "urbit: cram: unable to jam state\r\n");
  }
  else {
    fprintf(stderr, "urbit: cram: rock saved at event %" PRIu64 "\r\n", u3_Host.eve_d);
  }

  //  save even on failure, as we just did all the work of deduplication
  //
  u3m_save();
  u3_disk_exit(log_u);

  if ( c3n == ret_o ) {
    exit(1);
  }

  u3m_stop();
}

/* _cw_queu(): cue rock, save, and exit.
*/
static void
_cw_queu(c3_i argc, c3_c* argv[])
{
  c3_i  lid_i, ch_i;
  c3_w  arg_w;
  c3_c* roc_c = 0;

  static struct option lop_u[] = {
    { "loom",        required_argument, NULL, c3__loom },
    { "no-demand",   no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { "replay-from", required_argument, NULL, 'r' },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "r:", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case 'r': {
        roc_c = strdup(optarg);
      } break;

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  if ( !roc_c ) {
    fprintf(stderr, "invalid command, -r $EVENT required\r\n");
    exit(1);
  }

  //  argv[optind] is always "queu"
  //
  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  c3_d eve_d;

  if ( 1 != sscanf(roc_c, "%" PRIu64 "", &eve_d) ) {
    fprintf(stderr, "urbit: queu: invalid number '%s'\r\n", roc_c);
    exit(1);
  }
  else {
    u3_Host.eve_d = u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);
    u3_disk* log_u = _cw_disk_init(u3_Host.dir_c); // XX s/b try_aquire lock

    fprintf(stderr, "urbit: queu: preparing\r\n");

    //  XX can spuriously fail do to corrupt memory-image checkpoint,
    //  need a u3m_half_boot equivalent
    //  workaround is to delete/move the checkpoint in case of corruption
    //
    if ( c3n == u3u_uncram(u3_Host.dir_c, eve_d) ) {
      fprintf(stderr, "urbit: queu: failed\r\n");
      exit(1);
    }

    u3m_save();
    u3_disk_exit(log_u);

    fprintf(stderr, "urbit: queu: rock loaded at event %" PRIu64 "\r\n", eve_d);
    u3m_stop();
  }
}

/* _cw_uniq(): deduplicate persistent nouns
*/
static void
_cw_meld(c3_i argc, c3_c* argv[])
{
  c3_i ch_i, lid_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { "gc-early",  no_argument,       NULL, 9 },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case 9: {  //  gc-early
        u3C.wag_w |= u3o_check_corrupt;
        break;
      }

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "meld"
  //

  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  u3C.wag_w |= u3o_hashless;

  u3_Host.eve_d = u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);
  u3_disk* log_u = _cw_disk_init(u3_Host.dir_c); // XX s/b try_aquire lock

  u3a_print_memory(stderr, "urbit: meld: gained", u3u_meld());

  u3m_save();
  u3_disk_exit(log_u);
  u3m_stop();
}

/* _cw_next(): request upgrade
*/
static void
_cw_next(c3_i argc, c3_c* argv[])
{
  c3_i ch_i, lid_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "arch",      required_argument, NULL, 'a' },
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "a:", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case 'a': {
        u3_Host.arc_c = strdup(optarg);
      } break;

      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "next"
  //

  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  u3_Host.pep_o = c3y;
  u3_Host.nex_o = c3y;
  u3_Host.ops_u.tem = c3y;
}

/* _cw_pack(): compact memory, save, and exit.
*/
static void
_cw_pack(c3_i argc, c3_c* argv[])
{
  c3_i ch_i, lid_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { "gc-early",  no_argument,       NULL, 9 },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case 9: {  //  gc-early
        u3C.wag_w |= u3o_check_corrupt;
        break;
      }

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "pack"
  //

  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  u3_Host.eve_d = u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);
  u3_disk* log_u = _cw_disk_init(u3_Host.dir_c); // XX s/b try_aquire lock

  u3a_print_memory(stderr, "urbit: pack: gained", u3m_pack());

  u3m_save();
  u3_disk_exit(log_u);
  u3m_stop();
}

/* _cw_play_slog(): print during replay.
*/
static void
_cw_play_slog(u3_noun hod)
{
  u3_pier_tank(0, 0, u3k(u3t(hod)));
  u3z(hod);
}

/* _cw_play_snap(): prepare snapshot for full replay.
*/
static void
_cw_play_snap(u3_disk* log_u)
{
  c3_c chk_c[8193], epo_c[8193];
  snprintf(chk_c, 8193, "%s/.urb/chk", u3_Host.dir_c);
  snprintf(epo_c, 8192, "%s/0i%" PRIc3_d, log_u->com_u->pax_c, log_u->epo_d);

  if ( 0 == log_u->epo_d ) {
    //  if epoch 0 is the latest, delete the snapshot files in chk/
    c3_c nor_c[8193], sop_c[8193];
    snprintf(nor_c, 8193, "%s/.urb/chk/north.bin", u3_Host.dir_c);
    snprintf(sop_c, 8193, "%s/.urb/chk/south.bin", u3_Host.dir_c);
    if ( c3_unlink(nor_c) && (ENOENT != errno) ) {
      fprintf(stderr, "mars: failed to unlink %s: %s\r\n",
                      nor_c, strerror(errno));
      exit(1);
    }
    if ( c3_unlink(sop_c) && (ENOENT != errno) ) {
      fprintf(stderr, "mars: failed to unlink %s: %s\r\n",
                      sop_c, strerror(errno));
      exit(1);
    }
  }
  else if ( 0 != u3e_backup(epo_c, chk_c, c3y) ) {
    //  copy the latest epoch's snapshot files into chk/
    fprintf(stderr, "mars: failed to copy snapshot\r\n");
    exit(1);
  }
}

/* _cw_play_exit(): exit immediately.
*/
static void
_cw_play_exit(c3_i int_i)
{
  //  explicit fprintf to avoid allocation in u3l_log
  //
  fprintf(stderr, "\r\n[received keyboard stop signal, exiting]\r\n");
  raise(SIGINT);
}

/* _cw_play_impl(): replay events, but better.
*/
static c3_d
_cw_play_impl(c3_d eve_d, c3_d sap_d, c3_o mel_o, c3_o sof_o, c3_o ful_o)
{
  c3_d pay_d;

  //  XX handle SIGTSTP so that the lockfile is not orphaned?
  //
  u3_disk* log_u = _cw_disk_init(u3_Host.dir_c);

  //  Handle SIGTSTP as if it was SIGINT.
  //
  //    Configured here using signal() so as to be immediately available.
  //
  signal(SIGTSTP, _cw_play_exit);

  //  XX source these from a shared struct ops_u
  if ( c3y == mel_o ) {
    u3C.wag_w |= u3o_auto_meld;
  }

  if ( c3y == sof_o ) {
    u3C.wag_w |= u3o_soft_mugs;
  }

  u3C.wag_w |= u3o_hashless;

  if ( c3y == ful_o ) {
    u3l_log("mars: preparing for full replay");
    _cw_play_snap(log_u);
  }

  u3_Host.eve_d = u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);

  //  XX this should load from the epoc snapshot
  //  but that clobbers chk/ which is risky
  //
  if ( u3_Host.eve_d < log_u->epo_d ) {
    fprintf(stderr, "mars: pier corrupt: "
                    "snapshot (%" PRIu64 ") out of epoc (%" PRIu64 ")\r\n",
                    u3_Host.eve_d, log_u->epo_d);
    exit(1);
  }

  u3C.slog_f = _cw_play_slog;

  {
    u3_mars mar_u = {
      .log_u = log_u,
      .dir_c = u3_Host.dir_c,
      .sen_d = u3A->eve_d,
      .dun_d = u3A->eve_d,
    };

    pay_d = u3_mars_play(&mar_u, eve_d, sap_d);
    u3_Host.eve_d = mar_u.dun_d;

    //  migrate or rollover as needed
    //
    u3_disk_kindly(log_u, u3_Host.eve_d);
  }

  u3_disk_exit(log_u);
  //  NB: loom migrations without replay are not saved
  u3m_stop();

  return pay_d;
}

/* _cw_play(): replay events, but better.
*/
static void
_cw_play(c3_i argc, c3_c* argv[])
{
  c3_i lid_i, ch_i;
  c3_w arg_w;
  c3_o ful_o = c3n;
  c3_o mel_o = c3n;
  c3_o sof_o = c3n;
  c3_d eve_d = 0;
  c3_d sap_d = 0;

  static struct option lop_u[] = {
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "auto-meld", no_argument,       NULL, 7 },
    { "soft-mugs", no_argument,       NULL, 8 },
    { "full",      no_argument,       NULL, 'f' },
    { "replay-to", required_argument, NULL, 'n' },
    { "snap-at",   required_argument, NULL, 's' },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "fn:", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  auto-meld
        mel_o = c3y;
      } break;

      case 8: {  //  soft-mugs
        sof_o = c3y;
      } break;

      case 'f': {
        ful_o = c3y;
      } break;

      case 'n': {
        if ( 1 != sscanf(optarg, "%" PRIu64 "", &eve_d) ) {
          fprintf(stderr, "mars: replay-to invalid: '%s'\r\n", optarg);
          exit(1);
        }
      } break;

      case 's': {
        if ( 1 != sscanf(optarg, "%" PRIu64 "", &sap_d) ) {
          fprintf(stderr, "mars: snap-at invalid: '%s'\r\n", optarg);
          exit(1);
        }
      } break;

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "play"
  //

  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  if ( !_cw_play_impl(eve_d, sap_d, mel_o, sof_o, ful_o) ) {
    fprintf(stderr, "mars: nothing to do!\r\n");
  }
}

/* _cw_prep(): prepare for upgrade
*/
static void
_cw_prep(c3_i argc, c3_c* argv[])
{
  //  XX roll with old binary
  //     check that new epoch is empty, migrate snapshot in-place
  c3_i ch_i, lid_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "prep"
  //

  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  u3_Host.pep_o = c3y;
  u3_Host.ops_u.tem = c3y;
}

/* _cw_chop(): truncate event log
*/
static void
_cw_chop(c3_i argc, c3_c* argv[])
{
  c3_i ch_i, lid_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "chop"
  //

  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  // gracefully shutdown the pier if it's running
  u3_Host.eve_d = u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);
  u3_disk* log_u = _cw_disk_init(u3_Host.dir_c);

  u3_disk_kindly(log_u, u3_Host.eve_d);
  u3_disk_chop(log_u, u3_Host.eve_d);

  u3_disk_exit(log_u);
  u3m_stop();
}

/* _cw_roll(): rollover to new epoch
 */
static void
_cw_roll(c3_i argc, c3_c* argv[])
{
  c3_i ch_i, lid_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "loom", required_argument, NULL, c3__loom },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "roll"
  //

  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  // gracefully shutdown the pier if it's running
  u3_Host.eve_d = u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);
  u3_disk* log_u = _cw_disk_init(u3_Host.dir_c);

  u3_disk_kindly(log_u, u3_Host.eve_d);
  u3_disk_roll(log_u, u3_Host.eve_d);

  u3_disk_exit(log_u);
  u3m_stop();
}

/* _cw_vere(): download vere
*/
static void
_cw_vere(c3_i argc, c3_c* argv[])
{
  c3_c* pac_c = "live";
  c3_c* arc_c = 0;
  c3_c* ver_c = 0;
  c3_c* dir_c;

  c3_i ch_i, lid_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "arch",    required_argument, NULL, 'a' },
    { "pace",    required_argument, NULL, 'p' },
    { "version", required_argument, NULL, 'v' },
    { NULL, 0, NULL, 0 }
  };

  while ( -1 != (ch_i=getopt_long(argc, argv, "a:p:v:", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case 'a': {
        arc_c = strdup(optarg);
      } break;

      case 'p': {
        pac_c = strdup(optarg);
      } break;

      case 'v': {
        ver_c = strdup(optarg);
      } break;

      case '?': {
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "vere"/"fetch-vere"
  //

  if ( optind + 1 < argc ) {
    dir_c = argv[optind + 1];
    optind++;
  }
  else {
    fprintf(stderr, "invalid command, output directory required\r\n");
    exit(1);
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  if ( !arc_c ) {
#ifdef U3_OS_ARCH
    arc_c = U3_OS_ARCH;
#else
    fprintf(stderr, "unknown architecture, --arch required\r\n");
    exit(1);
#endif
  }

  //  Initialize OpenSSL for client and server
  //
  {
    SSL_library_init();
    SSL_load_error_strings();
  }

  //  initialize curl
  //
  if ( 0 != curl_global_init(CURL_GLOBAL_DEFAULT) ) {
    u3l_log("boot: curl initialization failed");
    exit(1);
  }

  _setup_cert_store();
  u3K.ssl_curl_f = _setup_ssl_curl;
  u3K.ssl_x509_f = _setup_ssl_x509;

  if ( !ver_c ) {
    switch ( u3_king_next(pac_c, &ver_c) ) {
      case -2: {
        fprintf(stderr, "vere: unable to check for next version\n");
        exit(1);
      } break;

      case -1: {
        fprintf(stderr, "you're already running it!\n");
        exit(0);
      } break;

      case 0: {
        fprintf(stderr, "vere: next (%%%s): %s\n", pac_c, ver_c);
      } break;

      default: u3_assert(0);
    }
  }


  if ( u3_king_vere(pac_c, ver_c, arc_c, dir_c, 0) ) {
    u3l_log("vere: download failed");
    exit(1);
  }

  u3l_log("vere: download succeeded");
}

/* _cw_vile(): generate/print keyfile
*/
static void
_cw_vile(c3_i argc, c3_c* argv[])
{
  c3_i ch_i, lid_i;
  c3_w arg_w;

  static struct option lop_u[] = {
    { "loom",      required_argument, NULL, c3__loom },
    { "no-demand", no_argument,       NULL, 6 },
    { "swap",      no_argument,       NULL, 7 },
    { "swap-to",   required_argument, NULL, 8 },
    { NULL, 0, NULL, 0 }
  };

  u3_Host.dir_c = _main_pier_run(argv[0]);

  while ( -1 != (ch_i=getopt_long(argc, argv, "", lop_u, &lid_i)) ) {
    switch ( ch_i ) {
      case c3__loom: {
        if (_main_readw_loom("loom", &u3_Host.ops_u.lom_y)) {
          exit(1);
        }
      } break;

      case 6: {  //  no-demand
        u3_Host.ops_u.map = c3n;
        u3C.wag_w |= u3o_no_demand;
      } break;

      case 7: {  //  swap
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
      } break;

      case 8: {  //  swap-to
        u3_Host.ops_u.eph = c3y;
        u3C.wag_w |= u3o_swap;
        u3C.eph_c = strdup(optarg);
        break;
      }

      case '?': {
        fprintf(stderr, "invalid argument\r\n");
        exit(1);
      } break;
    }
  }

  //  argv[optind] is always "vile"
  //

  if ( !u3_Host.dir_c ) {
    if ( optind + 1 < argc ) {
      u3_Host.dir_c = argv[optind + 1];
    }
    else {
      fprintf(stderr, "invalid command, pier required\r\n");
      exit(1);
    }

    optind++;
  }

  if ( optind + 1 != argc ) {
    fprintf(stderr, "invalid command\r\n");
    exit(1);
  }

  //  XX check if snapshot is stale?
  //
  c3_d  eve_d = u3m_boot(u3_Host.dir_c, (size_t)1 << u3_Host.ops_u.lom_y);
  u3_noun sam = u3nc(u3nc(u3_nul, u3_nul),
                     u3nc(c3n, u3nq(c3__once, 'j', c3__vile, u3_nul)));
  u3_noun res = u3v_soft_peek(0, sam);


  switch ( u3h(res) ) {
    default: u3_assert(0);

    case c3n: {
      fprintf(stderr, "vile: unable to retrieve key file\r\n");
      u3_pier_punt_goof("foo", u3k(u3t(res)));
    }
    case c3y: {
      u3_noun dat, vil, out;
      c3_c* out_c;

      if (  (u3_nul != u3h(u3t(res)))
         || (c3n == u3r_pq(u3t(u3t(res)), c3__omen, 0, &dat))
         || (c3n == u3r_p(dat, c3__atom, &vil))
         || (c3n == u3a_is_atom(vil)) )
      {
        fprintf(stderr, "vile: unable to extract key file\r\n");
        u3m_p("vil", res);
      }
      else {
        out = u3dc("scot", c3__uw, u3k(vil));
        out_c = u3r_string(out);
        puts(out_c);
        c3_free(out_c);
        u3z(out);
      }
    }
  }

  u3z(res);
}

/* _cw_utils(): "worker" utilities and "serf" entrypoint
*/
static c3_i
_cw_utils(c3_i argc, c3_c* argv[])
{
  //  utility commands and positional arguments, by analogy
  //
  //    $@  ~                                             ::  usage
  //    $%  [%cram dir=@t]                                ::  jam state
  //        [%dock dir=@t]                                ::  copy binary
  //        [?(%grab %mass) dir=@t]                       ::  gc
  //        [%info dir=@t]                                ::  print
  //        [%meld dir=@t]                                ::  deduplicate
  //        [?(%next %upgrade) dir=@t]                    ::  upgrade
  //        [%pack dir=@t]                                ::  defragment
  //        [%play dir=@t]                                ::  recompute
  //        [%prep dir=@t]                                ::  prep upgrade
  //        [%queu dir=@t eve=@ud]                        ::  cue state
  //        [?(%vere %fetch-vere) dir=@t]                 ::  download vere
  //        [%vile dir=@t]                                ::  extract keys
  //    ::                                                ::    ipc:
  //        $:  %serf                                     ::  compute
  //            dir=@t  key=@t wag=@t hap=@ud             ::
  //            lom=@ud eve=@ud                           ::
  //    ==  ==                                            ::
  //
  //    NB: don't print to anything other than stderr;
  //    other streams may be used for ipc.
  //
  c3_m mot_m = 0;

  if ( 2 <= argc ) {
    if ( 4 == strlen(argv[1]) ) {
      c3_c* s = argv[1];
      mot_m = c3_s4(s[0], s[1], s[2], s[3]);
    }
    else if ( 0 == strcmp(argv[1], "upgrade") ) {
      mot_m = c3__next;
    }
    else if ( 0 == strcmp(argv[1], "fetch-vere") ) {
      mot_m = c3__vere;
    }
  }

  switch ( mot_m ) {
    case c3__cram: _cw_cram(argc, argv); return 1;
    case c3__dock: _cw_dock(argc, argv); return 1;
    case c3__eval: _cw_eval(argc, argv); return 1;

    case c3__mass:
    case c3__grab: _cw_grab(argc, argv); return 1;

    case c3__info: _cw_info(argc, argv); return 1;
    case c3__meld: _cw_meld(argc, argv); return 1;
    case c3__next: _cw_next(argc, argv); return 2; // continue on
    case c3__pack: _cw_pack(argc, argv); return 1;
    case c3__play: _cw_play(argc, argv); return 1;
    case c3__prep: _cw_prep(argc, argv); return 2; // continue on
    case c3__queu: _cw_queu(argc, argv); return 1;
    case c3__chop: _cw_chop(argc, argv); return 1;
    case c3__roll: _cw_roll(argc, argv); return 1;
    case c3__vere: _cw_vere(argc, argv); return 1;
    case c3__vile: _cw_vile(argc, argv); return 1;

    case c3__serf: _cw_serf_commence(argc, argv); return 1;
  }

  return 0;
}

c3_i
main(c3_i   argc,
     c3_c** argv)
{
  if ( argc <= 0 ) {
    fprintf(stderr, "nice try, fbi\r\n");
    exit(1);
  }

  _main_init();

#if defined(U3_OS_osx)
  darwin_register_mach_exception_handler();
#endif

  c3_c* bin_c = strdup(argv[0]);

  //  parse for subcommands
  //
  switch ( _cw_utils(argc, argv) ) {
    default: u3_assert(0);

    //  no matching subcommand, parse arguments
    //
    case 0: {
      if ( c3n == _main_getopt(argc, argv) ) {
        u3_ve_usage(argc, argv);
        return 1;
      }
    } break;

    //  ran subcommand
    case 1: {
      return 0;
    }

    //  found subcommand, continue
    //
    case 2: break;
  }

  _main_self_path();

  //  XX add argument
  //
  if ( !u3_Host.wrk_c ) {
    u3_Host.wrk_c = bin_c;
  }
  else {
    c3_free(bin_c);
  }

  if ( c3y == u3_Host.ops_u.dem ) {
    //  In daemon mode, run the urbit as a background process, but don't
    //  exit from the parent process until the ship is finished booting.
    //
    u3_daemon_init();
  }

  if ( c3y == u3_Host.ops_u.rep ) {
    report();
    return 0;
  }

  if ( c3y == u3_Host.ops_u.tex ) {
    u3_Host.bot_f = _stop_on_boot_completed_cb;
  }

#if 0
  if ( 0 == getuid() ) {
    chroot(u3_Host.dir_c);
    u3_Host.dir_c = "/";
  }
#endif

  //  Block profiling signal, which should be delivered to exactly one thread.
  //
  //    XX review, may be unnecessary due to similar in u3m_init()
  //
#if defined(U3_OS_PROF)
  if ( _(u3_Host.ops_u.pro) ) {
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGPROF);
    if ( 0 != pthread_sigmask(SIG_BLOCK, &set, NULL) ) {
      u3l_log("boot: thread mask SIGPROF: %s", strerror(errno));
      exit(1);
    }
  }
#endif

  //  Handle SIGTSTP as if it was SIGTERM.
  //
  //    Configured here using signal() so as to be immediately available.
  //
  signal(SIGTSTP, _stop_exit);

  printf("~\n");
  //  printf("welcome.\n");
  printf("urbit %s\n", URBIT_VERSION);
  printf("boot: home is %s\n", u3_Host.dir_c);
  // printf("vere: hostname is %s\n", u3_Host.ops_u.nam_c);

  if ( c3y == u3_Host.ops_u.dem ) {
    printf("boot: running as daemon\n");
  }

  //  Instantiate process globals.
  {
    /*  Boot the image and checkpoint.  Set flags.
    */
    {
      /*  Set pier directory.
      */
      u3C.dir_c = u3_Host.dir_c;

      /*  Logging that doesn't interfere with console output.
      */
      u3C.stderr_log_f = u3_term_io_log;

      /*  Set GC flag.
      */
      if ( _(u3_Host.ops_u.gab) ) {
        u3C.wag_w |= u3o_debug_ram;
      }

      /*  Set no-demand flag.
      */
      if ( !_(u3_Host.ops_u.map) ) {
        u3C.wag_w |= u3o_no_demand;
      }

      /*  Set profile flag.
      */
      if ( _(u3_Host.ops_u.pro) ) {
        u3C.wag_w |= u3o_debug_cpu;
      }

      /*  Set verbose flag.
      */
      if ( _(u3_Host.ops_u.veb) ) {
        u3C.wag_w |= u3o_verbose;
      }

      /*  Set quiet flag.
      */
      if ( _(u3_Host.ops_u.qui) ) {
        u3C.wag_w |= u3o_quiet;
      }

      /*  Set dry-run flag.
      **
      **    XX also exit immediately?
      */
      if ( _(u3_Host.ops_u.dry) ) {
        u3C.wag_w |= u3o_dryrun;
      }

      /*  Set hashboard flag
      */
      if ( _(u3_Host.ops_u.has) ) {
        u3C.wag_w |= u3o_hashless;
      }

      /*  Set tracing flag
      */
      if ( _(u3_Host.ops_u.tra) ) {
        u3C.wag_w |= u3o_trace;
      }

      /*  Set swap flag
      */
      if ( _(u3_Host.ops_u.eph) ) {
        u3C.wag_w |= u3o_swap;
      }

      /*  Set toss flog
      */
      if ( _(u3_Host.ops_u.tos) ) {
        u3C.wag_w |= u3o_toss;
      }
    }

    //  we need the current snapshot's latest event number to
    //  validate whether we can execute disk migration
    if ( u3_Host.ops_u.nuu == c3n ) {
      _cw_play_impl(0, 0, c3n, c3n, c3n);
      //  XX  unmap loom, else parts of the snapshot could be left in memory
    }

    //  starting u3m configures OpenSSL memory functions, so we must do it
    //  before any OpenSSL allocations
    //
    u3m_boot_lite((size_t)1 << u3_Host.ops_u.lut_y);

    //  Initialize OpenSSL for client and server
    //
    {
      SSL_library_init();
      SSL_load_error_strings();
    }

    //  initialize curl
    //
    if ( 0 != curl_global_init(CURL_GLOBAL_DEFAULT) ) {
      u3l_log("boot: curl initialization failed");
      exit(1);
    }

    _setup_cert_store();
    u3K.ssl_curl_f = _setup_ssl_curl;
    u3K.ssl_x509_f = _setup_ssl_x509;

    u3_king_commence();

    //  uninitialize curl
    //
    curl_global_cleanup();

    //  uninitialize OpenSSL
    //
    //    see https://wiki.openssl.org/index.php/Library_Initialization
    //
    {
      ENGINE_cleanup();
      CONF_modules_unload(1);
      EVP_cleanup();
      CRYPTO_cleanup_all_ex_data();
      SSL_COMP_free_compression_methods();
      ERR_free_strings();
    }
  }

  return 0;
}
