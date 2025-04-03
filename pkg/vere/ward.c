/// @file

#include "vere.h"

#include "noun.h"

//  ward: lifecycle management for common structures
//
//    should contain anything allocated in multiple modules,
//    or allocated in one and freed in another
//

/* u3_dent_init(): initialize file record.
*/
u3_dent*
u3_dent_init(const c3_c* nam_c)
{
  u3_dent *det_u = c3_malloc(sizeof(*det_u));
  det_u->nex_u   = 0;
  det_u->nam_c   = c3_malloc(1 + strlen(nam_c));
  strcpy(det_u->nam_c, nam_c);

  return det_u;
}

/* u3_dent_free(): dispose file record.
*/
void
u3_dent_free(u3_dent *det_u)
{
  c3_free(det_u->nam_c);
  c3_free(det_u);
}

/* u3_dire_init(): initialize directory record.
*/
u3_dire*
u3_dire_init(const c3_c* pax_c)
{
  u3_dire *dir_u = c3_malloc(sizeof *dir_u);
  dir_u->all_u = 0;
  dir_u->dil_u = 0;
  dir_u->pax_c = c3_malloc(1 + strlen(pax_c));
  strcpy(dir_u->pax_c, pax_c);

  return dir_u;
}

/* u3_dire_free(): dispose directory record.
*/
void
u3_dire_free(u3_dire *dir_u)
{
  {
    u3_dent *det_u = dir_u->all_u;
    u3_dent *nex_u;

    while ( det_u ) {
      nex_u = det_u->nex_u;
      u3_dent_free(det_u);
      det_u = nex_u;
    }
  }

  {
    u3_dent *det_u = dir_u->dil_u;
    u3_dent *nex_u;

    while ( det_u ) {
      nex_u = det_u->nex_u;
      u3_dent_free(det_u);
      det_u = nex_u;
    }
  }

  c3_free(dir_u->pax_c);
  c3_free(dir_u);
}

/* u3_fact_init(): initialize completed event.
*/
u3_fact*
u3_fact_init(c3_d eve_d, c3_l mug_l, u3_noun job)
{
  u3_fact *tac_u = c3_malloc(sizeof(*tac_u));
  tac_u->eve_d = eve_d;
  tac_u->mug_l = mug_l;
  tac_u->nex_u = 0;
  tac_u->job   = job;

  return tac_u;
}

/* u3_fact_free(): dispose completed event.
*/
void
u3_fact_free(u3_fact *tac_u)
{
  u3z(tac_u->job);
  c3_free(tac_u);
}

/* u3_gift_init(): initialize effect list.
*/
u3_gift*
u3_gift_init(c3_d eve_d, u3_noun act)
{
  u3_gift *gif_u = c3_malloc(sizeof(*gif_u));
  gif_u->eve_d = eve_d;
  gif_u->nex_u = 0;
  gif_u->act   = act;

  return gif_u;
}

/* u3_gift_free(): dispose effect list.
*/
void
u3_gift_free(u3_gift *gif_u)
{
  u3z(gif_u->act);
  c3_free(gif_u);
}

/* u3_ovum_init: initialize an unlinked potential event
*/
u3_ovum*
u3_ovum_init(c3_w     mil_w,
             u3_noun    tar,
             u3_noun    wir,
             u3_noun    cad)
{
  u3_ovum* egg_u = c3_malloc(sizeof(*egg_u));
  egg_u->car_u = 0;
  egg_u->try_w = 0;
  egg_u->ptr_v = 0;
  egg_u->mil_w = mil_w;
  egg_u->tar   = tar;
  egg_u->wir   = wir;
  egg_u->cad   = cad;

  egg_u->pre_u = egg_u->nex_u = 0;

  egg_u->cb_u.news_f = 0;
  egg_u->cb_u.bail_f = 0;

  //  spinner defaults
  //
  egg_u->pin_u.lab   = u3k(u3h(wir));
  egg_u->pin_u.del_o = c3y;

  return egg_u;
}

/* u3_ovum_free: dispose an unlinked potential event
*/
void
u3_ovum_free(u3_ovum *egg_u)
{
  u3z(egg_u->pin_u.lab);
  u3z(egg_u->tar);
  u3z(egg_u->wir);
  u3z(egg_u->cad);

  c3_free(egg_u);
}

/* u3_pico_init(): initialize a scry request struct
*/
u3_pico*
u3_pico_init()
{
  u3_pico* pic_u = c3_calloc(sizeof(*pic_u));
  return pic_u;
}

/* u3_pico_free(): dispose a scry request struct
*/
void
u3_pico_free(u3_pico* pic_u)
{
  u3z(pic_u->gan);

  switch ( pic_u->typ_e ) {
    default: u3_assert(0);

    case u3_pico_full: {
      u3z(pic_u->ful);
    } break;

    case u3_pico_once: {
      u3z(pic_u->las_u.des);
      u3z(pic_u->las_u.pax);
    } break;
  }

  c3_free(pic_u);
}

/* u3_mcut_char(): measure/cut character.
*/
c3_w
u3_mcut_char(c3_c* buf_c, c3_w len_w, c3_c chr_c)
{
  if ( buf_c ) {
    buf_c[len_w] = chr_c;
  }
  return len_w + 1;
}

/* u3_mcut_cord(): measure/cut cord.
*/
c3_w
u3_mcut_cord(c3_c* buf_c, c3_w len_w, u3_noun san)
{
  c3_w ten_w = u3r_met(3, san);

  if ( buf_c ) {
    u3r_bytes(0, ten_w, (c3_y *)(buf_c + len_w), san);
  }
  u3z(san);
  return (len_w + ten_w);
}

/* u3_mcut_path(): measure/cut cord list.
*/
c3_w
u3_mcut_path(c3_c* buf_c, c3_w len_w, c3_c sep_c, u3_noun pax)
{
  u3_noun axp = pax;

  while ( u3_nul != axp ) {
    u3_noun h_axp = u3h(axp);

    len_w = u3_mcut_cord(buf_c, len_w, u3k(h_axp));
    axp = u3t(axp);

    if ( u3_nul != axp ) {
      len_w = u3_mcut_char(buf_c, len_w, sep_c);
    }
  }
  u3z(pax);
  return len_w;
}

/* u3_mcut_host(): measure/cut host.
*/
c3_w
u3_mcut_host(c3_c* buf_c, c3_w len_w, u3_noun hot)
{
  len_w = u3_mcut_path(buf_c, len_w, '.', u3kb_flop(u3k(hot)));
  u3z(hot);
  return len_w;
}

/* u3_mcut_hosts(): measure/cut host list.
*/
c3_w
u3_mcut_hosts(c3_c* buf_c, c3_w len_w, u3_noun hot)
{
  c3_c** dst_c = NULL;
  hot = u3kdi_tap(u3kdb_gas(u3_nul, hot));
  c3_w hots_w = u3qb_lent(hot);
  if (hots_w == 0) return len_w;
  if (buf_c) {
    dst_c = (void*)(buf_c + len_w);
    dst_c[hots_w] = 0;
  }
  len_w += (hots_w + 1) * sizeof(c3_c*);

  u3_noun oth = hot;
  c3_w i = 0;
  while ( u3_nul != oth ) { 
    if (dst_c) dst_c[i] = (void*)(buf_c + len_w);
    u3_noun h_oth = u3h(oth);

    len_w = u3_mcut_path(buf_c, len_w, '.', u3kb_flop(u3k(h_oth)));
    len_w = u3_mcut_char(buf_c, len_w, '.');
    len_w = u3_mcut_char(buf_c, len_w, '\0');
    oth = u3t(oth);

    i++;
  }
  u3z(hot);
  return len_w;
}

/* u3_mcut_str(): measure/cut string.
*/
c3_w
u3_mcut_str(c3_c* buf_c, c3_w len_w, c3_c* str)
{
  c3_w ten_w = strlen(str);

  if ( buf_c ) {
    memcpy((c3_y*)(buf_c + len_w), str, ten_w);
  }
  return (len_w + ten_w);
}

/* u3_mpef_turf(): measure/cut string list, prefixing.
*/
c3_w
u3_mpef_turfs(c3_c* buf_c, c3_w len_w, c3_c* pef_c, c3_c** tuf_c)
{
  c3_c** dst_c = NULL;

  c3_w tufs_w = 0;
  {
    c3_c** uft_c = tuf_c;
    while (NULL != *uft_c) {
      tufs_w++;
      uft_c++;
    }
  }
  
  if (buf_c) {
    dst_c = (void*)(buf_c + len_w);
    dst_c[tufs_w] = 0;
  }
  len_w += (tufs_w + 1) * sizeof(c3_c*);

  c3_w i = 0;
  while ( NULL != tuf_c[i] ) { 
    if (dst_c) dst_c[i] = (void*)(buf_c + len_w);
    len_w = u3_mcut_str(buf_c, len_w, pef_c);
    len_w = u3_mcut_char(buf_c, len_w, '.');
    len_w = u3_mcut_str(buf_c, len_w, tuf_c[i]);
    len_w = u3_mcut_char(buf_c, len_w, '\0');

    i++;
  }
  return len_w;
}

