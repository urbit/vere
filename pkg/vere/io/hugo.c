/// @file

#include "vere.h"

#include "noun.h"

/* u3_hugo a list of devices
*/
  typedef struct _u3_hugo {
    u3_auto     car_u;           //  driver
    // XX pretty sure you don't need any runtime state - the state
    //    is either in arvo or in the directory, and teh runtime is
    //    just shuttling those back and forth    
    //
    // u3_umon     mon_u;           //  mount point XX should this be a pointer?
    c3_c*       pax_c;           //  pier directory
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
  
  //  XX add retries?
  //

  u3_auto_bail_slog(egg_u, lud);
  u3_ovum_free(egg_u);

  u3l_log("hugo: initialization failed");

  // u3_pier_bail(car_u->pir_u);
}

/* _hugo_io_talk(): notify %hugo that we're live
*/
static void
_hugo_io_talk(u3_auto* car_u)
{
  u3_hugo* hug_u = (u3_hugo*)car_u;

  u3_noun wir = u3nc(c3__hugo, u3_nul);
  u3_noun cad = u3nc(c3__born, u3_nul);

  u3_auto_peer(
    u3_auto_plan(car_u, u3_ovum_init(0, c3__h, wir, cad)),
    0,
    _hugo_born_news,
    _hugo_born_bail);
}

/* u3_behn_ef_doze(): set or cancel timer
*/
static void
_hugo_ef_grab(u3_hugo* hug_u)
{
  if ( c3n == hug_u->car_u.liv_o ) {
    hug_u->car_u.liv_o = c3y; // XX what is this doing
  }

  // DIR* rid_u = c3_opendir("%s/.urb/get", u3_Host.dir_c);
  // if ( !rid_u ) {
  //   u3l_log("error opening pier directory: %s: %s",
  //           mon_u->dir_u.pax_c, strerror(errno));
  //   return;
  // }
  
  // u3_noun  can = u3_nul;
  // u3_unod* nod_u;
  // for ( nod_u = mon_u->dir_u.kid_u; nod_u; nod_u = nod_u->nex_u ) {
  //   can = u3kb_weld(_unix_update_node(unx_u, nod_u), can);
  // }
  //  see _unix_initial_update_dir - creates a (list [path mim=(unit mime)])
  //      we need this but it's a (unit octs)

  //  my job is to read a directory in unix and spit out a (list [path octs])
  //  package that into a card
  {
    u3_noun wir = u3nc(c3__hugo, u3_nul);
    u3_noun cad = u3nc(c3__fill, u3nc(u3_nul, u3_nul));
    u3_auto_plan(&hug_u->car_u, u3_ovum_init(0, c3__h, wir, cad));
  }
}

/* _hugo_io_kick(): apply effects.
*/
static c3_o
_hugo_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_hugo* hug_u = (u3_hugo*)car_u;

  u3_noun tag, dat, i_wir;
  c3_o ret_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3n == u3r_cell(cad, &tag, &dat))
     || (c3__hugo != i_wir) //  /hugo 
     || (c3__grab != tag)   //  %grab
     || (c3y != dat) )      // ~
  {
    ret_o = c3n;
  }
  else {
    ret_o = c3y;
    _hugo_ef_grab(hug_u);
  }

  u3z(wir); u3z(cad);
  return ret_o;
}

/* _hugo_io_exit(): terminate file system.
*/
static void
_hugo_io_exit(u3_auto* car_u)
{
  u3_hugo* hug_u = (u3_hugo*)car_u;
  // clean state up, free any pointers, dealloc stuff, close things down, etc.
}

/* u3_hugo(): initialize file syncing vane
*/
u3_auto*
u3_hugo_io_init(u3_pier* pir_u)
{
  u3_hugo* hug_u = c3_calloc(sizeof(*hug_u));
  hug_u->pax_c = strdup(pir_u->pax_c);

  u3_auto* car_u = &hug_u->car_u;
  car_u->nam_m = c3__hugo;
  
  car_u->liv_o = c3n;
  car_u->io.talk_f = _hugo_io_talk;
  car_u->io.kick_f = _hugo_io_kick;
  car_u->io.exit_f = _hugo_io_exit;
  return car_u;
}