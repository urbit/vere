/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


static_assert( (UINT32_MAX > u3a_cells),
               "length precision" );

static_assert(
  (UINT32_MAX < (SIZE_MAX / (2 * sizeof(u3_noun)))),
  "len_w * sizeof u3_noun overflow"
);

static void
_merge_sort(u3_noun* arr_u,
  u3_noun* tmp_u,
  c3_w lef_w,
  c3_w rit_w,
  u3j_site* sit_u)
{
  if ( lef_w >= rit_w ) return;
  c3_w mid_w =  (lef_w + rit_w) / 2;
  if (mid_w < lef_w)
  {
    //  addition wrapped around
    //
    u3m_bail(c3__fail);
  }
  _merge_sort(arr_u, tmp_u, lef_w, mid_w, sit_u);
  _merge_sort(arr_u, tmp_u, mid_w + 1, rit_w, sit_u);

  c3_w i_w = lef_w, j_w = mid_w + 1, k_w = lef_w;
  while (i_w <= mid_w && j_w <= rit_w)
  {
    //  reversed comparison to mimick order reversal of pivot
    //  and compared element in Hoon
    //
    u3_noun sam = u3nc(u3k(arr_u[j_w]), u3k(arr_u[i_w]));
    u3_noun hoz = u3j_gate_slam(sit_u, sam);
    if ( hoz > 1 ) u3m_bail(c3__exit);
    if ( c3n == hoz )
      tmp_u[k_w++] = arr_u[i_w++];
    else
      tmp_u[k_w++] = arr_u[j_w++];
  }

  while (i_w <= mid_w) tmp_u[k_w++] = arr_u[i_w++];
  while (j_w <= rit_w) tmp_u[k_w++] = arr_u[j_w++];

  for (i_w = lef_w; i_w <= rit_w; i_w++)
  {
    arr_u[i_w] = tmp_u[i_w];
  }
}

//  RETAINS list, transfers product
//
static u3_noun
_sort(u3j_site* sit_u, u3_noun list)
{
  if (u3_nul == list) return u3_nul;

  c3_w len_w = 1;
  {
    u3_noun t = u3t(list);
    while (u3_nul != t)
    {
      ++len_w; t = u3t(t);
    }
  }

  if (1 == len_w) return u3k(list);
  u3_noun* arr_u = u3a_malloc(sizeof(u3_noun) * len_w * 2);
  u3_noun* tmp_u = arr_u + len_w;
  for (c3_w i_w = 0; i_w < len_w; i_w++)
  {
    //  inlined u3r_cell without any checks
    //  since the list was already validated and measured
    //
    u3a_cell* cel_u = u3a_to_ptr(list);
    arr_u[i_w] = cel_u->hed;
    list = cel_u->tel;
  }

  _merge_sort(arr_u, tmp_u, 0, len_w - 1, sit_u);

  u3_noun pro = u3_nul;
  for (c3_w i_w = len_w; i_w--;)
  {
    pro = u3nc(u3k(arr_u[i_w]), pro);
  }
  
  u3a_free(arr_u);

  return pro;
}

u3_noun
u3qb_sort(u3_noun a,
          u3_noun b)
{
  u3_noun  pro;
  u3j_site sit_u;
  u3j_gate_prep(&sit_u, u3k(b));
  pro = _sort(&sit_u, a);
  u3j_gate_lose(&sit_u);
  return pro;
}

u3_noun
u3wb_sort(u3_noun cor)
{
  u3_noun a, b;

  if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) )
  {
    return u3m_bail(c3__exit);
  }
  else
  {
    return u3qb_sort(a, b);
  }
}
