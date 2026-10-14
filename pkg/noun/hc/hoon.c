// Hoon, shared by the compiler, hc/hc.c, and the language server, main.c,
// which each include it: hoon to a type and a nock formula, following
// +mint in hoon.hoon. The parser and +open follow +vast and +ap, the
// type system +ut: +play and +nest, and the part that makes nock, +mint
// and +mull, cores through +mine, +laze and +hemp, wing lookup that
// carries formulas, +fish, the formula builders, and +musk for ^~. Desk
// files are built through ford as clay builds them, imports and all.
//
// Types are the same nouns hoon uses, so the type of a program can be
// compared against the reference noun for noun, after +burp. hc/test/
// does that against the reference implementation.

#include "stdint.h"
#include "stddef.h"

#ifdef _MSC_VER
#pragma comment(linker, "/subsystem:console")
// the stack of Unix, not Windows' 1MB, for stackdeep below
#pragma comment(linker, "/stack:8388608")
#pragma comment(lib, "kernel32.lib")
#endif

#ifdef DEBUG
#  if __GNUC__
#    define assert(c) if (!(c)) __builtin_trap()
#  elif _MSC_VER
#    define assert(c) if (!(c)) __debugbreak()
#  else
#    define assert(c) if (!(c)) *(volatile i32 *)0 = 0
#  endif
#else
#  define assert(c)
#endif

typedef uint8_t     u8;
typedef char        byte;
typedef uint16_t    u16;
typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     b32;
typedef int32_t     i32;
typedef int64_t     i64;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef float       f32;
typedef double      f64;
typedef long double fld;
typedef uintptr_t   uptr;
typedef ptrdiff_t   size;
typedef size_t      usize;

#define sizeof(x)    (size)sizeof(x)
#define alignof(x)   (size)_Alignof(x)
#define countof(a)   (sizeof(a)/sizeof(*(a)))
#define lengthof(s)  (countof(s) - 1)
#define new(a, t, n) (t *)alloc(a, sizeof(t), alignof(t), n)

#define S(s) (s8){(u8 *)(s), lengthof(s)}

// the builtins used here that cl.exe lacks; clang-cl has them
#if defined(_MSC_VER) && !defined(__clang__)
unsigned char _BitScanReverse64(unsigned long *, unsigned __int64);
#pragma intrinsic(_BitScanReverse64)
// as clang's, so undefined for 0
static i32 msvcclzll(u64 v) {
  unsigned long i;
  _BitScanReverse64(&i, v);
  return 63 - (i32)i;
}
#define __builtin_clzll(v)  msvcclzll(v)
#endif

// __builtin_strlen of a string not known at compile time is a call to
// strlen, which Windows and Linux, built without libc, don't have; and
// cl.exe has no __builtin_strlen
#if defined(_WIN32) || defined(__linux__)
static usize ownstrlen(char *s) {
  usize n = 0;
  while (s[n]) n++;
  return n;
}
#define __builtin_strlen(s) ownstrlen(s)
#endif

#define MAX(x, y) ( ((x) > (y)) ? (x) : (y) )
#define MIN(x, y) ( ((x) < (y)) ? (x) : (y) )

#define MAXUINT64 0xffffffffffffffff

void osfail(void);
void osrelease(byte *, byte *);
b32  oswrite(i32, u8 *, i32);
size osread(i32, u8 *, size);
byte *osreserve(size);

// An arena is a list of chunks, each from the host with u3a_malloc: on
// the road a compile runs on, and gone with it. When one is full
// another is linked after it, so nothing is copied to make room. beg is
// where the next thing goes and end where the chunk it's in ends; dat
// is the start of the first. Setting beg back to where it was, as code
// all over does to give back what it's done with, may leave it in an
// earlier chunk than cur: alloc looks, and goes back to that one, and
// the chunks after it are kept to be filled again.
//
// In an arena that's never set back, solo, a big thing gets a chunk to
// itself, off to the side, which is given back to the host when the
// thing is released: a table that's outgrown, mostly.
typedef struct chunk chunk;
struct chunk {
  chunk *prev;
  chunk *next;
  byte  *end;
  // and its bytes
};

typedef struct {
  byte  *dat;
  byte  *beg;
  byte  *end;
  chunk *cur;
  b32    solo;
  chunk *side;   // chunks of one thing each
} arena;

enum { chunkmin = 1 << 20 };

void oom(void) {
#ifndef HC_EMBED
  static u8 msg[] = "out of memory\n";
  oswrite(2, (u8 *)msg, lengthof(msg));
#endif
  osfail();
}

void *hostalloc(size len);
void  hostfree(void *ptr);

static inline byte *chunkdata(chunk *c) {
  return (byte*)(c + 1);
}

// the room left in the chunk beg is in, having found which that is
size arenaroom(arena *a) {
  chunk *c = a->cur;
  if (!a->beg && c) {
    // set back to where it was before there was a chunk: the first
    while (c->prev) c = c->prev;
    a->beg = chunkdata(c);
  }
  while (c && !(a->beg >= chunkdata(c) && a->beg <= c->end)) c = c->prev;
  if (c != a->cur) {
    a->cur = c;
    a->end = c ? c->end : a->beg;
  }
  return c ? a->end - a->beg : 0;
}

// go on to a chunk with len bytes free: the next, if it's big enough,
// or one further on that is, or a new one, linked in after this
void arenamore(arena *a, size len) {
  chunk *nex = a->cur ? a->cur->next : 0;
  chunk *c = nex;
  // any of those kept that's big enough, brought next
  while (c && c->end - chunkdata(c) < len) c = c->next;
  if (c && c != nex) {
    c->prev->next = c->next;
    if (c->next) c->next->prev = c->prev;
  } else if (!c) {
    size cap = MAX(len, chunkmin);
    c = hostalloc(sizeof(chunk) + cap);
    c->end = chunkdata(c) + cap;
  }
  if (c != nex) {
    c->prev = a->cur;
    c->next = nex;
    if (a->cur) a->cur->next = c;
    if (nex) nex->prev = c;
  }
  a->cur = c;
  a->beg = chunkdata(c);
  a->end = c->end;
  if (!a->dat) a->dat = a->beg;
}

// give back the thing at p, if it has a chunk to itself
void arenafree(arena *a, byte *p) {
  for (chunk *c = a->side; c; c = c->next) {
    if (chunkdata(c) != p) continue;
    if (c->prev) c->prev->next = c->next; else a->side = c->next;
    if (c->next) c->next->prev = c->prev;
    hostfree(c);
    return;
  }
}

byte *alloc(arena *a, size objsize, size align, size count) {
  if (objsize && count > ((size)1 << 40) / objsize) oom();
  size total = count * objsize;
  if (a->solo && total >= chunkmin / 16) {
    // the host's are aligned as far as anything here asks
    chunk *n = hostalloc(sizeof(chunk) + total);
    n->prev = 0;
    n->next = a->side;
    n->end = chunkdata(n) + total;
    if (a->side) a->side->prev = n;
    a->side = n;
    byte *p = chunkdata(n);
    for (size i = 0; i < total; i++) p[i] = 0;
    return p;
  }
  size room = arenaroom(a);
  size padding = -(uptr)a->beg & (align - 1);
  if (!a->cur || total + padding > room) {
    arenamore(a, total + align);
    padding = -(uptr)a->beg & (align - 1);
  }
  byte *p = a->beg + padding;
  a->beg += padding + total;
  for (size i = 0; i < total; i++) {
    p[i] = 0;
  }
  return p;
}
typedef struct {
  u8  *buf;
  size len;
} s8;

b32 osreadfile(arena *, char *, s8 *);
size oslistdir(arena *, char *, char ***);
b32  oswritefile(arena *, char *, u8 *, size);
char *osgetenv(char *);
void osmkdir(char *);
char *osabspath(arena *, char *);
b32  osisdir(char *);
b32  osexists(char *);


#ifndef HC_EMBED
void * memset(void *, int, size_t);
#pragma intrinsic(memset)

#pragma function(memset)
void *memset(void *d, int c, size_t n) {
  char *dst = (char *)d;
  for (; n; n--) *dst++ = (char)c;
  return d;
}

void* memcpy(void *, const void *, size_t);
#pragma intrinsic(memcpy)

#pragma function(memcpy)
void* memcpy(void* dest, const void* src, size_t count) {
  char* dest8 = (char*)dest;
  const char* src8 = (const char*)src;
  while(count--) {
    *dest8++ = *src8++;
  }
  return dest;
}
#endif

void copy(byte *restrict dst, byte *restrict src, size len) {
  for (size i = 0; i < len; i++) {
    dst[i] = src[i];
  }
}

#define push(s, arena) \
  ((s)->len >= (s)->cap \
    ? grow(s, sizeof(*(s)->data), arena), \
      (s)->data + (s)->len++ \
    : (s)->data + (s)->len++)

void grow(void *slice, size siz, arena *a) {
  struct {
    byte *data;
    size len;
    size cap;
  } replica;
  copy((byte*)&replica, slice, sizeof(replica));

  size align = 16;

  if (!replica.data) {
    replica.cap = 1;
    replica.data = alloc(a, 2*siz, align, replica.cap);
  } else if (a->beg == replica.data + siz*replica.cap && arenaroom(a) >= siz*replica.cap) {
    // the last thing in its chunk, with room after it: in place
    alloc(a, siz, 1, replica.cap);
  } else {
    void *data = alloc(a, 2*siz, align, replica.cap);
    copy(data, replica.data, siz*replica.len);
    replica.data = data;
  }

  replica.cap *= 2;
  copy(slice, (byte*)&replica, sizeof(replica));
}

typedef struct {
  u8 *buf;
  i32 len;
  i32 cap;
  i32 fd;
  b32 err;
} bufout;

void flush(bufout *b) {
  b->err |= b->fd < 0;
  if (!b->err && b->len) {
    b->err |= !oswrite(b->fd, b->buf, b->len);
    b->len = 0;
  }
}



void append(bufout *b, s8 src) {
  u8 *end = src.buf + src.len;
  while (!b->err && src.buf<end) {
    size left = end - src.buf;
    size avail = b->cap - b->len;
    size amount = avail<left ? avail : left;

    for (size i = 0; i < amount; i++) {
      b->buf[b->len+i] = src.buf[i];
    }
    b->len += amount;
    src.buf += amount;

    if (amount < left) {
      flush(b);
    }
  }
}

void appendsize(bufout *b, size x) {
  u8 tmp[64];
  u8 *end = tmp + sizeof(tmp);
  u8 *beg = end;
  size t = x>0 ? -x : x;
  do {
    *--beg = '0' - t%10;
  } while (t /= 10);
  if (x < 0) {
    *--beg = '-';
  }
  s8 s = {.buf = beg, .len = end-beg};
  append(b, s);
}

// Profiling. Built with -DPROFILE, each function marked PROF(zone)
// counts its calls and time, inclusive and self, as read off the cycle
// counter, and the totals go to stderr at the end. On x86 that's the TSC.
// On arm64, where user code can't read cycles, it's the generic timer,
// which ticks at cyclefreq(): 24 MHz on Apple silicon, too coarse for
// one call but not for the millions counted here. Time is reported in
// the counter's ticks scaled to 1 GHz, so in ns.

#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
unsigned __int64 __rdtsc(void);
#pragma intrinsic(__rdtsc)
#endif

u64 cycles(void) {
#if defined(__x86_64__) || defined(__i386__)
  return __builtin_ia32_rdtsc();
#elif defined(__aarch64__) && !defined(_MSC_VER)
  u64 v;
  __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
  return v;
#elif defined(_MSC_VER) && defined(_M_X64)
  return __rdtsc();
#else
  return 0;
#endif
}

// ticks of cycles() per second, or 0 when it counts cycles
u64 cyclefreq(void) {
#if defined(__aarch64__) && !defined(_MSC_VER)
  u64 v;
  __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
  return v;
#else
  return 0;
#endif
}

#if defined(PROFILE) && !defined(_MSC_VER)

#define ZONES(X) \
  X(cons) X(parse) X(open) X(setput) X(setuni) X(memofind) X(tfork) \
  X(repo) X(peek) X(wrap) X(look) X(loot) X(fondlimb) X(fond) X(fire) \
  X(take) X(tack) X(elbo) X(fuse) X(crop) X(cool) X(chip) X(nest) X(redo) \
  X(miss) X(feel) X(play) X(hike) X(nock) X(araw) X(burp) X(bran) X(fish) \
  X(arfish) X(mfondlimb) X(mfond) X(mfire) X(toss) X(ergo) X(endo) \
  X(hemp) X(laze) X(mine) X(mile) X(mint) X(mull)

#define ZONEENUM(z) zone_##z,
enum { ZONES(ZONEENUM) nzones };
#define ZONENAME(z) #z,
char *zonenames[] = { ZONES(ZONENAME) };

typedef struct {
  u64 calls;
  u64 self;
  u64 incl;   // counted at the outermost of recursive calls
  i32 depth;
} zonestat;

zonestat zonestats[nzones];

typedef struct {
  i32 zone;
  u64 start;
  u64 child;  // time in the zones it called
} profframe;

profframe profstack[1 << 20];
i32       profsp;

typedef i32 profscope;

static inline profscope profenter(i32 z) {
  profframe *f = &profstack[profsp++];
  f->zone = z;
  f->child = 0;
  zonestats[z].calls++;
  zonestats[z].depth++;
  f->start = cycles();
  return z;
}

static inline void profleave(profscope *z) {
  u64 now = cycles();
  profframe *f = &profstack[--profsp];
  u64 el = now - f->start;
  zonestat *s = &zonestats[f->zone];
  s->self += el - f->child;
  if (!--s->depth) s->incl += el;
  if (profsp) profstack[profsp-1].child += el;
}

#define PROF(z) profscope prof_ __attribute__((cleanup(profleave))) = profenter(zone_##z)

void appendpad(bufout *b, size x, i32 width);

// the zones by self time, to stderr
void profreport(void) {
  u8 buf[1 << 14];
  bufout b[1] = {{buf, 0, sizeof(buf), 2, 0}};
  u64 freq = cyclefreq();
  i32 order[nzones];
  u64 total = 0;
  for (i32 i = 0; i < nzones; i++) {
    order[i] = i;
    total += zonestats[i].self;
  }
  for (i32 i = 1; i < nzones; i++) {
    i32 k = order[i], j = i;
    for (; j > 0 && zonestats[order[j-1]].self < zonestats[k].self; j--) order[j] = order[j-1];
    order[j] = k;
  }
  append(b, freq ? S("zone              calls     self ms  self%    incl ms   ns/call\n")
                 : S("zone              calls   self Mcyc  self%  incl Mcyc  cyc/call\n"));
  for (i32 i = 0; i < nzones; i++) {
    zonestat *s = &zonestats[order[i]];
    if (!s->calls) continue;
    // ticks to ns, or cycles as they are
    u64 self = freq ? s->self * 1000000000ull / freq : s->self;
    u64 incl = freq ? s->incl * 1000000000ull / freq : s->incl;
    char *nm = zonenames[order[i]];
    size n = 0;
    while (nm[n]) n++;
    append(b, (s8){(u8*)nm, n});
    for (size k = n; k < 12; k++) append(b, S(" "));
    appendpad(b, (size)s->calls, 11);
    appendpad(b, (size)(self / 1000000), 12);
    appendpad(b, total ? (size)(s->self * 100 / total) : 0, 7);
    appendpad(b, (size)(incl / 1000000), 11);
    appendpad(b, (size)(self / s->calls), 10);
    append(b, S("\n"));
  }
  flush(b);
}

void appendpad(bufout *b, size x, i32 width) {
  size digits = 1;
  for (size v = x; v >= 10; v /= 10) digits++;
  for (size k = digits; k < width; k++) append(b, S(" "));
  appendsize(b, x);
}
#else
#define PROF(z)
void profreport(void) {}
#endif

// Nouns. A noun is one of vere's, in the loom, as it is there: the
// subject a compile is given is read where it lies, and what the
// compile makes is made there, on the road the host runs it on, which
// is dropped when it's done. A handle is vere's noun with every bit
// turned, so that 0, which vere has for the atom 0, is no noun here, as
// the code all through takes it to be, and vere's u3_none is.
//
// Nouns aren't made once: two that are equal needn't be the same, and
// are compared by nouneq, which is vere's equality: by mug first, and
// unifying, so that two found equal are one from then on wherever they
// were found.
//
// Nothing is lost here: vere drops the road whole, and copies what's
// wanted out of it. A cell counts its head and tail, as vere's do, and
// every noun made keeps the count it was made with, held as it may be
// by a memo or a variable: so the one of two equal nouns that unifying
// lets go is never freed.

typedef u32 noun;

#define U3(n)  ((u3_noun)~(n))
#define HC(n)  ((noun)~(n))

enum {
  directmax = 1u << 31,       // atoms below this are direct
};

enum {
  memomin = 1 << 12,          // cells in the memo of +mint and +mull
};

typedef struct {
  u32       ncells;           // made, for the parser's budget
  u32       natoms;
  arena     perm;             // the hash tables and memos, apart from scratch
} heap;

heap H;

#define nul ((noun)~(u32)0)

typedef struct {
  noun *data;
  size  len;
  size  cap;
} nouns;

typedef struct {
  u8  *data;
  size len;
  size cap;
} bytes;

static inline b32 isatom(noun n) {
  return c3y == u3a_is_atom(U3(n));
}

static inline b32 iscell(noun n) {
  return !isatom(n);
}

static inline b32 isdirect(noun n) {
  return c3y == u3a_is_cat(U3(n));
}

static inline noun hd(noun n) { return HC(((u3a_cell*)u3a_to_ptr(U3(n)))->hed); }
static inline noun tl(noun n) { return HC(((u3a_cell*)u3a_to_ptr(U3(n)))->tel); }

i32 bytelen(u64 v) {
  i32 len = 0;
  while (len < 8 && v >> (8*len)) len++;
  return len;
}

size alen(noun n) {
  if (isdirect(n)) return bytelen(U3(n));
  return (size)u3r_met(3, U3(n));
}

// byte i of an atom, 0 past its end
u8 abyte(noun n, size i) {
  if (isdirect(n)) return i < 4 ? (u8)(U3(n) >> (8*i)) : 0;
  return u3r_byte((c3_w)i, U3(n));
}

// the bytes of an atom, into out if it's direct
u8 *abytes(noun n, u8 out[8]) {
  if (isdirect(n)) {
    for (i32 i = 0; i < 8; i++) out[i] = abyte(n, i);
    return out;
  }
  return (u8*)((u3a_atom*)u3a_to_ptr(U3(n)))->buf_w;
}

// nouns were once made loose, for parses thrown away: never here
struct {
  b32 on;
} loose;

noun cons(arena *a, noun h, noun t) {
  PROF(cons);
  H.ncells++;
  return HC(u3i_cell(u3k(U3(h)), u3k(U3(t))));
}

noun atomu64(arena *a, u64 v) {
  if (v < directmax) return HC((u3_noun)v);
  H.natoms++;
  return HC(u3i_chub(v));
}

noun atombytes(arena *a, u8 *b, size len) {
  while (len && !b[len-1]) len--;
  if (len <= 8) {
    u64 v = 0;
    for (size j = len - 1; j >= 0; j--) v = v << 8 | b[j];
    return atomu64(a, v);
  }
  H.natoms++;
  return HC(u3i_bytes((c3_w)len, b));
}

noun internatom(u8 *b, size len, u8 *keep) {
  return atombytes(0, b, len);
}

// the heap's arena is its own, and starts with nothing
void heapinit(arena *a) {
  H.perm = (arena){0};
  H.perm.solo = 1;
}

typedef struct {
  char *key;
  noun  val;
} termcell;

termcell termpool[1 << 12];

// the atom for a constant C string, shared by pointer
noun term(char *s) {
  size i = (size)(((u64)(uptr)s * 0x9e3779b97f4a7c15ull) >> 52);
  for (;;) {
    termcell *c = &termpool[i];
    if (c->key == s) return c->val;
    if (!c->key) {
      size len = 0;
      while (s[len]) len++;
      c->key = s;
      c->val = atombytes(0, (u8*)s, len);
      return c->val;
    }
    i = (i + 1) & ((1 << 12) - 1);
  }
}

noun termsites[8192];

noun termat(i32 site, char *s) {
  noun *t = &termsites[site];
  if (*t) return *t;
  return *t = term(s);
}

noun atomcstr(arena *a, char *s) {
  size len = 0;
  while (s[len]) len++;
  return atombytes(a, (u8*)s, len);
}

// the low 8 bytes of an atom
u64 atomlow(noun n) {
  if (isdirect(n)) return U3(n);
  return u3r_chub(0, U3(n));
}

b32 atomfits(noun n) {
  return isatom(n) && (isdirect(n) || u3r_met(3, U3(n)) <= 8);
}

b32 atomis(noun n, u64 v) {
  if (v < directmax) return n == HC((u3_noun)v);
  return atomfits(n) && atomlow(n) == v;
}

// compare an atom against a cord like "cnts"
b32 atomeqc(noun n, char *s) {
  if (!isatom(n)) return 0;
  if (atomfits(n)) {
    // as words, which folds when s is a literal
    u64 w = 0;
    i32 i = 0;
    for (; i < 8 && s[i]; i++) w |= (u64)(u8)s[i] << (8*i);
    if (s[i]) return 0;
    return atomlow(n) == w;
  }
  size len = alen(n), i = 0;
  for (; s[i]; i++) {
    if (i >= len || abyte(n, i) != (u8)s[i]) return 0;
  }
  return i == len;
}

static inline u32 mug(noun n) {
  return (u32)u3r_mug(U3(n));
}

// two nouns that aren't the same noun, and may be equal
b32 nouneqx(noun x, noun y) {
  if (!x || !y || isdirect(x) || isdirect(y)) return 0;
  // most that are compared aren't equal, and their mugs say so
  u32 mx = ((u3a_noun*)u3a_to_ptr(U3(x)))->mug_w, my = ((u3a_noun*)u3a_to_ptr(U3(y)))->mug_w;
  if (mx && my && mx != my) return 0;
  return c3y == u3r_sing(U3(x), U3(y));
}

// whether two nouns are equal, which nouns that are the same are
static inline b32 nouneq(noun x, noun y) {
  return x == y || nouneqx(x, y);
}

// Unsigned atom arithmetic, for literals and axes. Atoms that fit in a
// word, as axes nearly always do, go the quick way.

size wordbits(u64 v) {
  return v ? 64 - __builtin_clzll(v) : 0;
}

size atombits(noun n) {
  if (atomfits(n)) return wordbits(atomlow(n));
  if (!alen(n)) return 0;
  size bits = (alen(n) - 1) * 8;
  for (u8 top = abyte(n, alen(n)-1); top; top >>= 1) bits++;
  return bits;
}

i32 atomcmp(noun x, noun y) {
  if (atomfits(x) && atomfits(y)) {
    u64 a = atomlow(x), b = atomlow(y);
    return a < b ? -1 : a > b;
  }
  if (alen(x) != alen(y)) return alen(x) < alen(y) ? -1 : 1;
  for (size i = alen(x) - 1; i >= 0; i--) {
    if (abyte(x, i) != abyte(y, i)) return abyte(x, i) < abyte(y, i) ? -1 : 1;
  }
  return 0;
}

noun atomadd(arena *a, noun x, noun y) {
  if (atomfits(x) && atomfits(y)) {
    u64 s = atomlow(x) + atomlow(y);
    if (s >= atomlow(x)) return atomu64(a, s);
  }
  size len = MAX(alen(x), alen(y)) + 1;
  u8 *b = new(a, u8, len);
  u32 carry = 0;
  for (size i = 0; i < len; i++) {
    u32 s = carry;
    if (i < alen(x)) s += abyte(x, i);
    if (i < alen(y)) s += abyte(y, i);
    b[i] = (u8)s;
    carry = s >> 8;
  }
  return atombytes(a, b, len);
}

// x - y, assumes x >= y
noun atomsub(arena *a, noun x, noun y) {
  if (atomfits(x) && atomfits(y)) return atomu64(a, atomlow(x) - atomlow(y));
  u8 *b = new(a, u8, alen(x));
  i32 borrow = 0;
  for (size i = 0; i < alen(x); i++) {
    i32 d = (i32)abyte(x, i) - borrow - (i < alen(y) ? abyte(y, i) : 0);
    borrow = d < 0;
    b[i] = (u8)(d + (borrow ? 256 : 0));
  }
  return atombytes(a, b, alen(x));
}

noun atommulsmall(arena *a, noun x, u32 k) {
  size len = alen(x) + 5;
  u8 *b = new(a, u8, len);
  u64 carry = 0;
  for (size i = 0; i < len; i++) {
    u64 s = carry + (i < alen(x) ? (u64)abyte(x, i) * k : 0);
    b[i] = (u8)s;
    carry = s >> 8;
  }
  return atombytes(a, b, len);
}

noun atomaddsmall(arena *a, noun x, u64 k) {
  if (atomfits(x) && atomlow(x) + k >= k) return atomu64(a, atomlow(x) + k);
  return atomadd(a, x, atomu64(a, k));
}

noun atommul(arena *a, noun x, noun y) {
  size len = alen(x) + alen(y) + 1;
  u32 *acc = new(a, u32, len);
  for (size i = 0; i < alen(x); i++) {
    u32 carry = 0;
    for (size j = 0; j < alen(y); j++) {
      u32 s = acc[i+j] + (u32)abyte(x, i) * abyte(y, j) + carry;
      acc[i+j] = s & 0xff;
      carry = s >> 8;
    }
    for (size k = i + alen(y); carry; k++) {
      u32 s = acc[k] + carry;
      acc[k] = s & 0xff;
      carry = s >> 8;
    }
  }
  u8 *b = new(a, u8, len);
  for (size i = 0; i < len; i++) b[i] = (u8)acc[i];
  return atombytes(a, b, len);
}

noun atomdivsmall(arena *a, noun x, u32 k, u32 *rem) {
  u8 *b = new(a, u8, alen(x) + 1);
  u64 r = 0;
  for (size i = alen(x) - 1; i >= 0; i--) {
    r = (r << 8) | abyte(x, i);
    b[i] = (u8)(r / k);
    r %= k;
  }
  if (rem) *rem = (u32)r;
  return atombytes(a, b, alen(x));
}

noun atomlsh(arena *a, noun x, size bits) {
  if (!alen(x)) return x;
  if (atomfits(x) && wordbits(atomlow(x)) + bits <= 64) return atomu64(a, atomlow(x) << bits);
  size len = alen(x) + bits/8 + 1;
  u8 *b = new(a, u8, len);
  size sb = bits / 8;
  i32 sh = (i32)(bits % 8);
  for (size i = 0; i < alen(x); i++) {
    u32 v = (u32)abyte(x, i) << sh;
    b[i+sb]   |= (u8)v;
    b[i+sb+1] |= (u8)(v >> 8);
  }
  return atombytes(a, b, len);
}

noun atomrsh(arena *a, noun x, size bits) {
  if (atomfits(x)) return bits >= 64 ? nul : atomu64(a, atomlow(x) >> bits);
  size sb = bits / 8;
  if (sb >= alen(x)) return nul;
  i32 sh = (i32)(bits % 8);
  size len = alen(x) - sb;
  u8 *b = new(a, u8, len);
  for (size i = 0; i < len; i++) {
    u32 v = abyte(x, i+sb);
    if (i + sb + 1 < alen(x)) v |= (u32)abyte(x, i+sb+1) << 8;
    b[i] = (u8)(v >> sh);
  }
  return atombytes(a, b, len);
}

// low bits of x
noun atomend(arena *a, noun x, size bits) {
  if (atomfits(x)) return bits >= 64 ? x : atomu64(a, atomlow(x) & ((1ull << bits) - 1));
  size len = MIN(alen(x), bits/8 + 1);
  u8 *b = new(a, u8, len+1);
  u8 tmp[8];
  copy((byte*)b, (byte*)abytes(x, tmp), len);
  if (bits/8 < len) b[bits/8] &= (u8)((1u << (bits % 8)) - 1);
  return atombytes(a, b, len);
}

b32 atombit(noun x, size i) {
  if (atomfits(x)) return i < 64 && (atomlow(x) >> i & 1);
  if (i/8 >= alen(x)) return 0;
  return (abyte(x, i/8) >> (i % 8)) & 1;
}

noun atomor(arena *a, noun x, noun y) {
  if (atomfits(x) && atomfits(y)) return atomu64(a, atomlow(x) | atomlow(y));
  size len = MAX(alen(x), alen(y));
  u8 *b = new(a, u8, len);
  for (size i = 0; i < len; i++) {
    b[i] = (i < alen(x) ? abyte(x, i) : 0) | (i < alen(y) ? abyte(y, i) : 0);
  }
  return atombytes(a, b, len);
}

// x & y, or x ^ y with xor
noun atomandxor(arena *a, noun x, noun y, b32 xor) {
  if (atomfits(x) && atomfits(y)) {
    return atomu64(a, xor ? atomlow(x) ^ atomlow(y) : atomlow(x) & atomlow(y));
  }
  size len = MAX(alen(x), alen(y));
  u8 *b = new(a, u8, len);
  for (size i = 0; i < len; i++) {
    u8 c = abyte(x, i), d = abyte(y, i);
    b[i] = xor ? c ^ d : c & d;
  }
  return atombytes(a, b, len);
}

// x / y by shift and subtract, with remainder
noun atomdiv(arena *a, noun x, noun y, noun *rem) {
  size n = atombits(x);
  size qlen = alen(x) + 1;
  u8 *q = new(a, u8, qlen);
  noun r = nul;
  for (size i = n - 1; i >= 0; i--) {
    r = atomlsh(a, r, 1);
    if (atombit(x, i)) r = atomor(a, r, atomu64(a, 1));
    if (atomcmp(r, y) >= 0) {
      r = atomsub(a, r, y);
      q[i/8] |= (u8)(1u << (i % 8));
    }
  }
  if (rem) *rem = r;
  return atombytes(a, q, qlen);
}

noun atompow(arena *a, u32 base, size exp) {
  noun r = atomu64(a, 1);
  for (size i = 0; i < exp; i++) {
    r = atommulsmall(a, r, base);
  }
  return r;
}

// concatenate atoms by byte width, like (rap 3 list)
noun atomrap3(arena *a, noun *items, size n) {
  size len = 0;
  for (size i = 0; i < n; i++) len += alen(items[i]);
  u8 *b = new(a, u8, len + 1);
  size off = 0;
  for (size i = 0; i < n; i++) {
    u8 tmp[8];
    copy((byte*)b + off, (byte*)abytes(items[i], tmp), alen(items[i]));
    off += alen(items[i]);
  }
  return atombytes(a, b, len);
}

// Murmur3 based noun hashing, as +muk and +mug.

u32 rotl32(u32 x, i32 r) {
  return (x << r) | (x >> (32 - r));
}

u32 muk(u32 seed, size len, u8 *key, size keylen) {
  u32 c1 = 0xcc9e2d51;
  u32 c2 = 0x1b873593;
  u32 h1 = seed;
  size nblocks = len / 4;
  for (size i = 0; i < nblocks; i++) {
    u32 k1 = 0;
    for (i32 j = 3; j >= 0; j--) {
      size at = i*4 + j;
      k1 = (k1 << 8) | (at < keylen ? key[at] : 0);
    }
    k1 *= c1;
    k1 = rotl32(k1, 15);
    k1 *= c2;
    h1 ^= k1;
    h1 = rotl32(h1, 13);
    h1 = h1*5 + 0xe6546b64;
  }
  u32 k1 = 0;
  size tail = nblocks * 4;
  switch (len & 3) {
  case 3: k1 ^= (u32)(tail+2 < keylen ? key[tail+2] : 0) << 16;  // fallthrough
  case 2: k1 ^= (u32)(tail+1 < keylen ? key[tail+1] : 0) << 8;   // fallthrough
  case 1: k1 ^= (u32)(tail < keylen ? key[tail] : 0);
    k1 *= c1;
    k1 = rotl32(k1, 15);
    k1 *= c2;
    h1 ^= k1;
  }
  h1 ^= (u32)len;
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;
  return h1;
}

u32 mum(u32 syd, u32 fal, u8 *key, size len) {
  for (i32 i = 0; i < 8; i++) {
    u32 haz = muk(syd, len, key, len);
    u32 ham = (haz >> 31) ^ (haz & 0x7fffffff);
    if (ham) return ham;
    syd++;
  }
  return fal;
}

// muk of up to 8 bytes held in a word, little-endian: every cell and
// most atoms, without muk's byte at a time assembly
u32 muk8(u32 seed, u64 v, i32 len) {
  u32 c1 = 0xcc9e2d51;
  u32 c2 = 0x1b873593;
  u32 h1 = seed;
  i32 nblocks = len / 4;
  for (i32 i = 0; i < nblocks; i++) {
    u32 k1 = (u32)(v >> (32*i));
    k1 *= c1;
    k1 = rotl32(k1, 15);
    k1 *= c2;
    h1 ^= k1;
    h1 = rotl32(h1, 13);
    h1 = h1*5 + 0xe6546b64;
  }
  if (len & 3) {
    u32 k1 = (u32)(v >> (32*nblocks)) & ((1u << (8*(len & 3))) - 1);
    k1 *= c1;
    k1 = rotl32(k1, 15);
    k1 *= c2;
    h1 ^= k1;
  }
  h1 ^= (u32)len;
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;
  return h1;
}

u32 mum8(u32 syd, u32 fal, u64 v, i32 len) {
  for (i32 i = 0; i < 8; i++) {
    u32 haz = muk8(syd, v, len);
    u32 ham = (haz >> 31) ^ (haz & 0x7fffffff);
    if (ham) return ham;
    syd++;
  }
  return fal;
}

u32 mugu32(u32 v) {
  return mum8(0xcafebabe, 0x7fff, v, bytelen(v));
}

// noun orders +dor, +gor, +mor

b32 dor(noun x, noun y) {
  for (;;) {
    if (nouneq(x, y)) return 1;
    if (iscell(x)) {
      if (isatom(y)) return 0;
      if (nouneq(hd(x), hd(y))) {
        x = tl(x);
        y = tl(y);
      } else {
        x = hd(x);
        y = hd(y);
      }
      continue;
    }
    if (iscell(y)) return 1;
    return atomcmp(x, y) < 0;
  }
}

b32 gor(noun x, noun y) {
  u32 c = mug(x);
  u32 d = mug(y);
  if (c == d) return dor(x, y);
  return c < d;
}

// alphabetical order, as +aor: cells by their parts, atoms byte by byte
// from the low end
b32 aor(noun x, noun y) {
  for (;;) {
    if (nouneq(x, y)) return 1;
    if (iscell(x)) {
      if (isatom(y)) return 0;
      if (nouneq(hd(x), hd(y))) {
        x = tl(x);
        y = tl(y);
      } else {
        x = hd(x);
        y = hd(y);
      }
      continue;
    }
    if (iscell(y)) return 1;
    for (size i = 0; ; i++) {
      u8 c = abyte(x, i), d = abyte(y, i);
      if (c != d) return c < d;
      if (i >= alen(x) && i >= alen(y)) return 0;
    }
  }
}

b32 mor(noun x, noun y) {
  u32 c = mugu32(mug(x));
  u32 d = mugu32(mug(y));
  if (c == d) return dor(x, y);
  return c < d;
}

// Maps, as treaps of [n=[key val] l r] ordered like +by.

noun mapnode(arena *a, noun n, noun l, noun r) {
  return cons(a, n, cons(a, l, r));
}

#define mapn(m) hd(m)
#define mapl(m) hd(tl(m))
#define mapr(m) tl(tl(m))

noun mapput(arena *a, noun m, noun key, noun val) {
  if (isatom(m)) {
    return mapnode(a, cons(a, key, val), nul, nul);
  }
  if (nouneq(key, hd(mapn(m)))) {
    if (nouneq(val, tl(mapn(m)))) return m;
    return mapnode(a, cons(a, key, val), mapl(m), mapr(m));
  }
  if (gor(key, hd(mapn(m)))) {
    noun d = mapput(a, mapl(m), key, val);
    if (mor(hd(mapn(m)), hd(mapn(d)))) {
      return mapnode(a, mapn(m), d, mapr(m));
    }
    return mapnode(a, mapn(d), mapl(d), mapnode(a, mapn(m), mapr(d), mapr(m)));
  }
  noun d = mapput(a, mapr(m), key, val);
  if (mor(hd(mapn(m)), hd(mapn(d)))) {
    return mapnode(a, mapn(m), mapl(m), d);
  }
  return mapnode(a, mapn(d), mapnode(a, mapn(m), mapl(m), mapl(d)), mapr(d));
}

b32 maphas(noun m, noun key) {
  while (iscell(m)) {
    if (nouneq(key, hd(mapn(m)))) return 1;
    m = gor(key, hd(mapn(m))) ? mapl(m) : mapr(m);
  }
  return 0;
}

noun mapuni(arena *a, noun x, noun y) {
  if (isatom(y)) return x;
  if (isatom(x)) return y;
  noun nx = mapn(x);
  noun ny = mapn(y);
  if (nouneq(hd(ny), hd(nx))) {
    return mapnode(a, ny, mapuni(a, mapl(x), mapl(y)), mapuni(a, mapr(x), mapr(y)));
  }
  if (mor(hd(nx), hd(ny))) {
    if (gor(hd(ny), hd(nx))) {
      noun l = mapuni(a, mapl(x), mapnode(a, ny, mapl(y), nul));
      return mapuni(a, mapnode(a, nx, l, mapr(x)), mapr(y));
    }
    noun r = mapuni(a, mapr(x), mapnode(a, ny, nul, mapr(y)));
    return mapuni(a, mapnode(a, nx, mapl(x), r), mapl(y));
  }
  if (gor(hd(nx), hd(ny))) {
    noun l = mapuni(a, mapnode(a, nx, mapl(x), nul), mapl(y));
    return mapuni(a, mapr(x), mapnode(a, ny, l, mapr(y)));
  }
  noun r = mapuni(a, mapnode(a, nx, nul, mapr(x)), mapr(y));
  return mapuni(a, mapl(x), mapnode(a, ny, mapl(y), r));
}

// Parser state. Rules take the parser and a position; on success they
// return a noun and advance the position, on failure they return 0 and
// may leave the position anywhere, so callers restore it as needed.
//
// far is the furthest position reached, mirroring the hair that hoon's
// combinators carry in p.edge; it is reported as the error location.

typedef struct parser parser;
struct parser {
  arena    *a;
  u8       *buf;
  size      len;
  size      line;     // line number of buf[0]
  size      col;      // column number of buf[0]
  size     *nls;      // offsets of newlines, built lazily
  size      nnls;
  b32       nlsdone;
  size      far;      // furthest position reached in buf
  u64       farh;     // furthest hair reached in a nested parse
  b32       bug;      // wrap results in %dbug
  b32       allbug;   // and under !. too, for the language server
  noun      wer;      // the path in %dbug spots, as +rain gives it; 0 for ~
  i32       file;     // the file in srcfiles, for messages; 0 if none
  struct spans *toks; // what the parser recognized, for highlighting, or 0
  u32       base;     // global position of the file being parsed
  parser   *root;     // for a subparser, the parser of the file
  nouns     stk;      // items of lists being built, as a stack
  size      budget;   // rule entries left before giving up, see spend
  i32       depth;    // rules entered and not yet left, see within
  b32       deep;     // gave up for nesting, not backtracking
  u32       cells;    // the heap when the parse began, see spend
  u32       atoms;
};

typedef struct {
  size line;
  size col;
} hair;

#define C2(x, y)          cons(p->a, (x), (y))
#define C3(x, y, z)       C2((x), C2((y), (z)))
#define C4(w, x, y, z)    C2((w), C3((x), (y), (z)))
#define C5(v, w, x, y, z) C2((v), C4((w), (x), (y), (z)))
// terms from literals, each use remembering its own
#define K(s)              termat(__COUNTER__, s)
#define D(v)              atomu64(p->a, v)
#define YES               nul
#define NO                D(1)


noun mklist(parser *p, noun *items, size n) {
  noun r = nul;
  for (size i = n - 1; i >= 0; i--) {
    r = C2(items[i], r);
  }
  return r;
}

noun nounslist(parser *p, nouns *xs) {
  return mklist(p, xs->data, xs->len);
}

noun weld(parser *p, noun x, noun y) {
  nouns xs = {0};
  for (; iscell(x); x = tl(x)) *push(&xs, p->a) = hd(x);
  noun r = y;
  for (size i = xs.len - 1; i >= 0; i--) r = C2(xs.data[i], r);
  return r;
}

noun flop(parser *p, noun x) {
  noun r = nul;
  for (; iscell(x); x = tl(x)) r = C2(hd(x), r);
  return r;
}

size lent(noun x) {
  size n = 0;
  for (; iscell(x); x = tl(x)) n++;
  return n;
}

// a tape from a C string
noun tape(parser *p, char *s) {
  size len = 0;
  while (s[len]) len++;
  noun r = nul;
  for (size i = len - 1; i >= 0; i--) r = C2(D((u8)s[i]), r);
  return r;
}

noun tapeatom(parser *p, noun a) {
  noun r = nul;
  for (size i = alen(a) - 1; i >= 0; i--) r = C2(D(abyte(a, i)), r);
  return r;
}

void buildnls(parser *p) {
  if (p->nlsdone) return;
  size n = 0;
  for (size i = 0; i < p->len; i++) n += p->buf[i] == '\n';
  p->nls = new(p->a, size, n + 1);
  for (size i = 0; i < p->len; i++) {
    if (p->buf[i] == '\n') p->nls[p->nnls++] = i;
  }
  p->nlsdone = 1;
}

// line and column of a position, as +lust would count them
hair hairat(parser *p, size pos) {
  buildnls(p);
  size lo = 0, hi = p->nnls;
  while (lo < hi) {
    size mid = (lo + hi) / 2;
    if (p->nls[mid] < pos) lo = mid + 1; else hi = mid;
  }
  hair h;
  h.line = p->line + lo;
  h.col = lo ? pos - p->nls[lo-1] : pos + p->col;
  return h;
}

u64 hairkey(hair h) {
  return ((u64)h.line << 32) | (u64)h.col;
}

hair errhair(parser *p) {
  u64 k = hairkey(hairat(p, p->far));
  if (p->farh > k) k = p->farh;
  hair h = {(size)(k >> 32), (size)(k & 0xffffffff)};
  return h;
}

void reach(parser *p, size pos) {
  if (pos > p->far) p->far = pos;
}

typedef struct {
  size far;
  u64  farh;
} farmark;

farmark farget(parser *p) {
  farmark m = {p->far, p->farh};
  return m;
}

void farset(parser *p, farmark m) {
  p->far = m.far;
  p->farh = m.farh;
}

// Character primitives. These never move the position on failure.

b32 chr(parser *p, size *pos, u8 c) {
  if (*pos < p->len && p->buf[*pos] == c) {
    reach(p, ++*pos);
    return 1;
  }
  reach(p, *pos);
  return 0;
}

b32 range(parser *p, size *pos, u8 lo, u8 hi, u8 *out) {
  if (*pos < p->len && p->buf[*pos] >= lo && p->buf[*pos] <= hi) {
    if (out) *out = p->buf[*pos];
    reach(p, ++*pos);
    return 1;
  }
  reach(p, *pos);
  return 0;
}

b32 jest(parser *p, size *pos, char *s) {
  for (; *s; s++) {
    if (!chr(p, pos, (u8)*s)) return 0;
  }
  return 1;
}

b32 peek(parser *p, size pos, u8 c) {
  return pos < p->len && p->buf[pos] == c;
}

// the lookahead of +less: if it matches, its progress is forgotten
b32 lookahead(parser *p, size pos, b32 matched, farmark m) {
  if (matched) {
    farset(p, m);
    reach(p, pos);
  }
  return matched;
}

b32 prn(parser *p, size *pos, u8 *out) {
  if (*pos < p->len && p->buf[*pos] >= 32 && p->buf[*pos] != 127) {
    if (out) *out = p->buf[*pos];
    reach(p, ++*pos);
    return 1;
  }
  reach(p, *pos);
  return 0;
}

b32 low(parser *p, size *pos, u8 *out) { return range(p, pos, 'a', 'z', out); }
b32 hig(parser *p, size *pos, u8 *out) { return range(p, pos, 'A', 'Z', out); }
b32 nud(parser *p, size *pos, u8 *out) { return range(p, pos, '0', '9', out); }

typedef noun (*rule)(parser *, size *, b32);

// a parser over a whole buffer
parser newparser(arena *a, s8 src) {
  parser p = {0};
  p.a = a;
  p.buf = src.buf;
  p.len = src.len;
  p.line = 1;
  p.col = 1;
  p.budget = 64 * src.len + (1 << 16);
  p.cells = H.ncells;
  p.atoms = H.natoms;
  return p;
}

// a parser over a separate buffer, sharing the arena
parser subparser(parser *p, u8 *buf, size len, size line) {
  parser q = {0};
  q.a = p->a;
  q.buf = buf;
  q.len = len;
  q.line = line;
  q.col = 1;
  q.bug = p->bug;
  q.allbug = p->allbug;
  q.toks = p->toks;
  q.root = p->root ? p->root : p;
  return q;
}

// Backtracking takes exponential time on some inputs, like ?=((=( over
// and over, where each level parses what's inside it again for every
// alternative, or [[[ in a cram block. Real code enters a rule about once
// per ten bytes and cram text a word per byte, so a parse, with any
// subparsers, gives up after 64 per byte. Some rules build nouns as long
// as the rest of the line, so it gives up too after 8 cells or 2 atoms
// per byte, where real code takes at most 1.5 and 0.3.
b32 spend(parser *p) {
  parser *r = p->root ? p->root : p;
  if ((size)(H.ncells - r->cells) > 8 * r->len + (1 << 20)
      || (size)(H.natoms - r->atoms) > 2 * r->len + (1 << 18)) {
    r->budget = 0;
  }
  if (r->budget <= 0) return 0;
  r->budget--;
  return 1;
}

// Rules recurse as deep as the source nests, on the C stack, which a
// few hundred KB of [[[ would overflow: the parser gives up past 2000
// nested rules, where real code goes to 100 and an 8MB stack to 13000.
enum { maxdepth = 2000 };

// into a rule that counts toward the budget and the nesting, or 0 to give
// up; leave when it returns
b32 enter(parser *p) {
  parser *r = p->root ? p->root : p;
  if (r->depth >= maxdepth) {
    r->budget = 0;
    r->deep = 1;
  }
  if (!spend(p)) return 0;
  r->depth++;
  return 1;
}

void leave(parser *p) {
  (p->root ? p->root : p)->depth--;
}

noun within(parser *p, size *pos, b32 tol, rule f) {
  if (!enter(p)) return 0;
  noun v = f(p, pos, tol);
  leave(p);
  return v;
}

// whether a parse gave up, so that whatever it returned is wrong
b32 spent(parser *p) {
  return (p->root ? p->root : p)->budget <= 0;
}

// Highlighting. With toks set, rules note what they recognize in it;
// alternatives that fail may note things too, but they read the same
// text the same way, so the notes can be painted over each other.

enum {
  tok_comment, tok_string, tok_number, tok_keyword, tok_function,
  tok_variable, tok_type, tok_term,
};

typedef struct {
  u32 start;
  u32 end;
  u8  type;
} span;

typedef struct spans {
  span *data;
  size  len;
  size  cap;
} spans;

void note(parser *p, size start, size end, i32 type) {
  if (!p->toks || end <= start) return;
  span *t = push(p->toks, p->a);
  t->start = (u32)start;
  t->end = (u32)end;
  t->type = (u8)type;
}

// Whitespace

// Whitespace is scanned directly rather than through chr and prn, but
// reaches the same positions, so errors are reported in the same place.

// a comment, :: and printable characters to the end of the line
b32 vul(parser *p, size *pos) {
  u8 *b = p->buf;
  size s = *pos, i = s, n = p->len;
  if (i >= n || b[i] != ':' || ++i >= n || b[i] != ':') {
    reach(p, i);
    return 0;
  }
  i++;
  while (i < n && b[i] >= 32 && b[i] != 127) i++;
  note(p, s, i, tok_comment);
  if (i < n && b[i] == '\n') i++;
  else if (i < n) {
    reach(p, i);
    return 0;
  }
  reach(p, i);
  *pos = i;
  return 1;
}

// a newline, two spaces, or a comment after at most a space
b32 gaq(parser *p, size *pos) {
  u8 *b = p->buf;
  size i = *pos, n = p->len;
  if (i < n && b[i] == '\n') {
    reach(p, *pos = i + 1);
    return 1;
  }
  if (i < n && b[i] == ' ') {
    if (i + 1 < n && (b[i+1] == ' ' || b[i+1] == '\n')) {
      reach(p, *pos = i + 2);
      return 1;
    }
    reach(p, i + 1);
    size t = i + 1;
    if (vul(p, &t)) {
      *pos = t;
      return 1;
    }
    return 0;
  }
  return vul(p, pos);
}

// gaq, then any spaces, newlines and comments
b32 gap(parser *p, size *pos) {
  if (!gaq(p, pos)) return 0;
  u8 *b = p->buf;
  size i = *pos, n = p->len;
  for (;;) {
    while (i < n && (b[i] == ' ' || b[i] == '\n')) i++;
    size t = i;
    if (i + 1 < n && b[i] == ':' && b[i+1] == ':' && vul(p, &t)) {
      i = t;
      continue;
    }
    break;
  }
  reach(p, i);
  if (i < n && b[i] == ':') reach(p, i + 1);
  *pos = i;
  return 1;
}

b32 gay(parser *p, size *pos) {
  size s = *pos;
  if (!gap(p, pos)) *pos = s;
  return 1;
}

b32 ace(parser *p, size *pos) { return chr(p, pos, ' '); }

b32 duz(parser *p, size *pos) {
  size s = *pos;
  if (!jest(p, pos, "==")) return 0;
  note(p, s, *pos, tok_keyword);
  return 1;
}

b32 dun(parser *p, size *pos) {
  size s = *pos;
  if (!jest(p, pos, "--")) return 0;
  note(p, s, *pos, tok_keyword);
  return 1;
}

// . followed by optional whitespace, the separator in long numbers
b32 dog(parser *p, size *pos) { return chr(p, pos, '.') && gay(p, pos); }
b32 dof(parser *p, size *pos) { return chr(p, pos, '-') && gay(p, pos); }
b32 doh(parser *p, size *pos) { return jest(p, pos, "--") && gay(p, pos); }

noun sym(parser *p, size *pos) {
  u8 *b = p->buf;
  size s = *pos, i = s, n = p->len;
  if (i >= n || b[i] < 'a' || b[i] > 'z') {
    reach(p, i);
    return 0;
  }
  for (i++; i < n && ((b[i] >= 'a' && b[i] <= 'z') || (b[i] >= '0' && b[i] <= '9') || b[i] == '-'); i++) {}
  reach(p, i);
  *pos = i;
  return atombytes(p->a, b + s, i - s);
}

noun mixedsym(parser *p, size *pos) {
  size s = *pos;
  if (!low(p, pos, 0) && !hig(p, pos, 0)) return 0;
  while (low(p, pos, 0) || hig(p, pos, 0) || nud(p, pos, 0) || chr(p, pos, '-')) {}
  return atombytes(p->a, p->buf + s, *pos - s);
}

noun mota(parser *p, size *pos) {
  size s = *pos;
  while (low(p, pos, 0)) {}
  while (hig(p, pos, 0)) {}
  return atombytes(p->a, p->buf + s, *pos - s);
}

// sym or $ as %$
noun symbuc(parser *p, size *pos) {
  if (chr(p, pos, '$')) return nul;
  return sym(p, pos);
}

i32 hexval(u8 c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// hex digit, either case
b32 hit(parser *p, size *pos, i32 *out) {
  u8 c;
  if (nud(p, pos, &c) || range(p, pos, 'a', 'f', &c) || range(p, pos, 'A', 'F', &c)) {
    *out = hexval(c);
    return 1;
  }
  return 0;
}

// lowercase hex digit
b32 six(parser *p, size *pos, i32 *out) {
  u8 c;
  if (nud(p, pos, &c) || range(p, pos, 'a', 'f', &c)) {
    *out = hexval(c);
    return 1;
  }
  return 0;
}

// two lowercase hex digits as a byte
b32 bix(parser *p, size *pos, i32 *out) {
  i32 a, b;
  if (!six(p, pos, &a) || !six(p, pos, &b)) return 0;
  *out = a*16 + b;
  return 1;
}

// Indented blocks, as +inde: the text between ope and end with the
// block's indentation removed, parsed in full by sef.

typedef b32 (*prim)(parser *, size *);
typedef noun (*body)(parser *, size *);

b32 indeand(parser *p, size *pos, size lev, prim end) {
  if (!chr(p, pos, '\n')) return 0;
  for (size i = 0; i < lev; i++) {
    if (!chr(p, pos, ' ')) return 0;
  }
  return end(p, pos);
}

b32 indeled(parser *p, size *pos, size lev) {
  size s = *pos;
  if (chr(p, pos, '\n')) {
    size i = 0;
    for (; i < lev; i++) {
      if (!chr(p, pos, ' ')) break;
    }
    if (i == lev) return 1;
  }
  *pos = s;
  if (chr(p, pos, '\n') && chr(p, pos, '\n')) {
    *pos = s + 1;
    return 1;
  }
  return 0;
}

noun inde(parser *p, size *pos, prim ope, prim end, body sef) {
  farmark m = farget(p);
  size start = *pos;
  if (!ope(p, pos)) return 0;
  hair har = hairat(p, start);
  size lev = har.col - 1;
  if (!indeled(p, pos, lev)) return 0;

  bytes text = {0};
  for (;;) {
    size s = *pos;
    farmark f = farget(p);
    if (lookahead(p, s, indeand(p, pos, lev, end), f)) {
      *pos = s;
      break;
    }
    *pos = s;
    u8 c;
    if (prn(p, pos, &c)) {
      *push(&text, p->a) = c;
      continue;
    }
    *pos = s;
    if (indeled(p, pos, lev)) {
      *push(&text, p->a) = '\n';
      continue;
    }
    *pos = s;
    break;
  }
  size len = text.len;

  parser q = subparser(p, text.data, len, har.line);
  size qpos = 0;
  noun res = sef(&q, &qpos);
  if (res && qpos != len) {
    reach(&q, qpos);
    res = 0;
  }
  farset(p, m);
  if (!res) {
    hair fur = errhair(&q);
    fur.col += har.col - 1;
    p->farh = MAX(p->farh, hairkey(fur));
    return 0;
  }
  if (!indeand(p, pos, lev, end)) return 0;
  return res;
}

// Cords: 'text' with escapes, or an indented ''' block.

b32 soz(parser *p, size *pos) { return jest(p, pos, "'''"); }

b32 mes(parser *p, size *pos, i32 *out) {
  i32 a, b;
  if (!hit(p, pos, &a) || !hit(p, pos, &b)) return 0;
  *out = a*16 + b;
  return 1;
}

// \ followed by optional whitespace and /, joining long literals
void gon(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '\\') && gay(p, pos) && chr(p, pos, '/')) return;
  *pos = s;
}

b32 qit(parser *p, size *pos, u8 *out) {
  size s = *pos;
  farmark m = farget(p);
  if (!lookahead(p, s, chr(p, pos, '\\'), m)) {
    *pos = s;
    farmark n = farget(p);
    if (!lookahead(p, s, chr(p, pos, '\''), n)) {
      *pos = s;
      if (prn(p, pos, out)) return 1;
    }
  }
  *pos = s;
  if (!chr(p, pos, '\\')) return 0;
  if (chr(p, pos, '\\')) { *out = '\\'; return 1; }
  if (chr(p, pos, '\'')) { *out = '\''; return 1; }
  i32 v;
  if (mes(p, pos, &v)) { *out = (u8)v; return 1; }
  return 0;
}

b32 qutope(parser *p, size *pos) {
  if (!soz(p, pos)) return 0;
  size s = *pos;
  if (ace(p, pos)) {
    while (ace(p, pos)) {}
    if (vul(p, pos)) return 1;
  }
  *pos = s;
  return 1;
}

noun qutbody(parser *p, size *pos) {
  nouns bytes = {0};
  for (;;) {
    size s = *pos;
    u8 c;
    if (prn(p, pos, &c)) {
      *push(&bytes, p->a) = D(c);
      continue;
    }
    *pos = s;
    farmark m = farget(p);
    if (!lookahead(p, s, chr(p, pos, '\n') && soz(p, pos), m)) {
      *pos = s;
      if (chr(p, pos, '\n')) {
        *push(&bytes, p->a) = D('\n');
        continue;
      }
    }
    *pos = s;
    break;
  }
  return atomrap3(p->a, bytes.data, bytes.len);
}

noun qut(parser *p, size *pos) {
  size s = *pos;
  if (!peek(p, s, '\'')) {
    reach(p, s);
    return 0;
  }
  farmark m = farget(p);
  if (!lookahead(p, s, soz(p, pos), m)) {
    *pos = s;
    chr(p, pos, '\'');
    bytes b = {0};
    u8 c;
    size t = *pos;
    if (qit(p, &t, &c)) {
      *push(&b, p->a) = c;
      *pos = t;
      for (;;) {
        t = *pos;
        gon(p, &t);
        if (!qit(p, &t, &c)) break;
        *push(&b, p->a) = c;
        *pos = t;
      }
    }
    if (chr(p, pos, '\'')) {
      note(p, s, *pos, tok_string);
      return atombytes(p->a, b.data, b.len);
    }
  }
  *pos = s;
  noun r = inde(p, pos, qutope, soz, qutbody);
  if (r) note(p, s, *pos, tok_string);
  return r;
}

// Atom literals, following +so, +ag and +ab. Literals come back as
// dimes [aura value] or coins [%$ dime], [%blob noun], [%many list].

enum {
  dig_sid,  // 0-9
  dig_sed,  // 1-9
  dig_sib,  // 0-1
  dig_seb,  // 1
  dig_six,  // 0-9 a-f
  dig_sex,  // 1-9 a-f
  dig_hit,  // 0-9 a-f A-F
  dig_siv,  // 0-9 a-v
  dig_sev,  // 1-9 a-v
  dig_siw,  // 0-9 a-z A-Z - ~
  dig_sew,  // 1-9 a-z A-Z - ~
};

b32 digit(parser *p, size *pos, i32 cls, u8 *out) {
  u8 c;
  if (*pos >= p->len) {
    reach(p, *pos);
    return 0;
  }
  c = p->buf[*pos];
  i32 v = -1;
  b32 zero = cls == dig_sid || cls == dig_sib || cls == dig_six
          || cls == dig_hit || cls == dig_siv || cls == dig_siw;
  if (c >= '0' && c <= '9') {
    v = c - '0';
    if (v == 0 && !zero) v = -1;
    if ((cls == dig_sib || cls == dig_seb) && v > 1) v = -1;
    if (cls == dig_seb && v == 0) v = -1;
  } else if (c >= 'a' && c <= 'z') {
    if ((cls == dig_six || cls == dig_sex || cls == dig_hit) && c <= 'f') v = c - 87;
    if ((cls == dig_siv || cls == dig_sev) && c <= 'v') v = c - 87;
    if (cls == dig_siw || cls == dig_sew) v = c - 87;
  } else if (c >= 'A' && c <= 'Z') {
    if (cls == dig_hit && c <= 'F') v = c - 55;
    if (cls == dig_siw || cls == dig_sew) v = c - 29;
  } else if (c == '-' && (cls == dig_siw || cls == dig_sew)) {
    v = 62;
  } else if (c == '~' && (cls == dig_siw || cls == dig_sew)) {
    v = 63;
  }
  if (v < 0) {
    reach(p, *pos);
    return 0;
  }
  *out = (u8)v;
  reach(p, ++*pos);
  return 1;
}

typedef struct {
  u8  *data;
  size len;
  size cap;
} digits;

noun bass(parser *p, u32 base, u8 *d, size n) {
  // a power of two: the bits of each digit in place
  i32 k = 0;
  while ((1u << k) < base) k++;
  if ((1u << k) == base) {
    u8 *b = new(p->a, u8, (n * k + 7) / 8 + 1);
    size bit = 0;
    for (size i = n - 1; i >= 0; i--, bit += k) {
      for (i32 j = 0; j < k; j++) {
        if (d[i] >> j & 1) b[(bit + j) / 8] |= (u8)(1u << ((bit + j) % 8));
      }
    }
    return atombytes(p->a, b, (n * k + 7) / 8);
  }
  // otherwise in place, a word at a time, as many digits as keep base^c
  // in 32 bits: a new atom per step would take memory n^2 in the digits
  u32 *w = new(p->a, u32, n * k / 32 + 2);
  size len = 0;
  for (size i = 0; i < n;) {
    u32 v = 0, m = 1;
    for (; i < n && (u64)m * base <= 0xffffffffu; i++) {
      v = v * base + d[i];
      m *= base;
    }
    u64 carry = v;
    for (size j = 0; j < len; j++) {
      u64 s = (u64)w[j] * m + carry;
      w[j] = (u32)s;
      carry = s >> 32;
    }
    if (carry) w[len++] = (u32)carry;
  }
  u8 *b = new(p->a, u8, len * 4 + 1);
  for (size j = 0; j < len * 4; j++) b[j] = (u8)(w[j / 4] >> 8 * (j % 4));
  return atombytes(p->a, b, len * 4);
}

// between min and max digits of a class, appended to ds
b32 stun(parser *p, size *pos, i32 cls, i32 min, i32 max, digits *ds) {
  for (i32 i = 0; i < max; i++) {
    u8 v;
    if (!digit(p, pos, cls, &v)) return i >= min;
    *push(ds, p->a) = v;
  }
  return 1;
}

// a leading group, then dot-separated groups of exactly cnt digits
noun grouped(parser *p, size *pos, i32 lead, i32 cont, i32 maxcont,
              i32 grp, i32 cnt, u32 base) {
  digits ds = {0};
  u8 v;
  if (!digit(p, pos, lead, &v)) return 0;
  *push(&ds, p->a) = v;
  stun(p, pos, cont, 0, maxcont, &ds);
  for (;;) {
    size s = *pos;
    size n = ds.len;
    if (dog(p, pos) && stun(p, pos, grp, cnt, cnt, &ds)) continue;
    *pos = s;
    ds.len = n;
    break;
  }
  return bass(p, base, ds.data, ds.len);
}

noun dem(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) return nul;
  *pos = s;
  return grouped(p, pos, dig_sed, dig_sid, 2, dig_sid, 3, 10);
}

noun dip(parser *p, size *pos) {
  digits ds = {0};
  u8 v;
  if (!digit(p, pos, dig_sed, &v)) return 0;
  *push(&ds, p->a) = v;
  while (digit(p, pos, dig_sid, &v)) *push(&ds, p->a) = v;
  return bass(p, 10, ds.data, ds.len);
}

noun dim(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) return nul;
  *pos = s;
  return dip(p, pos);
}

noun dum(parser *p, size *pos) {
  digits ds = {0};
  u8 v;
  if (!digit(p, pos, dig_sid, &v)) return 0;
  *push(&ds, p->a) = v;
  while (digit(p, pos, dig_sid, &v)) *push(&ds, p->a) = v;
  return bass(p, 10, ds.data, ds.len);
}

noun dub(parser *p, size *pos) {
  u8 v;
  if (!chr(p, pos, '0') || !digit(p, pos, dig_sed, &v)) return 0;
  return D(v);
}

noun dap(parser *p, size *pos) {
  size s = *pos;
  noun r = dub(p, pos);
  if (r) return r;
  *pos = s;
  return dip(p, pos);
}

noun mot(parser *p, size *pos) {
  size s = *pos;
  u8 c;
  if (chr(p, pos, '1') && range(p, pos, '0', '2', &c)) return D(10 + c - '0');
  *pos = s;
  noun r = dub(p, pos);
  if (r) return r;
  *pos = s;
  if (digit(p, pos, dig_sed, &c)) return D(c);
  return 0;
}

noun bay(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) return nul;
  *pos = s;
  return grouped(p, pos, dig_seb, dig_sib, 3, dig_sib, 4, 2);
}

noun hex(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) return nul;
  *pos = s;
  return grouped(p, pos, dig_sex, dig_hit, 3, dig_six, 4, 16);
}

noun viz(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) return nul;
  *pos = s;
  return grouped(p, pos, dig_sev, dig_siv, 4, dig_siv, 5, 32);
}

noun wiz(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) return nul;
  *pos = s;
  return grouped(p, pos, dig_sew, dig_siw, 4, dig_siw, 5, 64);
}

// four lowercase hex digits
noun qix(parser *p, size *pos) {
  digits ds = {0};
  if (!stun(p, pos, dig_six, 4, 4, &ds)) return 0;
  return bass(p, 16, ds.data, ds.len);
}

// 1-3 decimal digits without leading zero, or a lone zero
noun tod(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) return nul;
  *pos = s;
  digits ds = {0};
  if (!stun(p, pos, dig_sed, 1, 1, &ds)) return 0;
  stun(p, pos, dig_sid, 0, 2, &ds);
  return bass(p, 10, ds.data, ds.len);
}

// 1-4 hex digits without leading zero, or a lone zero
noun qexz(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) return nul;
  *pos = s;
  digits ds = {0};
  if (!stun(p, pos, dig_sex, 1, 1, &ds)) return 0;
  stun(p, pos, dig_hit, 0, 3, &ds);
  return bass(p, 16, ds.data, ds.len);
}

// n groups separated by dots, combined in the given base
noun groups(parser *p, size *pos, noun (*grp)(parser *, size *), i32 n, u32 base) {
  noun r = grp(p, pos);
  if (!r) return 0;
  for (i32 i = 1; i < n; i++) {
    if (!dog(p, pos)) return 0;
    noun g = grp(p, pos);
    if (!g) return 0;
    r = atomadd(p->a, atommulsmall(p->a, r, base), g);
  }
  return r;
}

noun lip(parser *p, size *pos) { return groups(p, pos, tod, 4, 256); }
noun bip(parser *p, size *pos) { return groups(p, pos, qexz, 8, 0x10000); }

// Base58check @uc

typedef struct {
  u32 h[8];
  u8  buf[64];
  u64 len;
} sha256;

u32 sha256k[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

u32 rotr32(u32 x, i32 r) {
  return (x >> r) | (x << (32 - r));
}

void sha256block(sha256 *s, u8 *b) {
  u32 w[64];
  for (i32 i = 0; i < 16; i++) {
    w[i] = (u32)b[4*i] << 24 | (u32)b[4*i+1] << 16 | (u32)b[4*i+2] << 8 | b[4*i+3];
  }
  for (i32 i = 16; i < 64; i++) {
    u32 s0 = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
    u32 s1 = rotr32(w[i-2], 17) ^ rotr32(w[i-2], 19) ^ (w[i-2] >> 10);
    w[i] = w[i-16] + s0 + w[i-7] + s1;
  }
  u32 v[8];
  for (i32 i = 0; i < 8; i++) v[i] = s->h[i];
  for (i32 i = 0; i < 64; i++) {
    u32 s1 = rotr32(v[4], 6) ^ rotr32(v[4], 11) ^ rotr32(v[4], 25);
    u32 ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
    u32 t1 = v[7] + s1 + ch + sha256k[i] + w[i];
    u32 s0 = rotr32(v[0], 2) ^ rotr32(v[0], 13) ^ rotr32(v[0], 22);
    u32 mj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
    u32 t2 = s0 + mj;
    v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
    v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + t2;
  }
  for (i32 i = 0; i < 8; i++) s->h[i] += v[i];
}

void sha256hash(u8 *msg, size len, u8 out[32]) {
  sha256 s = {{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
               0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}, {0}, 0};
  size i = 0;
  for (; i + 64 <= len; i += 64) sha256block(&s, msg + i);
  u8 last[128] = {0};
  size rest = len - i;
  copy((byte*)last, (byte*)msg + i, rest);
  last[rest] = 0x80;
  size total = rest + 9 <= 64 ? 64 : 128;
  u64 bits = (u64)len * 8;
  for (i32 j = 0; j < 8; j++) last[total-1-j] = (u8)(bits >> (8*j));
  sha256block(&s, last);
  if (total == 128) sha256block(&s, last + 64);
  for (i32 j = 0; j < 8; j++) {
    out[4*j]   = (u8)(s.h[j] >> 24);
    out[4*j+1] = (u8)(s.h[j] >> 16);
    out[4*j+2] = (u8)(s.h[j] >> 8);
    out[4*j+3] = (u8)s.h[j];
  }
}

// the 4-byte checksum of +tok:fa
u32 base58tok(parser *p, noun a) {
  size pad = alen(a) >= 21 ? 0 : 21 - alen(a);
  size len = pad + alen(a);
  u8 *msg = new(p->a, u8, len + 1);
  for (size i = 0; i < alen(a); i++) msg[pad + i] = abyte(a, alen(a) - 1 - i);
  u8 h1[32], h2[32];
  sha256hash(msg, len, h1);
  sha256hash(h1, 32, h2);
  return (u32)h2[0] << 24 | (u32)h2[1] << 16 | (u32)h2[2] << 8 | h2[3];
}

noun fim(parser *p, size *pos) {
  char *key = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
  digits ds = {0};
  for (;;) {
    size s = *pos;
    u8 c;
    if (!low(p, pos, &c) && !hig(p, pos, &c) && !nud(p, pos, &c)) {
      *pos = s;
      break;
    }
    i32 v = -1;
    for (i32 i = 0; key[i]; i++) {
      if ((u8)key[i] == c) v = i;
    }
    if (v < 0) {
      *pos = s;
      break;
    }
    *push(&ds, p->a) = (u8)v;
  }
  if (!ds.len) return 0;
  noun a = bass(p, 58, ds.data, ds.len);
  noun b = atomrsh(p->a, a, 32);
  u32 chk = (u32)atomlow(atomend(p->a, a, 32));
  if (base58tok(p, b) != chk) return 0;
  return b;
}

// Phonemic @p and @q

char *prefixes =
  "dozmarbinwansamlitsighidfidlissogdirwacsabwissib"
  "rigsoldopmodfoglidhopdardorlorhodfolrintogsilmir"
  "holpaslacrovlivdalsatlibtabhanticpidtorbolfosdot"
  "losdilforpilramtirwintadbicdifrocwidbisdasmidlop"
  "rilnardapmolsanlocnovsitnidtipsicropwitnatpanmin"
  "ritpodmottamtolsavposnapnopsomfinfonbanmorworsip"
  "ronnorbotwicsocwatdolmagpicdavbidbaltimtasmallig"
  "sivtagpadsaldivdactansidfabtarmonranniswolmispal"
  "lasdismaprabtobrollatlonnodnavfignomnibpagsopral"
  "bilhaddocridmocpacravripfaltodtiltinhapmicfanpat"
  "taclabmogsimsonpinlomrictapfirhasbosbatpochactid"
  "havsaplindibhosdabbitbarracparloddosbortochilmac"
  "tomdigfilfasmithobharmighinradmashalraglagfadtop"
  "mophabnilnosmilfopfamdatnoldinhatnacrisfotribhoc"
  "nimlarfitwalrapsarnalmoslandondanladdovrivbacpol"
  "laptalpitnambonrostonfodponsovnocsorlavmatmipfip";

char *suffixes =
  "zodnecbudwessevpersutletfulpensytdurwepserwylsun"
  "rypsyxdyrnuphebpeglupdepdysputlughecryttyvsydnex"
  "lunmeplutseppesdelsulpedtemledtulmetwenbynhexfeb"
  "pyldulhetmevruttylwydtepbesdexsefwycburderneppur"
  "rysrebdennutsubpetrulsynregtydsupsemwynrecmegnet"
  "secmulnymtevwebsummutnyxrextebfushepbenmuswyxsym"
  "selrucdecwexsyrwetdylmynmesdetbetbeltuxtugmyrpel"
  "syptermebsetdutdegtexsurfeltudnuxruxrenwytnubmed"
  "lytdusnebrumtynseglyxpunresredfunrevrefmectedrus"
  "bexlebduxrynnumpyxrygryxfeptyrtustyclegnemfermer"
  "tenlusnussyltecmexpubrymtucfyllepdebbermughuttun"
  "bylsudpemdevlurdefbusbeprunmelpexdytbyttyplevmyl"
  "wedducfurfexnulluclennerlexrupnedlecrydlydfenwel"
  "nydhusrelrudneshesfetdesretdunlernyrsebhulryllud"
  "remlysfynwerrycsugnysnyllyndyndemluxfedsedbecmun"
  "lyrtesmudnytbyrsenwegfyrmurtelreptegpecnelnevfes";

// three lowercase letters looked up in a syllable table
i32 syllable(parser *p, size *pos, char *table) {
  size s = *pos;
  for (i32 i = 0; i < 3; i++) {
    if (!low(p, pos, 0)) return -1;
  }
  for (i32 i = 0; i < 256; i++) {
    char *t = table + 3*i;
    if (t[0] == p->buf[s] && t[1] == p->buf[s+1] && t[2] == p->buf[s+2]) return i;
  }
  return -1;
}

i32 tip(parser *p, size *pos) { return syllable(p, pos, prefixes); }
i32 tiq(parser *p, size *pos) { return syllable(p, pos, suffixes); }

// prefix and suffix as a 16-bit word
i32 hif(parser *p, size *pos) {
  i32 a = tip(p, pos);
  if (a < 0) return -1;
  i32 b = tiq(p, pos);
  if (b < 0) return -1;
  return a*256 + b;
}

u32 raku[4] = {0xb76d5eed, 0xee281300, 0x85bcae01, 0x4b387af7};

u32 eff(u32 j, u64 r) {
  u8 b[2] = {(u8)r, (u8)(r >> 8)};
  return muk(raku[j], 2, b, 2);
}

u64 fen(u64 r, u64 a, u64 b, u64 m) {
  u64 j = r;
  u64 ahh = r % 2 ? m / a : m % a;
  u64 ale = r % 2 ? m % a : m / a;
  u64 ell = ale == a ? ahh : ale;
  u64 arr = ale == a ? ale : ahh;
  while (j >= 1) {
    u64 f = eff((u32)(j - 1), ell);
    u64 tmp = j % 2 ? (arr + a - f % a) % a : (arr + b - f % b) % b;
    j--;
    arr = ell;
    ell = tmp;
  }
  return arr*a + ell;
}

u64 feen(u64 m) {
  u64 a = 0xffff, b = 0x10000, k = 0xffff*0x10000ull;
  u64 c = fen(4, a, b, m);
  if (c < k) return c;
  return fen(4, a, b, c);
}

u64 fynd(u64 cry) {
  if (cry >= 0x10000 && cry <= 0xffffffff) {
    return 0x10000 + feen(cry - 0x10000);
  }
  if (cry >= 0x100000000ull) {
    u64 lo = cry & 0xffffffff;
    u64 hi = cry & 0xffffffff00000000ull;
    return hi | fynd(lo);
  }
  return cry;
}

// a planet-or-smaller name as words: hef then count more words
b32 pwords(parser *p, size *pos, i32 min, i32 max, b32 nonzero, u64 *out) {
  i32 w = hif(p, pos);
  if (w < 0 || (nonzero && w == 0)) return 0;
  u64 v = (u64)w;
  for (i32 i = 0; i < max; i++) {
    size s = *pos;
    if (chr(p, pos, '-')) {
      i32 x = hif(p, pos);
      if (x >= 0) {
        v = v << 16 | (u64)x;
        continue;
      }
    }
    *pos = s;
    if (i < min) return 0;
    break;
  }
  *out = v;
  return 1;
}

noun fed(parser *p, size *pos) {
  size s = *pos;
  u64 v;
  // oversized, with -- between 64-bit parts
  if (pwords(p, pos, 0, 3, 1, &v)) {
    noun r = D(v);
    i32 n = 0;
    for (;;) {
      size t = *pos;
      u64 w;
      if (doh(p, pos) && pwords(p, pos, 3, 3, 0, &w)) {
        r = atomor(p->a, atomlsh(p->a, r, 64), D(w));
        n++;
        continue;
      }
      *pos = t;
      break;
    }
    if (n) return r;
  }
  *pos = s;
  if (pwords(p, pos, 1, 3, 1, &v)) return D(fynd(v));
  *pos = s;
  i32 a = tip(p, pos);
  if (a >= 0 && a != 0) {
    i32 b = tiq(p, pos);
    if (b >= 0) return D(fynd((u64)(a*256 + b)));
  }
  *pos = s;
  i32 b = tiq(p, pos);
  if (b >= 0) return D((u64)b);
  return 0;
}

noun feq(parser *p, size *pos) {
  size s = *pos;
  i32 w = hif(p, pos);
  if (w < 0) {
    *pos = s;
    w = tiq(p, pos);
    if (w < 0) return 0;
  }
  noun r = D((u64)w);
  for (;;) {
    size t = *pos;
    if (dof(p, pos)) {
      i32 x = hif(p, pos);
      if (x >= 0) {
        r = atomaddsmall(p->a, atomlsh(p->a, r, 16), (u64)x);
        continue;
      }
    }
    *pos = t;
    break;
  }
  return r;
}

// Text auras

noun urs(parser *p, size *pos) {
  size s = *pos;
  while (nud(p, pos, 0) || low(p, pos, 0) || chr(p, pos, '-')
      || chr(p, pos, '.') || chr(p, pos, '~') || chr(p, pos, '_')) {}
  return atombytes(p->a, p->buf + s, *pos - s);
}

noun urt(parser *p, size *pos) {
  size s = *pos;
  while (nud(p, pos, 0) || low(p, pos, 0) || chr(p, pos, '-')
      || chr(p, pos, '.') || chr(p, pos, '~')) {}
  return atombytes(p->a, p->buf + s, *pos - s);
}

// utf32 to utf8, as +tuft
noun tuft(parser *p, noun a) {
  u8 *b = new(p->a, u8, alen(a) * 2 + 4);
  size len = 0;
  for (size i = 0; i < alen(a); i += 4) {
    u32 c = 0;
    for (size j = 0; j < 4 && i + j < alen(a); j++) c |= (u32)abyte(a, i+j) << (8*j);
    if (c <= 0x7f) {
      b[len++] = (u8)c;
    } else if (c <= 0x7ff) {
      b[len++] = (u8)(0xc0 | (c >> 6));
      b[len++] = (u8)(0x80 | (c & 0x3f));
    } else if (c <= 0xffff) {
      b[len++] = (u8)(0xe0 | (c >> 12));
      b[len++] = (u8)(0x80 | ((c >> 6) & 0x3f));
      b[len++] = (u8)(0x80 | (c & 0x3f));
    } else {
      b[len++] = (u8)(0xf0 | ((c >> 18) & 0x7));
      b[len++] = (u8)(0x80 | ((c >> 12) & 0x3f));
      b[len++] = (u8)(0x80 | ((c >> 6) & 0x3f));
      b[len++] = (u8)(0x80 | (c & 0x3f));
    }
  }
  return atombytes(p->a, b, len);
}

// utf8 to utf32, as +taft; 0 if it doesn't round trip
noun taft(parser *p, noun a) {
  u8 *b = new(p->a, u8, alen(a) * 4 + 4);
  size len = 0;
  for (size i = 0; i < alen(a);) {
    u8 c = abyte(a, i);
    if (c < 32 && c != 10) return 0;
    i32 n = c <= 127 ? 1 : c <= 223 ? 2 : c <= 239 ? 3 : 4;
    if (i + n > alen(a)) return 0;
    u32 v;
    switch (n) {
    case 1: v = c; break;
    case 2: v = (u32)(c & 0x1f) << 6 | (abyte(a, i+1) & 0x3f); break;
    case 3: v = (u32)(c & 0xf) << 12 | (u32)(abyte(a, i+1) & 0x3f) << 6
              | (abyte(a, i+2) & 0x3f); break;
    default: v = (u32)(c & 0x7) << 18 | (u32)(abyte(a, i+1) & 0x3f) << 12
               | (u32)(abyte(a, i+2) & 0x3f) << 6 | (abyte(a, i+3) & 0x3f);
    }
    noun back = tuft(p, D(v));
    if (alen(back) != n) return 0;
    for (i32 j = 0; j < n; j++) {
      if (abyte(back, j) != abyte(a, i+j)) return 0;
    }
    for (i32 j = 0; j < 4; j++) b[len++] = (u8)(v >> (8*j));
    i += n;
  }
  return atombytes(p->a, b, len);
}

noun urx(parser *p, size *pos) {
  nouns parts = {0};
  for (;;) {
    size s = *pos;
    u8 c;
    if (nud(p, pos, &c) || low(p, pos, &c) || range(p, pos, '-', '-', &c) || range(p, pos, '_', '_', &c)) {
      *push(&parts, p->a) = D(c);
      continue;
    }
    if (chr(p, pos, '.')) {
      *push(&parts, p->a) = D(' ');
      continue;
    }
    if (chr(p, pos, '~')) {
      size t = *pos;
      noun h = hex(p, pos);
      if (h && chr(p, pos, '.')) {
        *push(&parts, p->a) = tuft(p, h);
        continue;
      }
      *pos = t;
      if (chr(p, pos, '~')) {
        *push(&parts, p->a) = D('~');
        continue;
      }
      if (chr(p, pos, '.')) {
        *push(&parts, p->a) = D('.');
        continue;
      }
    }
    *pos = s;
    break;
  }
  return atomrap3(p->a, parts.data, parts.len);
}

// Jammed nouns in ~0v literals

typedef struct {
  size  pos;
  noun val;
} cueref;

typedef struct {
  cueref *data;
  size    len;
  size    cap;
} cuerefs;

// length-prefixed atom at bit b, as +rub
b32 rub(parser *p, noun a, size b, size *len, noun *out) {
  size m = atombits(a);
  size c = 0;
  while (!atombit(a, b + c)) {
    if (b + c >= m) return 0;
    c++;
  }
  if (!c) {
    *len = 1;
    *out = nul;
    return 1;
  }
  size d = b + c + 1;
  if (c - 1 > 62) return 0;
  size e = (size)1 << (c - 1);
  for (size i = 0; i < c - 1; i++) {
    if (atombit(a, d + i)) e += (size)1 << i;
  }
  noun v = atomend(p->a, atomrsh(p->a, a, d + c - 1), e);
  *len = c + c + e;
  *out = v;
  return 1;
}

b32 cuestep(parser *p, noun a, size b, cuerefs *refs, size *len, noun *out, i32 depth) {
  if (depth > 10000) return 0;
  if (b > atombits(a)) return 0;
  if (!atombit(a, b)) {
    size l;
    noun v;
    if (!rub(p, a, b + 1, &l, &v)) return 0;
    *len = l + 1;
    *out = v;
    cueref *r = push(refs, p->a);
    r->pos = b;
    r->val = v;
    return 1;
  }
  size c = b + 2;
  if (!atombit(a, b + 1)) {
    size l1, l2;
    noun h, t;
    if (!cuestep(p, a, c, refs, &l1, &h, depth + 1)) return 0;
    if (!cuestep(p, a, c + l1, refs, &l2, &t, depth + 1)) return 0;
    *len = 2 + l1 + l2;
    *out = C2(h, t);
    cueref *r = push(refs, p->a);
    r->pos = b;
    r->val = *out;
    return 1;
  }
  size l;
  noun key;
  if (!rub(p, a, c, &l, &key)) return 0;
  if (!atomfits(key)) return 0;
  u64 k = atomlow(key);
  for (size i = 0; i < refs->len; i++) {
    if ((u64)refs->data[i].pos == k) {
      *len = 2 + l;
      *out = refs->data[i].val;
      return 1;
    }
  }
  return 0;
}

noun cue(parser *p, noun a) {
  cuerefs refs = {0};
  size len;
  noun out;
  if (!cuestep(p, a, 0, &refs, &len, &out, 0)) return 0;
  return out;
}

// Floating point: correctly rounded, nearest even, as +grd:ff

typedef struct {
  i32 w;     // exponent bits
  i32 p;     // fraction bits
  i32 bias;
} floatfmt;

floatfmt fmt_rh = {5, 10, 15};
floatfmt fmt_rs = {8, 23, 127};
floatfmt fmt_rd = {11, 52, 1023};
floatfmt fmt_rq = {15, 112, 16383};

noun floatinf(parser *p, floatfmt f, b32 neg) {
  noun e = atomsub(p->a, atomlsh(p->a, D(1), (size)f.w), D(1));
  noun r = atomlsh(p->a, e, (size)f.p);
  if (neg) r = atomor(p->a, r, atomlsh(p->a, D(1), (size)(f.w + f.p)));
  return r;
}

noun floatnan(parser *p, floatfmt f) {
  noun e = atomsub(p->a, atomlsh(p->a, D(1), (size)(f.w + 1)), D(1));
  return atomlsh(p->a, e, (size)(f.p - 1));
}

// m * 10^e10 as float bits
noun floatbits(parser *p, floatfmt f, b32 neg, noun m, noun eabs, b32 eneg) {
  noun sign = neg ? atomlsh(p->a, D(1), (size)(f.w + f.p)) : nul;
  if (!alen(m)) return sign;
  i64 prec = f.p + 1;
  i64 me = 1 - f.bias - f.p;
  i64 ndig = 0;
  {
    noun t = m;
    while (alen(t)) { t = atomdivsmall(p->a, t, 10, 0); ndig++; }
  }
  // far out of range either way: infinity or zero
  if (!atomfits(eabs) || atomlow(eabs) > 20000) {
    return eneg ? sign : floatinf(p, f, neg);
  }
  i64 e10 = (i64)atomlow(eabs);
  if (eneg) e10 = -e10;
  if (e10 + ndig > 5000) return floatinf(p, f, neg);
  if (e10 + ndig < -5000) return sign;

  noun num = m;
  noun den = D(1);
  if (e10 >= 0) {
    num = atommul(p->a, m, atompow(p->a, 5, (size)e10));
  } else {
    den = atompow(p->a, 5, (size)-e10);
  }
  i64 k = prec + 2 - ((i64)atombits(num) - (i64)atombits(den));
  if (k > 0) num = atomlsh(p->a, num, (size)k);
  else if (k < 0) den = atomlsh(p->a, den, (size)-k);
  noun rem;
  noun q = atomdiv(p->a, num, den, &rem);
  b32 sticky = alen(rem) != 0;
  i64 x0 = e10 - k;
  i64 nb = (i64)atombits(q);
  i64 x = MAX(x0 + nb - prec, me);
  i64 drop = x - x0;
  noun r = atomrsh(p->a, q, (size)drop);
  b32 halfbit = atombit(q, (size)(drop - 1));
  b32 lower = sticky;
  for (i64 i = 0; i < drop - 1 && !lower; i++) lower = atombit(q, (size)i);
  if (halfbit && (lower || atombit(r, 0))) {
    r = atomaddsmall(p->a, r, 1);
    if ((i64)atombits(r) > prec) {
      r = atomrsh(p->a, r, 1);
      x++;
    }
  }
  if ((i64)atombits(r) == prec) {
    i64 e = x - me + 1;
    if (e >= ((i64)1 << f.w) - 1) return floatinf(p, f, neg);
    noun mant = atomend(p->a, r, (size)f.p);
    r = atomor(p->a, atomlsh(p->a, D((u64)e), (size)f.p), mant);
  }
  return atomor(p->a, r, sign);
}

// the number part of a float literal, as +royl-rn and +royl-cell
noun roylrn(parser *p, size *pos, floatfmt f) {
  size s = *pos;
  b32 neg = chr(p, pos, '-');
  if (!neg) *pos = s;
  size t = *pos;
  noun i = dim(p, pos);
  if (i) {
    noun m = i;
    size nfrac = 0;
    size u = *pos;
    if (chr(p, pos, '.') && nud(p, pos, 0)) {
      size fs = *pos - 1;
      while (nud(p, pos, 0)) {}
      nfrac = *pos - fs;
      for (size j = fs; j < *pos; j++) {
        m = atomaddsmall(p->a, atommulsmall(p->a, m, 10), (u64)(p->buf[j] - '0'));
      }
    } else {
      *pos = u;
    }
    noun ex = nul;
    b32 exneg = 0;
    u = *pos;
    if (chr(p, pos, 'e')) {
      size v = *pos;
      exneg = chr(p, pos, '-');
      if (!exneg) *pos = v;
      ex = dim(p, pos);
      if (!ex) {
        *pos = u;
        ex = nul;
        exneg = 0;
      }
    }
    // exponent minus the count of fraction digits
    noun eabs;
    b32 eneg;
    if (exneg) {
      eabs = atomaddsmall(p->a, ex, (u64)nfrac);
      eneg = alen(eabs) != 0;
    } else if (atomcmp(ex, D((u64)nfrac)) >= 0) {
      eabs = atomsub(p->a, ex, D((u64)nfrac));
      eneg = 0;
    } else {
      eabs = atomsub(p->a, D((u64)nfrac), ex);
      eneg = 1;
    }
    return floatbits(p, f, neg, m, eabs, eneg);
  }
  *pos = t;
  if (jest(p, pos, "inf")) return floatinf(p, f, neg);
  *pos = s;
  if (jest(p, pos, "nan")) return floatnan(p, f);
  return 0;
}

noun royl(parser *p, size *pos) {
  size s = *pos;
  noun r;
  if (jest(p, pos, "~~") && (r = roylrn(p, pos, fmt_rh))) return C2(K("rh"), r);
  *pos = s;
  if (jest(p, pos, "~~~") && (r = roylrn(p, pos, fmt_rq))) return C2(K("rq"), r);
  *pos = s;
  if (chr(p, pos, '~') && (r = roylrn(p, pos, fmt_rd))) return C2(K("rd"), r);
  *pos = s;
  if ((r = roylrn(p, pos, fmt_rs))) return C2(K("rs"), r);
  return 0;
}

// Dates and spans

b32 yelp(parser *p, noun y) {
  u32 r4, r100, r400;
  atomdivsmall(p->a, y, 4, &r4);
  atomdivsmall(p->a, y, 100, &r100);
  atomdivsmall(p->a, y, 400, &r400);
  return r4 == 0 && (r100 != 0 || r400 == 0);
}

u8 moh[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
u8 moy[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// days since the beginning, as +yawn
noun yawn(parser *p, noun yer, u64 mon, noun day) {
  mon--;
  day = atomsub(p->a, day, D(1));
  u8 *cah = yelp(p, yer) ? moy : moh;
  for (u64 i = 0; i < mon; i++) day = atomaddsmall(p->a, day, cah[i]);
  for (;;) {
    u32 r;
    atomdivsmall(p->a, yer, 4, &r);
    if (r) {
      yer = atomsub(p->a, yer, D(1));
      day = atomaddsmall(p->a, day, yelp(p, yer) ? 366 : 365);
      continue;
    }
    atomdivsmall(p->a, yer, 100, &r);
    if (r) {
      yer = atomsub(p->a, yer, D(4));
      day = atomaddsmall(p->a, day, yelp(p, yer) ? 1461 : 1460);
      continue;
    }
    atomdivsmall(p->a, yer, 400, &r);
    if (r) {
      yer = atomsub(p->a, yer, D(100));
      day = atomaddsmall(p->a, day, yelp(p, yer) ? 36525 : 36524);
      continue;
    }
    noun q = atomdivsmall(p->a, yer, 400, 0);
    return atomadd(p->a, day, atommulsmall(p->a, q, 146097));
  }
}

// days, hours, minutes, seconds and 16-bit fractions to @d, as +yule
noun yule(parser *p, noun d, noun h, noun m, noun s, noun f) {
  noun sec = atommulsmall(p->a, d, 86400);
  sec = atomadd(p->a, sec, atommulsmall(p->a, h, 3600));
  sec = atomadd(p->a, sec, atommulsmall(p->a, m, 60));
  sec = atomadd(p->a, sec, s);
  noun fac = nul;
  i32 muc = 4;
  for (; iscell(f); f = tl(f)) {
    if (!muc) return 0;
    muc--;
    fac = atomadd(p->a, fac, atomlsh(p->a, hd(f), (size)(16*muc)));
  }
  return atomor(p->a, atomlsh(p->a, sec, 64), fac);
}

// dot-separated groups of four hex digits after ..
noun fracs(parser *p, size *pos) {
  size s = *pos;
  if (jest(p, pos, "..")) {
    nouns fs = {0};
    noun q = qix(p, pos);
    if (q) {
      *push(&fs, p->a) = q;
      for (;;) {
        size t = *pos;
        if (chr(p, pos, '.') && (q = qix(p, pos))) {
          *push(&fs, p->a) = q;
          continue;
        }
        *pos = t;
        break;
      }
      return nounslist(p, &fs);
    }
  }
  *pos = s;
  return nul;
}

noun when(parser *p, size *pos) {
  noun y = dim(p, pos);
  if (!y) return 0;
  size s = *pos;
  b32 ad = !chr(p, pos, '-');
  if (ad) *pos = s;
  if (!chr(p, pos, '.')) return 0;
  noun m = mot(p, pos);
  if (!m || !chr(p, pos, '.')) return 0;
  noun d = dap(p, pos);
  if (!d) return 0;
  noun h = nul, mi = nul, sc = nul, f = nul;
  s = *pos;
  noun hh, mm, ss;
  if (jest(p, pos, "..") && (hh = dum(p, pos)) && chr(p, pos, '.')
      && (mm = dum(p, pos)) && chr(p, pos, '.') && (ss = dum(p, pos))) {
    h = hh;
    mi = mm;
    sc = ss;
    f = fracs(p, pos);
  } else {
    *pos = s;
  }
  noun jes = D(292277024400ull);
  noun yer;
  if (ad) {
    yer = atomadd(p->a, jes, y);
  } else {
    if (!alen(y)) return 0;
    noun yd = atomsub(p->a, y, D(1));
    if (atomcmp(yd, jes) > 0) return 0;
    yer = atomsub(p->a, jes, yd);
  }
  noun day = yawn(p, yer, atomlow(m), d);
  return yule(p, day, h, mi, sc, f);
}

noun drspan(parser *p, size *pos) {
  noun t[4] = {nul, nul, nul, nul};
  char *units = "dhms";
  b32 first = 1;
  for (;;) {
    size s = *pos;
    if (!first && !chr(p, pos, '.')) {
      *pos = s;
      break;
    }
    i32 u = -1;
    for (i32 i = 0; i < 4; i++) {
      size t2 = *pos;
      if (chr(p, pos, (u8)units[i])) {
        u = i;
        break;
      }
      *pos = t2;
    }
    noun v = u >= 0 ? dim(p, pos) : 0;
    if (!v) {
      if (first) return 0;
      *pos = s;
      break;
    }
    t[u] = atomadd(p->a, t[u], v);
    first = 0;
  }
  noun f = fracs(p, pos);
  return yule(p, t[0], t[1], t[2], t[3], f);
}

// Coins

noun dime(parser *p, char *aura, noun v) {
  return C2(term(aura), v);
}

noun bisk(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) {
    size t = *pos;
    noun v;
    if (chr(p, pos, 'b') && (v = bay(p, pos))) return dime(p, "ub", v);
    *pos = t;
    if (chr(p, pos, 'c') && (v = fim(p, pos))) return dime(p, "uc", v);
    *pos = t;
    if (chr(p, pos, 'i') && (v = dim(p, pos))) return dime(p, "ui", v);
    *pos = t;
    if (chr(p, pos, 'x') && (v = hex(p, pos))) return dime(p, "ux", v);
    *pos = t;
    if (chr(p, pos, 'v') && (v = viz(p, pos))) return dime(p, "uv", v);
    *pos = t;
    if (chr(p, pos, 'w') && (v = wiz(p, pos))) return dime(p, "uw", v);
  }
  *pos = s;
  noun v = dem(p, pos);
  return v ? dime(p, "ud", v) : 0;
}

// a signed dime from an unsigned one, as +new:si
noun signdime(parser *p, b32 syn, noun d) {
  noun aura = hd(d);
  u8 *b = new(p->a, u8, alen(aura));
  u8 tmp[8];
  copy((byte*)b, (byte*)abytes(aura, tmp), alen(aura));
  b[0] = 's';
  noun v = tl(d);
  noun r;
  if (syn) {
    r = atomlsh(p->a, v, 1);
  } else if (!alen(v)) {
    r = nul;
  } else {
    r = atomaddsmall(p->a, atomlsh(p->a, atomsub(p->a, v, D(1)), 1), 1);
  }
  return C2(atombytes(p->a, b, alen(aura)), r);
}

noun tash(parser *p, size *pos) {
  if (!chr(p, pos, '-')) return 0;
  size s = *pos;
  noun d = bisk(p, pos);
  if (d) return signdime(p, 0, d);
  *pos = s;
  if (chr(p, pos, '-') && (d = bisk(p, pos))) return signdime(p, 1, d);
  return 0;
}

noun crub(parser *p, size *pos) {
  size s = *pos;
  noun v;
  if ((v = when(p, pos))) return dime(p, "da", v);
  *pos = s;
  if ((v = drspan(p, pos))) return dime(p, "dr", v);
  *pos = s;
  if ((v = fed(p, pos))) return dime(p, "p", v);
  *pos = s;
  if (chr(p, pos, '.')) return dime(p, "ta", urs(p, pos));
  *pos = s;
  if (chr(p, pos, '~')) return dime(p, "t", urx(p, pos));
  *pos = s;
  if (chr(p, pos, '-') && (v = taft(p, urx(p, pos)))) return dime(p, "c", v);
  return 0;
}

noun twid(parser *p, size *pos) {
  size s = *pos;
  if (chr(p, pos, '0')) {
    digits ds = {0};
    if (stun(p, pos, dig_siv, 1, 0x7fffffff, &ds)) {
      noun v = cue(p, bass(p, 32, ds.data, ds.len));
      if (v) return C2(K("blob"), v);
    }
  }
  *pos = s;
  noun d = crub(p, pos);
  return d ? C2(nul, d) : 0;
}

noun zust(parser *p, size *pos) {
  size s = *pos;
  noun v;
  if ((v = bip(p, pos))) return dime(p, "is", v);
  *pos = s;
  if ((v = lip(p, pos))) return dime(p, "if", v);
  *pos = s;
  if ((v = royl(p, pos))) return v;
  *pos = s;
  if (chr(p, pos, 'y')) return dime(p, "f", YES);
  if (chr(p, pos, 'n')) return dime(p, "f", NO);
  if (chr(p, pos, '~') && (v = feq(p, pos))) return dime(p, "q", v);
  return 0;
}

noun nuck(parser *p, size *pos);

// an escaped knot, unescaped and parsed as a whole coin
noun nusk(parser *p, size *pos) {
  noun t = urt(p, pos);
  u8 *b = new(p->a, u8, alen(t) + 1);
  size len = 0;
  for (size i = 0; i < alen(t); i++) {
    u8 c = abyte(t, i);
    if (c != '~') {
      b[len++] = c;
      continue;
    }
    if (i + 1 >= alen(t)) return 0;
    if (abyte(t, i+1) == '~') b[len++] = '~';
    else if (abyte(t, i+1) == '-') b[len++] = '_';
    else return 0;
    i++;
  }
  parser q = subparser(p, b, len, 1);
  size qpos = 0;
  noun r = nuck(&q, &qpos);
  if (!r || qpos != len) return 0;
  return r;
}

noun perd(parser *p, size *pos) {
  size s = *pos;
  noun d = zust(p, pos);
  if (d) return C2(nul, d);
  *pos = s;
  if (!chr(p, pos, '_')) return 0;
  nouns cs = {0};
  size t = *pos;
  noun c = nusk(p, pos);
  if (c) {
    *push(&cs, p->a) = c;
    for (;;) {
      t = *pos;
      if (chr(p, pos, '_') && (c = nusk(p, pos))) {
        *push(&cs, p->a) = c;
        continue;
      }
      *pos = t;
      break;
    }
  } else {
    *pos = t;
  }
  if (!jest(p, pos, "__")) return 0;
  return C2(K("many"), nounslist(p, &cs));
}

noun nuck(parser *p, size *pos) {
  if (*pos >= p->len) {
    reach(p, *pos);
    return 0;
  }
  u8 c = p->buf[*pos];
  if (c >= 'a' && c <= 'z') {
    noun s = sym(p, pos);
    return C3(nul, K("tas"), s);
  }
  if (c >= '0' && c <= '9') {
    noun d = bisk(p, pos);
    return d ? C2(nul, d) : 0;
  }
  if (c == '-') {
    noun d = tash(p, pos);
    return d ? C2(nul, d) : 0;
  }
  if (c == '.') {
    chr(p, pos, '.');
    return perd(p, pos);
  }
  if (c == '~') {
    chr(p, pos, '~');
    size s = *pos;
    noun r = twid(p, pos);
    if (r) return r;
    *pos = s;
    return C3(nul, K("n"), nul);
  }
  reach(p, *pos);
  return 0;
}

// coin to hoon, as +jock
noun jock(parser *p, b32 rad, noun lot) {
  if (atomis(hd(lot), 0)) {
    return C2(term(rad ? "rock" : "sand"), tl(lot));
  }
  if (atomeqc(hd(lot), "blob")) {
    noun v = tl(lot);
    if (rad) return C3(K("rock"), nul, v);
    if (isatom(v)) return C3(K("sand"), nul, v);
    return C2(jock(p, rad, C2(K("blob"), hd(v))), jock(p, rad, C2(K("blob"), tl(v))));
  }
  nouns xs = {0};
  for (noun l = tl(lot); iscell(l); l = tl(l)) *push(&xs, p->a) = jock(p, rad, hd(l));
  return C2(K("cltr"), nounslist(p, &xs));
}

// Hoon, following +vast. Rules taking tol are parameterized on tall form,
// like the +norm core.

noun tall(parser *p, size *pos);
noun wide(parser *p, size *pos);
noun till(parser *p, size *pos);
noun wyde(parser *p, size *pos);
noun scat(parser *p, size *pos);
noun scad(parser *p, size *pos);
noun makerest(parser *p, size *pos, noun x);
noun rope(parser *p, size *pos);
noun sailapex(parser *p, size *pos, b32 tall);
noun soil(parser *p, size *pos);

// a string literal of up to 8 bytes as the word of its atom, at compile
// time
#define TWB(s, i) ((i) < sizeof(s) - 1 ? (u64)(u8)(s)[(i) < sizeof(s) ? (i) : 0] << 8*(i) : 0)
#define TW(s) (TWB(s, 0) | TWB(s, 1) | TWB(s, 2) | TWB(s, 3) | \
               TWB(s, 4) | TWB(s, 5) | TWB(s, 6) | TWB(s, 7))

b32 tagisw(noun n, char *s, u64 w, size len) {
  if (!iscell(n) || !isatom(hd(n))) return 0;
  if (len > 8) return atomeqc(hd(n), s);
  return atomfits(hd(n)) && atomlow(hd(n)) == w;
}

// whether a cell's head is a literal, as [%tag ...]
#define tagis(n, s) tagisw((n), (s), TW(s), sizeof(s) - 1)

noun wart(parser *p, size start, size end, noun r) {
  if (!p->bug) return r;
  // as hoon, [wer [line col] [line col]]
  hair a = hairat(p, start);
  hair b = hairat(p, end);
  noun pint = C2(C2(D((u64)a.line), D((u64)a.col)), C2(D((u64)b.line), D((u64)b.col)));
  noun wer = (p->root ? p->root : p)->wer;
  return C3(K("dbug"), C2(wer ? wer : nul, pint), r);
}

noun loaf(parser *p, size *pos, b32 tol) { return tol ? tall(p, pos) : wide(p, pos); }
noun loan(parser *p, size *pos, b32 tol) { return tol ? till(p, pos) : wyde(p, pos); }

b32 muck(parser *p, size *pos, b32 tol) {
  return tol ? gap(p, pos) : ace(p, pos);
}

b32 mash(parser *p, size *pos, b32 tol) {
  return tol ? gap(p, pos) : (chr(p, pos, ',') && ace(p, pos));
}

// rules separated by muck, as ;~(gunk ...)
noun seqn(parser *p, size *pos, b32 tol, rule *rs) {
  noun v[8];
  i32 n = 0;
  while (rs[n]) n++;
  for (i32 i = 0; i < n; i++) {
    if (i && !muck(p, pos, tol)) return 0;
    if (!(v[i] = rs[i](p, pos, tol))) return 0;
  }
  noun r = v[n-1];
  for (i32 i = n - 2; i >= 0; i--) r = C2(v[i], r);
  return r;
}

#define SEQ(...) seqn(p, pos, tol, (rule[]){__VA_ARGS__, 0})

// one or more, separated
// the list of items pushed on the stack since base, popping them
noun stklist(parser *p, size base) {
  noun r = mklist(p, p->stk.data + base, p->stk.len - base);
  p->stk.len = base;
  return r;
}

noun most(parser *p, size *pos, b32 tol, b32 (*sep)(parser *, size *, b32), rule fel) {
  noun r = fel(p, pos, tol);
  if (!r) return 0;
  size base = p->stk.len;
  *push(&p->stk, p->a) = r;
  for (;;) {
    size s = *pos;
    if (sep(p, pos, tol) && (r = fel(p, pos, tol))) {
      *push(&p->stk, p->a) = r;
      continue;
    }
    *pos = s;
    return stklist(p, base);
  }
}

b32 sepace(parser *p, size *pos, b32 tol) { return ace(p, pos); }
b32 sepgap(parser *p, size *pos, b32 tol) { return gap(p, pos); }
b32 sepcol(parser *p, size *pos, b32 tol) { return chr(p, pos, ':'); }

noun widerule(parser *p, size *pos, b32 tol) { return wide(p, pos); }
noun tallrule(parser *p, size *pos, b32 tol) { return tall(p, pos); }
noun wyderule(parser *p, size *pos, b32 tol) { return wyde(p, pos); }
noun roperule(parser *p, size *pos, b32 tol) { return rope(p, pos); }
noun symrule(parser *p, size *pos, b32 tol) { return sym(p, pos); }

// (a b c) etc.
noun parens(parser *p, size *pos, rule fel) {
  if (!chr(p, pos, '(')) return 0;
  noun r = most(p, pos, 0, sepace, fel);
  if (!r || !chr(p, pos, ')')) return 0;
  return r;
}

noun brackets(parser *p, size *pos, rule fel) {
  if (!chr(p, pos, '[')) return 0;
  noun r = most(p, pos, 0, sepace, fel);
  if (!r || !chr(p, pos, ']')) return 0;
  return r;
}

// Macro helpers from +ap, +ax and +ah

noun reek(parser *p, noun gen) {
  for (;;) {
    if (atomis(hd(gen), 0)) return C2(C2(YES, tl(gen)), nul);
    if (tagis(gen, "limb")) return C2(tl(gen), nul);
    if (tagis(gen, "wing")) return tl(gen);
    if (tagis(gen, "cnts") && atomis(tl(tl(gen)), 0)) return hd(tl(gen));
    if (tagis(gen, "dbug")) {
      gen = tl(tl(gen));
      continue;
    }
    return 0;
  }
}

// a map from nouns to nouns by handle, which, as every noun is made
// once, is by value
typedef struct {
  noun *keys;
  noun *vals;
  u32   cap;
  u32   len;
} nounmap;

// by value: two nouns that are equal hash the same
u32 handlehash(noun k) {
  return (u32)(((u64)(k ? mug(k) : 0) * 0x9e3779b97f4a7c15ull) >> 32);
}

u32 nounmapslot(nounmap *m, noun k) {
  u32 j = handlehash(k) & (m->cap - 1);
  while (m->keys[j] && !nouneq(m->keys[j], k)) j = (j + 1) & (m->cap - 1);
  return j;
}

// the value for k, or 0
noun nounmapget(nounmap *m, noun k) {
  return m->cap ? m->vals[nounmapslot(m, k)] : 0;
}

void nounmapput(nounmap *m, noun k, noun v) {
  if (m->len*4 >= m->cap*3) {
    nounmap n = {0};
    n.cap = m->cap ? m->cap * 2 : 1 << 12;
    n.keys = new(&H.perm, noun, n.cap);
    n.vals = new(&H.perm, noun, n.cap);
    for (u32 i = 0; i < m->cap; i++) {
      if (!m->keys[i]) continue;
      u32 j = nounmapslot(&n, m->keys[i]);
      n.keys[j] = m->keys[i];
      n.vals[j] = m->vals[i];
    }
    if (m->cap) {
      osrelease((byte*)m->keys, (byte*)(m->keys + m->cap));
      osrelease((byte*)m->vals, (byte*)(m->vals + m->cap));
    }
    n.len = m->len;
    *m = n;
  }
  u32 j = nounmapslot(m, k);
  if (!m->keys[j]) m->len++;
  m->keys[j] = k;
  m->vals[j] = v;
}

// Sources, for messages: each file compiled, by its name and the path
// in its spots, and where in them the arms of cores are, by the hoon
// of each, which a type made from the arm carries. Hoons are made once,
// so equal arms in two places are found in the first.

typedef struct {
  char *name;   // as given, like sys/lull.hoon or app/foo.hoon
  noun  wer;    // the path in its spots, or 0
  s8    src;
  size  cap;    // of src, when it's a copy
} srcfile;

// whether srcadd keeps copies of names and texts, as the language
// server, whose texts don't last, needs
b32 srccopy;

struct {
  srcfile *data;
  i32      len;
  i32      cap;
} srcfiles;

nounmap armsites;   // arm hoon, hashed, to file<<40 | line<<16 | col
nounmap armnames;   // the $ arm a mold builder, |$, makes, hashed, to its name

// an arm's hoon as a key in armsites: two mugs of it, as an atom, which
// isn't collected as the hoon would be
noun armkey(arena *a, noun gen) {
  return atomu64(a, (u64)mug(gen) << 32 | mug(cons(a, gen, nul)));
}

b32 streq(char *a, char *b);

// a file for messages, by name; its number
i32 srcadd(char *name, noun wer, s8 src) {
  // a file read again, as the language server does, keeps its number
  for (i32 i = 1; i < srcfiles.len; i++) {
    srcfile *f = &srcfiles.data[i];
    if (!streq(f->name, name)) continue;
    f->wer = wer;
    if (!srccopy) {
      f->src = src;
    } else if (src.buf != f->src.buf) {
      // in the copy, made bigger if need be
      if (src.len > f->cap) {
        f->cap = src.len * 2;
        f->src.buf = new(&H.perm, u8, f->cap);
      }
      if (src.len) copy((byte*)f->src.buf, (byte*)src.buf, src.len);
      f->src.len = src.len;
    }
    return i;
  }
  size cap = 0;
  if (srccopy) {
    size n = (size)__builtin_strlen(name);
    char *nm = new(&H.perm, char, n + 1);
    copy(nm, name, n + 1);
    name = nm;
    cap = src.len * 2;
    u8 *b = new(&H.perm, u8, cap + 1);
    if (src.len) copy((byte*)b, (byte*)src.buf, src.len);
    src.buf = b;
  }
  if (srcfiles.len + 1 >= srcfiles.cap) {
    i32 cap = srcfiles.cap ? srcfiles.cap * 2 : 64;
    srcfile *more = new(&H.perm, srcfile, cap);
    if (srcfiles.len) copy((byte*)more, (byte*)srcfiles.data, srcfiles.len * (size)sizeof(srcfile));
    srcfiles.data = more;
    srcfiles.cap = cap;
    if (!srcfiles.len) srcfiles.len = 1;  // 0 is none
  }
  srcfiles.data[srcfiles.len] = (srcfile){name, wer, src, cap};
  return srcfiles.len++;
}

char *cwdpath;   // the directory we're run in, absolute, or 0

// a file's name as shown: from the directory we're in, if it's under it
s8 shown(char *name) {
  size n = (size)__builtin_strlen(name), k = 0;
  if (cwdpath) {
    while (cwdpath[k] && cwdpath[k] == name[k]) k++;
    if (!cwdpath[k] && name[k] == '/') return (s8){(u8*)name + k + 1, n - k - 1};
  }
  return (s8){(u8*)name, n};
}

// the file whose spots have the path wer; 0 if none
i32 srcbywer(noun wer) {
  for (i32 i = srcfiles.len - 1; i > 0; i--) {
    if (srcfiles.data[i].wer == wer) return i;
  }
  return 0;
}

// Errors. Where hoon would crash, the compiler returns 0 up to the top;
// what went wrong is kept here where it starts, as a nest that fails,
// and the innermost spot it's under is added on the way up.

enum { errnone, errnest, errfind, errtext, errdeep };

struct {
  i32   kind;
  noun  need;   // for errnest, the types
  noun  have;
  noun  hyp;    // for errfind, the wing
  noun  text;   // for errtext, a tape
  noun  spot;   // [path pint] of the innermost %dbug, or 0
} hcerr;

void errnew(i32 kind) {
  hcerr.kind = kind;
  hcerr.need = hcerr.have = hcerr.hyp = hcerr.text = hcerr.spot = 0;
}

// a failure under a spot: the innermost is kept
void errspot(noun spot) {
  if (!hcerr.spot) hcerr.spot = spot;
}

// The compiler recurses on the C stack as deep as the hoon it's
// compiling does, which for a hoon like an arm whose mold is itself,
// +$  a  a, among other arms, is forever: hoon runs out of stack and
// bails. Past stackmax of the 8MB stack, +mint, +mull and +play crash
// instead, and so does nock where it calls, for a mold like that run
// to bunt it. That can't be caught, as hoon's bail can't, so deeps
// counts them for what would otherwise catch it. stacktop is set where
// the program starts.
byte *stacktop;
#ifdef HC_EMBED
size stackmax = 6 << 20;   // set by the host, from the stack it has left
#else
enum { stackmax = 6 << 20 };
#endif
u64 deeps;

b32 stackdeep(void) {
  byte here;
  if (!stacktop || stacktop - &here < stackmax) return 0;
  errnew(errdeep);
  deeps++;
  return 1;
}

#ifdef HC_EMBED
// see below: asked in more places here, where nothing else would stop
// a compile that runs away
b32 (*interrupted)(void);
b32 stopping(void);
#endif

noun peg(parser *p, noun a, noun b);
// Macro expansion: +open from +ap and spec expansion from +ax. These are
// pure hoon to hoon rewrites, used by +flay while parsing and by +play.

#define HOON_VERSION 135

noun hoonopen(parser *p, noun gen);

// value lookup in a map, 0 if absent, as +get:by
noun mapget(noun m, noun key) {
  while (iscell(m)) {
    if (nouneq(key, hd(mapn(m)))) return tl(mapn(m));
    m = gor(key, hd(mapn(m))) ? mapl(m) : mapr(m);
  }
  return 0;
}

// apply f to every value in place, as +run:by
noun maprun(parser *p, noun m, noun (*f)(parser *, noun , void *), void *ctx) {
  if (isatom(m)) return m;
  noun n = mapn(m);
  noun v = f(p, tl(n), ctx);
  if (!v) return 0;
  noun l = maprun(p, mapl(m), f, ctx);
  noun r = l ? maprun(p, mapr(m), f, ctx) : 0;
  if (!r) return 0;
  return mapnode(p->a, C2(hd(n), v), l, r);
}

void maptap(noun m, nouns *out, arena *a) {
  if (isatom(m)) return;
  maptap(mapr(m), out, a);
  *push(out, a) = mapn(m);
  maptap(mapl(m), out, a);
}

// the spec compiler state, as the sample of +ax
typedef struct {
  noun dom;  // axis to home
  noun hay;  // wing to home
  noun cox;  // hygienic context, map term spec
  noun bug;  // debug spots
  noun nut;  // unit note
  noun def;  // unit default hoon
} axs;

axs axclear(axs a) {
  a.bug = nul;
  a.def = nul;
  a.nut = nul;
  return a;
}

noun axhome(parser *p, axs a, noun gen) {
  noun w = atomis(a.dom, 1) ? a.hay : weld(p, a.hay, C2(C2(YES, a.dom), nul));
  if (isatom(w)) return gen;
  return C3(K("tsgr"), C2(K("wing"), w), gen);
}

noun axdecorate(parser *p, axs a, noun gen) {
  nouns bs = {0};
  for (noun b = a.bug; iscell(b); b = tl(b)) *push(&bs, p->a) = hd(b);
  for (size i = bs.len - 1; i >= 0; i--) gen = C3(K("dbug"), bs.data[i], gen);
  if (isatom(a.nut)) return gen;
  return C3(K("note"), tl(a.nut), gen);
}

noun axbasal(parser *p, noun bas) {
  noun rock0 = C3(K("rock"), nul, nul);
  if (iscell(bas)) {
    // ~2000.1.1
    noun v = atomeqc(tl(bas), "da") ? atomlsh(p->a, D(0x8000000d070b5100ull), 64) : nul;
    return C3(K("sand"), tl(bas), v);
  }
  if (atomeqc(bas, "noun")) {
    return C3(K("ktls"), C4(K("dttr"), rock0, rock0, C3(K("rock"), nul, D(1))), rock0);
  }
  if (atomeqc(bas, "cell")) {
    noun n = axbasal(p, K("noun"));
    return C2(n, n);
  }
  if (atomeqc(bas, "flag")) return C3(K("ktls"), C3(K("dtts"), rock0, rock0), rock0);
  if (atomeqc(bas, "null")) return C3(K("rock"), K("n"), nul);
  return C2(K("zpzp"), nul);
}

noun axunfold(parser *p, noun fun, noun arg) {
  nouns xs = {0};
  for (; iscell(arg); arg = tl(arg)) *push(&xs, p->a) = C2(K("ktcl"), hd(arg));
  return C3(K("cncl"), fun, nounslist(p, &xs));
}

noun axunreel(parser *p, noun one, noun res) {
  if (isatom(res)) return C2(K("wing"), one);
  return C3(K("tsgl"), C2(K("wing"), one), axunreel(p, hd(res), tl(res)));
}

noun axpieces(parser *p, noun l) {
  nouns xs = {0};
  for (; iscell(l); l = tl(l)) *push(&xs, p->a) = C2(hd(l), nul);
  return nounslist(p, &xs);
}

noun axexample(parser *p, axs a, noun mod);
noun axrelative(parser *p, axs a, noun axe, noun mod);

noun axfunction(parser *p, axs a, noun fun, noun arg) {
  noun ctx = C2(axexample(p, axclear(a), fun), axexample(p, axclear(a), arg));
  return C3(K("tsgr"), ctx, C2(K("ktbr"), C3(K("brcl"), C2(nul, D(2)), C2(nul, D(15)))));
}

b32 streq(char *a, char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

noun axinterface(parser *p, axs a, char *variance, noun payload, noun arms) {
  nouns xs = {0};
  maptap(arms, &xs, p->a);
  noun m = nul;
  for (size i = 0; i < xs.len; i++) {
    m = mapput(p->a, m, hd(xs.data[i]), axexample(p, axclear(a), tl(xs.data[i])));
  }
  noun r = C3(K("tsgr"), axexample(p, axclear(a), payload),
               C3(K("brcn"), nul, mapnode(p->a, C2(nul, m), nul, nul)));
  if (streq(variance, "lead")) return C2(K("ktwt"), r);
  if (streq(variance, "zinc")) return C2(K("ktpm"), r);
  if (streq(variance, "iron")) return C2(K("ktbr"), r);
  return r;
}

// the default sample structure, before +spore wraps it
noun axsporeinner(parser *p, axs a, noun mod) {
  for (;;) {
    noun m = tl(mod);
    if (tagis(mod, "base")) {
      return atomeqc(m, "void") ? C3(K("rock"), K("n"), nul) : axbasal(p, m);
    }
    if (tagis(mod, "bcbc")) {
      a.cox = mapput(p->a, mapuni(p->a, a.cox, tl(m)), nul, hd(m));
      mod = hd(m);
      continue;
    }
    if (tagis(mod, "dbug")) return C3(K("dbug"), hd(m), axsporeinner(p, a, tl(m)));
    if (tagis(mod, "leaf")) return C3(K("rock"), hd(m), tl(m));
    if (tagis(mod, "loop")) {
      noun s = mapget(a.cox, m);
      if (!s) return 0;
      mod = s;
      continue;
    }
    if (tagis(mod, "like")) {
      mod = C2(K("bcmc"), axunreel(p, hd(m), tl(m)));
      continue;
    }
    if (tagis(mod, "made") || tagis(mod, "name") || tagis(mod, "bcgl") || tagis(mod, "bcgr")
        || tagis(mod, "bckt")) {
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "make")) {
      mod = C2(K("bcmc"), axunfold(p, hd(m), tl(m)));
      continue;
    }
    if (tagis(mod, "over")) {
      a.hay = hd(m);
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "bcbr") || tagis(mod, "bcpm") || tagis(mod, "bcpt")) {
      mod = hd(m);
      continue;
    }
    if (tagis(mod, "bccl")) {
      if (isatom(tl(m))) {
        mod = hd(m);
        continue;
      }
      noun h = axsporeinner(p, a, hd(m));
      noun t = axsporeinner(p, a, C2(K("bccl"), tl(m)));
      return h && t ? C2(h, t) : 0;
    }
    if (tagis(mod, "bccn") || tagis(mod, "bcwt")) {
      noun l = m;
      while (iscell(tl(l))) l = tl(l);
      mod = hd(l);
      continue;
    }
    if (tagis(mod, "bcls")) {
      noun r = axsporeinner(p, a, tl(m));
      return r ? C3(K("note"), C2(K("know"), hd(m)), r) : 0;
    }
    if (tagis(mod, "bcmc")) return C3(K("tsgl"), C2(nul, D(6)), m);
    if (tagis(mod, "bcsg")) return C3(K("kthp"), tl(m), hd(m));
    if (tagis(mod, "bcts")) {
      noun r = axsporeinner(p, a, tl(m));
      return r ? C3(K("ktts"), hd(m), r) : 0;
    }
    // %bccb %bchp %bcdt %bcfs %bctc %bczp
    return C3(K("rock"), K("n"), nul);
  }
}

noun axspore(parser *p, axs a, noun mod) {
  noun body = iscell(a.def) ? tl(a.def) : axsporeinner(p, a, mod);
  if (!body) return 0;
  return C3(K("ktls"), C2(K("bust"), K("noun")), axdecorate(p, a, axhome(p, a, body)));
}

axs axdescend(parser *p, axs a, u64 axe) {
  a.dom = peg(p, D(axe), a.dom);
  return a;
}

noun axexample(parser *p, axs a, noun mod) {
  for (;;) {
    noun m = tl(mod);
    if (tagis(mod, "base")) return axdecorate(p, a, axbasal(p, m));
    if (tagis(mod, "dbug")) {
      a.bug = C2(hd(m), a.bug);
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "leaf")) return axdecorate(p, a, C3(K("rock"), hd(m), tl(m)));
    if (tagis(mod, "like")) {
      mod = C2(K("bcmc"), axunreel(p, hd(m), tl(m)));
      continue;
    }
    if (tagis(mod, "loop")) return C2(K("limb"), m);
    if (tagis(mod, "made")) {
      a.nut = C2(nul, C3(K("made"), hd(hd(m)), C2(nul, axpieces(p, tl(hd(m))))));
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "make")) {
      mod = C2(K("bcmc"), axunfold(p, hd(m), tl(m)));
      continue;
    }
    if (tagis(mod, "name")) {
      a.nut = C2(nul, C3(K("made"), hd(m), nul));
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "over")) {
      a.hay = hd(m);
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "bccb")) return axdecorate(p, a, axhome(p, a, m));
    if (tagis(mod, "bccl")) {
      nouns xs = {0};
      for (noun l = m; iscell(l); l = tl(l)) {
        noun e = axexample(p, axclear(a), hd(l));
        if (!e) return 0;
        *push(&xs, p->a) = e;
      }
      noun r = xs.data[xs.len-1];
      for (size i = xs.len - 2; i >= 0; i--) r = C2(xs.data[i], r);
      return axdecorate(p, a, r);
    }
    if (tagis(mod, "bchp")) return axdecorate(p, a, axfunction(p, axclear(a), hd(m), tl(m)));
    if (tagis(mod, "bcmc")) {
      return axdecorate(p, a, axhome(p, a, C3(K("tsgl"), C2(K("limb"), nul), m)));
    }
    if (tagis(mod, "bcsg")) {
      noun e = axexample(p, a, tl(m));
      return e ? C3(K("ktls"), e, axhome(p, a, hd(m))) : 0;
    }
    if (tagis(mod, "bcls")) {
      noun e = axexample(p, a, tl(m));
      return e ? axdecorate(p, a, C3(K("note"), C2(K("know"), hd(m)), e)) : 0;
    }
    if (tagis(mod, "bcts")) {
      noun e = axexample(p, axclear(a), tl(m));
      return e ? axdecorate(p, a, C3(K("ktts"), hd(m), e)) : 0;
    }
    if (tagis(mod, "bcdt")) return axdecorate(p, a, axhome(p, a, axinterface(p, a, "gold", hd(m), tl(m))));
    if (tagis(mod, "bcfs")) return axdecorate(p, a, axhome(p, a, axinterface(p, a, "iron", hd(m), tl(m))));
    if (tagis(mod, "bczp")) return axdecorate(p, a, axhome(p, a, axinterface(p, a, "lead", hd(m), tl(m))));
    if (tagis(mod, "bctc")) return axdecorate(p, a, axhome(p, a, axinterface(p, a, "zinc", hd(m), tl(m))));
    // otherwise make and analyze a spore
    noun s = axspore(p, a, mod);
    if (!s) return 0;
    noun r = axrelative(p, axdescend(p, a, 3), D(2), mod);
    return r ? C3(K("tsls"), s, r) : 0;
  }
}

noun wingaxis(parser *p, noun axe) {
  return C2(C2(YES, axe), nul);
}

noun axbasic(parser *p, axs a, noun axe, noun bas) {
  noun fetch = C2(nul, axe);
  if (iscell(bas)) {
    noun ex = axexample(p, a, C2(K("base"), bas));
    noun ruth = C2(C2(C3(NO, nul, C2(nul, K("ruth"))), nul), nul);
    noun test = C4(K("wtpt"), wingaxis(p, axe), fetch, C2(K("zpzp"), nul));
    noun call = C4(K("cnls"), C2(K("limb"), K("ruth")), C3(K("sand"), K("ta"), tl(bas)), fetch);
    return C3(K("ktls"), ex, C4(K("zppt"), ruth, test, call));
  }
  if (atomeqc(bas, "cell")) {
    noun ex = axexample(p, a, C2(K("base"), bas));
    noun w = wingaxis(p, axe);
    return C3(K("ktls"), ex, C2(C2(K("wing"), C2(C2(YES, D(2)), w)),
                                C2(K("wing"), C2(C2(YES, D(3)), w))));
  }
  if (atomeqc(bas, "flag")) {
    return C4(K("wtcl"), C3(K("dtts"), C3(K("rock"), nul, YES), fetch), C3(K("rock"), K("f"), YES),
              C3(K("wtgr"), C3(K("dtts"), C3(K("rock"), nul, NO), fetch), C3(K("rock"), K("f"), NO)));
  }
  if (atomeqc(bas, "noun")) return fetch;
  if (atomeqc(bas, "null")) {
    return C3(K("wtgr"), C3(K("dtts"), C2(K("bust"), K("noun")), fetch), C3(K("rock"), K("n"), nul));
  }
  return C2(K("zpzp"), nul);
}

noun axchoice(parser *p, axs a, noun axe, noun one, noun rep) {
  if (isatom(rep)) return axrelative(p, axclear(a), axe, one);
  noun ex = axexample(p, axclear(a), one);
  noun yes = axrelative(p, axclear(a), axe, one);
  noun no = axchoice(p, a, axe, hd(rep), tl(rep));
  if (!ex || !yes || !no) return 0;
  return C4(K("wtcl"), C3(K("fits"), ex, wingaxis(p, axe)), yes, no);
}

noun axswitch(parser *p, axs a, noun axe, noun one, noun rep) {
  if (isatom(rep)) return axrelative(p, axclear(a), axe, one);
  noun fin = axswitch(p, a, axe, hd(rep), tl(rep));
  noun ex = axexample(p, axclear(a), one);
  noun yes = axrelative(p, axclear(a), axe, one);
  if (!fin || !ex || !yes) return 0;
  noun test = C3(K("fits"), C3(K("tsgl"), C2(nul, D(2)), ex), wingaxis(p, peg(p, axe, D(2))));
  return C4(K("wtcl"), test, yes, fin);
}

typedef struct {
  axs   a;
  noun axe;
} axarm;

noun axrelativearm(parser *p, noun spec, void *ctx) {
  axarm *c = ctx;
  return axrelative(p, c->a, c->axe, spec);
}

noun axrelative(parser *p, axs a, noun axe, noun mod) {
  for (;;) {
    noun m = tl(mod);
    noun fetch = C2(nul, axe);
    if (tagis(mod, "base")) return axdecorate(p, a, axbasic(p, axclear(a), axe, m));
    if (tagis(mod, "dbug")) {
      a.bug = C2(hd(m), a.bug);
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "leaf")) {
      noun r = C3(K("wtgr"), C3(K("dtts"), fetch, C3(K("rock"), nul, tl(m))),
                   C3(K("rock"), hd(m), tl(m)));
      return axdecorate(p, a, r);
    }
    if (tagis(mod, "make")) {
      mod = C2(K("bcmc"), axunfold(p, hd(m), tl(m)));
      continue;
    }
    if (tagis(mod, "like")) {
      mod = C2(K("bcmc"), axunreel(p, hd(m), tl(m)));
      continue;
    }
    if (tagis(mod, "loop")) return axdecorate(p, a, C3(K("cnhp"), C2(K("limb"), m), fetch));
    if (tagis(mod, "name")) {
      a.nut = C2(nul, C3(K("made"), hd(m), nul));
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "made")) {
      a.nut = C2(nul, C3(K("made"), hd(hd(m)), C2(nul, axpieces(p, tl(hd(m))))));
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "over")) {
      a.hay = hd(m);
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "bcbc")) {
      axarm c = {a, axe};
      c.a.dom = peg(p, D(3), a.dom);
      noun head = axrelative(p, c.a, axe, hd(m));
      noun arms = maprun(p, tl(m), axrelativearm, &c);
      if (!head || !arms) return 0;
      return C3(K("brkt"), head, mapnode(p->a, C2(nul, arms), nul, nul));
    }
    if (tagis(mod, "bcpm")) {
      noun raw = axrelative(p, a, axe, hd(m));
      if (!raw) return 0;
      noun s2 = C2(nul, D(2)), s3 = C2(nul, D(3)), s6 = C2(nul, D(6)), s14 = C2(nul, D(14));
      noun check = C3(K("wtbr"), C3(K("dtts"), s14, s2),
                       C2(C3(K("dtts"), s2, C3(K("cnhp"), s6, s2)), nul));
      return C3(K("tsls"), raw, C3(K("tsls"), C3(K("tsgr"), s3, tl(m)),
                C3(K("tsls"), C3(K("cnhp"), s2, s6), C3(K("wtgr"), check, s2))));
    }
    if (tagis(mod, "bcbr")) {
      noun raw = axrelative(p, a, axe, hd(m));
      if (!raw) return 0;
      noun s2 = C2(nul, D(2)), s3 = C2(nul, D(3));
      return C3(K("tsls"), raw, C3(K("wtgr"), C3(K("cnhp"), C3(K("tsgr"), s3, tl(m)), s2), s2));
    }
    if (tagis(mod, "bccb")) return axdecorate(p, a, axhome(p, a, m));
    if (tagis(mod, "bccn")) {
      noun r = axswitch(p, a, axe, hd(m), tl(m));
      return r ? axdecorate(p, a, r) : 0;
    }
    if (tagis(mod, "bccl")) {
      // each tail is its own tuple, decorated again
      if (isatom(tl(m))) {
        noun r = axrelative(p, axclear(a), axe, hd(m));
        return r ? axdecorate(p, a, r) : 0;
      }
      noun h = axrelative(p, axclear(a), peg(p, axe, D(2)), hd(m));
      noun t = axrelative(p, a, peg(p, axe, D(3)), C2(K("bccl"), tl(m)));
      return h && t ? axdecorate(p, a, C2(h, t)) : 0;
    }
    if (tagis(mod, "bcgl") || tagis(mod, "bcgr")) {
      noun r = axrelative(p, axclear(a), axe, tl(m));
      if (!r) return 0;
      noun test = C3(K("wtts"), C3(K("over"), C2(C2(YES, D(3)), nul), hd(m)),
                      C2(C2(YES, D(4)), nul));
      return C3(K("tsls"), r, C3(term(tagis(mod, "bcgl") ? "wtgl" : "wtgr"), test, C2(nul, D(2))));
    }
    if (tagis(mod, "bchp")) {
      noun fun = axfunction(p, axclear(a), hd(m), tl(m));
      if (iscell(a.def)) fun = C3(K("ktls"), fun, tl(a.def));
      return axdecorate(p, a, fun);
    }
    if (tagis(mod, "bckt")) {
      noun l = axrelative(p, axclear(a), axe, hd(m));
      noun r = axrelative(p, axclear(a), axe, tl(m));
      if (!l || !r) return 0;
      noun test = C2(K("dtwt"), C2(nul, peg(p, axe, D(2))));
      return axdecorate(p, a, C4(K("wtcl"), test, l, r));
    }
    if (tagis(mod, "bcmc")) {
      return axdecorate(p, a, C3(K("cncl"), axhome(p, a, m), C2(fetch, nul)));
    }
    if (tagis(mod, "bcsg")) {
      a.def = C2(nul, C3(K("kthp"), tl(m), hd(m)));
      mod = tl(m);
      continue;
    }
    if (tagis(mod, "bcwt")) {
      noun r = axchoice(p, a, axe, hd(m), tl(m));
      return r ? axdecorate(p, a, r) : 0;
    }
    if (tagis(mod, "bcts")) {
      noun r = axrelative(p, a, axe, tl(m));
      return r ? C3(K("ktts"), hd(m), r) : 0;
    }
    if (tagis(mod, "bcpt")) {
      noun l = axrelative(p, axclear(a), axe, tl(m));
      noun r = axrelative(p, axclear(a), axe, hd(m));
      if (!l || !r) return 0;
      return axdecorate(p, a, C4(K("wtcl"), C2(K("dtwt"), fetch), l, r));
    }
    if (tagis(mod, "bcls")) {
      noun r = axrelative(p, a, axe, tl(m));
      return r ? C3(K("note"), C2(K("know"), hd(m)), r) : 0;
    }
    if (tagis(mod, "bcdt")) return axdecorate(p, a, axhome(p, a, axinterface(p, a, "gold", hd(m), tl(m))));
    if (tagis(mod, "bcfs")) return axdecorate(p, a, axhome(p, a, axinterface(p, a, "iron", hd(m), tl(m))));
    if (tagis(mod, "bczp")) return axdecorate(p, a, axhome(p, a, axinterface(p, a, "lead", hd(m), tl(m))));
    if (tagis(mod, "bctc")) return axdecorate(p, a, axhome(p, a, axinterface(p, a, "zinc", hd(m), tl(m))));
    return 0;
  }
}

axs axinit(parser *p) {
  axs a = {D(1), nul, nul, nul, nul, nul};
  return a;
}

noun axfactory(parser *p, axs a, noun mod) {
  for (;;) {
    if (tagis(mod, "dbug")) {
      a.bug = C2(hd(tl(mod)), a.bug);
      mod = tl(tl(mod));
      continue;
    }
    if (tagis(mod, "bcsg")) {
      noun m = tl(mod);
      a.def = C2(nul, C3(K("kthp"), tl(m), hd(m)));
      mod = tl(m);
      continue;
    }
    break;
  }
  noun m = tl(mod);
  if (isatom(a.def)) {
    noun r = 0;
    if (tagis(mod, "bcmc")) r = m;
    if (tagis(mod, "like")) r = axunreel(p, hd(m), tl(m));
    if (tagis(mod, "loop")) r = C2(K("limb"), m);
    if (tagis(mod, "make")) r = axunfold(p, hd(m), tl(m));
    if (r) return axdecorate(p, a, axhome(p, a, r));
  }
  noun s = axspore(p, a, mod);
  noun r = axrelative(p, axdescend(p, a, 7), D(6), mod);
  if (!s || !r) return 0;
  noun eq = C3(K("dtts"), C2(nul, D(14)), C2(nul, D(2)));
  return C3(K("brcl"), C2(K("ktsg"), s), C3(K("tsls"), r, C3(K("tsls"), eq, C2(nul, D(6)))));
}

noun hoonfactory(parser *p, noun mod) {
  return axfactory(p, axinit(p), mod);
}

noun hoonexample(parser *p, noun mod) {
  return axexample(p, axinit(p), mod);
}

// split a hoon producing a cell, as +half:ap
b32 hoonhalf(parser *p, noun gen, noun *a, noun *b) {
  for (;;) {
    noun g = tl(gen);
    if (iscell(hd(gen))) { *a = hd(gen); *b = g; return 1; }
    if (tagis(gen, "dbug")) { gen = tl(g); continue; }
    if (tagis(gen, "clcb")) { *a = tl(g); *b = hd(g); return 1; }
    if (tagis(gen, "clhp")) { *a = hd(g); *b = tl(g); return 1; }
    if (tagis(gen, "clkt")) {
      *a = hd(g);
      *b = C2(K("clls"), tl(g));
      return 1;
    }
    if (tagis(gen, "clsg")) {
      if (isatom(g)) return 0;
      *a = hd(g);
      *b = C2(K("clsg"), tl(g));
      return 1;
    }
    if (tagis(gen, "cltr")) {
      if (isatom(g)) return 0;
      if (isatom(tl(g))) { gen = hd(g); continue; }
      *a = hd(g);
      *b = C2(K("cltr"), tl(g));
      return 1;
    }
    return 0;
  }
}

// apply a skin to a hoon, as +grip:ap
noun grip(parser *p, noun gen, noun skin, noun rel) {
  if (isatom(skin)) return C3(K("tsgl"), C2(K("tune"), skin), gen);
  noun s = tl(skin);
  if (tagis(skin, "base")) return atomeqc(s, "noun") ? gen : C3(K("kthp"), skin, gen);
  if (tagis(skin, "cell")) {
    noun a, b;
    if (hoonhalf(p, gen, &a, &b)) {
      return C2(grip(p, a, hd(s), rel), grip(p, b, tl(s), rel));
    }
    return C3(K("tsls"), gen, C2(grip(p, C2(nul, D(4)), hd(s), rel),
                                 grip(p, C2(nul, D(5)), tl(s), rel)));
  }
  if (tagis(skin, "dbug")) return C3(K("dbug"), hd(s), grip(p, gen, tl(s), rel));
  if (tagis(skin, "leaf")) return C3(K("kthp"), skin, gen);
  if (tagis(skin, "name")) return C3(K("tsgl"), C2(K("tune"), hd(s)), grip(p, gen, tl(s), rel));
  if (tagis(skin, "over")) return grip(p, gen, tl(s), weld(p, hd(s), rel));
  if (tagis(skin, "spec")) {
    noun spec = isatom(rel) ? hd(s) : C3(K("over"), rel, hd(s));
    return C3(K("kthp"), spec, grip(p, gen, tl(s), rel));
  }
  // %wash
  noun w = nul;
  for (u64 i = atomlow(s); i > 0; i--) w = C2(C3(NO, nul, nul), w);
  return C3(K("tsgl"), C2(K("wing"), w), gen);
}

noun knitpart(parser *p, noun i, noun res);

// from the end, in a loop, as a tape can be as long as the file
noun hoonknit(parser *p, noun l) {
  nouns xs = {0};
  for (; iscell(l); l = tl(l)) *push(&xs, p->a) = hd(l);
  noun res = C2(K("bust"), K("null"));
  for (size k = xs.len - 1; k >= 0; k--) res = knitpart(p, xs.data[k], res);
  return res;
}

// one part of a tape, a char or an interpolation, before the rest
noun knitpart(parser *p, noun i, noun res) {
  if (isatom(i)) return C2(C3(K("sand"), K("tD"), i), res);
  noun a = C3(K("ktts"), K("a"), C3(K("ktls"), C2(K("limb"), nul),
                                     C3(K("tsgr"), C2(K("limb"), K("v")), tl(i))));
  noun b = C3(K("ktts"), K("b"), res);
  noun s2 = C2(nul, D(2)), s3 = C2(nul, D(3));
  noun loop = C3(K("cnts"), C2(nul, nul),
                  C2(C2(C2(K("a"), nul), C3(K("tsgl"), s3, C2(K("limb"), K("a")))), nul));
  noun body = C2(K("brhp"), C4(K("wtpt"), C2(K("a"), nul), C2(K("limb"), K("b")),
                                C2(C3(K("tsgl"), s2, C2(K("limb"), K("a"))), loop)));
  return C3(K("tsls"), C2(a, b), body);
}

noun brcbarm(parser *p, noun hoon, void *ctx) {
  noun q = *(noun *)ctx;
  nouns xs = {0};
  for (; iscell(q); q = tl(q)) *push(&xs, p->a) = hd(q);
  for (size i = xs.len - 1; i >= 0; i--) {
    hoon = C4(K("tstr"), C2(hd(xs.data[i]), nul), tl(xs.data[i]), hoon);
  }
  return hoon;
}

noun brcbtome(parser *p, noun tome, void *ctx) {
  return maprun(p, tome, brcbarm, ctx);
}

noun hoonopenx(parser *p, noun gen);

// desugar one layer, as +open:ap; returns gen itself if primitive
nounmap opens;

// kept by hoon, as +play, +mint and +mull open the same ones over and over
noun hoonopen(parser *p, noun gen) {
  noun r = nounmapget(&opens, gen);
  if (!r && (r = hoonopenx(p, gen)) && !loose.on) nounmapput(&opens, gen, r);
  return r;
}

// [~ 2] and [~ 3], made once
noun s2, s3;

// nouns made from here are loose, long atoms in a, until looseend. Only
// loose nouns are made: a heap that's empty stays so
void loosebegin(arena *a) {
  if (!s2) {
    s2 = cons(a, nul, atomu64(a, 2));
    s3 = cons(a, nul, atomu64(a, 3));
  }
}

void looseend(void) {
}

noun hoonopenx(parser *p, noun gen) {
  PROF(open);
  noun g = tl(gen);
  if (iscell(hd(gen))) return gen;
  if (atomis(hd(gen), 0)) return C3(K("cnts"), C2(C2(YES, g), nul), nul);
  noun tag = hd(gen);
  // made once: every noun is made once
  if (!s2) {
    s2 = C2(nul, D(2));
    s3 = C2(nul, D(3));
  }
  // the tag as a word, to compare against words of literals
  u64 tw = atomfits(tag) ? atomlow(tag) : 0;
  #define IS(t) (tw == TW(t))
  #define P hd(g)
  #define Q hd(tl(g))
  #define QQ tl(g)
  #define R hd(tl(tl(g)))
  #define RR tl(tl(g))
  #define SS tl(tl(tl(g)))
  if (IS("base")) return hoonfactory(p, gen);
  if (IS("bust")) return hoonexample(p, C2(K("base"), g));
  if (IS("ktcl")) return hoonfactory(p, g);
  if (IS("dbug")) return QQ;
  if (IS("eror")) {
    errnew(errtext);
    hcerr.text = g;
    return 0;
  }
  if (IS("knit")) {
    noun top = C3(K("ktls"), C2(K("brhp"), C4(K("wtcl"), C2(K("bust"), K("flag")),
                                                C2(K("bust"), K("null")),
                                                C2(C3(K("ktts"), K("i"), C3(K("sand"), K("tD"), nul)),
                                                   C3(K("ktts"), K("t"), C2(K("limb"), nul))))),
                   hoonknit(p, g));
    return C3(K("tsgr"), C3(K("ktts"), K("v"), C2(nul, D(1))), C2(K("brhp"), top));
  }
  if (IS("leaf")) return hoonfactory(p, gen);
  if (IS("limb")) return C3(K("cnts"), C2(g, nul), nul);
  if (IS("tell") || IS("yell")) {
    return C4(K("cncl"), C2(K("limb"), term(IS("tell") ? "noah" : "cain")),
              C2(K("zpgr"), C2(K("cltr"), g)), nul);
  }
  if (IS("wing")) return C3(K("cnts"), g, nul);
  if (IS("note")) return QQ;
  if (IS("brbc")) {
    nouns xs = {0};
    noun tar = C2(K("base"), K("noun"));
    for (noun l = P; iscell(l); l = tl(l)) {
      *push(&xs, p->a) = C3(K("bcts"), hd(l), C3(K("bcsg"), tar, C3(K("bchp"), tar, tar)));
    }
    if (!xs.len) return 0;
    return C3(K("brtr"), C2(K("bccl"), nounslist(p, &xs)), C2(K("ktcl"), QQ));
  }
  if (IS("brcb")) {
    noun q = Q;
    noun arms = maprun(p, RR, brcbtome, &q);
    return C3(K("tsls"), C2(K("kttr"), P), C3(K("brcn"), nul, arms));
  }
  if (IS("brcl")) return C3(K("tsls"), P, C2(K("brdt"), QQ));
  if (IS("brdt")) {
    noun arms = mapnode(p->a, C2(nul, g), nul, nul);
    return C3(K("brcn"), nul, mapnode(p->a, C2(nul, arms), nul, nul));
  }
  if (IS("brkt")) {
    noun zil = mapget(QQ, nul);
    noun dom = zil ? mapput(p->a, QQ, nul, mapput(p->a, zil, nul, P))
                    : mapput(p->a, QQ, nul, mapnode(p->a, C2(nul, P), nul, nul));
    return C3(K("tsgl"), C2(K("limb"), nul), C3(K("brcn"), nul, dom));
  }
  if (IS("brhp")) return C3(K("tsgl"), C2(K("limb"), nul), C2(K("brdt"), g));
  if (IS("brsg")) return C2(K("ktbr"), C3(K("brts"), P, QQ));
  if (IS("brtr")) {
    noun arms = mapnode(p->a, C2(nul, QQ), nul, nul);
    return C3(K("tsls"), C2(K("kttr"), P),
              C3(K("brpt"), nul, mapnode(p->a, C2(nul, arms), nul, nul)));
  }
  if (IS("brts")) {
    noun arms = mapnode(p->a, C2(nul, QQ), nul, nul);
    return C4(K("brcb"), P, nul, mapnode(p->a, C2(nul, arms), nul, nul));
  }
  if (IS("brwt")) return C3(K("ktwt"), K("brdt"), g);
  if (IS("clkt")) return C4(P, Q, R, SS);
  if (IS("clls")) return C3(P, Q, RR);
  if (IS("clcb")) return C2(QQ, P);
  if (IS("clhp")) return C2(P, QQ);
  if (IS("clsg")) {
    nouns xs = {0};
    for (noun l = g; iscell(l); l = tl(l)) *push(&xs, p->a) = hd(l);
    noun r = C3(K("rock"), K("n"), nul);
    for (size i = xs.len - 1; i >= 0; i--) r = C2(xs.data[i], r);
    return r;
  }
  if (IS("cltr")) {
    if (isatom(g)) return C2(K("zpzp"), nul);
    nouns xs = {0};
    for (noun l = g; iscell(l); l = tl(l)) *push(&xs, p->a) = hd(l);
    noun r = xs.data[xs.len-1];
    for (size i = xs.len - 2; i >= 0; i--) r = C2(xs.data[i], r);
    return r;
  }
  if (IS("kttr")) {
    noun e = hoonexample(p, g);
    return e ? C2(K("ktsg"), e) : 0;
  }
  if (IS("cncb")) return C3(K("ktls"), C2(K("wing"), P), C3(K("cnts"), P, QQ));
  if (IS("cndt")) return C3(K("cncl"), QQ, C2(P, nul));
  if (IS("cnkt")) return C3(K("cncl"), P, C4(Q, R, SS, nul));
  if (IS("cnls")) return C3(K("cncl"), P, C3(Q, RR, nul));
  if (IS("cnhp")) return C3(K("cncl"), P, C2(QQ, nul));
  if (IS("cncl")) return C4(K("cnsg"), C2(nul, nul), P, QQ);
  if (IS("cnsg")) {
    nouns xs = {0};
    noun axe = D(6);
    for (noun r = RR; iscell(r); r = tl(r)) {
      noun ax = iscell(tl(r)) ? peg(p, axe, D(2)) : axe;
      noun w = C3(C3(NO, nul, nul), C2(YES, ax), nul);
      *push(&xs, p->a) = C2(w, hd(r));
      axe = peg(p, axe, D(3));
    }
    return C4(K("cntr"), P, Q, nounslist(p, &xs));
  }
  if (IS("cntr")) {
    if (isatom(RR)) return C3(K("tsgr"), Q, C2(K("wing"), P));
    nouns xs = {0};
    for (noun r = RR; iscell(r); r = tl(r)) {
      *push(&xs, p->a) = C2(hd(hd(r)), C3(K("tsgr"), s3, tl(hd(r))));
    }
    return C3(K("tsls"), Q, C3(K("cnts"), weld(p, P, C2(C2(YES, D(2)), nul)), nounslist(p, &xs)));
  }
  if (IS("ktdt")) return C3(K("ktls"), C4(K("cncl"), P, QQ, nul), QQ);
  if (IS("kthp")) {
    noun e = hoonexample(p, P);
    return e ? C3(K("ktls"), e, QQ) : 0;
  }
  if (IS("ktts")) return grip(p, QQ, P, nul);
  if (IS("sgbr")) {
    noun fek = P;
    while (tagis(fek, "dbug")) fek = tl(tl(fek));
    noun mean;
    if (tagis(fek, "rock") && atomeqc(hd(tl(fek)), "tas") && isatom(tl(tl(fek)))) {
      mean = C3(K("rock"), K("tas"), tl(tl(fek)));
    } else {
      mean = C2(K("brdt"), C4(K("cncl"), C2(K("limb"), K("cain")),
                              C2(K("zpgr"), C3(K("tsgr"), s3, P)), nul));
    }
    return C3(K("sggr"), C2(K("mean"), mean), QQ);
  }
  if (IS("sgcb")) return C3(K("sggr"), C2(K("mean"), C2(K("brdt"), P)), QQ);
  if (IS("sgcn")) {
    nouns xs = {0};
    for (noun r = hd(RR); iscell(r); r = tl(r)) {
      *push(&xs, p->a) = C2(C3(K("rock"), nul, hd(hd(r))), C2(K("zpts"), tl(hd(r))));
    }
    noun fast = C4(K("clls"), C3(K("rock"), nul, P), C2(K("zpts"), Q), C2(K("clsg"), nounslist(p, &xs)));
    return C3(K("sggl"), C2(K("fast"), fast), SS);
  }
  if (IS("sgfs")) return C5(K("sgcn"), P, C2(nul, D(7)), nul, QQ);
  if (IS("sggl")) return C3(K("tsgl"), C3(K("sggr"), P, C2(nul, D(1))), QQ);
  if (IS("sgbc")) return C3(K("sggr"), C2(K("live"), C3(K("rock"), nul, P)), QQ);
  if (IS("sgls")) return C3(K("sggr"), C3(K("memo"), K("rock"), C2(nul, P)), QQ);
  if (IS("sgpm")) {
    noun slog = C3(K("slog"), C3(K("sand"), nul, P),
                    C4(K("cncl"), C2(K("limb"), K("cain")), C2(K("zpgr"), Q), nul));
    return C3(K("sggr"), slog, RR);
  }
  if (IS("sgts")) return C3(K("sggr"), C2(K("germ"), P), QQ);
  if (IS("sgwt")) {
    noun slog = C5(K("sgpm"), P, R, K("bust"), K("null"));
    return C3(K("tsls"), C4(K("wtcl"), Q, slog, C2(K("bust"), K("null"))),
              C3(K("tsgr"), s3, SS));
  }
  if (IS("mcts")) {
    if (isatom(g)) return C2(K("bust"), K("null"));
    noun i = hd(g);
    noun rest = C2(K("mcts"), tl(g));
    noun next = hoonopen(p, rest);
    if (!next) return 0;
    if (iscell(hd(i))) return C2(C2(K("xray"), i), next);
    if (atomeqc(hd(i), "manx")) return C2(tl(i), next);
    if (atomeqc(hd(i), "tape")) return C2(C2(K("mcfs"), tl(i)), next);
    if (atomeqc(hd(i), "call")) return C3(K("cncl"), tl(i), C2(next, nul));
    // %marl
    noun sug = C2(C2(YES, D(12)), nul);
    noun arm = C4(K("wtsg"), sug,
                   C3(K("cnts"), sug, C2(C2(C2(C2(YES, D(1)), nul), C2(nul, D(13))), nul)),
                   C3(K("cnts"), sug, C2(C2(C2(C2(YES, D(3)), nul),
                                            C3(K("cnts"), C2(nul, nul),
                                               C2(C2(sug, C2(nul, D(25))), nul))), nul)));
    noun arms = mapnode(p->a, C2(nul, arm), nul, nul);
    noun core = C3(K("tsbr"), C2(K("base"), K("cell")),
                    C3(K("brpt"), nul, mapnode(p->a, C2(nul, arms), nul, nul)));
    return C3(K("cndt"), C2(tl(i), next), core);
  }
  if (IS("mccl")) {
    noun q = QQ;
    if (isatom(q)) return C2(K("zpzp"), nul);
    if (isatom(tl(q))) return hd(q);
    nouns xs = {0};
    for (noun l = q; iscell(l); l = tl(l)) *push(&xs, p->a) = hd(l);
    noun r = C3(K("tsgr"), s3, xs.data[xs.len-1]);
    for (size i = xs.len - 2; i >= 0; i--) {
      r = C5(K("cncl"), s2, C3(K("tsgr"), s3, xs.data[i]), r, nul);
    }
    return C3(K("tsls"), P, r);
  }
  if (IS("mcfs")) {
    noun zoy = C3(K("rock"), K("ta"), nul);
    return C3(K("clsg"), C2(zoy, C3(K("clsg"), C2(zoy, g), nul)), nul);
  }
  if (IS("mcgl")) {
    // [%cnls [%cnhp q ktcl+p] r [%brts p [%tsgr $+3 s]]]
    return C4(K("cnls"), C3(K("cnhp"), Q, C2(K("ktcl"), P)), R,
              C3(K("brts"), P, C3(K("tsgr"), s3, SS)));
  }
  if (IS("mcsg")) {
    noun q = QQ;
    if (isatom(q)) return 0;
    nouns xs = {0};
    for (noun l = q; iscell(l); l = tl(l)) *push(&xs, p->a) = hd(l);
    noun v = C2(K("limb"), K("v"));
    noun r = C3(K("tsgr"), v, xs.data[xs.len-1]);
    for (size i = xs.len - 2; i >= 0; i--) {
      noun c = C3(K("tsgl"), C2(K("wing"), C3(C3(NO, nul, nul), C2(YES, D(6)), nul)),
                   C2(K("limb"), K("b")));
      noun call = C4(K("cnls"), C3(K("tsgr"), v, P),
                      C4(K("cncl"), C2(K("limb"), K("b")), C2(K("limb"), K("c")), nul),
                      C3(K("cnts"), C2(K("a"), nul),
                         C2(C2(C3(C3(NO, nul, nul), C2(YES, D(6)), nul), C2(K("limb"), K("c"))), nul)));
      r = C3(K("tsls"), C3(K("ktts"), K("a"), r),
             C3(K("tsls"), C3(K("ktts"), K("b"), C3(K("tsgr"), v, xs.data[i])),
                C3(K("tsls"), C3(K("ktts"), K("c"), c), C2(K("brdt"), call))));
    }
    return C3(K("tsgr"), C3(K("ktts"), K("v"), C2(nul, D(1))), r);
  }
  if (IS("mcmc")) {
    noun f = hoonfactory(p, P);
    return f ? C3(K("cnhp"), f, QQ) : 0;
  }
  if (IS("tsbr")) return C3(K("tsls"), C2(K("kttr"), P), QQ);
  if (IS("tstr")) {
    noun pp = P;
    noun val = isatom(tl(pp)) ? Q : C3(K("kthp"), tl(tl(pp)), Q);
    noun tune = C2(mapnode(p->a, C2(hd(pp), C2(nul, val)), nul, nul), nul);
    return C3(K("tsgl"), RR, C2(K("tune"), tune));
  }
  if (IS("tscl")) return C3(K("tsgr"), C3(K("cncb"), C2(C2(YES, D(1)), nul), P), QQ);
  if (IS("tsfs")) return C3(K("tsls"), C3(K("ktts"), P, Q), RR);
  if (IS("tsmc")) return C4(K("tsfs"), P, RR, Q);
  if (IS("tsdt")) {
    return C3(K("tsgr"), C3(K("cncb"), C2(C2(YES, D(1)), nul), C2(C2(P, Q), nul)), RR);
  }
  if (IS("tswt")) return C4(K("tsdt"), P, C4(K("wtcl"), Q, R, C2(K("wing"), P)), SS);
  if (IS("tskt")) {
    noun wuy = weld(p, Q, C2(K("v"), nul));
    noun v = C2(K("limb"), K("v"));
    noun a = C3(K("ktts"), K("a"), C3(K("tsgr"), v, C3(K("ktcb"),
                 C2(K("kttr"), C2(K("base"), K("cell"))), R)));
    noun pair = C2(C3(K("ktts"), C3(K("over"), C2(K("v"), nul), P),
                       C3(K("tsgl"), s2, C2(K("limb"), K("a")))), v);
    return C3(K("tsgr"), C3(K("ktts"), K("v"), C2(nul, D(1))),
              C3(K("tsls"), a, C4(K("tsdt"), wuy, C3(K("tsgl"), s3, C2(K("limb"), K("a"))),
                                  C3(K("tsgr"), pair, SS))));
  }
  if (IS("tsgl")) return C3(K("tsgr"), QQ, P);
  if (IS("tsls")) return C3(K("tsgr"), C2(P, C2(nul, D(1))), QQ);
  if (IS("tshp")) return C3(K("tsls"), QQ, P);
  if (IS("tssg")) {
    if (isatom(g)) return C2(nul, D(1));
    if (isatom(tl(g))) return hd(g);
    return C3(K("tsgr"), hd(g), hoonopen(p, C2(K("tssg"), tl(g))));
  }
  if (IS("wtbr")) {
    if (isatom(g)) return C3(K("rock"), K("f"), NO);
    return C4(K("wtcl"), hd(g), C3(K("rock"), K("f"), YES), hoonopen(p, C2(K("wtbr"), tl(g))));
  }
  if (IS("wtdt")) return C4(K("wtcl"), P, RR, Q);
  if (IS("wtgl")) return C4(K("wtcl"), P, C2(K("zpzp"), nul), QQ);
  if (IS("wtgr")) return C4(K("wtcl"), P, QQ, C2(K("zpzp"), nul));
  if (IS("wtkt")) return C4(K("wtcl"), C3(K("wtts"), C3(K("base"), K("atom"), nul), P), RR, Q);
  if (IS("wthp")) {
    noun q = QQ;
    if (isatom(q)) return C2(K("lost"), C2(K("wing"), P));
    return C4(K("wtcl"), C3(K("wtts"), hd(hd(q)), P), tl(hd(q)),
              hoonopen(p, C3(K("wthp"), P, tl(q))));
  }
  if (IS("wtls")) {
    noun opt = weld(p, RR, C2(C2(C2(K("base"), K("noun")), Q), nul));
    return C3(K("wthp"), P, opt);
  }
  if (IS("wtpm")) {
    if (isatom(g)) return C3(K("rock"), K("f"), YES);
    return C4(K("wtcl"), hd(g), hoonopen(p, C2(K("wtpm"), tl(g))), C3(K("rock"), K("f"), NO));
  }
  if (IS("xray")) {
    noun x = g;
    noun mane = hd(hd(x));
    noun mart = tl(hd(x));
    #define OPENMANE(a) (isatom(a) ? C3(K("rock"), K("tas"), a) \
      : C2(C3(K("rock"), K("tas"), hd(a)), C3(K("rock"), K("tas"), tl(a))))
    nouns xs = {0};
    for (noun l = mart; iscell(l); l = tl(l)) {
      *push(&xs, p->a) = C2(OPENMANE(hd(hd(l))), C2(K("knit"), tl(hd(l))));
    }
    #undef OPENMANE
    noun head = C2(isatom(mane) ? C3(K("rock"), K("tas"), mane)
                    : C2(C3(K("rock"), K("tas"), hd(mane)), C3(K("rock"), K("tas"), tl(mane))),
                    C2(K("clsg"), nounslist(p, &xs)));
    return C2(head, C2(K("mcts"), tl(x)));
  }
  if (IS("wtpt")) return C4(K("wtcl"), C3(K("wtts"), C3(K("base"), K("atom"), nul), P), Q, RR);
  if (IS("wtsg")) return C4(K("wtcl"), C3(K("wtts"), C2(K("base"), K("null")), P), Q, RR);
  if (IS("wtts")) {
    noun e = hoonexample(p, P);
    return e ? C3(K("fits"), e, QQ) : 0;
  }
  if (IS("wtzp")) return C4(K("wtcl"), g, C3(K("rock"), K("f"), NO), C3(K("rock"), K("f"), YES));
  if (IS("zpgr")) {
    noun abel = C2(K("kttr"), C3(K("bcmc"), K("limb"), K("abel")));
    return C4(K("cncl"), C2(K("limb"), K("onan")), C3(K("zpmc"), abel, g), nul);
  }
  if (IS("zpwt")) {
    noun v = P;
    b32 ok = isatom(v) ? atomcmp(D(HOON_VERSION), v) <= 0
                       : atomcmp(D(HOON_VERSION), hd(v)) <= 0 && atomcmp(D(HOON_VERSION), tl(v)) >= 0;
    return ok ? QQ : 0;
  }
  #undef IS
  #undef P
  #undef Q
  #undef QQ
  #undef R
  #undef RR
  #undef SS
  return gen;
}

// hoon to skin, as +flay:ap
noun flay(parser *p, noun gen) {
  for (;;) {
    if (iscell(hd(gen))) {
      noun x = flay(p, hd(gen));
      if (!x) return 0;
      noun y = flay(p, tl(gen));
      if (!y) return 0;
      return C3(K("cell"), x, y);
    }
    noun a = tl(gen);
    if (tagis(gen, "base")) return gen;
    if (tagis(gen, "rock")) {
      if (isatom(tl(a))) return C3(K("leaf"), hd(a), tl(a));
      return 0;
    }
    if (tagis(gen, "cnts") && iscell(hd(a)) && isatom(hd(hd(a)))
        && atomis(tl(hd(a)), 0) && atomis(tl(a), 0)) {
      return hd(hd(a));
    }
    if (tagis(gen, "tsgr")) {
      noun w = reek(p, hd(a));
      if (!w) return 0;
      noun s = flay(p, tl(a));
      if (!s) return 0;
      return C3(K("over"), w, s);
    }
    if (tagis(gen, "limb") && isatom(a)) return a;
    if (tagis(gen, "wing")) {
      if (iscell(a) && isatom(hd(a)) && atomis(tl(a), 0)) return hd(a);
      for (noun l = a; iscell(l); l = tl(l)) {
        noun i = hd(l);
        if (!(iscell(i) && atomis(hd(i), 1) && iscell(tl(i))
              && atomis(hd(tl(i)), 0) && atomis(tl(tl(i)), 0))) {
          return 0;
        }
      }
      return C2(K("wash"), nul);
    }
    if (tagis(gen, "kttr")) return C3(K("spec"), a, C2(K("base"), K("noun")));
    if (tagis(gen, "ktts")) {
      noun s = flay(p, tl(a));
      if (!s) return 0;
      noun n = hd(a);
      if (isatom(n)) return C3(K("name"), n, s);
      if (tagis(n, "name") && isatom(hd(tl(n)))
          && nouneq(tl(tl(n)), C2(K("base"), K("noun")))) {
        return C3(K("name"), hd(tl(n)), s);
      }
      return 0;
    }
    noun o = hoonopen(p, gen);
    if (!o || nouneq(o, gen)) return 0;
    gen = o;
  }
}

// the name of a hoon, as +name:ap
noun hoonname(parser *p, noun gen) {
  for (;;) {
    if (tagis(gen, "wing")) {
      noun w = tl(gen);
      if (isatom(w)) return 0;
      noun i = hd(w);
      if (iscell(i)) {
        if (atomis(hd(i), 0)) return 0;
        noun u = tl(tl(i));
        return isatom(u) ? 0 : tl(u);
      }
      return i;
    }
    if (tagis(gen, "limb")) return tl(gen);
    if (tagis(gen, "dbug")) {
      gen = tl(tl(gen));
      continue;
    }
    if (tagis(gen, "tsgl")) {
      gen = hd(tl(gen));
      continue;
    }
    if (tagis(gen, "tsgr")) {
      gen = tl(tl(gen));
      continue;
    }
    return 0;
  }
}

// derive a face from a spec, as +autoname:ax
noun autoname(parser *p, noun mod) {
  for (;;) {
    noun a = tl(mod);
    if (tagis(mod, "base")) {
      if (!tagis(a, "atom")) return 0;
      return atomis(tl(a), 0) ? K("atom") : tl(a);
    }
    if (tagis(mod, "dbug") || tagis(mod, "made") || tagis(mod, "over")
        || tagis(mod, "name") || tagis(mod, "bcgl") || tagis(mod, "bcgr")
        || tagis(mod, "bckt") || tagis(mod, "bcls") || tagis(mod, "bcsg")
        || tagis(mod, "bcts") || tagis(mod, "bcpt")) {
      mod = tl(a);
      continue;
    }
    if (tagis(mod, "leaf")) return hd(a);
    if (tagis(mod, "loop")) return a;
    if (tagis(mod, "like")) {
      noun w = hd(a);
      if (isatom(w)) return 0;
      noun i = hd(w);
      if (iscell(i)) {
        if (atomis(hd(i), 0)) return 0;
        noun u = tl(tl(i));
        return isatom(u) ? 0 : tl(u);
      }
      return i;
    }
    if (tagis(mod, "make") || tagis(mod, "bccb") || tagis(mod, "bcmc")) {
      return hoonname(p, tagis(mod, "make") ? hd(a) : a);
    }
    if (tagis(mod, "bcbc") || tagis(mod, "bcbr") || tagis(mod, "bchp") || tagis(mod, "bcpm")) {
      mod = hd(a);
      continue;
    }
    if (tagis(mod, "bccl") || tagis(mod, "bccn") || tagis(mod, "bcwt")) {
      mod = hd(a);
      continue;
    }
    return 0;
  }
}

// tiki expansion, as +ah
noun ahblue(parser *p, noun tik, noun gen) {
  if (atomis(hd(tik), 1) && atomis(hd(tl(tik)), 0)) {
    return C3(K("tsgr"), C2(nul, D(3)), gen);
  }
  return gen;
}

noun ahteal(parser *p, noun tik, noun mod) {
  if (atomis(hd(tik), 0)) return mod;
  return C3(K("over"), C2(C2(YES, D(3)), nul), mod);
}

noun ahgray(parser *p, noun tik, noun gen) {
  noun u = hd(tl(tik));
  noun q = tl(tl(tik));
  if (atomis(hd(tik), 0)) {
    if (isatom(u)) return gen;
    return C4(K("tstr"), C2(tl(u), nul), C2(K("wing"), q), gen);
  }
  noun h = isatom(u) ? q : C3(K("ktts"), tl(u), q);
  return C3(K("tsls"), h, gen);
}

noun ahpuce(parser *p, noun tik) {
  noun u = hd(tl(tik));
  if (atomis(hd(tik), 0)) {
    if (isatom(u)) return tl(tl(tik));
    return C2(tl(u), nul);
  }
  return C2(C2(YES, D(2)), nul);
}

noun ahopts(parser *p, noun tik, noun opt) {
  nouns xs = {0};
  for (; iscell(opt); opt = tl(opt)) {
    *push(&xs, p->a) = C2(hd(hd(opt)), ahblue(p, tik, tl(hd(opt))));
  }
  return nounslist(p, &xs);
}

// Wings

noun peg(parser *p, noun a, noun b) {
  if (atomis(b, 1)) return a;
  if (!alen(b)) return b;   // no axis 0, left for its users to fail on
  size c = atombits(b);
  return atomadd(p->a, atomlsh(p->a, a, c - 1), atomend(p->a, b, c - 1));
}

noun ven(parser *p, size *pos) {
  nouns vs = {0};
  if (chr(p, pos, '-')) *push(&vs, p->a) = D(2);
  else if (chr(p, pos, '+')) *push(&vs, p->a) = D(3);
  else return 0;
  b32 hom = 0;
  for (;;) {
    u8 lo = hom ? '-' : '<';
    u8 hi = hom ? '+' : '>';
    if (chr(p, pos, lo)) *push(&vs, p->a) = D(2);
    else if (chr(p, pos, hi)) *push(&vs, p->a) = D(3);
    else break;
    hom = !hom;
  }
  noun acc = D(1);
  for (size i = vs.len - 1; i >= 1; i--) acc = peg(p, vs.data[i], acc);
  return peg(p, vs.data[0], acc);
}

// the axis of &n or |n, 2^(n+1) less 2 or 1. hoon builds it for any n,
// but a few digits must not make an atom of gigabytes, and no tuple is
// thousands deep: real code goes to 8
noun itemaxis(parser *p, noun n, u64 k) {
  if (alen(n) > 2 || atomlow(n) >= 1 << 12) return 0;
  return atomsub(p->a, atomlsh(p->a, D(1), (size)atomlow(n) + 1), D(k));
}

noun limb(parser *p, size *pos) {
  if (*pos >= p->len) {
    reach(p, *pos);
    return 0;
  }
  u8 c = p->buf[*pos];
  size s = *pos;
  noun v;
  switch (c) {
  case '$':
    chr(p, pos, '$');
    return nul;
  case ',':
    chr(p, pos, ',');
    return C3(NO, nul, nul);
  case '^': {
    size n = 0;
    while (chr(p, pos, '^')) n++;
    noun b = symbuc(p, pos);
    if (!b) return 0;
    return C3(NO, D((u64)n), C2(nul, b));
  }
  case '+':
    chr(p, pos, '+');
    if ((v = dim(p, pos))) return C2(YES, v);
    *pos = s;
    return (v = ven(p, pos)) ? C2(YES, v) : 0;
  case '-':
    return (v = ven(p, pos)) ? C2(YES, v) : 0;
  case '&':
    chr(p, pos, '&');
    return (v = dim(p, pos)) && (v = itemaxis(p, v, 2)) ? C2(YES, v) : 0;
  case '|':
    chr(p, pos, '|');
    return (v = dim(p, pos)) && (v = itemaxis(p, v, 1)) ? C2(YES, v) : 0;
  case '.':
    chr(p, pos, '.');
    return C2(YES, D(1));
  }
  if (c >= 'a' && c <= 'z') return sym(p, pos);
  reach(p, *pos);
  return 0;
}

noun ropex(parser *p, size *pos, b32 tol) {
  noun l = limb(p, pos);
  if (!l) return 0;
  size base = p->stk.len;
  *push(&p->stk, p->a) = l;
  for (;;) {
    size s = *pos;
    if (chr(p, pos, '.') && (l = limb(p, pos))) {
      *push(&p->stk, p->a) = l;
      continue;
    }
    *pos = s;
    return stklist(p, base);
  }
}

noun rope(parser *p, size *pos) {
  return ropex(p, pos, 0);
}

noun ropa(parser *p, size *pos, b32 tol) {
  return most(p, pos, 0, sepcol, roperule);
}

// Paths

noun hasp(parser *p, size *pos) {
  size s = *pos;
  noun r;
  if (chr(p, pos, '[') && (r = wide(p, pos)) && chr(p, pos, ']')) return r;
  *pos = s;
  if ((r = parens(p, pos, widerule))) return C2(K("cncl"), r);
  *pos = s;
  if (chr(p, pos, '$')) return C3(K("sand"), K("tas"), nul);
  *pos = s;
  if ((r = qut(p, pos))) return C3(K("sand"), K("t"), r);
  *pos = s;
  if ((r = nuck(p, pos))) {
    b32 tas = atomis(hd(r), 0) && atomeqc(hd(tl(r)), "tas");
    return C3(K("sand"), term(tas ? "tas" : "ta"), atombytes(p->a, p->buf + s, *pos - s));
  }
  return 0;
}

// tyke elements are ~ for = or [~ hoon]
noun gasp(parser *p, size *pos) {
  size s = *pos;
  size n1 = 0;
  while (chr(p, pos, '=')) n1++;
  noun h = hasp(p, pos);
  if (h) {
    size n2 = 0;
    while (chr(p, pos, '=')) n2++;
    noun r = nul;
    for (size i = 0; i < n2; i++) r = C2(nul, r);
    r = C2(C2(nul, h), r);
    for (size i = 0; i < n1; i++) r = C2(nul, r);
    return r;
  }
  *pos = s;
  size n = 0;
  while (chr(p, pos, '=')) n++;
  if (!n) return 0;
  noun r = nul;
  for (size i = 0; i < n; i++) r = C2(nul, r);
  return r;
}

noun limp(parser *p, size *pos) {
  size n = 0;
  while (chr(p, pos, '/')) n++;
  noun b = gasp(p, pos);
  if (!b) return 0;
  for (size i = 0; i < n; i++) b = C2(C2(nul, C3(K("sand"), K("tas"), nul)), b);
  return b;
}

noun gash(parser *p, size *pos) {
  size s = *pos;
  noun r = limp(p, pos);
  if (!r) {
    *pos = s;
    return nul;
  }
  for (;;) {
    size t = *pos;
    noun x;
    if (chr(p, pos, '/') && (x = limp(p, pos))) {
      r = weld(p, r, x);
      continue;
    }
    *pos = t;
    return r;
  }
}

// replace ~ entries in goo with successive elements of pag
noun poon(parser *p, noun pag, noun goo) {
  nouns xs = {0};
  for (; iscell(goo); goo = tl(goo)) {
    if (iscell(hd(goo))) {
      *push(&xs, p->a) = tl(hd(goo));
    } else {
      if (isatom(pag)) return 0;
      *push(&xs, p->a) = hd(pag);
    }
    if (iscell(pag)) pag = tl(pag);
  }
  return nounslist(p, &xs);
}

noun scag(parser *p, size n, noun l) {
  nouns xs = {0};
  for (; n && iscell(l); n--, l = tl(l)) *push(&xs, p->a) = hd(l);
  return nounslist(p, &xs);
}

noun slag(size n, noun l) {
  for (; n && iscell(l); n--) l = tl(l);
  return l;
}

// the parser's path as hoons, as +poof does with wer: what = in a path
// and % stand for
noun poof(parser *p) {
  noun wer = (p->root ? p->root : p)->wer;
  nouns xs = {0};
  for (noun l = wer ? wer : nul; iscell(l); l = tl(l)) *push(&xs, p->a) = C3(K("sand"), K("ta"), hd(l));
  return nounslist(p, &xs);
}

// a path from what was written and the parser's path, as +posh
noun posh(parser *p, noun pre, b32 haspof, size pofn, noun pofq) {
  noun wom = poof(p);
  noun yez = wom;
  if (pre) {
    yez = poon(p, wom, pre);
    if (!yez) return 0;
    if (haspof) yez = weld(p, yez, slag((size)lent(pre), wom));
  }
  if (!haspof) return yez;
  noun zey = flop(p, yez);
  noun moz = scag(p, pofn, zey);
  noun gul = slag(pofn, zey);
  noun zom = poon(p, flop(p, moz), pofq);
  if (!zom) return 0;
  return weld(p, flop(p, gul), zom);
}

b32 porc(parser *p, size *pos, size *n, noun *tyke) {
  *n = 0;
  while (chr(p, pos, '%')) (*n)++;
  if (!chr(p, pos, '/')) return 0;
  *tyke = gash(p, pos);
  return 1;
}

noun poor(parser *p, size *pos) {
  noun pre = gash(p, pos);
  size s = *pos;
  size n;
  noun q;
  if (chr(p, pos, '%') && porc(p, pos, &n, &q)) return posh(p, pre, 1, n, q);
  *pos = s;
  return posh(p, pre, 0, 0, 0);
}

noun rood(parser *p, size *pos) {
  if (!chr(p, pos, '/')) return 0;
  noun r = poor(p, pos);
  return r ? C2(K("clsg"), r) : 0;
}

// a coin valid as an iota, as (soft iota)
b32 isiota(noun d) {
  char *ok[] = {"ub", "uc", "ud", "ui", "ux", "uv", "uw", "sb", "sc", "sd",
                "si", "sx", "sv", "sw", "da", "dr", "f", "n", "if", "is",
                "t", "ta", "p", "q", "rs", "rd", "rh", "rq"};
  for (i32 i = 0; i < (i32)countof(ok); i++) {
    if (atomeqc(hd(d), ok[i])) {
      if (atomeqc(hd(d), "f")) return atomis(tl(d), 0) || atomis(tl(d), 1);
      if (atomeqc(hd(d), "n")) return atomis(tl(d), 0);
      return 1;
    }
  }
  return 0;
}

noun stemiota(parser *p, noun iota) {
  if (isatom(iota)) return C3(K("rock"), K("tas"), iota);
  if (atomeqc(hd(iota), "hoon")) return tl(iota);
  return C3(K("clhp"), C3(K("rock"), K("tas"), hd(iota)), C2(K("sand"), iota));
}

noun stem(parser *p, size *pos) {
  size s = *pos;
  if (*pos >= p->len) return stemiota(p, nul);
  u8 c = p->buf[*pos];
  noun r;
  if (c >= 'a' && c <= 'z') {
    noun n = sym(p, pos);
    size t = *pos;
    if (chr(p, pos, '+')) {
      size u = *pos;
      if ((r = parens(p, pos, widerule))) {
        return C3(K("clhp"), C3(K("rock"), K("tas"), n), C2(K("cncl"), r));
      }
      *pos = u;
      if (chr(p, pos, '[') && (r = wide(p, pos)) && chr(p, pos, ']')) {
        return C3(K("clhp"), C3(K("rock"), K("tas"), n), r);
      }
    }
    *pos = t;
    return stemiota(p, n);
  }
  if (c == '$') {
    chr(p, pos, '$');
    return stemiota(p, nul);
  }
  if (c >= '0' && c <= '9') {
    if ((r = bisk(p, pos)) && isiota(r)) return stemiota(p, r);
    *pos = s;
    return stemiota(p, nul);
  }
  if (c == '-') {
    if ((r = tash(p, pos)) && isiota(r)) return stemiota(p, r);
    *pos = s;
    return stemiota(p, nul);
  }
  if (c == '.') {
    chr(p, pos, '.');
    if ((r = zust(p, pos))) return stemiota(p, r);
    *pos = s;
    return stemiota(p, nul);
  }
  if (c == '~') {
    chr(p, pos, '~');
    size t = *pos;
    r = crub(p, pos);
    if (!r) {
      *pos = t;
      r = C2(K("n"), nul);
    }
    if (isiota(r)) return stemiota(p, r);
    *pos = s;
    return stemiota(p, nul);
  }
  if (c == '\'') {
    if ((r = qut(p, pos))) return stemiota(p, C2(K("t"), r));
    *pos = s;
    return stemiota(p, nul);
  }
  if (c == '[') {
    if (chr(p, pos, '[') && (r = wide(p, pos)) && chr(p, pos, ']')) return r;
    *pos = s;
    return stemiota(p, nul);
  }
  if (c == '(') {
    if ((r = parens(p, pos, widerule))) return C2(K("cncl"), r);
    *pos = s;
    return stemiota(p, nul);
  }
  reach(p, *pos);
  return stemiota(p, nul);
}

noun reed(parser *p, size *pos) {
  if (!chr(p, pos, '/')) return 0;
  nouns xs = {0};
  *push(&xs, p->a) = stem(p, pos);
  for (;;) {
    size s = *pos;
    if (chr(p, pos, '/')) {
      *push(&xs, p->a) = stem(p, pos);
      continue;
    }
    *pos = s;
    break;
  }
  return C2(K("clsg"), nounslist(p, &xs));
}

// Tapes: "text {interpolated}" as lists of woofs

noun sump(parser *p, size *pos) {
  if (!chr(p, pos, '{')) return 0;
  noun r = most(p, pos, 0, sepace, widerule);
  if (!r || !chr(p, pos, '}')) return 0;
  return C2(K("cltr"), r);
}

b32 soilope(parser *p, size *pos) { return jest(p, pos, "\"\"\""); }

noun soilblock(parser *p, size *pos) {
  nouns xs = {0};
  for (;;) {
    size s = *pos;
    i32 v;
    u8 c;
    noun h;
    if (chr(p, pos, '\\')) {
      if (chr(p, pos, '\\')) { *push(&xs, p->a) = D('\\'); continue; }
      if (chr(p, pos, '{')) { *push(&xs, p->a) = D('{'); continue; }
      if (bix(p, pos, &v)) { *push(&xs, p->a) = D((u64)v); continue; }
    }
    *pos = s;
    farmark m = farget(p);
    if (!lookahead(p, s, chr(p, pos, '\\'), m)) {
      *pos = s;
      farmark n = farget(p);
      if (!lookahead(p, s, chr(p, pos, '{'), n)) {
        *pos = s;
        if (prn(p, pos, &c)) { *push(&xs, p->a) = D(c); continue; }
      }
    }
    *pos = s;
    if (chr(p, pos, '\n')) { *push(&xs, p->a) = D('\n'); continue; }
    *pos = s;
    if ((h = sump(p, pos))) { *push(&xs, p->a) = C2(nul, h); continue; }
    *pos = s;
    return nounslist(p, &xs);
  }
}

noun soil(parser *p, size *pos) {
  size s = *pos;
  farmark m = farget(p);
  if (!lookahead(p, s, jest(p, pos, "\"\"\""), m)) {
    *pos = s;
    if (chr(p, pos, '"')) {
      nouns xs = {0};
      for (;;) {
        size t = *pos;
        i32 v;
        u8 c;
        noun h;
        if (chr(p, pos, '\\')) {
          if (chr(p, pos, '\\')) { *push(&xs, p->a) = D('\\'); continue; }
          if (chr(p, pos, '"')) { *push(&xs, p->a) = D('"'); continue; }
          if (chr(p, pos, '{')) { *push(&xs, p->a) = D('{'); continue; }
          if (bix(p, pos, &v)) { *push(&xs, p->a) = D((u64)v); continue; }
        }
        *pos = t;
        farmark f = farget(p);
        if (!lookahead(p, t, chr(p, pos, '"'), f)) {
          *pos = t;
          farmark g = farget(p);
          if (!lookahead(p, t, chr(p, pos, '\\'), g)) {
            *pos = t;
            farmark h2 = farget(p);
            if (!lookahead(p, t, chr(p, pos, '{'), h2)) {
              *pos = t;
              if (prn(p, pos, &c)) { *push(&xs, p->a) = D(c); continue; }
            }
          }
        }
        *pos = t;
        if ((h = sump(p, pos))) { *push(&xs, p->a) = C2(nul, h); continue; }
        *pos = t;
        break;
      }
      if (chr(p, pos, '"')) {
        note(p, s, *pos, tok_string);
        return nounslist(p, &xs);
      }
    }
  }
  *pos = s;
  noun r = inde(p, pos, soilope, soilope, soilblock);
  if (r) note(p, s, *pos, tok_string);
  return r;
}

// woof lists joined with dots, each after a prefix character
noun soils(parser *p, size *pos, i32 pre, nouns *parts) {
  size s = *pos;
  noun w;
  if (pre && !chr(p, pos, (u8)pre)) return 0;
  if (!(w = soil(p, pos))) return 0;
  *push(parts, p->a) = w;
  for (;;) {
    s = *pos;
    if (dog(p, pos) && (!pre || chr(p, pos, (u8)pre)) && (w = soil(p, pos))) {
      *push(parts, p->a) = w;
      continue;
    }
    *pos = s;
    return nul;
  }
}

noun knit(parser *p, nouns *parts) {
  noun r = nul;
  for (size i = parts->len - 1; i >= 0; i--) r = weld(p, parts->data[i], r);
  return C2(K("knit"), r);
}

// runs of characters become %knit'd tapes, as +phax
noun phax(parser *p, nouns *parts) {
  nouns yun = {0};
  nouns cah = {0};
  for (size i = 0; i < parts->len; i++) {
    for (noun l = parts->data[i]; iscell(l); l = tl(l)) {
      noun w = hd(l);
      if (isatom(w)) {
        *push(&cah, p->a) = w;
        continue;
      }
      if (cah.len) *push(&yun, p->a) = C3(K("mcfs"), K("knit"), nounslist(p, &cah));
      cah.len = 0;
      cah.data = 0;
      cah.cap = 0;
      *push(&yun, p->a) = tl(w);
    }
  }
  if (cah.len) *push(&yun, p->a) = C3(K("mcfs"), K("knit"), nounslist(p, &cah));
  return nounslist(p, &yun);
}

// Irregular forms

noun wede(parser *p, size *pos) {
  if (!chr(p, pos, '+') && !chr(p, pos, '/')) return 0;
  return wide(p, pos);
}

noun rump(parser *p, size *pos) {
  noun a = rope(p, pos);
  if (!a) return 0;
  size s = *pos;
  noun b = wede(p, pos);
  if (!b) {
    *pos = s;
    return C2(K("wing"), a);
  }
  if (!(isatom(hd(a)) && atomis(tl(a), 0))) return 0;
  return C2(C3(K("rock"), K("tas"), hd(a)), b);
}

noun rupl(parser *p, size *pos) {
  b32 sig = 0;
  if (!chr(p, pos, '[')) {
    if (!jest(p, pos, "~[")) return 0;
    sig = 1;
  }
  size s = *pos;
  noun b = 0;
  if (ace(p, pos) && (b = most(p, pos, 1, sepgap, tallrule)) && gap(p, pos)) {
  } else {
    *pos = s;
    b = most(p, pos, 0, sepace, widerule);
    if (!b) return 0;
  }
  s = *pos;
  b32 tail = 1;
  if (!jest(p, pos, "]~")) {
    *pos = s;
    if (!chr(p, pos, ']')) return 0;
    tail = 0;
  }
  if (sig) {
    return tail ? C2(K("clsg"), C2(C2(K("clsg"), b), nul)) : C2(K("clsg"), b);
  }
  return tail ? C2(K("clsg"), C2(C2(K("cltr"), b), nul)) : C2(K("cltr"), b);
}

noun lute(parser *p, size *pos) {
  if (!chr(p, pos, '[') || !gap(p, pos)) return 0;
  noun r = most(p, pos, 1, sepgap, tallrule);
  if (!r || !gap(p, pos) || !chr(p, pos, ']')) return 0;
  return C2(K("cltr"), r);
}

noun scatx(parser *p, size *pos, b32 tol) {
  if (*pos >= p->len) {
    reach(p, *pos);
    return 0;
  }
  u8 c = p->buf[*pos];
  size s = *pos;
  noun r, x;
  switch (c) {
  case ',':
    chr(p, pos, ',');
    if ((r = wyde(p, pos))) return C2(K("ktcl"), r);
    *pos = s;
    return (r = rope(p, pos)) ? C2(K("wing"), r) : 0;
  case '!':
    chr(p, pos, '!');
    if ((r = wide(p, pos))) return C2(K("wtzp"), r);
    *pos = s;
    return jest(p, pos, "!!") ? C2(K("zpzp"), nul) : 0;
  case '_':
    chr(p, pos, '_');
    return (r = wide(p, pos)) ? C3(K("ktcl"), K("bccb"), r) : 0;
  case '$':
    chr(p, pos, '$');
    if (chr(p, pos, '$')) return C3(K("leaf"), K("tas"), nul);
    *pos = s + 1;
    if ((r = qut(p, pos))) return C3(K("leaf"), K("t"), r);
    *pos = s + 1;
    if ((r = nuck(p, pos)) && atomis(hd(r), 0)) return C2(K("leaf"), tl(r));
    *pos = s;
    return rump(p, pos);
  case '%': {
    chr(p, pos, '%');
    size n;
    noun q;
    if (porc(p, pos, &n, &q) && (r = posh(p, 0, 1, n, q))) return C2(K("clsg"), r);
    *pos = s + 1;
    if (chr(p, pos, '$')) return C3(K("rock"), K("tas"), nul);
    if (chr(p, pos, '&')) return C3(K("rock"), K("f"), YES);
    if (chr(p, pos, '|')) return C3(K("rock"), K("f"), NO);
    if ((r = qut(p, pos))) return C3(K("rock"), K("t"), r);
    *pos = s + 1;
    if ((r = nuck(p, pos))) return jock(p, 1, r);
    *pos = s + 1;
    n = 0;
    while (chr(p, pos, '%')) n++;
    return (r = posh(p, 0, 1, n, nul)) ? C2(K("clsg"), r) : 0;
  }
  case '&':
    if ((r = rope(p, pos))) return C3(K("cnts"), r, nul);
    *pos = s;
    chr(p, pos, '&');
    if ((r = parens(p, pos, widerule))) return C2(K("wtpm"), r);
    *pos = s + 1;
    if ((r = wede(p, pos))) return C2(C3(K("rock"), K("f"), YES), r);
    *pos = s + 1;
    return C3(K("sand"), K("f"), YES);
  case '\'':
    return (r = qut(p, pos)) ? C3(K("sand"), K("t"), r) : 0;
  case '(':
    return (r = parens(p, pos, widerule)) ? C2(K("cncl"), r) : 0;
  case '*':
    chr(p, pos, '*');
    if ((r = wyde(p, pos))) return C2(K("kttr"), r);
    *pos = s + 1;
    return C2(K("base"), K("noun"));
  case '@':
    chr(p, pos, '@');
    return C3(K("base"), K("atom"), mota(p, pos));
  case '+': {
    chr(p, pos, '+');
    if (chr(p, pos, '(') && (r = wide(p, pos)) && chr(p, pos, ')')) return C2(K("dtls"), r);
    *pos = s;
    nouns parts = {0};
    if (soils(p, pos, '+', &parts)) return C3(K("mcfs"), K("knit"), tl(knit(p, &parts)));
    *pos = s;
    return (r = rope(p, pos)) ? C3(K("cnts"), r, nul) : 0;
  }
  case '-': {
    if ((r = tash(p, pos))) return C2(K("sand"), r);
    *pos = s;
    nouns parts = {0};
    if (soils(p, pos, '-', &parts)) return C2(K("clsg"), phax(p, &parts));
    *pos = s;
    return (r = rope(p, pos)) ? C3(K("cnts"), r, nul) : 0;
  }
  case '.':
    chr(p, pos, '.');
    if ((r = perd(p, pos))) return jock(p, 0, r);
    *pos = s;
    return (r = rope(p, pos)) ? C3(K("cnts"), r, nul) : 0;
  case ':':
    chr(p, pos, ':');
    if ((r = parens(p, pos, widerule))) return C2(K("mccl"), r);
    *pos = s + 1;
    return chr(p, pos, '/') && (r = wide(p, pos)) ? C2(K("mcfs"), r) : 0;
  case '=':
    chr(p, pos, '=');
    // =(a b), or else a spec to name, which at ( is (a ...) as wyde
    // would parse it: its head is the same wide, so parse that once
    if (chr(p, pos, '(')) {
      if (!(r = wide(p, pos))) return 0;
      size t = *pos;
      if (ace(p, pos) && (x = wide(p, pos)) && chr(p, pos, ')')) return C3(K("dtts"), r, x);
      *pos = t;
      if ((r = makerest(p, pos, r))) r = wart(p, s + 1, *pos, r);
    } else {
      *pos = s + 1;
      r = wyde(p, pos);
    }
    if (r) {
      noun n = autoname(p, r);
      if (n) return C3(K("ktts"), n, C2(K("kttr"), r));
    }
    return 0;
  case '?':
    chr(p, pos, '?');
    if ((r = parens(p, pos, wyderule))) return C3(K("ktcl"), K("bcwt"), r);
    *pos = s + 1;
    return C2(K("base"), K("flag"));
  case '[':
    return rupl(p, pos);
  case '^':
    if ((r = rope(p, pos))) return C2(K("wing"), r);
    *pos = s;
    chr(p, pos, '^');
    return C2(K("base"), K("cell"));
  case '`':
    chr(p, pos, '`');
    if (chr(p, pos, '@')) {
      noun m = mota(p, pos);
      if (chr(p, pos, '`') && (r = wide(p, pos))) {
        return C4(K("ktls"), C3(K("sand"), m, nul), K("ktls"), C2(C3(K("sand"), nul, nul), r));
      }
    }
    *pos = s + 1;
    if (chr(p, pos, '*') && chr(p, pos, '`') && (r = wide(p, pos))) {
      return C3(K("kthp"), C2(K("base"), K("noun")), r);
    }
    *pos = s + 1;
    if ((x = wyde(p, pos)) && chr(p, pos, '`') && (r = wide(p, pos))) {
      return C3(K("kthp"), x, r);
    }
    *pos = s + 1;
    if (chr(p, pos, '+') && (x = wide(p, pos)) && chr(p, pos, '`') && (r = wide(p, pos))) {
      return C3(K("ktls"), x, r);
    }
    *pos = s + 1;
    return (r = wide(p, pos)) ? C2(C3(K("rock"), K("n"), nul), r) : 0;
  case '"': {
    nouns parts = {0};
    return soils(p, pos, 0, &parts) ? knit(p, &parts) : 0;
  }
  case '|':
    if ((r = rope(p, pos))) return C3(K("cnts"), r, nul);
    *pos = s;
    chr(p, pos, '|');
    if ((r = parens(p, pos, widerule))) return C2(K("wtbr"), r);
    *pos = s + 1;
    if ((r = wede(p, pos))) return C2(C3(K("rock"), K("f"), NO), r);
    *pos = s + 1;
    return C3(K("sand"), K("f"), NO);
  case '~':
    if ((r = rupl(p, pos))) return r;
    *pos = s;
    chr(p, pos, '~');
    if ((r = brackets(p, pos, widerule))) return C2(K("clsg"), r);
    *pos = s + 1;
    if (chr(p, pos, '(') && (x = rope(p, pos)) && ace(p, pos) && (r = wide(p, pos))
        && ace(p, pos)) {
      noun l = most(p, pos, 0, sepace, widerule);
      if (l && chr(p, pos, ')')) return C4(K("cnsg"), x, r, l);
    }
    *pos = s + 1;
    if ((r = twid(p, pos))) return jock(p, 0, r);
    *pos = s + 1;
    if ((r = wede(p, pos))) return C2(C2(K("bust"), K("null")), r);
    *pos = s + 1;
    return C2(K("bust"), K("null"));
  case '/':
    return rood(p, pos);
  case '<':
    chr(p, pos, '<');
    r = most(p, pos, 0, sepace, widerule);
    return r && chr(p, pos, '>') ? C2(K("tell"), r) : 0;
  case '>':
    chr(p, pos, '>');
    r = most(p, pos, 0, sepace, widerule);
    return r && chr(p, pos, '<') ? C2(K("yell"), r) : 0;
  case '#':
    chr(p, pos, '#');
    return reed(p, pos);
  }
  if (c >= '0' && c <= '9') {
    noun d = bisk(p, pos);
    if (!d) return 0;
    size t = *pos;
    noun b = wede(p, pos);
    if (!b) {
      *pos = t;
      return C2(K("sand"), d);
    }
    return C2(C2(K("rock"), d), b);
  }
  if (c >= 'a' && c <= 'z') return rump(p, pos);
  reach(p, *pos);
  return 0;
}

noun scadx(parser *p, size *pos, b32 tol);

// what a wide form is, for highlighting
i32 scatkind(parser *p, size start, noun r) {
  noun tag = hd(r);
  u64 t = atomfits(tag) ? atomlow(tag) : 0;
  if (t == TW("clsg") && p->buf[start] == '/') return tok_string;
  if (t == TW("sand") || t == TW("rock")) {
    noun aura = hd(tl(r));
    if (atomeqc(aura, "t")) return tok_string;
    if (atomeqc(aura, "tas") && p->buf[start] == '%') return tok_term;
    return tok_number;
  }
  if (t == TW("knit")) return tok_string;
  if (t == TW("leaf")) return tok_term;
  if (t == TW("base") || t == TW("like")) return tok_type;
  if (t == TW("wing") || t == TW("cnts")) return tok_variable;
  if (t == TW("bust")) return tok_number;
  return -1;
}

noun scat(parser *p, size *pos) {
  size s = *pos;
  noun r = scatx(p, pos, 0);
  if (r && p->toks && iscell(r)) {
    i32 k = scatkind(p, s, r);
    if (k >= 0) note(p, s, *pos, k);
  }
  return r;
}

// the rest of a spec (x a b), after the head x
noun makerest(parser *p, size *pos, noun x) {
  size t = *pos;
  noun r = 0;
  if (ace(p, pos)) r = most(p, pos, 0, sepace, wyderule);
  if (!r) {
    *pos = t;
    r = nul;
  }
  if (!chr(p, pos, ')')) return 0;
  return C3(K("make"), x, r);
}

noun scadx(parser *p, size *pos, b32 tol) {
  if (*pos >= p->len) {
    reach(p, *pos);
    return 0;
  }
  u8 c = p->buf[*pos];
  size s = *pos;
  noun r, x;
  switch (c) {
  case '_':
    chr(p, pos, '_');
    return (r = wide(p, pos)) ? C2(K("bccb"), r) : 0;
  case ',':
    chr(p, pos, ',');
    return (r = wide(p, pos)) ? C2(K("bcmc"), r) : 0;
  case '$':
    return (r = ropa(p, pos, 0)) ? C2(K("like"), r) : 0;
  case '%':
    chr(p, pos, '%');
    if (chr(p, pos, '$')) return C3(K("leaf"), K("tas"), nul);
    if (chr(p, pos, '&')) return C3(K("leaf"), K("f"), YES);
    if (chr(p, pos, '|')) return C3(K("leaf"), K("f"), NO);
    if ((r = qut(p, pos))) return C3(K("leaf"), K("t"), r);
    *pos = s + 1;
    if ((r = nuck(p, pos)) && atomis(hd(r), 0)) return C2(K("leaf"), tl(r));
    return 0;
  case '(':
    chr(p, pos, '(');
    return (x = wide(p, pos)) ? makerest(p, pos, x) : 0;
  case '[':
    return (r = brackets(p, pos, wyderule)) ? C2(K("bccl"), r) : 0;
  case '*':
    chr(p, pos, '*');
    return C2(K("base"), K("noun"));
  case '/':
    chr(p, pos, '/');
    return (r = symbuc(p, pos)) ? C2(K("loop"), r) : 0;
  case '@':
    chr(p, pos, '@');
    return C3(K("base"), K("atom"), mota(p, pos));
  case '?':
    chr(p, pos, '?');
    if ((r = parens(p, pos, wyderule))) return C2(K("bcwt"), r);
    *pos = s + 1;
    return C2(K("base"), K("flag"));
  case '~':
    chr(p, pos, '~');
    return C2(K("base"), K("null"));
  case '!':
    return jest(p, pos, "!!") ? C2(K("base"), K("void")) : 0;
  case '^':
    if ((r = ropa(p, pos, 0))) return C2(K("like"), r);
    *pos = s;
    chr(p, pos, '^');
    return C2(K("base"), K("cell"));
  case '=': {
    chr(p, pos, '=');
    noun name = 0;
    noun spec;
    if ((x = sym(p, pos)) && chr(p, pos, '=') && (spec = wyde(p, pos))) {
      name = x;
    } else {
      *pos = s + 1;
      if (!(spec = wyde(p, pos))) return 0;
    }
    noun t = autoname(p, spec);
    if (!t) return 0;
    if (name) {
      noun parts[3] = {name, D('-'), t};
      t = atomrap3(p->a, parts, 3);
    }
    return C3(K("bcts"), t, spec);
  }
  }
  if (c >= 'a' && c <= 'z') {
    if ((x = sym(p, pos)) && chr(p, pos, '=') && (r = wyde(p, pos))) {
      return C3(K("bcts"), x, r);
    }
    *pos = s;
    return (r = ropa(p, pos, 0)) ? C2(K("like"), r) : 0;
  }
  reach(p, *pos);
  return 0;
}

noun scad(parser *p, size *pos) {
  size s = *pos;
  noun r = scadx(p, pos, 0);
  if (r && p->toks && iscell(r)) {
    i32 k = scatkind(p, s, r);
    if (k >= 0) note(p, s, *pos, k);
  }
  return r;
}

// Rune contents, as +norm

noun toad(parser *p, size *pos, b32 tol, rule har) {
  size s = *pos;
  if (tol) {
    noun r;
    if (gap(p, pos) && (r = har(p, pos, 1))) return r;
    *pos = s;
  }
  if (!chr(p, pos, '(')) return 0;
  noun r = har(p, pos, 0);
  if (!r || !chr(p, pos, ')')) return 0;
  return r;
}

noun butt(parser *p, size *pos, b32 tol, noun r) {
  if (!r) return 0;
  if (tol && !(gap(p, pos) && duz(p, pos))) return 0;
  return r;
}

noun hank(parser *p, size *pos, b32 tol) { return most(p, pos, tol, muck, loaf); }
noun hunk(parser *p, size *pos, b32 tol) { return most(p, pos, tol, muck, loan); }

noun rickpair(parser *p, size *pos, b32 tol) { return SEQ(roperule, loaf); }
noun ruckpair(parser *p, size *pos, b32 tol) { return SEQ(loan, loaf); }
noun rick(parser *p, size *pos, b32 tol) { return most(p, pos, tol, mash, rickpair); }
noun ruck(parser *p, size *pos, b32 tol) { return most(p, pos, tol, mash, ruckpair); }

noun lore(parser *p, size *pos, b32 tol) {
  noun r = loaf(p, pos, tol);
  return r ? flay(p, r) : 0;
}

noun lomp(parser *p, size *pos, b32 tol) {
  noun s = sym(p, pos);
  if (!s) return 0;
  size t = *pos;
  noun w;
  if (chr(p, pos, '=') && (w = wyde(p, pos))) return C3(s, nul, w);
  *pos = t;
  return C2(s, nul);
}

noun censym(parser *p, size *pos, b32 tol) {
  return chr(p, pos, '%') ? sym(p, pos) : 0;
}

noun wise(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r, x;
  noun noun_ = C2(K("base"), K("noun"));
  if (chr(p, pos, '=') && (r = wyde(p, pos))) {
    noun t = autoname(p, r);
    if (t) return C3(K("name"), t, C3(K("spec"), r, noun_));
  }
  *pos = s;
  if ((x = sym(p, pos))) {
    size t = *pos;
    if ((chr(p, pos, '/') || (*pos = t, chr(p, pos, '='))) && (r = wyde(p, pos))) {
      return C3(K("name"), x, C3(K("spec"), r, noun_));
    }
    *pos = t;
    return x;
  }
  *pos = s;
  if ((r = wyde(p, pos))) return C3(K("spec"), r, noun_);
  return 0;
}

// wing or hoon, optionally named
noun teakwyp(parser *p, size *pos) {
  size s = *pos;
  noun n, r;
  if ((n = sym(p, pos)) && chr(p, pos, '=')) {
    size t = *pos;
    if ((r = rope(p, pos))) return C3(YES, C2(nul, n), r);
    *pos = t;
    if ((r = wide(p, pos))) return C3(NO, C2(nul, n), r);
  }
  *pos = s;
  if ((r = rope(p, pos))) return C3(YES, nul, r);
  *pos = s;
  if ((r = wide(p, pos))) return C3(NO, nul, r);
  return 0;
}

noun teak(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r = teakwyp(p, pos);
  if (r || !tol) return r;
  *pos = s;
  noun n;
  if (jest(p, pos, "^=") && gap(p, pos) && (n = sym(p, pos)) && gap(p, pos)) {
    size t = *pos;
    if ((r = rope(p, pos))) return C3(YES, C2(nul, n), r);
    *pos = t;
    if ((r = tall(p, pos))) return C3(NO, C2(nul, n), r);
  }
  *pos = s;
  if ((r = tall(p, pos))) return C3(NO, nul, r);
  return 0;
}

noun lynx(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r;
  if ((r = brackets(p, pos, symrule))) return r;
  *pos = s;
  if (tol) {
    if ((r = most(p, pos, 1, sepgap, symrule)) && gap(p, pos) && duz(p, pos)) return r;
    *pos = s;
  }
  if ((r = sym(p, pos))) return C2(r, nul);
  return 0;
}

// Cores

noun bola(parser *p, size *pos, b32 tol) {
  noun n, h;
  size s = *pos;
  if (!jest(p, pos, "++") || !gap(p, pos)) return 0;
  size t = *pos;
  if (!(n = symbuc(p, pos))) return 0;
  note(p, s, s + 2, tok_keyword);
  note(p, t, *pos, tok_function);
  if (!gap(p, pos) || !(h = loaf(p, pos, tol))) return 0;
  return C2(n, h);
}

noun boba(parser *p, size *pos, b32 tol) {
  noun n, sp;
  size s = *pos;
  if (!jest(p, pos, "+$") || !gap(p, pos)) return 0;
  size t = *pos;
  if (!(n = sym(p, pos))) return 0;
  note(p, s, s + 2, tok_keyword);
  note(p, t, *pos, tok_type);
  if (!gap(p, pos) || !(sp = loan(p, pos, tol))) return 0;
  return C2(n, C4(K("ktcl"), K("name"), n, sp));
}

noun boog(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r = bola(p, pos, tol);
  if (!r) {
    *pos = s;
    r = boba(p, pos, tol);
  }
  // where the arm is, for messages about types made from it, and for a
  // mold builder, the arm of the gate it makes, [%ktcl spec] as +open
  // has it, whose types are named by it
  i32 file = (p->root ? p->root : p)->file;
  if (r && file && !loose.on) {
    hair at = hairat(p, s);
    noun v = atomu64(p->a, (u64)file << 40 | (u64)at.line << 16 | (u64)at.col);
    noun k = armkey(p->a, tl(r));
    if (!nounmapget(&armsites, k)) nounmapput(&armsites, k, v);
    noun b = tl(r);
    while (tagis(b, "dbug")) b = tl(tl(b));
    if (tagis(b, "brbc")) {
      noun g = armkey(p->a, C2(K("ktcl"), tl(tl(b))));
      if (!nounmapget(&armsites, g)) nounmapput(&armsites, g, v);
      if (!nounmapget(&armnames, g)) nounmapput(&armnames, g, hd(r));
    }
  }
  return r;
}

noun dupe(parser *p, char *what, noun name) {
  return C2(K("eror"), weld(p, tape(p, what), tapeatom(p, name)));
}

noun whap(parser *p, size *pos, b32 tol) {
  noun arms = most(p, pos, tol, muck, boog);
  if (!arms) return 0;
  nouns xs = {0};
  for (noun l = arms; iscell(l); l = tl(l)) *push(&xs, p->a) = hd(l);
  noun m = nul;
  for (size i = xs.len - 1; i >= 0; i--) {
    noun q = hd(xs.data[i]);
    noun v = maphas(m, q) ? dupe(p, "duplicate arm: +", q) : tl(xs.data[i]);
    m = mapput(p->a, m, q, v);
  }
  return m;
}

noun whip(parser *p, size *pos, b32 tol) {
  noun n, m;
  size s = *pos;
  if (!jest(p, pos, "+|") || !gap(p, pos)) return 0;
  size t = *pos;
  if (!chr(p, pos, '%') || !(n = sym(p, pos))) return 0;
  note(p, s, s + 2, tok_keyword);
  note(p, t, *pos, tok_term);
  if (!gap(p, pos) || !(m = whap(p, pos, tol))) return 0;
  return C2(n, m);
}

// a set of nouns by value, for names seen
typedef struct {
  noun *data;
  size  cap;
  size  len;
} nounset;

size nounslot(nounset *t, noun key) {
  size mask = t->cap - 1;
  size j = (size)mug(key) & mask;
  while (t->data[j] && !nouneq(t->data[j], key)) j = (j + 1) & mask;
  return j;
}

b32 nounsethas(nounset *t, noun key) {
  return t->cap && t->data[nounslot(t, key)];
}

void nounsetput(arena *a, nounset *t, noun key) {
  if (t->len*2 >= t->cap) {
    nounset n = {0};
    n.cap = t->cap ? t->cap * 2 : 64;
    n.data = new(a, noun, n.cap);
    for (size i = 0; i < t->cap; i++) {
      if (t->data[i]) n.data[nounslot(&n, t->data[i])] = t->data[i];
    }
    n.len = t->len;
    *t = n;
  }
  size j = nounslot(t, key);
  if (!t->data[j]) t->len++;
  t->data[j] = key;
}

b32 anyseen(noun m, nounset *seen) {
  if (isatom(m)) return 0;
  return nounsethas(seen, hd(mapn(m))) || anyseen(mapl(m), seen) || anyseen(mapr(m), seen);
}

void markseen(arena *a, noun m, nounset *seen) {
  if (isatom(m)) return;
  nounsetput(a, seen, hd(mapn(m)));
  markseen(a, mapl(m), seen);
  markseen(a, mapr(m), seen);
}

// arms of a chapter that a later chapter has too become errors
noun markdupes(parser *p, noun m, nounset *later) {
  if (!anyseen(m, later)) return m;
  noun n = mapn(m);
  noun v = nounsethas(later, hd(n)) ? dupe(p, "duplicate arm: +", hd(n)) : tl(n);
  return mapnode(p->a, C2(hd(n), v), markdupes(p, mapl(m), later), markdupes(p, mapr(m), later));
}

noun wisp(parser *p, size *pos, b32 tol) {
  if (!tol) {
    reach(p, *pos);
    return 0;
  }
  size s = *pos;
  if (dun(p, pos)) return nul;
  *pos = s;
  noun chaps = most(p, pos, tol, muck, whip);
  if (!chaps) {
    *pos = s;
    noun m = whap(p, pos, tol);
    if (!m) return 0;
    chaps = C2(C2(nul, m), nul);
  }
  if (!gap(p, pos) || !dun(p, pos)) return 0;
  nouns xs = {0};
  for (noun l = chaps; iscell(l); l = tl(l)) *push(&xs, p->a) = hd(l);
  noun tomes = nul;
  nounset arms = {0};
  for (size i = xs.len - 1; i >= 0; i--) {
    noun name = hd(xs.data[i]);
    noun m = markdupes(p, tl(xs.data[i]), &arms);
    noun v = m;
    if (maphas(tomes, name)) {
      noun e = dupe(p, "duplicate chapter: |", name);
      v = mapnode(p->a, C2(nul, e), nul, nul);
    }
    tomes = mapput(p->a, tomes, name, v);
    markseen(p->a, m, &arms);
  }
  return tomes;
}

noun waspalias(parser *p, size *pos, b32 tol) { return SEQ(symrule, loaf); }

noun wasp(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r;
  if (chr(p, pos, '+') && chr(p, pos, '*') && muck(p, pos, tol)
      && (r = most(p, pos, tol, muck, waspalias)) && muck(p, pos, tol)) {
    note(p, s, s + 2, tok_keyword);
    return r;
  }
  *pos = s;
  return nul;
}

noun waspwisp(parser *p, size *pos, b32 tol) {
  noun a = wasp(p, pos, tol);
  noun b = wisp(p, pos, tol);
  return b ? C2(a, b) : 0;
}

// Hints

noun bonk(parser *p, size *pos, b32 tol) {
  if (!chr(p, pos, '%')) return 0;
  size s = *pos;
  noun a, b, c;
  if ((a = sym(p, pos)) && chr(p, pos, ':') && (b = sym(p, pos)) && chr(p, pos, '.')
      && chr(p, pos, '.') && (c = dem(p, pos))) {
    return C3(a, b, c);
  }
  *pos = s;
  if ((a = sym(p, pos)) && chr(p, pos, ':') && (b = sym(p, pos)) && chr(p, pos, '.')
      && (c = dem(p, pos))) {
    return C3(a, b, c);
  }
  *pos = s;
  if ((a = sym(p, pos)) && chr(p, pos, '.') && (c = dem(p, pos))) return C2(a, c);
  *pos = s;
  return sym(p, pos);
}

noun bont(parser *p, size *pos, b32 tol) {
  noun a;
  if (!chr(p, pos, '%') || !(a = sym(p, pos))) return 0;
  size s = *pos;
  noun b = 0;
  if (chr(p, pos, '.')) {
    size t = *pos;
    if (!(b = wide(p, pos))) {
      *pos = t;
      if (muck(p, pos, tol)) b = loaf(p, pos, tol);
    }
  }
  if (!b) {
    *pos = s;
    return a;
  }
  return C2(a, b);
}

noun bony(parser *p, size *pos, b32 tol) {
  size n = 0;
  while (chr(p, pos, '=')) n++;
  return n ? D((u64)n) : 0;
}

noun bonzpair(parser *p, size *pos, b32 tol) { return SEQ(censym, loaf); }

noun bonz(parser *p, size *pos, b32 tol) {
  size s = *pos;
  if (chr(p, pos, '~')) return nul;
  *pos = s;
  if (tol ? !(duz(p, pos) && gap(p, pos)) : !chr(p, pos, '(')) return 0;
  size t = *pos;
  noun r = most(p, pos, tol, mash, bonzpair);
  if (!r) {
    *pos = t;
    r = nul;
  }
  if (tol ? !(gap(p, pos) && duz(p, pos)) : !chr(p, pos, ')')) return 0;
  return r;
}

noun gars(parser *p, size *pos, b32 tol) {
  size n = 0;
  while (n < 3 && chr(p, pos, '>')) n++;
  return n ? D((u64)n) : 0;
}

noun bonzloaf(parser *p, size *pos, b32 tol) { return SEQ(bonz, loaf); }

noun hinhnum(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun a, b;
  if ((a = dem(p, pos))) return a;
  *pos = s;
  if (chr(p, pos, '[') && (a = dem(p, pos)) && ace(p, pos) && (b = dem(p, pos))
      && chr(p, pos, ']')) {
    return C2(a, b);
  }
  return 0;
}

noun hinb(parser *p, size *pos, b32 tol) { return SEQ(bont, loaf); }
noun hind(parser *p, size *pos, b32 tol) { return SEQ(bonk, loaf, bonzloaf); }
noun hine(parser *p, size *pos, b32 tol) { return SEQ(bonk, loaf); }
noun hinh(parser *p, size *pos, b32 tol) { return SEQ(hinhnum, loaf); }

noun hinc(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r = SEQ(bony, loaf);
  if (r) return r;
  *pos = s;
  return (r = loaf(p, pos, tol)) ? C2(nul, r) : 0;
}

noun hinf(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r = SEQ(gars, loaf, loaf);
  if (r) return r;
  *pos = s;
  return (r = SEQ(loaf, loaf)) ? C2(nul, r) : 0;
}

noun hing(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r = SEQ(gars, loaf, loaf, loaf);
  if (r) return r;
  *pos = s;
  return (r = SEQ(loaf, loaf, loaf)) ? C2(nul, r) : 0;
}

// Contents

noun expa(parser *p, size *pos, b32 tol) { return loaf(p, pos, tol); }
noun expb(parser *p, size *pos, b32 tol) { return SEQ(loaf, loaf); }
noun expc(parser *p, size *pos, b32 tol) { return SEQ(loaf, loaf, loaf); }
noun expd(parser *p, size *pos, b32 tol) { return SEQ(loaf, loaf, loaf, loaf); }
noun expe(parser *p, size *pos, b32 tol) { return wisp(p, pos, tol); }
noun expft(parser *p, size *pos, b32 tol) { return SEQ(censym, loaf); }
noun expg(parser *p, size *pos, b32 tol) { return SEQ(lomp, loaf, loaf); }
noun exph(parser *p, size *pos, b32 tol) { return butt(p, pos, tol, SEQ(roperule, rick)); }
noun expi(parser *p, size *pos, b32 tol) { return butt(p, pos, tol, SEQ(loaf, hank)); }
noun expj(parser *p, size *pos, b32 tol) { return SEQ(lore, loaf); }
noun expm(parser *p, size *pos, b32 tol) { return butt(p, pos, tol, SEQ(roperule, loaf, rick)); }

noun loaf1(parser *p, size *pos, b32 tol) {
  noun r = loaf(p, pos, tol);
  return r ? C2(r, nul) : 0;
}

noun expn(parser *p, size *pos, b32 tol) { return SEQ(roperule, loaf, loaf1); }
noun expo(parser *p, size *pos, b32 tol) { return SEQ(wise, loaf, loaf); }

noun buttrick(parser *p, size *pos, b32 tol) { return butt(p, pos, tol, rick(p, pos, tol)); }

noun expp(parser *p, size *pos, b32 tol) { return SEQ(buttrick, loaf); }
noun expq(parser *p, size *pos, b32 tol) { return SEQ(roperule, loaf, loaf); }
noun expr(parser *p, size *pos, b32 tol) { return SEQ(loaf, wisp); }
noun exps(parser *p, size *pos, b32 tol) { return butt(p, pos, tol, hank(p, pos, tol)); }
noun expt(parser *p, size *pos, b32 tol) { return SEQ(wise, roperule, loaf, loaf); }
noun expw(parser *p, size *pos, b32 tol) { return SEQ(roperule, loaf, loaf, loaf); }
noun expx(parser *p, size *pos, b32 tol) { return SEQ(ropa, loaf, loaf); }
noun expz(parser *p, size *pos, b32 tol) { return SEQ(loan, loaf, loaf, loaf); }

noun expy(parser *p, size *pos, b32 tol) {
  b32 bug = p->bug;
  p->bug = 1;
  noun r = loaf(p, pos, tol);
  p->bug = bug;
  return r;
}

noun expbang(parser *p, size *pos, b32 tol) {
  b32 bug = p->bug;
  p->bug = p->allbug;
  noun r = loaf(p, pos, tol);
  p->bug = bug;
  return r;
}

noun exqa(parser *p, size *pos, b32 tol) { return loan(p, pos, tol); }
noun exqb(parser *p, size *pos, b32 tol) { return SEQ(loan, loan); }
noun exqc(parser *p, size *pos, b32 tol) { return SEQ(loan, loaf); }
noun exqd(parser *p, size *pos, b32 tol) { return SEQ(loaf, loan); }
noun exqe(parser *p, size *pos, b32 tol) { return SEQ(lynx, loan); }
noun exqs(parser *p, size *pos, b32 tol) { return butt(p, pos, tol, hunk(p, pos, tol)); }
noun exqg(parser *p, size *pos, b32 tol) { return SEQ(symrule, loan); }

noun cltrhank(parser *p, size *pos, b32 tol) {
  noun r = butt(p, pos, tol, hank(p, pos, tol));
  return r ? C2(K("cltr"), r) : 0;
}

noun exqn(parser *p, size *pos, b32 tol) { return SEQ(loan, cltrhank); }
noun exqr(parser *p, size *pos, b32 tol) { return SEQ(loan, waspwisp); }
noun exqx(parser *p, size *pos, b32 tol) { return SEQ(loaf, loan, loan); }
noun exqy(parser *p, size *pos, b32 tol) { return SEQ(loaf, loan, loan, loan); }

noun butthunk(parser *p, size *pos, b32 tol) { return butt(p, pos, tol, hunk(p, pos, tol)); }

noun exqz(parser *p, size *pos, b32 tol) { return SEQ(loaf, butthunk); }

// ?- ?^ ?= ?# ?+ ?@ ?~ with tiki expansion

noun txhp(parser *p, size *pos, b32 tol) {
  noun r = butt(p, pos, tol, SEQ(teak, ruck));
  if (!r) return 0;
  noun tik = hd(r);
  return ahgray(p, tik, C3(K("wthp"), ahpuce(p, tik), ahopts(p, tik, tl(r))));
}

noun tkkt(parser *p, size *pos, b32 tol) {
  noun r = SEQ(teak, loaf, loaf);
  if (!r) return 0;
  noun tik = hd(r);
  return ahgray(p, tik, C4(K("wtkt"), ahpuce(p, tik), ahblue(p, tik, hd(tl(r))),
                           ahblue(p, tik, tl(tl(r)))));
}

noun txls(parser *p, size *pos, b32 tol) {
  noun r = butt(p, pos, tol, SEQ(teak, loaf, ruck));
  if (!r) return 0;
  noun tik = hd(r);
  return ahgray(p, tik, C4(K("wtls"), ahpuce(p, tik), ahblue(p, tik, hd(tl(r))),
                           ahopts(p, tik, tl(tl(r)))));
}

noun tkvt(parser *p, size *pos, b32 tol) {
  noun r = SEQ(teak, loaf, loaf);
  if (!r) return 0;
  noun tik = hd(r);
  return ahgray(p, tik, C4(K("wtpt"), ahpuce(p, tik), ahblue(p, tik, hd(tl(r))),
                           ahblue(p, tik, tl(tl(r)))));
}

noun tksg(parser *p, size *pos, b32 tol) {
  noun r = SEQ(teak, loaf, loaf);
  if (!r) return 0;
  noun tik = hd(r);
  return ahgray(p, tik, C4(K("wtsg"), ahpuce(p, tik), ahblue(p, tik, hd(tl(r))),
                           ahblue(p, tik, tl(tl(r)))));
}

noun txts(parser *p, size *pos, b32 tol) {
  noun r = SEQ(loan, teak);
  if (!r) return 0;
  noun tik = tl(r);
  return ahgray(p, tik, C3(K("wtts"), ahteal(p, tik, hd(r)), ahpuce(p, tik)));
}

noun txhx(parser *p, size *pos, b32 tol) {
  noun r = SEQ(lore, teak);
  if (!r) return 0;
  noun tik = tl(r);
  return ahgray(p, tik, C3(K("wthx"), ahteal(p, tik, hd(r)), ahpuce(p, tik)));
}

// Rune tables

enum { rune_tag, rune_runo, rune_ktcl, rune_bare };

typedef struct {
  u8    c1;
  u8    c2;
  char *tag;
  rule  har;
  i32   kind;
} runedef;

runedef hoonrunes[] = {
  {'|', '_', "brcb", exqr, rune_tag},
  {'|', '%', "brcn", expe, rune_runo},
  {'|', '@', "brpt", expe, rune_runo},
  {'|', ':', "brcl", expb, rune_tag},
  {'|', '.', "brdt", expa, rune_tag},
  {'|', '-', "brhp", expa, rune_tag},
  {'|', '^', "brkt", expr, rune_tag},
  {'|', '~', "brsg", exqc, rune_tag},
  {'|', '*', "brtr", exqc, rune_tag},
  {'|', '=', "brts", exqc, rune_tag},
  {'|', '?', "brwt", expa, rune_tag},
  {'|', '$', "brbc", exqe, rune_tag},
  {'$', '@', "bcpt", exqb, rune_ktcl},
  {'$', '_', "bccb", expa, rune_ktcl},
  {'$', ':', "bccl", exqs, rune_ktcl},
  {'$', '%', "bccn", exqs, rune_ktcl},
  {'$', '<', "bcgl", exqb, rune_ktcl},
  {'$', '>', "bcgr", exqb, rune_ktcl},
  {'$', '|', "bcbr", exqc, rune_ktcl},
  {'$', '&', "bcpm", exqc, rune_ktcl},
  {'$', '^', "bckt", exqb, rune_ktcl},
  {'$', '~', "bcsg", exqd, rune_ktcl},
  {'$', '-', "bchp", exqb, rune_ktcl},
  {'$', '=', "bcts", exqg, rune_ktcl},
  {'$', '?', "bcwt", exqs, rune_ktcl},
  {'$', '+', "bcls", exqg, rune_ktcl},
  {'$', '.', "kttr", exqa, rune_tag},
  {'$', ',', "ktcl", exqa, rune_tag},
  {'%', '_', "cncb", exph, rune_tag},
  {'%', '.', "cndt", expb, rune_tag},
  {'%', '^', "cnkt", expd, rune_tag},
  {'%', '+', "cnls", expc, rune_tag},
  {'%', '-', "cnhp", expb, rune_tag},
  {'%', ':', "cncl", expi, rune_tag},
  {'%', '~', "cnsg", expn, rune_tag},
  {'%', '*', "cntr", expm, rune_tag},
  {'%', '=', "cnts", exph, rune_tag},
  {':', '_', "clcb", expb, rune_tag},
  {':', '^', "clkt", expd, rune_tag},
  {':', '+', "clls", expc, rune_tag},
  {':', '-', "clhp", expb, rune_tag},
  {':', '~', "clsg", exps, rune_tag},
  {':', '*', "cltr", exps, rune_tag},
  {'.', '+', "dtls", expa, rune_tag},
  {'.', '*', "dttr", expb, rune_tag},
  {'.', '=', "dtts", expb, rune_tag},
  {'.', '?', "dtwt", expa, rune_tag},
  {'.', '^', "dtkt", exqn, rune_tag},
  {'^', '|', "ktbr", expa, rune_tag},
  {'^', '.', "ktdt", expb, rune_tag},
  {'^', '-', "kthp", exqc, rune_tag},
  {'^', '+', "ktls", expb, rune_tag},
  {'^', '&', "ktpm", expa, rune_tag},
  {'^', '~', "ktsg", expa, rune_tag},
  {'^', '=', "ktts", expj, rune_tag},
  {'^', '?', "ktwt", expa, rune_tag},
  {'^', '*', "kttr", exqa, rune_tag},
  {'^', ':', "ktcl", exqa, rune_tag},
  {'^', '_', "ktcb", expb, rune_tag},
  {'~', '|', "sgbr", expb, rune_tag},
  {'~', '$', "sgbc", expft, rune_tag},
  {'~', '_', "sgcb", expb, rune_tag},
  {'~', '%', "sgcn", hind, rune_tag},
  {'~', '/', "sgfs", hine, rune_tag},
  {'~', '<', "sggl", hinb, rune_tag},
  {'~', '>', "sggr", hinb, rune_tag},
  {'~', '+', "sgls", hinc, rune_tag},
  {'~', '&', "sgpm", hinf, rune_tag},
  {'~', '?', "sgwt", hing, rune_tag},
  {'~', '=', "sgts", expb, rune_tag},
  {'~', '!', "sgzp", expb, rune_tag},
  {';', ':', "mccl", expi, rune_tag},
  {';', '/', "mcfs", expa, rune_tag},
  {';', '<', "mcgl", expz, rune_tag},
  {';', '~', "mcsg", expi, rune_tag},
  {';', ';', "mcmc", exqc, rune_tag},
  {'=', '|', "tsbr", exqc, rune_tag},
  {'=', '.', "tsdt", expq, rune_tag},
  {'=', '?', "tswt", expw, rune_tag},
  {'=', '^', "tskt", expt, rune_tag},
  {'=', ':', "tscl", expp, rune_tag},
  {'=', '/', "tsfs", expo, rune_tag},
  {'=', ';', "tsmc", expo, rune_tag},
  {'=', '<', "tsgl", expb, rune_tag},
  {'=', '>', "tsgr", expb, rune_tag},
  {'=', '-', "tshp", expb, rune_tag},
  {'=', '*', "tstr", expg, rune_tag},
  {'=', ',', "tscm", expb, rune_tag},
  {'=', '+', "tsls", expb, rune_tag},
  {'=', '~', "tssg", expi, rune_tag},
  {'?', '|', "wtbr", exps, rune_tag},
  {'?', ':', "wtcl", expc, rune_tag},
  {'?', '.', "wtdt", expc, rune_tag},
  {'?', '<', "wtgl", expb, rune_tag},
  {'?', '>', "wtgr", expb, rune_tag},
  {'?', '-', 0, txhp, rune_bare},
  {'?', '^', 0, tkkt, rune_bare},
  {'?', '=', 0, txts, rune_bare},
  {'?', '#', 0, txhx, rune_bare},
  {'?', '+', 0, txls, rune_bare},
  {'?', '&', "wtpm", exps, rune_tag},
  {'?', '@', 0, tkvt, rune_bare},
  {'?', '~', 0, tksg, rune_bare},
  {'?', '!', "wtzp", expa, rune_tag},
  {'!', ':', 0, expy, rune_bare},
  {'!', '.', 0, expbang, rune_bare},
  {'!', ',', "zpcm", expb, rune_tag},
  {'!', ';', "zpmc", expb, rune_tag},
  {'!', '>', "zpgr", expa, rune_tag},
  {'!', '<', "zpgl", exqc, rune_tag},
  {'!', '@', "zppt", expx, rune_tag},
  {'!', '=', "zpts", expa, rune_tag},
  {'!', '?', "zpwt", hinh, rune_tag},
};

runedef specrunes[] = {
  {'$', ':', "bccl", exqs, rune_tag},
  {'$', '%', "bccn", exqs, rune_tag},
  {'$', '<', "bcgl", exqb, rune_tag},
  {'$', '>', "bcgr", exqb, rune_tag},
  {'$', '^', "bckt", exqb, rune_tag},
  {'$', '~', "bcsg", exqd, rune_tag},
  {'$', '|', "bcbr", exqc, rune_tag},
  {'$', '&', "bcpm", exqc, rune_tag},
  {'$', '@', "bcpt", exqb, rune_tag},
  {'$', '_', "bccb", expa, rune_tag},
  {'$', '-', "bchp", exqb, rune_tag},
  {'$', '=', "bcts", exqg, rune_tag},
  {'$', '?', "bcwt", exqs, rune_tag},
  {'$', ';', "bcmc", expa, rune_tag},
  {'$', '+', "bcls", exqg, rune_tag},
  {'%', '^', "cnkt", exqy, rune_tag},
  {'%', '+', "cnls", exqx, rune_tag},
  {'%', '-', "cnhp", exqd, rune_tag},
  {'%', '.', "cndt", exqc, rune_tag},
  {'%', ':', "cncl", exqz, rune_tag},
};

// dispatch on the two rune characters, as the nested +stew tables
// runes by their two characters, an index into a table plus one
typedef struct {
  runedef *tab;
  u8       first[128];
  u8       at[128][128];
} runeindex;

runeindex hoonindex, specindex;

runedef *findrune(parser *p, size *pos, runedef *tab, size n) {
  runeindex *x = tab == hoonrunes ? &hoonindex : &specindex;
  if (x->tab != tab) {
    for (size i = n - 1; i >= 0; i--) {
      x->first[tab[i].c1] = 1;
      x->at[tab[i].c1][tab[i].c2] = (u8)(i + 1);
    }
    x->tab = tab;
  }
  if (*pos >= p->len || p->buf[*pos] >= 128 || !x->first[p->buf[*pos]]) {
    reach(p, *pos);
    return 0;
  }
  u8 c1 = p->buf[(*pos)++];
  reach(p, *pos);
  if (*pos < p->len && p->buf[*pos] < 128 && x->at[c1][p->buf[*pos]]) {
    runedef *r = &tab[x->at[c1][p->buf[*pos]] - 1];
    (*pos)++;
    reach(p, *pos);
    return r;
  }
  return 0;
}

noun expression(parser *p, size *pos, b32 tol) {
  size s = *pos;
  runedef *r = findrune(p, pos, hoonrunes, countof(hoonrunes));
  if (!r) return 0;
  noun v = toad(p, pos, tol, r->har);
  if (!v) return 0;
  note(p, s, s + 2, tok_keyword);
  switch (r->kind) {
  case rune_runo: return C3(term(r->tag), nul, v);
  case rune_ktcl: return C3(K("ktcl"), term(r->tag), v);
  case rune_bare: return v;
  }
  return C2(term(r->tag), v);
}

// #/foo/@ud/bar=@t/... path specs
noun hasfaselem(parser *p, size *pos) {
  if (*pos >= p->len) {
    reach(p, *pos);
    return C3(K("leaf"), K("tas"), nul);
  }
  u8 c = p->buf[*pos];
  size s = *pos;
  noun r, a;
  if (c == '$') {
    chr(p, pos, '$');
    return C3(K("leaf"), K("tas"), nul);
  }
  if (c >= 'a' && c <= 'z') {
    if ((r = sym(p, pos)) && chr(p, pos, '=') && chr(p, pos, '@')) {
      a = mota(p, pos);
      noun base;
      if (atomeqc(a, "f")) base = C2(K("base"), K("flag"));
      else if (atomeqc(a, "n")) base = C2(K("base"), K("null"));
      else base = C3(K("base"), K("atom"), a);
      return C4(K("bccl"), C3(K("leaf"), K("tas"), a), C3(K("bcts"), r, base), nul);
    }
    *pos = s;
    return C3(K("leaf"), K("tas"), sym(p, pos));
  }
  if (c == '@') {
    chr(p, pos, '@');
    a = mota(p, pos);
    return C4(K("bccl"), C3(K("leaf"), K("tas"), a), C3(K("base"), K("atom"), a), nul);
  }
  if (c == '?') {
    chr(p, pos, '?');
    return C4(K("bccl"), C3(K("leaf"), K("tas"), K("f")), C2(K("base"), K("flag")), nul);
  }
  if (c == '~') {
    chr(p, pos, '~');
    return C4(K("bccl"), C3(K("leaf"), K("tas"), K("n")), C2(K("base"), K("null")), nul);
  }
  reach(p, *pos);
  return C3(K("leaf"), K("tas"), nul);
}

noun hasfas(parser *p, size *pos) {
  if (!chr(p, pos, '#') || !chr(p, pos, '/')) return 0;
  nouns xs = {0};
  *push(&xs, p->a) = hasfaselem(p, pos);
  for (;;) {
    size s = *pos;
    farmark m = farget(p);
    b32 fastar = chr(p, pos, '/') && chr(p, pos, '*');
    *pos = s;
    if (!lookahead(p, s, fastar, m) && chr(p, pos, '/')) {
      *push(&xs, p->a) = hasfaselem(p, pos);
      continue;
    }
    *pos = s;
    break;
  }
  size s = *pos;
  noun e;
  if (chr(p, pos, '/') && chr(p, pos, '*')) {
    e = C2(K("base"), K("noun"));
  } else {
    *pos = s;
    e = C2(K("base"), K("null"));
  }
  *push(&xs, p->a) = e;
  return C2(K("bccl"), nounslist(p, &xs));
}

noun structure(parser *p, size *pos, b32 tol) {
  if (peek(p, *pos, '#')) return hasfas(p, pos);
  size s = *pos;
  runedef *r = findrune(p, pos, specrunes, countof(specrunes));
  if (!r) return 0;
  noun v = toad(p, pos, tol, r->har);
  if (!v) return 0;
  note(p, s, s + 2, tok_keyword);
  noun make = K("make");
  noun tag = term(r->tag);
  if (atomeqc(tag, "cnkt")) {
    return C3(make, hd(v), C4(hd(tl(v)), hd(tl(tl(v))), tl(tl(tl(v))), nul));
  }
  if (atomeqc(tag, "cnls")) return C3(make, hd(v), C3(hd(tl(v)), tl(tl(v)), nul));
  if (atomeqc(tag, "cnhp")) return C3(make, hd(v), C2(tl(v), nul));
  if (atomeqc(tag, "cndt")) return C3(make, tl(v), C2(hd(v), nul));
  if (atomeqc(tag, "cncl")) return C3(make, hd(v), tl(v));
  return C2(term(r->tag), v);
}

// Irregular suffixes: a=b, a:b, a^b, a(b c)

noun longx(parser *p, size *pos, b32 tol) {
  noun ros = scat(p, pos);
  if (!ros) return 0;
  size s = *pos;
  noun x;
  char kind = 0;
  if (chr(p, pos, '=') && (x = wide(p, pos))) kind = '=';
  if (!kind) {
    *pos = s;
    if (chr(p, pos, ':') && (x = wide(p, pos))) kind = ':';
  }
  if (!kind) {
    *pos = s;
    if (chr(p, pos, '^') && (x = wide(p, pos))) kind = '^';
  }
  if (!kind) {
    *pos = s;
    if (chr(p, pos, '(')) {
      nouns xs = {0};
      noun w, h;
      for (;;) {
        size t = *pos;
        if (xs.len && !(chr(p, pos, ',') && ace(p, pos))) {
          *pos = t;
          break;
        }
        if ((w = rope(p, pos)) && ace(p, pos) && (h = wide(p, pos))) {
          *push(&xs, p->a) = C2(w, h);
          continue;
        }
        *pos = t;
        break;
      }
      if (xs.len && chr(p, pos, ')')) {
        x = nounslist(p, &xs);
        kind = 'l';
      }
    }
  }
  if (!kind) {
    *pos = s;
    return ros;
  }
  noun r = 0;
  switch (kind) {
  case ':':
    if (!nouneq(ros, C2(K("base"), K("flag")))) r = C3(K("tsgl"), ros, x);
    break;
  case 'l': {
    noun w = reek(p, ros);
    if (w) r = C3(K("cnts"), w, x);
    break;
  }
  case '^':
    r = C2(ros, x);
    break;
  case '=': {
    noun k = flay(p, ros);
    if (k) r = C3(K("ktts"), k, x);
    break;
  }
  }
  if (!r) {
    *pos = s;
    return ros;
  }
  return r;
}

noun longr(parser *p, size *pos) {
  return longx(p, pos, 0);
}

noun tallin(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r;
  if ((r = expression(p, pos, 1))) return wart(p, s, *pos, r);
  *pos = s;
  if ((r = longr(p, pos))) return wart(p, s, *pos, r);
  *pos = s;
  if ((r = lute(p, pos))) return wart(p, s, *pos, r);
  *pos = s;
  if ((r = sailapex(p, pos, 1))) return wart(p, s, *pos, r);
  return 0;
}

noun tallx(parser *p, size *pos, b32 tol) { return within(p, pos, tol, tallin); }

noun widein(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r;
  if ((r = expression(p, pos, 0))) return wart(p, s, *pos, r);
  *pos = s;
  if ((r = longr(p, pos))) return wart(p, s, *pos, r);
  *pos = s;
  if ((r = sailapex(p, pos, 0))) return wart(p, s, *pos, r);
  return 0;
}

noun widex(parser *p, size *pos, b32 tol) { return within(p, pos, tol, widein); }

noun tillin(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r;
  if ((r = structure(p, pos, 1))) return wart(p, s, *pos, r);
  *pos = s;
  if ((r = scad(p, pos))) return wart(p, s, *pos, r);
  return 0;
}

noun tillx(parser *p, size *pos, b32 tol) { return within(p, pos, tol, tillin); }

noun wydein(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r;
  if ((r = structure(p, pos, 0))) return wart(p, s, *pos, r);
  *pos = s;
  if ((r = scad(p, pos))) return wart(p, s, *pos, r);
  return 0;
}

noun wydex(parser *p, size *pos, b32 tol) { return within(p, pos, tol, wydein); }

noun tall(parser *p, size *pos) { return tallx(p, pos, 1); }
noun wide(parser *p, size *pos) { return widex(p, pos, 0); }
noun till(parser *p, size *pos) { return tillx(p, pos, 1); }
noun wyde(parser *p, size *pos) { return wydex(p, pos, 0); }

// a whole file, as +vest
noun vest(parser *p, size *pos) {
  PROF(parse);
  gay(p, pos);
  noun r = tall(p, pos);
  if (!r || spent(p)) return 0;
  gay(p, pos);
  if (*pos != p->len) {
    reach(p, *pos);
    return 0;
  }
  return r;
}

// Sail, the XML template syntax, following +sail. tf is in-tall-form,
// lin forbids newlines in quoted text.

noun cram(parser *p, size *pos);

typedef struct {
  b32 tf;
  b32 lin;
} sailmode;

noun wideinnertop(parser *p, size *pos, sailmode m);
noun widetop(parser *p, size *pos, sailmode m);
noun toplevel(parser *p, size *pos, sailmode m);

// a text node, as ;/(tape)
noun textnode(parser *p, noun tape) {
  return C2(C2(nul, C2(C2(nul, tape), nul)), nul);
}

noun tuna_mode(parser *p, size *pos) {
  if (chr(p, pos, '-')) return K("tape");
  if (chr(p, pos, '+')) return K("manx");
  if (chr(p, pos, '*')) return K("marl");
  if (chr(p, pos, '%')) return K("call");
  return 0;
}

noun amane(parser *p, size *pos) {
  noun a = mixedsym(p, pos);
  if (!a) return 0;
  size s = *pos;
  noun b;
  if (chr(p, pos, '_') && (b = mixedsym(p, pos))) return C2(a, b);
  *pos = s;
  return a;
}

// a wide hoon, as a list of beers
noun hopefullyquote(parser *p, size *pos) {
  noun a = wide(p, pos);
  if (!a) return 0;
  if (tagis(a, "knit")) return tl(a);
  return C2(C2(nul, a), nul);
}

noun wideattr(parser *p, size *pos) {
  noun n, v;
  if (!(n = amane(p, pos)) || !ace(p, pos) || !(v = hopefullyquote(p, pos))) return 0;
  return C2(n, v);
}

noun wideattrs(parser *p, size *pos) {
  size s = *pos;
  if (!chr(p, pos, '(')) {
    *pos = s;
    return nul;
  }
  nouns xs = {0};
  size t = *pos;
  noun a = wideattr(p, pos);
  if (a) {
    *push(&xs, p->a) = a;
    for (;;) {
      t = *pos;
      if (jest(p, pos, ", ") && (a = wideattr(p, pos))) {
        *push(&xs, p->a) = a;
        continue;
      }
      *pos = t;
      break;
    }
  } else {
    *pos = t;
  }
  if (!chr(p, pos, ')')) {
    *pos = s;
    return nul;
  }
  return nounslist(p, &xs);
}

noun taghead(parser *p, size *pos) {
  noun n = amane(p, pos);
  if (!n) return 0;
  nouns attrs = {0};
  size s = *pos;
  noun x;
  if (chr(p, pos, '#') && (x = sym(p, pos))) {
    *push(&attrs, p->a) = C2(K("id"), tapeatom(p, x));
  } else {
    *pos = s;
  }
  nouns classes = {0};
  for (;;) {
    s = *pos;
    if (chr(p, pos, '.') && (x = sym(p, pos))) {
      *push(&classes, p->a) = x;
      continue;
    }
    *pos = s;
    break;
  }
  if (classes.len) {
    noun t = nul;
    for (size i = classes.len - 1; i >= 0; i--) {
      if (i != classes.len - 1) t = C2(D(' '), t);
      t = weld(p, tapeatom(p, classes.data[i]), t);
    }
    *push(&attrs, p->a) = C2(K("class"), t);
  }
  s = *pos;
  char *k = chr(p, pos, '/') ? "href" : (*pos = s, chr(p, pos, '@')) ? "src" : 0;
  if (k && (x = soil(p, pos))) {
    *push(&attrs, p->a) = C2(term(k), x);
  } else {
    *pos = s;
  }
  noun c = wideattrs(p, pos);
  return C2(n, weld(p, nounslist(p, &attrs), c));
}

// a tuna or a marl, to a marl
noun droptop(parser *p, noun e) {
  if (atomis(hd(e), 0)) return C2(tl(e), nul);
  return tl(e);
}

noun jointops(parser *p, nouns *es) {
  noun r = nul;
  for (size i = es->len - 1; i >= 0; i--) r = weld(p, droptop(p, es->data[i]), r);
  return r;
}

noun collapsechars(parser *p, noun reb, b32 tf) {
  nouns out = {0};
  nouns sim = {0};
  for (; iscell(reb); reb = tl(reb)) {
    noun i = hd(reb);
    if (isatom(i)) {
      *push(&sim, p->a) = i;
      continue;
    }
    if (sim.len) *push(&out, p->a) = textnode(p, nounslist(p, &sim));
    sim.len = 0;
    sim.data = 0;
    sim.cap = 0;
    *push(&out, p->a) = i;
  }
  if (tf) {
    while (sim.len && atomis(sim.data[sim.len-1], ' ')) sim.len--;
    *push(&sim, p->a) = D('\n');
  }
  if (sim.len) *push(&out, p->a) = textnode(p, nounslist(p, &sim));
  return nounslist(p, &out);
}

noun sump(parser *p, size *pos);

noun wideelems(parser *p, size *pos, sailmode m) {
  nouns es = {0};
  for (;;) {
    size s = *pos;
    noun e;
    if (ace(p, pos) && (e = wideinnertop(p, pos, m))) {
      *push(&es, p->a) = e;
      continue;
    }
    *pos = s;
    return jointops(p, &es);
  }
}

noun bracketedelem(parser *p, size *pos, sailmode m) {
  noun h, e;
  if (!chr(p, pos, '{') || !(h = taghead(p, pos))) return 0;
  e = wideelems(p, pos, m);
  if (!chr(p, pos, '}')) return 0;
  return C2(h, e);
}

noun inlineembed(parser *p, size *pos, sailmode m) {
  size s = *pos;
  noun r, x;
  sailmode w = {0, m.lin};
  if (chr(p, pos, ';') && (r = bracketedelem(p, pos, w))) return r;
  *pos = s;
  if ((x = tuna_mode(p, pos)) && (r = sump(p, pos))) return C2(x, r);
  *pos = s;
  if ((r = sump(p, pos))) return C2(K("tape"), r);
  return 0;
}

noun quoteinnards(parser *p, size *pos, sailmode m) {
  nouns xs = {0};
  for (;;) {
    size s = *pos;
    noun r;
    i32 v;
    u8 c;
    if (chr(p, pos, '\\')) {
      size t = *pos;
      if (*pos < p->len) {
        c = p->buf[*pos];
        if (c == '-' || c == '+' || c == '*' || c == '%' || c == ';' || c == '{'
            || c == '\\' || c == '"') {
          chr(p, pos, c);
          *push(&xs, p->a) = D(c);
          continue;
        }
      }
      reach(p, t);
      if (bix(p, pos, &v)) {
        *push(&xs, p->a) = D((u64)v);
        continue;
      }
    }
    *pos = s;
    if ((r = inlineembed(p, pos, m))) {
      *push(&xs, p->a) = r;
      continue;
    }
    *pos = s;
    farmark f = farget(p);
    b32 stop = lookahead(p, s, chr(p, pos, '\\'), f);
    if (!stop) {
      *pos = s;
      f = farget(p);
      stop = lookahead(p, s, chr(p, pos, '{'), f);
    }
    if (!stop && !m.tf) {
      *pos = s;
      f = farget(p);
      stop = lookahead(p, s, chr(p, pos, '"'), f);
    }
    *pos = s;
    if (!stop && prn(p, pos, &c)) {
      *push(&xs, p->a) = D(c);
      continue;
    }
    *pos = s;
    if (!m.lin && chr(p, pos, '\n')) {
      *push(&xs, p->a) = D('\n');
      continue;
    }
    *pos = s;
    return nounslist(p, &xs);
  }
}

sailmode quotemode;

b32 tripledoq(parser *p, size *pos) { return jest(p, pos, "\"\"\""); }

noun quoteblock(parser *p, size *pos) {
  sailmode m = quotemode;
  m.lin = 0;
  return collapsechars(p, quoteinnards(p, pos, m), m.tf);
}

noun widequote(parser *p, size *pos, sailmode m) {
  size s = *pos;
  farmark f = farget(p);
  if (!lookahead(p, s, jest(p, pos, "\"\"\""), f)) {
    *pos = s;
    if (chr(p, pos, '"')) {
      noun r = collapsechars(p, quoteinnards(p, pos, m), m.tf);
      if (chr(p, pos, '"')) return r;
    }
  }
  *pos = s;
  sailmode saved = quotemode;
  quotemode = m;
  noun r = inde(p, pos, tripledoq, tripledoq, quoteblock);
  quotemode = saved;
  return r;
}

noun wideparenelems(parser *p, size *pos, sailmode m) {
  if (!chr(p, pos, '(')) return 0;
  nouns es = {0};
  size s = *pos;
  noun e = wideinnertop(p, pos, m);
  if (e) {
    *push(&es, p->a) = e;
    for (;;) {
      s = *pos;
      if (ace(p, pos) && (e = wideinnertop(p, pos, m))) {
        *push(&es, p->a) = e;
        continue;
      }
      *pos = s;
      break;
    }
  } else {
    *pos = s;
  }
  if (!chr(p, pos, ')')) return 0;
  return jointops(p, &es);
}

noun wrappedelems(parser *p, size *pos, sailmode m) {
  size s = *pos;
  noun r;
  if ((r = wideparenelems(p, pos, m))) return r;
  *pos = s;
  if ((r = qut(p, pos))) return C2(textnode(p, tapeatom(p, r)), nul);
  *pos = s;
  if ((r = widetop(p, pos, m))) return droptop(p, r);
  return 0;
}

noun widetail(parser *p, size *pos, sailmode m) {
  size s = *pos;
  noun r;
  if (chr(p, pos, ':') && (r = wrappedelems(p, pos, m))) return r;
  *pos = s;
  if (chr(p, pos, ';')) return nul;
  *pos = s;
  return nul;
}

noun widetop(parser *p, size *pos, sailmode m) {
  size s = *pos;
  noun r, h;
  if ((r = widequote(p, pos, m))) return C2(NO, r);
  *pos = s;
  if ((r = wideparenelems(p, pos, m))) return C2(NO, r);
  *pos = s;
  if ((h = taghead(p, pos))) return C3(YES, h, widetail(p, pos, m));
  return 0;
}

noun wideinnertop(parser *p, size *pos, sailmode m) {
  size s = *pos;
  noun r, x;
  if ((r = widetop(p, pos, m))) return r;
  *pos = s;
  if ((x = tuna_mode(p, pos)) && (r = wide(p, pos))) return C3(YES, x, r);
  return 0;
}

noun scriptorstyle(parser *p, size *pos) {
  size s = *pos;
  noun n;
  if (jest(p, pos, "script")) {
    n = K("script");
  } else {
    *pos = s;
    if (!jest(p, pos, "style")) return 0;
    n = K("style");
  }
  return C2(n, wideattrs(p, pos));
}

noun scriptstyletail(parser *p, size *pos) {
  if (!gap(p, pos)) return 0;
  nouns xs = {0};
  for (;;) {
    size s = *pos;
    if (xs.len && !gap(p, pos)) {
      *pos = s;
      break;
    }
    if (!chr(p, pos, ';')) {
      *pos = s;
      break;
    }
    size t = *pos;
    size start;
    if (ace(p, pos)) {
      start = *pos;
      while (prn(p, pos, 0)) {}
      *push(&xs, p->a) = textnode(p, tapeatom(p, atombytes(p->a, p->buf + start, *pos - start)));
    } else {
      *pos = t;
      *push(&xs, p->a) = textnode(p, tape(p, "\n"));
    }
  }
  if (!xs.len) return 0;
  if (!gap(p, pos) || !duz(p, pos)) return 0;
  return nounslist(p, &xs);
}

noun tallkids(parser *p, size *pos, sailmode m) {
  nouns es = {0};
  for (;;) {
    size s = *pos;
    if (es.len && !gap(p, pos)) {
      *pos = s;
      break;
    }
    size t = *pos;
    noun r = toplevel(p, pos, m);
    if (!r) {
      *pos = t;
      noun c = cram(p, pos);
      if (c) r = C2(NO, c);
    }
    if (!r) {
      *pos = s;
      break;
    }
    *push(&es, p->a) = r;
  }
  if (!es.len) return 0;
  return jointops(p, &es);
}

noun talltail(parser *p, size *pos, sailmode m) {
  size s = *pos;
  noun r;
  sailmode w = {0, m.lin};
  if (chr(p, pos, ';')) return nul;
  *pos = s;
  if (chr(p, pos, ':') && (r = wrappedelems(p, pos, w))) return r;
  *pos = s;
  if (chr(p, pos, ':') && ace(p, pos) && (r = quoteinnards(p, pos, m))) {
    return collapsechars(p, r, 0);
  }
  *pos = s;
  if (gap(p, pos) && (r = tallkids(p, pos, m)) && gap(p, pos) && duz(p, pos)) return r;
  return 0;
}

noun tallattrs(parser *p, size *pos) {
  nouns xs = {0};
  for (;;) {
    size s = *pos;
    noun n, v;
    if (gap(p, pos) && chr(p, pos, '=') && (n = amane(p, pos)) && gap(p, pos)
        && (v = hopefullyquote(p, pos))) {
      *push(&xs, p->a) = C2(n, v);
      continue;
    }
    *pos = s;
    return nounslist(p, &xs);
  }
}

// tall sail nests elements in elements without going through tall
noun tallelem(parser *p, size *pos, sailmode m) {
  noun h, b, c;
  if (!enter(p)) return 0;
  if (!(h = taghead(p, pos))
      || (b = tallattrs(p, pos), !(c = talltail(p, pos, m)))) {
    leave(p);
    return 0;
  }
  leave(p);
  return C2(C2(hd(h), weld(p, tl(h), b)), c);
}

noun talltop(parser *p, size *pos, sailmode m) {
  size s = *pos;
  noun r, x;
  if (ace(p, pos)) {
    while (ace(p, pos)) {}
    if ((r = quoteinnards(p, pos, m))) return C2(NO, collapsechars(p, r, m.tf));
  }
  *pos = s;
  if ((x = scriptorstyle(p, pos)) && (r = scriptstyletail(p, pos))) return C3(YES, x, r);
  *pos = s;
  if ((r = tallelem(p, pos, m))) return C2(YES, r);
  *pos = s;
  if ((r = widequote(p, pos, m))) return C2(NO, r);
  *pos = s;
  if (chr(p, pos, '=') && (r = talltail(p, pos, m))) return C2(NO, r);
  *pos = s;
  if (chr(p, pos, '>') && gap(p, pos) && (r = cram(p, pos))) {
    return C3(YES, C2(K("div"), nul), r);
  }
  *pos = s;
  if ((x = tuna_mode(p, pos)) && gap(p, pos) && (r = tall(p, pos))) {
    return C3(NO, C2(x, r), nul);
  }
  *pos = s;
  return C3(NO, textnode(p, tape(p, "\n")), nul);
}

noun toplevel(parser *p, size *pos, sailmode m) {
  if (!chr(p, pos, ';')) return 0;
  return m.tf ? talltop(p, pos, m) : widetop(p, pos, m);
}

noun sailapex(parser *p, size *pos, b32 tall) {
  sailmode m = {tall, 1};
  noun r = toplevel(p, pos, m);
  if (!r) return 0;
  if (atomis(hd(r), 0)) return C2(K("xray"), tl(r));
  return C2(K("mcts"), tl(r));
}


// Cram, the markdown in ;> blocks, following +cram. Inline text is
// parsed into grafs, then collected into a tarp of xml nodes.

noun werk(parser *p, size *pos);

b32 whit(parser *p, size *pos) {
  if (!chr(p, pos, ' ') && !chr(p, pos, '\n')) return 0;
  for (;;) {
    size s = *pos;
    if (chr(p, pos, ' ') || chr(p, pos, '\n')) continue;
    *pos = s;
    return 1;
  }
}

noun textgraf(parser *p, noun tape) {
  return C2(K("text"), tape);
}

// characters up to tem, with \ escapes, as +calf
noun calf(parser *p, size *pos, u8 tem) {
  nouns xs = {0};
  for (;;) {
    size s = *pos;
    if (chr(p, pos, '\\') && chr(p, pos, tem)) {
      *push(&xs, p->a) = D(tem);
      continue;
    }
    *pos = s;
    farmark m = farget(p);
    u8 c;
    if (!lookahead(p, s, chr(p, pos, tem), m)) {
      *pos = s;
      if (prn(p, pos, &c)) {
        *push(&xs, p->a) = D(c);
        continue;
      }
    }
    *pos = s;
    return nounslist(p, &xs);
  }
}

// the raw text up to tem, as +cash
size cash(parser *p, size *pos, u8 tem) {
  size start = *pos;
  for (;;) {
    size s = *pos;
    if (whit(p, pos)) continue;
    *pos = s;
    if (chr(p, pos, '\\') && chr(p, pos, tem)) continue;
    *pos = s;
    farmark m = farget(p);
    if (!lookahead(p, s, chr(p, pos, tem), m)) {
      *pos = s;
      if (prn(p, pos, 0)) continue;
    }
    *pos = s;
    return *pos - start;
  }
}

noun slicetape(parser *p, size start, size end) {
  return tapeatom(p, atombytes(p->a, p->buf + start, end - start));
}

// reparse the text up to tem with werk, as (cool (cash tem) werk)
noun coolwerk(parser *p, size *pos, u8 tem) {
  size start = *pos;
  hair h = hairat(p, start);
  size n = cash(p, pos, tem);
  parser q = subparser(p, p->buf + start, n, h.line);
  q.col = h.col;
  size qpos = 0;
  noun r = werk(&q, &qpos);
  if (!r || qpos != n) {
    reach(p, start);
    return 0;
  }
  return r;
}

// either whitespace as a space, or nothing at the start of a line
noun spacesol(parser *p, size *pos) {
  size s = *pos;
  if (whit(p, pos)) return tape(p, " ");
  *pos = s;
  if (hairat(p, s).col == 1) return nul;
  reach(p, s);
  return 0;
}

// lookahead for whitespace, as ;~(simu whit (easy ~))
b32 simuwhit(parser *p, size pos) {
  return whit(p, &pos);
}

noun hoonconstant(parser *p, size *pos) {
  size s = *pos;
  noun r;
  u8 c;
  if (chr(p, pos, '0') && (low(p, pos, &c) || hig(p, pos, &c) || nud(p, pos, &c)
                           || chr(p, pos, '-'))) {
    *pos = s;
    if (bisk(p, pos)) return nul;
  }
  *pos = s;
  if (tash(p, pos)) return nul;
  *pos = s;
  if (chr(p, pos, '.') && perd(p, pos)) return nul;
  *pos = s;
  if (chr(p, pos, '~')) {
    size t = *pos;
    if (twid(p, pos)) return nul;
    *pos = t;
    return nul;
  }
  *pos = s;
  if (chr(p, pos, '%')) {
    size t = *pos;
    if (sym(p, pos)) return nul;
    *pos = t;
    if (chr(p, pos, '$') || chr(p, pos, '&') || chr(p, pos, '|')) return nul;
    if ((r = qut(p, pos))) return nul;
    *pos = t;
    if (nuck(p, pos)) return nul;
  }
  return 0;
}

noun wordin(parser *p, size *pos, b32 tol) {
  size s = *pos;
  noun r, x;
  u8 c;
  if (low(p, pos, &c) || hig(p, pos, &c)) {
    while (nud(p, pos, 0) || low(p, pos, 0) || hig(p, pos, 0) || chr(p, pos, '-')) {}
    return C2(textgraf(p, slicetape(p, s, *pos)), nul);
  }
  *pos = s;
  if (chr(p, pos, '\\')) {
    size t = *pos;
    farmark m = farget(p);
    if (!lookahead(p, t, chr(p, pos, ' '), m)) {
      *pos = t;
      if (prn(p, pos, &c)) return C2(textgraf(p, C2(D(c), nul)), nul);
    }
  }
  *pos = s;
  if (chr(p, pos, '\\') && chr(p, pos, '\n')) {
    noun br = C2(C2(K("br"), nul), nul);
    return C2(C2(K("expr"), br), nul);
  }
  *pos = s;
  if (chr(p, pos, '*') && (r = coolwerk(p, pos, '*')) && chr(p, pos, '*')) {
    return C2(C2(K("bold"), r), nul);
  }
  *pos = s;
  if (chr(p, pos, '_') && (r = coolwerk(p, pos, '_')) && chr(p, pos, '_')) {
    return C2(C2(K("talc"), r), nul);
  }
  *pos = s;
  if (chr(p, pos, '"') && (r = coolwerk(p, pos, '"')) && chr(p, pos, '"')) {
    return C2(C2(K("quod"), r), nul);
  }
  *pos = s;
  if (chr(p, pos, '`') && (r = calf(p, pos, '`')) && chr(p, pos, '`')) {
    return C2(C2(K("code"), r), nul);
  }
  *pos = s;
  if (chr(p, pos, '+') && (chr(p, pos, '+') || chr(p, pos, '$') || chr(p, pos, '*'))
      && low(p, pos, 0)) {
    while (nud(p, pos, 0) || low(p, pos, 0) || chr(p, pos, '-') || chr(p, pos, ':')) {}
    return C2(C2(K("code"), slicetape(p, s, *pos)), nul);
  }
  *pos = s;
  if (chr(p, pos, '[') && (r = coolwerk(p, pos, ']')) && chr(p, pos, ']')) {
    size t = *pos;
    if (!whit(p, pos)) *pos = t;
    if (chr(p, pos, '(')) {
      size u = *pos;
      size n = cash(p, pos, ')');
      if (chr(p, pos, ')')) {
        return C2(C3(K("link"), r, slicetape(p, u, u + n)), nul);
      }
    }
  }
  *pos = s;
  if (chr(p, pos, '!') && chr(p, pos, '[')) {
    size u = *pos;
    size n = cash(p, pos, ']');
    if (chr(p, pos, ']')) {
      size t = *pos;
      if (!whit(p, pos)) *pos = t;
      if (chr(p, pos, '(')) {
        size v = *pos;
        size m = cash(p, pos, ')');
        if (chr(p, pos, ')')) {
          return C2(C3(K("mage"), slicetape(p, u, u + n), slicetape(p, v, v + m)), nul);
        }
      }
    }
  }
  *pos = s;
  if ((x = spacesol(p, pos)) && chr(p, pos, '#')) {
    size t = *pos;
    if (wide(p, pos) && simuwhit(p, *pos)) {
      return C3(textgraf(p, x), C2(K("code"), slicetape(p, t, *pos)), nul);
    }
  }
  *pos = s;
  if ((x = spacesol(p, pos))) {
    size t = *pos;
    if (hoonconstant(p, pos) && simuwhit(p, *pos)) {
      return C3(textgraf(p, x), C2(K("code"), slicetape(p, t, *pos)), nul);
    }
  }
  *pos = s;
  if (whit(p, pos)) return C2(textgraf(p, tape(p, " ")), nul);
  *pos = s;
  sailmode m = {0, 1};
  if ((r = inlineembed(p, pos, m))) return C2(C2(K("expr"), r), nul);
  *pos = s;
  farmark f = farget(p);
  if (!lookahead(p, s, chr(p, pos, ' '), f)) {
    *pos = s;
    if (prn(p, pos, &c)) return C2(textgraf(p, C2(D(c), nul)), nul);
  }
  return 0;
}

noun word(parser *p, size *pos) { return within(p, pos, 0, wordin); }

noun werk(parser *p, size *pos) {
  nouns xs = {0};
  for (;;) {
    size s = *pos;
    noun w = word(p, pos);
    if (!w) {
      *pos = s;
      break;
    }
    for (; iscell(w); w = tl(w)) *push(&xs, p->a) = hd(w);
  }
  return nounslist(p, &xs);
}

noun downgrafs(parser *p, noun gaf);

noun downitem(parser *p, noun nex) {
  noun v = tl(nex);
  if (tagis(nex, "expr")) return C2(v, nul);
  if (tagis(nex, "bold")) return C2(C2(C2(K("b"), nul), downgrafs(p, v)), nul);
  if (tagis(nex, "talc")) return C2(C2(C2(K("i"), nul), downgrafs(p, v)), nul);
  if (tagis(nex, "code")) {
    return C2(C3(C2(K("code"), nul), textnode(p, v), nul), nul);
  }
  if (tagis(nex, "quod")) {
    noun open = C2(K("text"), tape(p, "\xe2\x80\x9c"));
    noun close = C2(K("text"), tape(p, "\xe2\x80\x9d"));
    return downgrafs(p, C2(open, weld(p, v, C2(close, nul))));
  }
  if (tagis(nex, "link")) {
    noun attrs = C2(C2(K("href"), tl(v)), nul);
    return C2(C2(C2(K("a"), attrs), downgrafs(p, hd(v))), nul);
  }
  // %mage
  noun alt = isatom(hd(v)) ? nul : C2(C2(K("alt"), hd(v)), nul);
  noun attrs = C2(C2(K("src"), tl(v)), alt);
  return C2(C2(C2(K("img"), attrs), nul), nul);
}

noun downgrafs(parser *p, noun gaf) {
  nouns out = {0};
  while (iscell(gaf)) {
    noun i = hd(gaf);
    if (!tagis(i, "text")) {
      for (noun l = downitem(p, i); iscell(l); l = tl(l)) *push(&out, p->a) = hd(l);
      gaf = tl(gaf);
      continue;
    }
    noun txt = nul;
    nouns fip = {0};
    for (; iscell(gaf) && tagis(hd(gaf), "text"); gaf = tl(gaf)) *push(&fip, p->a) = tl(hd(gaf));
    for (size j = fip.len - 1; j >= 0; j--) txt = weld(p, fip.data[j], txt);
    *push(&out, p->a) = textnode(p, txt);
  }
  return nounslist(p, &out);
}

noun down(parser *p, size *pos) {
  noun g = werk(p, pos);
  return downgrafs(p, g);
}

noun para(parser *p, size *pos) {
  size s = *pos;
  if (!whit(p, pos)) *pos = s;
  noun a = down(p, pos);
  if (isatom(a)) return nul;
  return C2(C2(C2(K("p"), nul), a), nul);
}

// lowercase alphanumerics of the header text, as +contents-to-id
void headerchars(parser *p, noun a, nouns *out) {
  for (; iscell(a); a = tl(a)) {
    noun i = hd(a);
    if (isatom(hd(i))) continue;
    noun g = hd(i);
    if (atomis(hd(g), 0) && iscell(tl(g)) && iscell(hd(tl(g))) && atomis(hd(hd(tl(g))), 0)
        && atomis(tl(tl(g)), 0) && atomis(tl(i), 0)) {
      for (noun v = tl(hd(tl(g))); iscell(v); v = tl(v)) {
        if (isatom(hd(v))) *push(out, p->a) = hd(v);
      }
      continue;
    }
    headerchars(p, tl(i), out);
  }
}

noun head(parser *p, size *pos) {
  while (ace(p, pos)) {}
  size n = 0;
  while (n < 6 && chr(p, pos, '#')) n++;
  if (!n || !whit(p, pos)) return 0;
  noun kids = down(p, pos);
  nouns cs = {0};
  headerchars(p, kids, &cs);
  nouns id = {0};
  for (size i = 0; i < cs.len; i++) {
    u64 c = atomlow(cs.data[i]);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) *push(&id, p->a) = D(c);
    else if (c >= 'A' && c <= 'Z') *push(&id, p->a) = D(c + 32);
    else *push(&id, p->a) = D('-');
  }
  u8 tag[2] = {'h', (u8)('0' + n)};
  noun attrs = C2(C2(K("id"), nounslist(p, &id)), nul);
  return C2(C2(C2(atombytes(p->a, tag, 2), attrs), kids), nul);
}

noun hrul(parser *p, size *pos) {
  while (ace(p, pos)) {}
  if (!jest(p, pos, "---")) return 0;
  while (chr(p, pos, '-')) {}
  if (!chr(p, pos, '\n')) return 0;
  return C2(C2(C2(K("hr"), nul), nul), nul);
}

b32 tics(parser *p, size *pos) {
  return jest(p, pos, "```") && chr(p, pos, '\n');
}

b32 spaces(parser *p, size *pos, size n) {
  for (size i = 0; i < n; i++) {
    if (!chr(p, pos, ' ')) return 0;
  }
  return 1;
}

noun fens(parser *p, size *pos, size col) {
  while (ace(p, pos)) {}
  if (!tics(p, pos)) return 0;
  nouns lines = {0};
  for (;;) {
    size s = *pos;
    if (spaces(p, pos, col - 1)) {
      size t = *pos;
      farmark m = farget(p);
      if (!lookahead(p, t, tics(p, pos), m)) {
        *pos = t;
        while (prn(p, pos, 0)) {}
        size e = *pos;
        if (chr(p, pos, '\n')) {
          *push(&lines, p->a) = slicetape(p, t, e);
          continue;
        }
      }
    }
    *pos = s;
    while (ace(p, pos)) {}
    if (chr(p, pos, '\n')) {
      *push(&lines, p->a) = nul;
      continue;
    }
    *pos = s;
    break;
  }
  if (!spaces(p, pos, col - 1) || !tics(p, pos)) return 0;
  noun txt = nul;
  for (size i = lines.len - 1; i >= 0; i--) txt = weld(p, lines.data[i], C2(D('\n'), txt));
  return C2(C3(C2(K("pre"), nul), textnode(p, txt), nul), nul);
}

noun cramexpr(parser *p, size *pos) {
  while (ace(p, pos)) {}
  sailmode m = {1, 1};
  noun r = toplevel(p, pos, m);
  if (!r) return 0;
  size s = *pos;
  farmark f = farget(p);
  b32 ok = gap(p, &s);
  farset(p, f);
  reach(p, *pos);
  if (!ok) return 0;
  return droptop(p, r);
}

// The line-oriented block structure

enum { mite_down, mite_lunt, mite_lime, mite_lord, mite_poem, mite_bloc, mite_head };

size miteindent[] = {2, 0, 2, 0, 8, 2, 0};
char *mitetag[] = {"", "ul", "li", "ol", "div", "blockquote", ""};

enum {
  sty_done, sty_stet, sty_dent,
  sty_rule, sty_fens, sty_expr,
  sty_head, sty_lint, sty_lite, sty_bloc, sty_poem,
  sty_text,
};

typedef struct {
  i32   mite;
  noun q;      // children, most recent first
} cramitem;

typedef struct {
  cramitem *data;
  size      len;
  size      cap;
} cramitems;

typedef struct {
  u8  *buf;
  size len;
} cramline;

typedef struct {
  cramline *data;
  size      len;
  size      cap;
} cramlines;

typedef struct {
  parser   *p;
  size      pos;
  b32       err;
  hair      errh;
  size      out;
  size      inr;
  cramitem  cur;
  cramitems hac;
  b32       par;
  hair      parloc;
  cramlines lines;
} cramstate;

b32 styend(i32 sty) { return sty <= sty_dent; }

// any number of # then a space
b32 headmark(parser *p, size *pos) {
  while (chr(p, pos, '#')) {}
  return ace(p, pos);
}

// classify a line, as +look; 0 for a blank line
b32 cramlook(cramstate *s, size pos, size *col, i32 *sty) {
  parser *p = s->p;
  while (ace(p, &pos)) {}
  *col = hairat(p, pos).col;
  size t = pos;
  if (chr(p, &t, '\n')) return 0;
  if (pos == p->len) {
    *sty = sty_done;
    return 1;
  }
  t = pos;
  if (duz(p, &t)) *sty = sty_stet;
  else if ((t = pos, jest(p, &t, "---"))) *sty = sty_rule;
  else if ((t = pos, jest(p, &t, "```"))) *sty = sty_fens;
  else if ((t = pos, chr(p, &t, ';'))) *sty = sty_expr;
  else if ((t = pos, headmark(p, &t))) *sty = sty_head;
  else if ((t = pos, chr(p, &t, '-') && ace(p, &t))) *sty = sty_lint;
  else if ((t = pos, chr(p, &t, '+') && ace(p, &t))) *sty = sty_lite;
  else if ((t = pos, chr(p, &t, '>') && ace(p, &t))) *sty = sty_bloc;
  else *sty = sty_text;
  if (!styend(*sty) && *col < s->out) *sty = sty_dent;
  return 1;
}

noun curtotarp(cramstate *s) {
  parser *p = s->p;
  noun kids = flop(p, s->cur.q);
  if (s->cur.mite == mite_down || s->cur.mite == mite_head) return kids;
  return C2(C2(C2(term(mitetag[s->cur.mite]), nul), kids), nul);
}

void closeitem(cramstate *s) {
  if (!s->hac.len) return;
  cramitem top = s->hac.data[--s->hac.len];
  noun q = weld(s->p, curtotarp(s), top.q);
  s->cur.mite = top.mite;
  s->cur.q = q;
}

void pushitem(cramstate *s, i32 mite) {
  *push(&s->hac, s->p->a) = s->cur;
  s->cur.mite = mite;
  s->cur.q = nul;
}

void seterr(cramstate *s, size line, size col) {
  s->err = 1;
  s->errh.line = line;
  s->errh.col = col;
}

void back(cramstate *s, size luc) {
  while (luc < s->inr) {
    size nex = miteindent[s->cur.mite];
    if (nex > s->inr - luc) {
      seterr(s, hairat(s->p, s->pos).line, luc);
      s->inr = luc;
      return;
    }
    closeitem(s);
    s->inr -= nex;
  }
}

enum { fin_more, fin_stop, fin_err };

// capture a line without its indentation, as +read-line
i32 readline(cramstate *s, cramline *out, hair *errh) {
  parser *p = s->p;
  size start = s->pos;
  size end = start;
  while (end < p->len && p->buf[end] != '\n') end++;
  u8 *b = new(p->a, u8, end - start + 1);
  size len = 0;
  for (;;) {
    if (s->pos == p->len) {
      *errh = hairat(p, s->pos);
      out->buf = b;
      out->len = 0;
      return fin_err;
    }
    u8 c = p->buf[s->pos];
    if (c != '\n') {
      if (s->inr > hairat(p, s->pos).col) {
        if (c != ' ') {
          *errh = hairat(p, s->pos);
          out->buf = b;
          out->len = 0;
          return fin_err;
        }
      } else {
        b[len++] = c;
      }
      s->pos++;
      continue;
    }
    while (len && b[len-1] == ' ') len--;
    out->buf = b;
    out->len = len;
    size col;
    i32 sty;
    if (cramlook(s, s->pos + 1, &col, &sty) && (sty == sty_stet || sty == sty_dent)) {
      return fin_stop;
    }
    s->pos++;
    return fin_more;
  }
}

noun linetape(parser *p, u8 *b, size len, char *suffix) {
  noun t = tape(p, suffix);
  for (size i = len - 1; i >= 0; i--) t = C2(D(b[i]), t);
  return t;
}

void closepar(cramstate *s) {
  parser *p = s->p;
  if (!s->par) return;
  if (s->cur.mite == mite_poem) {
    if (iscell(s->cur.q)) s->cur.q = C2(C2(C2(K("br"), nul), nul), s->cur.q);
    nouns ps = {0};
    for (size i = s->lines.len - 1; i >= 0; i--) {
      cramline l = s->lines.data[i];
      noun t = textnode(p, linetape(p, l.buf, l.len, "\n"));
      *push(&ps, p->a) = C2(C2(K("p"), nul), C2(t, nul));
    }
    s->cur.q = weld(p, nounslist(p, &ps), s->cur.q);
    s->par = 0;
    s->inr -= 8;
    closeitem(s);
    return;
  }
  size total = 0;
  for (size i = 0; i < s->lines.len; i++) total += s->inr + s->lines.data[i].len;
  u8 *yex = new(p->a, u8, total + 1);
  size n = 0;
  for (size i = 0; i < s->lines.len; i++) {
    for (size j = 1; j < s->inr; j++) yex[n++] = ' ';
    copy((byte*)yex + n, (byte*)s->lines.data[i].buf, s->lines.data[i].len);
    n += s->lines.data[i].len;
    yex[n++] = '\n';
  }
  parser q = subparser(p, yex, n, s->parloc.line);
  q.col = s->parloc.col;
  size qpos = 0;
  noun r = s->cur.mite == mite_head ? head(&q, &qpos) : para(&q, &qpos);
  if (r && qpos != n) {
    reach(&q, qpos);
    r = 0;
  }
  if (!r) {
    hair h = errhair(&q);
    seterr(s, h.line, h.col);
    return;
  }
  s->par = 0;
  s->cur.q = weld(p, r, s->cur.q);
  if (s->cur.mite == mite_head) closeitem(s);
}

void parseblock(cramstate *s, i32 sty) {
  parser *p = s->p;
  farmark m = farget(p);
  p->far = 0;
  p->farh = 0;
  size pos = s->pos;
  noun r;
  switch (sty) {
  case sty_expr: r = cramexpr(p, &pos); break;
  case sty_rule: r = hrul(p, &pos); break;
  default:       r = fens(p, &pos, s->inr); break;
  }
  if (!r) {
    hair h = errhair(p);
    farset(p, m);
    seterr(s, h.line, h.col);
    return;
  }
  farset(p, m);
  s->pos = pos;
  s->cur.q = weld(p, flop(p, r), s->cur.q);
}

void entr(cramstate *s, i32 mite) {
  s->inr += 2;
  s->pos += s->inr - hairat(s->p, s->pos).col;
  pushitem(s, mite);
}

void openitem(cramstate *s, i32 sty) {
  switch (sty) {
  case sty_poem: pushitem(s, mite_poem); break;
  case sty_head: pushitem(s, mite_head); break;
  case sty_bloc: entr(s, mite_bloc); break;
  case sty_lint:
    if (s->cur.mite != mite_lunt) pushitem(s, mite_lunt);
    entr(s, mite_lime);
    break;
  case sty_lite:
    if (s->cur.mite != mite_lord) pushitem(s, mite_lord);
    entr(s, mite_lime);
    break;
  }
}

void startpar(cramstate *s) {
  s->par = 1;
  s->parloc = hairat(s->p, s->pos);
  s->lines.len = 0;
  s->lines.data = 0;
  s->lines.cap = 0;
}

// returns 0 on a crash in the reference parser
b32 cramloop(cramstate *s) {
  parser *p = s->p;
  for (;;) {
    if (s->err) return 1;
    size col;
    i32 sty;
    if (!cramlook(s, s->pos, &col, &sty)) {
      cramline l;
      hair h;
      i32 fin = readline(s, &l, &h);
      if (fin == fin_err) {
        s->err = 1;
        s->errh = h;
        return 1;
      }
      if (fin == fin_stop) return 1;
      closepar(s);
      continue;
    }
    if (styend(sty)) return 1;
    if (!s->out) {
      s->out = col;
      s->inr = col;
    }
    i32 m = s->cur.mite;
    if (!s->par || ((m == mite_down || m == mite_lime || m == mite_bloc)
                    && (sty != sty_text || col > s->inr))) {
      closepar(s);
      back(s, col);
      size d = col - s->inr;
      if (d == 8) {
        sty = sty_poem;
      } else if (d != 0) {
        seterr(s, hairat(p, s->pos).line, col);
        return 1;
      }
      s->inr = col;
      if ((s->cur.mite == mite_lunt && sty != sty_lint)
          || (s->cur.mite == mite_lord && sty != sty_lite)) {
        closeitem(s);
      }
      if (sty == sty_rule || sty == sty_fens || sty == sty_expr) {
        parseblock(s, sty);
      } else if (sty != sty_text) {
        openitem(s, sty);
      }
      startpar(s);
      continue;
    }
    if (s->lines.len) {
      b32 ok;
      switch (s->cur.mite) {
      case mite_lord:
      case mite_lunt: return 0;
      case mite_head: ok = 0; break;
      case mite_poem: ok = col >= s->inr; break;
      default:        ok = col == s->inr;
      }
      if (!ok) {
        seterr(s, hairat(p, s->pos).line, col);
        return 1;
      }
    }
    cramline l;
    hair h;
    i32 fin = readline(s, &l, &h);
    *push(&s->lines, p->a) = l;
    if (fin == fin_err) {
      s->err = 1;
      s->errh = h;
      return 1;
    }
    if (fin == fin_stop) return 1;
  }
}

// cram blocks nest in cram blocks, through sail but not tall
noun cramin(parser *p, size *pos, b32 tol) {
  farmark m = farget(p);
  cramstate s = {0};
  s.p = p;
  s.pos = *pos;
  s.cur.mite = mite_down;
  s.cur.q = nul;
  if (!cramloop(&s)) {
    reach(p, s.pos);
    return 0;
  }
  farset(p, m);
  if (s.err) {
    p->farh = MAX(p->farh, hairkey(s.errh));
    return 0;
  }
  closepar(&s);
  while (s.hac.len) closeitem(&s);
  noun r = curtotarp(&s);
  if (s.pos == *pos) {
    reach(p, *pos);
    return 0;
  }
  reach(p, s.pos);
  *pos = s.pos;
  return r;
}

noun cram(parser *p, size *pos) { return within(p, pos, 0, cramin); }

// Sets, as +in. Like maps these are treaps, so equal sets are equal nouns.

noun setput(arena *a, noun s, noun b) {
  PROF(setput);
  if (isatom(s)) return mapnode(a, b, nul, nul);
  noun n = mapn(s);
  if (nouneq(b, n)) return s;
  if (gor(b, n)) {
    noun c = setput(a, mapl(s), b);
    if (mor(n, mapn(c))) return mapnode(a, n, c, mapr(s));
    return mapnode(a, mapn(c), mapl(c), mapnode(a, n, mapr(c), mapr(s)));
  }
  noun c = setput(a, mapr(s), b);
  if (mor(n, mapn(c))) return mapnode(a, n, mapl(s), c);
  return mapnode(a, mapn(c), mapnode(a, n, mapl(s), mapl(c)), mapr(c));
}

// elements in the order of +tap:in
void settap(noun s, nouns *out, arena *a) {
  if (isatom(s)) return;
  settap(mapr(s), out, a);
  *push(out, a) = mapn(s);
  settap(mapl(s), out, a);
}

noun setuni(arena *a, noun x, noun y) {
  PROF(setuni);
  if (nouneq(x, y)) return x;
  if (isatom(y)) return x;
  if (isatom(x)) return y;
  noun nx = mapn(x);
  noun ny = mapn(y);
  if (nouneq(ny, nx)) {
    return mapnode(a, ny, setuni(a, mapl(x), mapl(y)), setuni(a, mapr(x), mapr(y)));
  }
  if (mor(nx, ny)) {
    if (gor(ny, nx)) {
      noun l = setuni(a, mapl(x), mapnode(a, ny, mapl(y), nul));
      return setuni(a, mapnode(a, nx, l, mapr(x)), mapr(y));
    }
    noun r = setuni(a, mapr(x), mapnode(a, ny, nul, mapr(y)));
    return setuni(a, mapnode(a, nx, mapl(x), r), mapl(y));
  }
  if (gor(nx, ny)) {
    noun l = setuni(a, mapnode(a, nx, mapl(x), nul), mapl(y));
    return setuni(a, mapr(x), mapnode(a, ny, l, mapr(y)));
  }
  noun r = setuni(a, mapnode(a, nx, nul, mapr(x)), mapr(y));
  return setuni(a, mapl(x), mapnode(a, ny, mapl(y), r));
}

// Memos for +mint, +mull and +redo, keyed as vere's jets key theirs: by
// the subject, the expected type, dox for +mull, the hoon, and vet for
// +mint, but not by fan and rib, nor vet for +mull. So as in vere, the
// lazy batteries in a result are made with the door that first made it,
// and it's used with any other. As every noun is made once, keys hash
// and compare by handle.
typedef struct {
  noun sut;
  noun gol;
  noun dox;
  noun gen;
  u32  kind;   // see memokind
  noun res;
} memocell;

typedef struct {
  memocell *data;
  size      cap;
  size      len;
} memotable;

memotable memo;

enum memokind {
  memo_mint = 1,      // +1 without vet
  memo_mull = 3,
  memo_redo = 5,
};

u64 memohash(noun sut, noun gol, noun dox, noun gen, u32 kind) {
  u64 h = (u64)handlehash(sut) * 0x9e3779b97f4a7c15ull;
  h = (h ^ handlehash(gol)) * 0xc2b2ae3d27d4eb4full;
  h = (h ^ handlehash(dox)) * 0x9e3779b97f4a7c15ull;
  h = (h ^ handlehash(gen)) * 0xc2b2ae3d27d4eb4full;
  h ^= kind;
  return h ^ h >> 29;
}

size memoslot(noun sut, noun gol, noun dox, noun gen, u32 kind) {
  size mask = memo.cap - 1;
  size j = (size)(memohash(sut, gol, dox, gen, kind) >> 17) & mask;
  for (;; j = (j + 1) & mask) {
    memocell *c = &memo.data[j];
    if (!c->sut) return j;
    if (c->kind == kind && nouneq(c->gen, gen) && nouneq(c->sut, sut) && nouneq(c->gol, gol)
        && nouneq(c->dox, dox)) {
      return j;
    }
  }
}

// the memo's slot for a key, growing it as needed
size memofind(noun sut, noun gol, noun dox, noun gen, u32 kind) {
  PROF(memofind);
  if (memo.len*4 >= memo.cap*3) {
    memotable old = memo;
    memo.cap = old.cap ? old.cap * 2 : memomin;
    memo.data = new(&H.perm, memocell, memo.cap);
    for (size i = 0; i < old.cap; i++) {
      memocell *c = &old.data[i];
      if (c->sut) memo.data[memoslot(c->sut, c->gol, c->dox, c->gen, c->kind)] = *c;
    }
    if (old.cap) osrelease((byte*)old.data, (byte*)(old.data + old.cap));
  }
  return memoslot(sut, gol, dox, gen, kind);
}

void memoput(noun sut, noun gol, noun dox, noun gen, u32 kind, noun res) {
  size j = memofind(sut, gol, dox, gen, kind);
  if (!memo.data[j].sut) memo.len++;
  memo.data[j] = (memocell){sut, gol, dox, gen, kind, res};
}

// Types, following +ut. Types are the same nouns hoon uses:
//
//   %noun  %void  [%atom aura (unit @)]  [%cell type type]
//   [%core type coil]  [%face tool type]  [%fork (set type)]
//   [%hint [type note] type]  [%hold type hoon]
//
// Functions return 0 where hoon would crash.

typedef struct {
  noun sut;
  noun gen;
  noun res;
} playcell;

// Loop guards, like the fan of +rest and the gil of +nest, are sets in
// hoon but only ever asked for membership: here they are lists of up to
// three nouns in scratch memory, so they make no nouns.
typedef struct guard guard;
struct guard {
  guard *next;
  noun   a;
  noun   b;
  noun   c;
};

typedef struct {
  parser   *p;
  guard    *fan;      // [type hoon] pairs being evaluated by +rest
  // for the language server, to say where things are defined: with
  // spots, a face that +play makes is under a hint [%spot spot] of the
  // spot it made it at, and +fond keeps in lastface where the face it
  // found last came from, [%spot spot] or, for a face in a type a mold
  // made, [%made sut name], the mold's subject and name; and in bridged
  // what it found through a =, bridge
  b32       spots;
  noun      spot;     // the spot being played
  noun      lastface;
  noun      bridged;
  noun      ctx;      // the innermost such hint passed, while finding
} typer;


// the slot for a subject and hoon in the play memo, by the subject's
// value: +fuse rebuilds the types it narrows, and replaying their holds
// against each copy as new would type the same arms without end
// +play results by subject and hoon, for all files compiled
struct {
  playcell *data;
  size      cap;
  size      len;
} playmemo;

size playslot(typer *u, noun sut, noun gen) {
  u64 h = ((u64)handlehash(sut) * 0x9e3779b97f4a7c15ull) ^ ((u64)handlehash(gen) * 0xc2b2ae3d27d4eb4full);
  size mask = playmemo.cap - 1;
  size j = (size)(h >> 17) & mask;
  while (playmemo.data[j].sut && (!nouneq(playmemo.data[j].gen, gen) || !nouneq(playmemo.data[j].sut, sut))) {
    j = (j + 1) & mask;
  }
  return j;
}

#define TP parser *p = u->p

b32 isvoid(noun t) { return atomeqc(t, "void"); }
b32 isnountype(noun t) { return atomeqc(t, "noun"); }

noun tcell(typer *u, noun h, noun t) {
  TP;
  if (isvoid(h) || isvoid(t)) return K("void");
  return C3(K("cell"), h, t);
}

noun tcore(typer *u, noun pac, noun con) {
  TP;
  if (isvoid(pac)) return K("void");
  return C3(K("core"), pac, con);
}

noun thint(typer *u, noun pair, noun q) {
  TP;
  if (isvoid(q)) return K("void");
  if (isnountype(q)) return K("noun");
  return C3(K("hint"), pair, q);
}

noun tface(typer *u, noun giz, noun der) {
  TP;
  if (isvoid(der)) return K("void");
  return C3(K("face"), giz, der);
}

noun tfork(typer *u, noun *yed, size n) {
  PROF(tfork);
  TP;
  noun lez = nul;
  for (size i = 0; i < n; i++) {
    noun t = yed[i];
    if (isvoid(t)) continue;
    if (tagis(t, "fork")) lez = setuni(p->a, lez, tl(t));
    else lez = setput(p->a, lez, t);
  }
  if (isatom(lez)) return K("void");
  if (isatom(mapl(lez)) && isatom(mapr(lez))) return mapn(lez);
  return C2(K("fork"), lez);
}

noun tfork2(typer *u, noun a, noun b) {
  noun ts[2] = {a, b};
  return tfork(u, ts, 2);
}

noun tbool(typer *u) {
  TP;
  return tfork2(u, C3(K("atom"), K("f"), C2(nul, YES)), C3(K("atom"), K("f"), C2(nul, NO)));
}

// Axis arithmetic

// head or tail: 2 or 3
u64 cap(noun a) {
  size n = atombits(a);
  return atombit(a, n - 2) ? 3 : 2;
}

// axis within head or tail
noun mas(parser *p, noun a) {
  size n = atombits(a);
  noun low = atomend(p->a, a, n - 2);
  return atomor(p->a, low, atomlsh(p->a, D(1), n - 2));
}

// odor compatibility, as +fitz
b32 fitz(noun yaz, noun wix) {
  // split trailing size letter
  u8 ys = 0, ws = 0;
  size yl = alen(yaz), wl = alen(wix);
  if (yl && abyte(yaz, yl-1) >= 'A' && abyte(yaz, yl-1) <= 'Z') ys = abyte(yaz, --yl) - 64;
  if (wl && abyte(wix, wl-1) >= 'A' && abyte(wix, wl-1) <= 'Z') ws = abyte(wix, --wl) - 64;
  if (!(ys == 0 || ws == 0 || ws <= ys)) return 0;
  for (size i = 0; ; i++) {
    if (i >= yl || i >= wl) return 1;
    if (abyte(yaz, i) != abyte(wix, i)) return 0;
  }
}

// Coil access: [%core p=type q=[p=garb q=type r=[seminoun (map term tome)]]]

noun coreof(noun t) { return tl(tl(t)); }            // q.sut, the coil
noun coilgarb(noun c) { return hd(c); }
noun coilctx(noun c) { return hd(tl(c)); }
noun coilbat(noun c) { return tl(tl(tl(c))); }       // q.r, the chapters
noun garbvair(noun g) { return tl(tl(g)); }
noun garbpoly(noun g) { return hd(tl(g)); }

noun play(typer *u, noun sut, noun gen);
noun repo(typer *u, noun sut);
noun tpeek(typer *u, noun sut, char *way, noun axe);
noun fuse(typer *u, noun sut, noun ref);
noun crop(typer *u, noun sut, noun ref);
b32 nest(typer *u, noun sut, b32 tel, noun ref, b32 *ok);
noun find(typer *u, noun sut, char *way, noun hyp);
noun redo(typer *u, noun sut, noun ref);

// the head of each type in a list of [type foot]
noun tforkheads(typer *u, noun set) {
  TP;
  nouns xs = {0};
  settap(set, &xs, p->a);
  for (size i = 0; i < xs.len; i++) xs.data[i] = hd(xs.data[i]);
  return tfork(u, xs.data, xs.len);
}

b32 guardhas(guard *g, noun a, noun b, noun c) {
  for (; g; g = g->next) {
    if (nouneq(g->a, a) && nouneq(g->b, b) && nouneq(g->c, c)) return 1;
  }
  return 0;
}

guard *guardput(arena *ar, guard *g, noun a, noun b, noun c) {
  guard *n = new(ar, guard, 1);
  *n = (guard){g, a, b, c};
  return n;
}

noun rest(typer *u, noun typ, noun gen) {
  TP;
  if (guardhas(u->fan, typ, gen, 0)) return 0;
  guard *fan = u->fan;
  u->fan = guardput(p->a, fan, typ, gen, 0);
  noun r = play(u, typ, gen);
  u->fan = fan;
  return r;
}

noun repo(typer *u, noun sut) {
  PROF(repo);
  TP;
  if (tagis(sut, "core")) return C3(K("cell"), K("noun"), hd(tl(sut)));
  if (tagis(sut, "face") || tagis(sut, "hint")) return tl(tl(sut));
  if (tagis(sut, "hold")) return rest(u, hd(tl(sut)), tl(tl(sut)));
  if (isnountype(sut)) {
    return tfork2(u, C3(K("atom"), nul, nul), C3(K("cell"), K("noun"), K("noun")));
  }
  return 0;
}

// variance permissions, as +peel
void peel(char *way, noun met, b32 *sam, b32 *con) {
  if (atomeqc(met, "gold")) {
    *sam = *con = 1;
    return;
  }
  *con = 0;
  if (streq(way, "both")) *sam = 0;
  else if (streq(way, "free")) *sam = *con = 1;
  else if (streq(way, "read")) *sam = atomeqc(met, "zinc");
  else *sam = atomeqc(met, "iron");
}

noun peekx(typer *u, noun sut, char *way, noun axe, guard *gil) {
  PROF(peek);
  TP;
  for (;;) {
    if (atomis(axe, 1)) return sut;
    if (!alen(axe)) return 0;   // axis 0, where +cap and +mas crash
    u64 now = cap(axe);
    noun lat = mas(p, axe);
    if (tagis(sut, "atom") || isvoid(sut)) return K("void");
    if (tagis(sut, "cell")) {
      sut = now == 2 ? hd(tl(sut)) : tl(tl(sut));
      axe = lat;
      gil = 0;
      continue;
    }
    if (tagis(sut, "core")) {
      if (now != 3) return K("noun");
      b32 sam, con;
      peel(way, garbvair(coilgarb(coreof(sut))), &sam, &con);
      u64 tow = atomis(lat, 1) ? 1 : cap(lat);
      noun pay = hd(tl(sut));
      if ((sam && con) || (sam && tow == 2) || (con && tow == 3)) {
        sut = pay;
        axe = lat;
        gil = 0;
        continue;
      }
      if (!streq(way, "read")) return 0;
      noun h = sam ? peekx(u, pay, way, D(2), 0) : K("noun");
      noun t = con ? peekx(u, pay, way, D(3), 0) : K("noun");
      if (!h || !t) return 0;
      sut = tcell(u, h, t);
      axe = lat;
      gil = 0;
      continue;
    }
    if (tagis(sut, "fork")) {
      nouns xs = {0};
      settap(tl(sut), &xs, p->a);
      for (size i = 0; i < xs.len; i++) {
        if (!(xs.data[i] = peekx(u, xs.data[i], way, axe, gil))) return 0;
      }
      return tfork(u, xs.data, xs.len);
    }
    if (tagis(sut, "hold")) {
      if (guardhas(gil, sut, 0, 0)) return K("void");
      gil = guardput(p->a, gil, sut, 0, 0);
      if (!(sut = repo(u, sut))) return 0;
      continue;
    }
    if (isnountype(sut)) return K("noun");
    if (!(sut = repo(u, sut))) return 0;
  }
}

noun tpeek(typer *u, noun sut, char *way, noun axe) {
  return peekx(u, sut, way, axe, 0);
}

// gil holds the holds being expanded on the way down: hoon expands them
// without looking, so on a recursive type it never finishes, and here
// it fails instead
noun wrapx(typer *u, noun sut, char *yoz, guard *gil) {
  PROF(wrap);
  TP;
  if (tagis(sut, "cell")) {
    noun a = wrapx(u, hd(tl(sut)), yoz, gil);
    noun b = a ? wrapx(u, tl(tl(sut)), yoz, gil) : 0;
    return b ? tcell(u, a, b) : 0;
  }
  if (tagis(sut, "core")) {
    noun c = coreof(sut);
    noun g = coilgarb(c);
    if (!atomeqc(garbvair(g), "gold") && !streq(yoz, "lead")) return 0;
    noun g2 = C3(hd(g), garbpoly(g), term(yoz));
    return C3(K("core"), hd(tl(sut)), C2(g2, tl(c)));
  }
  if (tagis(sut, "face")) {
    noun d = wrapx(u, tl(tl(sut)), yoz, gil);
    return d ? tface(u, hd(tl(sut)), d) : 0;
  }
  if (tagis(sut, "fork")) {
    nouns xs = {0};
    settap(tl(sut), &xs, p->a);
    for (size i = 0; i < xs.len; i++) {
      if (!(xs.data[i] = wrapx(u, xs.data[i], yoz, gil))) return 0;
    }
    return tfork(u, xs.data, xs.len);
  }
  if (tagis(sut, "hint")) {
    noun d = wrapx(u, tl(tl(sut)), yoz, gil);
    return d ? thint(u, hd(tl(sut)), d) : 0;
  }
  if (tagis(sut, "hold")) {
    if (guardhas(gil, sut, 0, 0)) return 0;
    noun r = repo(u, sut);
    return r ? wrapx(u, r, yoz, guardput(p->a, gil, sut, 0, 0)) : 0;
  }
  return sut;
}

noun wrap(typer *u, noun sut, char *yoz) {
  return wrapx(u, sut, yoz, 0);
}

// Wing search

// look up an arm in a chapter, as +look: axis within the chapter
noun look(typer *u, noun cog, noun dab, noun *gen) {
  PROF(look);
  TP;
  noun axe = D(1);
  for (;;) {
    if (isatom(dab)) return 0;
    noun n = mapn(dab);
    b32 l = iscell(mapl(dab)), r = iscell(mapr(dab));
    if (!l && !r) {
      if (!nouneq(cog, hd(n))) return 0;
      *gen = tl(n);
      return axe;
    }
    if (nouneq(cog, hd(n))) {
      *gen = tl(n);
      return peg(p, axe, D(2));
    }
    if (!l) {
      if (gor(cog, hd(n))) return 0;
      axe = peg(p, axe, D(3));
      dab = mapr(dab);
    } else if (!r) {
      if (!gor(cog, hd(n))) return 0;
      axe = peg(p, axe, D(3));
      dab = mapl(dab);
    } else if (gor(cog, hd(n))) {
      axe = peg(p, axe, D(6));
      dab = mapl(dab);
    } else {
      axe = peg(p, axe, D(7));
      dab = mapr(dab);
    }
  }
}

noun lootx(typer *u, noun cog, noun dom, noun *gen);

// Arms found lately, by name and battery: +fond asks the same cores for
// the same names over and over, and a name not in a battery is looked
// for in every chapter. Keyed by handles, so emptied when collection
// moves the cells, and not used for loose nouns, whose space is reused.
typedef struct {
  noun cog;
  noun dom;
  noun axe;   // as loot returns, 0 for no arm
  noun gen;
} lootcell;

lootcell lootcache[1 << 16];

void lootclear(void) {
  for (size i = 0; i < countof(lootcache); i++) lootcache[i] = (lootcell){0};
}

// look up an arm in a battery, as +loot: its axis and, in gen, its hoon
noun loot(typer *u, noun cog, noun dom, noun *gen) {
  if (loose.on) return lootx(u, cog, dom, gen);
  u32 h = (u32)((((u64)cog << 32 | dom) * 0x9e3779b97f4a7c15ull) >> 48);
  lootcell *c = &lootcache[h];
  if (c->cog == cog && c->dom == dom && c->cog) {
    if (c->axe) *gen = c->gen;
    return c->axe;
  }
  noun g = 0;
  noun axe = lootx(u, cog, dom, &g);
  *c = (lootcell){cog, dom, axe, g};
  if (axe) *gen = g;
  return axe;
}

noun lootx(typer *u, noun cog, noun dom, noun *gen) {
  PROF(loot);
  TP;
  noun axe = D(1);
  for (;;) {
    if (isatom(dom)) return 0;
    noun n = mapn(dom);
    b32 l = iscell(mapl(dom)), r = iscell(mapr(dom));
    noun yep = look(u, cog, tl(n), gen);
    if (!l && !r) return yep ? peg(p, axe, yep) : 0;
    if (yep) return peg(p, peg(p, axe, D(2)), yep);
    if (l && r) {
      noun pey = lootx(u, cog, mapl(dom), gen);
      if (pey) return peg(p, peg(p, axe, D(6)), pey);
      axe = peg(p, axe, D(7));
      dom = mapr(dom);
    } else {
      axe = peg(p, axe, D(3));
      dom = l ? mapl(dom) : mapr(dom);
    }
  }
}

// Ponies, the results of +fond:
//   ~                   no match
//   [%& palo]           found a leg or arm, palo = [vein opal]
//   [%| %& count]       not found, count of names still to skip
//   [%| %| [type nock]] synthetic, found through an alias: with a
//                       formula for +mint, with ~ for +play
//
// The search makes and drops a pony at every step, so they're structs
// here, not nouns, and made nouns only where they're kept. The opal of
// a leg is [%& type], of an arm [%| axis set].
enum { pcrash, pnone, pfound, pskip, palias };

typedef struct {
  i32  how;
  noun vein;    // pfound
  noun type;    // pfound leg, palias
  noun axe;     // pfound arm
  noun set;     //   and its set of [type foot]
  noun cnt;     // pskip
  noun fol;     // palias
} pony;

b32 ponyeq(pony a, pony b) {
  return a.how == b.how && a.vein == b.vein && a.type == b.type && a.axe == b.axe
      && a.set == b.set && a.cnt == b.cnt && a.fol == b.fol;
}

pony ponyleg(noun vein, noun type) { return (pony){.how = pfound, .vein = vein, .type = type}; }
pony ponyskip(noun cnt) { return (pony){.how = pskip, .cnt = cnt}; }
pony ponyalias(noun type, noun fol) { return (pony){.how = palias, .type = type, .fol = fol}; }

// a found pony's opal, as a noun
noun ponyopal(parser *p, pony y) {
  return y.set ? C3(NO, y.axe, y.set) : C2(YES, y.type);
}

// a pony as a noun, 0 on crash
noun ponynoun(parser *p, pony y) {
  switch (y.how) {
  case pnone:  return nul;
  case pfound: return C3(YES, y.vein, ponyopal(p, y));
  case pskip:  return C3(NO, YES, y.cnt);
  case palias: return C3(NO, NO, C2(y.type, y.fol));
  }
  return 0;
}

noun fire(typer *u, noun hag);

// a found leg or arm, or an alias, as a type, as +fine
noun fine(typer *u, pony tor) {
  if (tor.how == palias || !tor.set) return tor.type;
  TP;
  nouns xs = {0};
  settap(tor.set, &xs, p->a);
  return fire(u, nounslist(p, &xs));
}

// two ponies for the same limb in two types of a fork, as one
pony twin(typer *u, pony hax, pony yor) {
  TP;
  if (ponyeq(hax, yor)) return hax;
  if (hax.how == pnone) return yor;
  if (yor.how == pnone) return hax;
  if (hax.how != pfound) {
    if (hax.how != palias || yor.how != palias) return (pony){0};
    if (!nouneq(hax.fol, yor.fol)) return (pony){0};
    return ponyalias(tfork2(u, hax.type, yor.type), hax.fol);
  }
  if (yor.how != pfound || !nouneq(hax.vein, yor.vein)) return (pony){0};
  if (!hax.set && !yor.set) return ponyleg(hax.vein, tfork2(u, hax.type, yor.type));
  if (!hax.set || !yor.set || !nouneq(hax.axe, yor.axe)) return (pony){0};
  return (pony){.how = pfound, .vein = hax.vein, .axe = hax.axe,
                .set = setuni(p->a, hax.set, yor.set)};
}

pony fund(typer *u, noun sut, char *way, noun gen);

// the search loop of +fond for one limb
pony fondlimb(typer *u, noun sut, char *way, noun cnt, noun nam, noun axe,
              noun lon, guard *gil) {
  PROF(fondlimb);
  TP;
  #define HERE (atomis(cnt, 0) ? ponyleg(C2(nul, C2(C2(nul, axe), lon)), sut) \
                               : ponyskip(atomsub(p->a, cnt, D(1))))
  #define LOSE ponyskip(cnt)
  #define NONE ((pony){.how = pnone})
  #define FAIL ((pony){0})
  for (;;) {
    if (isvoid(sut)) return NONE;
    if (isnountype(sut) || tagis(sut, "atom")) return isatom(nam) ? HERE : LOSE;
    if (tagis(sut, "cell")) {
      if (isatom(nam)) return HERE;
      noun ctx = u->ctx;
      pony taf = fondlimb(u, hd(tl(sut)), way, cnt, nam, peg(p, axe, D(2)), lon, gil);
      u->ctx = ctx;
      if (taf.how != pskip) return taf;
      cnt = taf.cnt;
      axe = peg(p, axe, D(3));
      sut = tl(tl(sut));
      continue;
    }
    if (tagis(sut, "core")) {
      if (isatom(nam)) return HERE;
      noun arm = 0;
      noun zem = loot(u, tl(nam), coilbat(coreof(sut)), &arm);
      if (zem) {
        if (!atomis(cnt, 0)) {
          zem = 0;
          cnt = atomsub(p->a, cnt, D(1));
        }
      }
      if (zem) {
        noun zut = C2(garbpoly(coilgarb(coreof(sut))), arm);
        noun set = mapnode(p->a, C2(sut, zut), nul, nul);
        return (pony){.how = pfound, .vein = C2(C2(nul, axe), lon), .axe = peg(p, D(2), zem),
                      .set = set};
      }
      b32 sam, con;
      peel(way, garbvair(coilgarb(coreof(sut))), &sam, &con);
      if (!sam) return LOSE;
      if (con) {
        sut = hd(tl(sut));
        axe = peg(p, axe, D(3));
      } else {
        sut = tpeek(u, hd(tl(sut)), way, D(2));
        if (!sut) return FAIL;
        axe = peg(p, axe, D(6));
      }
      continue;
    }
    if (tagis(sut, "hint")) {
      if (u->spots) {
        noun note = tl(hd(tl(sut)));
        if (tagis(note, "spot")) u->ctx = note;
        if (tagis(note, "made")) u->ctx = C3(K("made"), hd(hd(tl(sut))), hd(tl(note)));
      }
      if (!(sut = repo(u, sut))) return FAIL;
      continue;
    }
    if (tagis(sut, "face")) {
      noun inner = tl(tl(sut));
      if (isatom(nam)) {
        sut = inner;
        return HERE;
      }
      noun zot = hd(tl(sut));
      if (isatom(zot)) {
        if (nouneq(tl(nam), zot)) {
          if (u->spots && atomis(cnt, 0)) u->lastface = u->ctx;
          sut = inner;
          return HERE;
        }
        return LOSE;
      }
      // a tune: aliases and bridges
      noun tyr = mapget(hd(zot), tl(nam));
      if (tyr) {
        if (isatom(tyr)) {
          sut = inner;
          lon = C2(nul, lon);
          cnt = atomaddsmall(p->a, cnt, 1);
          continue;
        }
        if (atomis(cnt, 0)) {
          noun ctx = u->ctx;
          pony tor = fund(u, sut, way, tl(tyr));
          if (tor.how == pcrash) return FAIL;
          if (u->spots) {
            u->lastface = ctx;
            u->bridged = 0;
          }
          if (tor.how == pfound) {
            tor.vein = weld(p, tor.vein, C2(nul, C2(C2(nul, axe), lon)));
            return tor;
          }
          return ponyalias(tor.type, nul);
        }
        cnt = atomsub(p->a, cnt, D(1));
      }
      // next: search the bridges
      for (noun q = tl(zot); ; q = tl(q)) {
        if (isatom(q)) {
          sut = inner;
          lon = C2(nul, lon);
          break;
        }
        noun tiv = play(u, inner, hd(q));
        if (!tiv) return FAIL;
        noun ctx = u->ctx;
        u->ctx = 0;
        pony fid = fondlimb(u, tiv, way, cnt, nam, D(1), nul, 0);
        u->ctx = ctx;
        if (fid.how == pcrash || fid.how == pnone) return fid;
        if (fid.how == pskip) {
          cnt = fid.cnt;
          continue;
        }
        noun vat = fine(u, fid);
        if (!vat) return FAIL;
        if (u->spots && fid.how == pfound) u->bridged = ponynoun(p, fid);
        return ponyalias(vat, nul);
      }
      continue;
    }
    if (tagis(sut, "fork")) {
      nouns xs = {0};
      settap(tl(sut), &xs, p->a);
      if (!xs.len) return NONE;
      pony acc = FAIL;
      noun ctx = u->ctx;
      for (size i = xs.len - 1; i >= 0; i--) {
        u->ctx = ctx;
        pony r = fondlimb(u, xs.data[i], way, cnt, nam, axe, lon, gil);
        if (r.how == pcrash) return FAIL;
        if (i == xs.len - 1) {
          acc = r;
          continue;
        }
        acc = twin(u, r, acc);
        if (acc.how == pcrash) return FAIL;
      }
      return acc;
    }
    if (tagis(sut, "hold")) {
      if (guardhas(gil, sut, 0, 0)) return NONE;
      gil = guardput(p->a, gil, sut, 0, 0);
      if (!(sut = repo(u, sut))) return FAIL;
      continue;
    }
    return FAIL;
  }
  #undef HERE
  #undef LOSE
  #undef NONE
  #undef FAIL
}

// find a wing in a type, as +fond
pony fondp(typer *u, noun sut, char *way, noun hyp) {
  PROF(fond);
  TP;
  if (isatom(hyp)) return ponyleg(nul, sut);
  pony mor = fondp(u, sut, way, tl(hyp));
  noun i = hd(hyp);
  if (mor.how == pcrash || mor.how == pnone || mor.how == pskip) return mor;
  if (mor.how == palias) {
    noun t = play(u, mor.type, C2(K("wing"), C2(i, nul)));
    return t ? ponyalias(t, nul) : (pony){0};
  }
  noun lon = mor.vein;
  noun s = mor.set ? tforkheads(u, mor.set) : mor.type;
  if (iscell(i) && atomis(hd(i), 0)) {
    noun t = tpeek(u, s, way, tl(i));
    return t ? ponyleg(C2(C2(nul, tl(i)), lon), t) : (pony){0};
  }
  noun cnt = iscell(i) ? hd(tl(i)) : nul;
  noun nam = iscell(i) ? tl(tl(i)) : C2(nul, i);
  u->ctx = 0;
  return fondlimb(u, s, way, cnt, nam, D(1), lon, 0);
}

// the same as a noun; 0 on crash
noun fond(typer *u, noun sut, char *way, noun hyp) {
  return ponynoun(u->p, fondp(u, sut, way, hyp));
}

// as +find: found, or an alias; crash if not found
pony findp(typer *u, noun sut, char *way, noun hyp) {
  pony r = fondp(u, sut, way, hyp);
  if (r.how == pnone || r.how == pskip) {
    errnew(errfind);
    hcerr.hyp = hyp;
    return (pony){0};
  }
  return r;
}

// the same as a noun, [%& palo] or [%| type]; 0 if not found or on crash
noun find(typer *u, noun sut, char *way, noun hyp) {
  TP;
  pony r = findp(u, sut, way, hyp);
  if (r.how == pfound) return C3(YES, r.vein, ponyopal(p, r));
  if (r.how == palias) return C2(NO, r.type);
  return 0;
}

pony fund(typer *u, noun sut, char *way, noun gen) {
  TP;
  noun hup = reek(p, gen);
  if (!hup) {
    noun t = play(u, sut, gen);
    return t ? ponyalias(t, nul) : (pony){0};
  }
  return findp(u, sut, way, hup);
}

// the product type of a list of [core foot], as +fire
noun fire(typer *u, noun hag) {
  PROF(fire);
  TP;
  if (iscell(hag) && isatom(tl(hag))) {
    noun foot = tl(hd(hag));
    if (atomeqc(hd(foot), "wet") && nouneq(tl(foot), C2(nul, D(1)))) return hd(hd(hag));
  }
  nouns xs = {0};
  for (; iscell(hag); hag = tl(hag)) {
    noun t = hd(hd(hag));
    noun foot = tl(hd(hag));
    if (!tagis(t, "core")) return 0;
    noun coil = coreof(t);
    noun garb = coilgarb(coil);
    if (atomeqc(hd(foot), "dry")) {
      noun gold = C2(C3(hd(garb), garbpoly(garb), K("gold")), tl(coil));
      noun dox = C3(K("core"), coilctx(coil), gold);
      *push(&xs, p->a) = C3(K("hold"), dox, tl(foot));
    } else {
      noun pay = redo(u, hd(tl(t)), coilctx(coil));
      if (!pay) return 0;
      *push(&xs, p->a) = C3(K("hold"), C3(K("core"), pay, coil), tl(foot));
    }
  }
  return tfork(u, xs.data, xs.len);
}

// Changing legs of a type

noun tend(parser *p, noun vit) {
  if (isatom(vit)) return D(1);
  return peg(p, tend(p, tl(vit)), isatom(hd(vit)) ? D(1) : tl(hd(vit)));
}

typedef struct {
  noun mur;   // replacement type for +tack, or 0
  b32   gain;  // +cool: fuse or crop with ref
  noun ref;
  noun skin;  // +chip %wthx: gain or lose by skin
  b32   how;
} takefn;

noun argain(typer *u, noun sut, noun ref, noun skin);
noun arlose(typer *u, noun sut, noun ref, noun skin);

noun takeapply(typer *u, noun sut, noun t, takefn *f) {
  if (f->mur) return f->mur;
  if (f->skin) return f->how ? argain(u, sut, t, f->skin) : arlose(u, sut, t, f->skin);
  return f->gain ? fuse(u, t, f->ref) : crop(u, t, f->ref);
}

// replace the leg at a vein, as +take; vit is flopped
noun takex(typer *u, noun ctx, noun sut, noun vit, noun axe, guard *vil, takefn *f) {
  PROF(take);
  TP;
  if (isatom(vit)) return takeapply(u, ctx, sut, f);
  if (isatom(hd(vit))) {
    // a face in the vein: keep faces around the rest
    for (;;) {
      if (tagis(sut, "face")) {
        noun d = takex(u, ctx, tl(tl(sut)), tl(vit), 0, 0, f);
        return d ? tface(u, hd(tl(sut)), d) : 0;
      }
      if (tagis(sut, "hint")) {
        noun d = takex(u, ctx, tl(tl(sut)), vit, 0, 0, f);
        return d ? thint(u, hd(tl(sut)), d) : 0;
      }
      if (tagis(sut, "fork")) {
        nouns xs = {0};
        settap(tl(sut), &xs, p->a);
        for (size i = 0; i < xs.len; i++) {
          if (!(xs.data[i] = takex(u, ctx, xs.data[i], vit, 0, 0, f))) return 0;
        }
        return tfork(u, xs.data, xs.len);
      }
      if (tagis(sut, "hold")) {
        if (!(sut = repo(u, sut))) return 0;
        continue;
      }
      return takex(u, ctx, sut, tl(vit), 0, 0, f);
    }
  }
  if (!axe) axe = tl(hd(vit));
  for (;;) {
    if (atomis(axe, 1)) return takex(u, ctx, sut, tl(vit), 0, 0, f);
    if (!alen(axe)) return 0;   // axis 0, where +cap and +mas crash
    u64 now = cap(axe);
    noun lat = mas(p, axe);
    if (isnountype(sut)) {
      sut = C3(K("cell"), K("noun"), K("noun"));
      continue;
    }
    if (isvoid(sut) || tagis(sut, "atom")) return K("void");
    if (tagis(sut, "cell")) {
      noun h = hd(tl(sut)), t = tl(tl(sut));
      if (now == 2) {
        noun d = takex(u, ctx, h, vit, lat, vil, f);
        return d ? tcell(u, d, t) : 0;
      }
      noun d = takex(u, ctx, t, vit, lat, vil, f);
      return d ? tcell(u, h, d) : 0;
    }
    if (tagis(sut, "core")) {
      if (now == 2) {
        if (!(sut = repo(u, sut))) return 0;
        continue;
      }
      noun d = takex(u, ctx, hd(tl(sut)), vit, lat, vil, f);
      return d ? tcore(u, d, tl(tl(sut))) : 0;
    }
    if (tagis(sut, "face")) {
      noun d = takex(u, ctx, tl(tl(sut)), vit, axe, vil, f);
      return d ? tface(u, hd(tl(sut)), d) : 0;
    }
    if (tagis(sut, "fork")) {
      nouns xs = {0};
      settap(tl(sut), &xs, p->a);
      for (size i = 0; i < xs.len; i++) {
        if (!(xs.data[i] = takex(u, ctx, xs.data[i], vit, axe, vil, f))) return 0;
      }
      return tfork(u, xs.data, xs.len);
    }
    if (tagis(sut, "hint")) {
      noun d = takex(u, ctx, tl(tl(sut)), vit, axe, vil, f);
      return d ? thint(u, hd(tl(sut)), d) : 0;
    }
    if (tagis(sut, "hold")) {
      if (guardhas(vil, sut, 0, 0)) return K("void");
      vil = guardput(p->a, vil, sut, 0, 0);
      if (!(sut = repo(u, sut))) return 0;
      continue;
    }
    return 0;
  }
}

noun take(typer *u, noun sut, noun vit, takefn *f) {
  TP;
  return takex(u, sut, sut, flop(p, vit), 0, 0, f);
}

// replace the leg at a wing with type mur, as +tack
noun tack(typer *u, noun sut, noun hyp, noun mur, noun *axis) {
  PROF(tack);
  TP;
  pony fid = findp(u, sut, "rite", hyp);
  if (fid.how != pfound) return 0;
  if (axis) *axis = tend(p, fid.vein);
  takefn f = {mur, 0, 0, 0, 0};
  return take(u, sut, fid.vein, &f);
}

// type of a wing with changes, as +elbo
noun elbo(typer *u, noun sut, pony lop, noun rig) {
  PROF(elbo);
  TP;
  if (!lop.set) {
    noun t = lop.type;
    for (; iscell(rig); rig = tl(rig)) {
      noun zil = play(u, sut, tl(hd(rig)));
      if (!zil) return 0;
      if (!(t = tack(u, t, hd(hd(rig)), zil, 0))) return 0;
    }
    return t;
  }
  nouns hag = {0};
  settap(lop.set, &hag, p->a);
  for (; iscell(rig); rig = tl(rig)) {
    noun zil = play(u, sut, tl(hd(rig)));
    if (!zil) return 0;
    noun axe = 0;
    for (size i = 0; i < hag.len; i++) {
      noun ax;
      noun t = tack(u, hd(hag.data[i]), hd(hd(rig)), zil, &ax);
      if (!t) return 0;
      if (axe && !nouneq(axe, ax)) return 0;
      axe = ax;
      hag.data[i] = C2(t, tl(hag.data[i]));
    }
  }
  return fire(u, nounslist(p, &hag));
}

// Narrowing: +fuse, +crop, +cool, +chip and the skin engine +ar

noun fusex(typer *u, noun sut, noun ref, guard *bix) {
  PROF(fuse);
  TP;
  for (;;) {
    if (nouneq(sut, ref) || isnountype(ref)) return sut;
    if (tagis(sut, "atom")) {
      if (tagis(ref, "atom")) {
        noun foc = fitz(hd(tl(ref)), hd(tl(sut))) ? hd(tl(sut)) : hd(tl(ref));
        noun qs = tl(tl(sut)), qr = tl(tl(ref));
        if (iscell(qs)) {
          if (iscell(qr)) return nouneq(qs, qr) ? C3(K("atom"), foc, qs) : K("void");
          return C3(K("atom"), foc, qs);
        }
        return C3(K("atom"), foc, qr);
      }
      if (tagis(ref, "cell")) return K("void");
      noun t = sut;
      sut = ref;
      ref = t;
      continue;
    }
    if (tagis(sut, "cell")) {
      if (tagis(ref, "cell")) {
        noun h = fusex(u, hd(tl(sut)), hd(tl(ref)), bix);
        noun t = h ? fusex(u, tl(tl(sut)), tl(tl(ref)), bix) : 0;
        return t ? tcell(u, h, t) : 0;
      }
      noun t = sut;
      sut = ref;
      ref = t;
      continue;
    }
    if (tagis(sut, "core")) {
      if (!(sut = repo(u, sut))) return 0;
      continue;
    }
    if (tagis(sut, "face")) {
      noun d = fusex(u, tl(tl(sut)), ref, bix);
      return d ? tface(u, hd(tl(sut)), d) : 0;
    }
    if (tagis(sut, "fork")) {
      nouns xs = {0};
      settap(tl(sut), &xs, p->a);
      for (size i = 0; i < xs.len; i++) {
        if (!(xs.data[i] = fusex(u, xs.data[i], ref, bix))) return 0;
      }
      return tfork(u, xs.data, xs.len);
    }
    if (tagis(sut, "hint")) {
      noun d = fusex(u, tl(tl(sut)), ref, bix);
      return d ? thint(u, hd(tl(sut)), d) : 0;
    }
    if (tagis(sut, "hold")) {
      if (guardhas(bix, sut, ref, 0)) return 0;
      bix = guardput(p->a, bix, sut, ref, 0);
      if (!(sut = repo(u, sut))) return 0;
      continue;
    }
    if (isnountype(sut)) return ref;
    return K("void");
  }
}

noun fuse(typer *u, noun sut, noun ref) {
  return fusex(u, sut, ref, 0);
}

noun cropdext(typer *u, noun sut, noun ref, guard *bix);

noun cropsint(typer *u, noun sut, noun ref, guard *bix) {
  TP;
  if (tagis(ref, "core")) return sut;
  if (tagis(ref, "face") || tagis(ref, "hint") || tagis(ref, "hold")) {
    noun r = repo(u, ref);
    return r ? cropdext(u, sut, r, bix) : 0;
  }
  if (tagis(ref, "fork")) {
    nouns xs = {0};
    settap(tl(ref), &xs, p->a);
    for (size i = 0; i < xs.len; i++) {
      if (!(sut = cropdext(u, sut, xs.data[i], bix))) return 0;
    }
    return sut;
  }
  return 0;
}

noun cropdext(typer *u, noun sut, noun ref, guard *bix) {
  PROF(crop);
  TP;
  for (;;) {
    if (nouneq(sut, ref) || isnountype(ref)) return K("void");
    if (isvoid(ref)) return sut;
    if (tagis(sut, "atom")) {
      if (tagis(ref, "atom")) {
        noun qs = tl(tl(sut)), qr = tl(tl(ref));
        if (iscell(qs)) return iscell(qr) ? (nouneq(qr, qs) ? K("void") : sut) : K("void");
        return iscell(qr) ? sut : K("void");
      }
      if (tagis(ref, "cell")) return sut;
      return cropsint(u, sut, ref, bix);
    }
    if (tagis(sut, "cell")) {
      if (tagis(ref, "atom")) return sut;
      if (tagis(ref, "cell")) {
        b32 ok;
        if (!nest(u, hd(tl(ref)), 0, hd(tl(sut)), &ok)) return 0;
        if (!ok) return sut;
        noun t = cropdext(u, tl(tl(sut)), tl(tl(ref)), bix);
        return t ? tcell(u, hd(tl(sut)), t) : 0;
      }
      return cropsint(u, sut, ref, bix);
    }
    if (tagis(sut, "core")) {
      if (tagis(ref, "atom") || tagis(ref, "cell")) return sut;
      return cropsint(u, sut, ref, bix);
    }
    if (tagis(sut, "face")) {
      noun d = cropdext(u, tl(tl(sut)), ref, bix);
      return d ? tface(u, hd(tl(sut)), d) : 0;
    }
    if (tagis(sut, "fork")) {
      nouns xs = {0};
      settap(tl(sut), &xs, p->a);
      for (size i = 0; i < xs.len; i++) {
        if (!(xs.data[i] = cropdext(u, xs.data[i], ref, bix))) return 0;
      }
      return tfork(u, xs.data, xs.len);
    }
    if (tagis(sut, "hint")) {
      noun d = cropdext(u, tl(tl(sut)), ref, bix);
      return d ? thint(u, hd(tl(sut)), d) : 0;
    }
    if (tagis(sut, "hold")) {
      if (guardhas(bix, sut, ref, 0)) return 0;
      bix = guardput(p->a, bix, sut, ref, 0);
      if (!(sut = repo(u, sut))) return 0;
      continue;
    }
    if (isnountype(sut)) {
      if (!(sut = repo(u, sut))) return 0;
      continue;
    }
    return K("void");
  }
}

noun crop(typer *u, noun sut, noun ref) {
  return cropdext(u, sut, ref, 0);
}

noun cool(typer *u, noun sut, b32 pol, noun hyp, noun ref) {
  PROF(cool);
  pony fid = findp(u, sut, "both", hyp);
  if (fid.how == pcrash) return 0;
  if (fid.how == palias) return sut;
  takefn f = {0, pol, ref, 0, 0};
  return take(u, sut, fid.vein, &f);
}

noun chip(typer *u, noun sut, b32 how, noun gen) {
  PROF(chip);
  TP;
  for (;;) {
    noun g = tl(gen);
    if (tagis(gen, "wtts")) {
      noun e = hoonexample(p, hd(g));
      noun t = e ? play(u, sut, e) : 0;
      return t ? cool(u, sut, how, tl(g), t) : 0;
    }
    if (tagis(gen, "wthx")) {
      pony fid = findp(u, sut, "both", tl(g));
      if (fid.how == pcrash) return 0;
      if (fid.how == palias) return sut;
      takefn f = {0, 0, 0, hd(g), how};
      return take(u, sut, fid.vein, &f);
    }
    if ((how && tagis(gen, "wtpm")) || (!how && tagis(gen, "wtbr"))) {
      for (noun l = g; iscell(l); l = tl(l)) {
        if (!(sut = chip(u, sut, how, hd(l)))) return 0;
      }
      return sut;
    }
    if (tagis(gen, "wtzp")) {
      how = !how;
      gen = g;
      continue;
    }
    noun neg = hoonopen(p, gen);
    if (!neg) return 0;
    if (nouneq(neg, gen)) return sut;
    gen = neg;
  }
}

// a skin as a spec over the noun, for bare terms
noun skinspec(typer *u, noun skin) {
  TP;
  if (iscell(skin)) return skin;
  noun like = C3(K("like"), C2(skin, nul), nul);
  return C3(K("spec"), like, C2(K("base"), K("noun")));
}

noun ardispatch(typer *u, noun sut, noun ref, noun skin, b32 gain);

// restrict ref to atoms, cells or constants, looping through holds
noun arwalk(typer *u, noun sut, noun ref, noun skin, b32 gain, i32 kind, guard *gil) {
  TP;
  // kind: 0 atom base, 1 cell, 2 leaf
  noun s = tl(skin);
  for (;;) {
    if (isvoid(ref)) return K("void");
    if (tagis(ref, "face")) {
      if (gain || kind == 1) {
        ref = tl(tl(ref));
        continue;
      }
      noun d = arwalk(u, sut, tl(tl(ref)), skin, gain, kind, gil);
      return d ? tface(u, hd(tl(ref)), d) : 0;
    }
    if (tagis(ref, "fork")) {
      nouns xs = {0};
      settap(tl(ref), &xs, p->a);
      for (size i = 0; i < xs.len; i++) {
        if (!(xs.data[i] = arwalk(u, sut, xs.data[i], skin, gain, kind, gil))) return 0;
      }
      return tfork(u, xs.data, xs.len);
    }
    if (tagis(ref, "hint")) {
      noun d = arwalk(u, sut, tl(tl(ref)), skin, gain, kind, gil);
      return d ? thint(u, hd(tl(ref)), d) : 0;
    }
    if (tagis(ref, "hold")) {
      if (guardhas(gil, ref, 0, 0)) return K("void");
      gil = guardput(p->a, gil, ref, 0, 0);
      if (!(ref = repo(u, ref))) return 0;
      continue;
    }
    break;
  }
  if (kind == 0) {
    noun aura = tl(hd(s));
    if (gain) {
      if (isnountype(ref)) return C3(K("atom"), aura, nul);
      if (tagis(ref, "atom")) {
        if (!fitz(aura, hd(tl(ref)))) return 0;
        noun m = atomcmp(aura, hd(tl(ref))) >= 0 ? aura : hd(tl(ref));
        return C3(K("atom"), m, tl(tl(ref)));
      }
      return K("void");
    }
    if (isnountype(ref)) return C3(K("cell"), K("noun"), K("noun"));
    if (tagis(ref, "atom")) return K("void");
    return ref;
  }
  if (kind == 1) {
    noun hs = hd(s), ts = tl(s);
    if (isnountype(ref)) {
      if (gain) {
        noun h = ardispatch(u, sut, K("noun"), hs, 1);
        if (!h) return 0;
        if (isvoid(h)) return K("void");
        noun t = ardispatch(u, sut, K("noun"), ts, 1);
        return t ? tcell(u, h, t) : 0;
      }
      noun nn = C3(K("cell"), C2(K("base"), K("noun")), C2(K("base"), K("noun")));
      return nouneq(skin, nn) ? C3(K("atom"), nul, nul) : ref;
    }
    if (tagis(ref, "atom")) return gain ? K("void") : ref;
    if (tagis(ref, "cell")) {
      noun ph = hd(tl(ref)), qh = tl(tl(ref));
      if (gain) {
        noun h = ardispatch(u, sut, ph, hs, 1);
        if (!h) return 0;
        if (isvoid(h)) return K("void");
        noun t = ardispatch(u, sut, qh, ts, 1);
        return t ? tcell(u, h, t) : 0;
      }
      noun lef = ardispatch(u, sut, ph, hs, 0);
      noun rig = lef ? ardispatch(u, sut, qh, ts, 0) : 0;
      if (!rig) return 0;
      noun ts3[3] = {tcell(u, lef, rig), tcell(u, lef, qh), tcell(u, ph, rig)};
      return tfork(u, ts3, 3);
    }
    if (tagis(ref, "core")) {
      noun h = ardispatch(u, sut, hd(tl(ref)), hs, gain);
      if (!h) return 0;
      if (isvoid(h)) return K("void");
      if (!atomeqc(ts, "noun")) {
        noun t = ardispatch(u, sut, K("noun"), ts, gain);
        return t ? tcell(u, h, t) : 0;
      }
      return C3(K("core"), h, tl(tl(ref)));
    }
    return 0;
  }
  // leaf
  noun aura = hd(s), atom = tl(s);
  if (gain) {
    if (isnountype(ref)) return C3(K("atom"), aura, C2(nul, atom));
    if (tagis(ref, "atom")) {
      noun q = tl(tl(ref));
      if (iscell(q) && !nouneq(atom, tl(q))) return K("void");
      if (!fitz(aura, hd(tl(ref)))) return 0;
      noun m = atomcmp(aura, hd(tl(ref))) >= 0 ? aura : hd(tl(ref));
      return C3(K("atom"), m, C2(nul, atom));
    }
    return K("void");
  }
  if (isnountype(ref)) return K("noun");
  if (tagis(ref, "atom")) return nouneq(tl(tl(ref)), C2(nul, atom)) ? K("void") : ref;
  return ref;
}

noun ardispatch(typer *u, noun sut, noun ref, noun skin, b32 gain) {
  TP;
  for (;;) {
    skin = skinspec(u, skin);
    noun s = tl(skin);
    if (tagis(skin, "base")) {
      if (isatom(s)) {
        if (atomeqc(s, "cell")) {
          skin = C3(K("cell"), C2(K("base"), K("noun")), C2(K("base"), K("noun")));
          continue;
        }
        if (atomeqc(s, "flag")) {
          noun y = C3(K("leaf"), K("f"), YES), n = C3(K("leaf"), K("f"), NO);
          if (gain) {
            noun a = ardispatch(u, sut, ref, y, 1);
            noun b = a ? ardispatch(u, sut, ref, n, 1) : 0;
            return b ? tfork2(u, a, b) : 0;
          }
          noun a = ardispatch(u, sut, ref, y, 0);
          return a ? ardispatch(u, sut, a, n, 0) : 0;
        }
        if (atomeqc(s, "null")) {
          skin = C3(K("leaf"), K("n"), nul);
          continue;
        }
        if (atomeqc(s, "void")) return gain ? K("void") : ref;
        // %noun
        if (!gain) return K("void");
        b32 ok;
        if (!nest(u, K("void"), 0, ref, &ok)) return 0;
        return ok ? K("void") : ref;
      }
      return arwalk(u, sut, ref, skin, gain, 0, 0);
    }
    if (tagis(skin, "cell")) return arwalk(u, sut, ref, skin, gain, 1, 0);
    if (tagis(skin, "leaf")) return arwalk(u, sut, ref, skin, gain, 2, 0);
    if (tagis(skin, "dbug")) {
      skin = tl(s);
      continue;
    }
    if (tagis(skin, "name")) {
      if (!gain) {
        skin = tl(s);
        continue;
      }
      noun d = ardispatch(u, sut, ref, tl(s), 1);
      return d ? tface(u, hd(s), d) : 0;
    }
    if (tagis(skin, "over")) {
      if (!(sut = play(u, sut, C2(K("wing"), hd(s))))) return 0;
      skin = tl(s);
      continue;
    }
    if (tagis(skin, "spec")) {
      noun e = hoonexample(p, hd(s));
      noun hit = e ? play(u, sut, e) : 0;
      if (!hit) return 0;
      noun inner = ardispatch(u, sut, ref, tl(s), gain);
      if (!inner) return 0;
      return gain ? fuse(u, ref, hit) : crop(u, ref, hit);
    }
    if (tagis(skin, "wash")) {
      if (!gain) return ref;
      noun w = nul;
      for (u64 i = atomlow(s); i > 0; i--) w = C2(C3(NO, nul, nul), w);
      return play(u, ref, C2(K("wing"), w));
    }
    return 0;
  }
}

noun argain(typer *u, noun sut, noun ref, noun skin) {
  return ardispatch(u, sut, ref, skin, 1);
}

noun arlose(typer *u, noun sut, noun ref, noun skin) {
  return ardispatch(u, sut, ref, skin, 0);
}

// Nesting, as +nest: whether ref fits in sut. Returns 0 on crash.

typedef struct {
  guard *seg;
  guard *reg;
  guard *gil;
} nestctx;

b32 nestdext(typer *u, noun sut, noun ref, nestctx c, b32 *ok);

// Nesting runs on a stack of its own, not C's: a list of n items has a
// type n cells deep, and a few C frames for each would run out of stack
// on a long one. A step of the loop in nestdext either answers its goal,
// or gives the goal to answer instead, having pushed what to do with
// that answer: these frames.
enum { nestmemokind, nestandkind, nestanykind, nestallkind };

typedef struct {
  i32     kind;
  noun    sut;     // nestmemokind: the pair to remember the answer for;
  noun    ref;     // nestandkind: the goal to try next if the answer is
  nestctx c;       // yes; nestanykind and nestallkind: what the fork's
  noun   *xs;      // types are tried against, for sut of them, or ref
  size    i;       // the next type of the fork to try
  size    n;
} nestframe;

struct {
  nestframe *data;
  size       len;
  size       cap;
} nestk;

void nestpush(nestframe f) {
  if (nestk.len == nestk.cap) {
    size cap = nestk.cap ? nestk.cap * 2 : 1 << 10;
    nestframe *d = new(&H.perm, nestframe, cap);
    copy((byte*)d, (byte*)nestk.data, nestk.len * sizeof(nestframe));
    if (nestk.cap) osrelease((byte*)nestk.data, (byte*)(nestk.data + nestk.cap));
    nestk.data = d;
    nestk.cap = cap;
  }
  nestk.data[nestk.len++] = f;
}

// what a step does: answers in *ok, crashes, or leaves the next goal
enum { nestdone, nestcrash, nestgoal };

#define NESTDONE(v) do { *ok = (v); return nestdone; } while (0)
#define NESTGOAL(s, r, cc) do { *sutp = (s); *refp = (r); *cp = (cc); return nestgoal; } while (0)

// a step of +sint:nest, the ref side
i32 nestsint(typer *u, noun *sutp, noun *refp, nestctx *cp, b32 *ok) {
  TP;
  noun sut = *sutp, ref = *refp;
  nestctx c = *cp;
  if (isnountype(ref) || tagis(ref, "atom") || tagis(ref, "cell")) NESTDONE(0);
  if (isvoid(ref)) NESTDONE(1);
  if (tagis(ref, "core")) {
    noun r = repo(u, ref);
    if (!r) return nestcrash;
    NESTGOAL(sut, r, c);
  }
  if (tagis(ref, "face") || tagis(ref, "hint")) NESTGOAL(sut, tl(tl(ref)), c);
  if (tagis(ref, "fork")) {
    nouns xs = {0};
    settap(tl(ref), &xs, p->a);
    if (!xs.len) NESTDONE(1);
    nestpush((nestframe){nestallkind, sut, 0, c, xs.data, 1, xs.len});
    NESTGOAL(sut, xs.data[0], c);
  }
  // %hold
  if (guardhas(c.reg, ref, 0, 0) || guardhas(c.gil, sut, ref, 0)) NESTDONE(1);
  noun r = repo(u, ref);
  if (!r) return nestcrash;
  c.reg = guardput(p->a, c.reg, ref, 0, 0);
  c.gil = guardput(p->a, c.gil, sut, ref, 0);
  NESTGOAL(sut, r, c);
}

b32 nestdeem(typer *u, noun sut, noun ref, noun mel, noun ram, nestctx c, b32 *ok) {
  TP;
  if (!(nouneq(mel, ram) || atomeqc(mel, "lead") || atomeqc(ram, "gold"))) {
    *ok = 0;
    return 1;
  }
  if (atomeqc(mel, "lead")) {
    *ok = 1;
    return 1;
  }
  if (atomeqc(mel, "gold")) {
    if (!nestdext(u, sut, ref, c, ok)) return 0;
    if (!*ok) return 1;
    return nestdext(u, ref, sut, c, ok);
  }
  if (atomeqc(mel, "iron")) {
    noun a = tpeek(u, ref, "rite", D(2));
    noun b = tpeek(u, sut, "rite", D(2));
    return a && b && nestdext(u, a, b, c, ok);
  }
  noun a = tpeek(u, sut, "read", D(2));
  noun b = tpeek(u, ref, "read", D(2));
  return a && b && nestdext(u, a, b, c, ok);
}

// compare the arms of two batteries, as the chapter loop of +deep
b32 nestarms(typer *u, noun sut, noun ref, noun dab, noun hem, nestctx c, b32 *ok) {
  if (isatom(dab) || isatom(hem)) {
    *ok = isatom(dab) && isatom(hem);
    return 1;
  }
  if (!nouneq(hd(mapn(dab)), hd(mapn(hem)))) {
    *ok = 0;
    return 1;
  }
  if (!nestarms(u, sut, ref, mapl(dab), mapl(hem), c, ok)) return 0;
  if (!*ok) return 1;
  if (!nestarms(u, sut, ref, mapr(dab), mapr(hem), c, ok)) return 0;
  if (!*ok) return 1;
  noun a = play(u, sut, tl(mapn(dab)));
  noun b = a ? play(u, ref, tl(mapn(hem))) : 0;
  return b && nestdext(u, a, b, c, ok);
}

// matching batteries whose arm types nest, as +deep
b32 nestdeep(typer *u, noun sut, noun ref, noun dom, noun vim, nestctx c, b32 *ok) {
  if (isatom(dom) || isatom(vim)) {
    *ok = isatom(dom) && isatom(vim);
    return 1;
  }
  if (!nouneq(hd(mapn(dom)), hd(mapn(vim)))) {
    *ok = 0;
    return 1;
  }
  if (!nestdeep(u, sut, ref, mapl(dom), mapl(vim), c, ok)) return 0;
  if (!*ok) return 1;
  if (!nestdeep(u, sut, ref, mapr(dom), mapr(vim), c, ok)) return 0;
  if (!*ok) return 1;
  return nestarms(u, sut, ref, tl(mapn(dom)), tl(mapn(vim)), c, ok);
}

// Answers are kept by the pair of types, as vere's jet for +nest keeps
// them: yes when it took no assumption about holds on the ref side, no
// when none on the sut side. Comparing a constant against a recursive
// type otherwise redoes the same comparisons down every branch.
typedef struct {
  noun sut;
  noun ref;
  b32  ok;
} nestcell;

nestcell *nestmemo;
size      nestcap;
size      nestlen;

size nestslot(noun sut, noun ref) {
  u64 h = ((u64)handlehash(sut) * 0x9e3779b97f4a7c15ull) ^ ((u64)handlehash(ref) * 0xc2b2ae3d27d4eb4full);
  size mask = nestcap - 1;
  size j = (size)(h >> 17) & mask;
  while (nestmemo[j].sut && !(nouneq(nestmemo[j].sut, sut) && nouneq(nestmemo[j].ref, ref))) {
    j = (j + 1) & mask;
  }
  return j;
}

// make room in the memo: before every answer kept, not only every
// goal, as a deep nest keeps thousands on its way back up with no goal
// between them, and a table that fills is searched forever
void nestgrow(void) {
  if (nestlen*4 < nestcap*3) return;
  nestcell *old = nestmemo;
  size oldcap = nestcap;
  nestcap = oldcap ? oldcap * 2 : 1 << 12;
  nestmemo = new(&H.perm, nestcell, nestcap);
  for (size i = 0; i < oldcap; i++) {
    if (old[i].sut) nestmemo[nestslot(old[i].sut, old[i].ref)] = old[i];
  }
  if (oldcap) osrelease((byte*)old, (byte*)(old + oldcap));
}

i32 nestdextx(typer *u, noun *sutp, noun *refp, nestctx *cp, b32 *ok);

b32 nestdext(typer *u, noun sut, noun ref, nestctx c, b32 *ok) {
  size base = nestk.len;
  for (;;) {
#ifdef HC_EMBED
    // +mint and +mull ask, but this can run long between them
    if (interrupted && stopping()) {
      nestk.len = base;
      return 0;
    }
#endif
    nestgrow();
    b32 r;
    size j = nestslot(sut, ref);
    if (nestmemo[j].sut) {
      r = nestmemo[j].ok;
    } else {
      nestpush((nestframe){nestmemokind, sut, ref, c, 0, 0, 0});
      i32 k = nestdextx(u, &sut, &ref, &c, &r);
      if (k == nestcrash) {
        nestk.len = base;
        return 0;
      }
      if (k == nestgoal) continue;
    }
    // an answer: back through the frames waiting on it, to the next goal
    for (;;) {
      if (nestk.len == base) {
        *ok = r;
        return 1;
      }
      nestframe *f = &nestk.data[nestk.len - 1];
      if (f->kind == nestmemokind) {
        if ((r && !f->c.reg) || (!r && !f->c.seg)) {
          nestgrow();
          j = nestslot(f->sut, f->ref);
          if (!nestmemo[j].sut) nestlen++;
          nestmemo[j] = (nestcell){f->sut, f->ref, r};
        }
        nestk.len--;
        continue;
      }
      if (f->kind == nestandkind) {
        nestk.len--;
        if (!r) continue;
        sut = f->sut;
        ref = f->ref;
        c = f->c;
        break;
      }
      if (f->kind == nestanykind) {
        if (r || f->i == f->n) {
          nestk.len--;
          continue;
        }
        sut = f->xs[f->i++];
        ref = f->ref;
        c = f->c;
        break;
      }
      // nestallkind
      if (!r || f->i == f->n) {
        nestk.len--;
        continue;
      }
      sut = f->sut;
      ref = f->xs[f->i++];
      c = f->c;
      break;
    }
  }
}

// a step of +dext:nest
i32 nestdextx(typer *u, noun *sutp, noun *refp, nestctx *cp, b32 *ok) {
  PROF(nest);
  TP;
  noun sut = *sutp, ref = *refp;
  nestctx c = *cp;
  if (nouneq(sut, ref)) NESTDONE(1);
  if (isvoid(sut)) return nestsint(u, sutp, refp, cp, ok);
  if (isnountype(sut)) NESTDONE(1);
  if (tagis(sut, "atom")) {
    if (!tagis(ref, "atom")) return nestsint(u, sutp, refp, cp, ok);
    NESTDONE(fitz(hd(tl(sut)), hd(tl(ref)))
             && (isatom(tl(tl(sut))) || nouneq(tl(tl(sut)), tl(tl(ref)))));
  }
  if (tagis(sut, "cell")) {
    if (!tagis(ref, "cell")) return nestsint(u, sutp, refp, cp, ok);
    nestctx d = {0, 0, c.gil};
    nestpush((nestframe){nestandkind, tl(tl(sut)), tl(tl(ref)), d, 0, 0, 0});
    NESTGOAL(hd(tl(sut)), hd(tl(ref)), d);
  }
  if (tagis(sut, "core")) {
    if (!tagis(ref, "core")) return nestsint(u, sutp, refp, cp, ok);
    noun qs = coreof(sut), qr = coreof(ref);
    if (nouneq(qs, qr)) NESTGOAL(hd(tl(sut)), hd(tl(ref)), c);
    if (!nouneq(garbpoly(coilgarb(qs)), garbpoly(coilgarb(qr)))) NESTDONE(0);
    // meet(sut q.q.sut, ref p.sut), and the rest, nested as cores are
    if (!nestdext(u, coilctx(qs), hd(tl(sut)), c, ok)) return nestcrash;
    if (!*ok) return nestdone;
    if (!nestdext(u, hd(tl(sut)), coilctx(qs), c, ok)) return nestcrash;
    if (!*ok) return nestdone;
    if (!nestdext(u, coilctx(qr), hd(tl(ref)), c, ok)) return nestcrash;
    if (!*ok) return nestdone;
    if (!nestdeem(u, coilctx(qs), coilctx(qr), garbvair(coilgarb(qs)), garbvair(coilgarb(qr)), c, ok)) {
      return nestcrash;
    }
    if (!*ok) return nestdone;
    if (atomeqc(garbpoly(coilgarb(qs)), "wet")) NESTDONE(nouneq(coilbat(qs), coilbat(qr)));
    if (guardhas(c.gil, sut, ref, 0)) NESTDONE(1);
    nestctx d = c;
    d.gil = guardput(p->a, c.gil, sut, ref, 0);
    noun gs = coilgarb(qs), gr = coilgarb(qr);
    noun s2 = C3(K("core"), coilctx(qs), C2(C3(hd(gs), garbpoly(gs), K("gold")), tl(qs)));
    noun r2 = C3(K("core"), coilctx(qr), C2(C3(hd(gr), garbpoly(gr), K("gold")), tl(qr)));
    return nestdeep(u, s2, r2, coilbat(qs), coilbat(qr), d, ok) ? nestdone : nestcrash;
  }
  if (tagis(sut, "face") || tagis(sut, "hint")) NESTGOAL(tl(tl(sut)), ref, c);
  if (tagis(sut, "fork")) {
    if (!(tagis(ref, "atom") || isnountype(ref) || tagis(ref, "cell") || tagis(ref, "core"))) {
      return nestsint(u, sutp, refp, cp, ok);
    }
    nouns xs = {0};
    settap(tl(sut), &xs, p->a);
    if (!xs.len) NESTDONE(0);
    nestpush((nestframe){nestanykind, 0, ref, c, xs.data, 1, xs.len});
    NESTGOAL(xs.data[0], ref, c);
  }
  // %hold
  if (guardhas(c.seg, sut, 0, 0)) NESTDONE(0);
  if (guardhas(c.gil, sut, ref, 0)) NESTDONE(1);
  noun r = repo(u, sut);
  if (!r) return nestcrash;
  c.seg = guardput(p->a, c.seg, sut, 0, 0);
  c.gil = guardput(p->a, c.gil, sut, ref, 0);
  NESTGOAL(r, ref, c);
}

#undef NESTDONE
#undef NESTGOAL

// whether ref nests in sut; returns 0 on crash, the answer in ok
b32 nest(typer *u, noun sut, b32 tel, noun ref, b32 *ok) {
  nestctx c = {0, 0, 0};
  if (!nestdext(u, sut, ref, c, ok)) return 0;
  if (!*ok && tel) {
    errnew(errnest);
    hcerr.need = sut;
    hcerr.have = ref;
  }
  return *ok || !tel;
}

// Refurbishing faces for wet arms, as +redo

typedef struct {
  noun hos;   // subject tool stack, as a list
  noun wec;   // set of reference tool stacks
  guard *gil;
  noun ref;
} redoctx;

b32 miss(typer *u, noun sut, noun ref, b32 *out);

// reduce the reference by faces, forks and holds, as +sint:redo
b32 redosint(typer *u, noun sut, redoctx *c, b32 hod) {
  TP;
  for (;;) {
    noun ref = c->ref;
    if (tagis(ref, "hint")) {
      c->ref = tl(tl(ref));
      continue;
    }
    if (tagis(ref, "face")) {
      nouns xs = {0};
      settap(c->wec, &xs, p->a);
      noun w = nul;
      for (size i = 0; i < xs.len; i++) w = setput(p->a, w, C2(hd(tl(ref)), xs.data[i]));
      c->wec = w;
      c->ref = tl(tl(ref));
      continue;
    }
    if (tagis(ref, "fork")) {
      nouns moy = {0};
      settap(tl(ref), &moy, p->a);
      noun wec = nul;
      nouns refs = {0};
      for (size i = 0; i < moy.len; i++) {
        b32 m;
        if (!miss(u, sut, moy.data[i], &m)) return 0;
        if (m) continue;
        redoctx d = *c;
        d.ref = moy.data[i];
        if (!redosint(u, sut, &d, hod)) return 0;
        wec = setuni(p->a, wec, d.wec);
        *push(&refs, p->a) = d.ref;
      }
      c->wec = wec;
      c->ref = tfork(u, refs.data, refs.len);
      return 1;
    }
    if (tagis(ref, "hold") && hod) {
      if (!(c->ref = repo(u, ref))) return 0;
      continue;
    }
    return 1;
  }
}

noun redodone(typer *u, noun sut, redoctx *c) {
  TP;
  noun wec = c->wec;
  noun lov;
  if (isatom(wec)) {
    lov = nul;
  } else {
    if (!(isatom(mapl(wec)) && isatom(mapr(wec)))) return 0;
    noun har = mapn(wec);
    size lp = lent(c->hos), lq = lent(har);
    size lip = 0, found = 0;
    b32 have = 0;
    for (size k = 0; k <= lp && k <= lq; k++) {
      noun lep = slag((size)(lp - k), c->hos);
      noun lap = scag(p, k, har);
      if (nouneq(lep, lap)) {
        found = k;
        have = 1;
      }
    }
    lip = have ? found : 0;
    lov = weld(p, c->hos, slag(lip, har));
  }
  for (; iscell(lov); lov = tl(lov)) {
    sut = tface(u, hd(lov), sut);
  }
  return sut;
}

noun redodext(typer *u, noun sut, redoctx c) {
  PROF(redo);
  TP;
  noun ref = c.ref;
  if (nouneq(sut, ref) || isnountype(ref) || isvoid(ref) || tagis(ref, "atom") || tagis(ref, "core")) {
    return redodone(u, sut, &c);
  }
  if (isnountype(sut) || isvoid(sut) || tagis(sut, "atom") || tagis(sut, "core")) {
    if (!redosint(u, sut, &c, 1)) return 0;
    return redodone(u, sut, &c);
  }
  if (tagis(sut, "cell")) {
    if (!redosint(u, sut, &c, 1)) return 0;
    if (!tagis(sut, "cell")) return 0;
    noun r2 = tpeek(u, c.ref, "free", D(2));
    noun r3 = tpeek(u, c.ref, "free", D(3));
    if (!r2 || !r3) return 0;
    redoctx d = {nul, setput(p->a, nul, nul), c.gil, r2};
    noun h = redodext(u, hd(tl(sut)), d);
    d.ref = r3;
    noun t = h ? redodext(u, tl(tl(sut)), d) : 0;
    if (!t) return 0;
    return redodone(u, C3(K("cell"), h, t), &c);
  }
  if (tagis(sut, "face")) {
    c.hos = C2(hd(tl(sut)), c.hos);
    return redodext(u, tl(tl(sut)), c);
  }
  if (tagis(sut, "hint")) {
    noun d = redodext(u, tl(tl(sut)), c);
    return d ? thint(u, hd(tl(sut)), d) : 0;
  }
  if (tagis(sut, "fork")) {
    nouns xs = {0};
    settap(tl(sut), &xs, p->a);
    for (size i = 0; i < xs.len; i++) {
      if (!(xs.data[i] = redodext(u, xs.data[i], c))) return 0;
    }
    return tfork(u, xs.data, xs.len);
  }
  // %hold
  if (!redosint(u, sut, &c, 0)) return 0;
  if (guardhas(u->fan, hd(tl(sut)), tl(tl(sut)), 0)) {
    if (!redosint(u, sut, &c, 1)) return 0;
    return redodone(u, sut, &c);
  }
  if (guardhas(c.gil, sut, c.ref, 0)) {
    if (!redosint(u, sut, &c, 0)) return 0;
    return redodone(u, sut, &c);
  }
  noun r = repo(u, sut);
  if (!r) return 0;
  c.gil = guardput(p->a, c.gil, sut, c.ref, 0);
  noun d = redodext(u, r, c);
  if (!d) return 0;
  return nouneq(d, r) ? sut : d;
}

noun redo(typer *u, noun sut, noun ref) {
  TP;
  size j = memofind(sut, ref, 0, nul, memo_redo);
  if (memo.data[j].sut) return memo.data[j].res;
  redoctx c = {nul, setput(p->a, nul, nul), 0, ref};
  noun r = redodext(u, sut, c);
  if (r) memoput(sut, ref, 0, nul, memo_redo, r);
  return r;
}

// whether sut and ref have no nouns in common, as +miss
b32 missdext(typer *u, noun sut, noun ref, guard *gil, b32 *out);

b32 misssint(typer *u, noun sut, noun ref, guard *gil, b32 *out) {
  if (tagis(ref, "atom")) {
    if (!tagis(sut, "atom")) {
      *out = 1;
      return 1;
    }
    noun qr = tl(tl(ref)), qs = tl(tl(sut));
    *out = iscell(qr) && iscell(qs) && !nouneq(qr, qs);
    return 1;
  }
  if (tagis(ref, "cell")) {
    if (!tagis(sut, "cell")) {
      *out = 1;
      return 1;
    }
    if (!missdext(u, hd(tl(sut)), hd(tl(ref)), gil, out)) return 0;
    if (*out) return 1;
    return missdext(u, tl(tl(sut)), tl(tl(ref)), gil, out);
  }
  return missdext(u, ref, sut, gil, out);
}

b32 missdext(typer *u, noun sut, noun ref, guard *gil, b32 *out) {
  PROF(miss);
  TP;
#ifdef HC_EMBED
  // +mint and +mull ask, but this can run long between them
  if (interrupted && stopping()) return 0;
#endif
  if (nouneq(ref, sut)) {
    b32 ok;
    if (!nest(u, K("void"), 0, sut, &ok)) return 0;
    *out = ok;
    return 1;
  }
  if (isvoid(sut)) {
    *out = 1;
    return 1;
  }
  if (isnountype(sut)) {
    b32 ok;
    if (!nest(u, K("void"), 0, ref, &ok)) return 0;
    *out = ok;
    return 1;
  }
  if (tagis(sut, "atom") || tagis(sut, "cell")) return misssint(u, sut, ref, gil, out);
  if (tagis(sut, "core")) return misssint(u, C3(K("cell"), K("noun"), K("noun")), ref, gil, out);
  if (tagis(sut, "fork")) {
    nouns xs = {0};
    settap(tl(sut), &xs, p->a);
    for (size i = 0; i < xs.len; i++) {
      if (!missdext(u, xs.data[i], ref, gil, out)) return 0;
      if (!*out) return 1;
    }
    *out = 1;
    return 1;
  }
  if (tagis(sut, "face") || tagis(sut, "hint")) return missdext(u, tl(tl(sut)), ref, gil, out);
  // %hold
  // the set {sut ref}, in either order
  if (guardhas(gil, sut, ref, 0) || guardhas(gil, ref, sut, 0)) {
    *out = 1;
    return 1;
  }
  noun r = repo(u, sut);
  if (!r) return 0;
  return missdext(u, r, ref, guardput(p->a, gil, sut, ref, 0), out);
}

b32 miss(typer *u, noun sut, noun ref, b32 *out) {
  return missdext(u, sut, ref, 0, out);
}

// whether all wings in a list exist, as +feel
b32 feel(typer *u, noun sut, noun rot, b32 *out) {
  PROF(feel);
  TP;
  rot = flop(p, rot);
  for (; iscell(rot); rot = tl(rot)) {
    pony yep = fondp(u, sut, "free", hd(rot));
    if (yep.how == pcrash) return 0;
    if (yep.how == pnone || yep.how == pskip) {
      *out = 0;
      return 1;
    }
    if (!(sut = fine(u, yep))) return 0;
  }
  *out = 1;
  return 1;
}

// Inference, as +play

noun seminounbunt(typer *u) {
  TP;
  return C2(C4(K("full"), nul, nul, nul), nul);
}

noun playx(typer *u, noun sut, noun gen, noun *spot) {
  TP;
  for (;;) {
    noun g = tl(gen);
    if (iscell(hd(gen))) {
      // a tuple [a b c ...]: the heads in a loop, as it can be as long as
      // the file, then the cells from the end
      nouns hs = {0};
      noun t = gen;
      for (; iscell(t) && iscell(hd(t)); t = tl(t)) {
        noun a = play(u, sut, hd(t));
        if (!a) return 0;
        *push(&hs, p->a) = a;
      }
      noun r = play(u, sut, t);
      for (size i = hs.len - 1; r && i >= 0; i--) r = tcell(u, hs.data[i], r);
      return r;
    }
    noun tag = hd(gen);
    u64 tw = atomfits(tag) ? atomlow(tag) : 0;
    #define IS(t) (tw == TW(t))
    if (IS("brcn") || IS("brpt")) {
      noun garb = C3(hd(g), term(IS("brcn") ? "dry" : "wet"), K("gold"));
      return tcore(u, sut, C4(garb, sut, seminounbunt(u), tl(g)));
    }
    if (IS("cnts")) {
      pony lug = findp(u, sut, "read", hd(g));
      if (lug.how == pcrash) return 0;
      if (lug.how == palias) return isatom(tl(g)) ? lug.type : 0;
      return elbo(u, sut, lug, tl(g));
    }
    if (IS("dtkt")) {
      gen = C2(K("kttr"), hd(g));
      continue;
    }
    if (IS("dtls")) return C3(K("atom"), nul, nul);
    if (IS("rock") || (IS("sand") && iscell(tl(g)))) {
      noun aura = hd(g);
      // constant: atoms become constant atom types, cells recurse
      noun q = tl(g);
      if (isatom(q)) return C3(K("atom"), aura, C2(nul, q));
      noun a = playx(u, sut, C3(K("rock"), aura, hd(q)), spot);
      noun b = a ? playx(u, sut, C3(K("rock"), aura, tl(q)), spot) : 0;
      return b ? C3(K("cell"), a, b) : 0;
    }
    if (IS("sand")) {
      noun aura = hd(g), q = tl(g);
      if (atomeqc(aura, "n")) return atomis(q, 0) ? C3(K("atom"), aura, C2(nul, q)) : 0;
      if (atomeqc(aura, "f")) return atomcmp(q, D(1)) <= 0 ? tbool(u) : 0;
      return C3(K("atom"), aura, nul);
    }
    if (IS("tune")) {
      noun f = tface(u, g, sut);
      if (!u->spots || !u->spot || isvoid(f)) return f;
      return C3(K("hint"), C2(K("noun"), C2(K("spot"), u->spot)), f);
    }
    if (IS("dttr")) return K("noun");
    if (IS("dtts") || IS("dtwt") || IS("fits") || IS("wthx")) return tbool(u);
    if (IS("hand")) return hd(g);
    if (IS("ktbr") || IS("ktpm") || IS("ktwt")) {
      noun t = play(u, sut, g);
      return t ? wrap(u, t, IS("ktbr") ? "iron" : IS("ktpm") ? "zinc" : "lead") : 0;
    }
    if (IS("ktcb")) {
      gen = tl(g);
      continue;
    }
    if (IS("ktls")) {
      gen = hd(g);
      continue;
    }
    if (IS("ktsg") || IS("zpcm")) {
      gen = IS("ktsg") ? g : hd(g);
      continue;
    }
    if (IS("note")) {
      noun t = play(u, sut, tl(g));
      return t ? thint(u, C2(sut, hd(g)), t) : 0;
    }
    if (IS("sgzp") || IS("sggr") || IS("dbug")) {
      if (IS("dbug")) *spot = u->spots ? (u->spot = hd(g)) : hd(g);
      gen = tl(g);
      continue;
    }
    if (IS("tsgr")) {
      if (!(sut = play(u, sut, hd(g)))) return 0;
      gen = tl(g);
      continue;
    }
    if (IS("tscm")) {
      sut = C3(K("face"), C2(nul, C2(hd(g), nul)), sut);
      gen = tl(g);
      continue;
    }
    if (IS("wtcl")) {
      noun fex = chip(u, sut, 1, hd(g));
      noun wux = fex ? chip(u, sut, 0, hd(g)) : 0;
      if (!wux) return 0;
      noun a = isvoid(fex) ? K("void") : play(u, fex, hd(tl(g)));
      noun b = !a ? 0 : isvoid(wux) ? K("void") : play(u, wux, tl(tl(g)));
      return b ? tfork2(u, a, b) : 0;
    }
    if (IS("lost") || IS("zpzp")) return K("void");
    if (IS("zpmc")) {
      noun a = play(u, sut, hd(g));
      noun b = a ? play(u, sut, tl(g)) : 0;
      return b ? tcell(u, a, b) : 0;
    }
    if (IS("zpgl")) {
      gen = C2(K("kttr"), hd(g));
      continue;
    }
    if (IS("zpts")) return K("noun");
    if (IS("zppt")) {
      b32 ok;
      if (!feel(u, sut, hd(g), &ok)) return 0;
      // ?.((feel p.gen) q.gen r.gen): r when the wings are there
      gen = ok ? tl(tl(g)) : hd(tl(g));
      continue;
    }
    #undef IS
    noun doz = hoonopen(p, gen);
    if (!doz || nouneq(doz, gen)) return 0;
    gen = doz;
  }
}

// memoized by hoon identity and subject shape, see playslot
noun play(typer *u, noun sut, noun gen) {
#ifdef HC_EMBED
  if (interrupted && stopping()) return 0;
#endif
  PROF(play);
  if (stackdeep()) return 0;
  if (playmemo.len*4 >= playmemo.cap*3) {
    playcell *old = playmemo.data;
    size oldcap = playmemo.cap;
    playmemo.cap = oldcap ? oldcap * 2 : 1 << 12;
    playmemo.data = new(&H.perm, playcell, playmemo.cap);
    for (size i = 0; i < oldcap; i++) {
      if (!old[i].sut) continue;
      size j = playslot(u, old[i].sut, old[i].gen);
      playmemo.data[j] = old[i];
    }
    if (oldcap) osrelease((byte*)old, (byte*)(old + oldcap));
  }
  // with spots the types differ, so they're kept apart
  noun key = u->spots ? cons(u->p->a, K("spots"), gen) : gen;
  size j = playslot(u, sut, key);
  if (playmemo.data[j].sut) return playmemo.data[j].res;
  noun spot = 0, was = u->spot;
  noun r = playx(u, sut, gen, &spot);
  u->spot = was;
  if (!r) {
    if (spot) errspot(spot);
    return 0;
  }
  j = playslot(u, sut, key);
  playcell c = {sut, key, r};
  playmemo.data[j] = c;
  playmemo.len++;
  return r;
}

// Output

void appendhexatom(bufout *b, noun n) {
  s8 digits = S("0123456789abcdef");
  if (!alen(n)) {
    append(b, S("0"));
    return;
  }
  b32 lead = 1;
  for (size i = alen(n) - 1; i >= 0; i--) {
    for (i32 sh = 4; sh >= 0; sh -= 4) {
      u8 d = (abyte(n, i) >> sh) & 0xf;
      if (lead && !d) continue;
      lead = 0;
      s8 s = {digits.buf + d, 1};
      append(b, s);
    }
  }
}

// canonical form to a depth, deeper cells as …
void appendrawto(bufout *b, noun n, i32 depth) {
  if (!depth) {
    append(b, S("\xe2\x80\xa6"));
    return;
  }
  if (isatom(n)) {
    appendhexatom(b, n);
    return;
  }
  append(b, S("["));
  appendrawto(b, hd(n), depth - 1);
  append(b, S(" "));
  appendrawto(b, tl(n), depth - 1);
  append(b, S("]"));
}

// canonical form: fully bracketed cells, atoms in hex
void appendraw(bufout *b, noun n) {
  size depth = 0;
  for (; iscell(n); n = tl(n), depth++) {
    append(b, S("["));
    appendraw(b, hd(n));
    append(b, S(" "));
  }
  appendhexatom(b, n);
  for (size i = 0; i < depth; i++) append(b, S("]"));
}

b32 isterm(noun n) {
  if (!alen(n) || abyte(n, 0) < 'a' || abyte(n, 0) > 'z') return 0;
  for (size i = 0; i < alen(n); i++) {
    u8 c = abyte(n, i);
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return 0;
  }
  return 1;
}

b32 iscord(noun n) {
  if (alen(n) < 2) return 0;
  for (size i = 0; i < alen(n); i++) {
    if (abyte(n, i) < 32 || abyte(n, i) > 126) return 0;
  }
  return 1;
}

void appendatom(bufout *b, noun n) {
  if (!alen(n)) {
    append(b, S("~"));
  } else if (isterm(n)) {
    append(b, S("%"));
    u8 tmp[8];
    s8 s = {abytes(n, tmp), alen(n)};
    append(b, s);
  } else if (iscord(n)) {
    append(b, S("'"));
    for (size i = 0; i < alen(n); i++) {
      if (abyte(n, i) == '\'' || abyte(n, i) == '\\') append(b, S("\\"));
      u8 c = abyte(n, i);
      s8 s = {&c, 1};
      append(b, s);
    }
    append(b, S("'"));
  } else if (alen(n) <= 8) {
    appendsize(b, (size)atomlow(n));
  } else {
    append(b, S("0x"));
    appendhexatom(b, n);
  }
}

// readable form: right-nested cells collapsed, atoms as terms or numbers
void appendnoun(bufout *b, noun n) {
  if (isatom(n)) {
    appendatom(b, n);
    return;
  }
  append(b, S("["));
  appendnoun(b, hd(n));
  for (n = tl(n); iscell(n); n = tl(n)) {
    append(b, S(" "));
    appendnoun(b, hd(n));
  }
  append(b, S(" "));
  appendatom(b, n);
  append(b, S("]"));
}

// Read all of stdin
s8 readstdin(arena *a) {
  size cap = 1 << 16;
  s8 r = {new(a, u8, cap), 0};
  for (;;) {
    if (r.len == cap) {
      u8 *more = new(a, u8, cap*2);
      copy((byte*)more, (byte*)r.buf, r.len);
      r.buf = more;
      cap *= 2;
    }
    size n = osread(0, r.buf + r.len, cap - r.len);
    if (n <= 0) break;
    r.len += n;
  }
  return r;
}

void appendmug(bufout *b, noun n) {
  appendsize(b, (size)mug(n));
}

// The compiler state: the typer, and the sample of +ut that +play drops.
typedef struct {
  typer u;
  b32   vet;      // check types, as vet in +ut; off for wet arms
  guard *rib;     // [type type hoon] wet arms being checked by +fire
} minter;

#define MP typer *u = &m->u; parser *p = u->p; (void)u; (void)p



noun mint(minter *m, noun sut, noun gol, noun gen);
noun mull(minter *m, noun sut, noun gol, noun dox, noun gen);

// Formulas

// whether a formula is [op v] for a small v
b32 isfol(noun n, u64 op, u64 v) {
  return iscell(n) && atomis(hd(n), op) && atomis(tl(n), v);
}

b32 isop(noun n, u64 op) {
  return iscell(n) && atomis(hd(n), op);
}

// a formula cell, as +cons
noun fcons(parser *p, noun vur, noun sed) {
  if (isop(vur, 1) && isop(sed, 1)) return C3(D(1), tl(vur), tl(sed));
  return C2(vur, sed);
}

// compose two formulas, as +comb
noun comb(parser *p, noun mal, noun buz) {
  if (isop(mal, 0) && isatom(tl(mal)) && !atomis(tl(mal), 0)) {
    if (isop(buz, 0) && isatom(tl(buz)) && !atomis(tl(buz), 0)) {
      return C2(D(0), peg(p, tl(mal), tl(buz)));
    }
    noun b = tl(buz);
    if (isop(buz, 2) && iscell(b) && isop(hd(b), 0) && isop(tl(b), 0)
        && isatom(tl(hd(b))) && isatom(tl(tl(b)))) {
      return C3(D(2), C2(D(0), peg(p, tl(mal), tl(hd(b)))), C2(D(0), peg(p, tl(mal), tl(tl(b)))));
    }
    return C3(D(7), mal, buz);
  }
  if (iscell(mal) && iscell(hd(mal)) && isfol(tl(mal), 0, 1)) return C3(D(8), hd(mal), buz);
  if (isfol(buz, 0, 1)) return mal;
  return C3(D(7), mal, buz);
}

// if-then-else, as +cond
noun cond(parser *p, noun pex, noun yom, noun woq) {
  if (isfol(pex, 1, 0)) return yom;
  if (isfol(pex, 1, 1)) return woq;
  if (isfol(pex, 0, 0)) return pex;
  return C4(D(6), pex, yom, woq);
}

// loobean and, or and not, as +flan, +flor and +flip
noun flan(parser *p, noun bos, noun nif) {
  if (nouneq(bos, nif) || isfol(bos, 1, 1) || isfol(nif, 1, 0) || isfol(bos, 0, 0)) return bos;
  if (isfol(bos, 1, 0) || isfol(nif, 1, 1) || isfol(nif, 0, 0)) return nif;
  return C4(D(6), bos, nif, C2(D(1), NO));
}

noun flor(parser *p, noun bos, noun nif) {
  if (nouneq(bos, nif) || isfol(bos, 1, 0) || isfol(nif, 1, 1) || isfol(bos, 0, 0)) return bos;
  if (isfol(bos, 1, 1) || isfol(nif, 1, 0) || isfol(nif, 0, 0)) return nif;
  return C4(D(6), bos, C2(D(1), YES), nif);
}

noun flip(parser *p, noun dyr) {
  if (isfol(dyr, 1, 0)) return C2(D(1), NO);
  if (isfol(dyr, 1, 1)) return C2(D(1), YES);
  if (isfol(dyr, 0, 0)) return dyr;
  return C4(D(6), dyr, C2(D(1), NO), C2(D(1), YES));
}

// the axis of a [0 axis] formula under hints, as +cove; 0 on crash
noun cove(noun nug) {
  for (;;) {
    if (isop(nug, 0)) return tl(nug);
    if (!isop(nug, 11) || isatom(tl(nug))) return 0;
    nug = tl(tl(nug));
  }
}

// Edits of a subject, as +hike: axis a with [axis formula] changes,
// merged up the tree where siblings both change and applied with nock
// 10, deepest axis innermost. Earlier changes win, and the list comes
// in the reverse of the order they were written.

typedef struct {
  noun axe;
  noun fol;
} hikecell;

typedef struct {
  hikecell *data;
  size      len;
  size      cap;
} hikecells;

size hikefind(hikecells *n, noun axe) {
  for (size i = 0; i < n->len; i++) {
    if (nouneq(n->data[i].axe, axe)) return i;
  }
  return -1;
}

void hikedel(hikecells *n, size i) {
  n->data[i] = n->data[--n->len];
}

// whether contained is strictly below container
b32 hikecontains(parser *p, noun container, noun contained) {
  size big = atombits(container), small = atombits(contained);
  if (small <= big) return 0;
  return nouneq(container, atomrsh(p->a, contained, small - big));
}

void hikeinsert(parser *p, hikecells *n, noun axe, noun fol) {
  for (;;) {
    for (noun a = axe; !atomis(a, 1) && alen(a); a = atomrsh(p->a, a, 1)) {
      if (hikefind(n, a) >= 0) return;   // a parent, or itself, is in
    }
    for (size i = 0; i < n->len; i++) {
      if (hikecontains(p, axe, n->data[i].axe)) hikedel(n, i--);
    }
    b32 odd = atombit(axe, 0);
    noun sib = odd ? atomsub(p->a, axe, D(1)) : atomaddsmall(p->a, axe, 1);
    size j = hikefind(n, sib);
    if (j < 0) {
      *push(n, p->a) = (hikecell){axe, fol};
      return;
    }
    noun un = n->data[j].fol;
    hikedel(n, j);
    fol = atomcmp(sib, axe) > 0 ? fcons(p, fol, un) : fcons(p, un, fol);
    axe = atomrsh(p->a, sib, 1);
  }
}

noun hike(parser *p, noun a, noun pac) {
  PROF(hike);
  hikecells n = {0};
  for (; iscell(pac); pac = tl(pac)) hikeinsert(p, &n, hd(hd(pac)), tl(hd(pac)));
  // ascending, so the largest axis ends up outermost
  for (size i = 1; i < n.len; i++) {
    hikecell c = n.data[i];
    size j = i;
    for (; j > 0 && atomcmp(n.data[j-1].axe, c.axe) > 0; j--) n.data[j] = n.data[j-1];
    n.data[j] = c;
  }
  noun r = C2(D(0), a);
  for (size i = 0; i < n.len; i++) r = C3(D(10), C2(n.data[i].axe, n.data[i].fol), r);
  return r;
}

#ifdef HC_EMBED
// The host. The kernel that's compiling is the one running in vere,
// and the gate +laze makes is closed over it: the host makes the gate,
// by running that kernel's +laze, and says what a gate found in a type
// was made from, if it's one this kernel made. Formulas are run by the
// host too, with its jets, as +mack.

noun hostlaze(noun from);              // the gate +laze makes of from
noun hostfrom(noun gate);              // what a gate was made from, or 0
noun hostmack(noun core, noun axe);    // (mack core [9 axe 0 1]), 0 for ~
#endif

// Nock, for evaluating constants at compile time

// the subtree at an axis, as +frag; 0 if there's none
noun frag(noun axe, noun n) {
  if (!alen(axe)) return 0;
  for (size i = atombits(axe) - 2; i >= 0; i--) {
    if (isatom(n)) return 0;
    n = atombit(axe, i) ? tl(n) : hd(n);
  }
  return n;
}

// n with the subtree at axe replaced by v, as +edit; 0 if there's none
noun edit(parser *p, noun axe, noun n, noun v) {
  if (!alen(axe)) return 0;
  size bits = atombits(axe);
  if (bits == 1) return v;
  if (isatom(n)) return 0;
  b32 right = atombit(axe, bits - 2);
  noun lat = atomor(p->a, atomend(p->a, axe, bits - 2), atomlsh(p->a, D(1), bits - 2));
  noun d = edit(p, lat, right ? tl(n) : hd(n), v);
  if (!d) return 0;
  return right ? C2(hd(n), d) : C2(d, tl(n));
}

// Jets. Cores made under a %fast hint, as ~/ and ~% make them, are
// recorded by battery with their name and their parent's battery, as
// vere does. A call of arm 2 of a gate recorded under one of the names
// below, in the kernel's first two layers, runs in C instead: ^~ and
// the like would otherwise do arithmetic by decrement, and hash nouns
// a byte at a time to order sets.

nounmap fastnames;    // battery to name
nounmap fastparents;  // battery to the parent's battery, or ~

// record a core made under [%fast clue], clue [name parent hooks] with
// parent the formula [0 axis] of the parent within the core, or ~
void fastrecord(noun clue, noun core) {
  if (!iscell(clue) || !isatom(hd(clue)) || !iscell(tl(clue)) || !iscell(core)) return;
  noun par = hd(tl(clue));
  noun pb = nul;
  if (iscell(par) && atomis(hd(par), 0) && isatom(tl(par)) && !atomis(tl(par), 0)) {
    noun pc = frag(tl(par), core);
    if (!pc || isatom(pc)) return;
    pb = hd(pc);
  }
  nounmapput(&fastnames, hd(core), hd(clue));
  nounmapput(&fastparents, hd(core), pb);
}

// a bite, $@(bloq [bloq step]), as bits; 0 if too big to do here
b32 bitebits(noun bite, size *bits) {
  noun bloq = isatom(bite) ? bite : hd(bite);
  noun step = isatom(bite) ? atomu64(0, 1) : tl(bite);
  if (!atomfits(bloq) || atomlow(bloq) > 32 || !atomfits(step) || atomlow(step) >= 1ull << 31) return 0;
  *bits = (size)(atomlow(step) << atomlow(bloq));
  return *bits < (size)1 << 34;
}

// the blocks of 2^bloq bits a takes, as +met
noun jetmet(parser *p, noun bloq, noun a) {
  size n = atombits(a);
  if (!atomfits(bloq) || atomlow(bloq) > 62) return D(n ? 1 : 0);
  u64 k = atomlow(bloq);
  return D((u64)((n + ((size)1 << k) - 1) >> k));
}

typedef enum { jet_punt, jet_done, jet_crash } jetres;

// the product of a jetted gate on its sample
jetres jet(parser *p, noun core, noun *out) {
  noun nm = nounmapget(&fastnames, hd(core));
  if (!nm) return jet_punt;
  noun par = nounmapget(&fastparents, hd(core));
  noun pn = par ? nounmapget(&fastnames, par) : 0;
  if (!pn || !(atomeqc(pn, "one") || atomeqc(pn, "two"))) return jet_punt;
  noun s = frag(D(6), core);
  if (!s) return jet_punt;
  arena *a = p->a;
  #define IS(t) atomeqc(nm, t)
  #define DONE(v) do { *out = (v); return jet_done; } while (0)
  if (IS("dec") || IS("bex")) {
    if (iscell(s)) return jet_punt;
    if (IS("dec")) {
      if (atomis(s, 0)) return jet_crash;
      DONE(atomsub(a, s, D(1)));
    }
    if (!atomfits(s) || atomlow(s) >= 1ull << 34) return jet_punt;
    DONE(atomlsh(a, D(1), (size)atomlow(s)));
  }
  if (IS("mug")) DONE(D(mug(s)));
  if (isatom(s)) return jet_punt;
  noun x = hd(s), y = tl(s);
  if (IS("gor")) DONE(gor(x, y) ? YES : NO);
  if (IS("mor")) DONE(mor(x, y) ? YES : NO);
  if (IS("dor")) DONE(dor(x, y) ? YES : NO);
  if (IS("aor")) DONE(aor(x, y) ? YES : NO);
  if (IS("muk")) {
    // [syd len key], seed the low 32 bits of syd
    if (iscell(x) || isatom(y) || iscell(hd(y)) || iscell(tl(y))) return jet_punt;
    noun len = hd(y), key = tl(y);
    if (!atomfits(len) || atomlow(len) >= 1ull << 31) return jet_punt;
    u8 tmp[8];
    u32 syd = (u32)atomlow(x);
    DONE(D(muk(syd, (size)atomlow(len), abytes(key, tmp), alen(key))));
  }
  if (IS("add") || IS("sub") || IS("mul") || IS("div") || IS("mod") || IS("dvr")
      || IS("lth") || IS("lte") || IS("gth") || IS("gte") || IS("max") || IS("min")
      || IS("con") || IS("dis") || IS("mix")) {
    if (iscell(x) || iscell(y)) return jet_punt;
    i32 c = atomcmp(x, y);
    if (IS("add")) DONE(atomadd(a, x, y));
    if (IS("sub")) {
      if (c < 0) return jet_crash;
      DONE(atomsub(a, x, y));
    }
    if (IS("mul")) DONE(atommul(a, x, y));
    if (IS("lth")) DONE(c < 0 ? YES : NO);
    if (IS("lte")) DONE(c <= 0 ? YES : NO);
    if (IS("gth")) DONE(c > 0 ? YES : NO);
    if (IS("gte")) DONE(c >= 0 ? YES : NO);
    if (IS("max")) DONE(c >= 0 ? x : y);
    if (IS("min")) DONE(c <= 0 ? x : y);
    if (IS("con")) DONE(atomor(a, x, y));
    if (IS("dis")) DONE(atomandxor(a, x, y, 0));
    if (IS("mix")) DONE(atomandxor(a, x, y, 1));
    if (atomis(y, 0)) return jet_crash;
    noun r;
    noun q = atomdiv(a, x, y, &r);
    if (IS("div")) DONE(q);
    if (IS("mod")) DONE(r);
    DONE(C2(q, r));
  }
  if (IS("met")) {
    if (iscell(x) || iscell(y)) return jet_punt;
    DONE(jetmet(p, x, y));
  }
  if (IS("end") || IS("lsh") || IS("rsh")) {
    size bits;
    if (iscell(y) || !bitebits(x, &bits)) return jet_punt;
    if (IS("end")) DONE(atomend(a, y, bits));
    if (IS("lsh")) DONE(atomlsh(a, y, bits));
    DONE(atomrsh(a, y, bits));
  }
  if (IS("cat")) {
    // [bloq a b]: a, then b after it
    if (isatom(y) || iscell(hd(y)) || iscell(tl(y)) || !atomfits(x) || atomlow(x) > 32) return jet_punt;
    noun m = jetmet(p, x, hd(y));
    if (!atomfits(m)) return jet_punt;
    size bits = (size)(atomlow(m) << atomlow(x));
    if (bits >= (size)1 << 34) return jet_punt;
    DONE(atomadd(a, hd(y), atomlsh(a, tl(y), bits)));
  }
  if (IS("cut")) {
    // [bloq [from=step len=step] a]
    if (isatom(y) || isatom(hd(y)) || iscell(tl(y)) || !atomfits(x) || atomlow(x) > 32) return jet_punt;
    noun b = hd(hd(y)), c = tl(hd(y));
    if (!atomfits(b) || !atomfits(c) || atomlow(b) >= 1ull << 31 || atomlow(c) >= 1ull << 31) return jet_punt;
    size from = (size)(atomlow(b) << atomlow(x)), len = (size)(atomlow(c) << atomlow(x));
    if (from >= (size)1 << 34 || len >= (size)1 << 34) return jet_punt;
    DONE(atomend(a, atomrsh(a, tl(y), from), len));
  }
  if (IS("rap")) {
    // [bloq (list @)]: the atoms, each after the last
    if (iscell(x) || !atomfits(x) || atomlow(x) > 32) return jet_punt;
    nouns xs = {0};
    for (noun l = y; iscell(l); l = tl(l)) {
      if (iscell(hd(l))) return jet_punt;
      *push(&xs, a) = hd(l);
    }
    noun r = nul;
    for (size i = xs.len - 1; i >= 0; i--) {
      noun m = jetmet(p, x, xs.data[i]);
      size bits = (size)(atomlow(m) << atomlow(x));
      if (bits >= (size)1 << 34) return jet_punt;
      r = atomadd(a, xs.data[i], atomlsh(a, r, bits));
    }
    DONE(r);
  }
  if (IS("rip")) {
    // [bite a]: a in pieces, low first
    size bits;
    if (iscell(y) || !bitebits(x, &bits) || !bits) return jet_punt;
    nouns xs = {0};
    for (noun b = y; !atomis(b, 0); b = atomrsh(a, b, bits)) *push(&xs, a) = atomend(a, b, bits);
    DONE(nounslist(p, &xs));
  }
  #undef IS
  #undef DONE
  return jet_punt;
}

// the product of a formula against a subject, as +mack: 0 where +mink
// would fail or block, with no scry
noun nock(parser *p, noun sub, noun fol) {
  PROF(nock);
  for (;;) {
    if (isatom(fol)) return 0;
    noun op = hd(fol), arg = tl(fol);
    if (iscell(op)) {
      noun h = nock(p, sub, op);
      noun t = h ? nock(p, sub, arg) : 0;
      return t ? C2(h, t) : 0;
    }
    if (!atomfits(op) || atomlow(op) > 11) return 0;
    switch (atomlow(op)) {
    case 0:
      return isatom(arg) ? frag(arg, sub) : 0;
    case 1:
      return arg;
    case 2: {
      if (isatom(arg)) return 0;
      noun s = nock(p, sub, hd(arg));
      noun f = s ? nock(p, sub, tl(arg)) : 0;
      if (!f || stackdeep()) return 0;
      sub = s;
      fol = f;
      continue;
    }
    case 3: {
      noun x = nock(p, sub, arg);
      return x ? (iscell(x) ? YES : NO) : 0;
    }
    case 4: {
      noun x = nock(p, sub, arg);
      return x && isatom(x) ? atomaddsmall(p->a, x, 1) : 0;
    }
    case 5: {
      if (isatom(arg)) return 0;
      noun a = nock(p, sub, hd(arg));
      noun b = a ? nock(p, sub, tl(arg)) : 0;
      return b ? (nouneq(a, b) ? YES : NO) : 0;
    }
    case 6: {
      if (isatom(arg) || isatom(tl(arg))) return 0;
      noun t = nock(p, sub, hd(arg));
      if (!t || !isatom(t) || atomcmp(t, D(1)) > 0) return 0;
      fol = atomis(t, 0) ? hd(tl(arg)) : tl(tl(arg));
      continue;
    }
    case 7: {
      if (isatom(arg)) return 0;
      noun s = nock(p, sub, hd(arg));
      if (!s) return 0;
      sub = s;
      fol = tl(arg);
      continue;
    }
    case 8: {
      if (isatom(arg)) return 0;
      noun h = nock(p, sub, hd(arg));
      if (!h) return 0;
      sub = C2(h, sub);
      fol = tl(arg);
      continue;
    }
    case 9: {
      if (isatom(arg) || iscell(hd(arg))) return 0;
      noun core = nock(p, sub, tl(arg));
      if (core && iscell(core) && atomis(hd(arg), 2)) {
        noun r;
        jetres j = jet(p, core, &r);
        if (j == jet_done) return r;
        if (j == jet_crash) return 0;
      }
      noun arm = core ? frag(hd(arg), core) : 0;
      if (!arm || stackdeep()) return 0;
      sub = core;
      fol = arm;
      continue;
    }
    case 10: {
      if (isatom(arg) || isatom(hd(arg)) || iscell(hd(hd(arg))) || atomis(hd(hd(arg)), 0)) return 0;
      noun t = nock(p, sub, tl(arg));
      noun v = t ? nock(p, sub, tl(hd(arg))) : 0;
      return v ? edit(p, hd(hd(arg)), t, v) : 0;
    }
    default: {   // 11
      if (isatom(arg)) return 0;
      if (iscell(hd(arg))) {
        if (iscell(hd(hd(arg)))) return 0;
        noun clue = nock(p, sub, tl(hd(arg)));
        if (!clue) return 0;
        if (atomeqc(hd(hd(arg)), "fast")) {
          noun core = nock(p, sub, tl(arg));
          if (core) fastrecord(clue, core);
          return core;
        }
      }
      fol = tl(arg);
      continue;
    }
    }
  }
}

// Seminouns, partial nouns for +musk: [mask data], the mask
//
//   [%full blocks]      complete, or blocked when blocks isn't ~
//   [%half left rite]   a cell with parts of each
//   [%lazy axis laze]   a fragment of a battery still being compiled
//
// where laze is [%laze sut nym hud dom vet], everything the gate +laze
// makes in hoon closes over, so equal batteries have equal lazes.

noun semfull(parser *p, noun data) {
  return C2(C2(K("full"), nul), data);
}

// blocked, as hoon's [full/[~ ~ ~] ~]
noun semblock(parser *p) {
  return C2(C2(K("full"), C3(nul, nul, nul)), nul);
}

noun semmask(noun s) { return hd(s); }
noun semdata(noun s) { return tl(s); }

// combine two seminouns into a cell, as +combine:musk
noun combine(parser *p, noun hed, noun tal) {
  noun mh = semmask(hed), mt = semmask(tal);
  if (!(tagis(mh, "full") && tagis(mt, "full") && isatom(tl(mh)) == isatom(tl(mt)))) {
    return C2(C3(K("half"), mh, mt), C2(semdata(hed), semdata(tal)));
  }
  if (isatom(tl(mh))) return semfull(p, C2(semdata(hed), semdata(tal)));
  return C2(C2(K("full"), setuni(p->a, tl(mh), tl(mt))), nul);
}

noun lazeresolve(minter *m, noun laz, noun axe, b32 *crash);

// complete any laziness, as +complete:musk; 0 on a crash
noun complete(minter *m, noun bus) {
  MP;
  noun k = semmask(bus);
  if (tagis(k, "full")) return bus;
  if (tagis(k, "lazy")) {
    noun frg = hd(tl(k));
    if (atomis(frg, 1)) return semblock(p);
    b32 crash = 0;
    noun v = lazeresolve(m, tl(tl(k)), frg, &crash);
    if (crash) return 0;
    return v ? semfull(p, v) : semblock(p);
  }
  noun h = complete(m, C2(hd(tl(k)), hd(semdata(bus))));
  noun t = h ? complete(m, C2(tl(tl(k)), tl(semdata(bus)))) : 0;
  return t ? combine(p, h, t) : 0;
}

// the subtree at an axis, as +fragment:musk: nul to stop, 0 on a crash
noun fragment(minter *m, noun axe, noun bus) {
  MP;
  for (;;) {
    if (atomis(axe, 1)) return bus;
    u64 now = cap(axe);
    noun lat = mas(p, axe);
    noun k = semmask(bus);
    if (tagis(k, "lazy")) return C2(C3(K("lazy"), peg(p, hd(tl(k)), axe), tl(tl(k))), semdata(bus));
    if (tagis(k, "full")) {
      if (iscell(tl(k))) return bus;
      if (isatom(semdata(bus))) return nul;
      noun d = semdata(bus);
      bus = semfull(p, now == 2 ? hd(d) : tl(d));
      axe = lat;
      continue;
    }
    noun d = semdata(bus);
    bus = now == 2 ? C2(hd(tl(k)), hd(d)) : C2(tl(tl(k)), tl(d));
    axe = lat;
  }
}

// change the subtree at an axis, as +mutate:musk
noun mutate(minter *m, noun axe, noun lit, noun big) {
  MP;
  if (!alen(axe)) return nul;
  if (atomis(axe, 1)) return lit;
  for (;;) {
    if (atomis(axe, 2) || atomis(axe, 3)) {
      noun o = fragment(m, D(atomis(axe, 2) ? 3 : 2), big);
      if (!o || isatom(o)) return o;
      return atomis(axe, 2) ? combine(p, lit, o) : combine(p, o, lit);
    }
    noun mor = mas(p, axe);
    noun hed = fragment(m, D(2), big);
    if (!hed || isatom(hed)) return hed;
    noun tal = fragment(m, D(3), big);
    if (!tal || isatom(tal)) return tal;
    noun mut = mutate(m, mor, lit, cap(axe) == 2 ? hed : tal);
    if (!mut || isatom(mut)) return mut;
    return cap(axe) == 2 ? combine(p, mut, tal) : combine(p, hed, mut);
  }
}

// the blocks of a stencil, as +squash:musk; 0 on a crash
noun squash(minter *m, noun tyn) {
  MP;
  if (tagis(tyn, "full")) return tl(tyn);
  if (tagis(tyn, "lazy")) {
    noun c = complete(m, C2(tyn, nul));
    return c ? squash(m, semmask(c)) : 0;
  }
  noun l = squash(m, hd(tl(tyn)));
  noun r = l ? squash(m, tl(tl(tyn))) : 0;
  return r ? setuni(p->a, l, r) : 0;
}

// a step that needs a complete noun, as +require:musk: 0 on a crash, 1
// with the result to produce in *out, or 2 with the noun in *data
i32 require(minter *m, noun noy, noun *out, noun *data) {
  MP;
  if (isatom(noy)) {
    *out = nul;
    return 1;
  }
  noun bus = complete(m, noy);
  if (!bus) return 0;
  noun k = semmask(bus);
  if (tagis(k, "lazy")) return 0;
  if (tagis(k, "half")) {
    noun b = squash(m, k);
    if (!b) return 0;
    *out = C2(C2(K("full"), b), nul);
    return 1;
  }
  if (iscell(tl(k))) {
    *out = C2(k, nul);
    return 1;
  }
  *data = semdata(bus);
  return 2;
}

// run a formula on a partial subject, as +araw:musk: nul to stop, 0 on
// a crash, else a seminoun
noun araw(minter *m, noun bus, noun fol) {
  PROF(araw);
  if (stackdeep()) return 0;
  MP;
  #define REQ(noy, d) do { noun o_; i32 r_ = require(m, (noy), &o_, &(d)); \
                           if (!r_) return 0; if (r_ == 1) return o_; } while (0)
  for (;;) {
    if (isatom(fol)) return nul;
    noun op = hd(fol), arg = tl(fol);
    if (iscell(op)) {
      noun h = araw(m, bus, op);
      if (!h || isatom(h)) return h;
      noun t = araw(m, bus, arg);
      if (!t || isatom(t)) return t;
      return combine(p, h, t);
    }
    if (!atomfits(op) || atomlow(op) > 11) return nul;
    switch (atomlow(op)) {
    case 0:
      if (iscell(arg)) return nul;
      if (atomis(arg, 0)) return nul;
      return fragment(m, arg, bus);
    case 1:
      return semfull(p, arg);
    case 2: {
      if (isatom(arg)) return nul;
      noun ryf;
      noun c = araw(m, bus, tl(arg));
      if (!c) return 0;
      REQ(c, ryf);
      noun lub = araw(m, bus, hd(arg));
      if (!lub || isatom(lub)) return lub;
      bus = lub;
      fol = ryf;
      continue;
    }
    case 3: case 4: {
      noun fig;
      noun c = araw(m, bus, arg);
      if (!c) return 0;
      REQ(c, fig);
      if (atomis(op, 3)) return semfull(p, iscell(fig) ? YES : NO);
      return iscell(fig) ? nul : semfull(p, atomaddsmall(p->a, fig, 1));
    }
    case 5: {
      if (isatom(arg)) return nul;
      noun hed, tal;
      noun c = araw(m, bus, hd(arg));
      if (!c) return 0;
      REQ(c, hed);
      c = araw(m, bus, tl(arg));
      if (!c) return 0;
      REQ(c, tal);
      return semfull(p, nouneq(hed, tal) ? YES : NO);
    }
    case 6: {
      if (isatom(arg) || isatom(tl(arg))) return nul;
      noun fig;
      noun c = araw(m, bus, hd(arg));
      if (!c) return 0;
      REQ(c, fig);
      if (atomis(fig, 0)) fol = hd(tl(arg));
      else if (atomis(fig, 1)) fol = tl(tl(arg));
      else return nul;
      continue;
    }
    case 7: case 8: {
      if (isatom(arg)) return nul;
      noun one = araw(m, bus, hd(arg));
      if (!one || isatom(one)) return one;
      bus = atomis(op, 7) ? one : combine(p, one, bus);
      fol = tl(arg);
      continue;
    }
    case 9: {
      if (isatom(arg) || iscell(hd(arg))) return nul;
      noun one = araw(m, bus, tl(arg));
      if (!one || isatom(one)) return one;
      noun k = semmask(one);
      if (tagis(k, "full") && isatom(tl(k))) {
        u64 d = deeps;
#ifdef HC_EMBED
        noun r = hostmack(semdata(one), hd(arg));
#else
        noun r = nock(p, semdata(one), C3(D(9), hd(arg), C2(D(0), D(1))));
#endif
        if (!r && deeps != d) return 0;
        return r ? semfull(p, r) : nul;
      }
      noun ryf;
      noun f = fragment(m, hd(arg), one);
      if (!f) return 0;
      REQ(f, ryf);
      bus = one;
      fol = ryf;
      continue;
    }
    case 10: {
      if (isatom(arg) || isatom(hd(arg)) || iscell(hd(hd(arg)))) return nul;
      noun tar = araw(m, bus, tl(arg));
      if (!tar || isatom(tar)) return tar;
      noun inn = araw(m, bus, tl(hd(arg)));
      if (!inn || isatom(inn)) return inn;
      return mutate(m, hd(hd(arg)), inn, tar);
    }
    default: {   // 11
      if (isatom(arg)) return nul;
      if (iscell(hd(arg))) {
        if (iscell(hd(hd(arg)))) return nul;
        noun noy = araw(m, bus, tl(hd(arg)));
        if (!noy || isatom(noy)) return noy;
      }
      fol = tl(arg);
      continue;
    }
    }
  }
  #undef REQ
}

// Type helpers

noun tbool2(minter *m) {
  return tbool(&m->u);
}

// [%face [~ [gen ~]] sut], as +busk
noun busk(minter *m, noun sut, noun gen) {
  MP;
  return C3(K("face"), C2(nul, C2(gen, nul)), sut);
}

b32 nice(minter *m, noun gol, noun typ) {
  b32 ok;
  return !m->vet || nest(&m->u, gol, 1, typ, &ok);
}

noun burpx(minter *m, noun sut);

nounmap burps;

// expel undigested seminouns, as +burp: kept by type, as types share
// their parts and a walk as a tree would see a kernel's many times over
noun burp(minter *m, noun sut) {
  if (isatom(sut)) return sut;
  noun r = nounmapget(&burps, sut);
  if (!r) nounmapput(&burps, sut, r = burpx(m, sut));
  return r;
}

noun burpx(minter *m, noun sut) {
  PROF(burp);
  MP;
  noun g = tl(sut);
  if (tagis(sut, "cell")) return C3(K("cell"), burp(m, hd(g)), burp(m, tl(g)));
  if (tagis(sut, "core")) {
    noun c = tl(g);
    noun sem = hd(tl(tl(c)));
    noun k = semmask(sem);
    if (!(tagis(k, "full") && isatom(tl(k)))) sem = C2(C4(K("full"), nul, nul, nul), nul);
    noun coil = C3(hd(c), burp(m, hd(tl(c))), C2(sem, tl(tl(tl(c)))));
    return C3(K("core"), burp(m, hd(g)), coil);
  }
  if (tagis(sut, "face")) return C3(K("face"), hd(g), burp(m, tl(g)));
  if (tagis(sut, "fork")) {
    nouns xs = {0};
    settap(g, &xs, p->a);
    noun s = nul;
    for (size i = 0; i < xs.len; i++) s = setput(p->a, s, burp(m, xs.data[i]));
    return C2(K("fork"), s);
  }
  if (tagis(sut, "hint")) {
    return thint(u, C2(burp(m, hd(hd(g))), tl(hd(g))), burp(m, tl(g)));
  }
  if (tagis(sut, "hold")) return C3(K("hold"), burp(m, hd(g)), tl(g));
  return sut;
}

// the subject as a seminoun, as +bran: what's known of it at compile
// time; 0 on a crash
//
// A type is a graph, with the kernel's under every core in it, and
// walked as a tree it's walked many times over: hoon memoizes +bran, by
// the type and the holds it's under, gil. Here the +bran of a type is
// kept where the holds it was under made no difference to it, none of
// them being met again beneath it, along with the holds it did meet,
// hashed to a bit each in sig: it's the same under any holds that none
// of those is among, so under any whose bits miss sig.

typedef struct {
  noun sut;
  noun res;
  u64  sig;
} brancell;

struct {
  brancell *data;
  size      cap;
  size      len;
} branmemo;

size branslot(noun sut) {
  size mask = branmemo.cap - 1;
  size j = (size)handlehash(sut) & mask;
  while (branmemo.data[j].sut && !nouneq(branmemo.data[j].sut, sut)) j = (j + 1) & mask;
  return j;
}

void branput(noun sut, noun res, u64 sig) {
  if (branmemo.len*4 >= branmemo.cap*3) {
    brancell *old = branmemo.data;
    size oldcap = branmemo.cap;
    branmemo.cap = oldcap ? oldcap * 2 : 1 << 12;
    branmemo.data = new(&H.perm, brancell, branmemo.cap);
    for (size i = 0; i < oldcap; i++) {
      if (old[i].sut) branmemo.data[branslot(old[i].sut)] = old[i];
    }
    if (oldcap) osrelease((byte*)old, (byte*)(old + oldcap));
  }
  size j = branslot(sut);
  if (!branmemo.data[j].sut) branmemo.len++;
  branmemo.data[j] = (brancell){sut, res, sig};
}

void branclear(void) {
  if (branmemo.cap) osrelease((byte*)branmemo.data, (byte*)(branmemo.data + branmemo.cap));
  branmemo.data = 0;
  branmemo.cap = branmemo.len = 0;
}

u64 branbit(noun hold) {
  return 1ull << (handlehash(hold) >> 26);
}

// len is how many holds are in gil and gsig their bits; *low is lowered
// to the place in gil, counted from its far end, of the outermost hold
// met again, and the bits of the holds met are added to *seen
noun branx(minter *m, noun sut, guard *gil, i32 len, u64 gsig, i32 *low, u64 *seen) {
  PROF(bran);
  MP;
  noun whole = sut, r;
  i32 len0 = len, mylow = 0x7fffffff;
  u64 mysig = 0;
  if (branmemo.cap) {
    brancell *c = &branmemo.data[branslot(whole)];
    if (c->sut && !(c->sig & gsig)) {
      *seen |= c->sig;
      return c->res;
    }
  }
  for (;;) {
    if (isnountype(sut) || isvoid(sut) || tagis(sut, "fork")) {
      r = semblock(p);
      break;
    }
    if (tagis(sut, "atom")) {
      noun q = tl(tl(sut));
      r = isatom(q) ? semblock(p) : semfull(p, tl(q));
      break;
    }
    if (tagis(sut, "cell")) {
      noun h = branx(m, hd(tl(sut)), gil, len, gsig, &mylow, &mysig);
      noun t = h ? branx(m, tl(tl(sut)), gil, len, gsig, &mylow, &mysig) : 0;
      r = t ? combine(p, h, t) : 0;
      break;
    }
    if (tagis(sut, "core")) {
      noun t = branx(m, hd(tl(sut)), gil, len, gsig, &mylow, &mysig);
      r = t ? combine(p, hd(tl(tl(coreof(sut)))), t) : 0;
      break;
    }
    if (tagis(sut, "hold")) {
      mysig |= branbit(sut);
      i32 at = len - 1;
      guard *g = gil;
      for (; g; g = g->next, at--) {
        if (!g->b && !g->c && nouneq(g->a, sut)) break;
      }
      if (g) {
        if (at < mylow) mylow = at;
        r = semblock(p);
        break;
      }
      gil = guardput(p->a, gil, sut, 0, 0);
      gsig |= branbit(sut);
      len++;
    }
    if (!(sut = repo(u, sut))) {
      r = 0;
      break;
    }
  }
  // no hold met again was one this was under
  if (r && mylow >= len0 && !loose.on) branput(whole, r, mysig);
  if (mylow < *low) *low = mylow;
  *seen |= mysig;
  return r;
}

noun bran(minter *m, noun sut) {
  i32 low = 0x7fffffff;
  u64 seen = 0;
  return branx(m, sut, 0, 0, 0, &low, &seen);
}

// Tests of a noun against a type, as +fish

noun fishx(minter *m, noun sut, noun axe, guard *vot) {
  PROF(fish);
  MP;
  for (;;) {
    if (isvoid(sut)) return C2(D(1), NO);
    if (isnountype(sut)) return C2(D(1), YES);
    if (tagis(sut, "atom")) {
      noun q = tl(tl(sut));
      if (isatom(q)) return flip(p, C3(D(3), D(0), axe));
      return C3(D(5), C2(D(1), tl(q)), C2(D(0), axe));
    }
    if (tagis(sut, "cell")) {
      noun a = fishx(m, hd(tl(sut)), peg(p, axe, D(2)), vot);
      noun b = a ? fishx(m, tl(tl(sut)), peg(p, axe, D(3)), vot) : 0;
      return b ? flan(p, C3(D(3), D(0), axe), flan(p, a, b)) : 0;
    }
    if (tagis(sut, "core")) return 0;
    if (tagis(sut, "face") || tagis(sut, "hint")) {
      sut = tl(tl(sut));
      continue;
    }
    if (tagis(sut, "fork")) {
      nouns yed = {0};
      settap(tl(sut), &yed, p->a);
      noun r = C2(D(1), NO);
      for (size i = yed.len - 1; i >= 0; i--) {
        noun f = fishx(m, yed.data[i], axe, vot);
        if (!f) return 0;
        r = flor(p, f, r);
      }
      return r;
    }
    if (tagis(sut, "hold")) {
      if (guardhas(vot, sut, 0, 0)) return 0;
      vot = guardput(p->a, vot, sut, 0, 0);
      if (!(sut = repo(u, sut))) return 0;
      continue;
    }
    return 0;
  }
}

noun fish(minter *m, noun sut, noun axe) {
  return fishx(m, sut, axe, 0);
}

b32 nests(minter *m, noun sut, noun ref, b32 *ok) {
  return nest(&m->u, sut, 0, ref, ok);
}

noun fend(minter *m, noun sut, noun hyp, noun *axe);

// a test of ref at axis for a skin, as +fish:ar
noun arfish(minter *m, noun sut, noun ref, noun skin, noun axis) {
  PROF(arfish);
  MP;
  b32 ok;
  for (;;) {
    skin = skinspec(u, skin);
    noun s = tl(skin);
    if (tagis(skin, "base")) {
      if (isatom(s)) {
        if (atomeqc(s, "cell")) {
          skin = C3(K("cell"), C2(K("base"), K("noun")), C2(K("base"), K("noun")));
          continue;
        }
        if (atomeqc(s, "flag")) {
          if (!nests(m, tbool2(m), ref, &ok)) return 0;
          if (ok) return C2(D(1), YES);
          noun a = arfish(m, sut, ref, C2(K("base"), C2(K("atom"), nul)), axis);
          if (!a) return 0;
          noun y = C3(D(5), C2(D(0), axis), C2(D(1), YES));
          noun n = C3(D(5), C2(D(0), axis), C2(D(1), NO));
          return flan(p, a, flor(p, y, n));
        }
        if (atomeqc(s, "noun")) return C2(D(1), YES);
        if (atomeqc(s, "null")) {
          skin = C3(K("leaf"), K("n"), nul);
          continue;
        }
        if (atomeqc(s, "void")) return C2(D(1), NO);
        return 0;
      }
      // [%atom aura]
      if (!nests(m, C3(K("atom"), nul, nul), ref, &ok)) return 0;
      if (ok) return C2(D(1), YES);
      if (!nests(m, C3(K("cell"), K("noun"), K("noun")), ref, &ok)) return 0;
      if (ok) return C2(D(1), NO);
      return flip(p, C3(D(3), D(0), axis));
    }
    if (tagis(skin, "cell")) {
      if (!nests(m, C3(K("atom"), nul, nul), ref, &ok)) return 0;
      if (ok) return C2(D(1), NO);
      if (!nests(m, C3(K("cell"), K("noun"), K("noun")), ref, &ok)) return 0;
      noun a = ok ? C2(D(1), YES) : C3(D(3), D(0), axis);
      noun r2 = tpeek(u, ref, "free", D(2));
      noun b = r2 ? arfish(m, sut, r2, hd(s), peg(p, axis, D(2))) : 0;
      noun r3 = b ? tpeek(u, ref, "free", D(3)) : 0;
      noun c = r3 ? arfish(m, sut, r3, tl(s), peg(p, axis, D(3))) : 0;
      return c ? flan(p, a, flan(p, b, c)) : 0;
    }
    if (tagis(skin, "leaf")) {
      if (!nests(m, C3(K("atom"), nul, C2(nul, tl(s))), ref, &ok)) return 0;
      if (ok) return C2(D(1), YES);
      return C3(D(5), C2(D(1), tl(s)), C2(D(0), axis));
    }
    if (tagis(skin, "dbug") || tagis(skin, "name")) {
      skin = tl(s);
      continue;
    }
    if (tagis(skin, "over")) {
      noun axe;
      noun t = fend(m, sut, hd(s), &axe);
      if (!t) return 0;
      sut = t;
      axis = peg(p, axis, axe);
      skin = tl(s);
      continue;
    }
    if (tagis(skin, "spec")) {
      noun e = hoonexample(p, hd(s));
      noun hit = e ? play(u, sut, e) : 0;
      if (!hit || !nest(u, hit, 1, ref, &ok)) return 0;
      skin = tl(s);
      continue;
    }
    if (tagis(skin, "wash")) return C2(D(1), YES);
    return 0;
  }
}

// Wing lookup that makes formulas: +fond, +find, +fine and +fire as
// +mint has them. Ponies are as in +fond for +play above, but a
// synthetic match carries a formula with its type, in fol.

noun mfire(minter *m, noun sut, noun hag, b32 vet);
pony mfund(minter *m, noun sut, char *way, noun gen);

// [type nock] for a found leg or arm, or an alias, as +fine; 0 on crash
noun mfine(minter *m, noun sut, pony tor) {
  MP;
  if (tor.how == palias) return C2(tor.type, tor.fol);
  noun axe = tend(p, tor.vein);
  if (!tor.set) return C3(tor.type, D(0), axe);
  nouns xs = {0};
  settap(tor.set, &xs, p->a);
  noun t = mfire(m, sut, nounslist(p, &xs), m->vet);
  return t ? C2(t, C3(D(9), tor.axe, C2(D(0), axe))) : 0;
}

// the search loop of +fond for one limb
pony mfondlimb(minter *m, noun sut, char *way, noun cnt, noun nam, noun axe,
               noun lon, guard *gil) {
  PROF(mfondlimb);
  MP;
  #define HERE (atomis(cnt, 0) ? ponyleg(C2(nul, C2(C2(nul, axe), lon)), sut) \
                               : ponyskip(atomsub(p->a, cnt, D(1))))
  #define LOSE ponyskip(cnt)
  #define NONE ((pony){.how = pnone})
  #define FAIL return (pony){0}
  for (;;) {
    if (isvoid(sut)) return NONE;
    if (isnountype(sut) || tagis(sut, "atom")) return isatom(nam) ? HERE : LOSE;
    if (tagis(sut, "cell")) {
      if (isatom(nam)) return HERE;
      pony taf = mfondlimb(m, hd(tl(sut)), way, cnt, nam, peg(p, axe, D(2)), lon, gil);
      if (taf.how != pskip) return taf;
      cnt = taf.cnt;
      axe = peg(p, axe, D(3));
      sut = tl(tl(sut));
      continue;
    }
    if (tagis(sut, "core")) {
      if (isatom(nam)) return HERE;
      noun arm = 0;
      noun zem = loot(u, tl(nam), coilbat(coreof(sut)), &arm);
      if (zem) {
        if (!atomis(cnt, 0)) {
          zem = 0;
          cnt = atomsub(p->a, cnt, D(1));
        }
      }
      if (zem) {
        noun zut = C2(garbpoly(coilgarb(coreof(sut))), arm);
        noun set = mapnode(p->a, C2(sut, zut), nul, nul);
        return (pony){.how = pfound, .vein = C2(C2(nul, axe), lon), .axe = peg(p, D(2), zem),
                      .set = set};
      }
      b32 sam, con;
      peel(way, garbvair(coilgarb(coreof(sut))), &sam, &con);
      if (!sam) return LOSE;
      if (con) {
        sut = hd(tl(sut));
        axe = peg(p, axe, D(3));
      } else {
        if (!(sut = tpeek(u, hd(tl(sut)), way, D(2)))) FAIL;
        axe = peg(p, axe, D(6));
      }
      continue;
    }
    if (tagis(sut, "hint")) {
      if (!(sut = repo(u, sut))) FAIL;
      continue;
    }
    if (tagis(sut, "face")) {
      noun inner = tl(tl(sut));
      if (isatom(nam)) {
        sut = inner;
        return HERE;
      }
      noun zot = hd(tl(sut));
      if (isatom(zot)) {
        if (nouneq(tl(nam), zot)) {
          sut = inner;
          return HERE;
        }
        return LOSE;
      }
      // a tune: aliases and bridges
      noun tyr = mapget(hd(zot), tl(nam));
      b32 next = !tyr;
      if (tyr && isatom(tyr)) {
        sut = inner;
        lon = C2(nul, lon);
        cnt = atomaddsmall(p->a, cnt, 1);
        continue;
      }
      if (tyr && !atomis(cnt, 0)) {
        cnt = atomsub(p->a, cnt, D(1));
        next = 1;
      }
      if (!next) {
        pony tor = mfund(m, sut, way, tl(tyr));
        if (tor.how == pcrash) FAIL;
        if (tor.how == pfound) {
          tor.vein = weld(p, tor.vein, C2(nul, C2(C2(nul, axe), lon)));
          return tor;
        }
        return ponyalias(tor.type, comb(p, C2(D(0), axe), tor.fol));
      }
      // next: search the bridges
      for (noun q = tl(zot); ; q = tl(q)) {
        if (isatom(q)) {
          sut = inner;
          lon = C2(nul, lon);
          break;
        }
        noun tiv = mint(m, inner, K("noun"), hd(q));
        if (!tiv) FAIL;
        pony fid = mfondlimb(m, hd(tiv), way, cnt, nam, D(1), nul, 0);
        if (fid.how == pcrash || fid.how == pnone) return fid;
        if (fid.how == pskip) {
          cnt = fid.cnt;
          continue;
        }
        noun vat = mfine(m, sut, fid);
        if (!vat) FAIL;
        noun fol = comb(p, comb(p, C2(D(0), axe), tl(tiv)), tl(vat));
        return ponyalias(hd(vat), fol);
      }
      continue;
    }
    if (tagis(sut, "fork")) {
      nouns xs = {0};
      settap(tl(sut), &xs, p->a);
      if (!xs.len) return NONE;
      pony acc = {0};
      for (size i = xs.len - 1; i >= 0; i--) {
        pony r = mfondlimb(m, xs.data[i], way, cnt, nam, axe, lon, gil);
        if (r.how == pcrash) FAIL;
        if (i == xs.len - 1) {
          acc = r;
          continue;
        }
        acc = twin(u, r, acc);
        if (acc.how == pcrash) FAIL;
      }
      return acc;
    }
    if (tagis(sut, "hold")) {
      if (guardhas(gil, sut, 0, 0)) return NONE;
      gil = guardput(p->a, gil, sut, 0, 0);
      if (!(sut = repo(u, sut))) FAIL;
      continue;
    }
    FAIL;
  }
  #undef HERE
  #undef LOSE
  #undef NONE
  #undef FAIL
}

// find a wing, as +fond
pony mfond(minter *m, noun sut, char *way, noun hyp) {
  PROF(mfond);
  MP;
  if (isatom(hyp)) return ponyleg(nul, sut);
  pony mor = mfond(m, sut, way, tl(hyp));
  noun i = hd(hyp);
  if (mor.how == pcrash || mor.how == pnone || mor.how == pskip) return mor;
  if (mor.how == palias) {
    noun fex = mint(m, mor.type, K("noun"), C2(K("wing"), C2(i, nul)));
    return fex ? ponyalias(hd(fex), comb(p, mor.fol, tl(fex))) : (pony){0};
  }
  noun lon = mor.vein;
  noun s = mor.set ? tforkheads(u, mor.set) : mor.type;
  if (iscell(i) && atomis(hd(i), 0)) {
    noun t = tpeek(u, s, way, tl(i));
    return t ? ponyleg(C2(C2(nul, tl(i)), lon), t) : (pony){0};
  }
  noun cnt = iscell(i) ? hd(tl(i)) : nul;
  noun nam = iscell(i) ? tl(tl(i)) : C2(nul, i);
  return mfondlimb(m, s, way, cnt, nam, D(1), lon, 0);
}

// as +find: found, or an alias; crash if not found
pony mfind(minter *m, noun sut, char *way, noun hyp) {
  pony r = mfond(m, sut, way, hyp);
  if (r.how == pnone || r.how == pskip) {
    errnew(errfind);
    hcerr.hyp = hyp;
    return (pony){0};
  }
  return r;
}

pony mfund(minter *m, noun sut, char *way, noun gen) {
  MP;
  noun hup = reek(p, gen);
  if (!hup) {
    noun r = mint(m, sut, K("noun"), gen);
    return r ? ponyalias(hd(r), tl(r)) : (pony){0};
  }
  return mfind(m, sut, way, hup);
}

// the type and axis of a leg, as +fend; 0 on crash
noun fend(minter *m, noun sut, noun hyp, noun *axe) {
  MP;
  pony fid = mfind(m, sut, "read", hyp);
  if (fid.how != pfound || fid.set) return 0;
  *axe = tend(p, fid.vein);
  return fid.type;
}

// the product type of a list of [core foot], as +fire, checking the
// payload against a dry arm's core and a wet arm by +mull when vet
noun mfire(minter *m, noun sut, noun hag, b32 vet) {
  PROF(mfire);
  MP;
  if (iscell(hag) && isatom(tl(hag))) {
    noun foot = tl(hd(hag));
    if (atomeqc(hd(foot), "wet") && nouneq(tl(foot), C2(nul, D(1)))) return hd(hd(hag));
  }
  nouns xs = {0};
  for (; iscell(hag); hag = tl(hag)) {
    noun t = hd(hd(hag));
    noun foot = tl(hd(hag));
    if (!tagis(t, "core")) return 0;
    noun coil = coreof(t);
    noun garb = coilgarb(coil);
    noun dox = C3(K("core"), coilctx(coil), C2(C3(hd(garb), garbpoly(garb), K("gold")), tl(coil)));
    if (atomeqc(hd(foot), "dry")) {
      b32 ok;
      if (vet && !nest(u, coilctx(coil), 1, hd(tl(t)), &ok)) return 0;
      *push(&xs, p->a) = C3(K("hold"), dox, tl(foot));
      continue;
    }
    noun pay = redo(u, hd(tl(t)), coilctx(coil));
    if (!pay) return 0;
    noun core = C3(K("core"), pay, coil);
    if (vet && !guardhas(m->rib, sut, dox, tl(foot))) {
      guard *rib = m->rib;
      m->rib = guardput(p->a, rib, sut, dox, tl(foot));
      noun r = mull(m, core, K("noun"), dox, tl(foot));
      m->rib = rib;
      if (!r) return 0;
    }
    *push(&xs, p->a) = C3(K("hold"), core, tl(foot));
  }
  return tfork(u, xs.data, xs.len);
}

// change the leg at a wing in each core, as +toss; the axis in *axe
noun toss(minter *m, noun hyp, noun mur, noun men, noun *axe) {
  PROF(toss);
  MP;
  nouns xs = {0};
  noun ax = 0;
  for (; iscell(men); men = tl(men)) {
    noun a;
    noun t = tack(u, hd(hd(men)), hyp, mur, &a);
    if (!t) return 0;
    if (ax && !nouneq(ax, a)) return 0;
    ax = a;
    *push(&xs, p->a) = C2(t, tl(hd(men)));
  }
  if (!ax) return 0;
  *axe = ax;
  return nounslist(p, &xs);
}

// a wing with changes, as +ergo
noun ergo(minter *m, noun sut, pony lop, noun rig) {
  PROF(ergo);
  MP;
  noun axe = tend(p, lop.vein);
  noun hej = nul;
  if (!lop.set) {
    noun t = lop.type;
    for (; iscell(rig); rig = tl(rig)) {
      noun zil = mint(m, sut, K("noun"), tl(hd(rig)));
      if (!zil) return 0;
      noun ax;
      if (!(t = tack(u, t, hd(hd(rig)), hd(zil), &ax))) return 0;
      hej = C2(C2(ax, tl(zil)), hej);
    }
    return C2(t, hike(p, axe, hej));
  }
  nouns xs = {0};
  settap(lop.set, &xs, p->a);
  noun hag = nounslist(p, &xs);
  for (; iscell(rig); rig = tl(rig)) {
    noun zil = mint(m, sut, K("noun"), tl(hd(rig)));
    if (!zil) return 0;
    noun ax;
    if (!(hag = toss(m, hd(hd(rig)), hd(zil), hag, &ax))) return 0;
    hej = C2(C2(ax, tl(zil)), hej);
  }
  noun t = mfire(m, sut, hag, m->vet);
  return t ? C2(t, C3(D(9), lop.axe, hike(p, axe, hej))) : 0;
}

// the types of a wing with changes against the subject and dox, as +endo
noun endo(minter *m, noun sut, pony pl, pony ql, noun dox, noun rig) {
  PROF(endo);
  MP;
  if (!pl.set) {
    if (ql.set) return 0;
    noun a = pl.type, b = ql.type;
    for (; iscell(rig); rig = tl(rig)) {
      noun zil = mull(m, sut, K("noun"), dox, tl(hd(rig)));
      if (!zil) return 0;
      noun x, y;
      if (!(a = tack(u, a, hd(hd(rig)), hd(zil), &x))) return 0;
      if (!(b = tack(u, b, hd(hd(rig)), tl(zil), &y))) return 0;
      if (!nouneq(x, y)) return 0;
    }
    return C2(a, b);
  }
  if (!ql.set || !nouneq(pl.axe, ql.axe)) return 0;
  nouns xs = {0}, ys = {0};
  settap(pl.set, &xs, p->a);
  settap(ql.set, &ys, p->a);
  noun ph = nounslist(p, &xs), qh = nounslist(p, &ys);
  for (; iscell(rig); rig = tl(rig)) {
    noun zil = mull(m, sut, K("noun"), dox, tl(hd(rig)));
    if (!zil) return 0;
    noun x, y;
    if (!(ph = toss(m, hd(hd(rig)), hd(zil), ph, &x))) return 0;
    if (!(qh = toss(m, hd(hd(rig)), tl(zil), qh, &y))) return 0;
    if (!nouneq(x, y)) return 0;
  }
  noun a = mfire(m, sut, ph, m->vet);
  noun b = a ? mfire(m, sut, qh, 0) : 0;
  return b ? C2(a, b) : 0;
}

// Cores

// the arms of a battery by axis, as the map +laze builds
void lazechapter(parser *p, noun dab, noun axe, nouns *out) {
  for (;;) {
    if (isatom(dab)) return;
    b32 l = iscell(mapl(dab)), r = iscell(mapr(dab));
    noun gen = tl(mapn(dab));
    if (!l && !r) {
      *push(out, p->a) = C2(axe, gen);
      return;
    }
    if (l && r) {
      *push(out, p->a) = C2(peg(p, axe, D(2)), gen);
      lazechapter(p, mapl(dab), peg(p, axe, D(6)), out);
      dab = mapr(dab);
      axe = peg(p, axe, D(7));
      continue;
    }
    *push(out, p->a) = C2(peg(p, axe, D(2)), gen);
    dab = l ? mapl(dab) : mapr(dab);
    axe = peg(p, axe, D(3));
  }
}

void lazearms(parser *p, noun dom, noun axe, nouns *out) {
  for (;;) {
    if (isatom(dom)) return;
    b32 l = iscell(mapl(dom)), r = iscell(mapr(dom));
    noun dab = tl(mapn(dom));
    if (!l && !r) {
      lazechapter(p, dab, axe, out);
      return;
    }
    lazechapter(p, dab, peg(p, axe, D(2)), out);
    if (l && r) {
      lazearms(p, mapl(dom), peg(p, axe, D(6)), out);
      dom = mapr(dom);
      axe = peg(p, axe, D(7));
      continue;
    }
    dom = l ? mapl(dom) : mapr(dom);
    axe = peg(p, axe, D(3));
  }
}

// The compiling kernel. While a core's arms are compiled, its battery in
// their subject is lazy: in hoon, a gate that +laze makes, closed over
// the +ut door doing the compiling and so over the whole kernel running
// it. A type that holds it can end up in a formula, as .^ puts its type
// in one, and its mug orders forks. With a kernel, from -c, hc makes the
// gate as hoon does, by running that kernel's +laze; without one it puts
// [%laze sut nym hud dom vet fan rib] in its place, which compares equal
// where the gate would.

noun    lazer;      // |=([s f r v n h d] (laze:dor(...) n h d)), or 0
nounmap lazes;      // the gates made, to [sut nym hud dom vet fan rib]
nounmap lazememo;   // the lazy seminouns made, by the same

// a loop guard as the hoon set it stands for, of pairs or triples
noun guardset(parser *p, guard *g, b32 triple) {
  noun s = nul;
  for (; g; g = g->next) s = setput(p->a, s, triple ? C3(g->a, g->b, g->c) : C2(g->a, g->b));
  return s;
}

guard *setguard(parser *p, noun s, b32 triple) {
  nouns xs = {0};
  settap(s, &xs, p->a);
  guard *g = 0;
  for (size i = 0; i < xs.len; i++) {
    noun x = xs.data[i];
    g = triple ? guardput(p->a, g, hd(x), hd(tl(x)), tl(tl(x))) : guardput(p->a, g, hd(x), tl(x), 0);
  }
  return g;
}

// the lazy battery of a core whose arms are about to be compiled, as
// +laze makes it with the +ut door as m has it
noun lazemake(minter *m, noun sut, noun nym, noun hud, noun dom) {
  MP;
  noun fan = guardset(p, m->u.fan, 0), rib = guardset(p, m->rib, 1);
  noun vet = m->vet ? YES : NO;
  noun from = C5(sut, nym, hud, dom, C3(vet, fan, rib));
#ifdef HC_EMBED
  {
    noun r = nounmapget(&lazememo, from);
    if (r) return r;
    noun gate = hostlaze(from);
    r = C2(C3(K("lazy"), D(1), gate), nul);
    nounmapput(&lazememo, from, r);
    nounmapput(&lazes, gate, from);
    return r;
  }
#endif
  if (!lazer) return C2(C3(K("lazy"), D(1), C2(K("laze"), from)), nul);
  noun r = nounmapget(&lazememo, from);
  if (r) return r;
  noun sam = C5(sut, fan, rib, vet, C3(nym, hud, dom));
  noun core = edit(p, D(6), lazer, sam);
  r = core ? nock(p, core, C3(D(9), D(2), C2(D(0), D(1)))) : 0;
  if (!r || isatom(r) || !tagis(hd(r), "lazy")) {
    // the kernel's +laze failed: as without one
    return C2(C3(K("lazy"), D(1), C2(K("laze"), from)), nul);
  }
  nounmapput(&lazememo, from, r);
  nounmapput(&lazes, tl(tl(hd(r))), from);
  return r;
}

// a core type's battery as known at compile time: the battery, or the
// laze while its arms are being compiled; 0 if neither
noun corekey(typer *u, noun typ) {
  for (;;) {
    if (tagis(typ, "face") || tagis(typ, "hint")) {
      typ = tl(tl(typ));
      continue;
    }
    if (tagis(typ, "hold")) {
      if (!(typ = repo(u, typ))) return 0;
      continue;
    }
    break;
  }
  if (!tagis(typ, "core")) return 0;
  noun k = semmask(hd(tl(tl(coreof(typ)))));
  if (tagis(k, "full") && isatom(tl(k))) return semdata(hd(tl(tl(coreof(typ)))));
  if (tagis(k, "lazy")) return tl(tl(k));
  return 0;
}

// record a core compiled under [%fast clue], as fastrecord does for one
// made by nock: the kernel's layers are made at compile time, and a
// gate's parent is a layer still being compiled, known by its laze
void fastmint(minter *m, noun typ, noun clue) {
  MP;
  if (!iscell(clue) || !isatom(hd(clue)) || !iscell(tl(clue)) || !tagis(typ, "core")) return;
  noun c = coreof(typ);
  noun sem = hd(tl(tl(c)));
  if (!(tagis(semmask(sem), "full") && isatom(tl(semmask(sem))))) return;
  noun par = hd(tl(clue));
  noun pk = nul;
  if (iscell(par) && atomis(hd(par), 0) && isatom(tl(par)) && !atomis(tl(par), 0)) {
    noun pt = tpeek(u, typ, "free", tl(par));
    if (!pt || !(pk = corekey(u, pt))) return;
  }
  nounmapput(&fastnames, semdata(sem), hd(clue));
  nounmapput(&fastparents, semdata(sem), pk);
  // as its arms saw it
  noun g = coilgarb(c);
  noun laz = semmask(lazemake(m, hd(tl(typ)), hd(g), garbpoly(g), coilbat(c)));
  nounmapput(&fastnames, tl(tl(laz)), hd(clue));
}

// the core type that arms are compiled against
noun lazecore(minter *m, noun sut, noun nym, noun hud, noun dom) {
  MP;
  noun garb = C3(nym, hud, K("gold"));
  return tcore(u, sut, C3(garb, sut, C2(lazemake(m, sut, nym, hud, dom), dom)));
}

// generate a formula from a foot, as +hemp
noun hemp(minter *m, noun sut, noun hud, noun gol, noun gen) {
  PROF(hemp);
  b32 vet = m->vet;
  if (atomeqc(hud, "wet")) m->vet = 0;
  noun r = mint(m, sut, gol, gen);
  m->vet = vet;
  return r ? tl(r) : 0;
}

// the formula of the arm at a battery axis of a lazy battery, as the
// gate +laze makes does it: with the +ut door as it was then, against
// the core with this same lazy battery; 0 if there's none
noun lazeresolve(minter *m, noun laz, noun axe, b32 *crash) {
  PROF(laze);
  MP;
  noun from = tagis(laz, "laze") ? tl(laz) : nounmapget(&lazes, laz);
#ifdef HC_EMBED
  if (!from && (from = hostfrom(laz))) nounmapput(&lazes, laz, from);
#endif
  if (!from) return 0;
  noun sut = hd(from), nym = hd(tl(from)), hud = hd(tl(tl(from))), dom = hd(tl(tl(tl(from))));
  noun st = tl(tl(tl(tl(from))));
  nouns arms = {0};
  lazearms(p, dom, D(1), &arms);
  for (size i = 0; i < arms.len; i++) {
    if (!nouneq(hd(arms.data[i]), axe)) continue;
    b32 vet = m->vet;
    guard *fan = m->u.fan, *rib = m->rib;
    m->vet = atomis(hd(st), 0);
    m->u.fan = setguard(p, hd(tl(st)), 0);
    m->rib = setguard(p, tl(tl(st)), 1);
    noun sem = C2(C3(K("lazy"), D(1), laz), nul);
    noun core = tcore(u, sut, C3(C3(nym, hud, K("gold")), sut, C2(sem, dom)));
    noun r = hemp(m, core, hud, K("noun"), tl(arms.data[i]));
    m->vet = vet;
    m->u.fan = fan;
    m->rib = rib;
    if (!r) *crash = 1;
    return r;
  }
  return 0;
}

// what a core's arms are checked against, from the type expected of
// the core, as +core-check in +mine: %noun, [%cell ...], a core with
// %gold variance, or a fork of these as a list; 0 on crash
noun corecheck(minter *m, noun log) {
  MP;
  for (;;) {
    if (isnountype(log)) return log;
    if (isvoid(log) || tagis(log, "atom")) return 0;
    if (tagis(log, "cell")) {
      b32 ok;
      return nest(u, hd(tl(log)), 1, K("noun"), &ok) ? log : 0;
    }
    if (tagis(log, "core")) {
      noun c = coreof(log);
      noun g = coilgarb(c);
      return C3(K("core"), hd(tl(log)), C2(C3(hd(g), garbpoly(g), K("gold")), tl(c)));
    }
    if (tagis(log, "fork")) {
      nouns xs = {0};
      settap(tl(log), &xs, p->a);
      for (size i = 0; i < xs.len; i++) {
        if (!(xs.data[i] = corecheck(m, xs.data[i]))) return 0;
      }
      return C2(K("fork"), nounslist(p, &xs));
    }
    if (!(log = repo(u, log))) return 0;
  }
}

size mapwyt(noun m) {
  return isatom(m) ? 0 : 1 + mapwyt(mapl(m)) + mapwyt(mapr(m));
}

// as +chapters-check
b32 chapterscheck(noun log, noun dom) {
  if (tagis(log, "core")) return mapwyt(dom) == mapwyt(coilbat(coreof(log)));
  if (tagis(log, "fork")) {
    for (noun l = tl(log); iscell(l); l = tl(l)) {
      if (!chapterscheck(hd(l), dom)) return 0;
    }
  }
  return 1;
}

typedef struct {
  minter *m;
  noun    sut;    // the core type arms are compiled against
  noun    hud;
  noun    log;    // the expected core
  noun    dog;    // its chapters, or 0
} mineenv;

// the formulas of a chapter's arms, shaped like its map
noun minechapter(mineenv *e, noun dab, noun dag) {
  minter *m = e->m;
  MP;
  if (isatom(dab)) return nul;
  noun n = mapn(dab);
  noun gog = K("noun");
  if (dag) {
    noun gen = mapget(dag, hd(n));
    if (!gen || !(gog = play(u, e->log, gen))) return 0;
  }
  noun vad = hemp(m, e->sut, e->hud, gog, tl(n));
  if (!vad) return 0;
  b32 l = iscell(mapl(dab)), r = iscell(mapr(dab));
  if (!l && !r) return vad;
  noun x = l ? minechapter(e, mapl(dab), dag) : 0;
  if (l && !x) return 0;
  noun y = r ? minechapter(e, mapr(dab), dag) : 0;
  if (r && !y) return 0;
  if (l && r) return C3(vad, x, y);
  return C2(vad, l ? x : y);
}

noun minebattery(mineenv *e, noun dom) {
  minter *m = e->m;
  MP;
  if (isatom(dom)) return nul;
  noun n = mapn(dom);
  noun dab = tl(n);
  noun dag = 0;
  if (e->dog) {
    if (!(dag = mapget(e->dog, hd(n)))) return 0;
    if (mapwyt(dab) != mapwyt(dag)) return 0;
  }
  noun dov = minechapter(e, dab, dag);
  if (!dov) return 0;
  b32 l = iscell(mapl(dom)), r = iscell(mapr(dom));
  if (!l && !r) return dov;
  noun x = l ? minebattery(e, mapl(dom)) : 0;
  if (l && !x) return 0;
  noun y = r ? minebattery(e, mapr(dom)) : 0;
  if (r && !y) return 0;
  if (l && r) return C3(dov, x, y);
  return C2(dov, l ? x : y);
}

// compile the arms of a core, as +mine: [type [%1 battery]]
noun mine(minter *m, noun sut, noun gol, noun mel, noun nym, noun hud, noun dom) {
  PROF(mine);
  MP;
  noun log = corecheck(m, gol);
  if (!log || !chapterscheck(log, dom)) return 0;
  mineenv e = {m, lazecore(m, sut, nym, hud, dom), hud, log, 0};
  if (tagis(log, "core")) e.dog = coilbat(coreof(log));
  noun dez = minebattery(&e, dom);
  if (!dez) return 0;
  noun coil = C3(C3(nym, hud, mel), sut, C2(semfull(p, dez), dom));
  return C2(tcore(u, sut, coil), C2(D(1), dez));
}

// check the dry arms of a core against dox, as +mile: [yet hum]
noun mile(minter *m, noun sut, noun dox, noun nym, noun hud, noun dom) {
  PROF(mile);
  MP;
  noun laz = lazemake(m, sut, nym, hud, dom);
  noun garb = C3(nym, hud, K("gold"));
  noun yet = tcore(u, sut, C3(garb, sut, C2(laz, dom)));
  noun hum = tcore(u, dox, C3(garb, dox, C2(laz, dom)));
  if (atomeqc(hud, "dry")) {
    nouns cs = {0};
    maptap(dom, &cs, p->a);
    for (size i = 0; i < cs.len; i++) {
      nouns as = {0};
      maptap(tl(cs.data[i]), &as, p->a);
      for (size j = 0; j < as.len; j++) {
        if (!mull(m, yet, K("noun"), hum, tl(as.data[j]))) return 0;
      }
    }
  }
  return C2(yet, hum);
}

// Compiling, as +mint: [type nock] for a hoon against a subject, or 0
// where hoon would crash

noun mintx(minter *m, noun sut, noun gol, noun gen) {
  PROF(mint);
  MP;
  for (;;) {
    if (isvoid(sut) && !tagis(gen, "dbug")) {
      if (m->vet && !tagis(gen, "lost") && !tagis(gen, "zpzp")) return 0;
      return C3(K("void"), D(0), D(0));
    }
    noun g = tl(gen);
    if (iscell(hd(gen))) {
      // a tuple: the heads in a loop, then the cells from the end
      nouns hs = {0};
      noun t = gen;
      for (; iscell(t) && iscell(hd(t)); t = tl(t)) {
        noun a = mint(m, sut, K("noun"), hd(t));
        if (!a) return 0;
        *push(&hs, p->a) = a;
      }
      noun r = mint(m, sut, K("noun"), t);
      if (!r) return 0;
      for (size i = hs.len - 1; i >= 0; i--) {
        noun h = hs.data[i];
        r = C2(tcell(u, hd(h), hd(r)), fcons(p, tl(h), tl(r)));
      }
      return nice(m, gol, hd(r)) ? r : 0;
    }
    noun tag = hd(gen);
    u64 tw = atomfits(tag) ? atomlow(tag) : 0;
    #define IS(t) (tw == TW(t))
    #define NICE(t) do { if (!nice(m, gol, (t))) return 0; } while (0)
    if (IS("brcn") || IS("brpt")) {
      noun dan = mint(m, sut, K("noun"), C2(nul, D(1)));
      if (!dan) return 0;
      noun hud = term(IS("brcn") ? "dry" : "wet");
      noun pul = mine(m, sut, gol, K("gold"), hd(g), hud, tl(g));
      if (!pul) return 0;
      NICE(hd(pul));
      return C2(hd(pul), fcons(p, tl(pul), tl(dan)));
    }
    if (IS("cnts")) {
      pony lug = mfind(m, sut, "read", hd(g));
      if (lug.how == pcrash) return 0;
      noun r;
      if (lug.how == palias) {
        if (iscell(tl(g))) return 0;
        r = C2(lug.type, lug.fol);
      } else {
        if (!(r = ergo(m, sut, lug, tl(g)))) return 0;
      }
      NICE(hd(r));
      return r;
    }
    if (IS("dtkt")) {
      noun nef = mint(m, sut, gol, C2(K("kttr"), hd(g)));
      noun q = nef ? mint(m, sut, K("noun"), tl(g)) : 0;
      if (!q) return 0;
      return C2(hd(nef), C3(D(12), C3(D(1), D(HOON_VERSION), hd(nef)), tl(q)));
    }
    if (IS("dtls")) {
      noun a = C3(K("atom"), nul, nul);
      NICE(a);
      noun q = mint(m, sut, a, g);
      return q ? C2(a, C2(D(4), tl(q))) : 0;
    }
    if (IS("sand") || IS("rock")) {
      noun t = play(u, sut, gen);
      if (!t) return 0;
      NICE(t);
      return C2(t, C2(D(1), tl(g)));
    }
    if (IS("dttr") || IS("dtts")) {
      noun t = IS("dttr") ? K("noun") : tbool2(m);
      NICE(t);
      noun a = mint(m, sut, K("noun"), hd(g));
      noun b = a ? mint(m, sut, K("noun"), tl(g)) : 0;
      return b ? C2(t, C3(D(IS("dttr") ? 2 : 5), tl(a), tl(b))) : 0;
    }
    if (IS("dtwt")) {
      noun t = tbool2(m);
      NICE(t);
      noun a = mint(m, sut, K("noun"), g);
      return a ? C2(t, C2(D(3), tl(a))) : 0;
    }
    if (IS("hand")) return g;
    if (IS("ktbr") || IS("ktpm") || IS("ktwt")) {
      noun vat = mint(m, sut, gol, g);
      noun t = vat ? wrap(u, hd(vat), IS("ktbr") ? "iron" : IS("ktpm") ? "zinc" : "lead") : 0;
      if (!t) return 0;
      NICE(t);
      return C2(t, tl(vat));
    }
    if (IS("ktls") || IS("ktcb")) {
      noun hif = play(u, sut, hd(g));
      if (!hif) return 0;
      NICE(hif);
      if (IS("ktcb")) {
        gol = hif;
        gen = tl(g);
        continue;
      }
      noun q = mint(m, sut, hif, tl(g));
      return q ? C2(hif, tl(q)) : 0;
    }
    if (IS("ktsg")) {
      // as +blow: the value, if it can be computed now
      noun pro = mint(m, sut, gol, g);
      if (!pro) return 0;
      noun bus = bran(m, sut);
      noun jon = bus ? araw(m, bus, tl(pro)) : 0;
      if (!jon) return 0;
      if (isatom(jon)) return pro;
      noun b = squash(m, semmask(jon));
      if (!b) return 0;
      if (iscell(b)) return pro;
      return C2(hd(pro), C2(D(1), semdata(jon)));
    }
    if (IS("tune")) return C2(tface(u, g, sut), C2(D(0), D(1)));
    if (IS("note")) {
      noun hum = mint(m, sut, gol, tl(g));
      return hum ? C2(thint(u, C2(sut, hd(g)), hd(hum)), tl(hum)) : 0;
    }
    if (IS("sgzp")) {
      gen = tl(g);
      continue;
    }
    if (IS("sggr")) {
      noun hum = mint(m, sut, gol, tl(g));
      if (!hum) return 0;
      noun hin = hd(g);
      if (iscell(hin)) {
        noun q = mint(m, sut, K("noun"), tl(hin));
        if (!q) return 0;
        if (atomeqc(hd(hin), "fast") && isop(tl(q), 1)) fastmint(m, hd(hum), tl(tl(q)));
        hin = C2(hd(hin), tl(q));
      }
      return C2(hd(hum), C3(D(11), hin, tl(hum)));
    }
    if (IS("tsgr")) {
      noun fid = mint(m, sut, K("noun"), hd(g));
      noun dov = fid ? mint(m, hd(fid), gol, tl(g)) : 0;
      return dov ? C2(hd(dov), comb(p, tl(fid), tl(dov))) : 0;
    }
    if (IS("tscm")) {
      sut = busk(m, sut, hd(g));
      gen = tl(g);
      continue;
    }
    if (IS("wtcl")) {
      noun nor = mint(m, sut, tbool2(m), hd(g));
      if (!nor) return 0;
      noun fex = chip(u, sut, 1, hd(g));
      noun wux = fex ? chip(u, sut, 0, hd(g)) : 0;
      if (!wux) return 0;
      b32 ned;
      noun duy;
      if (isvoid(fex) && isvoid(wux)) {
        ned = 0;
        duy = C2(D(0), D(0));
      } else if (isvoid(fex)) {
        ned = 1;
        duy = C2(D(1), NO);
      } else if (isvoid(wux)) {
        ned = 1;
        duy = C2(D(1), YES);
      } else {
        ned = 0;
        duy = tl(nor);
      }
      noun hiq = mint(m, fex, gol, hd(tl(g)));
      noun ran = hiq ? mint(m, wux, gol, tl(tl(g))) : 0;
      if (!ran) return 0;
      noun fol = cond(p, duy, tl(hiq), tl(ran));
      if (ned) fol = C3(D(11), C2(K("toss"), tl(nor)), fol);
      return C2(tfork2(u, hd(hiq), hd(ran)), fol);
    }
    if (IS("wthx")) {
      noun t = tbool2(m);
      NICE(t);
      noun axe;
      noun typ = fend(m, sut, C2(C2(YES, D(1)), tl(g)), &axe);
      noun f = typ ? arfish(m, sut, typ, hd(g), axe) : 0;
      return f ? C2(t, f) : 0;
    }
    if (IS("fits")) {
      noun t = tbool2(m);
      NICE(t);
      noun ref = play(u, sut, hd(g));
      pony fid = ref ? mfind(m, sut, "read", tl(g)) : (pony){0};
      if (fid.how == pcrash) return 0;
      if (fid.how == pfound && !fid.set) {
        noun f = fish(m, ref, tend(p, fid.vein));
        return f ? C2(t, f) : 0;
      }
      noun pq = mfine(m, sut, fid);
      if (!pq) return 0;
      noun f = fish(m, ref, D(1));
      return f ? C2(t, C3(D(7), tl(pq), f)) : 0;
    }
    if (IS("dbug")) {
      noun hum = mint(m, sut, gol, tl(g));
      if (!hum) {
        errspot(hd(g));
        return 0;
      }
      return C2(hd(hum), C3(D(11), C3(K("spot"), D(1), hd(g)), tl(hum)));
    }
    if (IS("zpcm")) {
      noun t = play(u, sut, hd(g));
      if (!t) return 0;
      NICE(t);
      return C2(t, C2(D(1), tl(g)));
    }
    if (IS("lost")) {
      if (m->vet) return 0;
      return C3(K("void"), D(0), D(0));
    }
    if (IS("zpmc")) {
      noun vos = mint(m, sut, K("noun"), tl(g));
      noun ref = vos ? mint(m, sut, K("noun"), hd(g)) : 0;
      if (!ref) return 0;
      noun t = tcell(u, hd(ref), hd(vos));
      NICE(t);
      return C2(t, fcons(p, C2(D(1), burp(m, hd(vos))), tl(vos)));
    }
    if (IS("zpgl")) {
      noun typ = play(u, sut, C2(K("kttr"), hd(g)));
      if (!typ) return 0;
      NICE(typ);
      noun s2 = C2(nul, D(2)), s3 = C2(nul, D(3));
      noun a = C3(K("tsgr"), C2(K("zpgr"), C2(K("kttr"), hd(g))), s2);
      noun b = C3(K("tsgr"), tl(g), s2);
      noun tst = C3(K("cncl"), C2(K("limb"), K("levi")), C2(a, C2(b, nul)));
      noun val = mint(m, sut, K("noun"), C4(K("wtcl"), tst, C3(K("tsgr"), tl(g), s3), C2(K("zpzp"), nul)));
      return val ? C2(typ, tl(val)) : 0;
    }
    if (IS("zpts")) {
      NICE(K("noun"));
      b32 vet = m->vet;
      m->vet = 0;
      noun q = mint(m, sut, gol, g);
      m->vet = vet;
      return q ? C2(K("noun"), C2(D(1), tl(q))) : 0;
    }
    if (IS("zppt")) {
      b32 ok;
      if (!feel(u, sut, hd(g), &ok)) return 0;
      gen = ok ? tl(tl(g)) : hd(tl(g));
      continue;
    }
    if (IS("zpzp") && isatom(g)) return C3(K("void"), D(0), D(0));
    #undef IS
    #undef NICE
    noun doz = hoonopen(p, gen);
    if (!doz || nouneq(doz, gen)) return 0;
    gen = doz;
  }
}

// What lasts is in the heap, so the scratch a call uses is given back
// when it returns.
// Interrupting. With interrupted set, +mint and +mull ask it about once a
// millisecond whether to stop, and if so fail, and keep failing until
// aborted is cleared: the language server compiles while it has nothing
// else to do, and stops when a request comes. A failure from stopping
// is not an answer, so nothing is kept from it: memos keep only
// successes, and ford keeps nothing built while aborted.

b32 (*interrupted)(void);
b32 aborted;
u64 nextpoll;

b32 stopping(void) {
  if (aborted) return 1;
  u64 now = cycles();
  if (now < nextpoll) return 0;
  u64 hz = cyclefreq();
  nextpoll = now + (hz ? hz / 1000 : 1000000);
  return aborted = interrupted();
}

noun mint(minter *m, noun sut, noun gol, noun gen) {
  if (interrupted && stopping()) return 0;
  if (stackdeep()) return 0;
  u32 kind = memo_mint + !m->vet;
  size j = memofind(sut, gol, 0, gen, kind);
  if (memo.data[j].sut) return memo.data[j].res;
  byte *mark = m->u.p->a->beg;
  noun r = mintx(m, sut, gol, gen);
  m->u.p->a->beg = mark;
  if (r) memoput(sut, gol, 0, gen, kind, r);
#ifdef HCTRACE
  if (!r) {
    bufout e[1] = {{(u8[4096]){0}, 0, 4096, 2, 0}};
    append(e, S("mint crash: "));
    appendnoun(e, gen);
    append(e, S("\n"));
    flush(e);
  }
#endif
  return r;
}

// Checking, as +mull: the types of a hoon against the subject and
// against dox, the subject a wet arm was written for; 0 where hoon
// would crash

noun mullx(minter *m, noun sut, noun gol, noun dox, noun gen, noun *spot) {
  PROF(mull);
  MP;
  for (;;) {
    if (isvoid(sut)) return 0;
    noun g = tl(gen);
    #define NICE(t) do { if (!nice(m, gol, (t))) return 0; } while (0)
    #define BETH(t) do { noun t_ = (t); if (!t_) return 0; NICE(t_); return C2(t_, t_); } while (0)
    if (iscell(hd(gen))) {
      noun hed = mull(m, sut, K("noun"), dox, hd(gen));
      noun tal = hed ? mull(m, sut, K("noun"), dox, g) : 0;
      if (!tal) return 0;
      noun t = tcell(u, hd(hed), hd(tal));
      NICE(t);
      return C2(t, tcell(u, tl(hed), tl(tal)));
    }
    noun tag = hd(gen);
    u64 tw = atomfits(tag) ? atomlow(tag) : 0;
    #define IS(t) (tw == TW(t))
    if (IS("brcn") || IS("brpt")) {
      noun dan = mull(m, sut, K("noun"), dox, C2(nul, D(1)));
      if (!dan) return 0;
      noun hud = term(IS("brcn") ? "dry" : "wet");
      noun yaz = mile(m, hd(dan), tl(dan), hd(g), hud, tl(g));
      if (!yaz) return 0;
      NICE(hd(yaz));
      return yaz;
    }
    if (IS("cnts")) {
      noun hyp = hd(g), rig = tl(g);
      pony pl = mfind(m, sut, "read", hyp);
      pony ql = pl.how ? mfind(m, dox, "read", hyp) : (pony){0};
      if (ql.how == pcrash) return 0;
      noun r;
      if (pl.how == palias) {
        if (ql.how != palias || iscell(rig)) return 0;
        r = C2(pl.type, ql.type);
      } else {
        if (ql.how != pfound) return 0;
        if (!(r = endo(m, sut, pl, ql, dox, rig))) return 0;
      }
      NICE(hd(r));
      return r;
    }
    if (IS("dtkt")) {
      if (!mull(m, sut, K("noun"), dox, tl(g))) return 0;
      gen = C2(K("kttr"), hd(g));
      continue;
    }
    if (IS("dtls")) {
      noun a = C3(K("atom"), nul, nul);
      if (!mull(m, sut, a, dox, g)) return 0;
      BETH(a);
    }
    if (IS("sand") || IS("rock")) BETH(play(u, sut, gen));
    if (IS("dttr") || IS("dtts")) {
      if (!mull(m, sut, K("noun"), dox, hd(g))) return 0;
      if (!mull(m, sut, K("noun"), dox, tl(g))) return 0;
      BETH(IS("dttr") ? K("noun") : tbool2(m));
    }
    if (IS("dtwt")) {
      if (!mull(m, sut, K("noun"), dox, g)) return 0;
      BETH(tbool2(m));
    }
    if (IS("hand")) return C2(hd(g), hd(g));
    if (IS("ktbr") || IS("ktpm") || IS("ktwt")) {
      char *yoz = IS("ktbr") ? "iron" : IS("ktpm") ? "zinc" : "lead";
      noun vat = mull(m, sut, gol, dox, g);
      noun a = vat ? wrap(u, hd(vat), yoz) : 0;
      noun b = a ? wrap(u, tl(vat), yoz) : 0;
      return b ? C2(a, b) : 0;
    }
    if (IS("ktls") || IS("ktcb")) {
      noun a = play(u, sut, hd(g));
      if (!a) return 0;
      NICE(a);
      noun b = play(u, dox, hd(g));
      if (!b) return 0;
      if (IS("ktcb")) {
        gol = a;
        gen = tl(g);
        continue;
      }
      return mull(m, sut, a, dox, tl(g)) ? C2(a, b) : 0;
    }
    if (IS("tune")) return C2(tface(u, g, sut), tface(u, g, dox));
    if (IS("note")) {
      noun vat = mull(m, sut, gol, dox, tl(g));
      if (!vat) return 0;
      return C2(thint(u, C2(sut, hd(g)), hd(vat)), thint(u, C2(dox, hd(g)), tl(vat)));
    }
    if (IS("ktsg")) {
      gen = g;
      continue;
    }
    if (IS("sgzp") || IS("sggr") || IS("dbug")) {
      if (IS("dbug")) *spot = hd(g);
      gen = tl(g);
      continue;
    }
    if (IS("tsgr")) {
      noun lem = mull(m, sut, K("noun"), dox, hd(g));
      if (!lem) return 0;
      sut = hd(lem);
      dox = tl(lem);
      gen = tl(g);
      continue;
    }
    if (IS("tscm")) {
      sut = busk(m, sut, hd(g));
      dox = busk(m, dox, hd(g));
      gen = tl(g);
      continue;
    }
    if (IS("wtcl")) {
      if (!mull(m, sut, tbool2(m), dox, hd(g))) return 0;
      noun res[2][2];
      for (i32 k = 0; k < 2; k++) {
        noun a = chip(u, sut, !k, hd(g));
        noun b = a ? chip(u, dox, !k, hd(g)) : 0;
        if (!b) return 0;
        noun br = k ? tl(tl(g)) : hd(tl(g));
        if (isvoid(a)) {
          res[k][0] = K("void");
          res[k][1] = isvoid(b) ? K("void") : play(u, b, br);
          if (!res[k][1]) return 0;
          continue;
        }
        if (isvoid(b)) return 0;
        noun r = mull(m, a, gol, b, br);
        if (!r) return 0;
        res[k][0] = hd(r);
        res[k][1] = tl(r);
      }
      noun t = tfork2(u, res[0][0], res[1][0]);
      NICE(t);
      return C2(t, tfork2(u, res[0][1], res[1][1]));
    }
    if (IS("fits")) {
      noun pw = play(u, sut, hd(g));
      noun qw = pw ? play(u, dox, hd(g)) : 0;
      if (!qw) return 0;
      noun wing = C2(K("wing"), tl(g));
      noun ps = mint(m, sut, K("noun"), wing);
      noun qs = ps ? mint(m, dox, K("noun"), wing) : 0;
      if (!qs) return 0;
      noun pa = cove(tl(ps)), qa = cove(tl(qs));
      if (!pa || !qa) return 0;
      noun pf = fish(m, pw, pa);
      noun qf = pf ? fish(m, qw, qa) : 0;
      if (!qf || !nouneq(pa, qa) || !nouneq(pf, qf)) return 0;
      BETH(tbool2(m));
    }
    if (IS("wthx")) {
      noun hyp = C2(C2(YES, D(1)), tl(g));
      noun na, oa;
      noun nt = fend(m, sut, hyp, &na);
      noun ot = nt ? fend(m, dox, hyp, &oa) : 0;
      if (!ot || !nouneq(na, oa)) return 0;
      b32 ok;
      if (!nest(u, ot, 1, nt, &ok)) return 0;
      BETH(tbool2(m));
    }
    if (IS("zpcm")) {
      noun a = play(u, sut, hd(g));
      if (!a) return 0;
      NICE(a);
      noun b = play(u, dox, hd(g));
      return b ? C2(a, b) : 0;
    }
    if (IS("lost")) {
      if (m->vet) return 0;
      BETH(K("void"));
    }
    if (IS("zpts")) BETH(K("noun"));
    if (IS("zpmc")) {
      noun vos = mull(m, sut, K("noun"), dox, tl(g));
      noun a = vos ? play(u, sut, hd(g)) : 0;
      if (!a) return 0;
      noun t = tcell(u, a, hd(vos));
      NICE(t);
      noun b = play(u, dox, hd(g));
      return b ? C2(t, tcell(u, b, tl(vos))) : 0;
    }
    if (IS("zpgl")) BETH(play(u, sut, C2(K("kttr"), hd(g))));
    if (IS("zppt")) {
      b32 a, b;
      if (!feel(u, sut, hd(g), &a) || !feel(u, dox, hd(g), &b) || a != b) return 0;
      gen = a ? tl(tl(g)) : hd(tl(g));
      continue;
    }
    if (IS("zpzp")) BETH(K("void"));
    #undef IS
    #undef NICE
    #undef BETH
    noun doz = hoonopen(p, gen);
    if (!doz || nouneq(doz, gen)) return 0;
    gen = doz;
  }
}

noun mull(minter *m, noun sut, noun gol, noun dox, noun gen) {
  if (interrupted && stopping()) return 0;
  if (stackdeep()) return 0;
  u32 kind = memo_mull;
  size j = memofind(sut, gol, dox, gen, kind);
  if (memo.data[j].sut) return memo.data[j].res;
  byte *mark = m->u.p->a->beg;
  noun spot = 0;
  noun r = mullx(m, sut, gol, dox, gen, &spot);
  if (!r && spot) errspot(spot);
  m->u.p->a->beg = mark;
  if (r) memoput(sut, gol, dox, gen, kind, r);
#ifdef HCTRACE
  if (!r) {
    bufout e[1] = {{(u8[4096]){0}, 0, 4096, 2, 0}};
    append(e, S("mull crash: "));
    appendnoun(e, gen);
    append(e, S("\n"));
    flush(e);
  }
#endif
  return r;
}

// Ford. A file in a desk starts with imports, parsed as clay's
// +pile-rule, and is built as clay's +build-file builds it: against zuse,
// with the product of each import put in front of it in turn, under its
// face if it has one. Only types matter here, so a file's product is its
// type. /- and /+ import from /sur and /lib by name, /= a hoon file by
// path; the imports of marks and of files through marks, /% /$ /*, and
// of directories, /~, aren't built yet.

typedef noun (*rule1)(parser *, size *);

// +stap: a path /knot/knot, or 0
noun stap(parser *p, size *pos) {
  size s = *pos;
  if (!chr(p, pos, '/')) return 0;
  nouns ks = {0};
  for (;;) {
    *push(&ks, p->a) = urs(p, pos);
    size t = *pos;
    if (!chr(p, pos, '/')) {
      *pos = t;
      break;
    }
  }
  if (ks.len == 1 && atomis(ks.data[0], 0)) {
    note(p, s, *pos, tok_string);
    return nul;
  }
  if (atomis(ks.data[ks.len-1], 0)) {
    *pos = s;
    return 0;
  }
  note(p, s, *pos, tok_string);
  return nounslist(p, &ks);
}

// +gaw: comments and whitespace, or nothing
b32 gaw(parser *p, size *pos) {
  for (;;) {
    if (vul(p, pos) || chr(p, pos, ' ') || chr(p, pos, '\n')) continue;
    return 1;
  }
}

// an import from /sur or /lib, [face=(unit term) pax=term]: *name,
// face=name or name, as +taut-rule
noun tautrule(parser *p, size *pos) {
  size s = *pos;
  noun a, b;
  if (chr(p, pos, '*') && (b = sym(p, pos))) return C2(nul, b);
  *pos = s;
  if (!(a = sym(p, pos))) return 0;
  size t = *pos;
  if (chr(p, pos, '=') && (b = sym(p, pos))) return C2(C2(nul, a), b);
  *pos = t;
  return C2(C2(nul, a), a);
}

// imports separated by commas, as (most ;~(plug com gaw) taut-rule)
noun tauts(parser *p, size *pos) {
  nouns xs = {0};
  noun t = tautrule(p, pos);
  if (!t) return 0;
  *push(&xs, p->a) = t;
  for (;;) {
    size s = *pos;
    if (chr(p, pos, ',') && gaw(p, pos) && (t = tautrule(p, pos))) {
      *push(&xs, p->a) = t;
      continue;
    }
    *pos = s;
    return nounslist(p, &xs);
  }
}

// what follows each rune, as the rules in +pile-rule
noun pilehep(parser *p, size *pos) { return tauts(p, pos); }

noun piletis(parser *p, size *pos) {
  noun a = sym(p, pos), b;
  return a && gap(p, pos) && (b = stap(p, pos)) ? C2(a, b) : 0;
}

noun pilesig(parser *p, size *pos) {
  noun a = sym(p, pos), b, c;
  return a && gap(p, pos) && (b = wyde(p, pos)) && gap(p, pos) && (c = stap(p, pos)) ? C3(a, b, c) : 0;
}

noun pilecen(parser *p, size *pos) {
  noun a = sym(p, pos), b;
  return a && gap(p, pos) && chr(p, pos, '%') && (b = sym(p, pos)) ? C2(a, b) : 0;
}

noun pilebuc(parser *p, size *pos) {
  noun a = sym(p, pos), b, c;
  return a && gap(p, pos) && chr(p, pos, '%') && (b = sym(p, pos)) && gap(p, pos)
         && chr(p, pos, '%') && (c = sym(p, pos)) ? C3(a, b, c) : 0;
}

noun piletar(parser *p, size *pos) {
  noun a = sym(p, pos), b, c;
  return a && gap(p, pos) && chr(p, pos, '%') && (b = sym(p, pos)) && gap(p, pos)
         && (c = stap(p, pos)) ? C3(a, b, c) : 0;
}

// one line /c fel
noun pileline(parser *p, size *pos, u8 c, rule1 fel) {
  if (!chr(p, pos, '/') || !chr(p, pos, c) || !gap(p, pos)) return 0;
  return fel(p, pos);
}

// lines of /c fel, each after a gap and all followed by one; or none,
// as +rune in clay's parsing rules: (pant (mast gap ;~(pfix fas bus gap fel)))
noun pilerune(parser *p, size *pos, u8 c, rule1 fel) {
  size s = *pos;
  nouns xs = {0};
  size t = *pos;
  noun x = pileline(p, pos, c, fel);
  if (!x) {
    *pos = t;
  } else {
    *push(&xs, p->a) = x;
    for (;;) {
      t = *pos;
      if (gap(p, pos) && (x = pileline(p, pos, c, fel))) {
        *push(&xs, p->a) = x;
        continue;
      }
      *pos = t;
      break;
    }
  }
  if (!gap(p, pos)) {
    *pos = s;
    return nul;
  }
  return nounslist(p, &xs);
}

// the lists in a list, one after another
noun zing(parser *p, noun l) {
  nouns xs = {0};
  for (; iscell(l); l = tl(l)) {
    for (noun m = hd(l); iscell(m); m = tl(m)) *push(&xs, p->a) = hd(m);
  }
  return nounslist(p, &xs);
}

// a desk file, as +pile-rule: [sur lib raw raz maz caz bar hoon], the
// body parsed with %dbug spots naming pax; 0 on a syntax error
noun pilerule(parser *p, noun pax) {
  size pos = 0;
  p->bug = 0;
  gay(p, &pos);
  size s = pos;
  if (!(chr(p, &pos, '/') && chr(p, &pos, '?') && gap(p, &pos) && dem(p, &pos) && gap(p, &pos))) {
    pos = s;
  }
  noun sur = zing(p, pilerune(p, &pos, '-', pilehep));
  noun lib = zing(p, pilerune(p, &pos, '+', pilehep));
  noun raw = pilerune(p, &pos, '=', piletis);
  noun raz = pilerune(p, &pos, '~', pilesig);
  noun maz = pilerune(p, &pos, '%', pilecen);
  noun caz = pilerune(p, &pos, '$', pilebuc);
  noun bar = pilerune(p, &pos, '*', piletar);
  p->bug = 1;
  p->wer = pax;
  nouns hs = {0};
  noun g = tall(p, &pos);
  if (!g) return 0;
  *push(&hs, p->a) = g;
  for (;;) {
    s = pos;
    if (gap(p, &pos) && (g = tall(p, &pos))) {
      *push(&hs, p->a) = g;
      continue;
    }
    pos = s;
    break;
  }
  gay(p, &pos);
  if (pos != p->len) {
    reach(p, pos);
    return 0;
  }
  noun gen = C2(K("tssg"), nounslist(p, &hs));
  return C2(sur, C2(lib, C2(raw, C5(raz, maz, caz, bar, gen))));
}

// the paths a name could be in /sur or /lib, as clay's +segments: the
// name split at -s, each - either kept or a /, joined ones first
noun segmentsx(parser *p, noun *ws, size n) {
  if (n == 1) return C2(C2(ws[0], nul), nul);
  nouns out = {0};
  for (noun l = segmentsx(p, ws + 1, n - 1); iscell(l); l = tl(l)) {
    noun s = hd(l);
    *push(&out, p->a) = C2(ws[0], s);
    // ws[0]-i.s
    size la = alen(ws[0]), lb = alen(hd(s));
    u8 *b = new(p->a, u8, la + lb + 1);
    for (size i = 0; i < la; i++) b[i] = abyte(ws[0], i);
    b[la] = '-';
    for (size i = 0; i < lb; i++) b[la+1+i] = abyte(hd(s), i);
    *push(&out, p->a) = C2(atombytes(p->a, b, la + lb + 1), tl(s));
  }
  return nounslist(p, &out);
}

noun segments(parser *p, noun suffix) {
  // words of lowercase letters and digits, separated by -s
  nouns ws = {0};
  size n = alen(suffix), i = 0;
  b32 ok = n > 0;
  while (ok && i < n) {
    size s = i;
    while (i < n && ((abyte(suffix, i) >= 'a' && abyte(suffix, i) <= 'z')
                     || (abyte(suffix, i) >= '0' && abyte(suffix, i) <= '9'))) i++;
    if (i == s) ok = 0;
    u8 *b = new(p->a, u8, i - s + 1);
    for (size k = s; k < i; k++) b[k-s] = abyte(suffix, k);
    *push(&ws, p->a) = atombytes(p->a, b, i - s);
    if (i < n) {
      if (abyte(suffix, i) != '-') ok = 0;
      i++;
      if (i == n) ok = 0;
    }
  }
  if (!ok) {
    ws.len = 0;
    *push(&ws, p->a) = suffix;
  }
  return flop(p, segmentsx(p, ws.data, ws.len));
}

// Messages. Types are printed about as hoon prints them, but named
// where a mold named them, and with where that was.

typedef struct {
  minter *m;
  bytes   b;
  size    limit;  // bytes, past which it's cut
  i32     holds;  // holds evaluated, to bound it
} tyout;

void typuts(tyout *t, s8 s) {
  for (size i = 0; i < s.len && t->b.len <= t->limit; i++) *push(&t->b, t->m->u.p->a) = s.buf[i];
}

void typut(tyout *t, char *c) {
  typuts(t, (s8){(u8*)c, (size)__builtin_strlen(c)});
}

void tyatombytes(tyout *t, noun a) {
  for (size i = 0; i < alen(a) && t->b.len <= t->limit; i++) *push(&t->b, t->m->u.p->a) = abyte(a, i);
}

void tydecimal(tyout *t, noun a) {
  if (alen(a) > 8) {
    typut(t, "0x");
    // as hex, the bytes from the top
    for (size i = alen(a); i-- > 0; ) {
      u8 c = abyte(a, i), d[2] = {"0123456789abcdef"[c >> 4], "0123456789abcdef"[c & 15]};
      typuts(t, (s8){d, 2});
    }
    return;
  }
  u8 tmp[24], *e = tmp + 24, *s = e;
  u64 v = atomlow(a);
  do *--s = (u8)('0' + v % 10); while (v /= 10);
  typuts(t, (s8){s, e - s});
}

// a constant atom of an aura
void tyconst(tyout *t, noun aura, noun v) {
  u8 c0 = alen(aura) ? abyte(aura, 0) : 0;
  if (c0 == 'f' && atomfits(v) && atomlow(v) < 2) {
    typut(t, atomlow(v) ? "%.n" : "%.y");
  } else if (c0 == 'n' && !alen(v)) {
    typut(t, "~");
  } else if (c0 == 't' && alen(aura) > 1 && isterm(v)) {
    typut(t, "%");
    tyatombytes(t, v);
  } else if (c0 == 't' && iscord(v)) {
    typut(t, "'");
    tyatombytes(t, v);
    typut(t, "'");
  } else if (!c0 && isterm(v) && alen(v) > 1) {
    typut(t, "%");
    tyatombytes(t, v);
  } else {
    tydecimal(t, v);
  }
}

// a wing as written, a.b.c
void tywing(tyout *t, noun hyp) {
  parser *p = t->m->u.p;
  for (noun l = hyp; iscell(l); l = tl(l)) {
    if (l != hyp) typut(t, ".");
    noun i = hd(l);
    if (isatom(i)) {
      if (alen(i)) tyatombytes(t, i); else typut(t, "$");
    } else if (atomis(hd(i), 0)) {
      if (atomis(tl(i), 1)) {
        typut(t, ".");
      } else {
        typut(t, "+");
        tydecimal(t, tl(i));
      }
    } else {
      for (noun k = hd(tl(i)); atomfits(k) && atomlow(k) > 0; k = D(atomlow(k) - 1)) typut(t, "^");
      if (iscell(tl(tl(i)))) tyatombytes(t, tl(tl(tl(i)))); else typut(t, "..");
    }
  }
}

// the names of the arms in a core's chapters, up to max
void tyarms(tyout *t, noun bat, i32 *n, i32 max) {
  if (isatom(bat) || *n > max) return;
  // each arm in the chapter, in its tree
  nouns st = {0};
  *push(&st, t->m->u.p->a) = tl(mapn(bat));
  while (st.len) {
    noun a = st.data[--st.len];
    if (isatom(a)) continue;
    if ((*n)++ < max) {
      typut(t, " +");
      if (alen(hd(mapn(a)))) tyatombytes(t, hd(mapn(a))); else typut(t, "$");
    }
    *push(&st, t->m->u.p->a) = mapl(a);
    *push(&st, t->m->u.p->a) = mapr(a);
  }
  tyarms(t, mapl(bat), n, max);
  tyarms(t, mapr(bat), n, max);
}

b32 spotsource(noun spot, s8 *out);

// the only arm of a core, or 0
noun onlyarm(noun cor) {
  noun bat = coilbat(coreof(cor));
  if (isatom(bat) || iscell(mapl(bat)) || iscell(mapr(bat))) return 0;
  noun arms = tl(mapn(bat));
  if (isatom(arms) || iscell(mapl(arms)) || iscell(mapr(arms))) return 0;
  return tl(mapn(arms));
}

// for the type of a hold, the mold builder whose mold it's of, as list
// for (list @t), and that builder's core; 0 if none. The hold is of the
// $ arm of the builder or of the mold its spec makes, whose context is
// the builder
noun moldname(parser *p, noun cor, noun *gate) {
  for (i32 k = 0; k < 2 && tagis(cor, "core"); k++) {
    noun arm = onlyarm(cor);
    noun nam = arm ? nounmapget(&armnames, armkey(p->a, arm)) : 0;
    if (nam && tagis(hd(tl(cor)), "cell")) {
      *gate = cor;
      return nam;
    }
    noun pay = hd(tl(cor));
    if (!tagis(pay, "cell")) return 0;
    cor = tl(tl(pay));
    while (tagis(cor, "face") || tagis(cor, "hint")) cor = tl(tl(cor));
  }
  return 0;
}

void tyrender(tyout *t, noun ty, i32 depth) {
  parser *p = t->m->u.p;
  if (t->b.len > t->limit) return;
  if (depth > 10) {
    typut(t, "...");
    return;
  }
  if (atomeqc(ty, "noun")) {
    typut(t, "*");
  } else if (atomeqc(ty, "void")) {
    typut(t, "!!");
  } else if (isatom(ty)) {
    typut(t, "?");
  } else if (tagis(ty, "atom")) {
    noun q = tl(tl(ty));
    if (iscell(q)) {
      tyconst(t, hd(tl(ty)), tl(q));
    } else {
      typut(t, "@");
      tyatombytes(t, hd(tl(ty)));
    }
  } else if (tagis(ty, "cell")) {
    typut(t, "[");
    tyrender(t, hd(tl(ty)), depth + 1);
    noun r = tl(tl(ty));
    for (i32 k = 0; tagis(r, "cell") && k < 12; k++) {
      typut(t, " ");
      tyrender(t, hd(tl(r)), depth + 1);
      r = tl(tl(r));
    }
    typut(t, " ");
    tyrender(t, r, depth + 1);
    typut(t, "]");
  } else if (tagis(ty, "face")) {
    noun f = hd(tl(ty));
    if (isatom(f)) {
      tyatombytes(t, f);
      typut(t, "=");
    }
    tyrender(t, tl(tl(ty)), depth + 1);
  } else if (tagis(ty, "fork")) {
    noun set = tl(ty);
    // ? for a loobean
    noun y = C3(K("atom"), K("f"), C2(nul, D(0))), n = C3(K("atom"), K("f"), C2(nul, D(1)));
    b32 hy = 0, hn = 0, more = 0;
    for (noun s[3] = {set}, *sp = s + 1; sp > s; ) {
      noun x = *--sp;
      if (isatom(x)) continue;
      if (hd(x) == y) hy = 1; else if (hd(x) == n) hn = 1; else more = 1;
      if (sp - s < 2) {
        *sp++ = mapl(x);
        *sp++ = mapr(x);
      } else {
        more = 1;
      }
    }
    if (hy && hn && !more) {
      typut(t, "?");
      return;
    }
    typut(t, "?(");
    nouns st = {0};
    *push(&st, t->m->u.p->a) = set;
    i32 k = 0;
    while (st.len && t->b.len <= t->limit) {
      noun s = st.data[--st.len];
      if (isatom(s)) continue;
      if (k++) typut(t, " ");
      if (k > 8) {
        typut(t, "...");
        break;
      }
      tyrender(t, hd(s), depth + 1);
      *push(&st, t->m->u.p->a) = tl(tl(s));
      *push(&st, t->m->u.p->a) = hd(tl(s));
    }
    typut(t, ")");
  } else if (tagis(ty, "hint")) {
    noun note = tl(hd(tl(ty)));
    if (tagis(note, "made")) {
      noun args = tl(tl(note));
      if (iscell(args)) typut(t, "(");
      tyatombytes(t, hd(tl(note)));
      if (iscell(args)) {
        for (noun l = tl(args); iscell(l); l = tl(l)) {
          typut(t, " ");
          tywing(t, hd(l));
        }
        typut(t, ")");
      }
    } else if (tagis(note, "know") && tagis(tl(tl(ty)), "hint")
               && tagis(tl(hd(tl(tl(tl(ty))))), "made")) {
      tyrender(t, tl(tl(ty)), depth);
    } else if (tagis(note, "know") && isatom(tl(note))) {
      tyatombytes(t, tl(note));
    } else {
      tyrender(t, tl(tl(ty)), depth + 1);
    }
  } else if (tagis(ty, "hold")) {
    // as (list @t) for a mold builder's, as written if it's in a file we
    // have, or evaluated
    s8 src;
    noun gen = tl(tl(ty)), cor;
    noun nam = moldname(p, hd(tl(ty)), &cor);
    if (nam) {
      typut(t, "(");
      tyatombytes(t, nam);
      // the sample, a face for each argument
      noun sam = hd(tl(hd(tl(cor))));
      for (i32 k = 0; k < 8; k++) {
        typut(t, " ");
        noun x = tagis(sam, "cell") ? hd(tl(sam)) : sam;
        if (tagis(x, "face")) x = tl(tl(x));
        // an argument is a mold: the type it makes
        noun y = tagis(x, "core") ? play(&t->m->u, x, C2(K("limb"), nul)) : 0;
        tyrender(t, y ? y : x, depth + 1);
        if (!tagis(sam, "cell")) break;
        sam = tl(tl(sam));
      }
      typut(t, ")");
    } else if (tagis(gen, "dbug") && spotsource(hd(tl(gen)), &src) && src.len <= 60) {
      typuts(t, src);
    } else if (t->holds++ < 16) {
      noun r = repo(&t->m->u, ty);
      if (r) tyrender(t, r, depth + 1); else typut(t, "?");
    } else {
      typut(t, "...");
    }
  } else if (tagis(ty, "core")) {
    noun c = coreof(ty), bat = coilbat(c);
    noun pay = hd(tl(ty));
    i32 n = 0;
    b32 gate = iscell(bat) && isatom(mapl(bat)) && isatom(mapr(bat)) && iscell(tl(mapn(bat)))
               && isatom(mapl(tl(mapn(bat)))) && isatom(mapr(tl(mapn(bat))))
               && !alen(hd(mapn(tl(mapn(bat)))));
    if (gate) {
      typut(t, "$-(");
      if (tagis(pay, "cell")) tyrender(t, hd(tl(pay)), depth + 1); else typut(t, "*");
      typut(t, " ...)");
      return;
    }
    typut(t, "<core");
    tyarms(t, bat, &n, 6);
    if (n > 6) typut(t, " ...");
    typut(t, ">");
  } else {
    typut(t, "?");
  }
}

// a type, printed
s8 typestr(minter *m, noun ty) {
  tyout t = {m, {0}, 160, 0};
  tyrender(&t, ty, 0);
  if (t.b.len > t.limit) {
    t.b.len = t.limit;
    typut(&t, "...");
  }
  return (s8){t.b.data, t.b.len};
}

// Where types come from: the arm of a core, by the name a mold left in a
// hint, found in the subject it was made in

typedef struct {
  i32  file;   // in srcfiles, or 0
  noun wer;    // the path, if file is 0
  size line;
  size col;
  noun name;   // the arm, or 0
} site;

// the hoon of an arm named nam in the cores of a type, outermost first
// as a wing would find it
noun armin(noun sut, noun nam, i32 *budget) {
  while (--*budget > 0) {
    if (tagis(sut, "core")) {
      // the chapters, each a map of arms
      noun stack[64];
      i32 sp = 0;
      stack[sp++] = coilbat(coreof(sut));
      while (sp) {
        noun c = stack[--sp];
        if (isatom(c)) continue;
        noun found = mapget(tl(mapn(c)), nam);
        if (found) return found;
        if (sp < 62) {
          stack[sp++] = mapl(c);
          stack[sp++] = mapr(c);
        }
      }
      sut = hd(tl(sut));
    } else if (tagis(sut, "face") || tagis(sut, "hint")) {
      sut = tl(tl(sut));
    } else if (tagis(sut, "cell")) {
      noun r = armin(hd(tl(sut)), nam, budget);
      if (r) return r;
      sut = tl(tl(sut));
    } else {
      return 0;
    }
  }
  return 0;
}

b32 armsite(minter *m, noun gen, site *s) {
  noun v = nounmapget(&armsites, armkey(m->u.p->a, gen));
  if (!v) return 0;
  u64 x = atomlow(v);
  s->file = (i32)(x >> 40);
  s->line = (size)(x >> 16 & 0xffffff);
  s->col = (size)(x & 0xffff);
  return 1;
}

b32 spotsite(noun spot, site *s) {
  noun pint = tl(spot);
  s->wer = hd(spot);
  s->file = srcbywer(hd(spot));
  s->line = (size)atomlow(hd(hd(pint)));
  s->col = (size)atomlow(tl(hd(pint)));
  return 1;
}

// where a type was made, if it can be told
b32 typesite(minter *m, noun ty, site *s, i32 depth) {
  if (depth > 8 || isatom(ty)) return 0;
  if (tagis(ty, "hint")) {
    noun note = tl(hd(tl(ty)));
    if (tagis(note, "made")) {
      i32 budget = 4096;
      noun gen = armin(hd(hd(tl(ty))), hd(tl(note)), &budget);
      if (gen && armsite(m, gen, s)) {
        s->name = hd(tl(note));
        return 1;
      }
    }
    return typesite(m, tl(tl(ty)), s, depth + 1);
  }
  if (tagis(ty, "face")) return typesite(m, tl(tl(ty)), s, depth + 1);
  if (tagis(ty, "hold")) {
    noun gen = tl(tl(ty)), cor;
    noun nam = moldname(m->u.p, hd(tl(ty)), &cor);
    if (nam && armsite(m, onlyarm(cor), s)) {
      s->name = nam;
      return 1;
    }
    if (tagis(gen, "dbug")) return spotsite(hd(tl(gen)), s);
    if (armsite(m, gen, s)) return 1;
    noun r = repo(&m->u, ty);
    return r && typesite(m, r, s, depth + 1);
  }
  if (tagis(ty, "core")) {
    // a core is where its arms are
    noun bat = coilbat(coreof(ty));
    if (iscell(bat) && iscell(tl(mapn(bat)))) {
      noun arms = tl(mapn(bat));
      if (armsite(m, tl(mapn(arms)), s)) return 1;
      noun g = tl(mapn(arms));
      if (tagis(g, "dbug")) return spotsite(hd(tl(g)), s);
    }
  }
  return 0;
}

// the name of a file, as given or from its path, /app/foo/hoon as
// app/foo.hoon
void appendsite(bufout *o, site *s) {
  if (s->file) {
    append(o, shown(srcfiles.data[s->file].name));
  } else {
    for (noun l = s->wer; iscell(l); l = tl(l)) {
      if (l != s->wer) append(o, isatom(tl(tl(l))) ? S(".") : S("/"));
      u8 tmp[8];
      append(o, (s8){abytes(hd(l), tmp), alen(hd(l))});
    }
  }
  append(o, S(":"));
  appendsize(o, s->line);
  append(o, S(":"));
  appendsize(o, s->col);
}

// the line of a file at a spot
b32 spotline(noun spot, s8 *line, size *ln, size *c0, size *c1) {
  site s = {0};
  spotsite(spot, &s);
  if (!s.file) return 0;
  s8 src = srcfiles.data[s.file].src;
  size l = 1, at = 0;
  while (at < src.len && l < s.line) if (src.buf[at++] == '\n') l++;
  size end = at;
  while (end < src.len && src.buf[end] != '\n') end++;
  *line = (s8){src.buf + at, end - at};
  *ln = s.line;
  noun pint = tl(spot);
  *c0 = s.col;
  *c1 = atomlow(hd(tl(pint))) == (u64)s.line ? (size)atomlow(tl(tl(pint))) : end - at + 1;
  return 1;
}

// the text at a spot, if it's on one line
b32 spotsource(noun spot, s8 *out) {
  s8 line;
  size ln, c0, c1;
  if (!spotline(spot, &line, &ln, &c0, &c1) || atomlow(hd(tl(tl(spot)))) != (u64)ln) return 0;
  if (c0 < 1 || c1 > line.len + 1 || c1 <= c0) return 0;
  *out = (s8){line.buf + c0 - 1, c1 - c0};
  return 1;
}

// a type without the name it prints as: hints off, a hold evaluated
noun unname(minter *m, noun ty) {
  for (i32 k = 0; k < 8; k++) {
    if (tagis(ty, "face")) {
      noun q = unname(m, tl(tl(ty)));
      return cons(m->u.p->a, K("face"), cons(m->u.p->a, hd(tl(ty)), q));
    }
    noun r = tagis(ty, "hint") ? tl(tl(ty)) : tagis(ty, "hold") ? repo(&m->u, ty) : 0;
    if (!r) break;
    ty = r;
  }
  return ty;
}

void errtype(bufout *o, minter *m, char *what, noun ty) {
  append(o, S("  "));
  append(o, (s8){(u8*)what, (size)__builtin_strlen(what)});
  append(o, S(": "));
  append(o, typestr(m, ty));
  append(o, S("\n"));
  site s = {0};
  if (typesite(m, ty, &s, 0)) {
    append(o, S("        from "));
    if (s.name) {
      append(o, S("+"));
      u8 tmp[8];
      append(o, (s8){abytes(s.name, tmp), alen(s.name)});
      append(o, S(" at "));
    }
    appendsite(o, &s);
    append(o, S("\n"));
  }
}

// the error kept, for a file named name, as
//
//   app/foo.hoon:12:3: nest-fail
//      12 |   ^-  @ud  'abc'
//         |   ^^^^^^^^^^^^^^
//     need: @ud
//     have: 'abc'
void errwhat(bufout *o, minter *m);
void errdetail(bufout *o, minter *m);

void errprint(bufout *o, minter *m, char *name) {
  noun spot = hcerr.spot;
  if (spot) {
    site s = {0};
    spotsite(spot, &s);
    appendsite(o, &s);
  } else {
    append(o, shown(name));
  }
  append(o, S(": "));
  errwhat(o, m);
  s8 line;
  size ln, c0, c1;
  if (spot && spotline(spot, &line, &ln, &c0, &c1)) {
    u8 num[24], *e = num + 24, *b = e;
    size v = ln;
    do *--b = (u8)('0' + v % 10); while (v /= 10);
    size w = e - b;
    append(o, S("  "));
    append(o, (s8){b, w});
    append(o, S(" | "));
    append(o, line);
    append(o, S("\n  "));
    for (size i = 0; i < w; i++) append(o, S(" "));
    append(o, S(" | "));
    for (size i = 1; i < c0; i++) append(o, line.buf[i-1] == '\t' ? S("\t") : S(" "));
    for (size i = c0; i < c1 && i <= line.len; i++) append(o, S("^"));
    if (c1 <= c0) append(o, S("^"));
    append(o, S("\n"));
  }
  errdetail(o, m);
}

// what the error kept is, a line
void errwhat(bufout *o, minter *m) {
  if (hcerr.kind == errnest) {
    append(o, S("nest-fail"));
  } else if (hcerr.kind == errfind) {
    tyout t = {m, {0}, 200, 0};
    tywing(&t, hcerr.hyp);
    append(o, S("find-fail: no "));
    append(o, (s8){t.b.data, t.b.len});
  } else if (hcerr.kind == errdeep) {
    append(o, S("nested too deep"));
  } else if (hcerr.kind == errtext) {
    for (noun l = hcerr.text; iscell(l); l = tl(l)) {
      u8 c = (u8)atomlow(hd(l));
      append(o, (s8){&c, 1});
    }
  } else {
    append(o, S("cannot compile"));
  }
  append(o, S("\n"));
}

// the types of a nest-fail, and where they're from
// the two types of a nest-fail where they differ: if they're cells alike
// on one side, as the sample of a gate against the payload of its core,
// the sides that differ, and if they print the same, as a type made by a
// mold and then changed, what they're made of
void errnarrow(minter *m, noun *needp, noun *havep) {
  noun need = hcerr.need, have = hcerr.have;
  {
    for (i32 k = 0; k < 4; k++) {
      // where the two differ, if they're cells alike on one side, as the
      // sample of a gate against the payload of its core
      while (tagis(need, "cell") && tagis(have, "cell")) {
        if (tl(tl(need)) == tl(tl(have))) {
          need = hd(tl(need));
          have = hd(tl(have));
        } else if (hd(tl(need)) == hd(tl(have))) {
          need = tl(tl(need));
          have = tl(tl(have));
        } else {
          break;
        }
      }
      // and if they print the same, as a type made by a mold and then
      // changed, what they're made of
      s8 a = typestr(m, need), b = typestr(m, have);
      b32 same = a.len == b.len;
      for (size i = 0; same && i < a.len; i++) same = a.buf[i] == b.buf[i];
      if (!same) break;
      need = unname(m, need);
      have = unname(m, have);
    }
  }
  *needp = need;
  *havep = have;
}

// where the types of a nest-fail come from, as errdetail says: up to
// three, need, have, and what need is part of, if it's narrowed; how many
typedef struct {
  char *what;   // need, have or in
  s8    type;   // as printed
  site  at;
} errsite;

i32 errsites(minter *m, errsite *out) {
  if (hcerr.kind != errnest) return 0;
  noun need, have;
  errnarrow(m, &need, &have);
  noun ts[3] = {need, have, hcerr.need};
  char *ws[3] = {"need", "have", "in"};
  i32 n = 0;
  for (i32 i = 0; i < 3; i++) {
    if (i == 2 && need == hcerr.need) break;
    errsite e = {ws[i], typestr(m, ts[i]), {0}};
    if (typesite(m, ts[i], &e.at, 0)) out[n++] = e;
  }
  return n;
}

void errdetail(bufout *o, minter *m) {
  if (hcerr.kind == errnest) {
    noun need, have;
    errnarrow(m, &need, &have);
    errtype(o, m, "need", need);
    errtype(o, m, "have", have);
    // and what that's part of, if it's not all
    site s = {0};
    if (need != hcerr.need && typesite(m, hcerr.need, &s, 0)) {
      append(o, S("  in: "));
      append(o, typestr(m, hcerr.need));
      append(o, S("\n"));
      {
        append(o, S("        from "));
        if (s.name) {
          u8 tmp[8];
          append(o, S("+"));
          append(o, (s8){abytes(s.name, tmp), alen(s.name)});
          append(o, S(" at "));
        }
        appendsite(o, &s);
        append(o, S("\n"));
      }
    }
  }
}

typedef struct {
  char   *desk;       // the desk's directory
  char  **deps;       // and others searched for what it doesn't have
  i32     ndeps;
  noun    zuse;       // the subject before imports
  nounmap built;      // desk path to type
  nounmap building;   // desk paths being built, for cycles
  nounmap importers;  // what's built to the list of what was built with
                      // it, to drop when it changes
} fordenv;

// what's being built, a desk path, or [1 mark] or [2 mark mark] for a
// mark or a conversion
noun fordcur;

// that what's being built is built with key
void fordneeds(fordenv *f, noun key) {
  if (!fordcur) return;
  noun l = nounmapget(&f->importers, key);
  for (noun k = l; l && iscell(k); k = tl(k)) if (hd(k) == fordcur) return;
  nounmapput(&f->importers, key, cons(0, fordcur, l ? l : nul));
}

// forget what was built from key, as when its file changes, and all that
// was built with it
void fordforget(fordenv *f, noun key) {
  noun l = nounmapget(&f->importers, key);
  nounmapput(&f->built, key, 0);
  if (!l) return;
  nounmapput(&f->importers, key, 0);
  for (; iscell(l); l = tl(l)) fordforget(f, hd(l));
}

// Sources are read from disk, or, where srcreader is set, as it reads
// them: the language server has files open in an editor
b32 (*srcreader)(arena *, char *, s8 *);

b32 srcread(arena *a, char *path, s8 *out) {
  return srcreader ? srcreader(a, path, out) : osreadfile(a, path, out);
}

b32 srcexists(arena *a, char *path) {
  byte *mark = a->beg;
  s8 s;
  b32 r = srcread(a, path, &s);
  a->beg = mark;
  return r;
}

// a desk path [a b c %hoon] as a name, a/b/c.hoon
char *pathstr(arena *a, noun pax) {
  bytes b = {0};
  for (noun l = pax; iscell(l); l = tl(l)) {
    if (l != pax) *push(&b, a) = isatom(tl(l)) ? '.' : '/';
    for (size i = 0; i < alen(hd(l)); i++) *push(&b, a) = abyte(hd(l), i);
  }
  *push(&b, a) = 0;
  return (char*)b.data;
}

// a desk path [a b c %hoon] as a file in a desk, desk/a/b/c.hoon
char *deskfilein(arena *a, char *desk, noun pax) {
  bytes b = {0};
  for (char *c = desk; *c; c++) *push(&b, a) = (u8)*c;
  for (noun l = pax; iscell(l); l = tl(l)) {
    *push(&b, a) = isatom(tl(l)) ? '.' : '/';
    for (size i = 0; i < alen(hd(l)); i++) *push(&b, a) = abyte(hd(l), i);
  }
  *push(&b, a) = 0;
  return (char*)b.data;
}

// a desk path as a file: in the desk, or else in the first desk searched
// that has it, or else where it would be in the desk
char *deskfile(arena *a, fordenv *f, noun pax) {
  char *r = deskfilein(a, f->desk, pax);
  if (!f->ndeps || srcexists(a, r)) return r;
  for (i32 i = 0; i < f->ndeps; i++) {
    char *d = deskfilein(a, f->deps[i], pax);
    if (srcexists(a, d)) return d;
  }
  return r;
}

// the path in the desk for a name in /sur or /lib, as clay's
// +try-fit-path; 0 if there's none
noun fitpath(parser *p, fordenv *f, char *pre, noun name) {
  for (noun l = segments(p, name); iscell(l); l = tl(l)) {
    noun pax = C2(term(pre), weld(p, hd(l), C2(K("hoon"), nul)));
    s8 src;
    if (srcread(p->a, deskfile(p->a, f, pax), &src)) return pax;
  }
  return 0;
}

noun fordbuild(arena *a, fordenv *f, noun pax, bufout *err);

char *fordat;   // the file whose imports are being built, for messages

void fordfail(bufout *err, char *why, noun what) {
  if (fordat) {
    append(err, shown(fordat));
    append(err, S(": "));
  }
  append(err, (s8){(u8*)why, (size)__builtin_strlen(why)});
  append(err, S(" "));
  // a path as one, /a/b/c.hoon
  b32 path = iscell(what);
  for (noun l = what; path && iscell(l); l = tl(l)) path = isatom(hd(l)) && isterm(hd(l));
  if (path) {
    for (noun l = what; iscell(l); l = tl(l)) {
      append(err, isatom(tl(l)) && l != what ? S(".") : S("/"));
      u8 tmp[8];
      append(err, (s8){abytes(hd(l), tmp), alen(hd(l))});
    }
  } else {
    appendnoun(err, what);
  }
  append(err, S("\n"));
}

// Marks, as clay's ford makes them for /% /$ /* and /~. Of each vase
// clay makes only the type is made here: the one value clay looks at
// is the +grad of a mark, a mark or a core, which +musk finds in the
// mark's type, as it does for ^~.

// a hoon in clay's ford, which is parsed without spots
noun fordgen(parser *p, char *txt) {
  parser q = newparser(p->a, (s8){(u8*)txt, (size)__builtin_strlen(txt)});
  size pos = 0;
  return vest(&q, &pos);
}

// the nave of a mark whose +grad is a core, and of one whose +grad
// names another mark, as in the %mark case of +bush-to-vase
char *navegrad =
  "=/  typ  _+<.cor\n"
  "=/  dif  _*diff:grad:cor\n"
  "^-  (nave:clay typ dif)\n"
  "|%\n"
  "++  diff  |=([old=typ new=typ] (diff:~(grad cor old) new))\n"
  "++  form  form:grad:cor\n"
  "++  join\n"
  "  |=  [a=dif b=dif]\n"
  "  ^-  (unit (unit dif))\n"
  "  ?:  =(a b)\n"
  "    ~\n"
  "  `(join:grad:cor a b)\n"
  "++  mash\n"
  "  |=  [a=[=ship =desk =dif] b=[=ship =desk =dif]]\n"
  "  ^-  (unit dif)\n"
  "  ?:  =(dif.a dif.b)\n"
  "    ~\n"
  "  `(mash:grad:cor a b)\n"
  "++  pact  |=([v=typ d=dif] (pact:~(grad cor v) d))\n"
  "++  vale  noun:grab:cor\n"
  "--\n";

char *navedeg =
  "=/  typ  _+<.cor\n"
  "=/  dif  _*diff:deg\n"
  "^-  (nave typ dif)\n"
  "|%\n"
  "++  diff\n"
  "  |=  [old=typ new=typ]\n"
  "  ^-  dif\n"
  "  (diff:deg (tub old) (tub new))\n"
  "++  form  form:deg\n"
  "++  join  join:deg\n"
  "++  mash  mash:deg\n"
  "++  pact\n"
  "  |=  [v=typ d=dif]\n"
  "  ^-  typ\n"
  "  (but (pact:deg (tub v) d))\n"
  "++  vale  noun:grab:cor\n"
  "--\n";

// the type of a vase of type sut slapped with gen, as +slub; 0 if it
// doesn't compile
noun fordslap(parser *p, noun sut, noun gen) {
  if (!sut || !gen) return 0;
  minter m = {0};
  m.u.p = p;
  m.vet = 1;
  noun r = mint(&m, sut, K("noun"), gen);
  return r ? hd(r) : 0;
}

// the type of a gate slammed with a sample, as +slam
noun fordslam(parser *p, noun gat, noun sam) {
  if (!gat || !sam) return 0;
  noun gen = C4(K("cnsg"), C2(nul, nul), C2(nul, D(2)), C2(C2(nul, D(3)), nul));
  return fordslap(p, C3(K("cell"), gat, sam), gen);
}

// the type of a vase made in clay's ford by !>(gen) against sut,
// which is burped
noun fordvase(parser *p, noun sut, char *gen) {
  minter m = {0};
  m.u.p = p;
  m.vet = 1;
  noun t = fordslap(p, sut, fordgen(p, gen));
  return t ? burp(&m, t) : 0;
}

// clay's bud: zuse as clay sees it, !>(..zuse), or with what, the
// type of what in it, as nave:clay
noun fordbud(parser *p, fordenv *f, char *what) {
  noun zus = fordvase(p, f->zuse, "..zuse");
  return what ? fordslap(p, zus, fordgen(p, what)) : zus;
}

// what +musk can tell of the product of fol on a subject of type sut
// without the subject's value: 0 nothing, 1 an atom, in out, 2 a cell
i32 fordknow(parser *p, noun sut, noun fol, noun *out) {
  minter m = {0};
  m.u.p = p;
  m.vet = 1;
  noun bus = bran(&m, sut);
  noun s = bus ? araw(&m, bus, fol) : 0;
  if (!s || isatom(s) || !(s = complete(&m, s))) return 0;
  noun k = semmask(s);
  if (tagis(k, "half")) return 2;
  if (!tagis(k, "full") || iscell(tl(k))) return 0;
  if (iscell(semdata(s))) return 2;
  *out = semdata(s);
  return 1;
}

// whether a chapter in bat has an arm named cog
b32 slobarms(noun bat, noun cog) {
  for (; iscell(bat); bat = mapr(bat)) {
    if (maphas(tl(mapn(bat)), cog) || slobarms(mapl(bat), cog)) return 1;
  }
  return 0;
}

// whether a core type has an arm named cog, as +slob
b32 slob(parser *p, noun cog, noun typ) {
  typer u = {0};
  u.p = p;
  while (typ && (tagis(typ, "hold") || tagis(typ, "hint"))) typ = repo(&u, typ);
  return typ && tagis(typ, "core") && slobarms(coilbat(coreof(typ)), cog);
}

// whether the core of a mark file has an arm, +grow or +grab, with an
// arm for mak, as clay's +has-arm
b32 hasarm(parser *p, noun arm, noun mak, noun cor) {
  if (!slob(p, arm, cor)) return 0;
  noun rib = fordslap(p, cor, C2(K("wing"), C2(arm, nul)));
  return rib && slob(p, mak, rib);
}

// [%spin %cltr [%sand %t 'how-%a->%b'] ~], naming a conversion
noun fordspin(parser *p, char *how, noun a, noun b) {
  bytes t = {0};
  for (char *c = how; *c; c++) *push(&t, p->a) = (u8)*c;
  *push(&t, p->a) = '-';
  *push(&t, p->a) = '%';
  for (size i = 0; i < alen(a); i++) *push(&t, p->a) = abyte(a, i);
  *push(&t, p->a) = '-';
  *push(&t, p->a) = '>';
  *push(&t, p->a) = '%';
  for (size i = 0; i < alen(b); i++) *push(&t, p->a) = abyte(b, i);
  noun txt = atombytes(p->a, t.data, t.len);
  return C2(K("spin"), C2(K("cltr"), C2(C3(K("sand"), K("t"), txt), nul)));
}

noun fordnave(parser *p, fordenv *f, noun mak, bufout *err);
noun fordcast(parser *p, fordenv *f, noun a, noun b, bufout *err);

noun fordnavex(parser *p, fordenv *f, noun mak, bufout *err) {
  noun pax = fitpath(p, f, "mar", mak);
  if (!pax) {
    fordfail(err, "ford: no mark", mak);
    return 0;
  }
  noun cor = fordbuild(p->a, f, pax, err);
  if (!cor) return 0;
  minter m = {0};
  m.u.p = p;
  m.vet = 1;
  noun gad = mint(&m, cor, K("noun"), C2(K("limb"), K("grad")));
  noun val = 0;
  i32 k = gad ? fordknow(p, cor, tl(gad), &val) : 0;
  noun t = 0;
  if (k == 2) {
    noun sut = C3(K("cell"), C3(K("face"), K("cor"), cor), fordbud(p, f, 0));
    t = fordslap(p, sut, fordgen(p, navegrad));
  } else if (k == 1) {
    noun deg = fordnave(p, f, val, err);
    noun tub = deg ? fordcast(p, f, mak, val, err) : 0;
    noun but = tub ? fordcast(p, f, val, mak, err) : 0;
    if (!but) return 0;
    noun sut = C3(K("cell"), C3(K("face"), K("nave"), fordbud(p, f, "nave:clay")),
               C3(K("cell"), C3(K("face"), K("cor"), cor),
               C3(K("cell"), C3(K("face"), K("but"), but),
               C3(K("cell"), C3(K("face"), K("tub"), tub), C3(K("face"), K("deg"), deg)))));
    t = fordslap(p, sut, fordgen(p, navedeg));
  } else {
    fordfail(err, "ford: no +grad in mark", mak);
    return 0;
  }
  if (!t) fordfail(err, "ford: cannot build mark", mak);
  return t;
}

// the type of the statically typed core of a mark, as +build-nave
noun fordnave(parser *p, fordenv *f, noun mak, bufout *err) {
  noun key = C2(D(1), mak);
  fordneeds(f, key);
  noun t = nounmapget(&f->built, key);
  if (t) return t;
  if (nounmapget(&f->building, key)) {
    fordfail(err, "ford: cycle at mark", mak);
    return 0;
  }
  nounmapput(&f->building, key, YES);
  noun cur = fordcur;
  fordcur = key;
  t = fordnavex(p, f, mak, err);
  fordcur = cur;
  nounmapput(&f->building, key, 0);
  if (t && !aborted) nounmapput(&f->built, key, t);
  return t;
}

noun fordcastx(parser *p, fordenv *f, noun a, noun b, bufout *err) {
  noun pa = fitpath(p, f, "mar", a);
  noun old = pa ? fordbuild(p->a, f, pa, err) : 0;
  if (pa && !old) return 0;
  if (old && hasarm(p, K("grow"), b, old)) {
    noun gen = C3(K("brcl"), fordgen(p, "v=+<.cor"),
                  C3(K("sggr"), fordspin(p, "grow", a, b),
                     C3(K("tsgl"), C2(K("limb"), b), fordgen(p, "~(grow cor v)"))));
    noun t = fordslap(p, C3(K("face"), K("cor"), old), gen);
    if (!t) fordfail(err, "ford: cannot build +grow to", b);
    return t;
  }
  noun pb = fitpath(p, f, "mar", b);
  noun new = pb ? fordbuild(p->a, f, pb, err) : 0;
  if (pb && !new) return 0;
  if (new && hasarm(p, K("grab"), a, new)) {
    noun gen = C3(K("sggr"), fordspin(p, "grab", a, b),
                  C3(K("tsgl"), C2(K("limb"), a), C2(K("limb"), K("grab"))));
    minter m = {0};
    m.u.p = p;
    m.vet = 1;
    noun r = mint(&m, new, K("noun"), gen);
    noun val;
    if (!r || fordknow(p, new, tl(r), &val) == 1) {
      fordfail(err, "ford: cannot build +grab from", a);
      return 0;
    }
    return hd(r);
  }
  if (b == K("noun")) return fordbud(p, f, "same");
  fordfail(err, "ford: no cast from", C2(a, b));
  return 0;
}

// the type of a gate converting mark a to b, as +build-cast
noun fordcast(parser *p, fordenv *f, noun a, noun b, bufout *err) {
  if (a == b) return fordbud(p, f, "same");
  if (a == K("mime") && b == K("hoon")) return fordvase(p, fordbud(p, f, 0), "|=(m=mime q.q.m)");
  noun key = C3(D(2), a, b);
  fordneeds(f, key);
  noun t = nounmapget(&f->built, key);
  if (t) return t;
  if (nounmapget(&f->building, key)) {
    fordfail(err, "ford: cycle at cast", C2(a, b));
    return 0;
  }
  nounmapput(&f->building, key, YES);
  noun cur = fordcur;
  fordcur = key;
  t = fordcastx(p, f, a, b, err);
  fordcur = cur;
  nounmapput(&f->building, key, 0);
  if (t && !aborted) nounmapput(&f->built, key, t);
  return t;
}

// the type of the file at pax in the desk as mark mak, as +cast-path
noun fordfile(parser *p, fordenv *f, noun mak, noun pax, bufout *err) {
  fordneeds(f, pax);
  s8 src;
  if (isatom(pax) || !srcread(p->a, deskfile(p->a, f, pax), &src)) {
    fordfail(err, "ford: no file", pax);
    return 0;
  }
  noun mok = pax;
  while (iscell(tl(mok))) mok = tl(mok);
  mok = hd(mok);
  noun t;
  if (mok == K("hoon")) {
    t = C3(K("atom"), K("t"), nul);
  } else if (mok == K("mime")) {
    fordfail(err, "ford: unsupported mark", mok);
    return 0;
  } else {
    // validated by the mark, as +page-to-cage
    noun nav = fordnave(p, f, mok, err);
    if (!nav) return 0;
    t = fordslam(p, fordslap(p, nav, C2(K("limb"), K("vale"))), K("noun"));
  }
  if (t && mok != mak) {
    noun gat = fordcast(p, f, mok, mak, err);
    if (!gat) return 0;
    t = fordslam(p, gat, t);
  }
  if (!t) fordfail(err, "ford: cannot read", pax);
  return t;
}

// the directory at a path in the desk
char *deskdir(arena *a, fordenv *f, noun pax) {
  bytes b = {0};
  for (char *c = f->desk; *c; c++) *push(&b, a) = (u8)*c;
  for (noun l = pax; iscell(l); l = tl(l)) {
    *push(&b, a) = '/';
    for (size i = 0; i < alen(hd(l)); i++) *push(&b, a) = abyte(hd(l), i);
  }
  *push(&b, a) = 0;
  return (char*)b.data;
}

// the type of the map of the files directly in the directory at pax,
// each nesting in spec in sut, as the %arch case of +bush-to-vase
noun fordarch(parser *p, fordenv *f, noun sut, noun spec, noun pax, bufout *err) {
  typer u = {0};
  u.p = p;
  noun val = play(&u, sut, C2(K("kttr"), spec));
  noun typ = val ? play(&u, sut, C2(K("kttr"), C3(K("make"), C2(K("wing"), C2(K("map"), nul)),
                                                  C2(C3(K("base"), K("atom"), K("ta")), C2(spec, nul))))) : 0;
  if (!typ) {
    fordfail(err, "ford: cannot build", spec);
    return 0;
  }
  char **names;
  size n = oslistdir(p->a, deskdir(p->a, f, pax), &names);
  for (size i = 0; i < n; i++) {
    size k = 0;
    while (names[i][k]) k++;
    if (k < 6 || !streq(names[i] + k - 5, ".hoon")) continue;
    noun nom = atombytes(p->a, (u8*)names[i], k - 5);
    noun fil = weld(p, pax, C3(nom, K("hoon"), nul));
    s8 src;
    if (!srcread(p->a, deskfile(p->a, f, fil), &src)) continue;
    noun t = fordbuild(p->a, f, fil, err);
    if (!t) return 0;
    b32 ok = 0;
    if (!nest(&u, val, 0, t, &ok) || !ok) {
      fordfail(err, "ford: nest fail in", fil);
      return 0;
    }
  }
  return typ;
}

// the subject of a file with a pile: zuse with each import in front of
// it in turn, as the %hoon case of +bush-to-vase; 0 if one can't be
// built
noun fordsubjectx(parser *p, fordenv *f, noun pil, bufout *err);

noun fordsubject(parser *p, fordenv *f, noun pil, bufout *err) {
  char *at = fordat;
  fordat = srcfiles.data[(p->root ? p->root : p)->file].name;
  noun r = fordsubjectx(p, f, pil, err);
  fordat = at;
  return r;
}

noun fordsubjectx(parser *p, fordenv *f, noun pil, bufout *err) {
  noun sut = f->zuse;
  noun l = pil;
  for (i32 k = 0; k < 7; k++, l = tl(l)) {
    for (noun i = hd(l); iscell(i); i = tl(i)) {
      noun x = hd(i), face = hd(x), t = 0;
      if (k < 2) {
        // /- and /+: [face=(unit term) name]
        face = isatom(hd(x)) ? 0 : tl(hd(x));
        noun pax = fitpath(p, f, k ? "lib" : "sur", tl(x));
        if (!pax) {
          fordfail(err, k ? "ford: no /lib file for" : "ford: no /sur file for", tl(x));
          return 0;
        }
        t = fordbuild(p->a, f, pax, err);
      } else if (k == 2) {
        t = fordbuild(p->a, f, weld(p, tl(x), C2(K("hoon"), nul)), err);
      } else if (k == 3) {
        t = fordarch(p, f, sut, hd(tl(x)), tl(tl(x)), err);
      } else if (k == 4) {
        t = fordnave(p, f, tl(x), err);
      } else if (k == 5) {
        t = fordcast(p, f, hd(tl(x)), tl(tl(x)), err);
      } else {
        t = fordfile(p, f, hd(tl(x)), tl(tl(x)), err);
      }
      if (!t) return 0;
      if (face) t = C3(K("face"), face, t);
      sut = C3(K("cell"), t, sut);
    }
  }
  return sut;
}

// the type of the desk file at pax, as clay's +build-file; 0 if it
// can't be built, with the reason in err
noun fordbuildx(arena *a, fordenv *f, noun pax, bufout *err);

noun fordbuild(arena *a, fordenv *f, noun pax, bufout *err) {
  fordneeds(f, pax);
  noun t = nounmapget(&f->built, pax);
  if (t) return t;
  noun cur = fordcur;
  fordcur = pax;
  t = fordbuildx(a, f, pax, err);
  fordcur = cur;
  return t;
}

noun fordbuildx(arena *a, fordenv *f, noun pax, bufout *err) {
  if (nounmapget(&f->building, pax)) {
    fordfail(err, "ford: import cycle at", pax);
    return 0;
  }
  s8 src;
  if (!srcread(a, deskfile(a, f, pax), &src)) {
    fordfail(err, "ford: no file", pax);
    return 0;
  }
  nounmapput(&f->building, pax, YES);
  parser *p = new(a, parser, 1);
  *p = newparser(a, src);
  p->file = srcadd(deskfile(a, f, pax), pax, src);
  noun pil = pilerule(p, pax);
  noun sut = pil ? fordsubject(p, f, pil, err) : 0;
  noun r = 0;
  if (!pil) {
    fordfail(err, "ford: syntax error in", pax);
  } else if (sut) {
    minter m = {0};
    m.u.p = p;
    m.vet = 1;
    noun gen = tl(tl(tl(tl(tl(tl(tl(pil)))))));
    errnew(errnone);
    if (!(r = mint(&m, sut, K("noun"), gen))) {
      errprint(err, &m, srcfiles.data[p->file].name);
      fordfail(err, "ford: cannot compile", pax);
    }
  }
  nounmapput(&f->building, pax, 0);
  if (!r) return 0;
  if (!aborted) nounmapput(&f->built, pax, hd(r));
  return hd(r);
}

// Compiling files and the kernel

noun deskpath(arena *a, char *rel, size len);

// the path of a file from its sys directory, /sys/lull/hoon for
// arvo/sys/lull.hoon; 0 if it isn't in one
noun syspath(arena *a, char *path) {
  size n = (size)__builtin_strlen(path), at = -1;
  for (size i = 0; i + 4 <= n; i++) {
    if ((i == 0 || path[i-1] == '/') && path[i] == 's' && path[i+1] == 'y' && path[i+2] == 's'
        && path[i+3] == '/') {
      at = i;
    }
  }
  if (at >= 0) return deskpath(a, path + at, n - at);
  // in a directory not named sys: /sys/name/hoon
  size s = n;
  while (s > 0 && path[s-1] != '/' && path[s-1] != '\\') s--;
  noun r = deskpath(a, path + s, n - s);
  return cons(a, atomcstr(a, "sys"), r);
}

// the type of a file, or with expr 1 of the text in path, compiled
// against sut, with expr 2 parsed with its sys path; 0 on failure
noun mintfile(arena *a, bufout *err, char *path, i32 expr, noun sut) {
  s8 src;
  if (expr == 1) {
    src = (s8){(u8*)path, (size)__builtin_strlen(path)};
  } else if (!osreadfile(a, path, &src)) {
    append(err, S("cannot read "));
    append(err, (s8){(u8*)path, (size)__builtin_strlen(path)});
    append(err, S("\n"));
    return 0;
  }
  parser *p = new(a, parser, 1);
  *p = newparser(a, src);
  p->file = srcadd(expr == 1 ? "-e" : path, 0, src);
  if (expr == 2 && !(p->wer = syspath(a, path))) {
    append(err, S("not in a sys directory: "));
    append(err, (s8){(u8*)path, (size)__builtin_strlen(path)});
    append(err, S("\n"));
    return 0;
  }
  srcfiles.data[p->file].wer = p->wer;
  size pos = 0;
  noun gen = vest(p, &pos);
  minter m = {0};
  m.u.p = p;
  m.u.fan = 0;
  m.vet = 1;
  m.rib = 0;
  errnew(errnone);
  noun r = gen ? mint(&m, sut, atomcstr(a, "noun"), gen) : 0;
  if (!r) {
    if (gen) errprint(err, &m, path);
    append(err, S("cannot compile "));
    append(err, (s8){(u8*)path, (size)__builtin_strlen(path)});
    append(err, S("\n"));
    return 0;
  }
  return hd(r);
}

// make the kernel compiled from hoon.hoon at path the compiling kernel,
// as the ivory pill builds it: parsed as +rain does with the path
// /sys/hoon/hoon, compiled against %noun and run. In it, a gate that
// makes lazy batteries as +laze does with a +ut door set as given
b32 kernelload(arena *a, bufout *err, char *path) {
  s8 src;
  if (!osreadfile(a, path, &src)) {
    append(err, S("cannot read "));
    append(err, (s8){(u8*)path, (size)__builtin_strlen(path)});
    append(err, S("\n"));
    return 0;
  }
  parser *p = new(a, parser, 1);
  *p = newparser(a, src);
  p->wer = C4(K("sys"), K("hoon"), K("hoon"), nul);
  p->file = srcadd(path, p->wer, src);
  size pos = 0;
  noun gen = vest(p, &pos);
  minter m = {0};
  m.u.p = p;
  m.vet = 1;
  noun r = gen ? mint(&m, K("noun"), K("noun"), gen) : 0;
  noun kern = r ? nock(p, nul, tl(r)) : 0;
  char *txt =
    "=/  dor  ut\n"
    "|=  [s=type f=(set [type hoon]) r=(set [type type hoon]) v=? n=(unit term) h=poly d=(map term tome)]\n"
    "=.  dor  dor(sut s, fan f, rib r, vet v)\n"
    "(laze:dor n h d)\n";
  parser q = newparser(a, (s8){(u8*)txt, (size)__builtin_strlen(txt)});
  pos = 0;
  noun lgen = kern ? vest(&q, &pos) : 0;
  minter lm = {0};
  lm.u.p = &q;
  lm.vet = 1;
  noun l = lgen ? mint(&lm, hd(r), K("noun"), lgen) : 0;
  noun gate = l ? nock(p, kern, tl(l)) : 0;
  if (!gate) {
    append(err, S("cannot make a kernel of "));
    append(err, (s8){(u8*)path, (size)__builtin_strlen(path)});
    append(err, S("\n"));
    return 0;
  }
  lazer = gate;
  return 1;
}

// a file in a desk, like app/foo.hoon, as its path in the desk,
// /app/foo/hoon
noun deskpath(arena *a, char *rel, size len) {
  nouns ks = {0};
  size s = 0;
  for (size i = 0; i <= len; i++) {
    if (i < len && rel[i] != '/') continue;
    // the last part is a name and an extension
    size dot = i;
    if (i == len) {
      for (size k = i; k > s; k--) {
        if (rel[k-1] == '.') {
          dot = k - 1;
          break;
        }
      }
    }
    if (dot > s) *push(&ks, a) = atombytes(a, (u8*)rel + s, dot - s);
    if (dot < i) *push(&ks, a) = atombytes(a, (u8*)rel + dot + 1, i - dot - 1);
    s = i + 1;
  }
  noun r = nul;
  for (size i = ks.len - 1; i >= 0; i--) r = cons(a, ks.data[i], r);
  return r;
}


// The operating system is the host's: see hc.c.
