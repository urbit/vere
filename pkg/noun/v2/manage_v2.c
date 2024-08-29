/// @file

#include "pkg/noun/v2/manage.h"

#include "pkg/noun/v1/allocate.h"
#include "pkg/noun/v2/allocate.h"
#include "pkg/noun/v2/hashtable.h"
#include "pkg/noun/v2/jets.h"
#include "pkg/noun/v2/nock.h"
#include "pkg/noun/v2/options.h"
#include "pkg/noun/vortex.h"
#include "pkg/noun/v1/vortex.h"
#include "pkg/noun/v2/vortex.h"

/* _cm_pack_rewrite(): trace through arena, rewriting pointers.
*/
static void
_cm_pack_rewrite(void)
{
  u3v_v2_mig_rewrite_compact();
  u3j_v2_mig_rewrite_compact();
  u3n_v2_mig_rewrite_compact();
  u3a_v2_mig_rewrite_compact();
}

static void
_migrate_reclaim()
{
  //  XX update this and similar printfs
  fprintf(stderr, "loom: migration reclaim\r\n");
  u3m_v1_reclaim();
}

static void
_migrate_seek(const u3a_v2_road *rod_u)
{
  /*
    very much like u3a_v2_pack_seek with the following changes:
    - there is no need to account for free space as |pack is performed before
      the migration
    - odd sized boxes will be padded by one word to achieve an even size
    - rut will be moved from one word ahead of u3_Loom to two words ahead
  */
  c3_w *    box_w = u3a_v2_into(rod_u->rut_p);
  c3_w *    end_w = u3a_v2_into(rod_u->hat_p);
  u3_post   new_p = (rod_u->rut_p + 1 + c3_wiseof(u3a_v2_box));
  u3a_v2_box * box_u = (void *)box_w;

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

  _cm_pack_rewrite();
}

static void
_migrate_move(u3a_v2_road *rod_u)
{
  fprintf(stderr, "loom: migration move\r\n");

  c3_z hiz_z = u3a_v2_heap(rod_u) * sizeof(c3_w);

  /* calculate required shift distance to prevent write head overlapping read head */
  c3_w  off_w = 1;  /* at least 1 word because u3R_v1->rut_p migrates from 1 to 2 */
  for (u3a_v2_box *box_u = u3a_v2_into(rod_u->rut_p)
         ; (void *)box_u < u3a_v2_into(rod_u->hat_p)
         ; box_u = (void *)((c3_w *)box_u + box_u->siz_w))
    off_w += box_u->siz_w & 1; /* odd-sized boxes are padded by one word */

  /* shift */
  memmove(u3a_v2_into(u3H_v2->rod_u.rut_p + off_w),
          u3a_v2_into(u3H_v2->rod_u.rut_p),
          hiz_z);
  /* manually zero the former rut */
  *(c3_w *)u3a_v2_into(rod_u->rut_p) = 0;

  /* relocate boxes to DWORD-aligned addresses stored in trailing size word */
  c3_w *box_w = u3a_v2_into(rod_u->rut_p + off_w);
  c3_w *end_w = u3a_v2_into(rod_u->hat_p + off_w);
  u3a_v2_box *old_u = (void *)box_w;
  c3_w siz_w = old_u->siz_w;
  u3p(c3_w) new_p = rod_u->rut_p + 1 + c3_wiseof(u3a_v2_box);
  c3_w *new_w;

  for (; box_w < end_w
         ; box_w += siz_w
         , old_u = (void *)box_w
         , siz_w = old_u->siz_w) {
    old_u->use_w &= 0x7fffffff;

    if (!old_u->use_w)
      continue;

    new_w = (void *)u3a_v2_botox(u3a_v2_into(new_p));
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
  rod_u->hat_p = new_p - c3_wiseof(u3a_v2_box);

  /* like |pack, clear the free lists and cell allocator */
  for (c3_w i_w = 0; i_w < u3a_v2_fbox_no; i_w++)
    u3R_v1->all.fre_p[i_w] = 0;

  u3R_v1->all.fre_w = 0;
  u3R_v1->all.cel_p = 0;
}


/* u3m_v2_migrate: perform loom migration if necessary.
*/
void
u3m_v2_migrate()
{
  c3_w len_w = u3C_v2.wor_i - 1;
  c3_w ver_w = *(u3_Loom + len_w);

  u3_assert( U3V_VER1 == ver_w );

  c3_w* mem_w = u3_Loom + 1;
  c3_w  siz_w = c3_wiseof(u3v_v1_home);
  c3_w* mat_w = (mem_w + len_w) - siz_w;

  u3H_v1 = (void *)mat_w;
  u3R_v1 = &u3H_v1->rod_u;

  u3R_v1->cap_p = u3R_v1->mat_p = u3a_v1_outa(u3H_v1);

  fprintf(stderr, "loom: pointer compression migration running...\r\n");

  /* perform the migration in a pattern similar to |pack */
  _migrate_reclaim();
  _migrate_seek(&u3H_v1->rod_u);
  _migrate_rewrite();
  _migrate_move(&u3H_v1->rod_u);

  /* finally update the version and commit to disk */
  u3H_v1->ver_w = U3V_VER2;

  fprintf(stderr, "loom: pointer compression migration done\r\n");
}
