/// @file
/// POSIX loom collisions must fail without replacing unrelated mappings.

#include "noun.h"

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static c3_y* sen_y;
static size_t sen_i;
static c3_i out_i;
static c3_y* old_y;

/* _report_sentinel(): inspect the child's mapping even when boot exits.
**
**   Checking the file alone would miss MAP_FIXED replacing its mapping
**   with private anonymous memory, the failure reported in #1124.
*/
static void
_report_sentinel(void)
{
  c3_y ok_y = 1;
  if ( old_y && (0x5a != *old_y) ) {
    ok_y = 0;
  }
  for ( size_t i_i = 0; i_i < sen_i; i_i++ ) {
    if ( 0xa5 != sen_y[i_i] ) {
      ok_y = 0;
      break;
    }
  }
  if ( 1 != write(out_i, &ok_y, 1) ) {
    _exit(2);
  }
}

/* _test_collision(): occupy a page before initializing or growing the loom.
*/
static c3_o
_test_collision(size_t off_i, c3_o grow_o)
{
  c3_i pip_i[2];
  if ( pipe(pip_i) ) {
    return c3n;
  }

  pid_t pid_i = fork();
  if ( -1 == pid_i ) {
    close(pip_i[0]);
    close(pip_i[1]);
    return c3n;
  }
  if ( !pid_i ) {
    alarm(30);
    close(pip_i[0]);
    out_i = pip_i[1];

    if ( c3y == grow_o ) {
      u3m_init((size_t)1 << 20);
      old_y = (c3_y*)u3_Loom;
      *old_y = 0x5a;
    }

    FILE* fil_u = tmpfile();
    sen_i = (size_t)sysconf(_SC_PAGESIZE);
    void* adr_v = (c3_y*)u3_Loom + off_i;
    if ( !fil_u || ftruncate(fileno(fil_u), sen_i) ) {
      _exit(2);
    }
    sen_y = mmap(adr_v, sen_i, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fileno(fil_u), 0);
    if ( adr_v != sen_y ) {
      _exit(2);
    }
    memset(sen_y, 0xa5, sen_i);
    if ( atexit(_report_sentinel) ) {
      _exit(2);
    }

    u3m_init((size_t)1 << 22);
    exit(0);  // a taken address must not be accepted
  }

  close(pip_i[1]);
  c3_y ok_y = 0;
  ssize_t red_i = read(pip_i[0], &ok_y, 1);
  close(pip_i[0]);
  c3_i sat_i;
  if ( pid_i != waitpid(pid_i, &sat_i, 0) ) {
    return c3n;
  }
  return (  (1 == red_i) && ok_y
         && WIFEXITED(sat_i) && (1 == WEXITSTATUS(sat_i)) ) ? c3y : c3n;
}

static c3_o
_test_owned(void)
{
  void* bas_v = (void*)u3_Loom;
  size_t len_i = (size_t)1 << 22;
  size_t pag_i = (size_t)sysconf(_SC_PAGESIZE);

  //  Repeating or shrinking a claim must keep it reserved. Growing may
  //  acquire only the tail; fixed remapping is restricted to owned bytes.
  //
  u3_assert( c3y == u3m_loom_stake(bas_v, len_i) );
  u3_assert( c3y == u3m_loom_stake(bas_v, len_i) );
  u3_assert( c3y == u3m_loom_stake(bas_v, len_i / 2) );
  void* map_v = mmap(bas_v, len_i, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
  u3_assert( MAP_FAILED != map_v && bas_v != map_v );
  u3_assert( !munmap(map_v, len_i) );

  u3_assert( bas_v == u3m_loom_map(bas_v, len_i, PROT_READ | PROT_WRITE,
                                  MAP_ANON | MAP_PRIVATE, -1, 0) );
  *((c3_y*)bas_v) = 0x5a;
  u3_assert( c3y == u3m_loom_stake(bas_v, len_i * 2) );
  u3_assert( 0x5a == *((c3_y*)bas_v) );

  //  A large 64-bit native reservation can contain the migration source.
  //  Releasing that source must leave the parent reservation continuous.
  //
  void* sou_v = (c3_y*)bas_v + len_i;
  u3_assert( c3y == u3m_loom_stake(sou_v, len_i / 2) );
  u3_assert( c3y == u3m_loom_stake(sou_v, len_i) );
  u3_assert( c3y == u3m_loom_stake(sou_v, len_i / 2) );
  u3_assert( c3n == u3m_loom_stake(sou_v, len_i * 2) );
  void* alt_v = (c3_y*)bas_v + len_i * 4;
  void* ext_v = (c3_y*)bas_v + len_i * 6;
  u3_assert( c3y == u3m_loom_stake(alt_v, len_i) );
  u3_assert( c3n == u3m_loom_stake(ext_v, len_i) );
  u3_assert( !u3m_loom_drop(alt_v) );
  u3_assert( c3y == u3m_loom_stake(ext_v, len_i) );
  u3_assert( !u3m_loom_drop(ext_v) );
  u3_assert( sou_v == u3m_loom_map(sou_v, len_i, PROT_READ | PROT_WRITE,
                                  MAP_ANON | MAP_PRIVATE, -1, 0) );
  *((c3_y*)sou_v) = 0xa5;
  u3_assert( -1 == u3m_loom_drop(bas_v) );
  u3_assert( 0x5a == *((c3_y*)bas_v) && 0xa5 == *((c3_y*)sou_v) );
  u3_assert( !u3m_loom_drop(sou_v) );
  map_v = mmap(sou_v, len_i, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
  u3_assert( MAP_FAILED != map_v && sou_v != map_v );
  u3_assert( !munmap(map_v, len_i) );

  void* tail_v = (c3_y*)bas_v + len_i * 2;
  map_v = mmap(tail_v, pag_i, PROT_READ | PROT_WRITE,
               MAP_ANON | MAP_PRIVATE, -1, 0);
  u3_assert( tail_v == map_v );
  memset(map_v, 0xa5, pag_i);
  u3_assert( MAP_FAILED == u3m_loom_map(tail_v, pag_i, PROT_READ,
                                       MAP_ANON | MAP_PRIVATE, -1, 0) );
  u3_assert( MAP_FAILED == u3m_loom_map((c3_y*)tail_v - pag_i, pag_i * 2,
                                       PROT_READ, MAP_ANON | MAP_PRIVATE,
                                       -1, 0) );
  for ( size_t i_i = 0; i_i < pag_i; i_i++ ) {
    u3_assert( 0xa5 == ((c3_y*)map_v)[i_i] );
  }
  u3_assert( !munmap(map_v, pag_i) );

  u3_assert( !u3m_loom_drop(bas_v) );
  map_v = mmap(bas_v, len_i * 2, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
  u3_assert( bas_v == map_v );
  u3_assert( MAP_FAILED == u3m_loom_map(bas_v, pag_i, PROT_READ,
                                       MAP_ANON | MAP_PRIVATE, -1, 0) );
  u3_assert( !munmap(map_v, len_i * 2) );
  u3_assert( c3y == u3m_loom_stake(bas_v, len_i) );

  //  Reservation must fail cleanly when no virtual address space remains.
  //
  u3_assert( c3n == u3m_loom_stake((void*)(uintptr_t)pag_i,
                                  SIZE_MAX - pag_i * 2 + 1) );
  return c3y;
}

static c3_o
_test_invalid(void)
{
  c3_y* bas_y = (c3_y*)u3_Loom;
  size_t pag_i = (size_t)sysconf(_SC_PAGESIZE);
  size_t len_i = (size_t)1 << 22;
  u3_assert( c3y == u3m_loom_stake(bas_y, len_i) );
  u3_assert( c3n == u3m_loom_stake(bas_y + 1, pag_i) );
  u3_assert( c3n == u3m_loom_stake(bas_y + len_i, pag_i - 1) );
  u3_assert( c3n == u3m_loom_stake(bas_y, 0) );
  u3_assert( c3n == u3m_loom_stake((void*)(UINTPTR_MAX - pag_i + 1),
                                  pag_i * 2) );
  u3_assert( !u3m_loom_drop(bas_y) );
  for ( size_t i_i = 0; i_i < 3; i_i++ ) {
    u3_assert( c3y == u3m_loom_stake(bas_y + i_i * len_i, pag_i) );
  }
  return c3y;
}

static c3_o
_test_source_collision(void)
{
#ifdef VERE64
  void* bas_v = (void*)u3_Loom_h;
#else
  void* bas_v = (void*)u3_Loom_d;
#endif
  size_t pag_i = (size_t)sysconf(_SC_PAGESIZE);
  FILE* fil_u = tmpfile();
  u3_assert( fil_u && !ftruncate(fileno(fil_u), pag_i) );
  c3_y* map_y = mmap(bas_v, pag_i, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fileno(fil_u), 0);
  u3_assert( bas_v == map_y );
  memset(map_y, 0xa5, pag_i);
  u3_assert( c3n == u3m_loom_stake(bas_v, (size_t)1 << 22) );
  u3_assert( MAP_FAILED == u3m_loom_map(bas_v, pag_i, PROT_READ,
                                       MAP_ANON | MAP_PRIVATE, -1, 0) );
  for ( size_t i_i = 0; i_i < pag_i; i_i++ ) {
    u3_assert( 0xa5 == map_y[i_i] );
  }
  u3_assert( !munmap(map_y, pag_i) );
  fclose(fil_u);
  return c3y;
}

static c3_o
_test_reinit(void)
{
  u3m_init((size_t)1 << 22);
  u3m_init((size_t)1 << 20);
  u3m_init((size_t)1 << 23);
  ((c3_y*)u3_Loom)[((size_t)1 << 23) - 1] = 0x5a;
  return c3y;
}

static c3_o
_run(c3_o (*test_f)(void))
{
  pid_t pid_i = fork();
  if ( -1 == pid_i ) {
    return c3n;
  }
  if ( !pid_i ) {
    alarm(30);
    _exit((c3y == test_f()) ? 0 : 1);
  }
  c3_i sat_i;
  return (  (pid_i == waitpid(pid_i, &sat_i, 0))
         && WIFEXITED(sat_i) && !WEXITSTATUS(sat_i) ) ? c3y : c3n;
}

int
main(void)
{
  c3_i fal_i = 0;
  const size_t off_i[] = { 0, (size_t)1 << 21, (size_t)1 << 21 };
  const c3_o grow_o[] = { c3n, c3n, c3y };
  const char* nam_c[] = { "base collision", "partial collision",
                         "conflicting growth" };

  for ( size_t i_i = 0; i_i < 3; i_i++ ) {
    c3_o ok_o = _test_collision(off_i[i_i], grow_o[i_i]);
    fprintf(stderr, "loom: %s: %s\r\n", nam_c[i_i],
                    (c3y == ok_o) ? "ok" : "FAIL (exit or sentinel)");
    fal_i += (c3n == ok_o);
  }
  c3_o (*test_f[])(void) = { _test_owned, _test_reinit, _test_source_collision,
                           _test_invalid };
  const char* ext_c[] = { "ownership, growth, release and exhaustion",
                         "repeated initialization", "migration source collision",
                         "invalid claims leave ownership intact" };
  for ( size_t i_i = 0; i_i < 4; i_i++ ) {
    c3_o ok_o = _run(test_f[i_i]);
    fprintf(stderr, "loom: %s: %s\r\n", ext_c[i_i],
                    (c3y == ok_o) ? "ok" : "FAIL");
    fal_i += (c3n == ok_o);
  }
  return fal_i ? 1 : 0;
}
