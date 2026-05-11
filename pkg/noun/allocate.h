#ifndef U3_ALLOCATE_H
#define U3_ALLOCATE_H

#include "error.h"
#include "manage.h"
#include "rsignal.h"
#include "c3/c3.h"

  /**  Constants.
  **/
    /* u3a_bits: number of bits in word-addressed pointer.  29 == 2GB.
    */
#     define u3a_bits    U3_OS_LoomBits /* 30 */

    /* u3a_vits: number of virtual bits in a noun reference gained via shifting
    */
#     define u3a_vits_h   2
#     define u3a_vits_d   0
#ifndef VERE64
#     define u3a_vits   u3a_vits_h
#else
#     define u3a_vits   u3a_vits_d
#endif

#     define u3a_word_bytes  (sizeof(c3_w))

#     define u3a_half_bits  32
#     define u3a_half_bits_log 5
#     define u3a_half_bytes_shift  (u3a_half_bits_log - 3)
#     define u3a_indirect_mask_h  0x3fffffff
#     define u3a_direct_max_h  0x7fffffff
#     define u3a_indirect_flag_h  0x80000000
#     define u3a_cell_flag_h  0xc0000000

#     define u3a_chub_bits  64
#     define u3a_chub_bits_log 6
#     define u3a_chub_bytes_shift  (u3a_chub_bits_log - 3)
#     define u3a_indirect_mask_d  0x3fffffffffffffffULL
#     define u3a_direct_max_d  0x7fffffffffffffffULL
#     define u3a_indirect_flag_d  0x8000000000000000ULL
#     define u3a_cell_flag_d  0xc000000000000000ULL

#ifndef VERE64
#     define u3a_word_bits  u3a_half_bits
#     define u3a_word_bytes_shift  u3a_half_bytes_shift
#     define u3a_word_bits_log u3a_half_bits_log
#     define u3a_indirect_mask  u3a_indirect_mask_h
#     define u3a_direct_max  u3a_direct_max_h
#     define u3a_indirect_flag  u3a_indirect_flag_h
#     define u3a_cell_flag  u3a_cell_flag_h
#     define u3a_word_words 1
#else
#     define u3a_word_bits  u3a_chub_bits
#     define u3a_word_bytes_shift  u3a_chub_bytes_shift
#     define u3a_word_bits_log u3a_chub_bits_log
#     define u3a_indirect_mask  u3a_indirect_mask_d
#     define u3a_direct_max  u3a_direct_max_d
#     define u3a_indirect_flag  u3a_indirect_flag_d
#     define u3a_cell_flag  u3a_cell_flag_d
#     define u3a_word_words 2
#endif

    /* u3a_walign: references into the loom are guaranteed to be word-aligned to:
    */
#     define u3a_walign_h  ((c3_h)1 << u3a_vits_h)
#     define u3a_walign_d  ((c3_d)1 << u3a_vits_d)
#ifndef VERE64
#     define u3a_walign  u3a_walign_h
#else
#     define u3a_walign  u3a_walign_d
#endif

     /* u3a_bits_max: max loom bex
     */
#     define u3a_bits_max_h ((c3_h)8 * sizeof(c3_h) + u3a_vits_h)
#     define u3a_bits_max_d ((c3_d)8 * sizeof(c3_d) + u3a_vits_d)
#ifndef VERE64
#     define u3a_bits_max  u3a_bits_max_h
#else
#     define u3a_bits_max  u3a_bits_max_d
#endif

    /* u3a_page: number of bits in word-addressed page.  12 == 16K page
    */
#     define u3a_page_h    12ULL
#     define u3a_page_d    11ULL
#ifndef VERE64
#     define u3a_page    u3a_page_h
#else
#     define u3a_page    u3a_page_d
#endif

    /* u3a_pages: maximum number of pages in memory.
    */
#     define u3a_pages   (1ULL << (u3a_bits + u3a_vits - u3a_page) )

    /* u3a_words: maximum number of words in memory.
    */
#     define u3a_words   ( 1ULL << (u3a_bits + u3a_vits))

    /* u3a_bytes: maximum number of bytes in memory.
    */
#     define u3a_bytes   ((sizeof(c3_w) * u3a_words))

    /* u3a_cells: number of representable cells.
    */
#     define u3a_cells   (( u3a_words / u3a_minimum ))

    /* u3a_maximum: maximum loom object size (largest possible atom).
    */
#     define u3a_maximum (u3a_words - c3_wiseof(u3a_atom))

    /* u3a_minimum: minimum loom object size.
    */
#ifndef VERE64
#     define u3a_minimum  (c3_w)4  //  actual size of a cell
#else
#     define u3a_minimum  (c3_w)2  //  half the size of a cell
#endif
//#     define u3a_minimum ((c3_w)c3_wiseof(u3a_cell))

    /* u3a_min_log: log2(u3a_minimum)
    */
#ifndef VERE64
#     define u3a_min_log  (c3_w)2
#else
#     define u3a_min_log  (c3_w)1
#endif

// XX: add static asserts for u3a_minimum, u3a_min_log, and u3a_cell (probably u3a_atom too?)

    /* u3a_crag_no: number of hunk (small allocation) sizes
    */
#     define u3a_crag_no  (u3a_page - u3a_min_log)

    /* page table constants
    */
#     define u3a_free_pg  (u3p(u3a_crag))0
#     define u3a_head_pg  (u3p(u3a_crag))1
#     define u3a_rest_pg  (u3p(u3a_crag))2

  /**  Structures.
  **/
      typedef struct {
        c3_w use_w;
        c3_w buf_w[0];
      } u3a_rate;
    /* u3a_noun: shared atom/cell prefix.
    */
      typedef struct {
        c3_w use_w;
        c3_w mug_w;
      } u3a_noun;

    /* Bit-pinned format declarations.  Both 32-bit and 64-bit loom layouts
    ** are declared unconditionally; native u3a_atom / u3a_cell / u3a_road
    ** typedef-alias the matching bitness.
    */
#     define  u3a_crag_no_h  10u
#     define  u3a_crag_no_d  10u

#     define  _ce_len_h(i)        ((size_t)(i) << (u3a_page_h + u3a_half_bytes_shift))
#     define  _ce_len_words_h(i)  ((size_t)(i) << u3a_page_h)
#     define  _ce_page_h          _ce_len_h(1)
#     define  _ce_ptr_h(i)        ((void *)((c3_c*)u3_Loom_h + _ce_len_h(i)))
#     define  _ce_len_d(i)        ((size_t)(i) << (u3a_page_d + u3a_chub_bytes_shift))
#     define  _ce_page_d          _ce_len_d(1)

#     define  u3a_is_cat_h(som)   (((som) >> 31) ? c3n : c3y)
#     define  u3a_is_pug_h(som)   ((0b10 == ((som) >> 30)) ? c3y : c3n)
#     define  u3a_is_pom_h(som)   ((0b11 == ((som) >> 30)) ? c3y : c3n)
#     define  u3a_is_cell_h(som)  u3a_is_pom_h(som)
#     define  u3a_is_cat_d(som)   (((som) >> 63) ? c3n : c3y)
#     define  u3a_is_pug_d(som)   ((0x2ULL == ((som) >> 62)) ? c3y : c3n)
#     define  u3a_is_pom_d(som)   ((0x3ULL == ((som) >> 62)) ? c3y : c3n)
#     define  u3a_is_cell_d(som)  u3a_is_pom_d(som)

#     define  u3a_to_off_h(som)   (((som) & 0x3fffffffu) << u3a_vits_h)
#     define  u3a_to_ptr_h(som)   (u3a_into_h(u3a_to_off_h(som)))
#     define  u3a_to_pug_h(off)   (((off) >> u3a_vits_h) | 0x80000000u)
#     define  u3a_to_off_d(som)   ((som) & 0x3fffffffffffffffULL)
#     define  u3a_to_ptr_d(som)   (u3a_into_d(u3a_to_off_d(som)))

      typedef struct {
        c3_h    use_w;
        c3_h    mug_w;
        c3_h    len_w;
        c3_h    buf_w[0];
      } u3a_atom_h;

      typedef struct {
        c3_h    use_w;
        c3_h    mug_w;
        u3_noun_h hed;
        u3_noun_h tel;
      } u3a_cell_h;

      typedef struct {
        c3_d     use_w;
        c3_d     mug_w;
        c3_d     len_w;
        c3_d     buf_w[0];
      } u3a_atom_d;

      typedef struct {
        c3_d       use_w;
        c3_d       mug_w;
        u3_noun_d  hed;
        u3_noun_d  tel;
      } u3a_cell_d;

#     define  u3a_head_h(som)  (((u3a_cell_h *)u3a_to_ptr_h(som))->hed)
#     define  u3a_tail_h(som)  (((u3a_cell_h *)u3a_to_ptr_h(som))->tel)
#     define  u3a_head_d(som)  (((u3a_cell_d *)u3a_to_ptr_d(som))->hed)
#     define  u3a_tail_d(som)  (((u3a_cell_d *)u3a_to_ptr_d(som))->tel)

    /* u3a_atom, u3a_cell: native logical atom and cell (alias matching bitness).
    */
#ifndef VERE64
      typedef u3a_atom_h u3a_atom;
      typedef u3a_cell_h u3a_cell;
#else
      typedef u3a_atom_d u3a_atom;
      typedef u3a_cell_d u3a_cell;
#endif

STATIC_ASSERT( (((c3_w)1) << u3a_min_log) == u3a_minimum,
               "log2 minimum allocation" );
STATIC_ASSERT( u3a_vits <= u3a_min_log,
               "virtual-bit alignment" );

    /* u3a_crag: hunk-page metadata
    */
      typedef struct _u3a_crag {
        u3p(struct _u3a_crag) nex_p;     //  next
        c3_w                  pag_w;     //  page index
        c3_s                  log_s;     //  size log2
        c3_s                  fre_s;     //  free chunks
        c3_w                  map_w[1];  //  free-chunk bitmap
      } u3a_crag;

    /* u3a_dell: page free-list entry
    */
      typedef struct _u3a_dell {
        u3p(struct _u3a_dell) nex_p;     //  next
        u3p(struct _u3a_dell) pre_p;     //  prev
        c3_w                  pag_w;     //  page index
        c3_w                  siz_w;     //  number of pages
      } u3a_dell;

    /* u3a_{32,64}_jets: 32-bit / 64-bit jet dashboard layouts.
    */
      typedef struct _u3a_jets_h {
        c3_h hot_p;
        c3_h war_p;
        c3_h cod_p;
        c3_h han_p;
        c3_h bas_p;
      } u3a_jets_h;

      typedef struct _u3a_jets_d {
        c3_d hot_p;
        c3_d war_p;
        c3_d cod_p;
        c3_d han_p;
        c3_d bas_p;
      } u3a_jets_d;

    /* u3a_jets: native jet dashboard (alias matching bitness).
    */
#ifndef VERE64
      typedef u3a_jets_h u3a_jets;
#else
      typedef u3a_jets_d u3a_jets;
#endif

    /* u3a_{32,64}_road: 32-bit and 64-bit contiguous allocation and
    ** execution context layouts.
    */
      typedef struct _u3a_road_h {
        c3_h par_p;
        c3_h kid_p;
        c3_h nex_p;

        c3_h cap_p;
        c3_h hat_p;
        c3_h mat_p;
        c3_h rut_p;
        c3_h ear_p;

        c3_h    off_w;
        c3_h    fow_w;
        c3_h    lop_p;
        u3_noun_h tim;

        c3_h fut_w[28];

        struct {
          union {
            jmp_buf buf;
            c3_h    buf_w[256];
          };
        } esc;

        struct { c3_h fag_w; } how;

        struct {
          c3_h fre_w;
          c3_h max_w;
        } all;

        struct {
          c3_h  fre_p;
          c3_h  erf_p;
          c3_h  cac_p;
          c3_hs dir_ws;
          c3_hs off_ws;
          c3_h  siz_w;
          c3_h  len_w;
          c3_h  pag_p;
          c3_h  wee_p[u3a_crag_no_h];
        } hep;

        struct {
          c3_h cel_p;
          c3_h hav_w;
          c3_h bat_w;
        } cel;

        u3a_jets_h jed;

        struct { c3_h    har_p;                } byc;
        struct { u3_noun_h gul;                  } ski;
        struct { u3_noun_h tax; u3_noun_h mer;  } bug;

        struct {
          c3_d       nox_d;
          c3_d       cel_d;
          u3_noun_h don;
          u3_noun_h trace;
          u3_noun_h day;
        } pro;

        struct {
          c3_h har_p;
          c3_h per_p;
          c3_h for_p;
        } cax;
      } u3a_road_h;

      typedef struct _u3a_road_d {
        c3_d par_p;
        c3_d kid_p;
        c3_d nex_p;

        c3_d cap_p;
        c3_d hat_p;
        c3_d mat_p;
        c3_d rut_p;
        c3_d ear_p;

        c3_d    off_w;
        c3_d    fow_w;
        c3_d    lop_p;
        u3_noun_d tim;

        c3_d fut_w[28];

        struct {
          union {
            jmp_buf buf;
            c3_d why_w;
            c3_d buf_w[256];
          };
        } esc;

        struct { c3_d fag_w; } how;

        struct {
          c3_d fre_w;
          c3_d max_w;
        } all;

        struct {
          c3_d  fre_p;
          c3_d  erf_p;
          c3_d  cac_p;
          c3_d  dir_ws;
          c3_d  off_ws;
          c3_d  siz_w;
          c3_d  len_w;
          c3_d  pag_p;
          c3_d  wee_p[u3a_crag_no_d];
        } hep;

        struct {
          c3_d cel_p;
          c3_d hav_w;
          c3_d bat_w;
        } cel;

        u3a_jets_d jed;

        struct { c3_d    har_p;                        } byc;
        struct { u3_noun_d gul;                          } ski;
        struct { u3_noun_d tax; u3_noun_d mer;          } bug;

        struct {
          c3_d       nox_d;
          c3_d       cel_d;
          u3_noun_d  don;
          u3_noun_d  trace;
          u3_noun_d  day;
        } pro;

        struct {
          c3_d har_p;
          c3_d per_p;
          c3_d for_p;
        } cax;
      } u3a_road_d;

    /* u3a_road: native contiguous allocation and execution context
    ** (alias matching bitness).
    */
#ifndef VERE64
      typedef u3a_road_h u3a_road;
#else
      typedef u3a_road_d u3a_road;
#endif
      typedef u3a_road u3_road;

    /* u3a_flag: flags for how.fag_w.  All arena related.
    */
      enum u3a_flag {
        u3a_flag_sand  = 1 << 1,              //  bump allocation (XX not impl)
        u3a_flag_cash  = 1 << 2,              //  memo cache harvesting, flows forward
      };

    /* u3a_pile: stack control, abstracted over road direction.
    */
      typedef struct _u3a_pile {
        c3_ws    mov_ws;
        c3_ws    off_ws;
        u3_post   top_p;
#ifdef U3_MEMORY_DEBUG
        u3a_road* rod_u;
#endif
      } u3a_pile;

  /**  Macros.  Should be better commented.
  **/
    /* u3a_is_cat(): yes if noun [som] is direct atom.
    */
#     define u3a_is_cat(som)    (((som) >> (u3a_word_bits - 1)) ? c3n : c3y)

    /* u3a_is_dog(): yes if noun [som] is indirect noun.
    */
#     define u3a_is_dog(som)    (((som) >> (u3a_word_bits - 1)) ? c3y : c3n)

    /* u3a_is_pug(): yes if noun [som] is indirect atom.
    */
#     define u3a_is_pug(som)    ((0b10 == ((som) >> (u3a_word_bits - 2))) ? c3y : c3n)

    /* u3a_is_pom(): yes if noun [som] is indirect cell.
    */
#     define u3a_is_pom(som)    ((0b11 == ((som) >> (u3a_word_bits - 2))) ? c3y : c3n)

    /* u3a_is_atom(): yes if noun [som] is direct atom or indirect atom.
    */
#     define u3a_is_atom(som)    c3o(u3a_is_cat(som), \
                                     u3a_is_pug(som))
#     define u3ud(som)           u3a_is_atom(som)

    /* u3a_is_cell: yes if noun [som] is cell.
    */
#     define u3a_is_cell(som)    u3a_is_pom(som)
#     define u3du(som)           u3a_is_cell(som)

    /* u3a_h(): get head of cell [som]. Bail if [som] is not cell.
    */
#     define u3a_h(som) \
        ( _(u3a_is_cell(som)) \
           ? ( ((u3a_cell *)u3a_to_ptr(som))->hed )\
           : u3m_bail(c3__exit) )
#     define u3h(som) u3a_h(som)

    /* u3a_t(): get tail of cell [som]. Bail if [som] is not cell.
    */
#     define u3a_t(som) \
        ( _(u3a_is_cell(som)) \
           ? ( ((u3a_cell *)u3a_to_ptr(som))->tel )\
           : u3m_bail(c3__exit) )
#     define u3t(som) u3a_t(som)

#     define  u3to(type, x) ((type *)u3a_into(x))
#     define  u3tn(type, x) (x) ? (type*)u3a_into(x) : (void*)NULL

#     define  u3of(type, x) (u3a_outa((type*)x))

    /* u3a_is_north(): yes if road [r] is north road.
    */
#     define  u3a_is_north(r)  __((r)->mat_p > (r)->rut_p)

    /* u3a_is_south(): yes if road [r] is south road.
    */
#     define  u3a_is_south(r)  !u3a_is_north((r))

    /* u3a_open(): words of contiguous free space in road [r]
    */
#     define  u3a_open(r)  ( (c3y == u3a_is_north(r)) \
                             ? (c3_w)((r)->cap_p - (r)->hat_p) \
                             : (c3_w)((r)->hat_p - (r)->cap_p) )

    /* u3a_full(): total words in road [r];
    ** u3a_full(r) == u3a_heap(r) + u3a_temp(r) + u3a_open(r)
    */
#     define  u3a_full(r)  ( (c3y == u3a_is_north(r)) \
                             ? (c3_w)((r)->mat_p - (r)->rut_p) \
                             : (c3_w)((r)->rut_p - (r)->mat_p) )

    /* u3a_heap(): words of heap in road [r]
    */
#     define  u3a_heap(r)  ( (c3y == u3a_is_north(r)) \
                             ? (c3_w)((r)->hat_p - (r)->rut_p) \
                             : (c3_w)((r)->rut_p - (r)->hat_p) )

    /* u3a_temp(): words of stack in road [r]
    */
#     define  u3a_temp(r)  ( (c3y == u3a_is_north(r)) \
                             ? (c3_w)((r)->mat_p - (r)->cap_p) \
                             : (c3_w)((r)->cap_p - (r)->mat_p) )

#     define  u3a_north_is_senior(r, dog) \
                __((u3a_to_off(dog) < (r)->rut_p) ||  \
                       (u3a_to_off(dog) >= (r)->mat_p))

#     define  u3a_north_is_junior(r, dog) \
                __((u3a_to_off(dog) >= (r)->cap_p) && \
                       (u3a_to_off(dog) < (r)->mat_p))

#     define  u3a_north_is_normal(r, dog) \
                c3a(!(u3a_north_is_senior(r, dog)),  \
                       !(u3a_north_is_junior(r, dog)))

#     define  u3a_south_is_senior(r, dog) \
                __((u3a_to_off(dog) < (r)->mat_p) || \
                       (u3a_to_off(dog) >= (r)->rut_p))

#     define  u3a_south_is_junior(r, dog) \
                __((u3a_to_off(dog) < (r)->cap_p) && \
                       (u3a_to_off(dog) >= (r)->mat_p))

#     define  u3a_south_is_normal(r, dog) \
                c3a(!(u3a_south_is_senior(r, dog)),  \
                       !(u3a_south_is_junior(r, dog)))

#     define  u3a_is_junior(r, som) \
                ( _(u3a_is_cat(som)) \
                      ?  c3n \
                      :  _(u3a_is_north(r)) \
                         ?  u3a_north_is_junior(r, som) \
                         :  u3a_south_is_junior(r, som) )

#     define  u3a_is_senior(r, som) \
                ( _(u3a_is_cat(som)) \
                      ?  c3y \
                      :  _(u3a_is_north(r)) \
                         ?  u3a_north_is_senior(r, som) \
                         :  u3a_south_is_senior(r, som) )

#     define  u3a_is_mutable(r, som) \
                ( _(u3a_is_atom(som)) \
                  ? c3n \
                  : _(u3a_is_senior(r, som)) \
                  ? c3n \
                  : _(u3a_is_junior(r, som)) \
                  ? c3n \
                  : (((u3a_rate*)u3a_to_ptr(som))->use_w == 1) \
                  ? c3y : c3n )

/* like _box_vaal but for rods. Again, probably want to prefix validation
   functions at the very least. Maybe they can be defined in their own header.

   ps. while arguably cooler to have this compile to

   do {(void(0));(void(0));} while(0)

   It may be nicer to just wrap an inline function in #ifdef C3DBG guards. You
   could even return the then validated road like

   u3a_road f() {
   u3a_road rod_u;
   ...
   return _rod_vaal(rod_u);
   }
*/
#     define _rod_vaal(rod_u)                                           \
             do {                                                       \
               c3_dessert(((uintptr_t)((u3a_road*)(rod_u))->hat_p       \
                           & u3a_walign-1) == 0);                     \
             } while(0)


typedef struct _u3a_mark {
  c3_w    wee_w[u3a_crag_no];
  c3_w*   bit_w;
  //  XX factor out?
  c3_w    siz_w;
  c3_w    len_w;
  c3_w*   buf_w;
} u3a_mark;

typedef struct _u3a_gack {
  c3_w *bit_w;  //  mark bits
  c3_w *pap_w;  //  global page bitmap
  c3_w *pum_w;  //  global cumulative sums
  //  XX factor out?
  c3_w  siz_w;
  c3_w  len_w;
  c3_w *buf_w;
} u3a_gack;

typedef struct {
  c3_s log_s;     //  size log2
  c3_s len_s;     //  1U << log_s
  c3_s tot_s;     //  total chunks
  c3_s map_s;     //  bitmap size
  c3_s siz_s;     //  wiseof(crag) - 1 + map_s
  c3_s hun_s;     //  chunks reserved
  c3_s ful_s;     //  tot_s - hun_s
} u3a_hunk_dose;

      extern u3a_gack u3a_Gack;

      extern u3a_mark u3a_Mark;

      extern u3a_hunk_dose u3a_Hunk[u3a_crag_no];

  /**  Globals.
  **/
      /// Current road (thread-local).
      extern u3_road* u3a_Road;
#       define u3R  u3a_Road

    /* u3_Code: memory code.
    */
#ifdef U3_MEMORY_DEBUG
      extern c3_w u3_Code;
#endif

#ifdef VERE64
#   define u3_Loom_d   ((c3_d *)(void *)U3_OS_LoomBase)
#   define u3_Loom_h   ((c3_h *)((c3_c *)U3_OS_LoomBase + ((c3_z)8 << u3a_bits_max_h)))
#   define u3_Loom      u3_Loom_d
#else
#   define u3_Loom_h   ((c3_h *)(void *)U3_OS_LoomBase)
#   define u3_Loom_d   ((c3_d *)((c3_c *)U3_OS_LoomBase + ((c3_z)8 << u3a_bits_max_h)))
#   define u3_Loom      u3_Loom_h
#endif

  /* u3a_{32,64}_into(): convert loom offset [x] into generic pointer.
   */
#   define u3a_into_h(x)  ((void *)(u3_Loom_h + (x)))
#   define u3a_into_d(x)  ((void *)(u3_Loom_d + (x)))

  /* u3a_{32,64}_outa(): convert pointer [p] into word offset into loom.
   */
#   define u3a_outa_h(p)  ((c3_h *)(void *)(p) - u3_Loom_h)
#   define u3a_outa_d(p)  ((c3_d *)(void *)(p) - u3_Loom_d)

  /* u3a_into(): convert loom offset [x] into generic pointer.
   */
#ifdef VERE64
#   define u3a_into(x)  u3a_into_d(x)
#   define u3a_outa(p)  u3a_outa_d(p)
#else
#   define u3a_into(x)  u3a_into_h(x)
#   define u3a_outa(p)  u3a_outa_h(p)
#endif

  /* u3a_to_off(): mask off bits 30 and 31 from noun [som].
   */
#   define u3a_to_off(som)  (((som) & u3a_indirect_mask) << u3a_vits)

  /* u3a_to_ptr(): convert noun [som] into generic pointer into loom.
   */
#   define u3a_to_ptr(som)  (u3a_into(u3a_to_off(som)))

  /* u3a_to_wtr(): convert noun [som] into word pointer into loom.
   */
#   define u3a_to_wtr(som)  ((c3_w *)u3a_to_ptr(som))

  /**  Inline functions.
  **/
  /* u3a_to_pug(): set indirect atom flag bit of [off].
   */
  inline c3_w u3a_to_pug(c3_w off) {
    c3_dessert((off & u3a_walign-1) == 0);
    return (off >> u3a_vits) | u3a_indirect_flag;
  }

  /* u3a_to_pom(): set indirect cell flag bits of [off].
   */
  inline c3_w u3a_to_pom(c3_w off) {
    c3_dessert((off & u3a_walign-1) == 0);
    return (off >> u3a_vits) | u3a_cell_flag;
  }

    /**  road stack.
    **/
        /* u3a_drop(): drop a road stack frame per [pil_u].
        */
          inline void
          u3a_drop(const u3a_pile* pil_u)
          {
            u3R->cap_p -= pil_u->mov_ws;
          }

        /* u3a_peek(): examine the top of the road stack.
        */
          inline void*
          u3a_peek(const u3a_pile* pil_u)
          {
            return u3to(void, (u3R->cap_p + pil_u->off_ws));
          }

        /* u3a_pop(): drop a road stack frame, peek at the new top.
        */
          inline void*
          u3a_pop(const u3a_pile* pil_u)
          {
            u3a_drop(pil_u);
            return u3a_peek(pil_u);
          }

        /* u3a_push(): push a frame onto the road stack, per [pil_u].
        */
          inline void*
          u3a_push(const u3a_pile* pil_u)
          {
            u3R->cap_p += pil_u->mov_ws;

#ifndef U3_GUARD_PAGE
            //  !off means we're on a north road
            //
            if ( !pil_u->off_ws ) {
              if( !(u3R->cap_p > u3R->hat_p) ) {
                u3m_bail(c3__meme);
              }
# ifdef U3_MEMORY_DEBUG
              u3_assert( pil_u->top_p >= u3R->cap_p );
# endif
            }
            else {
              if( !(u3R->cap_p < u3R->hat_p) ) {
                u3m_bail(c3__meme);
              }
# ifdef U3_MEMORY_DEBUG
              u3_assert( pil_u->top_p <= u3R->cap_p );
# endif
            }
#endif /* ifndef U3_GUARD_PAGE */

#ifdef U3_MEMORY_DEBUG
            u3_assert( pil_u->rod_u == u3R );
#endif

            return u3a_peek(pil_u);
          }

        /* u3a_pile_done(): assert valid upon completion.
        */
          inline c3_o
          u3a_pile_done(const u3a_pile* pil_u)
          {
            return (pil_u->top_p == u3R->cap_p) ? c3y : c3n;
          }

  /**  Functions.
  **/

void
u3a_init_once(void);
void
u3a_init_heap(void);
void
u3a_drop_heap(u3_post cap_p, u3_post ear_p);
void
u3a_mark_init(void);
void*
u3a_mark_alloc(c3_w len_w);
void
u3a_mark_done(void);

void
u3a_pack_init(void);
void*
u3a_pack_alloc(c3_w len_w);
void
u3a_pack_done(void);

void*
u3a_into_fn(u3_post);
u3_post
u3a_outa_fn(void*);
u3_post
u3a_to_off_fn(u3_noun);
u3a_noun*
u3a_to_ptr_fn(u3_noun);
u3_noun
u3a_head(u3_noun);
u3_noun
u3a_tail(u3_noun);
void
u3a_post_info(u3_post);

    /**  Allocation.
    **/
      /* Word-aligned allocation.
      */
        /* u3a_walloc(): allocate storage measured in words.
        */
          void*
          u3a_walloc(c3_w len_w);

        /* u3a_celloc(): allocate a cell.  Faster, sometimes.
        */
          c3_w*
          u3a_celloc(void);

        /* u3a_wfree(): free storage.
        */
          void
          u3a_wfree(void* lag_v);

        /* u3a_wtrim(): trim storage.
        */
          void
          u3a_wtrim(void* tox_v, c3_w old_w, c3_w len_w);

        /* u3a_wealloc(): word realloc.
        */
          void*
          u3a_wealloc(void* lag_v, c3_w len_w);

        /* u3a_pile_prep(): initialize stack control.
        */
          void
          u3a_pile_prep(u3a_pile* pil_u, c3_w len_w);

      /* C-style aligned allocation - *not* compatible with above.
      */
        /* u3a_malloc(): aligned storage measured in bytes.
        */
          void*
          u3a_malloc(size_t len_i);

        /* u3a_calloc(): aligned storage measured in bytes.
        */
          void*
          u3a_calloc(size_t num_i, size_t len_i);

        /* u3a_realloc(): aligned realloc in bytes.
        */
          void*
          u3a_realloc(void* lag_v, size_t len_i);

        /* u3a_free(): free for aligned malloc.
        */
          void
          u3a_free(void* tox_v);

      /* Reference and arena control.
      */
        /* u3a_gain(): gain a reference count in normal space.
        */
          u3_weak
          u3a_gain(u3_weak som);
#         define u3k(som) u3a_gain(som)

        /* u3a_take(): gain, copying juniors.
        */
          u3_noun
          u3a_take(u3_noun som);

        /* u3a_left(): true of junior if preserved.
        */
          c3_o
          u3a_left(u3_noun som);

        /* u3a_lose(): lose a reference.
        */
          void
          u3a_lose(u3_weak som);
#         define u3z(som) u3a_lose(som)

        /* u3a_wash(): wash all lazy mugs in subtree.  RETAIN.
        */
          void
          u3a_wash(u3_noun som);

        /* u3a_use(): reference count.
        */
          c3_w
          u3a_use(u3_noun som);

        /* u3a_wed(): unify noun references.
        */
          void
          u3a_wed(u3_noun *restrict a, u3_noun *restrict b);

        /* u3a_luse(): check refcount sanity.
        */
          void
          u3a_luse(u3_noun som);

        /* u3a_mark_ptr(): mark a pointer for gc.  Produce size.
        */
          c3_w
          u3a_mark_ptr(void* ptr_v);

        /* u3a_mark_mptr(): mark a u3_malloc-allocated ptr for gc.
        */
          c3_w
          u3a_mark_mptr(void* ptr_v);

        /* u3a_mark_noun(): mark a noun for gc.  Produce size.
        */
          c3_w
          u3a_mark_noun(u3_noun som);

        /* u3a_mark_road(): mark ad-hoc persistent road structures.
        */
          u3m_quac*
          u3a_mark_road();

        /* u3a_reclaim(): clear ad-hoc persistent caches to reclaim memory.
        */
          void
          u3a_reclaim(void);

        /* u3a_relocate_post(): replace post with relocation pointer (unchecked).
        */
          void
          u3a_relocate_post(u3_post *som_p);

        /* u3a_mark_relocate_post(): replace post with relocation pointer (checked).
        */
          u3_post
          u3a_mark_relocate_post(u3_post som_p, c3_t *fir_t);

        /* u3a_relocate_noun(): replace noun with relocation reference, recursively.
        */
          void
          u3a_relocate_noun(u3_noun *som);

        /* u3a_rewrite_compact(): rewrite pointers in ad-hoc persistent road structures.
        */
          void
          u3a_rewrite_compact(void);

        /* u3a_count_noun(): count size of noun.
        */
          c3_w
          u3a_count_noun(u3_noun som);

        /* u3a_discount_noun(): clean up after counting a noun.
        */
          c3_w
          u3a_discount_noun(u3_noun som);

        /* u3a_count_ptr(): count a pointer for gc.  Produce size.  */
          c3_w
          u3a_count_ptr(void* ptr_v);

        /* u3a_discount_ptr(): discount a pointer for gc.  Produce size.  */
          c3_w
          u3a_discount_ptr(void* ptr_v);

        /* u3a_idle(): measure free-lists in [rod_u]
        */
          c3_w
          u3a_idle(u3a_road* rod_u);

        /* u3a_ream(): ream free-lists.
        */
          void
          u3a_ream(void);

void
u3a_wait(void);

void
u3a_dash(void);

        /* u3a_sweep(): sweep a fully marked road.
        */
          c3_w
          u3a_sweep(void);

        /* u3a_pack_seek(): sweep the heap, modifying boxes to record new addresses.
        */
          void
          u3a_pack_seek(u3a_road* rod_u);

        /* u3a_pack_move(): sweep the heap, moving boxes to new addresses.
        */
          void
          u3a_pack_move(u3a_road* rod_u);

        /* u3a_sane(): check allocator sanity.
        */
          void
          u3a_sane(void);

        /* u3a_lush(): leak push.
        */
          c3_w
          u3a_lush(c3_w lab_w);

        /* u3a_lop(): leak pop.
        */
          void
          u3a_lop(c3_w lab_w);

        /* u3a_print_time: print microsecond time.
        */
          void
          u3a_print_time(c3_c* str_c, c3_c* cap_c, c3_d mic_d);

        /* u3a_print_quac: print a quac memory report.
        */
          void
          u3a_print_quac(FILE* fil_u, c3_h den_h, u3m_quac* mas_u);

        /* u3a_print_memory(): print memory amount to file descriptor.
        */
          void
          u3a_print_memory(FILE* fil_u, c3_c* cap_c, c3_w wor_w);

        /* u3a_print_memory(): print memory amount to string.
        */
          void
          u3a_print_memory_str(c3_c* str_c, c3_c* cap_c, c3_w wor_w);

        /* u3a_prof(): mark/measure/print memory profile. RETAIN.
        */
          u3m_quac*
          u3a_prof(FILE* fil_u, u3_noun mas);

        /* u3a_maid(): maybe print memory.
        */
          c3_w
          u3a_maid(FILE* fil_u, c3_c* cap_c, c3_w wor_w);

        /* u3a_quac_free(): free quac memory.
        */
          void
          u3a_quac_free(u3m_quac* qua_u);

        /* u3a_uncap_print_memory(): un-captioned print memory amount.
        */
          void
          u3a_uncap_print_memory(FILE* fil_u, c3_w byt_w);

        /* u3a_deadbeef(): write 0xdeadbeef from hat to cap.
        */
          void
          u3a_deadbeef(void);

        /* u3a_walk_fore(): preorder traversal, visits ever limb of a noun.
        **
        **   cells are visited *before* their heads and tails
        **   and can shortcircuit traversal by returning [c3n]
        */
          void
          u3a_walk_fore(u3_noun    a,
                        void*      ptr_v,
                        void     (*pat_f)(u3_atom, void*),
                        c3_o     (*cel_f)(u3_noun, void*));

        /* u3a_string(): `a` as an on-loom c-string.
        */
          c3_c*
          u3a_string(u3_atom a);

        /* u3a_loom_sane(): sanity checks the state of the loom for obvious corruption
        */
          void
          u3a_loom_sane(void);

#endif /* ifndef U3_ALLOCATE_H */
