/// @file

#include "pkg/noun/v2/manage.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "pkg/noun/v2/allocate.h"
#include "events.h"
#include "pkg/noun/v2/hashtable.h"
#include "imprison.h"
#include "pkg/noun/v2/jets.h"
#include "jets/k.h"
#include "log.h"
#include "nock.h"
#include "openssl/crypto.h"
#include "options.h"
#include "platform/rsignal.h"
#include "retrieve.h"
#include "trace.h"
#include "urcrypt/urcrypt.h"
#include "pkg/noun/vortex.h"
#include "pkg/noun/v2/vortex.h"
#include "xtract.h"

/* u3m_v2_reclaim: clear persistent caches to reclaim memory
*/
void
u3m_v2_reclaim(void)
{
  u3v_reclaim();
  u3j_v2_reclaim();
  u3n_v2_reclaim();
  u3a_v2_reclaim();
}

/* _cm_pack_rewrite(): trace through arena, rewriting pointers.
 *                     XX need to version; dynamic scope insanity!
*/
static void
_cm_pack_rewrite(void)
{
  //  XX fix u3a_rewrite* to support south roads
  //
  u3_assert( &(u3H->rod_u) == u3R );

  //  NB: these implementations must be kept in sync with u3m_reclaim();
  //  anything not reclaimed must be rewritable
  //
  u3v_v2_rewrite_compact();  //  XX need to version
  u3j_v2_rewrite_compact();  //  XX need to version
  u3n_v2_rewrite_compact();  //  XX need to version
  u3a_v2_rewrite_compact();  //  XX need to version
}

static void
_migrate_reclaim()
{
  fprintf(stderr, "loom: migration reclaim\r\n");
  u3m_v2_reclaim();
}

static void
_migrate_seek(const u3a_road *rod_u)
{
  /*
    very much like u3a_pack_seek with the following changes:
    - there is no need to account for free space as |pack is performed before
      the migration
    - odd sized boxes will be padded by one word to achieve an even size
    - rut will be moved from one word ahead of u3_Loom to two words ahead
  */
  c3_w *    box_w = u3a_into(rod_u->rut_p);
  c3_w *    end_w = u3a_into(rod_u->hat_p);
  u3_post   new_p = (rod_u->rut_p + 1 + c3_wiseof(u3a_box));
  u3a_box * box_u = (void *)box_w;

  fprintf(stderr, "loom: migration seek\r\n");

  for (; box_w < end_w
         ; box_w += box_u->siz_w
         , box_u = (void*)box_w)
    {
      if (!box_u->use_w)
        continue;
      u3_assert(box_u->siz_w);
      u3_assert(box_u->use_w);
      box_w[box_u->siz_w - 1] = new_p;
      new_p = c3_align(new_p + box_u->siz_w, 2, C3_ALGHI);
    }
}

static void
_migrate_rewrite()
{
  fprintf(stderr, "loom: migration rewrite\r\n");

  /* So that rewritten pointers are compressed, this flag is set */
  u3C.migration_state = MIG_REWRITE_COMPRESSED;
  _cm_pack_rewrite();  //  XX need to version
  u3C.migration_state = MIG_NONE;
}

static void
_migrate_move(u3a_road *rod_u)
{
  fprintf(stderr, "loom: migration move\r\n");

  c3_z hiz_z = u3a_heap(rod_u) * sizeof(c3_w);

  /* calculate required shift distance to prevent write head overlapping read head */
  c3_w  off_w = 1;  /* at least 1 word because u3R->rut_p migrates from 1 to 2 */
  for (u3a_box *box_u = u3a_into(rod_u->rut_p)
         ; (void *)box_u < u3a_into(rod_u->hat_p)
         ; box_u = (void *)((c3_w *)box_u + box_u->siz_w))
    off_w += box_u->siz_w & 1; /* odd-sized boxes are padded by one word */

  /* shift */
  memmove(u3a_into(u3H->rod_u.rut_p + off_w),
          u3a_into(u3H->rod_u.rut_p),
          hiz_z);
  /* manually zero the former rut */
  *(c3_w *)u3a_into(rod_u->rut_p) = 0;

  /* relocate boxes to DWORD-aligned addresses stored in trailing size word */
  c3_w *box_w = u3a_into(rod_u->rut_p + off_w);
  c3_w *end_w = u3a_into(rod_u->hat_p + off_w);
  u3a_box *old_u = (void *)box_w;
  c3_w siz_w = old_u->siz_w;
  u3p(c3_w) new_p = rod_u->rut_p + 1 + c3_wiseof(u3a_box);
  c3_w *new_w;

  for (; box_w < end_w
         ; box_w += siz_w
         , old_u = (void *)box_w
         , siz_w = old_u->siz_w) {
    old_u->use_w &= 0x7fffffff;

    if (!old_u->use_w)
      continue;

    new_w = (void *)u3a_botox(u3a_into(new_p));
    u3_assert(box_w[siz_w - 1] == new_p);
    u3_assert(new_w <= box_w);

    c3_w i_w;
    for (i_w = 0; i_w < siz_w - 1; i_w++)
      new_w[i_w] = box_w[i_w];

    if (siz_w & 1) {
      new_w[i_w++] = 0;         /* pad odd sized boxes */
      new_w[i_w++] = siz_w + 1; /* restore trailing size word */
      new_w[0] = siz_w + 1;     /* and the leading size word */
    }
    else {
      new_w[i_w++] = siz_w;
    }

    new_p += i_w;
  }

  /* restore proper heap state */
  rod_u->rut_p = 2;
  rod_u->hat_p = new_p - c3_wiseof(u3a_box);

  /* like |pack, clear the free lists and cell allocator */
  for (c3_w i_w = 0; i_w < u3a_fbox_no; i_w++)
    u3R->all.fre_p[i_w] = 0;

  u3R->all.fre_w = 0;
  u3R->all.cel_p = 0;
}


/* u3m_v2_migrate: perform loom migration if necessary.
   ver_w - target version
*/
void
u3m_v2_migrate()
{
  if ( U3V_VER2 == u3H->ver_w )
    return;

  /* 1 -> 2 is all that is currently supported */
  c3_dessert(u3H->ver_w == U3V_VER1);

  /* only home road migration is supported */
  c3_dessert((uintptr_t)u3H == (uintptr_t)u3R);

  fprintf(stderr, "loom: pointer compression migration running...\r\n");

  /* packing first simplifies migration logic and minimizes required buffer space */
  //  XX determine if we need to version this for the v2 migration
  // u3m_pack();

  /* perform the migration in a pattern similar to |pack */
  _migrate_reclaim();
  _migrate_seek(&u3H->rod_u);
  _migrate_rewrite();
  _migrate_move(&u3H->rod_u);

  /* finally update the version and commit to disk */
  u3H->ver_w = U3V_VER2;
}
