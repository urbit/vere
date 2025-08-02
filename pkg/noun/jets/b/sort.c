/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


#define SWAP(l, r)    \
  do { typeof(l) t = l; l = r; r = t; } while (0)

static void
_quicksort(u3j_site* sit_u, u3_noun* arr, c3_ws low_ws, c3_ws hig_ws)
{
  if ( low_ws >= hig_ws ) return;
  
  u3_noun pivot = arr[hig_ws];
  c3_ws i_ws = low_ws;
  
  for (c3_ws j_ws = low_ws; j_ws < hig_ws; j_ws++) {
    u3_noun hoz = u3j_gate_slam(sit_u, u3nc(u3k(arr[j_ws]), u3k(pivot)));
    
    if (hoz > 1) u3m_bail(c3__exit);
    if ( _(hoz) ) {
      SWAP(arr[i_ws], arr[j_ws]);
      i_ws++;
    }
  }

  SWAP(arr[i_ws], arr[hig_ws]);

  _quicksort(sit_u, arr, low_ws, i_ws - 1);
  _quicksort(sit_u, arr, i_ws + 1, hig_ws);
}

#undef SWAP

static_assert(
  (UINT32_MAX < (SIZE_MAX / sizeof(u3_noun))),
  "len_w * sizeof u3_noun overflow"
);

static_assert( (UINT32_MAX > u3a_cells),
               "length precision" );

//  RETAINS list, transfer product
//
static u3_noun
_sort(u3j_site* sit_u, u3_noun list)
{
  if (u3_nul == list) {
    return u3_nul;
  }
  
  c3_w len_w = 1;
  {
    u3_noun lit = u3t(list);
    while ( u3_nul != lit ) {
      ++len_w; lit = u3t(lit);
    }
  }

  if (1 == len_w) {
    return u3k(list);
  }

  c3_w i_w;
  u3a_cell* cel_u;
  u3_noun* elements = u3a_malloc(sizeof(u3_noun) * len_w);
  for (i_w = 0; i_w < len_w; i_w++) {
    //  inlined u3r_cell without any checks
    //  since the list was already validated and measured
    //
    cel_u = u3a_to_ptr(list);
    elements[i_w] = cel_u->hed;
    list = cel_u->tel;
  }

  _quicksort(sit_u, elements, 0, len_w - 1);

  u3_noun pro = u3_nul;
  for (i_w = len_w; i_w--;) {
    pro = u3nc(u3k(elements[i_w]), pro);
  }
  
  u3a_free(elements);

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

  if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ) {
    return u3m_bail(c3__exit);
  } else {
    return u3qb_sort(a, b);
  }
}
