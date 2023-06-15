/// @file

#include "vere.h"

#include "noun.h"

/* u3_grab a file that arvo is aware of
*/
  typedef struct _u3_grab {
    u3_noun pax_c;  //  file path
    u3_noun haz_c;  //  file hash
  } u3_grab;

/* u3_hugo driver state
*/
  typedef struct _u3_hugo {
    u3_auto     car_u;           //  driver
    c3_c*       pax_c;           //  pier directory // XX I don't think you need this
  } u3_hugo;

// HEPERS

/* _hugo_it_path(): path to string
*/
static c3_c*
_hugo_it_path(u3_noun pax)
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

/* u3_behn_ef_grab(): set or cancel timer
*/
static void
_hugo_ef_grab(u3_hugo* hug_u, u3_noun lis)
{
  // XX implementation is wrong
  // 1. turn lis into a hash table
  // 2. iterate through all unix files in /put/
  //    - if the hashes are the same, continue
  //    - if they are different, or it doesn't exist in the hashtable then add it to var
  //    - OOPS because of deletes we have to iterate again LOL
  // iterate through all the files in the unix directory

  if ( c3n == hug_u->car_u.liv_o ) {
    hug_u->car_u.liv_o = c3y; // XX what is this doing
  }

  c3_c* hos_c;
  sprintf(hos_c, "%s/.urb/get", u3_Host.dir_c);
  DIR* rid_u = c3_opendir(hos_c);
  if ( !rid_u ) {
    u3l_log("error opening pier directory: %s/.urb/get: %s",
            u3_Host.dir_c, strerror(errno));
    return;
  }

  while ( u3_nul != lis ) {
    u3_noun hed = u3h(lis);
    lis = u3t(lis);

    u3_noun pax = u3h(hed);
    u3_noun hax = u3t(hed);
    u3_noun del = u3i_list(u3_nul); // XX might be bad
    u3_noun var = u3i_list(u3_nul);

    c3_c* pax_c = _hugo_it_path(pax);

    struct stat buf_u;
    c3_i  fid_i = c3_open(pax_c, O_RDONLY, 0644);
    c3_ws len_ws, red_ws;
    c3_w  old_w;
    c3_y* old_y;
    
    u3_noun del;  // deletes array
    u3_noun var;  // changes array

    if ( fid_i < 0 || fstat(fid_i, &buf_u) < 0 ) {
      if ( ENOENT == errno ) {
        del = u3i_list(del, u3k(pax)); //  put in deletes here
        continue;
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
    c3_w hax_w = u3r_word(0, hax);

    if ( old_w == hax_w ) continue;

    // XX we don't cover new files

  }
  // iterate through
  
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
    //  XX  probably should be a switch here
    ret_o = c3y;
    _hugo_ef_grab(hug_u, u3k(dat)); // XX maybe dont ref count this IDK
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