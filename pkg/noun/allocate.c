/// @file

#include "allocate.h"

#include "hashtable.h"
#include "log.h"
#include "manage.h"
#include "options.h"
#include "retrieve.h"
#include "trace.h"
#include "vortex.h"

#include "palloc.c"

u3_road* u3a_Road;
u3a_mark u3a_Mark;
u3a_gack u3a_Gack;
u3a_hunk_dose u3a_Hunk[u3a_crag_no];

#ifdef U3_MEMORY_DEBUG
c3_w u3_Code;
#endif

c3_w u3a_to_pug(c3_w off);
c3_w u3a_to_pom(c3_w off);

void
u3a_drop(const u3a_pile* pil_u);
void*
u3a_peek(const u3a_pile* pil_u);
void*
u3a_pop(const u3a_pile* pil_u);
void*
u3a_push(const u3a_pile* pil_u);
c3_o
u3a_pile_done(const u3a_pile* pil_u);

void*
u3a_into_fn(u3_post som_p)
{
  return u3a_into(som_p);
}

u3_post
u3a_outa_fn(void* som_v)
{
  return u3a_outa(som_v);
}

u3_post
u3a_to_off_fn(u3_noun som)
{
  return u3a_to_off(som);
}

u3a_noun*
u3a_to_ptr_fn(u3_noun som)
{
  return u3a_to_ptr(som);
}

u3_noun
u3a_head(u3_noun som)
{
  return u3h(som);
}

u3_noun
u3a_tail(u3_noun som)
{
  return u3t(som);
}

void
u3a_post_info(u3_post som_p)
{
  _post_status(som_p);
}

void
u3a_init_once(void)
{
  _init_once();
}

void
u3a_init_heap(void)
{
  _init_heap();
}

void
u3a_drop_heap(u3_post cap_p, u3_post ear_p)
{
#ifdef ASAN_ENABLED
  if ( cap_p > ear_p ) {  // in north, drop inner south
    _drop(ear_p, cap_p - ear_p);
  }
  else {                  // in south, drop inner north
    _drop(cap_p, ear_p - cap_p);
  }
#else
  (void)cap_p;
  (void)ear_p;
#endif
}

void
u3a_mark_init(void)
{
  c3_w bit_w = (u3R->hep.len_w + 31) >> 5;

  u3a_Mark.bit_w = c3_calloc(sizeof(c3_w) * bit_w);
  u3a_Mark.siz_w = u3R->hep.siz_w * 2;
  u3a_Mark.len_w = u3R->hep.len_w;
  u3a_Mark.buf_w = c3_calloc(sizeof(c3_w) * u3a_Mark.siz_w);

  memset(u3a_Mark.wee_w, 0, sizeof(c3_w) * u3a_crag_no);
}

void
u3a_mark_done(void)
{
  c3_free(u3a_Mark.bit_w);
  c3_free(u3a_Mark.buf_w);
  memset(&u3a_Mark, 0, sizeof(u3a_Mark));
}

void*
u3a_mark_alloc(c3_w len_w) // words
{
  void* ptr_v;

  if ( len_w > (u3a_Mark.siz_w - u3a_Mark.len_w) ) {
    u3a_Mark.siz_w += c3_max(u3a_Mark.len_w, len_w);
    u3a_Mark.buf_w  = c3_realloc(u3a_Mark.buf_w, sizeof(c3_w) * u3a_Mark.siz_w);
  }

  ptr_v = &(u3a_Mark.buf_w[u3a_Mark.len_w]);
  u3a_Mark.len_w += len_w;

  return ptr_v;
}

void
u3a_pack_init(void)
{
  c3_w bit_w = (u3R->hep.len_w + 31) >> 5;
  u3a_Gack.bit_w = c3_calloc(sizeof(c3_w) * bit_w);
  u3a_Gack.pap_w = c3_calloc(sizeof(c3_w) * bit_w);
  u3a_Gack.pum_w = c3_calloc(sizeof(c3_w) * bit_w);

  u3a_Gack.siz_w = u3R->hep.siz_w * 2;
  u3a_Gack.len_w = u3R->hep.len_w;
  u3a_Gack.buf_w = c3_calloc(sizeof(c3_w) * u3a_Gack.siz_w);
}

void*
u3a_pack_alloc(c3_w len_w) // words
{
  void* ptr_v;

  if ( len_w > (u3a_Gack.siz_w - u3a_Gack.len_w) ) {
    u3a_Gack.siz_w += c3_max(u3a_Gack.len_w, len_w);
    u3a_Gack.buf_w  = c3_realloc(u3a_Gack.buf_w, sizeof(c3_w) * u3a_Gack.siz_w);
  }

  ptr_v = &(u3a_Gack.buf_w[u3a_Gack.len_w]);
  u3a_Gack.len_w += len_w;

  return ptr_v;
}

void
u3a_pack_done(void)
{
  c3_free(u3a_Gack.pap_w);
  c3_free(u3a_Gack.pum_w);
  c3_free(u3a_Gack.buf_w);
}

/* _box_count(): adjust memory count.
*/
#ifdef  U3_CPU_DEBUG
static void
_box_count(c3_ws siz_ws)
{
  u3R->all.fre_w += siz_ws;

  {
    c3_w end_w = u3a_heap(u3R);
    c3_w all_w = (end_w - u3R->all.fre_w);

    if ( all_w > u3R->all.max_w ) {
      u3R->all.max_w = all_w;
    }
  }
}
#else
static void
_box_count(c3_ws siz_ws) { }
#endif

/* _ca_reclaim_half(): reclaim from memoization cache.
*/
static void
_ca_reclaim_half(void)
{
  //  XX u3l_log avoid here, as it can
  //  cause problems when handling errors

  if ( (0 == u3R->cax.har_p) ||
       (0 == u3to(u3h_root, u3R->cax.har_p)->use_w) )
  {
    fprintf(stderr, "allocate: reclaim: memo cache: empty\r\n");
    u3m_bail(c3__meme);
  }

#if 1
  fprintf(stderr, "allocate: reclaim: half of %d entries\r\n",
          u3to(u3h_root, u3R->cax.har_p)->use_w);

  u3h_trim_to(u3R->cax.har_p, u3to(u3h_root, u3R->cax.har_p)->use_w / 2);
#else
  /*  brutal and guaranteed effective
  */
  u3h_free(u3R->cax.har_p);
  u3R->cax.har_p = u3h_new();
#endif
}

/* u3a_walloc(): allocate storage words on hat heap.
*/
void*
u3a_walloc(c3_w len_w)
{
  return u3a_into(_imalloc(len_w));
}

/* u3a_wealloc(): realloc in words.
*/
void*
u3a_wealloc(void* lag_v, c3_w len_w)
{
  if ( !lag_v ) {
    return u3a_walloc(len_w);
  }

  return u3a_into(_irealloc(u3a_outa(lag_v), len_w));
}

/* u3a_pile_prep(): initialize stack control.
*/
void
u3a_pile_prep(u3a_pile* pil_u, c3_w len_w)
{
  //  frame size, in words
  //
  c3_w wor_w = (len_w + 3) >> 2;
  c3_o nor_o = u3a_is_north(u3R);

  pil_u->mov_ws = (c3y == nor_o) ? -wor_w :  wor_w;
  pil_u->off_ws = (c3y == nor_o) ?      0 : -wor_w;
  pil_u->top_p  = u3R->cap_p;

#ifdef U3_MEMORY_DEBUG
  pil_u->rod_u  = u3R;
#endif
}

/* u3a_wfree(): free storage.
*/
void
u3a_wfree(void* tox_v)
{
  if ( tox_v ) {
    _ifree(u3a_outa(tox_v));
  }
}

/* u3a_wtrim(): trim storage.

   old_w - old length
   len_w - new length
*/
void
u3a_wtrim(void* tox_v, c3_w old_w, c3_w len_w)
{
  // XX realloc?
}

/* u3a_calloc(): allocate and zero-initialize array
*/
void*
u3a_calloc(size_t num_i, size_t len_i)
{
  size_t byt_i = num_i * len_i;
  c3_w* out_w;

  u3_assert(byt_i / len_i == num_i);
  out_w = u3a_malloc(byt_i);
  memset(out_w, 0, byt_i);

  return out_w;
}

/* u3a_malloc(): aligned storage measured in bytes.

   Internally pads allocations to 16-byte alignment independent of DWORD
   alignment ensured for word sized allocations.

*/
void*
u3a_malloc(size_t len_i)
{
  return u3a_walloc((len_i + 3) >> 2);
}

/* u3a_celloc(): allocate a cell.
*/
c3_w*
u3a_celloc(void)
{
  u3a_cell *cel_u;
  u3_post  *cel_p;

  if ( u3R->cel.cel_p ) {
    cel_p = u3to(u3_post, u3R->cel.cel_p);

    if ( !u3R->cel.hav_w ) {
      _rake_chunks(c3_wiseof(*cel_u), (1U << u3a_page),
                   (u3R->cel.bat_w++ & 1), &u3R->cel.hav_w, cel_p);
    }

    cel_u = u3to(u3a_cell, cel_p[--u3R->cel.hav_w]);
  }
  else {
    cel_u = u3a_walloc(c3_wiseof(*cel_u));
  }

#ifdef U3_CPU_DEBUG
  u3R->pro.cel_d++;
#endif

  return (c3_w*)cel_u;
}


/* u3a_cfree(): free a cell.
*/
void
u3a_cfree(c3_w* cel_w)
{
  u3_post *cel_p;

  if ( u3R->cel.cel_p ) {
    if ( u3R->cel.hav_w < (1U << u3a_page) ) {
      cel_p = u3to(u3_post, u3R->cel.cel_p);
      cel_p[u3R->cel.hav_w++] = u3a_outa(cel_w);
      return;
    }
  }

  u3a_wfree(cel_w);
}

/* u3a_realloc(): aligned realloc in bytes.
*/
void*
u3a_realloc(void* lag_v, size_t len_i)
{
  if ( !lag_v ) {
    return u3a_malloc(len_i);
  }

  return u3a_wealloc(lag_v, (len_i + 3) >> 2);
}

/* u3a_free(): free for aligned malloc.
*/
void
u3a_free(void* tox_v)
{
  u3a_wfree((c3_w*)tox_v);
}

/* _me_wash_north(): clean up mug slots after copy.
*/
static void _me_wash_north(u3_noun dog);
static void
_me_wash_north_in(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ) return;
  if ( !_(u3a_north_is_junior(u3R, som)) ) return;

  _me_wash_north(som);
}
static void
_me_wash_north(u3_noun dog)
{
  u3_assert(c3y == u3a_is_dog(dog));
  // u3_assert(c3y == u3a_north_is_junior(u3R, dog));
  {
    u3a_noun* dog_u = u3a_to_ptr(dog);

    if ( dog_u->mug_w == 0 ) return;

    dog_u->mug_w = 0;    //  power wash
    // if ( dog_u->mug_w >> 31 ) { dog_u->mug_w = 0; }

    if ( _(u3a_is_pom(dog)) ) {
      u3a_cell* god_u = (u3a_cell *)(void *)dog_u;

      _me_wash_north_in(god_u->hed);
      _me_wash_north_in(god_u->tel);
    }
  }
}

/* _me_wash_south(): clean up mug slots after copy.
*/
static void _me_wash_south(u3_noun dog);
static void
_me_wash_south_in(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ) return;
  if ( !_(u3a_south_is_junior(u3R, som)) ) return;

  _me_wash_south(som);
}
static void
_me_wash_south(u3_noun dog)
{
  u3_assert(c3y == u3a_is_dog(dog));
  // u3_assert(c3y == u3a_south_is_junior(u3R, dog));
  {
    u3a_noun* dog_u = u3a_to_ptr(dog);

    if ( dog_u->mug_w == 0 ) return;

    dog_u->mug_w = 0;    //  power wash
    //  if ( dog_u->mug_w >> 31 ) { dog_u->mug_w = 0; }

    if ( _(u3a_is_pom(dog)) ) {
      u3a_cell* god_u = (u3a_cell *)(void *)dog_u;

      _me_wash_south_in(god_u->hed);
      _me_wash_south_in(god_u->tel);
    }
  }
}

/* u3a_wash(): wash all lazy mugs.  RETAIN.
*/
void
u3a_wash(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ) {
    return;
  }
  if ( _(u3a_is_north(u3R)) ) {
    if ( _(u3a_north_is_junior(u3R, som)) ) {
      _me_wash_north(som);
    }
  }
  else {
    if ( _(u3a_south_is_junior(u3R, som)) ) {
      _me_wash_south(som);
    }
  }
}

/* _me_gain_use(): increment use count.
*/
static void
_me_gain_use(u3_noun dog)
{
  u3a_noun* box_u = u3a_to_ptr(dog);

  if ( 0x7fffffff == box_u->use_w ) {
    u3l_log("fail in _me_gain_use");
    u3m_bail(c3__fail);
  }
  else {
    if ( box_u->use_w == 0 ) {
      u3m_bail(c3__foul);
    }
    box_u->use_w += 1;

#ifdef U3_MEMORY_DEBUG
    //  enable to (maybe) help track down leaks
    //
    // if ( u3_Code && !box_u->cod_w ) { box_u->cod_w = u3_Code; }
#endif
  }
}

#undef VERBOSE_TAKE

/* _ca_take_atom(): reallocate an indirect atom off the stack.
*/
static inline u3_atom
_ca_take_atom(u3a_atom* old_u)
{
  c3_w*     new_w = u3a_walloc(old_u->len_w + c3_wiseof(u3a_atom));
  u3a_atom* new_u = (u3a_atom*)(void *)new_w;
  u3_noun     new = u3a_to_pug(u3a_outa(new_u));

  new_u->use_w = 1;

#ifdef VERBOSE_TAKE
  u3l_log("%s: atom %p to %p", ( c3y == u3a_is_north(u3R) )
                                   ? "north"
                                   : "south",
                                   old_u,
                                   new_u);
#endif

  //  XX use memcpy?
  //
  new_u->mug_w = old_u->mug_w;
  new_u->len_w = old_u->len_w;
  {
    c3_w i_w;

    for ( i_w=0; i_w < old_u->len_w; i_w++ ) {
      new_u->buf_w[i_w] = old_u->buf_w[i_w];
    }
  }

  //  borrow mug slot to record new destination in [old_u]
  //
  old_u->mug_w = new;

  return new;
}

/* _ca_take_cell(): reallocate a cell off the stack.
*/
static inline u3_cell
_ca_take_cell(u3a_cell* old_u, u3_noun hed, u3_noun tel)
{
  c3_w*     new_w = u3a_celloc();
  u3a_cell* new_u = (u3a_cell*)(void *)new_w;
  u3_cell     new = u3a_to_pom(u3a_outa(new_u));

#ifdef VERBOSE_TAKE
  u3l_log("%s: cell %p to %p", ( c3y == u3a_is_north(u3R) )
                                   ? "north"
                                   : "south",
                                   old_u,
                                   new_u);
#endif

  new_u->use_w = 1;
  new_u->mug_w = old_u->mug_w;
  new_u->hed   = hed;
  new_u->tel   = tel;

  //  borrow mug slot to record new destination in [old_u]
  //
  old_u->mug_w = new;

  return new;
}

/* _ca_take: stack frame for recording cell travesal
**           (u3_none == hed) == head-frame
*/
typedef struct _ca_take
{
  u3_weak hed;  //  taken head
  u3_cell old;  //  old cell
} _ca_take;

/* _ca_take_next_south: take next noun, pushing cells on stack.
*/
static inline u3_noun
_ca_take_next_north(u3a_pile* pil_u, u3_noun veb)
{
  while ( 1 ) {
    //  direct atoms and senior refs are not counted.
    //
    if (  (c3y == u3a_is_cat(veb))
       || (c3y == u3a_north_is_senior(u3R, veb)) )
    {
      return veb;
    }
    //  not junior; normal (heap) refs on our road are counted.
    //
    else if ( c3n == u3a_north_is_junior(u3R, veb) ) {
      _me_gain_use(veb); // bypass branches in u3k()
      return veb;
    }
    //  junior (stack) refs are copied.
    //
    else {
      u3a_noun* veb_u = u3a_to_ptr(veb);

      //  32-bit mug_w: already copied [veb] and [mug_w] is the new ref.
      //
      if ( veb_u->mug_w >> 31 ) {
        u3_noun nov = (u3_noun)veb_u->mug_w;

        u3_assert( c3y == u3a_north_is_normal(u3R, nov) );

#ifdef VERBOSE_TAKE
        u3l_log("north: %p is already %p", veb_u, u3a_to_ptr(nov));
#endif

        _me_gain_use(nov); // bypass branches in u3k()
        return nov;
      }
      else if ( c3y == u3a_is_atom(veb) ) {
        return _ca_take_atom((u3a_atom*)veb_u);
      }
      else {
        u3a_cell* old_u = (u3a_cell*)veb_u;
        _ca_take* fam_u = u3a_push(pil_u);

        fam_u->hed = u3_none;
        fam_u->old = veb;

        veb = old_u->hed;
        continue;
      }
    }
  }
}

/* _ca_take_next_south: take next noun, pushing cells on stack.
*/
static inline u3_noun
_ca_take_next_south(u3a_pile* pil_u, u3_noun veb)
{
  while ( 1 ) {
    //  direct atoms and senior refs are not counted.
    //
    if (  (c3y == u3a_is_cat(veb))
       || (c3y == u3a_south_is_senior(u3R, veb)) )
    {
      return veb;
    }
    //  not junior; a normal pointer in our road -- refcounted
    //
    else if ( c3n == u3a_south_is_junior(u3R, veb) ) {
      _me_gain_use(veb); // bypass branches in u3k()
      return veb;
    }
    //  junior (stack) refs are copied.
    //
    else {
      u3a_noun* veb_u = u3a_to_ptr(veb);

      //  32-bit mug_w: already copied [veb] and [mug_w] is the new ref.
      //
      if ( veb_u->mug_w >> 31 ) {
        u3_noun nov = (u3_noun)veb_u->mug_w;

        u3_assert( c3y == u3a_south_is_normal(u3R, nov) );

#ifdef VERBOSE_TAKE
        u3l_log("south: %p is already %p", veb_u, u3a_to_ptr(nov));
#endif

        _me_gain_use(nov); // bypass branches in u3k()
        return nov;
      }
      else if ( c3y == u3a_is_atom(veb) ) {
        return _ca_take_atom((u3a_atom*)veb_u);
      }
      else {
        u3a_cell* old_u = (u3a_cell*)veb_u;
        _ca_take* fam_u = u3a_push(pil_u);

        fam_u->hed = u3_none;
        fam_u->old = veb;

        veb = old_u->hed;
        continue;
      }
    }
  }
}

/* _ca_take_north(): in a north road, gain, copying juniors (from stack).
*/
static u3_noun
_ca_take_north(u3_noun veb)
{
  u3_noun     pro;
  _ca_take* fam_u;
  u3a_pile  pil_u;
  u3a_pile_prep(&pil_u, sizeof(*fam_u));

  //  commence taking
  //
  pro = _ca_take_next_north(&pil_u, veb);

  //  process cell results
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
    fam_u = u3a_peek(&pil_u);

    do {
      //  head-frame: stash copy and continue into the tail
      //
      if ( u3_none == fam_u->hed ) {
        u3a_cell* old_u = u3a_to_ptr(fam_u->old);
        fam_u->hed = pro;
        pro        = _ca_take_next_north(&pil_u, old_u->tel);
        fam_u      = u3a_peek(&pil_u);
      }
      //  tail-frame: copy cell and pop the stack
      //
      else {
        u3a_cell* old_u = u3a_to_ptr(fam_u->old);
        pro   = _ca_take_cell(old_u, fam_u->hed, pro);
        fam_u = u3a_pop(&pil_u);
      }
    } while ( c3n == u3a_pile_done(&pil_u) );
  }

  return pro;
}
/* _ca_take_south(): in a south road, gain, copying juniors (from stack).
*/
static u3_noun
_ca_take_south(u3_noun veb)
{
  u3_noun     pro;
  _ca_take* fam_u;
  u3a_pile  pil_u;
  u3a_pile_prep(&pil_u, sizeof(*fam_u));

  //  commence taking
  //
  pro = _ca_take_next_south(&pil_u, veb);

  //  process cell results
  //
  if ( c3n == u3a_pile_done(&pil_u) ) {
    fam_u = u3a_peek(&pil_u);

    do {
      //  head-frame: stash copy and continue into the tail
      //
      if ( u3_none == fam_u->hed ) {
        u3a_cell* old_u = u3a_to_ptr(fam_u->old);
        fam_u->hed = pro;
        pro        = _ca_take_next_south(&pil_u, old_u->tel);
        fam_u      = u3a_peek(&pil_u);
      }
      //  tail-frame: copy cell and pop the stack
      //
      else {
        u3a_cell* old_u = u3a_to_ptr(fam_u->old);
        pro   = _ca_take_cell(old_u, fam_u->hed, pro);
        fam_u = u3a_pop(&pil_u);
      }
    } while ( c3n == u3a_pile_done(&pil_u) );
  }

  return pro;
}

/* u3a_take(): gain, copying juniors.
*/
u3_noun
u3a_take(u3_noun veb)
{
  u3_noun pro;
  u3t_on(coy_o);

  u3_assert(u3_none != veb);

  pro = ( c3y == u3a_is_north(u3R) )
        ? _ca_take_north(veb)
        : _ca_take_south(veb);

  u3t_off(coy_o);
  return pro;
}

/* u3a_left(): true of junior if preserved.
*/
c3_o
u3a_left(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ||
       !_(u3a_is_junior(u3R, som)) )
  {
    return c3y;
  }
  else {
    u3a_noun* dog_u = u3a_to_ptr(som);

    return __(0 != (dog_u->mug_w >> 31));
  }
}

/* _me_gain_north(): gain on a north road.
*/
static u3_noun
_me_gain_north(u3_noun dog)
{
  if ( c3y == u3a_north_is_senior(u3R, dog) ) {
    /*  senior pointers are not refcounted
    */
    return dog;
  }
  else {
    /* junior nouns are disallowed
    */
    u3_assert(!_(u3a_north_is_junior(u3R, dog)));

    /* normal pointers are refcounted
    */
    _me_gain_use(dog);
    return dog;
  }
}

/* _me_gain_south(): gain on a south road.
*/
static u3_noun
_me_gain_south(u3_noun dog)
{
  if ( c3y == u3a_south_is_senior(u3R, dog) ) {
    /*  senior pointers are not refcounted
    */
    return dog;
  }
  else {
    /* junior nouns are disallowed
    */
    u3_assert(!_(u3a_south_is_junior(u3R, dog)));

    /* normal nouns are refcounted
    */
    _me_gain_use(dog);
    return dog;
  }
}

/* _me_lose_north(): lose on a north road.
*/
static void
_me_lose_north(u3_noun dog)
{
top:
  if ( c3y == u3a_north_is_normal(u3R, dog) ) {
    u3a_noun* box_u = u3a_to_ptr(dog);

    if ( box_u->use_w > 1 ) {
      box_u->use_w -= 1;
    }
    else {
      if ( 0 == box_u->use_w ) {
        u3m_bail(c3__foul);
      }
      else {
        if ( _(u3a_is_pom(dog)) ) {
          u3a_cell* dog_u = (void*)box_u;
          u3_noun   h_dog = dog_u->hed;
          u3_noun   t_dog = dog_u->tel;

          if ( !_(u3a_is_cat(h_dog)) ) {
            _me_lose_north(h_dog);
          }
          u3a_cfree((c3_w*)dog_u);
          if ( !_(u3a_is_cat(t_dog)) ) {
            dog = t_dog;
            goto top;
          }
        }
        else {
          u3a_wfree(box_u);
        }
      }
    }
  }
}

/* _me_lose_south(): lose on a south road.
*/
static void
_me_lose_south(u3_noun dog)
{
top:
  if ( c3y == u3a_south_is_normal(u3R, dog) ) {
    u3a_noun* box_u = u3a_to_ptr(dog);

    if ( box_u->use_w > 1 ) {
      box_u->use_w -= 1;
    }
    else {
      if ( 0 == box_u->use_w ) {
        u3m_bail(c3__foul);
      }
      else {
        if ( _(u3a_is_pom(dog)) ) {
          u3a_cell* dog_u = (void*)box_u;
          u3_noun   h_dog = dog_u->hed;
          u3_noun   t_dog = dog_u->tel;

          if ( !_(u3a_is_cat(h_dog)) ) {
            _me_lose_south(h_dog);
          }
          u3a_cfree((c3_w*)dog_u);
          if ( !_(u3a_is_cat(t_dog)) ) {
            dog = t_dog;
            goto top;
          }
        }
        else {
          u3a_wfree(box_u);
        }
      }
    }
  }
}

/* u3a_gain(): gain a reference count in normal space.
*/
u3_noun
u3a_gain(u3_noun som)
{
  u3t_on(mal_o);
  u3_assert(u3_none != som);

  if ( !_(u3a_is_cat(som)) ) {
    som = _(u3a_is_north(u3R))
              ? _me_gain_north(som)
              : _me_gain_south(som);
  }
  u3t_off(mal_o);

  return som;
}

/* u3a_lose(): lose a reference count.
*/
void
u3a_lose(u3_noun som)
{
  u3t_on(mal_o);
  if ( !_(u3a_is_cat(som)) ) {
    if ( _(u3a_is_north(u3R)) ) {
      _me_lose_north(som);
    } else {
      _me_lose_south(som);
    }
  }
  u3t_off(mal_o);
}

/* u3a_use(): reference count.
*/
c3_w
u3a_use(u3_noun som)
{
  if ( _(u3a_is_cat(som)) ) {
    return 1;
  }
  else {
    u3a_noun* box_u = u3a_to_ptr(som);
    return box_u->use_w;
  }
}

#define SWAP(l, r)    \
  do { typeof(l) t = l; l = r; r = t; } while (0)

/* _ca_wed_our(): unify [a] and [b] on u3R.
*/
static inline c3_o
_ca_wed_our(u3_noun *restrict a, u3_noun *restrict b)
{
  c3_t asr_t = ( c3y == u3a_is_senior(u3R, *a) );
  c3_t bsr_t = ( c3y == u3a_is_senior(u3R, *b) );

  if ( asr_t == bsr_t ) {
    //  both [a] and [b] are senior; we can't unify on u3R
    //
    if ( asr_t ) return c3n;

    //  both are on u3R; keep the deeper address
    //  (and gain a reference)
    //
    //    (N && <) || (S && >)
    //    XX consider keeping higher refcount instead
    //
    if ( (*a > *b) == (c3y == u3a_is_north(u3R)) ) SWAP(a, b);

    _me_gain_use(*a);
  }
  //  one of [a] or [b] are senior; keep it
  //
  else if ( !asr_t ) SWAP(a, b);

  u3z(*b);
  *b = *a;
  return c3y;
}

/* _ca_wed_you(): unify [a] and [b] on senior [rod_u]. leaks
*/
static c3_o
_ca_wed_you(u3a_road* rod_u, u3_noun *restrict a, u3_noun *restrict b)
{
  //  XX assume( rod_u != u3R )
  c3_t asr_t = ( c3y == u3a_is_senior(rod_u, *a) );
  c3_t bsr_t = ( c3y == u3a_is_senior(rod_u, *b) );

  if ( asr_t == bsr_t ) {
    //  both [a] and [b] are senior; we can't unify on [rod_u]
    //
    if ( asr_t ) return c3n;

    //  both are on [rod_u]; keep the deeper address
    //  (and gain a reference)
    //
    //    (N && <) || (S && >)
    //    XX consider keeping higher refcount instead
    //
    if ( (*a > *b) == (c3y == u3a_is_north(rod_u)) ) SWAP(a, b);

    _me_gain_use(*a);
  }
  //  one of [a] or [b] are senior; keep it
  //
  else if ( !asr_t ) SWAP(a, b);

  *b = *a;
  return c3y;
}

#undef SWAP

/* u3a_wed(): unify noun references.
*/
void
u3a_wed(u3_noun *restrict a, u3_noun *restrict b)
{
  //  XX assume( *a != *b )
  u3_road* rod_u = u3R;
  c3_o     wed_o;

  if ( rod_u->kid_p ) return;

  wed_o = _ca_wed_our(a, b);

#ifdef U3_MEMORY_DEBUG
  return;
#else
  if ( u3C.wag_w & u3o_debug_ram ) return;
#endif

  //  while not at home, attempt to unify
  //
  //    we try to unify on our road, and retry on senior roads
  //    until we succeed or reach the home road.
  //
  //    we can't perform this kind of butchery on the home road,
  //    where asynchronous things can allocate.
  //    (XX anything besides u3t_samp?)
  //
  //    when unifying on a higher road, we can't free nouns,
  //    because we can't track junior nouns that point into
  //    that road.
  //
  //    this is just an implementation issue -- we could set use
  //    counts to 0 without actually freeing.  but the allocator
  //    would have to be actually designed for this.
  //    (alternately, we could keep a deferred free-list)
  //
  //    not freeing may generate spurious leaks, so we disable
  //    senior unification when debugging memory.  this will
  //    cause a very slow boot process as the compiler compiles
  //    itself, constantly running into duplicates.
  //

  while (  (c3n == wed_o)
        && rod_u->par_p
        && (&u3H->rod_u != (rod_u = u3to(u3_road, rod_u->par_p))) )
  {
    wed_o = _ca_wed_you(rod_u, a, b);
  }
}

/* u3a_luse(): check refcount sanity.
*/
void
u3a_luse(u3_noun som)
{
  if ( 0 == u3a_use(som) ) {
    fprintf(stderr, "loom: insane %d 0x%x\r\n", som, som);
    abort();
  }
  if ( _(u3du(som)) ) {
    u3a_luse(u3h(som));
    u3a_luse(u3t(som));
  }
}

/* u3a_mark_ptr(): mark a pointer for gc.  Produce size if first mark.
*/
c3_w
u3a_mark_ptr(void* ptr_v)
{
  //  XX restore loom-bounds check
  u3_post som_p = u3a_outa(ptr_v);
  c3_w    siz_w = !(u3C.wag_w & u3o_debug_ram)
                ? _mark_post(som_p)
                : _count_post(som_p, 0);

  return siz_w;
}

/* u3a_relocate_post(): replace post with relocation pointer (unchecked).
*/
void
u3a_relocate_post(u3_post *som_p)
{
  *som_p = _pack_relocate(*som_p);
}

/* u3a_mark_relocate_post(): replace post with relocation pointer (checked).
*/
u3_post
u3a_mark_relocate_post(u3_post som_p, c3_t *fir_t)
{
  return _pack_relocate_mark(som_p, fir_t);
}

/* u3a_relocate_noun(): replace noun with relocation reference, recursively.
*/
void
u3a_relocate_noun(u3_noun *som)
{
  u3_post   old_p, new_p;
  u3_noun     old;
  u3a_cell* cel_u;
  c3_t      fir_t;

  while ( 1 ) {
    old = *som;

    if ( c3y == u3a_is_cat(old) ) return;

    old_p = u3a_to_off(old);

    if ( c3n == u3a_is_cell(old) ) {
      new_p = _pack_relocate(old_p);
      *som = u3a_to_pug(new_p);
      return;
    }

    new_p = _pack_relocate_mark(old_p, &fir_t);
    *som  = u3a_to_pom(new_p);

    if ( !fir_t ) return;

    cel_u = u3to(u3a_cell, old_p);
    u3a_relocate_noun(&(cel_u->hed));
    som   = &(cel_u->tel);
  }
}

/* u3a_mark_mptr(): mark a malloc-allocated ptr for gc.
*/
c3_w
u3a_mark_mptr(void* ptr_v)
{
  return u3a_mark_ptr(ptr_v);
}

/* u3a_mark_rptr(): mark a refcounted, word-aligned ptr for gc.
*/
c3_w
u3a_mark_rptr(void* ptr_v)
{
  u3_post som_p = u3a_outa(ptr_v);
  c3_w    siz_w = !(u3C.wag_w & u3o_debug_ram)
                ? _mark_post(som_p)
                : _count_post(som_p, 1);

  return siz_w;
}

/* u3a_mark_noun(): mark a noun for gc.  Produce size.
*/
c3_w
u3a_mark_noun(u3_noun som)
{
  c3_w siz_w = 0;

  while ( 1 ) {
    if ( _(u3a_is_senior(u3R, som)) ) {
      return siz_w;
    }
    else {
      c3_w* dog_w = u3a_to_ptr(som);
      c3_w  new_w = u3a_mark_rptr(dog_w);

      if ( 0 == new_w || 0xffffffff == new_w ) {      //  see u3a_mark_ptr()
        return siz_w;
      }
      else {
        siz_w += new_w;
        if ( _(u3du(som)) ) {
          siz_w += u3a_mark_noun(u3h(som));
          som = u3t(som);
        }
        else return siz_w;
      }
    }
  }
}

/* u3a_count_noun(): count size of pointer.
*/
c3_w
u3a_count_ptr(void* ptr_v)
{
#if 0
  if ( _(u3a_is_north(u3R)) ) {
    if ( !((ptr_v >= u3a_into(u3R->rut_p)) &&
           (ptr_v < u3a_into(u3R->hat_p))) )
    {
      return 0;
    }
  }
  else {
    if ( !((ptr_v >= u3a_into(u3R->hat_p)) &&
           (ptr_v < u3a_into(u3R->rut_p))) )
    {
      return 0;
    }
  }
  {
    u3a_box* box_u  = u3a_botox(ptr_v);
    c3_w     siz_w;

    c3_ws use_ws = (c3_ws)box_u->use_w;

    if ( use_ws == 0 ) {
      fprintf(stderr, "%p is bogus\r\n", ptr_v);
      siz_w = 0;
    }
    else {
      u3_assert(use_ws != 0);

      if ( use_ws < 0 ) {
        siz_w = 0;
      }
      else {
        use_ws = -use_ws;
        siz_w = box_u->siz_w;
      }
      box_u->use_w = (c3_w)use_ws;
    }
    return siz_w;
  }
#endif
  return 0;
}

/* u3a_count_noun(): count size of noun.
*/
c3_w
u3a_count_noun(u3_noun som)
{
  c3_w siz_w = 0;

  while ( 1 ) {
    if ( _(u3a_is_senior(u3R, som)) ) {
      return siz_w;
    }
    else {
      c3_w* dog_w = u3a_to_ptr(som);
      c3_w  new_w = u3a_count_ptr(dog_w);

      if ( 0 == new_w ) {
        return siz_w;
      }
      else {
        siz_w += new_w;
        if ( _(u3du(som)) ) {
          siz_w += u3a_count_noun(u3h(som));
          som = u3t(som);
        }
        else return siz_w;
      }
    }
  }
}

/* u3a_discount_ptr(): clean up after counting a pointer.
*/
c3_w
u3a_discount_ptr(void* ptr_v)
{
#if 0
  if ( _(u3a_is_north(u3R)) ) {
    if ( !((ptr_v >= u3a_into(u3R->rut_p)) &&
           (ptr_v < u3a_into(u3R->hat_p))) )
    {
      return 0;
    }
  }
  else {
    if ( !((ptr_v >= u3a_into(u3R->hat_p)) &&
           (ptr_v < u3a_into(u3R->rut_p))) )
    {
      return 0;
    }
  }
  u3a_box* box_u  = u3a_botox(ptr_v);
  c3_w     siz_w;

  c3_ws use_ws = (c3_ws)box_u->use_w;

  if ( use_ws == 0 ) {
    fprintf(stderr, "%p is bogus\r\n", ptr_v);
    siz_w = 0;
  }
  else {
    u3_assert(use_ws != 0);

    if ( use_ws < 0 ) {
      use_ws = -use_ws;
      siz_w = box_u->siz_w;
    }
    else {
      siz_w = 0;
    }
    box_u->use_w = (c3_w)use_ws;
  }

  return siz_w;
#endif
  return 0;
}

/* u3a_discount_noun(): clean up after counting a noun.
*/
c3_w
u3a_discount_noun(u3_noun som)
{
  c3_w siz_w = 0;

  while ( 1 ) {
    if ( _(u3a_is_senior(u3R, som)) ) {
      return siz_w;
    }
    else {
      c3_w* dog_w = u3a_to_ptr(som);
      c3_w  new_w = u3a_discount_ptr(dog_w);

      if ( 0 == new_w ) {
        return siz_w;
      }
      else {
        siz_w += new_w;
        if ( _(u3du(som)) ) {
          siz_w += u3a_discount_noun(u3h(som));
          som = u3t(som);
        }
        else return siz_w;
      }
    }
  }
}

/* u3a_print_time: print microsecond time.
*/
void
u3a_print_time(c3_c* str_c, c3_c* cap_c, c3_d mic_d)
{
  u3_assert( 0 != str_c );

  c3_w sec_w = (mic_d / 1000000);
  c3_w mec_w = (mic_d % 1000000) / 1000;
  c3_w mic_w = (mic_d % 1000);

  if ( sec_w ) {
    sprintf(str_c, "%s s/%d.%03d.%03d", cap_c, sec_w, mec_w, mic_w);
  }
  else if ( mec_w ) {
    sprintf(str_c, "%s ms/%d.%03d", cap_c, mec_w, mic_w);
  }
  else {
    sprintf(str_c, "%s \xc2\xb5s/%d", cap_c, mic_w);
  }
}

/* u3a_print_memory: print memory amount to file descriptor.
*/
void
u3a_print_memory(FILE* fil_u, c3_c* cap_c, c3_w wor_w)
{
  u3_assert( 0 != fil_u );

  c3_z byt_z = ((c3_z)wor_w * 4);
  c3_z gib_z = (byt_z / 1000000000);
  c3_z mib_z = (byt_z % 1000000000) / 1000000;
  c3_z kib_z = (byt_z % 1000000) / 1000;
  c3_z bib_z = (byt_z % 1000);

  if ( byt_z ) {
    if ( gib_z ) {
      fprintf(fil_u, "%s: GB/%" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z "\r\n",
              cap_c, gib_z, mib_z, kib_z, bib_z);
    }
    else if ( mib_z ) {
      fprintf(fil_u, "%s: MB/%" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z "\r\n",
              cap_c, mib_z, kib_z, bib_z);
    }
    else if ( kib_z ) {
      fprintf(fil_u, "%s: KB/%" PRIc3_z ".%03" PRIc3_z "\r\n",
              cap_c, kib_z, bib_z);
    }
    else if ( bib_z ) {
      fprintf(fil_u, "%s: B/%" PRIc3_z "\r\n",
              cap_c, bib_z);
    }
  }
}

/* u3a_print_memory_str: print memory amount to string.
*/
void
u3a_print_memory_str(c3_c* str_c, c3_c* cap_c, c3_w wor_w)
{
  u3_assert( 0 != str_c );

  c3_z byt_z = ((c3_z)wor_w * 4);
  c3_z gib_z = (byt_z / 1000000000);
  c3_z mib_z = (byt_z % 1000000000) / 1000000;
  c3_z kib_z = (byt_z % 1000000) / 1000;
  c3_z bib_z = (byt_z % 1000);

  if ( byt_z ) {
    if ( gib_z ) {
      sprintf(str_c, "%s: GB/%" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z "\r\n",
              cap_c, gib_z, mib_z, kib_z, bib_z);
    }
    else if ( mib_z ) {
      sprintf(str_c, "%s: MB/%" PRIc3_z ".%03" PRIc3_z ".%03" PRIc3_z "\r\n",
              cap_c, mib_z, kib_z, bib_z);
    }
    else if ( kib_z ) {
      sprintf(str_c, "%s: KB/%" PRIc3_z ".%03" PRIc3_z "\r\n",
              cap_c, kib_z, bib_z);
    }
    else if ( bib_z ) {
      sprintf(str_c, "%s: B/%" PRIc3_z "\r\n",
              cap_c, bib_z);
    }
  }
}

/* u3a_maid(): maybe print memory.
*/
c3_w
u3a_maid(FILE* fil_u, c3_c* cap_c, c3_w wor_w)
{
  if ( 0 != fil_u ) {
    u3a_print_memory(fil_u, cap_c, wor_w);
  }
  return wor_w;
}

/* _ca_print_memory(): un-captioned u3a_print_memory().
*/
static void
_ca_print_memory(FILE* fil_u, c3_w byt_w)
{
  c3_w gib_w = (byt_w / 1000000000);
  c3_w mib_w = (byt_w % 1000000000) / 1000000;
  c3_w kib_w = (byt_w % 1000000) / 1000;
  c3_w bib_w = (byt_w % 1000);

  if ( gib_w ) {
    fprintf(fil_u, "GB/%d.%03d.%03d.%03d\r\n",
            gib_w, mib_w, kib_w, bib_w);
  }
  else if ( mib_w ) {
    fprintf(fil_u, "MB/%d.%03d.%03d\r\n", mib_w, kib_w, bib_w);
  }
  else if ( kib_w ) {
    fprintf(fil_u, "KB/%d.%03d\r\n", kib_w, bib_w);
  }
  else {
    fprintf(fil_u, "B/%d\r\n", bib_w);
  }
}

/* u3a_quac_free: free quac memory.
*/
void
u3a_quac_free(u3m_quac* qua_u)
{
  c3_w i_w = 0;

  if ( qua_u->qua_u ) {
    while ( qua_u->qua_u[i_w] != NULL ) {
      u3a_quac_free(qua_u->qua_u[i_w]);
      i_w++;
    }
  }
  c3_free(qua_u->nam_c);
  c3_free(qua_u->qua_u);
  c3_free(qua_u);
}

static c3_w
_ca_prof_mark(u3_noun som)
{
  if ( c3y == u3a_is_senior(u3R, som) ) {
    return 0;
  }

  //  marking memory-profile entries under u3o_debug_ram
  //  requires special care to avoid over-incrementing the refcount,
  //  as the entries in the profile are not true roots:
  //  they either belong to arvo or the profile itself.
  //  we measure them here, but account for them elsewhere (subsequently)
  //  from a refcounting standpoint
  //
  u3_post som_p = u3a_to_off(som);
  c3_w    siz_w = !(u3C.wag_w & u3o_debug_ram)
                ? _mark_post(som_p)
                : _count_post(som_p, 2);

  if ( !siz_w ) {
    return 0;
  }

  if ( c3y == u3a_is_cell(som) ) {
    siz_w += u3a_mark_noun(u3h(som));
    siz_w += u3a_mark_noun(u3t(som));
  }

  return siz_w;
}

/* u3a_prof(): mark/measure/print memory profile. RETAIN.
*/
u3m_quac*
u3a_prof(FILE* fil_u, u3_noun mas)
{
  u3m_quac* pro_u = c3_calloc(sizeof(*pro_u));
  u3_noun h_mas, t_mas;

  if ( c3n == u3r_cell(mas, &h_mas, &t_mas) ) {
    fprintf(fil_u, "mistyped mass\r\n");
    c3_free(pro_u);
    return NULL;
  }
  else if ( c3y == u3du(h_mas) ) {
    fprintf(fil_u, "mistyped mass head\r\n");
    {
      c3_c* lab_c = u3m_pretty(h_mas);
      fprintf(fil_u, "h_mas: %s", lab_c);
      c3_free(lab_c);
    }
    c3_free(pro_u);
    return NULL;
  }
  else {

    u3_noun it_mas, tt_mas;

    if ( c3n == u3r_cell(t_mas, &it_mas, &tt_mas) ) {
      fprintf(fil_u, "mistyped mass tail\r\n");
      c3_free(pro_u);
      return NULL;
    }
    else if ( c3y == it_mas ) {
      c3_w siz_w = _ca_prof_mark(tt_mas);

      pro_u->nam_c = u3r_string(h_mas);
      pro_u->siz_w = siz_w*4;
      pro_u->qua_u = NULL;
      return pro_u;

    }
    else if ( c3n == it_mas ) {
      pro_u->qua_u = c3_malloc(sizeof(pro_u->qua_u));
      c3_w i_w = 0;
      c3_t bad_t = 0;
      while ( c3y == u3du(tt_mas) ) {
        u3m_quac* new_u = u3a_prof(fil_u, u3h(tt_mas));
        if ( NULL == new_u ) {
          bad_t = 1;
        } else {
          pro_u->qua_u = c3_realloc(pro_u->qua_u, (i_w + 2) * sizeof(pro_u->qua_u));
          pro_u->siz_w += new_u->siz_w;
          pro_u->qua_u[i_w] = new_u;
        }
        tt_mas = u3t(tt_mas);
        i_w++;
      }
      pro_u->qua_u[i_w] = NULL;

      if ( bad_t ) {
        i_w = 0;
        while ( pro_u->qua_u[i_w] != NULL ) {
          u3a_quac_free(pro_u->qua_u[i_w]);
          i_w++;
        }
        c3_free(pro_u->qua_u);
        c3_free(pro_u);
        return NULL;
      } else {
        pro_u->nam_c = u3r_string(h_mas);
        return pro_u;
      }
    }
    else {
      fprintf(fil_u, "mistyped (strange) mass tail\r\n");
      c3_free(pro_u);
      return NULL;
    }
  }
}


/* u3a_print_quac: print a memory report.
*/

void
u3a_print_quac(FILE* fil_u, c3_w den_w, u3m_quac* mas_u)
{
  u3_assert( 0 != fil_u );

  if ( mas_u->siz_w ) {
    fprintf(fil_u, "%*s%s: ", den_w, "", mas_u->nam_c);

    if ( mas_u->qua_u == NULL ) {
      _ca_print_memory(fil_u, mas_u->siz_w);
    } else {
      fprintf(fil_u, "\r\n");
      c3_w i_w = 0;
      while ( mas_u->qua_u[i_w] != NULL ) {
        u3a_print_quac(fil_u, den_w+2, mas_u->qua_u[i_w]);
        i_w++;
      }
      fprintf(fil_u, "%*s--", den_w, "");
      _ca_print_memory(fil_u, mas_u->siz_w);
    }
  }
}

/* u3a_mark_road(): mark ad-hoc persistent road structures.
*/
u3m_quac*
u3a_mark_road()
{
  u3m_quac** qua_u = c3_malloc(sizeof(*qua_u) * 15);

  qua_u[0] = c3_calloc(sizeof(*qua_u[0]));
  qua_u[0]->nam_c = strdup("namespace");
  qua_u[0]->siz_w = u3a_mark_noun(u3R->ski.gul) * 4;

  qua_u[1] = c3_calloc(sizeof(*qua_u[1]));
  qua_u[1]->nam_c = strdup("trace stack");
  qua_u[1]->siz_w = u3a_mark_noun(u3R->bug.tax) * 4;

  qua_u[2] = c3_calloc(sizeof(*qua_u[2]));
  qua_u[2]->nam_c = strdup("trace buffer");
  qua_u[2]->siz_w = u3a_mark_noun(u3R->bug.mer) * 4;

  qua_u[3] = c3_calloc(sizeof(*qua_u[3]));
  qua_u[3]->nam_c = strdup("profile batteries");
  qua_u[3]->siz_w = u3a_mark_noun(u3R->pro.don) * 4;

  qua_u[4] = c3_calloc(sizeof(*qua_u[4]));
  qua_u[4]->nam_c = strdup("profile doss");
  qua_u[4]->siz_w = u3a_mark_noun(u3R->pro.day) * 4;

  qua_u[5] = c3_calloc(sizeof(*qua_u[5]));
  qua_u[5]->nam_c = strdup("new profile trace");
  qua_u[5]->siz_w = u3a_mark_noun(u3R->pro.trace) * 4;

  qua_u[6] = c3_calloc(sizeof(*qua_u[6]));
  qua_u[6]->nam_c = strdup("transient memoization cache");
  qua_u[6]->siz_w = u3h_mark(u3R->cax.har_p) * 4;

  qua_u[7] = c3_calloc(sizeof(*qua_u[7]));
  qua_u[7]->nam_c = strdup("persistent memoization cache");
  qua_u[7]->siz_w = u3h_mark(u3R->cax.per_p) * 4;

  qua_u[8] = c3_calloc(sizeof(*qua_u[8]));
  qua_u[8]->nam_c = strdup("page directory");
  qua_u[8]->siz_w = u3a_mark_ptr(u3a_into(u3R->hep.pag_p)) * 4;

  qua_u[9] = c3_calloc(sizeof(*qua_u[9]));
  qua_u[9]->nam_c = strdup("cell pool");

  {
    u3_post *cel_p;
    c3_w     cel_w = 0;

    if ( u3R->cel.cel_p ) {
      cel_p  = u3to(u3_post, u3R->cel.cel_p);
      cel_w += u3a_mark_ptr(cel_p);

      for ( c3_w i_w = 0; i_w < u3R->cel.hav_w; i_w++ ) {
        cel_w += u3a_mark_ptr(u3a_into(cel_p[i_w]));
      }
    }

    qua_u[9]->siz_w = cel_w * 4;
  }

  qua_u[10] = c3_calloc(sizeof(*qua_u[10]));
  qua_u[10]->nam_c = strdup("free list");

  {
    u3a_dell *fre_u = u3tn(u3a_dell, u3R->hep.fre_p);
    c3_w      fre_w = 0;

    while ( fre_u ) {
      fre_w += u3a_mark_ptr(fre_u);
      fre_u  = u3tn(u3a_dell, fre_u->nex_p);
    }

    if ( u3R->hep.cac_p ) {
      fre_w += u3a_mark_ptr(u3a_into(u3R->hep.cac_p));
    }

    qua_u[10]->siz_w = fre_w * 4;
  }

  qua_u[11] = c3_calloc(sizeof(*qua_u[11]));
  qua_u[11]->nam_c = strdup("metadata");

  {
    c3_w wee_w = 0;

    for ( c3_w i_w = 0; i_w < u3a_crag_no; i_w++ ) {
      wee_w += u3a_Mark.wee_w[i_w];
    }

    qua_u[11]->siz_w = wee_w * 4;
  }

  qua_u[12] = c3_calloc(sizeof(*qua_u[12]));
  qua_u[12]->nam_c = strdup("loop hint set");
  qua_u[12]->siz_w = u3h_mark(u3R->lop_p) * 4;
  
  qua_u[13] = c3_calloc(sizeof(*qua_u[13]));
  qua_u[13]->nam_c = strdup("timer stack");
  qua_u[13]->siz_w = u3a_mark_noun(u3R->tim) * 4;
  
  qua_u[14] = NULL;

  c3_w sum_w = 0;
  for (c3_w i_w = 0; qua_u[i_w]; i_w++) {
    sum_w += qua_u[i_w]->siz_w;
  }

  u3m_quac* tot_u = c3_malloc(sizeof(*tot_u));
  tot_u->nam_c = strdup("total road stuff");
  tot_u->siz_w = sum_w;
  tot_u->qua_u = qua_u;

  return tot_u;
}

/* u3a_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3a_reclaim(void)
{
  //  clear the memoization cache
  //
  u3h_free(u3R->cax.har_p);
  u3R->cax.har_p = u3h_new();
}

/* u3a_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
*/
void
u3a_rewrite_compact(void)
{
  //  XX relocate cel_p
  u3a_relocate_noun(&(u3R->ski.gul));
  u3a_relocate_noun(&(u3R->bug.tax));
  u3a_relocate_noun(&(u3R->bug.mer));
  u3a_relocate_noun(&(u3R->pro.don));
  u3a_relocate_noun(&(u3R->pro.day));
  u3a_relocate_noun(&(u3R->pro.trace));
  u3a_relocate_noun(&(u3R->tim));
  u3h_relocate(&(u3R->cax.har_p));
  u3h_relocate(&(u3R->cax.per_p));
  u3h_relocate(&(u3R->lop_p));
}

/* u3a_idle(): measure free-lists in [rod_u]
*/
c3_w
u3a_idle(u3a_road* rod_u)
{
  //  XX ignores argument
  c3_w pag_w = _idle_pages();
  if ( (u3C.wag_w & u3o_verbose) && pag_w ) {
    fprintf(stderr, "loom: idle %u complete pages\r\n", pag_w);
  }
  return (pag_w << u3a_page) + _idle_words();
}

void
u3a_ream(void)
{
  _poison_pages();
  _poison_words();

  //  XX enable behind flag
  // _sane_dell();
}

void
u3a_wait(void)
{
  _unpoison_words();
}

void
u3a_dash(void)
{
  _poison_words();
}

/* u3a_sweep(): sweep a fully marked road.
*/
c3_w
u3a_sweep(void)
{
  c3_w siz_w = !(u3C.wag_w & u3o_debug_ram)
               ? _sweep_directory()
               : _sweep_counts();

  return siz_w;
}

/* u3a_pack_seek(): sweep the heap, modifying boxes to record new addresses.
*/
void
u3a_pack_seek(u3a_road* rod_u)
{
  u3a_pack_init();

  //  XX clear cell pool on inner roads?

  //  XX use road argument
  _pack_seek();
  _pack_relocate_heap();
}

/* u3a_pack_move(): sweep the heap, moving boxes to new addresses.
*/
void
u3a_pack_move(u3a_road* rod_u)
{
  //  XX use road argument
  _pack_move();

  u3a_pack_done();

  u3_post old_p = u3R->hat_p;

  //  XX move me?
  //
  u3R->hat_p = u3R->rut_p + (u3R->hep.dir_ws * (c3_ws)(u3R->hep.len_w << u3a_page));

  u3a_drop_heap(old_p, u3R->hat_p);
}

#if 0
/* _ca_detect(): in u3a_detect().
*/
static c3_d
_ca_detect(u3p(u3h_root) har_p, u3_noun fum, u3_noun som, c3_d axe_d)
{
  while ( 1 ) {
    if ( som == fum ) {
      return axe_d;
    }
    else if ( !_(u3du(fum)) || (u3_none != u3h_get(har_p, fum)) ) {
      return 0;
    }
    else {
      c3_d eax_d;

      u3h_put(har_p, fum, 0);

      if ( 0 != (eax_d = _ca_detect(har_p, u3h(fum), som, 2ULL * axe_d)) ) {
        return c3y;
      }
      else {
        fum = u3t(fum);
        axe_d = (2ULL * axe_d) + 1;
      }
    }
  }
}

/* u3a_detect(): for debugging, check if (som) is referenced from (fum).
**
** (som) and (fum) are both RETAINED.
*/
c3_d
u3a_detect(u3_noun fum, u3_noun som)
{
  u3p(u3h_root) har_p = u3h_new();
  c3_o          ret_o;

  ret_o = _ca_detect(har_p, fum, som, 1);
  u3h_free(har_p);

  return ret_o;
}
#endif

#ifdef U3_MEMORY_DEBUG
/* u3a_lush(): leak push.
*/
c3_w
u3a_lush(c3_w lab_w)
{
  c3_w cod_w = u3_Code;

  u3_Code = lab_w;
  return cod_w;
}

/* u3a_lop(): leak pop.
*/
void
u3a_lop(c3_w lab_w)
{
  u3_Code = lab_w;
}
#else
/* u3a_lush(): leak push.
*/
c3_w
u3a_lush(c3_w lab_w)
{
  return 0;
}

/* u3a_lop(): leak pop.
*/
void
u3a_lop(c3_w lab_w)
{
}
#endif

/* u3a_walk_fore(): preorder traversal, visits ever limb of a noun.
**
**   cells are visited *before* their heads and tails
**   and can shortcircuit traversal by returning [c3n]
*/
void
u3a_walk_fore(u3_noun    a,
              void*      ptr_v,
              void     (*pat_f)(u3_atom, void*),
              c3_o     (*cel_f)(u3_noun, void*))
{
  u3_noun*   top;
  u3a_pile pil_u;

  //  initialize stack control; push argument
  //
  u3a_pile_prep(&pil_u, sizeof(u3_noun));
  top  = u3a_push(&pil_u);
  *top = a;

  while ( c3n == u3a_pile_done(&pil_u) ) {
    //  visit an atom, then pop the stack
    //
    if ( c3y == u3a_is_atom(a) ) {
      pat_f(a, ptr_v);
      top = u3a_pop(&pil_u);
    }
    //  vist a cell, if c3n, pop the stack
    //
    else if ( c3n == cel_f(a, ptr_v) ) {
      top = u3a_pop(&pil_u);
    }
    //  otherwise, push the tail and continue into the head
    //
    else {
      *top = u3t(a);
      top  = u3a_push(&pil_u);
      *top = u3h(a);
    }

    a = *top;
  }
}

/* u3a_string(): `a` as an on-loom c-string.
*/
c3_c*
u3a_string(u3_atom a)
{
  c3_w  met_w = u3r_met(3, a);
  c3_c* str_c = u3a_malloc(met_w + 1);

  u3r_bytes(0, met_w, (c3_y*)str_c, a);
  str_c[met_w] = 0;
  return str_c;
}

/* u3a_loom_sane(): sanity checks the state of the loom for obvious corruption
 */
void
u3a_loom_sane(void)
{
}
