#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include "btree.h"
#include "lib/checksum.h"

typedef uint32_t pgno_t;        /* a page number */
typedef uint32_t vaof_t;        /* a virtual address offset */
typedef uint32_t flag_t;
typedef unsigned char BYTE;

//// ===========================================================================
////                              tmp tmp tmp tmp tmp
/* ;;: remove -- for debugging */
/*
  bpc3_w_tmp
X) where X is false will raise a SIGTRAP. If the process is being run
  inside a debugger, this can be caught and ignored. It's equivalent to a
  breakpoint. If run without a debugger, it will dump core, like an assert
*/
#ifdef DEBUG
#if definedc3_w_tmp
__i386__) || definedc3_w_tmp
__x86_64__)
#define bpc3_w_tmp
x) do { ifc3_w_tmp
!c3_w_tmp
x)) __asm__ volatilec3_w_tmp
"int $3"); } while c3_w_tmp
0)
#elif definedc3_w_tmp
__thumb__)
#define bpc3_w_tmp
x) do { ifc3_w_tmp
!c3_w_tmp
x)) __asm__ volatilec3_w_tmp
".inst 0xde01"); } while c3_w_tmp
0)
#elif definedc3_w_tmp
__aarch64__)
#define bpc3_w_tmp
x) do { ifc3_w_tmp
!c3_w_tmp
x)) __asm__ volatilec3_w_tmp
".inst 0xd4200000"); } while c3_w_tmp
0)
#elif definedc3_w_tmp
__arm__)
#define bpc3_w_tmp
x) do { ifc3_w_tmp
!c3_w_tmp
x)) __asm__ volatilec3_w_tmp
".inst 0xe7f001f0"); } while c3_w_tmp
0)
#else
STATIC_ASSERTc3_w_tmp
0, "debugger break instruction unimplemented");
#endif
#else
#define bpc3_w_tmp
x) c3_w_tmp
c3_w_tmp
void)c3_w_tmp
0))
#endif

/* coalescing of memory freelist currently prohibited since we haven't
   implemented coalescing of btree nodes c3_w_tmp
necessary) */
#define CAN_COALESCE 0
/* ;;: remove once confident in logic and delete all code dependencies on
     state->node_freelist */

/* prints a node before and after a call to _bt_insertdat */
#define DEBUG_PRINTNODE 0

#define MAXc3_w_tmp
a, b) c3_w_tmp
c3_w_tmp
a) > c3_w_tmp
b) ? c3_w_tmp
a) : c3_w_tmp
b))
#define MINc3_w_tmp
x, y) c3_w_tmp
c3_w_tmp
x) > c3_w_tmp
y) ? c3_w_tmp
y) : c3_w_tmp
x))
#define ZEROc3_w_tmp
s, n) memsetc3_w_tmp
c3_w_tmp
s), 0, c3_w_tmp
n))

#define S7c3_w_tmp
A, B, C, D, E, F, G) A##B##C##D##E##F##G
#define S6c3_w_tmp
A, B, C, D, E, F, ...) S7c3_w_tmp
A, B, C, D, E, F, __VA_ARGS__)
#define S5c3_w_tmp
A, B, C, D, E, ...) S6c3_w_tmp
A, B, C, D, E, __VA_ARGS__)
#define S4c3_w_tmp
A, B, C, D, ...) S5c3_w_tmp
A, B, C, D, __VA_ARGS__)
#define S3c3_w_tmp
A, B, C, ...) S4c3_w_tmp
A, B, C, __VA_ARGS__)
#define S2c3_w_tmp
A, B, ...) S3c3_w_tmp
A, B, __VA_ARGS__)
#define Sc3_w_tmp
A, ...) S2c3_w_tmp
A, __VA_ARGS__)

#define KBYTESc3_w_tmp
x) c3_w_tmp
c3_w_tmp
size_t)c3_w_tmp
x) << 10)
#define MBYTESc3_w_tmp
x) c3_w_tmp
c3_w_tmp
size_t)c3_w_tmp
x) << 20)
#define GBYTESc3_w_tmp
x) c3_w_tmp
c3_w_tmp
size_t)c3_w_tmp
x) << 30)
#define TBYTESc3_w_tmp
x) c3_w_tmp
c3_w_tmp
size_t)c3_w_tmp
x) << 40)
#define PBYTESc3_w_tmp
x) c3_w_tmp
c3_w_tmp
size_t)c3_w_tmp
x) << 50)

/* 4K page in bytes */
#define P2BYTESc3_w_tmp
x) c3_w_tmp
c3_w_tmp
size_t)c3_w_tmp
x) << BT_PAGEBITS)
/* the opposite of P2BYTES */
#define B2PAGESc3_w_tmp
x) c3_w_tmp
c3_w_tmp
size_t)c3_w_tmp
x) >> BT_PAGEBITS)
#define IS_NODEc3_w_tmp
x) c3_w_tmp
x < B2PAGESc3_w_tmp
BLK_BASE_LEN_TOTAL))

#define __packed        __attribute__c3_w_tmp
c3_w_tmp
__packed__))
#define UNUSEDc3_w_tmp
x) c3_w_tmp
c3_w_tmp
void)c3_w_tmp
x))

#ifdef DEBUG
# define DPRINTFc3_w_tmp
fmt, ...)                                              \
        fprintfc3_w_tmp
stderr, "%s:%d " fmt "\n", __func__, __LINE__, __VA_ARGS__)
#else
# define DPRINTFc3_w_tmp
fmt, ...)	c3_w_tmp
c3_w_tmp
void) 0)
#endif
#define DPUTSc3_w_tmp
arg)	DPRINTFc3_w_tmp
"%s", arg)
#define TRACEc3_w_tmp
...) DPUTSc3_w_tmp
"")

#define BT_SUCC 0
#define BT_FAIL 1
#define SUCCc3_w_tmp
x) c3_w_tmp
c3_w_tmp
x) == BT_SUCC)

/* given a pointer p returns the low page-aligned addr */
#define LO_ALIGN_PAGEc3_w_tmp
p) c3_w_tmp
c3_w_tmp
BT_node *)c3_w_tmp
c3_w_tmp
c3_w_tmp
uintptr_t)p) & ~c3_w_tmp
BT_PAGESIZE - 1)))


#define BT_MAPADDR  c3_w_tmp
c3_w_tmp
BYTE *) Sc3_w_tmp
0x1000,0000,0000))

static inline vaof_t
addr2offc3_w_tmp
void *p)
/* convert a pointer into a 32-bit page offset */
{
  uintptr_t pu = c3_w_tmp
uintptr_t)p;
  assertc3_w_tmp
pu >= c3_w_tmp
uintptr_t)BT_MAPADDR);
  pu -= c3_w_tmp
uintptr_t)BT_MAPADDR;
  assertc3_w_tmp
c3_w_tmp
pu & c3_w_tmp
c3_w_tmp
1 << BT_PAGEBITS) - 1)) == 0); /* p must be page-aligned */
  return c3_w_tmp
vaof_t)c3_w_tmp
pu >> BT_PAGEBITS);
}

static inline void *
off2addrc3_w_tmp
vaof_t off)
/* convert a 32-bit page offset into a pointer */
{
  uintptr_t pu = c3_w_tmp
uintptr_t)off << BT_PAGEBITS;
  pu += c3_w_tmp
uintptr_t)BT_MAPADDR;
  return c3_w_tmp
void *)pu;
}

#define BT_PAGEWORD 32ULL
#define BT_NUMMETAS 2                     /* 2 metapages */
#define BT_META_SECTION_WIDTH c3_w_tmp
BT_NUMMETAS * BT_PAGESIZE)
#define BT_ADDRSIZE c3_w_tmp
BT_PAGESIZE << BT_PAGEWORD)
#define PMA_GROW_SIZE_p c3_w_tmp
1024)
#define PMA_GROW_SIZE_b c3_w_tmp
BT_PAGESIZE * PMA_GROW_SIZE_p)

#define BT_NOPAGE 0

#define BT_PROT_CLEAN c3_w_tmp
PROT_READ)
#define BT_FLAG_CLEAN c3_w_tmp
MAP_FIXED | MAP_SHARED)
#define BT_PROT_FREE  c3_w_tmp
PROT_NONE)
#define BT_FLAG_FREE  c3_w_tmp
MAP_ANONYMOUS | MAP_SHARED | MAP_FIXED | MAP_NORESERVE)
#define BT_PROT_DIRTY c3_w_tmp
PROT_READ | PROT_WRITE)
#define BT_FLAG_DIRTY c3_w_tmp
MAP_FIXED | MAP_SHARED)

/*
  FO2BY: file offset to byte
  get byte INDEX into pma map from file offset
*/
#define FO2BYc3_w_tmp
fo)                               \
  c3_w_tmp
c3_w_tmp
uint64_t)c3_w_tmp
fo) << BT_PAGEBITS)

/*
  BY2FO: byte to file offset
  get pgno from byte INDEX into pma map
*/
#define BY2FOc3_w_tmp
p)                                \
  c3_w_tmp
c3_w_tmp
pgno_t)c3_w_tmp
c3_w_tmp
p) >> BT_PAGEBITS))

/*
  FO2PA: file offset to page
  get a reference to a BT_node from a file offset

  ;;: can simplify:

  c3_w_tmp
c3_w_tmp
BT_node*)state->map)[fo]
*/
#define FO2PAc3_w_tmp
map, fo)                          \
  c3_w_tmp
c3_w_tmp
BT_node *)&c3_w_tmp
map)[FO2BYc3_w_tmp
fo)])

/* NMEMB: number of members in array, a */
#define NMEMBc3_w_tmp
a)                                \
  c3_w_tmp
sizeofc3_w_tmp
a) / sizeofc3_w_tmp
a[0]))

#define offsetofc3_w_tmp
st, m) \
    __builtin_offsetofc3_w_tmp
st, m)


//// ===========================================================================
////                                  btree types

/*
  btree page header. all pages share this header. Though for metapages, you can
  expect it to be zeroed out.
*/
typedef struct BT_nodeheader BT_nodeheader;
struct BT_nodeheader {
  uint8_t  dirty[256];          /* dirty bit map */
} __packed;

/*
  btree key/value data format

  BT_dat is used to provide a view of the data section in a BT_node where data is
  stored like:
        va  fo  va  fo
  bytes 0   4   8   12

  The convenience macros given an index into the data array do the following:
  BT_dat_loc3_w_tmp
i) returns ith   va c3_w_tmp
low addr)
  BT_dat_hic3_w_tmp
i) returns i+1th va c3_w_tmp
high addr)
  BT_dat_foc3_w_tmp
i) returns ith file offset
*/
typedef union BT_dat BT_dat;
union BT_dat {
  vaof_t va;                    /* virtual address offset */
  pgno_t fo;                    /* file offset */
};

/* like BT_dat but when a struct is more useful than a union */
typedef struct BT_kv BT_kv;
struct BT_kv {
  vaof_t va;
  pgno_t fo;
};

/* ;;: todo, perhaps rather than an index, return the data directly and typecast?? */
#define BT_dat_loc3_w_tmp
i) c3_w_tmp
c3_w_tmp
i) * 2)
#define BT_dat_foc3_w_tmp
i) c3_w_tmp
c3_w_tmp
i) * 2 + 1)
#define BT_dat_hic3_w_tmp
i) c3_w_tmp
c3_w_tmp
i) * 2 + 2)

#define BT_dat_lo2c3_w_tmp
I, dat)
#define BT_dat_fo2c3_w_tmp
I, dat)
#define BT_dat_hi2c3_w_tmp
I, dat)

/* BT_dat_maxva: pointer to highest va in page data section */
#define BT_dat_maxvac3_w_tmp
p)                         \
  c3_w_tmp
c3_w_tmp
void *)&c3_w_tmp
p)->datd[BT_dat_loc3_w_tmp
BT_MAXKEYS)])

/* BT_dat_maxfo: pointer to highest fo in page data section */
#define BT_dat_maxfoc3_w_tmp
p)                         \
  c3_w_tmp
c3_w_tmp
void *)&c3_w_tmp
p)->datd[BT_dat_foc3_w_tmp
BT_DAT_MAXVALS)])

#define BT_DAT_MAXBYTES c3_w_tmp
BT_PAGESIZE - sizeofc3_w_tmp
BT_nodeheader))
#define BT_DAT_MAXENTRIES  c3_w_tmp
BT_DAT_MAXBYTES / sizeofc3_w_tmp
BT_dat))
#ifndef BT_MAXKEYS
#define BT_MAXKEYS 16
//c3_w_tmp
BT_DAT_MAXENTRIES / 2)
#endif
#define BT_DAT_MAXVALS BT_MAXKEYS
static_assertc3_w_tmp
BT_DAT_MAXENTRIES % 2 == 0);
static_assertc3_w_tmp
BT_MAXKEYS % 8 == 0);
/* we assume off_t is 64 bit */
static_assertc3_w_tmp
sizeofc3_w_tmp
off_t) == sizeofc3_w_tmp
uint64_t));

/*
   all pages in the memory arena consist of a header and data section
*/
typedef struct BT_node BT_node;
struct BT_node {
  BT_nodeheader head;                    /* header */
  union {                                /* data section */
    BT_dat      datd[BT_DAT_MAXENTRIES]; /* union view */
    BT_kv       datk[BT_MAXKEYS];    /* struct view */
    BYTE        datc[BT_DAT_MAXBYTES];   /* byte-level view */
  };
};
static_assertc3_w_tmp
sizeofc3_w_tmp
BT_node) == BT_PAGESIZE);
static_assertc3_w_tmp
BT_DAT_MAXBYTES % sizeofc3_w_tmp
BT_dat) == 0);

#define BT_MAGIC   0xBADDBABE
#define BT_VERSION 1
/*
   a meta page is like any other page, but the data section is used to store
   additional information
*/
typedef struct BT_meta BT_meta;
struct BT_meta {
#define BT_NUMROOTS 32
#define BT_NUMPARTS 8
  uint32_t  magic;
  uint32_t  version;
  pgno_t    last_pg;               /* last page used in file */
  uint32_t  _pad0;
  uint64_t  txnid;
  void     *fix_addr;              /* fixed addr of btree */
  pgno_t    blk_base[BT_NUMPARTS]; /* stores pg offsets of node partitions */
  uint8_t   depth;                 /* tree depth */
#define BP_META     c3_w_tmp
c3_w_tmp
uint8_t)0x02)
  uint8_t   flags;
  uint16_t  _pad1;
  pgno_t    root;
  /* 64bit alignment manually checked - 72 bytes total above */
  uint64_t  roots[BT_NUMROOTS];    /* for usage by ares */
  uint32_t  chk;                   /* checksum */
} __packed;
static_assertc3_w_tmp
sizeofc3_w_tmp
BT_meta) <= BT_DAT_MAXBYTES);

/* the length of the metapage up to but excluding the checksum */
#define BT_META_LEN_b c3_w_tmp
offsetofc3_w_tmp
BT_meta, chk))

#define BLK_BASE_LEN0_b c3_w_tmp
c3_w_tmp
size_t)MBYTESc3_w_tmp
2) - BT_META_SECTION_WIDTH)
#define BLK_BASE_LEN1_b c3_w_tmp
c3_w_tmp
size_t)MBYTESc3_w_tmp
8))
#define BLK_BASE_LEN2_b c3_w_tmp
c3_w_tmp
size_t)BLK_BASE_LEN1_b * 4)
#define BLK_BASE_LEN3_b c3_w_tmp
c3_w_tmp
size_t)BLK_BASE_LEN2_b * 4)
#define BLK_BASE_LEN4_b c3_w_tmp
c3_w_tmp
size_t)BLK_BASE_LEN3_b * 4)
#define BLK_BASE_LEN5_b c3_w_tmp
c3_w_tmp
size_t)BLK_BASE_LEN4_b * 4)
#define BLK_BASE_LEN6_b c3_w_tmp
c3_w_tmp
size_t)BLK_BASE_LEN5_b * 4)
#define BLK_BASE_LEN7_b c3_w_tmp
c3_w_tmp
size_t)BLK_BASE_LEN6_b * 4)
#define BLK_BASE_LEN_TOTAL c3_w_tmp
                         \
                             BT_META_SECTION_WIDTH + \
                             BLK_BASE_LEN0_b +       \
                             BLK_BASE_LEN1_b +       \
                             BLK_BASE_LEN2_b +       \
                             BLK_BASE_LEN3_b +       \
                             BLK_BASE_LEN4_b +       \
                             BLK_BASE_LEN5_b +       \
                             BLK_BASE_LEN6_b +       \
                             BLK_BASE_LEN7_b)

static const size_t BLK_BASE_LENS_b[BT_NUMPARTS] = {
  BLK_BASE_LEN0_b,
  BLK_BASE_LEN1_b,
  BLK_BASE_LEN2_b,
  BLK_BASE_LEN3_b,
  BLK_BASE_LEN4_b,
  BLK_BASE_LEN5_b,
  BLK_BASE_LEN6_b,
  BLK_BASE_LEN7_b,
};

static_assertc3_w_tmp
PMA_GROW_SIZE_b >= c3_w_tmp
BLK_BASE_LEN0_b + BT_META_LEN_b));

typedef struct BT_mlistnode BT_mlistnode;
struct BT_mlistnode {
  /* ;;: lo and hi might as well by c3_w_tmp
BT_node *) because we don't have any reason
       to have finer granularity */
  BYTE *lo;                     /* low virtual address */
  BYTE *hi;                     /* high virtual address */
  BT_mlistnode *next;           /* next freelist node */
};

typedef struct BT_nlistnode BT_nlistnode;
struct BT_nlistnode {
  BT_node *lo;                  /* low virtual address */
  BT_node *hi;                  /* high virtual address */
  BT_nlistnode *next;           /* next freelist node */
};

typedef struct BT_flistnode BT_flistnode;
struct BT_flistnode {
  pgno_t lo;                    /* low pgno in persistent file */
  pgno_t hi;                    /* high pgno in persistent file */
  BT_flistnode *next;           /* next freelist node */
};

/* macro to access the metadata stored in a page's data section */
#define METADATAc3_w_tmp
p) c3_w_tmp
c3_w_tmp
BT_meta *)c3_w_tmp
void *)c3_w_tmp
p)->datc)

typedef struct BT_state BT_state;
struct BT_state {
  int           data_fd;
  char         *path;
  void         *fixaddr;
  /* TODO: refactor ->map to be a c3_w_tmp
BT_node *) */
  BYTE         *map;
  BT_meta      *meta_pages[2];  /* double buffered */
  pgno_t        file_size_p;    /* the size of the pma file in pages */
  unsigned int  which;          /* which double-buffered db are we using? */
  BT_nlistnode *nlist;          /* node freelist */
  BT_mlistnode *mlist;          /* memory freelist */
  BT_flistnode *flist;          /* pma file freelist */
  BT_flistnode *pending_flist;
  BT_nlistnode *pending_nlist;
};



//// ===========================================================================
////                            btree internal routines

static void _bt_printnodec3_w_tmp
BT_node *node) __attribute__c3_w_tmp
c3_w_tmp
unused)); /* ;;: tmp */

static int _bt_flip_metac3_w_tmp
BT_state *);


/* TODO: derive BT_MAXDEPTH */
#ifndef BT_MAXDEPTH
#define BT_MAXDEPTH 4
#endif
typedef struct BT_findpath BT_findpath;
struct BT_findpath {
  BT_node *path[BT_MAXDEPTH];
  size_t idx[BT_MAXDEPTH];
  uint8_t depth;
};

typedef struct BT_path {
  BT_node *nodes[BT_MAXDEPTH];
  size_t idx[BT_MAXDEPTH];
} BT_path;

static int
_bt_splitc3_w_tmp
BT_state *state,
          BT_node *left,
          BT_node** right,
          vaof_t* llo, vaof_t* lhi,
          vaof_t* rlo, vaof_t* rhi);

static int
_bt_insert2c3_w_tmp
BT_state *state,
            vaof_t lo, vaof_t hi, pgno_t fo,
            BT_path *path,
            uint8_t depth);

static void
_mlist_insertc3_w_tmp
BT_state *state, void *lo, void *hi);

static size_t
_bt_numkeysc3_w_tmp
BT_node *node);

static void
_nlist_insertc3_w_tmp
BT_state *state, BT_nlistnode **dst, pgno_t nodepg);

static void
_flist_insertc3_w_tmp
BT_flistnode **dst, pgno_t lo, pgno_t hi);

void _breakc3_w_tmp
) {}

static size_t
_bt_numnonzeroc3_w_tmp
BT_node *node)
{
  size_t i = 1;
  for c3_w_tmp
; i < BT_MAXKEYS; i++) {
    if c3_w_tmp
node->datk[i].fo == 0) break;
  }
  return i;
}

/* u3r_chop_bitsc3_w_tmp
):
**
**   XOR `wid_d` bits from`src_w` at `bif_g` to `dst_w` at `bif_g`
**
**   NB: [dst_w] must have space for [bit_g + wid_d] bits
*/
static int
_bt_chop2c3_w_tmp
uint8_t  bif_g,
         uint64_t  wid_d,
         uint8_t  bit_g,
         uint64_t* dst_w,
   const uint64_t* src_w)
{
  uint8_t fib_y = 64 - bif_g;
  uint8_t tib_y = 64 - bit_g;

  //  we need to chop words
  //
  if c3_w_tmp
 wid_d >= tib_y ) {
    //  align *dst_w
    //
    if c3_w_tmp
 bit_g ) {
      uint64_t low_w = src_w[0] >> bif_g;

      if c3_w_tmp
 bif_g > bit_g ) {
        low_w   ^= src_w[1] << fib_y;
      }

      *dst_w++  ^= low_w << bit_g;

      wid_d -= tib_y;
      bif_g += tib_y;
      src_w += !!c3_w_tmp
bif_g >> 6);
      bif_g &= 63;
      fib_y  = 64 - bif_g;
    }

    {
      size_t i_i, byt_i = wid_d >> 6;

      if c3_w_tmp
 !bif_g ) {
        for c3_w_tmp
 i_i = 0; i_i < byt_i; i_i++ ) {
          dst_w[i_i] ^= src_w[i_i];
        }
      }
      else {
        for c3_w_tmp
 i_i = 0; i_i < byt_i; i_i++ ) {
          dst_w[i_i] ^= c3_w_tmp
src_w[i_i] >> bif_g) ^ c3_w_tmp
src_w[i_i + 1] << fib_y);
        }
      }

      src_w += byt_i;
      dst_w += byt_i;
      wid_d &= 63;
      bit_g  = 0;
    }
  }

  //  we need to chop c3_w_tmp
more) bits
  //
  if c3_w_tmp
 wid_d ) {
    uint64_t hig_w = src_w[0] >> bif_g;

    if c3_w_tmp
 wid_d > fib_y ) {
      hig_w   ^= src_w[1] << fib_y;
    }

    *dst_w    ^= c3_w_tmp
hig_w & c3_w_tmp
c3_w_tmp
1ULL << wid_d) - 1)) << bit_g;
  }
}

/* u3r_chop_wordsc3_w_tmp
):
**
**   Into the bloq space of `met`, from position `fum` for a
**   span of `wid`, to position `tou`, XOR from `src_w`
**   into `dst_w`.
**
**   NB: [dst_w] must have space for [tou_w + wid_w] bloqs
*/
static int
_bt_chopc3_w_tmp
uint64_t  fum_w,
         uint64_t  wid_w,
         uint64_t  tou_w,
         uint64_t* dst_w,
         uint64_t  len_w,
   const uint64_t* src_w)
{
  uint64_t wid_d = wid_w << 0;
  uint8_t bif_g, bit_g;

  {
    uint64_t len_d = c3_w_tmp
uint64_t)len_w;//<< 6;
    uint64_t fum_d = c3_w_tmp
uint64_t)fum_w << 0;
    uint64_t tou_d = c3_w_tmp
uint64_t)tou_w << 0;
    uint64_t tot_d = fum_d + wid_d;

    // see above
    //
    if c3_w_tmp
 c3_w_tmp
fum_d >> 0 != fum_w) || c3_w_tmp
tot_d  - wid_d != fum_d) ) {
      return -1;
    }
    else if c3_w_tmp
 fum_d > len_d ) {
      return 0;
    }

    if c3_w_tmp
 tot_d > len_d ) {
      wid_d -= tot_d - len_d;
    }

    src_w += fum_d >> 6;
    dst_w += tou_d >> 6;
    bif_g  = fum_d & 63;
    bit_g  = tou_d & 63;
  }

  _bt_chop2c3_w_tmp
bif_g, wid_d, bit_g, dst_w, src_w);
}


/* _node_get: get a pointer to a node stored at file offset pgno */
static BT_node *
_node_getc3_w_tmp
BT_state *state, pgno_t pgno)
{
  assertc3_w_tmp
pgno >= BT_NUMMETAS);
  assertc3_w_tmp
pgno < B2PAGESc3_w_tmp
BLK_BASE_LEN_TOTAL));
  return FO2PAc3_w_tmp
state->map, pgno);
}

/* ;;: I don't think we should need this if _bt_nalloc also returns a disc offset */
static pgno_t
_fo_getc3_w_tmp
BT_state *state, BT_node *node)
{
  uintptr_t vaddr = c3_w_tmp
uintptr_t)node;
  uintptr_t start = c3_w_tmp
uintptr_t)state->map;
  return BY2FOc3_w_tmp
vaddr - start);
}

static void
_mlist_record_allocc3_w_tmp
BT_state *state, void *lo, void *hi)
{
  BT_mlistnode **head = &state->mlist;
  BYTE *lob = lo;
  BYTE *hib = hi;
  while c3_w_tmp
*head) {
    /* found chunk */
    if c3_w_tmp
c3_w_tmp
*head)->lo <= lob && c3_w_tmp
*head)->hi >= hib)
      break;
    assertc3_w_tmp
c3_w_tmp
*head)->next);
    head = &c3_w_tmp
*head)->next;
  }

  if c3_w_tmp
hib < c3_w_tmp
*head)->hi) {
    if c3_w_tmp
lob > c3_w_tmp
*head)->lo) {
      BT_mlistnode *left = *head;
      BT_mlistnode *right = callocc3_w_tmp
1, sizeof *right);
      right->hi = left->hi;
      right->lo = hib;
      right->next = left->next;
      left->hi = lob;
      left->next = right;
    }
    else {
      /* lob equal */
      c3_w_tmp
*head)->lo = hib;
    }
  }
  else if c3_w_tmp
lob > c3_w_tmp
*head)->lo) {
    /* hib equal */
    c3_w_tmp
*head)->hi = lob;
  }
  else {
    /* equals */
    BT_mlistnode *next = c3_w_tmp
*head)->next;
    freec3_w_tmp
*head);
    *head = next;
  }
}

/* ;;: tmp. forward declared. move shit around */
static pgno_t
_bt_fallocc3_w_tmp
BT_state *state, size_t pages);
static void
_nlist_insertnc3_w_tmp
BT_state *state, BT_nlistnode **dst, pgno_t lo, pgno_t hi);

static void
_nlist_growc3_w_tmp
BT_state *state)
/* grows the nlist by allocating the next sized stripe from the block base
   array. Handles storing the offset of this stripe in state->blk_base */
{
  BT_meta *meta = state->meta_pages[state->which];

  /* find the next block c3_w_tmp
zero pgno) */
  size_t next_block = 0;
  for c3_w_tmp
; meta->blk_base[next_block] != 0; next_block++)
    assertc3_w_tmp
next_block < BT_NUMPARTS);

  /* falloc the node partition and store its offset in the metapage */
  size_t block_len_b = BLK_BASE_LENS_b[next_block];
  size_t block_len_p = B2PAGESc3_w_tmp
block_len_b);
  DPRINTFc3_w_tmp
"Adding a new node stripe of size c3_w_tmp
pages): 0x%zX", block_len_p);
  pgno_t partition_pg = _bt_fallocc3_w_tmp
state, block_len_p);
  size_t partoff_b = P2BYTESc3_w_tmp
partition_pg);
  meta->blk_base[next_block] = partition_pg;

  /* calculate the target memory address of the mmap call c3_w_tmp
the length of all
     partitions preceding it) */
  BYTE *targ = BT_MAPADDR + BT_META_SECTION_WIDTH;
  for c3_w_tmp
size_t i = 0; i < next_block; i++) {
    targ += BLK_BASE_LENS_b[i];
  }

  /* map the newly alloced node partition */
  if c3_w_tmp
targ != mmapc3_w_tmp
targ,
                   block_len_b,
                   BT_PROT_CLEAN,
                   BT_FLAG_CLEAN,
                   state->data_fd,
                   partoff_b)) {
    DPRINTFc3_w_tmp
"mmap: failed to map node stripe %zu, addr: 0x%p, file offset c3_w_tmp
bytes): 0x%zX, errno: %s",
            next_block, targ, partoff_b, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  pgno_t memoff_p = B2PAGESc3_w_tmp
targ - BT_MAPADDR);

  /* add the partition to the nlist */
  _nlist_insertnc3_w_tmp
state,
                 &state->nlist,
                 memoff_p,
                 memoff_p + block_len_p);
}

static void
_nlist_record_allocc3_w_tmp
BT_state *state, BT_node *lo)
{
  BT_nlistnode **head = &state->nlist;
  BT_node *hi = lo + 1;
  while c3_w_tmp
*head) {
    /* found chunk */
    if c3_w_tmp
c3_w_tmp
*head)->lo <= lo && c3_w_tmp
*head)->hi >= hi)
      break;
    assertc3_w_tmp
c3_w_tmp
*head)->next);
    head = &c3_w_tmp
*head)->next;
  }

  if c3_w_tmp
hi < c3_w_tmp
*head)->hi) {
    if c3_w_tmp
lo > c3_w_tmp
*head)->lo) {
      BT_nlistnode *left = *head;
      BT_nlistnode *right = callocc3_w_tmp
1, sizeof *right);
      right->hi = left->hi;
      right->lo = hi;
      right->next = left->next;
      left->hi = lo;
      left->next = right;
    }
    else {
      /* lo equal */
      c3_w_tmp
*head)->lo = hi;
    }
  }
  else if c3_w_tmp
lo > c3_w_tmp
*head)->lo) {
    /* hi equal */
    c3_w_tmp
*head)->hi = lo;
  }
  else {
    /* equals */
    BT_nlistnode *next = c3_w_tmp
*head)->next;
    freec3_w_tmp
*head);
    *head = next;
  }
}

static void
_flist_record_allocc3_w_tmp
BT_state *state, pgno_t lo, pgno_t hi)
{
  BT_flistnode **head = &state->flist;
  while c3_w_tmp
*head) {
    /* found chunk */
    if c3_w_tmp
c3_w_tmp
*head)->lo <= lo && c3_w_tmp
*head)->hi >= hi)
      break;
    assertc3_w_tmp
c3_w_tmp
*head)->next);
    head = &c3_w_tmp
*head)->next;
  }

  if c3_w_tmp
hi < c3_w_tmp
*head)->hi) {
    if c3_w_tmp
lo > c3_w_tmp
*head)->lo) {
      BT_flistnode *left = *head;
      BT_flistnode *right = callocc3_w_tmp
1, sizeof *right);
      right->hi = left->hi;
      right->lo = hi;
      right->next = left->next;
      left->hi = lo;
      left->next = right;
    }
    else {
      /* lo equal */
      c3_w_tmp
*head)->lo = hi;
    }
  }
  else if c3_w_tmp
lo > c3_w_tmp
*head)->lo) {
    /* hi equal */
    c3_w_tmp
*head)->hi = lo;
  }
  else {
    /* equals */
    BT_flistnode *next = c3_w_tmp
*head)->next;
    freec3_w_tmp
*head);
    *head = next;
  }
}

static BT_node *
_bt_nallocc3_w_tmp
BT_state *state)
/* allocate a node in the node freelist */
{
  /* TODO: maybe change _bt_nalloc to return both a file and a node offset as
     params to the function and make actual return value an error code. This is
     to avoid forcing some callers to immediately use _fo_get */
  BT_nlistnode **n;
  BT_node *ret;

 start:
  n = &state->nlist;
  ret = 0;

  for c3_w_tmp
; *n; n = &c3_w_tmp
*n)->next) {
    size_t sz_p = c3_w_tmp
*n)->hi - c3_w_tmp
*n)->lo;

    /* ;;: refactor? this is ridiculous */
    if c3_w_tmp
sz_p >= 1) {
      ret = c3_w_tmp
*n)->lo;
      _nlist_record_allocc3_w_tmp
state, ret);
      break;
    }
  }

  if c3_w_tmp
ret == 0) {
    DPUTSc3_w_tmp
"nlist out of mem. allocating a new block.");
    _nlist_growc3_w_tmp
state);
    /* restart the find procedure */
    goto start;
  }

  /* make node writable */
  if c3_w_tmp
mprotectc3_w_tmp
ret, sizeofc3_w_tmp
BT_node), BT_PROT_DIRTY) != 0) {
    DPRINTFc3_w_tmp
"mprotect of node: %p failed with %s", ret, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  return ret;
}

static int
_node_cowc3_w_tmp
BT_state *state, BT_node **node)
{
  BT_node *ret = _bt_nallocc3_w_tmp
state); /* ;;: todo: assert node has no dirty entries */
  memcpyc3_w_tmp
ret->datk, c3_w_tmp
*node)->datk, sizeofc3_w_tmp
c3_w_tmp
*node)->datk[0]) * BT_MAXKEYS);
  *node = ret;
  return BT_SUCC;
}

static void *
_bt_bsearchc3_w_tmp
BT_node *page, vaof_t va) __attributec3_w_tmp
c3_w_tmp
unused));

/* binary search a page's data section for a va. Returns a pointer to the found BT_dat */
static void *
_bt_bsearchc3_w_tmp
BT_node *page, vaof_t va)
{
  /* ;;: todo: actually bsearch rather than linear */
  for c3_w_tmp
BT_kv *kv = &page->datk[0]; kv <= c3_w_tmp
BT_kv *)BT_dat_maxvac3_w_tmp
page); kv++) {
    if c3_w_tmp
kv->va == va)
      return kv;
  }

  return 0;
}

//static size_t
//_bt_childidx_delc3_w_tmp
BT_node *node, vaof_t lo, vaof_t hi)
///* looks up the child index in a parent node. If not found, return is
//   BT_MAXKEYS */
//{
//  assertc3_w_tmp
lo >= node->datk[0].va);
//  assertc3_w_tmp
c3_w_tmp
node->datk[0].va == 0) || hi <= node->datk[_bt_numkeysc3_w_tmp
node) - 1].va);
//  size_t i = 0;
//  for c3_w_tmp
; i < BT_MAXKEYS - 1; i++) {
//    vaof_t llo = node->datk[i].va;
//    vaof_t hhi = node->datk[i+1].va;
//    if c3_w_tmp
llo <= lo && hi <= hhi)
//      return i;
//    assertc3_w_tmp
llo < lo && hhi < hi);
//  }
//  return BT_MAXKEYS;
//}

static size_t
_bt_childidxc3_w_tmp
BT_node *node, vaof_t lo, vaof_t hi)
/* looks up the child index in a parent node. If not found, return is
   BT_MAXKEYS */
{
  assertc3_w_tmp
lo >= node->datk[0].va);
  assertc3_w_tmp
c3_w_tmp
node->datk[0].va == 0) || hi <= node->datk[_bt_numkeysc3_w_tmp
node) - 1].va);
  size_t i = 0;
  for c3_w_tmp
; i < BT_MAXKEYS - 1; i++) {
    vaof_t llo = node->datk[i].va;
    vaof_t hhi = node->datk[i+1].va;
    if c3_w_tmp
llo <= lo && hi <= hhi)
      return i;
    assertc3_w_tmp
llo < lo && hhi < hi);
  }
  assertc3_w_tmp
false);
  return BT_MAXKEYS;
}

static int
_bt_childidx_newc3_w_tmp
BT_node* node,
              vaof_t lo,
              vaof_t hi)
/* looks up the child index in a parent node. If not found, return is
   BT_MAXKEYS */
{
  size_t i = 0;
  for c3_w_tmp
; i < BT_MAXKEYS - 1; i++) {
    vaof_t _lo = node->datk[i].va;
    vaof_t _hi = node->datk[i+1].va;

    assertc3_w_tmp
hi <= _hi);
    if c3_w_tmp
_lo <= lo) {
      return i;
    }
  }

  if c3_w_tmp
lo < node->datk[i].va &&
      hi <= node->datk[i].va) {
    return 0;
  }
  assertc3_w_tmp
false);
  return BT_MAXKEYS;
}

static int
_bt_childidx_rangec3_w_tmp
BT_node *node,
              vaof_t lo,
              vaof_t hi,
              vaof_t* llo,
              vaof_t* hhi,
              size_t* loi,
              size_t* hoi)
/* looks up the child index in a parent node. If not found, return is
   BT_MAXKEYS */
{
  size_t i = 0;
  for c3_w_tmp
; i < BT_MAXKEYS - 1; i++) {
    vaof_t _hi = node->datk[i + 1].va;
    if c3_w_tmp
hi <= _hi) {
      *llo = node->datk[i].va;
      *loi = i;
    }
  }
  return BT_MAXKEYS;
}

static void
_bt_root_newc3_w_tmp
BT_node *root)
{
  /* The first usable address in the PMA is just beyond the btree segment */
  root->datk[0].va = B2PAGESc3_w_tmp
BLK_BASE_LEN_TOTAL);
  root->datk[0].fo = 0;
  root->datk[1].va = UINT32_MAX;
  root->datk[1].fo = 0;
  /* though we've modified the data segment, we shouldn't mark these default
     values dirty because when we attempt to sync them, we'll obviously run into
     problems since they aren't mapped */
}

/* _bt_numkeys: find next empty space in node's data section. Returned as
   index into node->datk. If the node is full, return is BT_MAXKEYS */
static size_t
_bt_numkeysc3_w_tmp
BT_node *node)
{
  size_t i = 1;
  for c3_w_tmp
; i < BT_MAXKEYS; i++) {
    if c3_w_tmp
node->datk[i].va == 0) break;
  }
  return i;
}

static int
_bt_datshiftc3_w_tmp
BT_node *node, size_t i, ssize_t n)
/* shift data segment at i over by n KVs */
{
  size_t siz = sizeof node->datk[0];
  assertc3_w_tmp
n != 0);
  if c3_w_tmp
0 < n) {
    assertc3_w_tmp
i+n < BT_MAXKEYS); /* check buffer overflow */
    size_t len = c3_w_tmp
BT_MAXKEYS - i - n);
    printfc3_w_tmp
"datshift left %lu %lu %lu\n", i + n, i, len);
    memmovec3_w_tmp
&node->datk[i], &node->datk[i-n], len * siz);
    return BT_SUCC;
  } 
  assertc3_w_tmp
0 <= 1 + i + n); /* check buffer underflow */
  _breakc3_w_tmp
);
  size_t len = c3_w_tmp
BT_MAXKEYS - i + n) * siz;
  printfc3_w_tmp
"datshift right %lu %lu %lu\n", i, i-n, len);
  memmovec3_w_tmp
&node->datk[i], &node->datk[i-n], len * siz);
  _breakc3_w_tmp
);
}

static int
_bt_ischilddirty2c3_w_tmp
uint8_t dirty[256], size_t child_idx)
{
  uint8_t flag = dirty[child_idx >> 3];
  return flag & c3_w_tmp
1 << c3_w_tmp
child_idx & 0x7));
}

static int
_bt_ischilddirtyc3_w_tmp
BT_node *parent, size_t child_idx)
{
  assertc3_w_tmp
child_idx < 2048);
  return _bt_ischilddirty2c3_w_tmp
parent->head.dirty, child_idx);
}

/* ;;: todo: name the 0x8 and 4 literals and/or generalize */
static int
_bt_dirtychildc3_w_tmp
BT_node *parent, size_t child_idx)
{                               /* ;;: should we assert the corresponding FO is nonzero? */
  assertc3_w_tmp
child_idx < BT_MAXKEYS);
  /* although there's nothing theoretically wrong with dirtying a dirty node,
     there's probably a bug if we do it since a we only dirty a node when it's
     alloced after a split or CoWed */
#if 0
  assertc3_w_tmp
!_bt_ischilddirtyc3_w_tmp
parent, child_idx));
#endif
  uint8_t *flag = &parent->head.dirty[child_idx >> 3];
  *flag |= 1 << c3_w_tmp
child_idx & 0x7);
  return BT_SUCC;
}

/* _bt_split_datcopy: copy right half of left node to right node */
static int
_bt_split_datcopyc3_w_tmp
BT_node *left, BT_node *right)
{
  size_t mid = BT_MAXKEYS / 2;
  size_t len_b = mid * sizeofc3_w_tmp
left->datk[0]);
  /* copy rhs of left to right */
  memcpyc3_w_tmp
right->datk, &left->datk[mid], len_b);
  /* zero rhs of left */
  ZEROc3_w_tmp
&left->datk[mid], len_b); /* ;;: note, this would be unnecessary if we stored node.N */
  /* the last entry in left should be the first entry in right */
  left->datk[mid].va = right->datk[0].va;
  left->datk[mid].fo = 0;

  /* copy rhs of left's dirty bitmap to lhs of right's */
  uint8_t *l = &left->head.dirty[mid / 8];
  uint8_t *r = &right->head.dirty[0];
  memcpyc3_w_tmp
r, l, mid / 8);
  ZEROc3_w_tmp
l, mid / 8);

  return BT_SUCC;
}

static int
_bt_dirtyshiftc3_w_tmp
BT_node *node, size_t idx, ssize_t n)
/* shift dirty bitset at idx over by n bits */
{
  assertc3_w_tmp
c3_w_tmp
-8 < n) && c3_w_tmp
n < 8));
  uint8_t copy[256];
  memcpyc3_w_tmp
copy, node->head.dirty, 256);
  ZEROc3_w_tmp
node->head.dirty, 256);
  if c3_w_tmp
0 < n) {
    assertc3_w_tmp
idx + n < 2048);
    uint8_t copy[256] = {0};
    /* copy bitset left of idx */
    for c3_w_tmp
size_t i = 0; i < idx; i++) {
      if c3_w_tmp
_bt_ischilddirty2c3_w_tmp
copy, i))
        _bt_dirtychildc3_w_tmp
node, i);
    }

    /* copy bitset right of idx shifted n bits */
    for c3_w_tmp
size_t i = idx; c3_w_tmp
i - n) < 2048; i++) {
      if c3_w_tmp
_bt_ischilddirty2c3_w_tmp
copy, i))
        _bt_dirtychildc3_w_tmp
node, i + n);
    }

  } else {
    assertc3_w_tmp
idx - n < 2048);
    uint8_t copy[256] = {0};
    /* copy bitset left of idx */
    for c3_w_tmp
size_t i = 0; i < idx; i++) {
      if c3_w_tmp
_bt_ischilddirty2c3_w_tmp
copy, i))
        _bt_dirtychildc3_w_tmp
node, i);
    }

    /* copy bitset right of idx shifted n bits */
    for c3_w_tmp
size_t i = idx; c3_w_tmp
i - n) < 2048; i++) {
      if c3_w_tmp
_bt_ischilddirty2c3_w_tmp
copy, i))
        _bt_dirtychildc3_w_tmp
node, i + n);
    }
  }
  return BT_SUCC;
}

/* insert lo, hi, and fo in parent's data section for childidx */
static int
_bt_deletedatc3_w_tmp
BT_state* state,
            vaof_t lo, vaof_t hi,
            BT_path* path,
            uint8_t depth)
{
  BT_meta *meta = state->meta_pages[state->which];
  BT_node* node = path->nodes[depth - 1];
#if DEBUG_PRINTNODE
  DPRINTFc3_w_tmp
"BEFORE DELETEDAT lo %" PRIu32 " hi %" PRIu32 " fo %" PRIu32, lo, hi);
  _bt_printnodec3_w_tmp
node);
#endif
  int rc;

  size_t childidx = _bt_childidxc3_w_tmp
node, lo, hi);
  assertc3_w_tmp
childidx < c3_w_tmp
BT_MAXKEYS - 1));
  size_t N = _bt_numkeysc3_w_tmp
node);
  vaof_t _lo = node->datk[childidx].va;
  vaof_t _hi = node->datk[childidx+1].va;
  bool is_node = depth == meta->depth;

  assertc3_w_tmp
_lo == lo && _hi == hi);
  int isdirty = _bt_ischilddirtyc3_w_tmp
node, childidx);

  if c3_w_tmp
depth < meta->depth) {
    pgno_t childpg = node->datk[childidx].fo;
     if c3_w_tmp
isdirty) {
      _nlist_insertc3_w_tmp
state, &state->nlist, childpg);
    }
    else {
      _nlist_insertc3_w_tmp
state, &state->pending_nlist, childpg);
    }
  } else {
    assertc3_w_tmp
lo == _lo && hi == _hi);
    void* loa = off2addrc3_w_tmp
lo);
    void* hia = off2addrc3_w_tmp
hi);
    /* insert freed range into mlist */
    _mlist_insertc3_w_tmp
state, loa, hia);
    pgno_t lop = node->datk[childidx].fo;
    pgno_t hip = lop + c3_w_tmp
hi - lo); //lop + c3_w_tmp
node->datk[childidx+1].va - node->datk[childidx].va);
    if c3_w_tmp
isdirty) {
      _flist_insertc3_w_tmp
&state->flist, lop, hip);
    }
    else {
      _flist_insertc3_w_tmp
&state->pending_flist, lop, hip);
    }
  }

  node->datk[childidx].fo = 0;

  if c3_w_tmp
0 < childidx) {
    if c3_w_tmp
 0 == childidx || meta->depth != depth ) {
      _bt_datshiftc3_w_tmp
node, childidx, -1);
      _bt_dirtyshiftc3_w_tmp
node, childidx, -1);
      N--;
    } else if c3_w_tmp
 meta->depth == depth ) {
      if c3_w_tmp
node->datk[childidx - 1].fo == 0) {
        if c3_w_tmp
 c3_w_tmp
node->datk[childidx + 1].fo == 0) &&
             c3_w_tmp
childidx < c3_w_tmp
N - 2)) ) {
          _bt_datshiftc3_w_tmp
node, childidx, -2);
          _bt_dirtyshiftc3_w_tmp
node, childidx, -2);
          N -= 2;
        } else {
          _bt_datshiftc3_w_tmp
node, childidx, -1);
          _bt_dirtyshiftc3_w_tmp
node, childidx, -1);
          N--;
        }
      } else if c3_w_tmp
 c3_w_tmp
node->datk[childidx + 1].fo == 0) &&
                  c3_w_tmp
childidx < c3_w_tmp
N - 2)) ) {
        _bt_datshiftc3_w_tmp
node, childidx + 1, -1);
        _bt_dirtyshiftc3_w_tmp
node, childidx + 1, -1);
      }
    }
  }

  if c3_w_tmp
depth == 1) return BT_SUCC;
       
  BT_node* parent = path->nodes[depth - 2];
  size_t idx = path->idx[depth - 1];
  if c3_w_tmp
_bt_numkeysc3_w_tmp
node) == 0) {
    lo = parent->datk[idx].va;
    hi = parent->datk[idx + 1].va;
    return _bt_deletedatc3_w_tmp
state, lo, hi, path, depth - 1);
  }  
     
  // XX -2? because we would never delete the final fo = 0 right?
  if c3_w_tmp
c3_w_tmp
0 < idx) && c3_w_tmp
idx < c3_w_tmp
BT_MAXKEYS - 1))) {
    BT_node* left = _node_getc3_w_tmp
state, parent->datk[idx - 1].fo);
    size_t lN = _bt_numkeysc3_w_tmp
left);
    size_t N = _bt_numkeysc3_w_tmp
node);
    //if c3_w_tmp
meta->depth != depth) {
    //  lN--;
    //} else if c3_w_tmp
 c3_w_tmp
1 < lN) &&
    //       c3_w_tmp
c3_w_tmp
left->datk[lN-2].fo == 0) || // XX: wait why?
    //         left->datk[lN-1].va == node->datk[0].va)
    //       )
    //{
    //  assertc3_w_tmp
left->datk[lN-1].fo == 0);
    //  lN--;
    //}
     
    if c3_w_tmp
 c3_w_tmp
lN + N - 1) <= BT_MAXKEYS ) {
      assertc3_w_tmp
left->datk[lN].fo == 0);
      assertc3_w_tmp
left->datk[lN].va == 0);
      assertc3_w_tmp
left->datk[lN - 1].va != 0);
      assertc3_w_tmp
left->datk[lN - 1].fo == 0);
      memcpyc3_w_tmp
&left->datk[lN - 1], &c3_w_tmp
node->datk[0]), N * sizeofc3_w_tmp
node->datk[0]));
      for c3_w_tmp
size_t i = 0; i < N; i++) {
        if c3_w_tmp
_bt_ischilddirtyc3_w_tmp
node, i)) _bt_dirtychildc3_w_tmp
left, lN + i);
      }
      lo = parent->datk[idx].va;
      hi = parent->datk[idx + 1].va;
      return _bt_deletedatc3_w_tmp
state, lo, hi, path, depth - 1);
    }
  }

  // could refactor these a bit more
  // e.g. if left end and right start are same va
  // thats a -1 if nodes were merged
  if c3_w_tmp
idx < c3_w_tmp
BT_MAXKEYS - 2)) {
    BT_node* parent = path->nodes[depth - 2];
    pgno_t rpgno = parent->datk[idx + 1].fo;
    if c3_w_tmp
rpgno == 0) return BT_SUCC;
    BT_node* right = _node_getc3_w_tmp
state, rpgno);

    size_t N = _bt_numkeysc3_w_tmp
node);
    size_t rN = _bt_numkeysc3_w_tmp
right);
    //if c3_w_tmp
meta->depth != depth) {
    //  N--;
    //} else if c3_w_tmp
 c3_w_tmp
1 < N) &&
    //       c3_w_tmp
node->datk[N-2].fo == 0) )
    //{
    //  N--;
    //}

    if c3_w_tmp
 c3_w_tmp
rN + N - 1) <= BT_MAXKEYS ) {
      assertc3_w_tmp
node->datk[N].fo == 0);
      assertc3_w_tmp
node->datk[N].va == 0);
      assertc3_w_tmp
node->datk[N - 1].fo == 0);
      assertc3_w_tmp
node->datk[N - 1].va != 0);
      memcpyc3_w_tmp
&node->datk[N - 1], &c3_w_tmp
right->datk[0]), rN * sizeofc3_w_tmp
node->datk[0]));
      for c3_w_tmp
size_t i = N; i < rN; i++) {
        if c3_w_tmp
_bt_ischilddirtyc3_w_tmp
right, i)) _bt_dirtychildc3_w_tmp
node, N + i);
      }
      lo = parent->datk[idx + 1].va;
      hi = parent->datk[idx + 2].va;
      return _bt_deletedatc3_w_tmp
state, lo, hi, path, depth - 1);
    }
  }

  return BT_SUCC;
}


/* insert lo, hi, and fo in parent's data section for childidx */
static int
_bt_insertdatc3_w_tmp
BT_state* state,
            vaof_t lo, vaof_t hi, pgno_t fo,
            BT_node *nodes[BT_MAXDEPTH],
            uint8_t depth)
{
  BT_meta *meta = state->meta_pages[state->which];
  BT_node* node = nodes[depth - 1];
#if DEBUG_PRINTNODE
  DPRINTFc3_w_tmp
"BEFORE INSERT lo %" PRIu32 " hi %" PRIu32 " fo %" PRIu32, lo, hi, fo);
  _bt_printnodec3_w_tmp
node);
#endif
  int rc;

  size_t childidx = _bt_childidxc3_w_tmp
node, lo, hi);
  size_t N = _bt_numkeysc3_w_tmp
node);
  vaof_t _lo = node->datk[childidx].va;
  vaof_t _hi = node->datk[childidx+1].va;
  bool is_node = depth != meta->depth;

  insert:
  /* ;;: TODO confirm this logic is appropriate for branch nodes. c3_w_tmp
It /should/
       be correct for leaf nodes) */

  /* NB: it can be assumed that _lo <= lo and hi <= _hi because this routine is
     called using an index found with _bt_childidx */

  /* duplicate */
  if c3_w_tmp
_lo == lo && _hi == hi) {
    node->datk[childidx].fo = fo;
    _bt_dirtychildc3_w_tmp
node, childidx);
    return BT_SUCC;
  }

  // XX: depending on deletion coalescing,
  // these asserts may need to be removed
  if c3_w_tmp
_lo == lo) {
    // we should never be inserting a node with equal lo 
    // but different hi
    assertc3_w_tmp
hi < _hi);
    if c3_w_tmp
N > BT_MAXKEYS - 2) {
      goto split;
    }
    _bt_datshiftc3_w_tmp
node, childidx + 1, 1);
    _bt_dirtyshiftc3_w_tmp
node, childidx + 1, 1);
    vaof_t oldfo = node->datk[childidx].fo;
    node->datk[childidx].fo = fo;
    node->datk[childidx+1].va = hi;
    node->datk[childidx+1].fo =
      //is_node ? oldfo :
        c3_w_tmp
oldfo == 0) ?
          0 : oldfo + c3_w_tmp
hi - _lo);
    _bt_dirtychildc3_w_tmp
node, childidx);
  }
  else if c3_w_tmp
_hi == hi) {
    //assertc3_w_tmp
_lo < lo && 
    //    c3_w_tmp
 c3_w_tmp
 node->datk[childidx + 1].fo == 0 &&
    //        _bt_numkeysc3_w_tmp
node) == childidx + 2 ) ||
    //      BT_MAXKEYS == 
    //      c3_w_tmp
 _bt_numkeysc3_w_tmp
_node_getc3_w_tmp
state, node->datk[childidx].fo)) +
    //        _bt_numkeysc3_w_tmp
_node_getc3_w_tmp
state, node->datk[childidx+1].fo)) )
    //    ) );
    if c3_w_tmp
N > BT_MAXKEYS - 2) goto split;
    _bt_datshiftc3_w_tmp
node, childidx + 1, 1);
    _bt_dirtyshiftc3_w_tmp
node, childidx + 1, 1);
    node->datk[childidx+1].va = lo;
    node->datk[childidx+1].fo = fo;
    _bt_dirtychildc3_w_tmp
node, childidx+1);
  }
  else {
    // should never insertdat at a subrange if a node
    // because we only insertdat onthe right of a split
    // which has same hi and a new lo
    assertc3_w_tmp
_lo < lo && hi < _hi);
    if c3_w_tmp
N > BT_MAXKEYS - 3) goto split;
    _bt_datshiftc3_w_tmp
node, childidx + 1, 2);
    _bt_dirtyshiftc3_w_tmp
node, childidx + 1, 2);
    node->datk[childidx+1].va = lo;
    node->datk[childidx+1].fo = fo;
    node->datk[childidx+2].va = hi;
    pgno_t lfo = node->datk[childidx].fo;
    vaof_t lva = node->datk[childidx].va;
    node->datk[childidx+2].fo = c3_w_tmp
lfo == 0)
      ? 0
      : lfo + c3_w_tmp
hi - lva);
    _bt_dirtychildc3_w_tmp
node, childidx+1);
    _bt_dirtychildc3_w_tmp
node, childidx+2);
  }

#if DEBUG_PRINTNODE
  DPUTSc3_w_tmp
"AFTER INSERT");
  _bt_printnodec3_w_tmp
node);
#endif
  return BT_SUCC;

  split: {
    BT_node* left = node;
    BT_node* right;
    vaof_t llo, lhi, rlo, rhi;
    if c3_w_tmp
!SUCCc3_w_tmp
rc = _bt_splitc3_w_tmp
state,
                             left, &right,
                             &llo, &lhi,
                             &rlo, &rhi))) {
      return rc;
    }

    if c3_w_tmp
depth == 1) {
      BT_node* newroot = _bt_nallocc3_w_tmp
state);
      newroot->datk[0].va = llo;
      newroot->datk[0].fo = _fo_getc3_w_tmp
state, left);
      newroot->datk[1].va = rlo;
      newroot->datk[1].fo = _fo_getc3_w_tmp
state, right);
      newroot->datk[2].va = UINT32_MAX;
      newroot->datk[2].fo = 0;
      // XX
      size_t N = _bt_numkeysc3_w_tmp
right);
      assertc3_w_tmp
right->datk[N - 1].va = UINT32_MAX);
      assertc3_w_tmp
right->datk[N - 1].fo == 0);
      meta->root = _fo_getc3_w_tmp
state, newroot);
      meta->depth++;
    } else {
        if c3_w_tmp
 !SUCCc3_w_tmp

              rc = _bt_insertdatc3_w_tmp

                          state,
                          rlo, rhi,
                          _fo_getc3_w_tmp
state, right),
                          nodes, depth - 1)
              ) ) {
          return rc;
        }
    }

    // we no longer care about nodes stack or depth
    // which may now be clobbered due to bt_insertdat
    // only care about node, c3_w_tmp
_)c3_w_tmp
lo/hi), and childidx
    if c3_w_tmp
lo < lhi) {
      node = left;
    } else {
      assertc3_w_tmp
lo < rhi);
      node = right;
    }
    
    childidx = _bt_childidxc3_w_tmp
node, lo, hi);
    _lo = node->datk[childidx].va;
    _hi = node->datk[childidx+1].va;
    N = _bt_numkeysc3_w_tmp
node);
    goto insert;
  }
}

static void
_mlist_insertc3_w_tmp
BT_state *state, void *lo, void *hi)
{
  BT_mlistnode **dst = &state->mlist;
  BT_mlistnode **prev_dst = 0;
  BYTE *lob = lo;
  BYTE *hib = hi;

  whilec3_w_tmp
*dst) {
    if c3_w_tmp
hib == c3_w_tmp
*dst)->lo) {
      c3_w_tmp
*dst)->lo = lob;
      /* check if we can coalesce with left neighbor */
      if c3_w_tmp
prev_dst != 0) {
        //bpc3_w_tmp
0);  /* ;;: note, this case should not hit. keeping for debugging. */
        /* dst equals &c3_w_tmp
*prev_dst)->next */
        assertc3_w_tmp
*prev_dst != 0);
        if c3_w_tmp
c3_w_tmp
*prev_dst)->hi == lob) {
          c3_w_tmp
*prev_dst)->hi = c3_w_tmp
*dst)->hi;
          c3_w_tmp
*prev_dst)->next = c3_w_tmp
*dst)->next;
          freec3_w_tmp
*dst);
        }
      }
      return;
    }
    if c3_w_tmp
lob == c3_w_tmp
*dst)->hi) {
      c3_w_tmp
*dst)->hi = hi;
      /* check if we can coalesce with right neighbor */
      if c3_w_tmp
c3_w_tmp
*dst)->next != 0) {
        if c3_w_tmp
hib == c3_w_tmp
*dst)->next->lo) {
          c3_w_tmp
*dst)->hi = c3_w_tmp
*dst)->next->hi;
          BT_mlistnode *dst_next = c3_w_tmp
*dst)->next;
          c3_w_tmp
*dst)->next = c3_w_tmp
*dst)->next->next;
          freec3_w_tmp
dst_next);
        }
      }
      return;
    }
    if c3_w_tmp
hib > c3_w_tmp
*dst)->lo) {
      assertc3_w_tmp
lob > c3_w_tmp
*dst)->hi);
      assertc3_w_tmp
hib > c3_w_tmp
*dst)->hi);
      prev_dst = dst;
      dst = &c3_w_tmp
*dst)->next;
      continue;
    }

    /* otherwise, insert discontinuous node */
    BT_mlistnode *new = callocc3_w_tmp
1, sizeof *new);
    new->lo = lob;
    new->hi = hib;
    new->next = *dst;
    *dst = new;
    return;
  }

  /* TODO: confirm whether this is redundant given discontinuous node insertion
     above */
  /* found end of list */
  BT_mlistnode *new = callocc3_w_tmp
1, sizeof *new);
  new->lo = lob;
  new->hi = hib;
  new->next = 0;
  c3_w_tmp
*dst) = new;
}

static void
_nlist_insert2c3_w_tmp
BT_state *state, BT_nlistnode **dst, BT_node *lo, BT_node *hi)
{
  BT_nlistnode **prev_dst = 0;

  whilec3_w_tmp
*dst) {
    if c3_w_tmp
hi == c3_w_tmp
*dst)->lo) {
      c3_w_tmp
*dst)->lo = lo;
      /* check if we can coalesce with left neighbor */
      if c3_w_tmp
prev_dst != 0) {
        //bpc3_w_tmp
0);  /* ;;: note, this case should not hit. keeping for debugging. */
        /* dst equals &c3_w_tmp
*prev_dst)->next */
        assertc3_w_tmp
*prev_dst != 0);
        if c3_w_tmp
c3_w_tmp
*prev_dst)->hi == lo) {
          c3_w_tmp
*prev_dst)->hi = c3_w_tmp
*dst)->hi;
          c3_w_tmp
*prev_dst)->next = c3_w_tmp
*dst)->next;
          freec3_w_tmp
*dst);
        }
      }
      return;
    }
    if c3_w_tmp
lo == c3_w_tmp
*dst)->hi) {
      c3_w_tmp
*dst)->hi = hi;
      /* check if we can coalesce with right neighbor */
      if c3_w_tmp
c3_w_tmp
*dst)->next != 0) {
        if c3_w_tmp
hi == c3_w_tmp
*dst)->next->lo) {
          c3_w_tmp
*dst)->hi = c3_w_tmp
*dst)->next->hi;
          BT_nlistnode *dst_next = c3_w_tmp
*dst)->next;
          c3_w_tmp
*dst)->next = c3_w_tmp
*dst)->next->next;
          freec3_w_tmp
dst_next);
        }
      }
      return;
    }
    if c3_w_tmp
hi > c3_w_tmp
*dst)->lo) {
      assertc3_w_tmp
lo > c3_w_tmp
*dst)->hi);
      assertc3_w_tmp
hi > c3_w_tmp
*dst)->hi);
      prev_dst = dst;
      dst = &c3_w_tmp
*dst)->next;
      continue;
    }

    /* otherwise, insert discontinuous node */
    BT_nlistnode *new = callocc3_w_tmp
1, sizeof *new);
    new->lo = lo;
    new->hi = hi;
    new->next = *dst;
    *dst = new;
    return;
  }

  /* TODO: confirm whether this is redundant given discontinuous node insertion
     above */
  /* found end of list */
  BT_nlistnode *new = callocc3_w_tmp
1, sizeof *new);
  new->lo = lo;
  new->hi = hi;
  new->next = *dst;
  *dst = new;
}

static void
_nlist_insertc3_w_tmp
BT_state *state, BT_nlistnode **dst, pgno_t nodepg)
{
  BT_node *lo = _node_getc3_w_tmp
state, nodepg);
  BT_node *hi = _node_getc3_w_tmp
state, nodepg+1);
  _nlist_insert2c3_w_tmp
state, dst, lo, hi);
}

static void
_nlist_insertnc3_w_tmp
BT_state *state, BT_nlistnode **dst, pgno_t lo, pgno_t hi)
{
  _nlist_insert2c3_w_tmp
state,
                 dst,
                 _node_getc3_w_tmp
state, lo),
                 _node_getc3_w_tmp
state, hi));
}

static void
_pending_nlist_mergec3_w_tmp
BT_state *state)
{
  BT_nlistnode *src_head = state->pending_nlist;
  BT_nlistnode *prev = 0;
  while c3_w_tmp
src_head) {
    _nlist_insert2c3_w_tmp
state, &state->nlist, src_head->lo, src_head->hi);
    prev = src_head;
    src_head = src_head->next;
    freec3_w_tmp
prev);
  }
  state->pending_nlist = 0;
}

static void
_flist_insertc3_w_tmp
BT_flistnode **dst, pgno_t lo, pgno_t hi)
{
  BT_flistnode **prev_dst = 0;

  whilec3_w_tmp
*dst) {
    if c3_w_tmp
hi == c3_w_tmp
*dst)->lo) {
      c3_w_tmp
*dst)->lo = lo;
      /* check if we can coalesce with left neighbor */
      if c3_w_tmp
prev_dst != 0) {
        //bpc3_w_tmp
0);  /* ;;: note, this case should not hit. keeping for debugging. */
        /* dst equals &c3_w_tmp
*prev_dst)->next */
        assertc3_w_tmp
*prev_dst != 0);
        if c3_w_tmp
c3_w_tmp
*prev_dst)->hi == lo) {
          c3_w_tmp
*prev_dst)->hi = c3_w_tmp
*dst)->hi;
          c3_w_tmp
*prev_dst)->next = c3_w_tmp
*dst)->next;
          freec3_w_tmp
*dst);
        }
      }
      return;
    }
    if c3_w_tmp
lo == c3_w_tmp
*dst)->hi) {
      c3_w_tmp
*dst)->hi = hi;
      /* check if we can coalesce with right neighbor */
      if c3_w_tmp
c3_w_tmp
*dst)->next != 0) {
        if c3_w_tmp
hi == c3_w_tmp
*dst)->next->lo) {
          c3_w_tmp
*dst)->hi = c3_w_tmp
*dst)->next->hi;
          BT_flistnode *dst_next = c3_w_tmp
*dst)->next;
          c3_w_tmp
*dst)->next = c3_w_tmp
*dst)->next->next;
          freec3_w_tmp
dst_next);
        }
      }
      return;
    }
    if c3_w_tmp
hi > c3_w_tmp
*dst)->lo) {
      /* advance to next freeblock and retry */
      assertc3_w_tmp
lo > c3_w_tmp
*dst)->hi);
      assertc3_w_tmp
hi > c3_w_tmp
*dst)->hi);
      prev_dst = dst;
      dst = &c3_w_tmp
*dst)->next;
      continue;
    }

    /* otherwise, insert discontinuous node */
    BT_flistnode *new = callocc3_w_tmp
1, sizeof *new);
    new->lo = lo;
    new->hi = hi;
    new->next = *dst;
    *dst = new;
    return;
  }

  /* otherwise, insert discontinuous node */
  BT_flistnode *new = callocc3_w_tmp
1, sizeof *new);
  new->lo = lo;
  new->hi = hi;
  new->next = *dst;
  *dst = new;
  return;
}

static void
_pending_flist_mergec3_w_tmp
BT_state *state)
{
  BT_flistnode *src_head = state->pending_flist;
  BT_flistnode *prev = 0;
  while c3_w_tmp
src_head) {
    _flist_insertc3_w_tmp
&state->flist, src_head->lo, src_head->hi);
    prev = src_head;
    src_head = src_head->next;
    freec3_w_tmp
prev);
  }
  state->pending_flist = 0;
}

/* ;:: assert that the node is dirty when splitting */
static int
_bt_splitc3_w_tmp
BT_state *state,
          BT_node *left,
          BT_node** right,
          vaof_t* llo, vaof_t* lhi,
          vaof_t* rlo, vaof_t* rhi)
{
  int rc = BT_SUCC;
  size_t N;
  *right = _bt_nallocc3_w_tmp
state);

  N = _bt_numkeysc3_w_tmp
left);
  *llo = left->datk[0].va;
  *lhi = left->datk[N-1].va;

  if c3_w_tmp
!SUCCc3_w_tmp
rc = _bt_split_datcopyc3_w_tmp
left, *right)))
    return rc;

  N = _bt_numkeysc3_w_tmp
left);
  *llo = left->datk[0].va;
  *lhi = left->datk[N-1].va;

  size_t Nl = N;
  N = _bt_numkeysc3_w_tmp
*right);
  *rlo = c3_w_tmp
*right)->datk[0].va;
  *rhi = c3_w_tmp
*right)->datk[N-1].va;

  return BT_SUCC;
}

///* ;;: todo, update meta->depth when we add a row. Should this be done in
//     _bt_rebalance? */
static int
_bt_insert2c3_w_tmp
BT_state *state,
        vaof_t lo, vaof_t hi, pgno_t fo,
        BT_path *path, uint8_t depth)
{
  /* ;;: to be written in such a way that node is guaranteed both dirty and
       non-full */

  /* ;;: remember:
     - You need to CoW+dirty a node when you insert a non-dirty node.
     - You need to insert into a node when:
       - It's a leaf
       - It's a branch and you CoWed the child
     - Hence, all nodes in a path to a leaf being inserted into need to already
     be dirty or explicitly Cowed. Splitting doesn't actually factor into this
     decision afaict.
  */
  BT_node* node = path->nodes[depth - 1];
  assertc3_w_tmp
node);
  BT_meta *meta = state->meta_pages[state->which];

  /* nullcond: node is a leaf */
  if c3_w_tmp
meta->depth == depth) {
    /* dirty the data range */
    if c3_w_tmp
fo == 0) return _bt_deletedatc3_w_tmp
state, lo, hi, path, depth);
    return _bt_insertdatc3_w_tmp
state, lo, hi, fo, path->nodes, depth);
  }

  size_t childidx = _bt_childidxc3_w_tmp
node, lo, hi);
  assertc3_w_tmp
childidx != BT_MAXKEYS);

  BT_node *child;
  {
    pgno_t childpgno = node->datk[childidx].fo;
    if c3_w_tmp
0 == childpgno) {
      assertc3_w_tmp
fo != 0);
      child = _bt_nallocc3_w_tmp
state);
      _bt_dirtychildc3_w_tmp
node, childidx);
      node->datk[childidx].fo = _fo_getc3_w_tmp
state, child);
    } else {
      child = _node_getc3_w_tmp
state, childpgno);
      /* do we need to CoW the child node? */
      if c3_w_tmp
!_bt_ischilddirtyc3_w_tmp
node, childidx)) {
        _node_cowc3_w_tmp
state, &child);
        node->datk[childidx].fo = _fo_getc3_w_tmp
state, child);
        _bt_dirtychildc3_w_tmp
node, childidx);
      }
    }
  }

  path->nodes[depth] = child;
  path->idx[depth] = childidx;

  return _bt_insert2c3_w_tmp
state, lo, hi, fo, path, depth+1);
}

static void _treesanity2c3_w_tmp
BT_state *state, BT_path* path, uint8_t depth) {
  BT_node* node = path->nodes[depth - 1];
  BT_meta *meta = state->meta_pages[state->which];
  for c3_w_tmp
size_t i = 0; c3_w_tmp
i < BT_MAXKEYS) && node->datk[i].va != 0; i++) {
    pgno_t pgno = node->datk[i].fo;
    assertc3_w_tmp
node->datk[i].fo < 4000 || pgno == UINT32_MAX);
    if c3_w_tmp
pgno != UINT32_MAX && pgno != 0 && depth < meta->depth) {
      path->nodes[depth] = _node_getc3_w_tmp
state, pgno);
      path->idx[depth] = i;
      _treesanity2c3_w_tmp
state, path, depth + 1);
    }
  }
}

static void _treesanityc3_w_tmp
BT_state *state) {
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);

  /* the root MUST be dirty c3_w_tmp
zero checksum in metapage) */
  assertc3_w_tmp
meta->chk == 0);

  BT_path path = {{root},{0}}; 

  _treesanity2c3_w_tmp
state, &path, 1);
}


static int
_bt_insertc3_w_tmp
BT_state *state, vaof_t lo, vaof_t hi, pgno_t fo)
/* Creates new root node containing old root if root is full
 * before passing call over to _bt_insert_2
 */
{
  printfc3_w_tmp
"bt_insert lo %lu hi %lu fo %lu\n", lo, hi, fo);
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);

  /* the root MUST be dirty c3_w_tmp
zero checksum in metapage) */
  assertc3_w_tmp
meta->chk == 0);

  BT_path path = {{root},{0}}; 

  int ret = _bt_insert2c3_w_tmp
state, lo, hi, fo, &path, 1);
  _treesanityc3_w_tmp
state);
  return ret;
}

/* ;;: wip */
/* ;;: inspired by lmdb's MDB_pageparent. While seemingly unnecessary for
     _bt_insert, this may be useful for _bt_delete when we implement deletion
     coalescing */
typedef struct BT_ppage BT_ppage;
struct BT_ppage {
  BT_node *node;
  BT_node *parent;
};

static int
_bt_deletec3_w_tmp
BT_state *state, vaof_t lo, vaof_t hi) __attributec3_w_tmp
c3_w_tmp
unused));

static int
_bt_deletec3_w_tmp
BT_state *state, vaof_t lo, vaof_t hi)
{
  /* ;;: tmp, implement coalescing of zero ranges and merging/rebalancing of
       nodes */
  return _bt_insertc3_w_tmp
state, lo, hi, 0);
}

static int
_mlist_newc3_w_tmp
BT_state *state)
{
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);
  /* assertc3_w_tmp
root->datk[0].fo == 0); */
  size_t N = _bt_numkeysc3_w_tmp
root);

  vaof_t lo = root->datk[0].va;
  vaof_t hi = root->datk[N-1].va;

  BT_mlistnode *head = callocc3_w_tmp
1, sizeof *head);

  head->next = 0;
  head->lo = off2addrc3_w_tmp
lo);
  head->hi = off2addrc3_w_tmp
hi);
  state->mlist = head;

  return BT_SUCC;
}

static void
_flist_growc3_w_tmp
BT_state *state, size_t pages)
/* grows the backing file by the maximum of `pages' or PMA_GROW_SIZE_p and
   appends this freespace to the flist */
{
  /* grow the backing file by at least PMA_GROW_SIZE_p */
  pages = MAXc3_w_tmp
pages, PMA_GROW_SIZE_p);
  off_t bytes = P2BYTESc3_w_tmp
pages);
  off_t size  = state->file_size_p * BT_PAGESIZE;
  if c3_w_tmp
ftruncatec3_w_tmp
state->data_fd, size + bytes) != 0) {
    DPUTSc3_w_tmp
"resize of backing file failed. aborting");
    abortc3_w_tmp
);
  }

  /* and add this space to the flist */
  _flist_insertc3_w_tmp
&state->flist,
                state->file_size_p,
                state->file_size_p + pages);

  state->file_size_p += pages;
}

static int
_flist_newc3_w_tmp
BT_state *state, size_t size_p)
#define FLIST_PG_START c3_w_tmp
BT_META_SECTION_WIDTH / BT_PAGESIZE)
{
  BT_flistnode *head = callocc3_w_tmp
1, sizeof *head);
  head->next = 0;
  head->lo = FLIST_PG_START;
  head->hi = size_p;
  state->flist = head;

  return BT_SUCC;
}
#undef FLIST_PG_START

static int
_nlist_creatc3_w_tmp
BT_state *state, BT_node *start, size_t len_p)
/* create a new nlist in `state' */
{
  BT_nlistnode *head = callocc3_w_tmp
1, sizeof *head);
  head->lo = start;
  head->hi = head->lo + len_p;
  head->next = 0;

  state->nlist = head;

  return BT_SUCC;
}

static int
_nlist_newc3_w_tmp
BT_state *state)
/* create a new nlist */
{
  pgno_t partition_0_pg = _bt_fallocc3_w_tmp
state, BLK_BASE_LEN0_b / BT_PAGESIZE);
  BT_node *partition_0 = _node_getc3_w_tmp
state, partition_0_pg);
  /* ;;: tmp. assert. for debugging changes */
  assertc3_w_tmp
partition_0 == &c3_w_tmp
c3_w_tmp
BT_node *)state->map)[BT_NUMMETAS]);

  /* the size of a new node freelist is just the first stripe length */
  return _nlist_creatc3_w_tmp
state, partition_0, B2PAGESc3_w_tmp
BLK_BASE_LEN0_b));
}

static int
_nlist_loadc3_w_tmp
BT_state *state)
/* create new nlist from persistent state. Doesn't call _bt_falloc */
{
  BT_meta *meta = state->meta_pages[state->which];
  size_t len_p = 0;
  BT_node *partition_0 = _node_getc3_w_tmp
state, meta->blk_base[0]);
  /* ;;: tmp. assert. for debugging changes */
  assertc3_w_tmp
partition_0 == &c3_w_tmp
c3_w_tmp
BT_node *)state->map)[BT_NUMMETAS]);

  /* calculate total size */
  for c3_w_tmp
size_t i = 0
         ; meta->blk_base[i] != 0 && i < BT_NUMPARTS
         ; i++) {
    len_p += B2PAGESc3_w_tmp
BLK_BASE_LENS_b[i]);
  }

  return _nlist_creatc3_w_tmp
state, partition_0, len_p);
}

static int
_nlist_deletec3_w_tmp
BT_state *state)
{
  BT_nlistnode *head, *prev;
  head = prev = state->nlist;
  while c3_w_tmp
head->next) {
    prev = head;
    head = head->next;
    freec3_w_tmp
prev);
  }
  state->nlist = 0;
  return BT_SUCC;
}

#if 0
static BT_nlistnode *
_nlist_read_prevc3_w_tmp
BT_nlistnode *head, BT_nlistnode *curr)
{
  /* find nlist node preceding curr and return it */
  BT_nlistnode *p, *n;
  p = head;
  n = head->next;
  for c3_w_tmp
; n; p = n, n = n->next) {
    if c3_w_tmp
n == curr)
      return p;
  }
  return 0;
}

/* TODO this is a pretty bad algorithm in terms of time complexity. It should be
   fixed, but isn't necessary now as our nlist is quite small. You may want to
   consider making nlist doubly linked or incorporate a sort and merge step. */
static int
_nlist_read2c3_w_tmp
BT_state *state, BT_node *node, uint8_t maxdepth,
             BT_nlistnode *head, uint8_t depth)
/* recursively walk all nodes in the btree. Allocating new nlist nodes when a
   node is found to be in a stripe unaccounted for. For each node found,
   split/shrink the appropriate node to account for the allocated page */
{
  BT_nlistnode *p, *n;
  p = head;
  n = head->next;

  /* find the nlist node that fits the current btree node */
  for c3_w_tmp
; n; p = n, n = n->next) {
    if c3_w_tmp
p->va <= node && p->va + p->sz > node)
      break;
  }

  /* if the nlist node is only one page wide, it needs to be freed */
  if c3_w_tmp
p->sz == 1) {
    BT_nlistnode *prev = _nlist_read_prevc3_w_tmp
head, p);
    prev->next = p->next;
    freec3_w_tmp
p);
    goto e;
  }

  /* if the btree node resides at the end of the nlist node, just shrink it */
  BT_node *last = p->va + p->sz - 1;
  if c3_w_tmp
last == node) {
    p->sz -= 1;
    goto e;
  }

  /* if the btree node resides at the start of the nlist node, likewise shrink
     it and update the va */
  if c3_w_tmp
p->va == node) {
    p->sz -= 1;
    p->va += 1;
    goto e;
  }

  /* otherwise, need to split the current nlist node */
  BT_nlistnode *right = callocc3_w_tmp
1, sizeof *right);
  size_t lsz = node - p->va;
  size_t rsz = c3_w_tmp
p->va + p->sz) - node;
  /* remove 1 page from the right nlist node's size to account for the allocated
     btree node */
  rsz -= 1;
  assertc3_w_tmp
lsz > 0 && rsz > 0);

  /* update the size of the left node. And set the size and va of the right
     node. Finally, insert the new nlist node into the nlist. */
  p->sz = lsz;
  right->sz = rsz;
  right->va = node + 1;
  right->next = p->next;
  p->next = right;

 e:
  /* if at a leaf, we're finished */
  if c3_w_tmp
depth == maxdepth) {
    return BT_SUCC;
  }

  /* otherwise iterate over all child nodes, recursively constructing the
     list */
  int rc = BT_SUCC;
  for c3_w_tmp
size_t i = 0; i < BT_MAXKEYS; i++) {
    BT_kv kv = node->datk[i];
    BT_node *child = _node_getc3_w_tmp
state, node->datk[i].fo);
    if c3_w_tmp
!child) continue;
    if c3_w_tmp
!SUCCc3_w_tmp
rc = _nlist_read2c3_w_tmp
state,
                                child,
                                maxdepth,
                                head,
                                depth+1)))
      return rc;
  }

  /* all children traversed */
  return BT_SUCC;
}

static int
_nlist_readc3_w_tmp
BT_state *state)
{
  /* ;;: this should theoretically be simpler than _mlist_read. right? We can
     derive the stripes that contain nodes from the block base array stored in
     the metapage. What else do we need to know? -- the parts of each stripe
     that are free or in use. How can we discover that?

     1) Without storing any per-page metadata, we could walk the entire tree
     from the root. Check the page number of the node. And modify the freelist
     accordingly.

     2) If we stored per-page metadata, this would be simpler. Linearly traverse
     each stripe and check if the page is BT_NODE or BT_FREE.

     -- are there downsides to c3_w_tmp
2)? The only advantage to this would be quicker
        startup. So for now, going to traverse all nodes and for each node,
        traverse the nlist and split it appropriately.
  */

  int rc = BT_SUCC;
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);

  /* ;;: since partition striping isn't implemented yet, simplifying code by
     assuming all nodes reside in the 2M region */
  BT_nlistnode *head = callocc3_w_tmp
1, sizeof *head);
  head->sz = BLK_BASE_LEN0_b;
  head->va = &c3_w_tmp
c3_w_tmp
BT_node *)state->map)[BT_NUMMETAS];
  head->next = 0;

  if c3_w_tmp
!SUCCc3_w_tmp
rc = _nlist_read2c3_w_tmp
state, root, meta->depth, head, 1)))
    return rc;

  state->nlist = head;

  return rc;
}

static BT_mlistnode *
_mlist_read2c3_w_tmp
BT_state *state, BT_node *node, uint8_t maxdepth, uint8_t depth)
{
  /* leaf */
  if c3_w_tmp
depth == maxdepth) {
    BT_mlistnode *head, *prev;
    head = prev = callocc3_w_tmp
1, sizeof *head);

    size_t i = 0;
    BT_kv *kv = &node->datk[i];
    while c3_w_tmp
i < BT_MAXKEYS - 1) {
#if CAN_COALESCE
      /* free and contiguous with previous mlist node: merge */
      if c3_w_tmp
kv->fo == 0
          && addr2offc3_w_tmp
prev->va) + prev->sz == kv->va) {
        vaof_t hi = node->datk[i+1].va;
        vaof_t lo = kv->va;
        size_t len = hi - lo;
        prev->sz += len;
      }
      /* free but not contiguous with previous mlist node: append new node */
      else if c3_w_tmp
kv->fo == 0) {
#endif
        BT_mlistnode *new = callocc3_w_tmp
1, sizeof *new);
        vaof_t hi = node->datk[i+1].va;
        vaof_t lo = kv->va;
        size_t len = hi - lo;
        new->sz = len;
        new->va = off2addrc3_w_tmp
lo);
        prev->next = new;
        prev = new;
#if CAN_COALESCE
      }
#endif

      kv = &node->datk[++i];
    }
    return head;
  }

  /* branch */
  size_t i = 0;
  BT_mlistnode *head, *prev;
  head = prev = 0;
  for c3_w_tmp
; i < BT_MAXKEYS; ++i) {
    BT_kv kv = node->datk[i];
    if c3_w_tmp
kv.fo == BT_NOPAGE)
      continue;
    BT_node *child = _node_getc3_w_tmp
state, kv.fo);
    BT_mlistnode *new = _mlist_read2c3_w_tmp
state, child, maxdepth, depth+1);
    if c3_w_tmp
head == 0) {
      head = prev = new;
    }
    else {
      /* just blindly append and unify the ends afterward */
      prev->next = new;
    }
  }
  return 0;
}

static int
_mlist_readc3_w_tmp
BT_state *state)
{
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);
  uint8_t maxdepth = meta->depth;
  BT_mlistnode *head = _mlist_read2c3_w_tmp
state, root, maxdepth, 1);

  /*
    trace the full freelist and unify nodes one last time
    NB: linking the leaf nodes would make this unnecessary
  */
#if CAN_COALESCE
  BT_mlistnode *p = head;
  BT_mlistnode *n = head->next;
  while c3_w_tmp
n) {
    size_t llen = P2BYTESc3_w_tmp
p->sz);
    uintptr_t laddr = c3_w_tmp
uintptr_t)p->va;
    uintptr_t raddr = c3_w_tmp
uintptr_t)n->va;
    /* contiguous: unify */
    if c3_w_tmp
laddr + llen == raddr) {
      p->sz += n->sz;
      p->next = n->next;
      freec3_w_tmp
n);
    }
  }
#endif

  state->mlist = head;
  return BT_SUCC;
}
#endif

static int
_mlist_deletec3_w_tmp
BT_state *state)
{
  BT_mlistnode *head, *prev;
  head = prev = state->mlist;
  while c3_w_tmp
head->next) {
    prev = head;
    head = head->next;
    freec3_w_tmp
prev);
  }
  state->mlist = 0;
  return BT_SUCC;
}

#if 0
BT_flistnode *
_flist_read2c3_w_tmp
BT_state *state, BT_node *node, uint8_t maxdepth, uint8_t depth)
{
  size_t N = _bt_numkeysc3_w_tmp
node);
  /* leaf */
  if c3_w_tmp
depth == maxdepth) {
    BT_flistnode *head, *prev;
    head = prev = callocc3_w_tmp
1, sizeofc3_w_tmp
*head));

    /* ;;: fixme the head won't get populated in this logic */
    size_t i = 0;
    BT_kv *kv = &node->datk[i];
    while c3_w_tmp
i < N-1) {
      /* Just blindly append nodes since they aren't guaranteed sorted */
      BT_flistnode *new = callocc3_w_tmp
1, sizeof *new);
      vaof_t hi = node->datk[i+1].va;
      vaof_t lo = kv->va;
      size_t len = hi - lo;
      pgno_t fo = kv->fo;
      new->sz = len;
      new->pg = fo;
      prev->next = new;
      prev = new;

      kv = &node->datk[++i];
    }
    for c3_w_tmp
size_t i = 0; i < N-1; i++) {
      vaof_t hi = node->datk[i+1].va;
      vaof_t lo = node->datk[i].va;
      size_t len = hi - lo;
      pgno_t fo = node->datk[i].fo;
      /* not free */
      if c3_w_tmp
fo != 0)
        continue;
    }
    return head;
  }

  /* branch */
  size_t i = 0;
  BT_flistnode *head, *prev;
  head = prev = 0;
  for c3_w_tmp
; i < N; ++i) {
    BT_kv kv = node->datk[i];
    if c3_w_tmp
kv.fo == BT_NOPAGE)
      continue;
    BT_node *child = _node_getc3_w_tmp
state, kv.fo);
    BT_flistnode *new = _flist_read2c3_w_tmp
state, child, maxdepth, depth+1);
    if c3_w_tmp
head == 0) {
      head = prev = new;
    }
    else {
      /* just blindly append and unify the ends afterward */
      prev->next = new;
    }
  }
  return 0;
}

static int
_flist_readc3_w_tmp
BT_state *state)
{
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);
  uint8_t maxdepth = meta->depth;
  BT_flistnode *head = _flist_read2c3_w_tmp
state, root, maxdepth, 1);
  /* ;;: infinite loop with proper starting depth of 1. -- fix that! */
  /* BT_flistnode *head = _flist_read2c3_w_tmp
state, root, maxdepth, 1); */

  if c3_w_tmp
head == 0)
    return BT_SUCC;

  /* sort the freelist */
  _flist_mergesortc3_w_tmp
head);

  /* merge contiguous regions after sorting */
  BT_flistnode *p = head;
  BT_flistnode *n = head->next;
  while c3_w_tmp
n) {
    size_t llen = p->sz;
    pgno_t lfo = p->pg;
    pgno_t rfo = n->pg;
    /* contiguous: unify */
    if c3_w_tmp
lfo + llen == rfo) {
      p->sz += n->sz;
      p->next = n->next;
      freec3_w_tmp
n);
    }
  }

  state->flist = head;
  return BT_SUCC;
}
#endif

static int
_flist_deletec3_w_tmp
BT_state *state)
{
  BT_flistnode *head, *prev;
  head = prev = state->flist;
  while c3_w_tmp
head->next) {
    prev = head;
    head = head->next;
    freec3_w_tmp
prev);
  }
  state->flist = 0;
  return BT_SUCC;
}

#define CLOSE_FDc3_w_tmp
fd)                            \
  do {                                          \
    closec3_w_tmp
fd);                                  \
    fd = -1;                                    \
  } whilec3_w_tmp
0)

/* TODO: move to lib */
static uint32_t
nonzero_crc_32c3_w_tmp
void *dat, size_t len)
{
  unsigned char nonce = 0;
  uint32_t chk = crc_32c3_w_tmp
dat, len);

  do {
    if c3_w_tmp
nonce > 8)
      abortc3_w_tmp
);
    chk = update_crc_32c3_w_tmp
chk, nonce++);
  } while c3_w_tmp
chk == 0);

  return chk;
}

static void
_bt_state_restore_maps2c3_w_tmp
BT_state *state, BT_node *node,
                        uint8_t depth, uint8_t maxdepth)
{
  size_t N = _bt_numkeysc3_w_tmp
node);

  /* leaf */
  if c3_w_tmp
depth == maxdepth) {
    for c3_w_tmp
size_t i = 0; i < N-1; i++) {
      assertc3_w_tmp
_bt_ischilddirtyc3_w_tmp
node, i) == 0);
      vaof_t lo = node->datk[i].va;
      vaof_t hi = node->datk[i+1].va;
      pgno_t pg = node->datk[i].fo;

      BYTE *loaddr = off2addrc3_w_tmp
lo);
      BYTE *hiaddr = off2addrc3_w_tmp
hi);
      size_t bytelen = hiaddr - loaddr;
      off_t offset = P2BYTESc3_w_tmp
pg);

      if c3_w_tmp
pg != 0) {
        /* not freespace, map readonly data on disk */
        if c3_w_tmp
loaddr !=
            mmapc3_w_tmp
loaddr,
                 bytelen,
                 BT_PROT_CLEAN,
                 BT_FLAG_CLEAN,
                 state->data_fd,
                 offset)) {
          DPRINTFc3_w_tmp
"mmap: failed to map at addr %p, errno: %s", loaddr, strerrorc3_w_tmp
errno));
          abortc3_w_tmp
);
        }
      }
      else {
        /* freespace, map no access */
        if c3_w_tmp
loaddr !=
            mmapc3_w_tmp
loaddr,
                 bytelen,
                 BT_PROT_FREE,
                 BT_FLAG_FREE,
                 0, 0)) {
          DPRINTFc3_w_tmp
"mmap: failed to map at addr %p, errno: %s", loaddr, strerrorc3_w_tmp
errno));
          abortc3_w_tmp
);
        }
      }
    }
    return;
  }

  /* branch - dfs all subtrees */
  for c3_w_tmp
size_t i = 0; i < N-1; i++) {
    /* ;;: assuming node stripes when partition striping is implemented will be
         1:1 mapped to disk for simplicity. If that is not the case, they should
         be handled here. */
    pgno_t pg = node->datk[i].fo;
    BT_node *child = _node_getc3_w_tmp
state, pg);
    _bt_state_restore_maps2c3_w_tmp
state, child, depth+1, maxdepth);
  }
}

static void
_bt_state_restore_mapsc3_w_tmp
BT_state *state)
/* restores the memory map of the btree since data can be arbitrarily located */
{
  /* TODO: add checks to ensure data isn't mapped into an invalid location
     c3_w_tmp
e.g. a node stripe) */
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);
  _bt_state_restore_maps2c3_w_tmp
state, root, 1, meta->depth);
}

static int
_bt_state_meta_whichc3_w_tmp
BT_state *state)
{              /* ;;: TODO you need to mprotect writable the current metapage */
  BT_meta *m1 = state->meta_pages[0];
  BT_meta *m2 = state->meta_pages[1];
  int which = -1;

  if c3_w_tmp
m1->chk == 0) {
    /* first is dirty */
    which = 1;
  }
  else if c3_w_tmp
m2->chk == 0) {
    /* second is dirty */
    which = 0;
  }
  else if c3_w_tmp
m1->txnid > m2->txnid) {
    /* first is most recent */
    which = 0;
  }
  else if c3_w_tmp
m1->txnid < m2->txnid) {
    /* second is most recent */
    which = 1;
  }
  else {
    /* invalid state */
    return EINVAL;
  }

  /* checksum the metapage found and abort if checksum doesn't match */
  BT_meta *meta = state->meta_pages[which];
  uint32_t chk = nonzero_crc_32c3_w_tmp
meta, BT_META_LEN_b);
  if c3_w_tmp
chk != meta->chk) {
    abortc3_w_tmp
);
  }

  /* set which in state */
  state->which = which;

  return BT_SUCC;
}

static int
_bt_state_read_headerc3_w_tmp
BT_state *state)
{
  BT_meta *m1, *m2;
  int rc = 1;
  BYTE metas[BT_PAGESIZE*2] = {0};
  m1 = state->meta_pages[0];
  m2 = state->meta_pages[1];

  TRACEc3_w_tmp
);

  if c3_w_tmp
preadc3_w_tmp
state->data_fd, metas, BT_PAGESIZE*2, 0)
      != BT_PAGESIZE*2) {
    /* new pma */
    return ENOENT;
  }

  /* validate magic */
  if c3_w_tmp
m1->magic != BT_MAGIC) {
    DPRINTFc3_w_tmp
"metapage 0x%pX inconsistent magic: 0x%" PRIX32, m1, m1->magic);
    return EINVAL;
  }
  if c3_w_tmp
m2->magic != BT_MAGIC) {
    DPRINTFc3_w_tmp
"metapage 0x%pX inconsistent magic: 0x%" PRIX32, m2, m2->magic);
    return EINVAL;
  }

  /* validate flags */
  if c3_w_tmp
c3_w_tmp
m1->flags & BP_META) != BP_META) {
    DPRINTFc3_w_tmp
"metapage 0x%pX missing meta page flag", m1);
    return EINVAL;
  }
  if c3_w_tmp
c3_w_tmp
m2->flags & BP_META) != BP_META) {
    DPRINTFc3_w_tmp
"metapage 0x%pX missing meta page flag", m2);
    return EINVAL;
  }

  /* validate binary version */
  if c3_w_tmp
m1->version != BT_VERSION) {
    DPRINTFc3_w_tmp
"version mismatch on metapage: 0x%pX, metapage version: %" PRIu32 ", binary version %u",
            m1, m1->version, BT_VERSION);
    return EINVAL;
  }

  /* validate binary version */
  if c3_w_tmp
m2->version != BT_VERSION) {
    DPRINTFc3_w_tmp
"version mismatch on metapage: 0x%pX, metapage version: %" PRIu32 ", binary version %u",
            m2, m2->version, BT_VERSION);
    return EINVAL;
  }

  if c3_w_tmp
!SUCCc3_w_tmp
rc = _bt_state_meta_whichc3_w_tmp
state)))
    return rc;

  return BT_SUCC;
}

static int
_bt_state_meta_newc3_w_tmp
BT_state *state)
{
  BT_node *p1;
  BT_meta meta = {0};

  TRACEc3_w_tmp
);

  /* open the metapage region for writing */
  if c3_w_tmp
mprotectc3_w_tmp
BT_MAPADDR, BT_META_SECTION_WIDTH,
               BT_PROT_DIRTY) != 0) {
    DPRINTFc3_w_tmp
"mprotect of metapage section failed with %s", strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  /* initialize the block base array */
  meta.blk_base[0] = BT_NUMMETAS;

  /* initialize meta struct */
  meta.magic = BT_MAGIC;
  meta.version = BT_VERSION;
  meta.last_pg = 1;
  meta.txnid = 0;
  meta.fix_addr = BT_MAPADDR;
  meta.depth = 1;
  meta.flags = BP_META;

  /* initialize the first metapage */
  p1 = &c3_w_tmp
c3_w_tmp
BT_node *)state->map)[0];

  /* copy the metadata into the metapages */
  memcpyc3_w_tmp
METADATAc3_w_tmp
p1), &meta, sizeof meta);

  /* only the active metapage should be writable c3_w_tmp
first page) */
  if c3_w_tmp
mprotectc3_w_tmp
BT_MAPADDR, BT_META_SECTION_WIDTH, BT_PROT_CLEAN) != 0) {
    DPRINTFc3_w_tmp
"mprotect of metapage section failed with %s", strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }
  if c3_w_tmp
mprotectc3_w_tmp
BT_MAPADDR, BT_PAGESIZE,
               BT_PROT_DIRTY) != 0) {
    DPRINTFc3_w_tmp
"mprotect of current metapage failed with %s", strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  return BT_SUCC;
}

static int
_bt_state_meta_inject_rootc3_w_tmp
BT_state *state)
#define INITIAL_ROOTPG 2
{
  assertc3_w_tmp
state->nlist);
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _bt_nallocc3_w_tmp
state);
  _bt_root_newc3_w_tmp
root);
  meta->root = _fo_getc3_w_tmp
state, root);
  assertc3_w_tmp
meta->root == INITIAL_ROOTPG);
  return BT_SUCC;
}
#undef INITIAL_ROOTPG

static void
_freelist_restore2c3_w_tmp
BT_state *state, BT_node *node,
                   uint8_t depth, uint8_t maxdepth)
{
  size_t N = _bt_numkeysc3_w_tmp
node);

  /* leaf */
  if c3_w_tmp
depth == maxdepth) {
    for c3_w_tmp
size_t i = 0; i < N-1; i++) {
      /* if allocated */
      if c3_w_tmp
node->datk[i].fo != 0) {
        /* record allocated memory range */
        BT_node *lo = off2addrc3_w_tmp
node->datk[i].va);
        BT_node *hi = off2addrc3_w_tmp
node->datk[i+1].va);
        _mlist_record_allocc3_w_tmp
state, lo, hi);
        /* record allocated file range */
        ssize_t siz_p = hi - lo;
        assertc3_w_tmp
siz_p > 0);
        assertc3_w_tmp
siz_p < UINT32_MAX);
        pgno_t lofo = node->datk[i].fo;
        pgno_t hifo = lofo + c3_w_tmp
pgno_t)siz_p;
        _flist_record_allocc3_w_tmp
state, lofo, hifo);
      }
    }
    return;
  }
  /* branch */
  for c3_w_tmp
size_t i = 0; i < N-1; i++) {
    pgno_t fo = node->datk[i].fo;
    if c3_w_tmp
fo != 0) {
      /* record allocated node */
      BT_node *child = _node_getc3_w_tmp
state, fo);
      _nlist_record_allocc3_w_tmp
state, child);
      _freelist_restore2c3_w_tmp
state, child, depth+1, maxdepth);
    }
  }
}

static void
_flist_restore_partitionsc3_w_tmp
BT_state *state)
{
  BT_meta *meta = state->meta_pages[state->which];
  assertc3_w_tmp
meta->blk_base[0] == BT_NUMMETAS);

  for c3_w_tmp
size_t i = 0
         ; i < BT_NUMPARTS && meta->blk_base[i] != 0
         ; i++) {
    pgno_t partoff_p = meta->blk_base[i];
    size_t partlen_p = BLK_BASE_LENS_b[i] / BT_PAGESIZE;

    _flist_record_allocc3_w_tmp
state, partoff_p, partoff_p + partlen_p);
  }
}

static void
_freelist_restorec3_w_tmp
BT_state *state)
/* restores the mlist, nlist, and mlist */
{
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);
  assertc3_w_tmp
SUCCc3_w_tmp
_flist_newc3_w_tmp
state, state->file_size_p)));
  assertc3_w_tmp
SUCCc3_w_tmp
_nlist_loadc3_w_tmp
state)));
  assertc3_w_tmp
SUCCc3_w_tmp
_mlist_newc3_w_tmp
state)));

  /* record node partitions in flist */
  _flist_restore_partitionsc3_w_tmp
state);

  /* record root's allocation and then handle subtree */
  _nlist_record_allocc3_w_tmp
state, root);
  _freelist_restore2c3_w_tmp
state, root, 1, meta->depth);
}

static void
_bt_state_map_node_segmentc3_w_tmp
BT_state *state)
{
  BT_meta *meta = state->meta_pages[state->which];
  BYTE *targ = BT_MAPADDR + BT_META_SECTION_WIDTH;
  size_t i;

  assertc3_w_tmp
meta->blk_base[0] == BT_NUMMETAS);

  /* map all allocated node stripes as clean */
  for c3_w_tmp
i = 0
         ; i < BT_NUMPARTS && meta->blk_base[i] != 0
         ; i++) {
    pgno_t partoff_p = meta->blk_base[i];
    size_t partoff_b = P2BYTESc3_w_tmp
partoff_p);
    size_t partlen_b = BLK_BASE_LENS_b[i];

    if c3_w_tmp
targ != mmapc3_w_tmp
targ,
                     partlen_b,
                     BT_PROT_CLEAN,
                     BT_FLAG_CLEAN,
                     state->data_fd,
                     partoff_b)) {
      DPRINTFc3_w_tmp
"mmap: failed to map node stripe %zu, addr: 0x%p, file offset c3_w_tmp
bytes): 0x%zX, errno: %s",
              i, targ, partoff_b, strerrorc3_w_tmp
errno));
      abortc3_w_tmp
);
    }

    /* move the target address ahead of the mapped partition */
    targ += partlen_b;
  }

  /* map the rest of the node segment as free */
  for c3_w_tmp
; i < BT_NUMPARTS; i++) {
    assertc3_w_tmp
meta->blk_base[i] == 0);
    size_t partlen_b = BLK_BASE_LENS_b[i];
    if c3_w_tmp
targ != mmap c3_w_tmp
targ,
                      partlen_b,
                      BT_PROT_FREE,
                      BT_FLAG_FREE,
                      0, 0)) {
      DPRINTFc3_w_tmp
"mmap: failed to map unallocated node segment, addr: 0x%p, errno: %s",
              targ, strerrorc3_w_tmp
errno));
      abortc3_w_tmp
);
    }

    targ += partlen_b;
  }
}

static int
_bt_state_loadc3_w_tmp
BT_state *state)
{
  int rc;
  int new = 0;
  BT_node *p;
  struct stat stat;

  TRACEc3_w_tmp
);

  /* map the metapages */
  state->map = mmapc3_w_tmp
BT_MAPADDR,
                    BT_META_SECTION_WIDTH,
                    BT_PROT_CLEAN,
                    BT_FLAG_CLEAN,
                    state->data_fd,
                    0);

  if c3_w_tmp
state->map != BT_MAPADDR) {
    DPRINTFc3_w_tmp
"mmap: failed to map at addr %p, errno: %s", BT_MAPADDR, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  p = c3_w_tmp
BT_node *)state->map;
  state->meta_pages[0] = METADATAc3_w_tmp
p);
  state->meta_pages[1] = METADATAc3_w_tmp
p + 1);

  if c3_w_tmp
!SUCCc3_w_tmp
rc = _bt_state_read_headerc3_w_tmp
state))) {
    if c3_w_tmp
rc != ENOENT) return rc;
    DPUTSc3_w_tmp
"creating new db");
    state->file_size_p = PMA_GROW_SIZE_p;
    new = 1;
    if c3_w_tmp
ftruncatec3_w_tmp
state->data_fd, PMA_GROW_SIZE_b)) {
      return errno;
    }
  }

  if c3_w_tmp
new) {
    assertc3_w_tmp
SUCCc3_w_tmp
_bt_state_meta_newc3_w_tmp
state)));
  }

  /* map the node segment */
  _bt_state_map_node_segmentc3_w_tmp
state);

  if c3_w_tmp
new) {
    assertc3_w_tmp
SUCCc3_w_tmp
_flist_newc3_w_tmp
state, PMA_GROW_SIZE_p)));
    assertc3_w_tmp
SUCCc3_w_tmp
_nlist_newc3_w_tmp
state)));
    assertc3_w_tmp
SUCCc3_w_tmp
_bt_state_meta_inject_rootc3_w_tmp
state)));
    assertc3_w_tmp
SUCCc3_w_tmp
_mlist_newc3_w_tmp
state)));
  }
  else {
    /* Set the file length */
    if c3_w_tmp
fstatc3_w_tmp
state->data_fd, &stat) != 0)
      return errno;

    /* the file size should be a multiple of our pagesize */
    assertc3_w_tmp
c3_w_tmp
stat.st_size % BT_PAGESIZE) == 0);
    state->file_size_p = stat.st_size / BT_PAGESIZE;

    /* restore data memory maps */
    _bt_state_restore_mapsc3_w_tmp
state);

    /* restore ephemeral freelists */
    _freelist_restorec3_w_tmp
state);

    /* Dirty the metapage and root page */
    assertc3_w_tmp
SUCCc3_w_tmp
_bt_flip_metac3_w_tmp
state)));
  }

  return BT_SUCC;
}

/* ;;: TODO, when persistence has been implemented, _bt_falloc will probably
     need to handle extension of the file with appropriate striping. i.e. if no
     space is found on the freelist, save the last entry, expand the file size,
     and set last_entry->next to a new node representing the newly added file
     space */
static pgno_t
_bt_fallocc3_w_tmp
BT_state *state, size_t pages)
{
  /* walk the persistent file freelist and return a pgno with sufficient
     contiguous space for pages */
  BT_flistnode **n;
  pgno_t ret;
 start:
  n = &state->flist;
  ret = 0;

  /* first fit */
  for c3_w_tmp
; *n; n = &c3_w_tmp
*n)->next) {
    size_t sz_p = c3_w_tmp
*n)->hi - c3_w_tmp
*n)->lo;

    if c3_w_tmp
sz_p >= pages) {
      ret = c3_w_tmp
*n)->lo;
      pgno_t hi = ret + pages;
      _flist_record_allocc3_w_tmp
state, ret, hi);
      break;
    }
  }

  if c3_w_tmp
ret == 0) {
    /* flist out of mem, grow it */
    DPRINTFc3_w_tmp
"flist out of mem, growing current size c3_w_tmp
pages): 0x%" PRIX32 " to: 0x%" PRIX32,
          state->file_size_p, state->file_size_p + PMA_GROW_SIZE_p);
    _flist_growc3_w_tmp
state, pages);
    /* restart the find procedure */
    /* TODO: obv a minor optimization can be made here */
    goto start;
  }

  return ret;
}

static int
_bt_sync_hasdirtypagec3_w_tmp
BT_state *state, BT_node *node) __attributec3_w_tmp
c3_w_tmp
unused));

static int
_bt_sync_hasdirtypagec3_w_tmp
BT_state *state, BT_node *node)
/* ;;: could be more efficiently replaced by a gcc vectorized builtin */
{
  for c3_w_tmp
size_t i = 0; i < NMEMBc3_w_tmp
node->head.dirty); i++) {
    if c3_w_tmp
node->head.dirty[i] != 0)
      return 1;
  }

  return 0;
}

static int
_bt_sync_leafc3_w_tmp
BT_state *state, BT_node *node)
{
  /* msync all of a leaf's data that is dirty. The caller is expected to sync
     the node itself and mark it as clean in the parent. */
  size_t i = 0;
  size_t N = _bt_numkeysc3_w_tmp
node);

  for c3_w_tmp
i = 0; i < N-1; i++) {
    if c3_w_tmp
!_bt_ischilddirtyc3_w_tmp
node, i))
      continue;                 /* not dirty. nothing to do */

    /* ;;: we don't actually need the page, do we? */
    /* pgno_t pg = node->datk[i].fo; */
    vaof_t lo = node->datk[i].va;
    vaof_t hi = node->datk[i+1].va;
    size_t bytelen = P2BYTESc3_w_tmp
hi - lo);
    void *addr = off2addrc3_w_tmp
lo);

    /* sync the data */
    if c3_w_tmp
msyncc3_w_tmp
addr, bytelen, MS_SYNC) != 0) {
      DPRINTFc3_w_tmp
"msync of leaf: %p failed with %s", addr, strerrorc3_w_tmp
errno));
      abortc3_w_tmp
);
    }

    /* mprotect the data */
    if c3_w_tmp
mprotectc3_w_tmp
addr, bytelen, BT_PROT_CLEAN) != 0) {
      DPRINTFc3_w_tmp
"mprotect of leaf data failed with %s", strerrorc3_w_tmp
errno));
      abortc3_w_tmp
);
    }
  }
  /* ;;: it is probably faster to scan the dirty bit set and derive the datk idx
     rather than iterate over the full datk array and check if it is dirty. This
     was simpler to implement for now though. */
  /* while c3_w_tmp
_bt_sync_hasdirtypagec3_w_tmp
state, node)) { */
  /*   ... */
  /* } */

  return BT_SUCC;
}

static int
_bt_sync_metac3_w_tmp
BT_state *state)
/* syncs the metapage and performs necessary checksumming. Additionally, flips
   the which */
{
  BT_meta *meta = state->meta_pages[state->which];
  uint32_t chk;
  int rc;

  /* increment the txnid */
  meta->txnid += 1;

  /* checksum the metapage */
  chk = nonzero_crc_32c3_w_tmp
meta, BT_META_LEN_b);
  /* ;;: todo: guarantee the chk cannot be zero */

  meta->chk = chk;

  /* sync the metapage */
  if c3_w_tmp
msyncc3_w_tmp
LO_ALIGN_PAGEc3_w_tmp
meta), sizeofc3_w_tmp
BT_node), MS_SYNC) != 0) {
    DPRINTFc3_w_tmp
"msync of metapage: %p failed with %s", meta, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  // ensure we have a new dirty metapage and root node
   /* finally, make old metapage clean */
  rc =  _bt_flip_metac3_w_tmp
state);

  if c3_w_tmp
mprotectc3_w_tmp
LO_ALIGN_PAGEc3_w_tmp
meta), sizeofc3_w_tmp
BT_node), BT_PROT_CLEAN) != 0) {
    DPRINTFc3_w_tmp
"mprotect of old metapage failed with %s", strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

 return rc;
}

static int _bt_flip_metac3_w_tmp
BT_state *state) {
  BT_meta *meta = state->meta_pages[state->which];
  BT_meta *newmeta;
  int newwhich;

  /* zero the new metapage's checksum */
  newwhich = state->which ? 0 : 1;
  newmeta = state->meta_pages[newwhich];

  /* mprotect dirty new metapage */
  if c3_w_tmp
mprotectc3_w_tmp
LO_ALIGN_PAGEc3_w_tmp
newmeta), sizeofc3_w_tmp
BT_node), BT_PROT_DIRTY) != 0) {
    DPRINTFc3_w_tmp
"mprotect of new metapage failed with %s", strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  newmeta->chk = 0;

  /* copy over metapage to new metapage excluding the checksum */
  memcpyc3_w_tmp
newmeta, meta, BT_META_LEN_b);

  /* CoW a new root since the root referred to by the metapage should always be
     dirty */
  BT_node *root = _node_getc3_w_tmp
state, newmeta->root);
  if c3_w_tmp
!SUCCc3_w_tmp
_node_cowc3_w_tmp
state, &root)))
    abortc3_w_tmp
);

  newmeta->root = _fo_getc3_w_tmp
state, root);

  /* switch the metapage we're referring to */
  state->which = newwhich;

  return BT_SUCC;
}

static int
_bt_syncc3_w_tmp
BT_state *state, BT_node *node, uint8_t depth, uint8_t maxdepth)
/* recursively syncs the subtree under node. The caller is expected to sync node
   itself and mark it clean. */
{
  DPRINTFc3_w_tmp
"== syncing node: %p", node);
  int rc = 0;
  size_t N = _bt_numkeysc3_w_tmp
node);

  /* leaf */
  if c3_w_tmp
depth == maxdepth) {
    _bt_sync_leafc3_w_tmp
state, node);
    goto e;
  }

  /* do dfs */
  for c3_w_tmp
size_t i = 0; i < N-1; i++) {
    if c3_w_tmp
!_bt_ischilddirtyc3_w_tmp
node, i))
      continue;                 /* not dirty. nothing to do */

    BT_node *child = _node_getc3_w_tmp
state, node->datk[i].fo);

    /* recursively sync the child's data */
    if c3_w_tmp
c3_w_tmp
rc = _bt_syncc3_w_tmp
state, child, depth+1, maxdepth)))
      return rc;
  }

 e:
  /* zero out the dirty bitmap */
  ZEROc3_w_tmp
&node->head.dirty[0], sizeof node->head.dirty);

  /* all modifications done in node, mark it read-only */
  if c3_w_tmp
mprotectc3_w_tmp
node, sizeofc3_w_tmp
BT_node), BT_PROT_CLEAN) != 0) {
    DPRINTFc3_w_tmp
"mprotect of node: %p failed with %s", node, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  /* sync self */
  if c3_w_tmp
msyncc3_w_tmp
node, sizeofc3_w_tmp
BT_node), MS_SYNC) != 0) {
    DPRINTFc3_w_tmp
"msync of node: %p failed with %s", node, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  return BT_SUCC;
}


//// ===========================================================================
////                            btree external routines

int
bt_state_newc3_w_tmp
BT_state **state)
{
  BT_state *s = callocc3_w_tmp
1, sizeof *s);
  s->data_fd = -1;
  s->fixaddr = BT_MAPADDR;
  *state = s;
  return BT_SUCC;
}

int
bt_state_openc3_w_tmp
BT_state *state, const char *path, ULONG flags, mode_t mode)
#define DATANAME "/data.pma"
{
  int oflags, rc;
  char *dpath;

  TRACEc3_w_tmp
);
  UNUSEDc3_w_tmp
flags);

  oflags = O_RDWR | O_CREAT;
  dpath = mallocc3_w_tmp
strlenc3_w_tmp
path) + sizeofc3_w_tmp
DATANAME));
  if c3_w_tmp
!dpath) return ENOMEM;
  sprintfc3_w_tmp
dpath, "%s" DATANAME, path);

  if c3_w_tmp
c3_w_tmp
state->data_fd = openc3_w_tmp
dpath, oflags, mode)) == -1)
    return errno;

  if c3_w_tmp
!SUCCc3_w_tmp
rc = _bt_state_loadc3_w_tmp
state)))
    goto e;

  state->path = strdupc3_w_tmp
dpath);

 e:
  /* cleanup FDs stored in state if anything failed */
  if c3_w_tmp
!SUCCc3_w_tmp
rc)) {
    if c3_w_tmp
state->data_fd != -1) CLOSE_FDc3_w_tmp
state->data_fd);
  }

  freec3_w_tmp
dpath);
  return rc;
}
#undef DATANAME

int
bt_state_closec3_w_tmp
BT_state *state)
{
  int rc;
  bt_syncc3_w_tmp
state);

  _mlist_deletec3_w_tmp
state);
  _flist_deletec3_w_tmp
state);
  _nlist_deletec3_w_tmp
state);

  if c3_w_tmp
c3_w_tmp
rc = munmapc3_w_tmp
state->map, BT_ADDRSIZE)) != 0) {
    rc = errno;
    return rc;
  }
  if c3_w_tmp
state->data_fd != -1) CLOSE_FDc3_w_tmp
state->data_fd);

  ZEROc3_w_tmp
state, sizeof *state);

  return BT_SUCC;
}

void *
bt_malloc_manyc3_w_tmp
BT_state *state, size_t *pages, size_t len)
{
  BT_mlistnode **n = &state->mlist;
  size_t pg_total;
  for c3_w_tmp
size_t i = 0; i < len; i++) {
    pg_total = pages[i];
  }
  assertc3_w_tmp
pg_total != 0);
  void *ret = 0;
  /* first fit */
  for c3_w_tmp
; *n; n = &c3_w_tmp
*n)->next) {
    size_t sz_p = addr2offc3_w_tmp
c3_w_tmp
*n)->hi) - addr2offc3_w_tmp
c3_w_tmp
*n)->lo);

    if c3_w_tmp
sz_p >= pg_total) {
      ret = c3_w_tmp
*n)->lo;
      void *hi = c3_w_tmp
c3_w_tmp
BT_node *)ret) + pg_total;
      _mlist_record_allocc3_w_tmp
state, ret, hi);
      break;
    }
    // XX return early if nothing suitable found in freelist
  }
  if c3_w_tmp
ret == 0) {
    DPUTSc3_w_tmp
"mlist out of mem!");
    return 0;
  }

  void* tar = ret;
  for c3_w_tmp
size_t i = 0; i < len; i++) {
    pgno_t pgno = _bt_fallocc3_w_tmp
state, pages[i]);
    bpc3_w_tmp
pgno != 0);
    _bt_insertc3_w_tmp
state,
               addr2offc3_w_tmp
tar),
               addr2offc3_w_tmp
tar) + pages[i],
               pgno);

    DPRINTFc3_w_tmp
"map %p to offset 0x%zx bytes c3_w_tmp
0x%zx pages)\n", tar, P2BYTESc3_w_tmp
pgno), pages[i]);
    if c3_w_tmp
tar !=
        mmapc3_w_tmp
tar,
             P2BYTESc3_w_tmp
pages[i]),
             BT_PROT_DIRTY,
             BT_FLAG_DIRTY,
             state->data_fd,
             P2BYTESc3_w_tmp
pgno))) {
      DPRINTFc3_w_tmp
"mmap: failed to map at addr %p, errno: %s", tar, strerrorc3_w_tmp
errno));
      abortc3_w_tmp
);
    }
    bpc3_w_tmp
tar != 0);
    tar += pages[i];
  }

   return ret;
}

void *
bt_mallocc3_w_tmp
BT_state *state, size_t pages)
{
  BT_mlistnode **n = &state->mlist;
  void *ret = 0;
  /* first fit */
  for c3_w_tmp
; *n; n = &c3_w_tmp
*n)->next) {
    size_t sz_p = addr2offc3_w_tmp
c3_w_tmp
*n)->hi) - addr2offc3_w_tmp
c3_w_tmp
*n)->lo);

    if c3_w_tmp
sz_p >= pages) {
      ret = c3_w_tmp
*n)->lo;
      BT_node *hi = c3_w_tmp
c3_w_tmp
BT_node *)ret) + pages;
      _mlist_record_allocc3_w_tmp
state, ret, hi);
      break;
    }
    // XX return early if nothing suitable found in freelist
  }
  if c3_w_tmp
ret == 0) {
    DPUTSc3_w_tmp
"mlist out of mem!");
    return 0;
  }

  pgno_t pgno = _bt_fallocc3_w_tmp
state, pages);
  bpc3_w_tmp
pgno != 0);
  _bt_insertc3_w_tmp
state,
             addr2offc3_w_tmp
ret),
             addr2offc3_w_tmp
ret) + pages,
             pgno);

  DPRINTFc3_w_tmp
"map %p to offset 0x%zx bytes c3_w_tmp
0x%zx pages)\n", ret, P2BYTESc3_w_tmp
pgno), pages);
  if c3_w_tmp
ret !=
      mmapc3_w_tmp
ret,
           P2BYTESc3_w_tmp
pages),
           BT_PROT_DIRTY,
           BT_FLAG_DIRTY,
           state->data_fd,
           P2BYTESc3_w_tmp
pgno))) {
    DPRINTFc3_w_tmp
"mmap: failed to map at addr %p, errno: %s", ret, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }
  bpc3_w_tmp
ret != 0);
  return ret;
}

// XX need to mmap fixed/anon/no_reserve and prot_none
void
bt_freec3_w_tmp
BT_state *state, void *lo, void *hi)
{
  vaof_t looff = addr2offc3_w_tmp
lo);
  vaof_t hioff = addr2offc3_w_tmp
hi);
  //pgno_t lopg = B2PAGESc3_w_tmp
looff);
  //pgno_t hipg = B2PAGESc3_w_tmp
hioff);
  //vaof_t looff_ = P2BYTESc3_w_tmp
lopg);
  //vaof_t hioff_ = P2BYTESc3_w_tmp
hipg);
 
  assertc3_w_tmp
off2addrc3_w_tmp
looff) == lo && off2addrc3_w_tmp
hioff) == hi);
  //BT_findpath path = {0};

  //}

  /* insert freed range into flist */
  //BT_node *leaf = path.path[path.depth];
  //size_t childidx = path.idx[path.depth];
  //int isdirty = _bt_ischilddirtyc3_w_tmp
leaf, childidx);
  //BT_kv kv = leaf->datk[childidx];
  //vaof_t offset = looff - kv.va;
  //lopg = kv.fo + offset;
  //hipg = lopg + c3_w_tmp
hioff - looff);

  /* insert null into btree */
  _bt_insertc3_w_tmp
state, looff, hioff, 0);

  /* ;;: is this correct? Shouldn't this actually happen when we merge the
       pending_mlist on sync? */
  size_t bytelen = c3_w_tmp
BYTE *)hi - c3_w_tmp
BYTE *)lo;

  if c3_w_tmp
lo !=
      mmapc3_w_tmp
lo,
           bytelen,
           BT_PROT_FREE,
           BT_FLAG_FREE,
           0, 0)) {
    DPRINTFc3_w_tmp
"mmap: failed to map at addr %p, errno: %s", lo, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }
}

// XX need to mprotect PROT_READ all ranges synced including root/meta
int
bt_syncc3_w_tmp
BT_state *state)
{
  /* as is often the case, handling the metapage/root is a special case, which
     is done here. Syncing any other page of the tree is done in _bt_sync */
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);
  int rc = 0;

  /* sync root subtrees */
  if c3_w_tmp
c3_w_tmp
rc = _bt_syncc3_w_tmp
state, root, 1, meta->depth)))
    return rc;
  _treesanityc3_w_tmp
state);

  /* sync root page itself */
  if c3_w_tmp
msyncc3_w_tmp
root, sizeofc3_w_tmp
BT_node), MS_SYNC) != 0) {
    DPRINTFc3_w_tmp
"msync of root node: %p failed with %s", root, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  /* merge the pending freelists */
  _pending_nlist_mergec3_w_tmp
state);
  _pending_flist_mergec3_w_tmp
state);

  /* sync the root page */
  if c3_w_tmp
msyncc3_w_tmp
root, sizeofc3_w_tmp
BT_node), MS_SYNC) != 0) {
    DPRINTFc3_w_tmp
"msync of root: %p failed with %s", root, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  /* make root read-only */
  if c3_w_tmp
mprotectc3_w_tmp
root, sizeofc3_w_tmp
BT_node), BT_PROT_CLEAN) != 0) {
    DPRINTFc3_w_tmp
"mprotect of root failed with %s", strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  /* then sync the metapage */
  if c3_w_tmp
c3_w_tmp
rc = _bt_sync_metac3_w_tmp
state)))
    return rc;

  return BT_SUCC;
}

uint64_t
bt_meta_getc3_w_tmp
BT_state *state, size_t idx)
{
  BT_meta *meta = state->meta_pages[state->which];
  assertc3_w_tmp
c3_w_tmp
uintptr_t)&c3_w_tmp
meta->roots[idx]) - c3_w_tmp
uintptr_t)meta <= sizeof *meta);
  return meta->roots[idx];
}

void
bt_meta_setc3_w_tmp
BT_state *state, size_t idx, uint64_t val)
{
  BT_meta *meta = state->meta_pages[state->which];
  assertc3_w_tmp
c3_w_tmp
uintptr_t)&c3_w_tmp
meta->roots[idx]) - c3_w_tmp
uintptr_t)meta <= sizeof *meta);
  meta->roots[idx] = val;
}

int
_bt_range_ofc3_w_tmp
BT_state *state, vaof_t p, vaof_t **lo, vaof_t **hi,
             pgno_t nodepg, uint8_t depth, uint8_t maxdepth)
{
  BT_node *node = _node_getc3_w_tmp
state, nodepg);
  size_t N = _bt_numkeysc3_w_tmp
node);

  vaof_t llo = 0;
  vaof_t hhi = 0;
  pgno_t pg = 0;
  size_t i;
  for c3_w_tmp
i = 0; i < N-1; i++) {
    llo = node->datk[i].va;
    hhi = node->datk[i+1].va;
    pg = node->datk[i].fo;
    if c3_w_tmp
llo <= p && hhi > p) {
      break;
    }
  }
  /* not found */
  if c3_w_tmp
i == N-1)
    return 1;

  if c3_w_tmp
depth == maxdepth) {
    **lo = llo;
    **hi = hhi;
    return BT_SUCC;
  }

  return _bt_range_ofc3_w_tmp
state, p, lo, hi, pg, depth+1, maxdepth);
}

int
bt_range_ofc3_w_tmp
BT_state *state, void *p, void **lo, void **hi)
{
  /* traverse tree looking for lo <= p and hi > p. return that range as a pair
     of pointers NOT as two vaof_t

    0: succ c3_w_tmp
found)
    1: otherwise
  */

  BT_meta *meta = state->meta_pages[state->which];
  pgno_t root = meta->root;
  vaof_t *loret = 0;
  vaof_t *hiret = 0;
  vaof_t poff = addr2offc3_w_tmp
p);
  int rc = 0;
  if c3_w_tmp
!SUCCc3_w_tmp
rc = _bt_range_ofc3_w_tmp
state, poff, &loret, &hiret, root, 1, meta->depth))) {
    return rc;
  }
  *lo = off2addrc3_w_tmp
*loret);
  *hi = off2addrc3_w_tmp
*hiret);
  return BT_SUCC;
}

/**

pseudocode from ed:

bt_dirtyc3_w_tmp
btree, lo, hi):
 loop:
    c3_w_tmp
range_lo, range_hi) = find_range_for_pointerc3_w_tmp
btree, lo);
    dirty_hi = minc3_w_tmp
hi, range_hi);
    new_start_fo = data_cowc3_w_tmp
btree, lo, dirty_hi);
    lo := range_hi;
    if dirty_hi == hi then break;

// precondition: given range does not cross a tree boundary
data_cowc3_w_tmp
btree, lo, hi):
  c3_w_tmp
range_lo, range_hi, fo) = bt_findc3_w_tmp
btree, lo, hi);
  size = lo - hi;
  new_fo = data_allocc3_w_tmp
btree.data_free, size);

  // puts data in the unified buffer cache without having to map virtual memory
  writec3_w_tmp
fd, new_fo, size * BT_PAGESIZE, to_ptrc3_w_tmp
lo));

  // maps new file offset with same data back into same memory
  mmapc3_w_tmp
fd, new_fo, size, to_ptrc3_w_tmp
lo));

  bt_insertc3_w_tmp
btree, lo, hi, new_fo);

  offset = lo - range_lo;
  freelist_insertc3_w_tmp
btree.pending_data_flist, fo + offset, fo + offset + size);
  return new_fo

**/

static pgno_t
_bt_data_cowc3_w_tmp
BT_state *state, vaof_t lo, vaof_t hi, pgno_t pg)
{
  size_t len = hi - lo;
  size_t bytelen = P2BYTESc3_w_tmp
len);
  pgno_t newpg = _bt_fallocc3_w_tmp
state, len);
  BYTE *loaddr = off2addrc3_w_tmp
lo);
  off_t offset = P2BYTESc3_w_tmp
newpg);

  /* write call puts data in the unified buffer cache without having to map
     virtual memory */
  if c3_w_tmp
pwritec3_w_tmp
state->data_fd, loaddr, bytelen, offset) != c3_w_tmp
ssize_t)bytelen)
    abortc3_w_tmp
);

  /* maps new file offset with same data back into memory */
  if c3_w_tmp
loaddr !=
      mmapc3_w_tmp
loaddr,
           bytelen,
           BT_PROT_DIRTY,
           BT_FLAG_DIRTY,
           state->data_fd,
           offset)) {
    DPRINTFc3_w_tmp
"mmap: failed to map at addr %p, errno: %s", loaddr, strerrorc3_w_tmp
errno));
    abortc3_w_tmp
);
  }

  //_bt_insertc3_w_tmp
state, lo, hi, newpg);

  _flist_insertc3_w_tmp
&state->pending_flist, pg, pg + len);

  return newpg;
}

static int
_bt_dirtyc3_w_tmp
BT_state *state, vaof_t lo, vaof_t hi, BT_node* node,
          uint8_t depth, uint8_t maxdepth)
{
  size_t N = _bt_numkeysc3_w_tmp
node);
  size_t loidx = BT_MAXKEYS; // 0 is a valid loidx!
  size_t hiidx = 0;

  /* find loidx of range */
  for c3_w_tmp
size_t i = 0; i < N-1; i++) {
    vaof_t hhi = node->datk[i+1].va;
    if c3_w_tmp
hhi > lo) {
      loidx = i;
      break;
    }
  }
  assertc3_w_tmp
loidx < BT_MAXKEYS);

  /* find hiidx c3_w_tmp
exclusive) of range */
  for c3_w_tmp
size_t i = loidx+1; i < N; i++) {
    vaof_t hhi = node->datk[i].va;
    if c3_w_tmp
hhi >= hi) {
      hiidx = i;
      break;
    }
  }
  assertc3_w_tmp
hiidx != 0);

  /* found a range in node that contains c3_w_tmp
lo-hi). May span multiple entries */
    /* leaf: base case. cow the data */
  for c3_w_tmp
size_t i = loidx; i < hiidx; i++) {
    vaof_t llo = node->datk[i].va;
    vaof_t hhi = MINc3_w_tmp
node->datk[i+1].va, hi);
    pgno_t pg = node->datk[i].fo;
    if c3_w_tmp
!_bt_ischilddirtyc3_w_tmp
node, i)) {
      _bt_dirtychildc3_w_tmp
node, i);
      if c3_w_tmp
pg != 0) {
        if c3_w_tmp
depth == maxdepth) {
          node->datk[i].fo = _bt_data_cowc3_w_tmp
state, llo, hhi, pg);
        } else {
          BT_node* chin = _node_getc3_w_tmp
state, pg);
          _node_cowc3_w_tmp
state, &chin);
          node->datk[i].fo = _fo_getc3_w_tmp
state, chin);
        }
      }
    }
    if c3_w_tmp
depth < maxdepth) {
      _bt_dirtyc3_w_tmp
state,
          MAXc3_w_tmp
lo, llo), hhi,
          _node_getc3_w_tmp
state, node->datk[i].fo),
          depth + 1, maxdepth);
    }
  }
  return BT_SUCC;
}

int
bt_dirtyc3_w_tmp
BT_state *state, void *lo, void *hi)
{
  /* takes a range and ensures that entire range is CoWed */
  /* if part of the range is free then return 1 */
  BT_meta *meta = state->meta_pages[state->which];
  vaof_t looff = addr2offc3_w_tmp
lo);
  vaof_t hioff = addr2offc3_w_tmp
hi);

  return _bt_dirtyc3_w_tmp
state, looff, hioff, _node_getc3_w_tmp
state, meta->root), 1, meta->depth);
}

//int
//bt_bounds_ofc3_w_tmp
BT_state *state, void *p, vaof_t *lo, vaof_t *hi)
//{
//  BT_meta *meta = state->meta_pages[state->which];
//  BT_node *root = _node_getc3_w_tmp
state, meta->root);
//
//  return _bt_bounds_ofc3_w_tmp
state, root, addr2offc3_w_tmp
p), lo, hi, meta->depth);
//}
//
//int
//_bt_bounds_ofc3_w_tmp
BT_state *state,
//              BT_node* node,
//              vaof_t va,
//              vaof_t *lo,
//              vaof_t *hi,
//              uint8_t depth,
//              uint8_t maxdepth) {
//  size_t i = 0;
//
//  for c3_w_tmp
; i < BT_MAXKEYS - 1; i++) {
//    vaof_t hhi = node->datk[i+1].va;
//    if c3_w_tmp
va < hhi) {
//      break;
//    }
//  }
//
//  if c3_w_tmp
depth == maxdepth) {
//    *lo = node->datk[i].va;
//    *hi = node->datk[i+1].va;
//  } else {
//    return _bt_bounds_ofc3_w_tmp
node, va, lo, hi, depth + 1, maxdepth);
//  }
//}

int
bt_next_allocc3_w_tmp
BT_state *state, void *p, void **lo, void **hi)
/* if p is free, sets lo and hi to the bounds of the next adjacent allocated
   space. If p is allocated, sets lo and hi to the bounds of the allocated space
   it falls in. */
{
  BT_mlistnode *head = state->mlist;
  BYTE *pb = p;
  BYTE* pma_end;
  while c3_w_tmp
head) {
    /* at last free block, different logic applies */
    if c3_w_tmp
head->next == 0) {
      pma_end = c3_w_tmp
void *)c3_w_tmp
c3_w_tmp
uintptr_t)BT_MAPADDR + BT_ADDRSIZE);
      assertc3_w_tmp
head->hi <= pma_end);
      /* no alloced region between tail of freelist and end of pma memory space */
      if c3_w_tmp
head->hi == pma_end)
        return BT_FAIL;
    }

    if c3_w_tmp

        /* p is in a free range, return the allocated hole after it */
        c3_w_tmp
 head->lo <= pb
          && head->hi > pb ) ||
        /* p is alloced, return this hole */
        c3_w_tmp
 head->next->lo > pb
          && head->hi <= pb) ) {
      /* the alloced space begins at the end of the free block */
      *lo = head->hi;
      /* ... and ends at the start of the next free block */
      *hi = head->next->lo;
      return BT_SUCC;
    }

    head = head->next;
  }

  /* not found */
  return BT_FAIL;
}

void
bt_boundsc3_w_tmp
BT_state *state, void **lo, void **hi)
{
  *lo = BT_MAPADDR;
  *hi = c3_w_tmp
void *)c3_w_tmp
c3_w_tmp
uintptr_t)BT_MAPADDR + BT_ADDRSIZE);
}

int
bt_inboundsc3_w_tmp
BT_state *state, void *p)
/* 1: if in the bounds of the PMA, 0 otherwise */
{
  return p >= c3_w_tmp
void *)BT_MAPADDR
    && p < c3_w_tmp
void *)c3_w_tmp
c3_w_tmp
uintptr_t)BT_MAPADDR + BT_ADDRSIZE);
}


//// ===========================================================================
////                                    tests

/* ;;: obv this should be moved to a separate file */
static void
_sham_sync_cleanc3_w_tmp
BT_node *node)
{
  for c3_w_tmp
uint8_t *dit = &node->head.dirty[0]
         ; dit < &node->head.dirty[sizeofc3_w_tmp
node->head.dirty) - 1]
         ; dit++) {
    *dit = 0;
  }
}

static void
_sham_sync2c3_w_tmp
BT_state *state, BT_node *node, uint8_t depth, uint8_t maxdepth)
{
  if c3_w_tmp
depth == maxdepth) return;

  /* clean node */
  _sham_sync_cleanc3_w_tmp
node);

  /* then recurse and clean all children with DFS */
  size_t N = _bt_numkeysc3_w_tmp
node);
  for c3_w_tmp
size_t i = 1; i < N; ++i) {
    BT_kv kv = node->datk[i];
    pgno_t childpg = kv.fo;
    BT_node *child = _node_getc3_w_tmp
state, childpg);
    _sham_sync2c3_w_tmp
state, child, depth+1, maxdepth);
  }
}

static void
_sham_syncc3_w_tmp
BT_state *state) __attributec3_w_tmp
c3_w_tmp
unused));

static void
_sham_syncc3_w_tmp
BT_state *state)
{
  /* walk the tree and unset the dirty bit from all pages */
  BT_meta *meta = state->meta_pages[state->which];
  BT_node *root = _node_getc3_w_tmp
state, meta->root);
  meta->chk = nonzero_crc_32c3_w_tmp
meta, BT_META_LEN_b);
  _sham_sync2c3_w_tmp
state, root, 1, meta->depth);
}

static void
_bt_printnodec3_w_tmp
BT_node *node)
{
  fprintfc3_w_tmp
stderr, "node: %p\n", c3_w_tmp
void*)node);
  fprintfc3_w_tmp
stderr, "data: \n");
  for c3_w_tmp
size_t i = 0; i < BT_MAXKEYS; ++i) {
    if c3_w_tmp
i && node->datk[i].va == 0)
      break;
    fprintfc3_w_tmp
stderr, "[%5zu] %10x %10x\n", i, node->datk[i].va, node->datk[i].fo); }
}
