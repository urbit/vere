/// @file
///
/// hc, the hoon compiler in hoon.c, built into vere: see hc.h. hoon.c
/// is written to be a whole program, with names like cons and mug, so
/// it's included here with everything in it kept to this file, and what
/// vere calls is defined after it.
///
/// hoon.c is hc's, forked: its nouns are vere's, in the loom, read and
/// made where they are, and nothing is brought across. The operating
/// system is left to this file, and the gate +laze makes and the
/// formulas a compile runs are left to the host. Its memory, apart from
/// the nouns, is in arenas of chunks from u3a_malloc(), on the road the
/// call runs on, and goes with that road: it keeps nothing from one
/// call to the next, and hc_wipe() after each forgets where it was.

#if defined(VERE64) || defined(_WIN32)

//  XX hoon.c's nouns are 32 bits wide, and it has names windows has for
//  other things: with vere's nouns at 64, and on windows, the compiler
//  isn't to be had, and the kernel's own compiles
//
#include "noun.h"
#include "hc.h"

jmp_buf hc_fail;

int      hc_boot(void)                          { return 0; }
void     hc_wipe(void)                          {}
uint32_t hc_cells(void)                         { return 0; }
size_t   hc_bytes(void)                         { return 0; }
void     hc_arm(int arm)                        {}
int      hc_same(hc_noun one, hc_noun two)      { return 0; }
hc_noun  hc_cons(hc_noun hed, hc_noun tel)      { return 0; }
uint8_t* hc_text(size_t len)                    { return 0; }
hc_noun  hc_vest(uint8_t* txt, size_t len)      { return 0; }
hc_noun  hc_pile(uint8_t* txt, size_t len, hc_noun pax) { return 0; }
hc_noun  hc_mint(hc_noun sut, hc_noun gol, hc_noun gen, int vet,
                 hc_noun fan, hc_noun rib, size_t stk) { return 0; }

#else

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <setjmp.h>

#include "noun.h"
#include "hc.h"

#define HC_EMBED 1

//  names of hoon.c's that libc and vere have too
//
#undef assert
#define nice hcnice

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wparentheses"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wchar-subscripts"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wunused-value"
#pragma clang diagnostic ignored "-Wdate-time"
#pragma clang attribute push (__attribute__((internal_linkage)), apply_to = any(function, variable(is_global)))

#include "hoon.c"

//  what's given back when a call is done with it: scratch
//
arena   hcscratch;
b32     hcarmed;

/* The operating system, as hoon.c wants it. Nothing here reads or
** writes files, and running out of memory ends the call, not the
** process.
*/

/* osfail(): leave the compiler, from wherever it is.
*/
void
osfail(void)
{
  if ( !hcarmed ) {
    abort();
  }
  hcarmed = 0;
  longjmp(hc_fail, 1);
}

/* hostalloc(): memory for a chunk of an arena, from the loom.
*/
size hcgot;   // bytes of chunks a call has had, to say

void*
hostalloc(size len)
{
  hcgot += len;
  return u3a_malloc((c3_z)len);
}

/* hostfree(): give a chunk back.
*/
void
hostfree(void* ptr)
{
  u3a_free(ptr);
}

/* osreserve(): there's no address space to reserve.
*/
byte*
osreserve(size cap)
{
  return 0;
}

/* osrelease(): what's released is a table outgrown, in the heap's
** arena: given back if it has a chunk to itself, and if not it goes with
** the road.
*/
void
osrelease(byte* beg, byte* end)
{
  arenafree(&H.perm, beg);
}


/* oswrite(): messages go nowhere.
*/
b32
oswrite(i32 fd, u8* buf, i32 len)
{
  return 1;
}

/* osread(): there's no input.
*/
size
osread(i32 fd, u8* buf, size len)
{
  return 0;
}

/* osreadfile(): there are no files.
*/
b32
osreadfile(arena* a, char* path, s8* out)
{
  return 0;
}

/* oslistdir(): nor directories.
*/
size
oslistdir(arena* a, char* path, char*** out)
{
  return 0;
}

/* oswritefile(): nothing is written.
*/
b32
oswritefile(arena* a, char* path, u8* buf, size len)
{
  return 0;
}

/* osgetenv(): no variable is set.
*/
char*
osgetenv(char* name)
{
  return 0;
}

/* osmkdir(): nothing is made.
*/
void
osmkdir(char* path)
{
}

/* osabspath(): no path is known.
*/
char*
osabspath(arena* a, char* path)
{
  return 0;
}

/* osisdir(): nothing is a directory.
*/
b32
osisdir(char* path)
{
  return 0;
}

/* osexists(): nothing exists.
*/
b32
osexists(char* path)
{
  return 0;
}

/* hostlaze(), hostfrom(), hostmack(): the host's, by the names hoon.c
** has for them.
*/
noun
hostlaze(noun from)
{
  return hc_host_laze(from);
}

noun
hostfrom(noun gate)
{
  return hc_host_from(gate);
}

noun
hostmack(noun core, noun axe)
{
  return hc_host_mack(core, axe);
}

/* hostover(): whether to stop, for hoon.c's [interrupted].
*/
b32
hostover(void)
{
  return hc_host_over();
}

/* hcfresh(): everything hoon.c keeps between calls, as it was when the
** process started, and its heap made over memory that reads as zeros.
**
**   These are all the variables of hoon.c that aren't constant tables:
**   one added there must be added here. `nm hc.o` lists them, as b.
*/
void
hcfresh(void)
{
#define Z(x) __builtin_memset(&(x), 0, (size_t)sizeof(x))
  Z(H);           Z(loose);       Z(termpool);    Z(termsites);
  Z(srcfiles);    Z(srccopy);     Z(srcreader);   Z(armsites);
  Z(armnames);    Z(cwdpath);     Z(hcerr);       Z(stacktop);
  Z(deeps);       Z(opens);       Z(s2);          Z(s3);
  Z(hoonindex);   Z(specindex);   Z(quotemode);   Z(memo);
  Z(playmemo);    Z(lootcache);   Z(nestk);       Z(nestmemo);
  Z(nestcap);     Z(nestlen);     Z(fastnames);   Z(fastparents);
  Z(burps);       Z(lazer);       Z(lazes);       Z(lazememo);
  Z(interrupted); Z(aborted);     Z(nextpoll);    Z(fordcur);
  Z(fordat);      Z(branmemo);
#undef Z

  hcscratch = (arena){0};
  heapinit(&hcscratch);
}

#pragma clang attribute pop
#pragma clang diagnostic pop

jmp_buf hc_fail;

//  K() gives each of its uses a slot in termsites
//
_Static_assert(__COUNTER__ < countof(termsites), "hc: termsites is full");

/* hc_boot(): the compiler is there.
*/
int
hc_boot(void)
{
  return 1;
}

/* hc_wipe(): forget everything a call kept. Its memory was its road's.
*/
void
hc_wipe(void)
{
  hcgot = 0;
  hcfresh();
}

/* hc_cells(): the cells made.
*/
uint32_t
hc_cells(void)
{
  return H.ncells;
}

/* hc_arm(): say whether hc_fail is good to go to.
*/
void
hc_arm(int arm)
{
  hcarmed = arm;
}

/* hc_same(): whether two nouns are equal.
*/
int
hc_same(hc_noun one, hc_noun two)
{
  return nouneq(one, two);
}

/* hc_cons(): the cell of two nouns.
*/
hc_noun
hc_cons(hc_noun hed, hc_noun tel)
{
  return cons(&hcscratch, hed, tel);
}

/* hc_mint(): [type nock] of a hoon, as +mint in +ut.
*/
hc_noun
hc_mint(hc_noun sut,
        hc_noun gol,
        hc_noun gen,
        int     vet,
        hc_noun fan,
        hc_noun rib,
        size_t  stk)
{
  byte   top;
  parser p;
  minter m = {0};
  noun   r;

  hcscratch.beg = hcscratch.dat;

  p = newparser(&hcscratch, (s8){0, 0});
  m.u.p   = &p;
  m.vet   = vet;
  m.u.fan = setguard(&p, fan, 0);
  m.rib   = setguard(&p, rib, 1);

  stacktop    = &top;
  stackmax    = (size)stk;
  interrupted = hostover;
  errnew(errnone);

  r = mint(&m, sut, gol, gen);

  hcscratch.beg = hcscratch.dat;
  return r;
}

/* hc_text(): bytes for hc_vest() to parse.
*/
uint8_t*
hc_text(size_t len)
{
  hcscratch.beg = hcscratch.dat;
  return new(&hcscratch, u8, (size)len + 1);
}

/* hc_vest(): the hoon of a text, as +vest parses it from [1 1].
*/
hc_noun
hc_vest(uint8_t* txt, size_t len)
{
  parser p;
  size   pos = 0;
  noun   r;

  p = newparser(&hcscratch, (s8){txt, (size)len});
  r = vest(&p, &pos);

  hcscratch.beg = hcscratch.dat;
  return r;
}

/* hc_pile(): the pile of a text, as +pile-rule in clay parses it.
*/
hc_noun
hc_pile(uint8_t* txt, size_t len, hc_noun pax)
{
  parser p = newparser(&hcscratch, (s8){txt, (size)len});
  noun   r = pilerule(&p, pax);

  if ( spent(&p) ) {
    r = 0;
  }
  hcscratch.beg = hcscratch.dat;
  return r;
}

/* hc_bytes(): the bytes of chunks the call has had from u3a_malloc().
*/
size_t
hc_bytes(void)
{
  return (size_t)hcgot;
}

#endif /* if defined(VERE64) || defined(_WIN32) */
