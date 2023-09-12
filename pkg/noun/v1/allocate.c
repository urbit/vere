/// @file

#include "pkg/noun/allocate.h"
#include "pkg/noun/v1/allocate.h"

#include "pkg/noun/v1/hashtable.h"

/* u3a_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
*/
void
u3a_v1_reclaim(void)
{
  //  clear the memoization cache
  //
  u3h_v1_free(u3R->cax.har_p);
  u3R->cax.har_p = u3h_new();
}

/* _me_lose_north(): lose on a north road.
*/
static void
_me_lose_north(u3_noun dog)
{
top:
  {
    c3_w* dog_w      = u3a_v1_to_ptr(dog);
    u3a_box* box_u = u3a_botox(dog_w);

    if ( box_u->use_w > 1 ) {
      box_u->use_w -= 1;
    }
    else {
      if ( 0 == box_u->use_w ) {
        u3m_bail(c3__foul);
      }
      else {
        if ( _(u3a_is_pom(dog)) ) {
          u3a_cell* dog_u = (void *)dog_w;
          u3_noun     h_dog = dog_u->hed;
          u3_noun     t_dog = dog_u->tel;

          if ( !_(u3a_is_cat(h_dog)) ) {
            _me_lose_north(h_dog);
          }
          u3a_wfree(dog_w);
          if ( !_(u3a_is_cat(t_dog)) ) {
            dog = t_dog;
            goto top;
          }
        }
        else {
          u3a_wfree(dog_w);
        }
      }
    }
  }
}

/* u3a_v1_lose(): lose a reference count.
*/
void
u3a_v1_lose(u3_noun som)
{
  if ( !_(u3a_is_cat(som)) ) {
    _me_lose_north(som);
  }
}
