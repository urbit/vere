/// @file
///
/// Native code for u3nc programs, by copy and patch.  Included by
/// nock-compile.c in a build with U3NC_JIT.  Not a standalone header.
///
/// The bodies of the opcodes (nock-compile-burn.c) are compiled a second
/// time, apart, as stencils: each the function of one opcode, whose
/// operands, and the code it goes to next, are undefined symbols, the
/// holes.  A script (jit/stencils.py) reads the machine code of each out
/// of that object, with its relocations, into nock-stencils.h.  To lower
/// a program is to lay the stencils of its ops out one after another and
/// to fill the holes: a slot becomes a displacement from the register that
/// holds the slots, an atom an immediate, a branch a jump to the code of
/// its target, and the jump to the next op, where a stencil ends in one,
/// is dropped: the next op follows.
///
/// The code runs as the interpreter's opcode functions do, with the same
/// state in the same registers, on the same frames, so the two mix: a
/// program is entered through the pointer in it (u3nc_prog.nat_f), which
/// is its native code or the stub that runs its bytecode, and a frame says
/// which of the two to go back to.  A program gets native code when it has
/// started often enough as bytecode (u3nc_Hot): native code is some tens of
/// times the size of the bytecode, and code that isn't hot runs faster out
/// of the bytecode, which stays in the caches.  The ops too big to copy at
/// every site call bodies all their sites share (nock-compile-burn.c).
/// Nothing in the code names its program: the literals and the call sites
/// are read through the program register, so the code goes with a program
/// when it is copied to a senior road.
///
/// The code is this process's: a restored program starts over from the
/// stubs (_nc_prog_fix()).  x86-64 ELF only: the stencils want addresses
/// that fit in 32 bits, of the code they are copied to and of the binary
/// they call into.

#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

/* nj_rel: a hole in a stencil.
*/
enum { nj_abs32, nj_abs32s, nj_pc32, nj_abs64 };
enum { nj_slot, nj_value, nj_next, nj_jump, nj_ext, nj_rod };

typedef struct {
  c3_h    off_h;    //  where, in the code of the stencil
  c3_y    typ_y;    //  how it is written: nj_abs32..
  c3_y    sym_y;    //  what goes in: nj_slot..
  c3_h    idx_h;    //  which: the operand, the external symbol (_nj_ext_v),
                    //  or the offset in the constants (_nj_rod_y)
  int32_t add_i;    //  added to it
} nj_rel;

/* nj_sten: the stencil of an opcode.
*/
typedef struct {
  c3_h cod_h;       //  its code in _nj_cod_y
  c3_h len_h;       //    and its length
  c3_h rel_h;       //  its holes in _nj_rel_u
  c3_h ren_h;       //    and their number
  c3_y tal_y;       //  ends in a jump to the next op, of 5 bytes
  const c3_c* nam_c;  //  the opcode, for profiles
} nj_sten;

#include "nock-stencils.h"

/* nj_blob: the native code of a program, in the arena, the code after
**          this header.
*/
typedef struct _nj_blob {
  c3_d             len_d;   //  bytes, with the header
  struct _nj_blob* nex_u;   //  the next free one
} nj_blob;

#define _nj_are_z  ((c3_z)1 << 28)

static FILE*    _nj_map_u;  //  /tmp/perf-PID.map, the symbols of the code
                            //  for perf, if U3NC_JIT_MAP is set
static c3_y*    _nj_are_y;  //  the arena, below 2G
static c3_z     _nj_top_z;  //  bytes of it handed out
static nj_blob* _nj_fre_u;  //  freed blobs

/* _nj_alloc(): room for len_w bytes of native code.
*/
static c3_y*
_nj_alloc(c3_w len_w)
{
  nj_blob** pre_u;
  nj_blob*  blo_u;
  c3_d      len_d = c3_align(sizeof(nj_blob) + len_w, 16, C3_ALGHI);

  if ( !_nj_are_y ) {
    _nj_are_y = mmap(0, _nj_are_z, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT | MAP_NORESERVE,
                     -1, 0);
    u3_assert( MAP_FAILED != (void*)_nj_are_y );

    if ( getenv("U3NC_JIT_MAP") ) {
      c3_c pax_c[64];
      snprintf(pax_c, sizeof(pax_c), "/tmp/perf-%d.map", (int)getpid());
      _nj_map_u = fopen(pax_c, "w");
    }
  }

  for ( pre_u = &_nj_fre_u; (blo_u = *pre_u); pre_u = &(blo_u->nex_u) ) {
    if ( (blo_u->len_d >= len_d) && (blo_u->len_d <= (2 * len_d)) ) {
      *pre_u = blo_u->nex_u;
      return (c3_y*)(blo_u + 1);
    }
  }

  u3_assert( (_nj_top_z + len_d) <= _nj_are_z );
  blo_u = (nj_blob*)(_nj_are_y + _nj_top_z);
  blo_u->len_d = len_d;
  _nj_top_z   += len_d;

  return (c3_y*)(blo_u + 1);
}

/* _nj_has(): whether a program has native code for a road direction.
*/
static inline c3_t
_nj_has(const u3nc_prog* pog_u, c3_y dir_y)
{
  return pog_u->nat_f[dir_y]
      != ( dir_y ? (void*)_nc_burn_south_cold : (void*)_nc_burn_north_cold );
}

/* _nj_keep(): the native code of pog_u goes to its copy gop_u, on a
**             senior road: pog_u is left behind with its road.
*/
static void
_nj_keep(u3nc_prog* gop_u, const u3nc_prog* pog_u)
{
  gop_u->nat_f[0] = pog_u->nat_f[0];
  gop_u->nat_f[1] = pog_u->nat_f[1];
}

/* _nj_lose(): free the native code of a program.
*/
static void
_nj_lose(u3nc_prog* pog_u)
{
  c3_y dir_y;

  for ( dir_y = 0; dir_y < 2; dir_y++ ) {
    if ( _nj_has(pog_u, dir_y) ) {
      nj_blob* blo_u = (nj_blob*)pog_u->nat_f[dir_y] - 1;

      blo_u->nex_u = _nj_fre_u;
      _nj_fre_u    = blo_u;
    }
  }
}

/* nj_gen: the lowering of a program: where the code goes, or 0 while it
**         is being measured.
*/
typedef struct {
  u3nc_prog* pog_u;   //  the program
  c3_y       dir_y;   //  the road direction: 0 north, 1 south
  c3_y*      cod_y;   //  its code, or 0
  c3_w       off_w;   //  the length of the code so far
  c3_w*      map_w;   //  offset of an op in the bytecode -> in the code
} nj_gen;

/* _nj_put(): the stencil cod_h, its holes filled with the operands and,
**            for a branch, with the code of the op at tar_h.
*/
static void
_nj_put(nj_gen* gen_u, c3_h cod_h, c3_h a_h, c3_h b_h, c3_h c_h, c3_h tar_h)
{
  const nj_sten* ste_u = &_nj_sten_u[gen_u->dir_y][cod_h];
  c3_h           lon_h = ste_u->len_h - (ste_u->tal_y ? 5 : 0);
  c3_h           arg_h[3] = { a_h, b_h, c_h };

  u3_assert( ste_u->len_h );

  if ( gen_u->cod_y ) {
    c3_y*         out_y = gen_u->cod_y + gen_u->off_w;
    c3_y*         jum_y = gen_u->cod_y + gen_u->map_w[tar_h];
    const nj_rel* rel_u = _nj_rel_u + ste_u->rel_h;
    c3_h          i_h;

    memcpy(out_y, _nj_cod_y + ste_u->cod_h, lon_h);
    if ( _nj_map_u ) {
      fprintf(_nj_map_u, "%lx %x nj_%s\n", (unsigned long)(uintptr_t)out_y,
              lon_h, ste_u->nam_c);
    }

    for ( i_h = 0; i_h < ste_u->ren_h; i_h++, rel_u++ ) {
      c3_y* her_y = out_y + rel_u->off_h;
      c3_ds val_ds;

      if ( rel_u->off_h >= lon_h ) {
        continue;
      }

      switch ( rel_u->sym_y ) {
        default: u3_assert(0);
        case nj_slot:  val_ds = sizeof(u3_noun) * arg_h[rel_u->idx_h];  break;
        case nj_value: val_ds = arg_h[rel_u->idx_h];                    break;
        case nj_next:  val_ds = (uintptr_t)(out_y + lon_h);             break;
        case nj_jump:  val_ds = (uintptr_t)jum_y;                       break;
        case nj_ext:   val_ds = (uintptr_t)_nj_ext_v[rel_u->idx_h];     break;
        case nj_rod:   val_ds = (uintptr_t)(_nj_rod_y + rel_u->idx_h);  break;
      }
      val_ds += rel_u->add_i;

      switch ( rel_u->typ_y ) {
        default: u3_assert(0);

        case nj_abs32: {
          c3_h val_h = val_ds;
          u3_assert( val_ds == (c3_ds)val_h );
          memcpy(her_y, &val_h, 4);
        } break;

        case nj_pc32:
        case nj_abs32s: {
          int32_t val_i;

          if ( nj_pc32 == rel_u->typ_y ) {
            val_ds -= (uintptr_t)her_y;
          }
          val_i = val_ds;
          u3_assert( val_ds == (c3_ds)val_i );
          memcpy(her_y, &val_i, 4);
        } break;

        case nj_abs64: {
          memcpy(her_y, &val_ds, 8);
        } break;
      }
    }
  }

  gen_u->off_w += lon_h;
}

/* _nj_code(): the code of the program, all its ops: measured, if there is
**             nowhere to put it yet.  A call, a tail call and a call of a
**             total array jet are made of pieces, one per argument, from
**             the site; every other op is its stencil.
*/
static void
_nj_code(nj_gen* gen_u)
{
  u3nc_prog*  pog_u = gen_u->pog_u;
  const c3_y* byc_y = pog_u->byc_u.ops_y;
  c3_h        len_h = pog_u->byc_u.len_h, ip_h, arg_h[3] = {0};

  gen_u->off_w = 0;

  for ( ip_h = 0; ip_h < len_h; ) {
    c3_y fam_y, kon_y, wid_y, num_y, i_y;

    gen_u->map_w[ip_h] = gen_u->off_w;

    if ( BOM == byc_y[ip_h] ) {
      ip_h++;
      _nj_put(gen_u, BOM, 0, 0, 0, 0);
      continue;
    }

    fam_y = _nc_decode(byc_y, &ip_h, &kon_y, &wid_y);
    num_y = _nc_fam[fam_y].imm_y + _nc_fam[fam_y].src_y
          + _nc_fam[fam_y].dst_y + _nc_fam[fam_y].tar_y;
    for ( i_y = 0; i_y < num_y; i_y++ ) {
      arg_h[i_y] = _nc_read(byc_y, &ip_h, wid_y);
    }

    if (  (_nc_cal == fam_y) || (_nc_caf == fam_y)
       || ((_nc_jmp == fam_y) && !kon_y) )
    {
      const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[arg_h[0]]);
      const c3_h*      sot_h = pog_u->sot_u.sot_h + dir_u->sot_h;
      const c3_y*      kos_y = pog_u->sot_u.kon_y + dir_u->sot_h;
      c3_h             j_h;

      if ( _nc_cal == fam_y ) {
        _nj_put(gen_u, CALB, arg_h[0], 0, 0, 0);
        for ( j_h = 0; j_h < dir_u->len_h; j_h++ ) {
          _nj_put(gen_u, kos_y[j_h] ? ARGK : ARGB, sot_h[j_h], j_h, 0, 0);
        }
        _nj_put(gen_u, CALE, dir_u->des_h, dir_u->len_h, 0, 0);
      }
      else if ( _nc_jmp == fam_y ) {
        _nj_put(gen_u, JMPB, arg_h[0], dir_u->len_h, 0, 0);
        for ( j_h = 0; j_h < dir_u->len_h; j_h++ ) {
          _nj_put(gen_u, kos_y[j_h] ? ARGK : ARGB, sot_h[j_h], j_h, 0, 0);
        }
        _nj_put(gen_u, ( dir_u->len_h && (dir_u->len_h <= 4) )
                       ? (JMPE + dir_u->len_h) : JMPE,
                dir_u->len_h, 0, 0, 0);
      }
      else {
        c3_h wor_h = c3_max(dir_u->len_h, 1);

        _nj_put(gen_u, JETB, wor_h, 0, 0, 0);
        for ( j_h = 0; j_h < dir_u->len_h; j_h++ ) {
          _nj_put(gen_u, ARGC, sot_h[j_h], j_h, 0, 0);
        }
        _nj_put(gen_u, JETE, arg_h[0], 0, 0, 0);
        for ( j_h = 0; j_h < dir_u->len_h; j_h++ ) {
          if ( kos_y[j_h] ) {
            _nj_put(gen_u, DRO_B, sot_h[j_h], 0, 0, 0);
          }
        }
        _nj_put(gen_u, JETP, wor_h, dir_u->des_h, 0, 0);
      }
    }
    else {
      _nj_put(gen_u, _nc_bas_y[fam_y] + (2 * kon_y),
              arg_h[0], arg_h[1], arg_h[2], num_y ? arg_h[num_y - 1] : 0);
    }
  }

  gen_u->map_w[len_h] = gen_u->off_w;
}

/* _nj_lower(): make the native code of a program for a road direction
**              (0 north, 1 south).
*/
static void
_nj_lower(u3nc_prog* pog_u, c3_y dir_y)
{
  c3_h            len_h = pog_u->byc_u.len_h;
  nj_gen          gen_u = { pog_u, dir_y, 0, 0, 0 };
  c3_w            siz_w;
  struct timespec beg_u, end_u;

  clock_gettime(CLOCK_MONOTONIC, &beg_u);

  //  once to measure, for the jumps ahead; then for the code: after the
  //  last op, which doesn't go on, a trap (ud2)
  //
  gen_u.map_w = c3_malloc(((c3_z)len_h + 1) * sizeof(c3_w));
  _nj_code(&gen_u);
  siz_w = gen_u.off_w;

  gen_u.cod_y = _nj_alloc(siz_w + 2);
  gen_u.cod_y[siz_w]     = 0x0f;
  gen_u.cod_y[siz_w + 1] = 0x0b;
  _nj_code(&gen_u);
  u3_assert( siz_w == gen_u.off_w );

  c3_free(gen_u.map_w);
  pog_u->nat_f[dir_y] = gen_u.cod_y;
  if ( _nj_map_u ) {
    fflush(_nj_map_u);
  }

  clock_gettime(CLOCK_MONOTONIC, &end_u);
  u3nc_Stat.low_d++;
  u3nc_Stat.lob_d += len_h;
  u3nc_Stat.lon_d += siz_w;
  u3nc_Stat.lot_d += ((end_u.tv_sec - beg_u.tv_sec) * 1000000000LL)
                   + (end_u.tv_nsec - beg_u.tv_nsec);
}
