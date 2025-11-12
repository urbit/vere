/// @file

#include "vere.h"
#include <sys/stat.h>
#include <sys/types.h>
#include "noun.h"

/* u3_chan: incoming ipc port connection.
*/
typedef struct _u3_chan {
  struct _u3_moor   mor_u;            //  message handler
  c3_l              coq_l;            //  connection number
  c3_o              liv_o;            //  connection live
  struct _u3_shan*  san_u;            //  server backpointer
} u3_chan;

/* u3_shan: ipc port server.
*/
typedef struct _u3_shan {
  uv_pipe_t          pyp_u;           //  server stream handler
  c3_l               nex_l;           //  next connection number
  struct _u3_port*   gen_u;           //  port backpointer
  struct _u3_chan*   can_u;           //  connection list
} u3_shan;


/* u3_port: description of an IPC port
*/
typedef struct _u3_port {
  c3_c*              nam_c;           //  name of port
  c3_o               con_o;
  c3_o               liv_o;
  struct _u3_shan*   san_u;           //  server reference
  struct _u3_lick*   lic_u;           //  device backpointer
  struct _u3_port*   nex_u;           //  next pointer
} u3_port;

/* u3_lick: a list of devices
*/
typedef struct _u3_lick {
  u3_auto            car_u;           //  driver
  c3_c*              fod_c;           //  IPC folder location
  u3_cue_xeno*       sil_u;           //  cue handle
  struct _u3_port*   gen_u;           //  port list
} u3_lick;

static const c3_c URB_DEV_PATH[] = "/.urb/dev";

/* _lick_string_to_knot(): convert c unix path component to $knot
*/
static u3_atom
_lick_string_to_knot(c3_c* pax_c)
{
  u3_assert(pax_c);
  u3_assert(!strchr(pax_c, '/'));
  if ( '!' == *pax_c ) {
    pax_c++;
  }
  return u3i_string(pax_c);
}

/* _lick_string_to_path(): convert c string to u3_noun $path
**
**  c string must begin with the pier path plus mountpoint
*/
static u3_noun
_lick_string_to_path(c3_c* pax_c)
{
  u3_noun not;

  //u3_assert(pax_c[-1] == '/');
  c3_c* end_c = strchr(pax_c, '/');
  if ( !end_c ) {
    return u3nc(_lick_string_to_knot(pax_c), u3_nul);
  }
  else {
    *end_c = 0;
    not = _lick_string_to_knot(pax_c);
    *end_c = '/';
    return u3nc(not, _lick_string_to_path(end_c + 1));
  }
}

/* _lick_it_path(): path for ipc files
*/
static c3_c*
_lick_it_path(u3_noun pax)
{
  c3_w len_w = 0;
  c3_c *pas_c;

  //  measure
  //
  {
    u3_noun wiz = pax;

    while ( u3_nul != wiz ) {
      len_w += (1 + u3r_met(3, u3h(wiz)));
      wiz = u3t(wiz);
    }
  }

  //  cut
  //
  pas_c = c3_malloc(len_w + 1);
  pas_c[len_w] = '\0';
  {
    u3_noun wiz   = pax;
    c3_c*   waq_c = pas_c;

    while ( u3_nul != wiz ) {
      c3_w tis_w = u3r_met(3, u3h(wiz));

      if ( (u3_nul == u3t(wiz)) ) {
        *waq_c++ = '/';
      } else *waq_c++ = '/';

      u3r_bytes(0, tis_w, (c3_y*)waq_c, u3h(wiz));
      waq_c += tis_w;

      wiz = u3t(wiz);
    }
    *waq_c = 0;
  }
  u3z(pax);
  return pas_c;
}

/* _lick_send_noun(): jam and send noun over chan.
*/
static void
_lick_send_noun(u3_chan* can_u, u3_noun nun)
{
  c3_y* byt_y;
  c3_d  len_d;

  u3s_jam_xeno(nun, &len_d, &byt_y);
  u3z(nun);
  u3_newt_send((u3_mojo*)&can_u->mor_u, len_d, byt_y);
}

/* _lick_mote_free(): u3_moat-shaped close callback.
*/
static void
_lick_moat_free(void* ptr_v, ssize_t err_i, const c3_c* err_c)
{
  c3_free((u3_chan*)ptr_v);
}

/* _lick_close_cb(): socket close callback.
*/
static void
_lick_close_cb(uv_handle_t* had_u)
{
  c3_free((u3_shan*)had_u);
}

/* _lick_moor_poke(): called on message read from u3_moor.
*/
static c3_o
_lick_moor_poke(void* ptr_v, c3_d len_d, c3_y* byt_y)
{
  u3_weak   put;
  u3_noun   dev, nam, dat, wir, cad;

  u3_chan*  can_u = (u3_chan*)ptr_v;
  u3_port*  gen_u = can_u->san_u->gen_u;
  u3_lick*  lic_u = gen_u->lic_u;
  c3_i      err_i = 0;
  c3_c*     err_c;
  c3_c*     tag_c;
  c3_c*     rid_c;

  put = u3s_cue_xeno_with(lic_u->sil_u, len_d, byt_y);
  if ( u3_none == put ) {
    can_u->mor_u.bal_f(can_u, -1, "cue-none");
    return c3y;
  }
  if ( c3n == u3r_cell(put, &nam, &dat) ) {
    can_u->mor_u.bal_f(can_u, -2, "put-bad");
    u3z(put);
    return c3y;
  }

  wir = u3nc(c3__lick, u3_nul);
  dev = _lick_string_to_path(gen_u->nam_c+1);
  cad = u3nt(c3__soak, dev, put);
  u3_auto_peer(
    u3_auto_plan(&lic_u->car_u, u3_ovum_init(0, c3__l, wir, cad)),
    0, 0, 0);

  return c3y;
}

/* _lick_close_chan(): close given channel, freeing.
*/
static void
_lick_close_chan(u3_chan* can_u)
{
  u3_shan* san_u = can_u->san_u;
  u3_lick* lic_u = san_u->gen_u->lic_u;
  u3_port* gen_u = san_u->gen_u;
  gen_u->con_o = c3n;
  u3_chan*  inn_u;
  //  remove chan from server's connection list.
  //
  if ( san_u->can_u == can_u ) {
    san_u->can_u = (u3_chan*)can_u->mor_u.nex_u;
  }
  else {
    for ( inn_u = san_u->can_u; inn_u; inn_u = (u3_chan*)inn_u->mor_u.nex_u ) {
      if ( (u3_chan*)inn_u->mor_u.nex_u == can_u ) {
        inn_u->mor_u.nex_u = can_u->mor_u.nex_u;
        break;
      }
    }
  }
  can_u->mor_u.nex_u = NULL;

  if ( NULL == san_u->can_u && c3y == gen_u->liv_o ) {
    //  send a close event to arvo and stop reading.
    //
    u3_noun wir, cad, dev, dat, mar;

    wir = u3nc(c3__lick, u3_nul);
    dev = _lick_string_to_path(gen_u->nam_c+1);
    mar = u3i_string("disconnect");
    dat = u3_nul;

    cad = u3nq(c3__soak, dev, mar, dat);

    u3_auto_peer(
      u3_auto_plan(&lic_u->car_u,
                   u3_ovum_init(0, c3__l, wir, cad)),
      0, 0, 0);
  }

  u3_newt_moat_stop((u3_moat*)&can_u->mor_u, _lick_moat_free);
}

/* _lick_moor_bail(): error callback for u3_moor.
*/
static void
_lick_moor_bail(void* ptr_v, ssize_t err_i, const c3_c* err_c)
{
  u3_chan*  can_u = (u3_chan*)ptr_v;

  if ( err_i != UV_EOF ) {
    u3l_log("lick: moor bail %zd %s", err_i, err_c);
    if ( _(can_u->liv_o) ) {
      _lick_send_noun(can_u, u3nq(0, c3__bail, u3i_word(err_i),
                      u3i_string(err_c)));
      can_u->liv_o = c3n;
    }
  }
  _lick_close_chan(can_u);
}

/* _lick_sock_cb(): socket connection callback.
*/
static void
_lick_sock_cb(uv_stream_t* sem_u, c3_i tas_i)
{
  u3_shan*  san_u = (u3_shan*)sem_u;
  u3_port*  gen_u = san_u->gen_u;
  u3_chan*  can_u;
  c3_i      err_i;

  can_u = c3_calloc(sizeof(u3_chan));
  can_u->mor_u.ptr_v = can_u;
  can_u->mor_u.pok_f = _lick_moor_poke;
  can_u->mor_u.bal_f = _lick_moor_bail;
  can_u->coq_l = san_u->nex_l++;
  can_u->san_u = san_u;
  err_i = uv_timer_init(u3L, &can_u->mor_u.tim_u);
  u3_assert(!err_i);
  err_i = uv_pipe_init(u3L, &can_u->mor_u.pyp_u, 0);
  u3_assert(!err_i);
  err_i = uv_accept(sem_u, (uv_stream_t*)&can_u->mor_u.pyp_u);
  u3_assert(!err_i);
  u3_newt_read((u3_moat*)&can_u->mor_u);
  can_u->mor_u.nex_u = (u3_moor*)san_u->can_u;
  san_u->can_u = can_u;
  gen_u->con_o = c3y;

  // only send on first connection
  if ( NULL == can_u->mor_u.nex_u ) {
    u3_noun   dev, dat, wir, cad, mar;
    wir = u3nc(c3__lick, u3_nul);
    dev = _lick_string_to_path(gen_u->nam_c+1);
    mar = u3i_string("connect");
    dat = u3_nul;

    cad = u3nq(c3__soak, dev, mar, dat);
    u3_auto_peer(
      u3_auto_plan(&gen_u->lic_u->car_u, u3_ovum_init(0, c3__l, wir, cad)),
      0, 0, 0);
  }
}

/* _lick_close_sock():  close an port's socket
*/
static void
_lick_close_sock(u3_shan* san_u)
{
  u3_lick*  lic_u = san_u->gen_u->lic_u;

  if ( NULL != san_u->can_u ) {
    _lick_close_chan(san_u->can_u);
  }

  c3_w len_w = strlen(lic_u->fod_c) + strlen(san_u->gen_u->nam_c) + 2;
  c3_c* paf_c = c3_malloc(len_w);
  c3_i  wit_i;
  wit_i = snprintf(paf_c, len_w, "%s/%s", lic_u->fod_c, san_u->gen_u->nam_c);

  u3_assert(wit_i > 0);
  u3_assert(len_w == (c3_w)wit_i + 1);

  if ( 0 != unlink(paf_c) ) {
    if ( ENOENT != errno ) {
      u3l_log("lick: failed to unlink socket: %s", uv_strerror(errno));
    }
  }

  uv_close((uv_handle_t*)&san_u->pyp_u, _lick_close_cb);
  c3_free(paf_c);
}

/* _lick_mkdirp(): recursive mkdir of dirname of pax_c.
*/
static c3_o
_lick_mkdirp(c3_c* por_c)
{
  c3_c pax_c[2048];

  strncpy(pax_c, por_c, sizeof(pax_c));

  c3_c* fas_c = strchr(pax_c + 1, '/');

  while ( fas_c ) {
    *fas_c = 0;
    if ( 0 != mkdir(pax_c, 0777) && EEXIST != errno ) {
      return c3n;
    }
    *fas_c++ = '/';
    fas_c = strchr(fas_c, '/');
  }
  return c3y;
}

/* _lick_init_sock(): initialize socket device.
*/
static void
_lick_init_sock(u3_shan* san_u)
{
  //  the full socket path is limited to about 108 characters,
  //  and we want it to be relative to the pier. save our current
  //  path, chdir to the pier, open the socket at the desired
  //  path, then chdir back. hopefully there aren't any threads.
  //
  c3_c pax_c[2048];
  c3_i err_i;
  c3_c por_c[2048] = ".";
  u3_port* gen_u = san_u->gen_u;

  if ( NULL == getcwd(pax_c, sizeof(pax_c)) ) {
    u3l_log("lick: getcwd: %s", uv_strerror(errno));
    u3_king_bail();
  }
  if ( 0 != chdir(u3_Host.dir_c) ) {
    u3l_log("lick: chdir: %s", uv_strerror(errno));
    u3_king_bail();
  }

  strcat(por_c, URB_DEV_PATH);
  strcat(por_c, gen_u->nam_c);

  if ( c3y != _lick_mkdirp(por_c) ) {
    u3l_log("lick: mkdir %s: %s", por_c, strerror(errno));
    goto _lick_sock_err_chdir;
  }

  if ( 0 != unlink(por_c) && errno != ENOENT ) {
    u3l_log("lick: unlink: %s", uv_strerror(errno));
    goto _lick_sock_err_chdir;
  }
  if ( 0 != (err_i = uv_pipe_init(u3L, &san_u->pyp_u, 0)) ) {
    u3l_log("lick: uv_pipe_init: %s", uv_strerror(err_i));
    goto _lick_sock_err_chdir;
  }

  if ( 0 != (err_i = uv_pipe_bind(&san_u->pyp_u, por_c)) ) {
    u3l_log("lick: uv_pipe_bind: %s", uv_strerror(err_i));
    u3l_log("lick: uv_pipe_bind: %s", por_c);
    goto _lick_sock_err_chdir;
  }
  if ( 0 != (err_i = uv_listen((uv_stream_t*)&san_u->pyp_u, 0,
                               _lick_sock_cb)) ) {
    u3l_log("lick: uv_listen: %s", uv_strerror(err_i));
    goto _lick_sock_err_unlink;
  }
  if ( 0 != chdir(pax_c) ) {
    u3l_log("lick: chdir: %s", uv_strerror(errno));
    goto _lick_sock_err_close;
  }
  return;

_lick_sock_err_close:
  uv_close((uv_handle_t*)&san_u->pyp_u, _lick_close_cb);
_lick_sock_err_unlink:
  if ( 0 != unlink(gen_u->nam_c) ) {
    u3l_log("lick: unlink: %s", uv_strerror(errno));
  }
_lick_sock_err_chdir:
  if ( 0 != chdir(pax_c) ) {
    u3l_log("lick: chdir: %s", uv_strerror(errno));
  }
  u3_king_bail();
}

/* u3_lick_ef_shut(): Close an IPC port
*/
static void
_lick_ef_shut(u3_lick* lic_u, u3_noun nam)
{
  c3_c* nam_c = _lick_it_path(nam);

  u3_port* cur_u = lic_u->gen_u;
  u3_port* las_u = NULL;

  while ( NULL != cur_u ) {
    if ( 0 == strcmp(cur_u->nam_c, nam_c) ) {
      cur_u->liv_o = c3n;
      _lick_close_sock(cur_u->san_u);
      if( las_u == NULL ) {
        lic_u->gen_u = cur_u->nex_u;
      }
      else {
        las_u->nex_u = cur_u->nex_u;
      }
      c3_free(cur_u);
      return;
    }
    las_u = cur_u;
    cur_u = cur_u->nex_u;
  }
  // XX We should delete empty folders in the pier/.urb/dev path
}


/* u3_lick_ef_spin(): Open an IPC port
*/
static void
_lick_ef_spin(u3_lick* lic_u, u3_noun nam)
{
  c3_c* nam_c    = _lick_it_path(nam);
  u3_port* las_u = lic_u->gen_u;

  while ( NULL != las_u ) {
    if( 0 == strcmp(las_u->nam_c, nam_c) ) {
      c3_free(nam_c);
      return;
    }
    las_u = las_u->nex_u;
  }
  u3_port* gen_u      = c3_calloc(sizeof(*gen_u));
  gen_u->san_u        = c3_calloc(sizeof(*gen_u->san_u));

  gen_u->lic_u        = lic_u;
  gen_u->san_u->gen_u = gen_u;
  gen_u->nam_c        = nam_c;
  gen_u->con_o        = c3n;
  gen_u->liv_o        = c3y;

  _lick_init_sock(gen_u->san_u);
  gen_u ->nex_u = lic_u->gen_u;
  lic_u->gen_u = gen_u;
}

/* _lick_ef_spit(): spit effects to port
*/
static void
_lick_ef_spit(u3_lick* lic_u, u3_noun nam, u3_noun dat)
{
  c3_c*    nam_c = _lick_it_path(nam);
  u3_port* gen_u = NULL;
  u3_port* cur_u = lic_u->gen_u;
  while (cur_u != NULL){
    if( 0 == strcmp(cur_u->nam_c, nam_c) ) {
      gen_u = cur_u;
      break;
    }
    cur_u = cur_u->nex_u;
  }
  if ( NULL == gen_u ) {
    u3l_log("lick: spit: gen %s not found", nam_c);
    u3z(dat);
    return;
  }

  if( c3y == gen_u->con_o ) {
    _lick_send_noun(gen_u->san_u->can_u, dat);
  }
  else {
    u3_noun dev, wir, cad, mar;
    u3z(dat);
    wir =  u3nc(c3__lick, u3_nul);
    dev = _lick_string_to_path(gen_u->nam_c+1);
    mar = u3i_string("error");
    dat = u3i_string("not connected");
    cad = u3nq(c3__soak, dev, mar, dat);
    u3_auto_peer(
      u3_auto_plan(&gen_u->lic_u->car_u, u3_ovum_init(0, c3__l, wir, cad)),
      0, 0, 0);
  }
}


/* _lick_io_kick(): apply effects.
*/
static c3_o
_lick_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_lick* lic_u = (u3_lick*)car_u;

  u3_noun tag, i_wir, par;
  u3_noun nam, dat, tmp;
  c3_o ret_o, dev_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3n == u3r_cell(cad, &tag, &tmp))
     || (c3__lick != i_wir) )
  {
    ret_o = c3n;
  }
  else {
    if ( (c3__spin == tag) ) {
      _lick_ef_spin(lic_u, u3k(tmp)); // execute spin command
      ret_o = c3y;
    }
    else if ( (c3__shut == tag) ) {
      _lick_ef_shut(lic_u, u3k(tmp)); // execute shut command
      ret_o = c3y;
    }
    else if ( c3__spit == tag ) {
      if ( c3y == u3r_cell(tmp, &nam, &dat) ) {
        _lick_ef_spit(lic_u, u3k(nam), u3k(dat));
      }
      ret_o = c3y;
    }
    else {
      ret_o = c3n;
    }
  }

  u3z(wir);
  u3z(cad);
  return ret_o;
}

/* _lick_born_news(): initialization complete on %born.
*/
static void
_lick_born_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3_auto* car_u = egg_u->car_u;

  if ( u3_ovum_done == new_e ) {
    car_u->liv_o = c3y;
  }
}

/* _lick_born_bail(): %born is essential, retry failures.
*/
static void
_lick_born_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3l_log("lick: %%born failure;");
  u3z(lud);
  u3_ovum_free(egg_u);
}

/* _lick_io_talk(): notify %lick that we're live
 *                  pass it a list of device names
*/
static void
_lick_io_talk(u3_auto* car_u)
{
  u3_lick* lic_u = (u3_lick*)car_u;
  u3_noun    wir = u3nc(c3__lick, u3_nul);
  u3_noun    cad = u3nc(c3__born, u3_nul);

  u3_auto_peer(
    u3_auto_plan(car_u, u3_ovum_init(0, c3__l, wir, cad)),
    0,
    _lick_born_news,
    _lick_born_bail);

  car_u->liv_o = c3y;
}

/* _lick_io_exit(): close all open device files and exit
*/
static void
_lick_io_exit(u3_auto* car_u)
{
  u3_lick* lic_u = (u3_lick*)car_u;
  u3_port* cur_u = lic_u->gen_u;
  u3_port* nex_u;
  while ( NULL != cur_u ) {
    _lick_close_sock(cur_u->san_u);
    nex_u = cur_u->nex_u;
    c3_free(cur_u);
    cur_u = nex_u;
  }

  u3s_cue_xeno_done(lic_u->sil_u);
  c3_free(lic_u);
}


/* u3_lick(): initialize hardware control vane.
*/
u3_auto*
u3_lick_io_init(u3_pier* pir_u)
{
  u3_lick* lic_u = c3_calloc(sizeof(*lic_u));

  c3_c pax_c[2048] = "";
  struct stat st = {0};
  strcat(pax_c, u3_Host.dir_c);
  strcat(pax_c, URB_DEV_PATH);

  if ( -1 == stat(pax_c, &st) ) {
    u3l_log("lick: init mkdir %s",pax_c );
    if ( mkdir(pax_c, 0700) && (errno != EEXIST) ) {
      u3l_log("lick: cannot make directory");
      u3_king_bail();
    }
  }

  lic_u->sil_u = u3s_cue_xeno_init();
  lic_u->fod_c = strdup(pax_c);

  u3_auto* car_u = &lic_u->car_u;
  car_u->nam_m = c3__lick;
  car_u->liv_o = c3n;
  car_u->io.talk_f = _lick_io_talk;
  car_u->io.kick_f = _lick_io_kick;
  car_u->io.exit_f = _lick_io_exit;

  return car_u;
}
