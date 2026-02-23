/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


static_assert( (UINT32_MAX > u3a_cells),
               "length precision" );

static_assert(
  (UINT32_MAX < (SIZE_MAX / (3 * sizeof(u3_noun)))),
  "allocation size overflow"
);

static void
_quicksort(u3_noun* restrict arr_u,
    c3_w len_w,
    u3_noun* restrict lef_u,
    u3_noun* restrict rit_u,
    u3j_site* sit_u)
{
  if ( len_w <= 1 ) return;
  u3_noun pivot = arr_u[0];
  c3_w lef_w = 0, rit_w = 0;
  for (c3_w i_w = 1; i_w < len_w; i_w++)
  {
    u3_noun sam = u3nc(u3k(arr_u[i_w]), u3k(pivot));
    c3_o hoz_o = u3x_loob(u3j_gate_slam(sit_u, sam));
    if ( _(hoz_o) )
    {
      lef_u[lef_w++] = arr_u[i_w];
    }
    else
    {
      rit_u[rit_w++] = arr_u[i_w];
    }
  }

  arr_u[lef_w] = pivot;
  memcpy(arr_u,             lef_u, lef_w * sizeof(u3_noun));
  memcpy(arr_u + lef_w + 1, rit_u, rit_w * sizeof(u3_noun));

  _quicksort(arr_u,             lef_w, lef_u, rit_u, sit_u);
  _quicksort(arr_u + lef_w + 1, rit_w, lef_u, rit_u, sit_u);
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
  u3_noun* arr_u = u3a_malloc(sizeof(u3_noun) * len_w * 3);
  u3_noun* lef_u = arr_u + len_w;
  u3_noun* rit_u = lef_u + len_w;
  for (c3_w i_w = 0; i_w < len_w; i_w++)
  {
    //  inlined u3r_cell without any checks
    //  since the list was already validated and measured
    //
    u3a_cell* cel_u = u3a_to_ptr(list);
    arr_u[i_w] = u3k(cel_u->hed);
    list = cel_u->tel;
  }

  _quicksort(arr_u, len_w, lef_u, rit_u, sit_u);

  u3_noun pro = u3_nul;
  for (c3_w i_w = len_w; i_w--;)
  {
    pro = u3nc(arr_u[i_w], pro);
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
