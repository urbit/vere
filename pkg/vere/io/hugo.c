/// @file

#include "vere.h"

#include "noun.h"

/* u3_hugo a list of devices
*/
  typedef struct _u3_hugo {
    u3_auto            car_u;           //  driver
  } u3_hugo;

/* _hugo_born_news(): initialization complete on %born.
*/
static void
_hugo_born_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3_auto* car_u = egg_u->car_u;

  if ( u3_ovum_done == new_e ) {
    car_u->liv_o = c3y;
  }
}

/* _hugo_born_bail(): %born is essential, retry failures.
*/
static void
_hugo_born_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3_auto* car_u = egg_u->car_u;
}

/* _hugo_io_talk(): notify %hugo that we're live
*/
static void
_hugo_io_talk(u3_auto* car_u)
{
  u3_hugo* teh_u = (u3_hugo*)car_u;

  //  XX remove [sev_l]
  //
  u3_noun wir = u3nc(c3__hugo,
                     u3_nul);
  u3_noun cad = u3nc(c3__born, u3_nul);

  u3_auto_peer(
    u3_auto_plan(car_u, u3_ovum_init(0, c3__b, wir, cad)),
    0,
    _hugo_born_news,
    _hugo_born_bail);
}

/* _hugo_io_kick(): apply effects.
*/
static c3_o
_hugo_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_hugo* teh_u = (u3_hugo*)car_u;

  u3_noun tag, dat, i_wir;
  c3_o ret_o;

  ret_o = c3y;

  u3z(wir); u3z(cad);
  return ret_o;
}

/* _hugo_io_exit(): terminate timer.
*/
static void
_hugo_io_exit(u3_auto* car_u)
{
  u3_hugo* hug_u = (u3_hugo*)car_u;
}

/* u3_hugo(): initialize file syncing vane
*/
u3_auto*
u3_hugo_io_init(u3_pier* pir_u)
{
  u3_hugo* hug_u = c3_calloc(sizeof(*hug_u));
  
  u3_auto* car_u = &hug_u->car_u;
  car_u->nam_m = c3__hugo;
  
  car_u->liv_o = c3n;
  car_u->io.talk_f = _hugo_io_talk;
  car_u->io.kick_f = _hugo_io_kick;
  car_u->io.exit_f = _hugo_io_exit;
  return car_u;
}