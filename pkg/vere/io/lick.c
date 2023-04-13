/// @file

#include "vere.h"
#include <sys/stat.h>
#include <sys/types.h>
#include "noun.h"

/* u3_device: description of a device
 */
  typedef struct _u3_agent {
    c3_c*              nam_c;            //  name of device
    c3_c*              ver_c;            //  name of device
    uv_pipe_t          pyp_u;            //  stream handler
    struct _u3_agent*  nex_u;            //  next pointer
  } u3_agent;

/* u3_lick: a list of devices
*/
  typedef struct _u3_lick {
    u3_auto            car_u;            //  driver
    c3_c*              fod_c;            //  IPC folder location
    u3_cue_xeno*       sil_u;            //  cue handle
    u3_agent*          gen_u;            //  agent list
  } u3_lick;

static const c3_c URB_DEV_PATH[] = "/.urb/dev/";

/* _lick_close_cb(): socket close callback.
*/
static void
_lick_close_cb(uv_handle_t* had_u)
{
  c3_free(had_u);
}


/* _lick_sock_cb(): socket connection callback.
*/
static void
_lick_sock_cb(uv_stream_t* sem_u, c3_i tas_i)
{
/*  u3_shan*  san_u = (u3_shan*)sem_u;
  u3_lick*  con_u = san_u->con_u;
  u3_chan*  can_u;
  c3_i      err_i;

  can_u = c3_calloc(sizeof(u3_chan));
  can_u->mor_u.ptr_v = can_u;
  can_u->mor_u.pok_f = _lick_moor_poke;
  can_u->mor_u.bal_f = _lick_moor_bail;
  can_u->coq_l = san_u->nex_l++;
  can_u->san_u = san_u;
  err_i = uv_timer_init(u3L, &can_u->mor_u.tim_u);
  c3_assert(!err_i);
  err_i = uv_pipe_init(u3L, &can_u->mor_u.pyp_u, 0);
  c3_assert(!err_i);
  err_i = uv_accept(sem_u, (uv_stream_t*)&can_u->mor_u.pyp_u);
  c3_assert(!err_i);
  u3_newt_read((u3_moat*)&can_u->mor_u);
  can_u->mor_u.nex_u = (u3_moor*)san_u->can_u;
  san_u->can_u = can_u;
  */
}

/* _lick_close_sock():  close an agent's socket
*/
static void
_lick_close_sock(u3_agent* gen_u)
{
  c3_c* pax_c = u3_Host.dir_c;
  c3_w len_w = strlen(pax_c) + 2 + sizeof(URB_DEV_PATH) + strlen(gen_u->nam_c);
  c3_c* paf_c = c3_malloc(len_w);
  c3_i  wit_i;
  wit_i = snprintf(paf_c, len_w, "%s/%s/%s", pax_c, URB_DEV_PATH, gen_u->nam_c);

  u3l_log("lick: closing %s/%s/%s",pax_c, URB_DEV_PATH, gen_u->nam_c);

  c3_assert(wit_i > 0);
  c3_assert(len_w == (c3_w)wit_i + 1);

  if ( 0 != unlink(paf_c) ) {
    if ( ENOENT != errno ) {
      u3l_log("lick: failed to unlink socket: %s", uv_strerror(errno));
    }
  }
  else {
    // u3l_log("lick: unlinked %s", paf_c);
  }
  uv_close((uv_handle_t*)&gen_u->pyp_u, _lick_close_cb);
  c3_free(paf_c);
}


/* _lick_init_sock(): initialize socket device.
*/
static void
_lick_init_sock(u3_agent* gen_u)
{
  
  //  the full socket path is limited to about 108 characters,
  //  and we want it to be relative to the pier. save our current
  //  path, chdir to the pier, open the socket at the desired
  //  path, then chdir back. hopefully there aren't any threads.
  //
  c3_c pax_c[2048];
  c3_i err_i;
  c3_c por_c[2048] = ".";

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

  if ( 0 != unlink(gen_u->nam_c) && errno != ENOENT ) {
    u3l_log("lick: unlink: %s", uv_strerror(errno));
    goto _lick_sock_err_chdir;
  }
  if ( 0 != (err_i = uv_pipe_init(u3L, &gen_u->pyp_u, 0)) ) {
    u3l_log("lick: uv_pipe_init: %s", uv_strerror(err_i));
    goto _lick_sock_err_chdir;
  }

  if ( 0 != (err_i = uv_pipe_bind(&gen_u->pyp_u, por_c)) ) {
    u3l_log("lick: uv_pipe_bind: %s", uv_strerror(err_i));
    u3l_log("lick: uv_pipe_bind: %s", por_c);
    goto _lick_sock_err_chdir;
  }
  if ( 0 != (err_i = uv_listen((uv_stream_t*)&gen_u->pyp_u, 0,
                               _lick_sock_cb)) ) {
    u3l_log("lick: uv_listen: %s", uv_strerror(err_i));
    goto _lick_sock_err_unlink;
  }
  if ( 0 != chdir(pax_c) ) {
    u3l_log("lick: chdir: %s", uv_strerror(errno));
    goto _lick_sock_err_close;
  }
  u3l_log("lick: listening on %s/%s", u3_Host.dir_c, por_c);
  return;

_lick_sock_err_close:
  uv_close((uv_handle_t*)&gen_u->pyp_u, _lick_close_cb);
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


/* u3_lick_ef_book(): Open an IPC port
*/
static void
_lick_ef_book(u3_lick* lic_u, u3_noun wir_i, 
    u3_noun nam, u3_noun ver)
{

  u3l_log("lick book: %s %s", u3r_string(nam), u3r_string(ver));

  u3_agent* gen_u = c3_calloc(sizeof(*gen_u));
  gen_u->nam_c = u3r_string(nam);
  gen_u->ver_c = u3r_string(ver);

  u3_agent* hed_u = lic_u->gen_u;
  
  if( NULL == lic_u->gen_u )
  {
    _lick_init_sock(gen_u);
    lic_u->gen_u = gen_u;
  }
  else
  {
    u3_agent* las_u = lic_u->gen_u;

    while ( NULL != las_u->nex_u )
    {
      if( 0 == strcmp(las_u->nam_c, gen_u->nam_c) )
      {
        las_u->ver_c = gen_u->ver_c;
        return;
      }
      las_u = las_u->nex_u;
    }

    if( 0 == strcmp(las_u->nam_c, gen_u->nam_c) )
      las_u->ver_c = gen_u->ver_c;
    else
    {
      _lick_init_sock(gen_u);
      las_u->nex_u = gen_u;
    }
  }

}

/* _lick_io_kick(): apply effects.
*/
static c3_o
_lick_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_lick* lic_u = (u3_lick*)car_u;

  u3_noun tag, i_wir, par;
  u3_noun nam, ver, tmp;
  c3_o ret_o, dev_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3n == u3r_cell(cad, &tag, &tmp))
     || (c3__lick != i_wir) )
  {
    return  c3n;
  }
  else {
    if ( (c3__book == tag) ){
      if( c3y == u3r_cell(tmp, &nam, &ver) )
      {
        _lick_ef_book(lic_u, wir, nam, ver); // execute read command
        ret_o = c3y;
      } else { ret_o = c3n; }

    } else if ( (c3__read == tag) )
    {
      u3l_log("lick read");
       ret_o=c3y;
      /*if( c3y == u3r_trel(tmp, &wut, &cmd, &cnt) )
      {
        _lick_ef_read(loc_u, wir, dev_d, wut, cmd, cnt); // execute read command
        ret_o = c3y;
      } else { ret_o = c3n; }*/
    } else if ( c3__rite == tag )
    {
      u3l_log("lick: rite ");
       ret_o=c3y;
      /*if( c3y == u3r_qual(tmp, &wut, &cmd, &dat, &cnt) )
      {
        _lick_ef_rite(loc_u, wir, dev_d, wut, cmd, dat, cnt); // execute write command
        ret_o = c3y;
      } else { ret_o = c3n; }*/
    }
    else {
      ret_o = c3n;
    }
  }

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
  u3l_log("lick born");
  u3_noun  wir = u3nc(c3__lick, u3_nul);
  u3_noun  cad = u3nc(c3__born, u3_nul);

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
  
  u3_lick*          lic_u = (u3_lick*)car_u;
  c3_c*             pax_c = u3_Host.dir_c;

  u3_agent*         cur_u=lic_u->gen_u;
  u3_agent*         nex_u;
  while( NULL != cur_u )
  {
    _lick_close_sock(cur_u);
    nex_u = cur_u->nex_u;
    c3_free(cur_u);
    cur_u = nex_u;
  }

  //u3s_cue_xeno_done(lic_u->sil_u);
  c3_free(lic_u);
}


/* u3_lick(): initialize hardware control vane.
*/
u3_auto*
u3_lick_io_init(u3_pier* pir_u)
{
  u3_lick* lic_u = c3_calloc(sizeof(*lic_u));

  c3_c pax_c[2048];
  struct stat st = {0};
  strcat(pax_c, u3_Host.dir_c);
  strcat(pax_c, URB_DEV_PATH);

  if( -1 == stat(pax_c, &st) ) {
    u3l_log("lick init %s", lic_u->fod_c);
    mkdir(pax_c, 0700);
  }

  lic_u->fod_c = c3_calloc(strlen(pax_c));
  strcpy(lic_u->fod_c, pax_c);


  u3_auto* car_u = &lic_u->car_u;
  car_u->nam_m = c3__lick;

  car_u->liv_o = c3n;
  car_u->io.talk_f = _lick_io_talk;
  car_u->io.kick_f = _lick_io_kick;
  car_u->io.exit_f = _lick_io_exit;

  return car_u;
}
