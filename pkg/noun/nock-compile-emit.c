/// @file
///
/// Machine code for u3nc programs, by an emitter of our own.  Included by
/// nock-compile.c in a build with U3NC_JIT, beside the stencils
/// (nock-compile-jit.c): u3nc_Emit says which of the two lowers a program.
/// Not a standalone header.
///
/// A program becomes a machine function.  Its slots live in machine
/// registers, the ones no call of a nock function is ever live across;
/// the others live in the function's frame on the machine stack.  A call
/// of a nock function is a machine call: the arguments in the slot
/// registers, in order, the product in a register of its own, the further
/// parts of a product in parts (RTS, RTL) in the slot registers; a return
/// is a machine return, a tail call a jump.  The fast path of an op is in
/// line; the slow path is at the end of the function, out of the way, and
/// calls a helper in C that keeps every register.
///
/// The code runs on a stack of its own, entered through a trampoline
/// (_ne_run()): nock recursion has no bound, the stack C runs on has one.
/// The low end of that stack is a guard that can't be touched, and the
/// fault of a function, or of the C it calls, that runs into it is the
/// signal (%over) a stack overflow is elsewhere (u3nc_fault()).  A bail
/// anywhere is a longjmp, which drops the frames.
///
/// Native functions and bytecode call each other: bytecode enters a
/// program through its record (nj_side.nat_f), which for a program lowered here is
/// the adapter (emit, in nock-compile-burn.c), which goes through the
/// trampoline; native code calls a program that has no code yet through
/// a thunk (_ne_thunk()), which runs the interpreter on it, and which is
/// repointed at the code when the program gets some.
///
/// The code names its program (literals are immediates, the call sites
/// have cells in the code), so it does not go with a program that is
/// copied to a senior road: the copy starts over (_ne_keep()).
///
/// x86-64 only (nock-compile-x64.h); nothing depends on where the binary
/// or the code is.

#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#include "nock-compile-x64.h"

/* ne_ctx: what the code reads of its run, through nx_ctx: made by
**         _ne_run() for the road it enters on.
*/
typedef struct {
  c3_w   lo_w;      //  the first word of the road's heap, as the offset of
                    //  a noun there (_ne_off())
  c3_w   spa_w;     //  its extent: a noun at the offset off is the road's
                    //  own, counted, iff (off - lo) < spa
  c3_w   reu_w;     //  the same, or 0 on the home road: whether a unique
                    //  cell may be reused (HER, TAR)
  c3_w   cel_w;     //  u3a_cell_flag, u3a_direct_max and one less: words
  c3_w   dmx_w;     //  that an instruction on 64-bit words can't hold,
  c3_w   dm1_w;     //  and takes from here
  c3_y*  lim_y;     //  a function that calls finds the stack below
                    //  this once in a run: the run has gone deep, and the
                    //  pages it touched are given back later
  u3a_road* rod_u;  //  the road
} ne_ctx;

#define _ne_ctx_lo   ((int32_t)offsetof(ne_ctx, lo_w))
#define _ne_ctx_spa  ((int32_t)offsetof(ne_ctx, spa_w))
#define _ne_ctx_reu  ((int32_t)offsetof(ne_ctx, reu_w))
#define _ne_ctx_cel  ((int32_t)offsetof(ne_ctx, cel_w))
#define _ne_ctx_dmx  ((int32_t)offsetof(ne_ctx, dmx_w))
#define _ne_ctx_dm1  ((int32_t)offsetof(ne_ctx, dm1_w))
#define _ne_ctx_lim  ((int32_t)offsetof(ne_ctx, lim_y))
#define _ne_ctx_rod  ((int32_t)offsetof(ne_ctx, rod_u))

/* the word of the loom, for the code:
**
**   _ne_wb:  its bytes: a slot of the frame, an argument, a part;
**   _ne_off(): the offset of a noun, i.e. the noun shifted left by two
**              (its flags gone), from the word offset the allocator
**              knows it by: the same number on the 32-bit loom, whose
**              references are word offsets shifted right by two, and
**              four times it on the 64-bit one, whose references are
**              not shifted (_nx_os);
**   _ne_c_use.. _ne_c_tel: the fields of a cell.
*/
#define _ne_wb     ((int32_t)sizeof(u3_noun))
#ifdef VERE64
#  define _ne_off(pos)  ((c3_w)(pos) << 2)
#else
#  define _ne_off(pos)  ((c3_w)(pos))
#endif
#define _ne_c_use  ((int32_t)offsetof(u3a_cell, use_w))
#define _ne_c_mug  ((int32_t)offsetof(u3a_cell, mug_w))
#define _ne_c_hed  ((int32_t)offsetof(u3a_cell, hed))
#define _ne_c_tel  ((int32_t)offsetof(u3a_cell, tel))

/* ne_base: the start of the arena: what all code reads, within reach of a
**          32-bit distance.
*/
enum {
  ne_h_gain, ne_h_lose, ne_h_bail, ne_h_link, ne_h_interp, ne_h_inc,
  ne_h_dec, ne_h_add, ne_h_cons, ne_h_sing, ne_h_ret, ne_h_tin,
  ne_h_box, ne_h_nok, ne_h_cap, ne_h_cam, ne_h_csm, ne_h_save, ne_h_hmp,
  ne_h_tdrop, ne_h_hcp, ne_h_hlp, ne_h_hsp, ne_h_hse, ne_h_hdp, ne_h_hde,
  ne_h_spy, ne_h_cac, ne_h_mono, ne_h_deep,
  ne_h_last
};

#define _ne_arg_max  64     //  arguments of a function, at most

typedef struct {
  void*    hel_v[48];              //  the helpers, by ne_h_
} ne_base;

/* ne_data: what the code writes: beside the arena, within the same reach,
**          and never executable.
*/
typedef struct {
  c3_d     cnt_d[64];              //  counting: ops run, by family
  u3_noun  arg[_ne_arg_max];       //  the arguments past the registers,
                                   //  from a call to the callee's start
  u3nc_prog* gen_u;                //  the program to interpret, for a
                                   //  call that found no room for a thunk;
                                   //  or 0:
  u3_noun  fol;                    //  the formula to run in u3n, ours
} ne_data;

/* ne_blob: a piece of the arena: the code of a program, or a thunk,
**          which follows it; or a free piece.  The pieces tile the arena
**          from its start to its top, each knowing the length of the one
**          before it, so that a freed piece joins its free neighbours.
*/
typedef struct {
  c3_h len_h;   //  bytes, with this header, a multiple of 16; bit 0: free
  c3_h pre_h;   //  bytes of the piece before it, or 0 for the first
  c3_h nex_h;   //  free: the next free piece of its bin, by offset, or 0
  c3_h bak_h;   //  free: the one before it in the bin, or 0
} ne_blob;

#define _ne_bin_w  64       //  bins of free pieces, by size

/* ne_cell: a call site, in the code of its program: where it calls, and
**          which it is, for the linking (_ne_h_link()).
*/
typedef struct {
  void*  tar_v;     //  the code of the callee, a thunk, or the link stub
  c3_d   sit_d;     //  the index of the site; bit 32: it is a tail call
  c3_y*  ins_y;     //  the instruction that calls through the cell, six
                    //  bytes: once linked, it calls directly
} ne_cell;


//  The arena is one piece of memory mapped twice: where it is executed
//  and can't be written, and where it is written and can't be executed.
//  Code is made and patched through the second, and every address kept
//  of it, in a record, a cell or an instruction, is of the first.
//
static c3_y*    _ne_exe_y;          //  the arena, where it runs
static c3_y*    _ne_wri_y;          //    and where it is written
static c3_z     _ne_are_z;          //  its size, or less: how far it may
                                    //  grow (u3nc_emit_limit())
static c3_z     _ne_map_z;          //  its size
static c3_z     _ne_top_z;          //  the end of its last piece
static c3_z     _ne_bot_z;          //  the end of what is never freed: the
                                    //  table and the trampolines
static c3_h     _ne_las_h;          //  the length of its last piece, or 0
static c3_h     _ne_bin_h[_ne_bin_w];  //  free pieces, by offset
static c3_z     _ne_use_z;          //  bytes of pieces in use
static c3_z     _ne_pek_z;          //  the most the top has been
static c3_y     _ne_att_y;          //  why the code is to be evicted when
                                    //  no machine function runs, and
                                    //  nothing more is lowered till then:
#define _ne_att_ful  1              //    a piece could not be had
#define _ne_att_bus  2              //    a change of the arena is under
                                    //    way; or, seen from outside one,
                                    //    was abandoned (_ne_open())
static c3_d     _ne_evi_d;          //  evictions
static ne_base* _ne_bas_u;          //  its start (where it runs)
static ne_data* _ne_dat_u;          //  what the code writes
static c3_t     _ne_off_t;          //  the arena can't be had: nothing is
                                    //  lowered

//  _ne_w(ptr): where to write what runs at ptr
//
#define _ne_w(ptr)                                                              \
  ((void*)(_ne_wri_y + ((c3_y*)(ptr) - _ne_exe_y)))
//  The code runs on a stack of its own.  Its low end can't be touched:
//  a function, or the C it calls, that runs into it faults, and the
//  fault is the bail a stack out of room gets elsewhere (u3nc_fault()).
//
static c3_y*    _ne_stk_y;          //  the stack of the code: its low end
static c3_z     _ne_stk_z;          //    and its size
static c3_z     _ne_gua_z;          //    and how much of it is the guard
static c3_z     _ne_kep_z;          //  how deep a run may go before its
                                    //  pages are given back
static c3_t     _ne_dep_t;          //  a run has gone deeper than that
                                    //  since the pages were given back
static c3_t     _ne_now_t;          //  the run from C that is under way
                                    //  has, or else the last one had
static c3_d     _ne_tri_d;          //  times the pages were given back
static c3_t     _ne_cnt_t;          //  the code counts its ops, for the tests

//  the trampolines: C to native code (_ne_run()), and native code to the
//  interpreter (through a thunk)
//
static ne_duo (*_ne_ent_f)(void* cod_v, u3_noun* arg, ne_ctx* ctx_u,
                            c3_y* new_y);
static c3_y* _ne_tin_y;
static c3_y* _ne_gen_y;             //  the same, the program from ne_data


/* _ne_has(): whether a program has code made here.
*/
static inline c3_t
_ne_has(const u3nc_prog* pog_u)
{
  const nj_side* sid_u = _nj_side(pog_u);

  return sid_u && sid_u->emi_f && (_ne_fail != sid_u->emi_f);
}

/*  the helpers: the slow paths.  Each keeps every register but the one
**  the product comes in (and nx_t5).
*/
static _nc_cold void
_ne_h_bail(c3_w mot_w)
{
  u3m_bail(mot_w);
}

static _nc_cold void
_ne_h_gain(u3_noun som)
{
  u3a_gain(som);
}

//  the code has found the noun on the road and its count at 1: straight
//  to the loop that frees, as the bytecode goes
//
static _nc_cold void
_ne_h_lose(u3_noun som)
{
  u3a_lose_free(som);
}

static _nc_cold u3_noun
_ne_h_inc(u3_noun x, c3_w kon_w)
{
  return u3i_vint(kon_w ? x : u3k(x));
}

static _nc_cold u3_noun
_ne_h_dec(u3_noun x, c3_w kon_w)
{
  u3_noun pro;

  if ( c3n == u3ud(x) ) {
    return u3m_bail(c3__fail);
  }
  pro = u3qa_dec(x);
  if ( kon_w ) {
    u3z(x);
  }
  return pro;
}

static _nc_cold u3_noun
_ne_h_add(u3_noun x, u3_noun y, c3_w kon_w)
{
  u3_noun pro;

  if ( (c3n == u3ud(x)) || (c3n == u3ud(y)) ) {
    return u3m_bail(c3__fail);
  }
  pro = u3qa_add(x, y);
  if ( kon_w & 1 ) {
    u3z(x);
  }
  if ( kon_w & 2 ) {
    u3z(y);
  }
  return pro;
}

static _nc_cold u3_noun
_ne_h_cons(u3_noun h, u3_noun t)
{
  return u3nc(h, t);
}

static _nc_cold c3_w
_ne_h_sing(u3_noun a, u3_noun b)
{
  return u3r_sing(a, b);
}

static _nc_cold u3_noun
_ne_h_box(u3_noun sap, u3_noun pro)
{
  return _nc_box(sap, pro);
}

//  the hints
//
static _nc_cold void
_ne_h_hmp(u3nc_prog* pog_u, c3_w sit_w, u3_noun* arg)
{
  u3nc_dire*  dir_u = &(pog_u->dir_u.dat_u[sit_w]);
  const c3_y* kon_y = pog_u->sot_u.kon_y + dir_u->sot_h;
  u3_noun     lis   = u3_nul;
  c3_h        i_h;

  for ( i_h = dir_u->len_h; i_h-- > 0; ) {
    lis = u3nc(kon_y[i_h] ? arg[i_h] : u3k(arg[i_h]), lis);
  }
  u3t_push(u3nt(c3__meaf, u3k(dir_u->bell), lis));
}

static _nc_cold void
_ne_h_tdrop(void)
{
  u3t_drop();
}

static _nc_cold void
_ne_h_hcp(u3_noun lit)
{
  u3t_push(u3nc(c3__meac, u3k(lit)));
}

static _nc_cold void
_ne_h_hlp(u3_noun lit)
{
  u3t_push(u3k(lit));
}

static _nc_cold u3_noun
_ne_h_hsp(u3_noun tag)
{
  return _nc_hilt_fore(tag);
}

static _nc_cold void
_ne_h_hse(u3_noun tag, u3_noun tok)
{
  _nc_hilt_hind(tag, tok);
}

static _nc_cold u3_noun
_ne_h_hdp(u3_noun tag, u3_noun clu)
{
  return _nc_hint_fore(tag, clu);
}

static _nc_cold void
_ne_h_hde(u3_noun tag, u3_noun tok)
{
  _nc_hint_hind(tag, tok);
}

static _nc_cold u3_noun
_ne_h_spy(u3_noun ref, u3_noun pax)
{
  u3_noun x = u3m_soft_esc(u3k(ref), u3k(pax)), pro;

  if ( c3n == u3du(x) ) {
    return u3m_bail(u3nc(1, u3k(pax)));
  }
  else if ( c3n == u3du(u3t(x)) ) {
    u3t_push(u3nt(c3__hunk, u3k(ref), u3k(pax)));
    return u3m_bail(c3__exit);
  }
  pro = u3k(u3t(u3t(x)));
  u3z(x);
  return pro;
}

/*  the allocator of the arena.  A piece is named by its offset; its
**  header is read and written where the arena is writable.
*/
#define _ne_blo(off)  ((ne_blob*)(_ne_wri_y + (off)))

/* _ne_bin(): the bin of the free pieces of a length: four to a power of
**            two.
*/
static inline c3_w
_ne_bin(c3_h len_h)
{
  c3_w log_w = 31 - __builtin_clz(len_h);
  c3_w bin_w = ((log_w - 5) << 2) | ((len_h >> (log_w - 2)) & 3);

  return c3_min(bin_w, _ne_bin_w - 1);
}

static void
_ne_bin_put(c3_h off_h)
{
  ne_blob* blo_u = _ne_blo(off_h);
  c3_w     bin_w = _ne_bin(blo_u->len_h & ~(c3_h)1);

  blo_u->len_h |= 1;
  blo_u->bak_h  = 0;
  blo_u->nex_h  = _ne_bin_h[bin_w];
  if ( blo_u->nex_h ) {
    _ne_blo(blo_u->nex_h)->bak_h = off_h;
  }
  _ne_bin_h[bin_w] = off_h;
}

static void
_ne_bin_cut(c3_h off_h)
{
  ne_blob* blo_u = _ne_blo(off_h);

  if ( blo_u->bak_h ) {
    _ne_blo(blo_u->bak_h)->nex_h = blo_u->nex_h;
  }
  else {
    _ne_bin_h[_ne_bin(blo_u->len_h & ~(c3_h)1)] = blo_u->nex_h;
  }
  if ( blo_u->nex_h ) {
    _ne_blo(blo_u->nex_h)->bak_h = blo_u->bak_h;
  }
  blo_u->len_h &= ~(c3_h)1;
}

/* _ne_alloc(): room for len_w bytes in the arena: where they will run;
**              or 0 if the arena has none.  A free piece that is big
**              enough, the rest of it staying free; else the arena grows.
*/
static c3_y*
_ne_alloc(c3_w len_w)
{
  c3_h len_h = c3_align(sizeof(ne_blob) + c3_max(len_w, 16), 16, C3_ALGHI);
  c3_w bin_w;

  for ( bin_w = _ne_bin(len_h); bin_w < _ne_bin_w; bin_w++ ) {
    c3_h off_h;

    for ( off_h = _ne_bin_h[bin_w]; off_h; off_h = _ne_blo(off_h)->nex_h ) {
      ne_blob* blo_u = _ne_blo(off_h);
      c3_h     has_h = blo_u->len_h & ~(c3_h)1;

      if ( has_h >= len_h ) {
        _ne_bin_cut(off_h);

        if ( (has_h - len_h) >= 32 ) {
          ne_blob* res_u = _ne_blo(off_h + len_h);

          res_u->len_h = has_h - len_h;
          res_u->pre_h = len_h;
          blo_u->len_h = len_h;
          if ( ((c3_z)off_h + has_h) < _ne_top_z ) {
            _ne_blo(off_h + has_h)->pre_h = res_u->len_h;
          }
          else {
            _ne_las_h = res_u->len_h;
          }
          _ne_bin_put(off_h + len_h);
        }
        _ne_use_z += blo_u->len_h;
        return _ne_exe_y + off_h + sizeof(ne_blob);
      }
    }
  }

  if ( (_ne_top_z + len_h) > _ne_are_z ) {
    return 0;
  }
  {
    ne_blob* blo_u = _ne_blo(_ne_top_z);
    c3_y*    cod_y = _ne_exe_y + _ne_top_z + sizeof(ne_blob);

    blo_u->len_h = len_h;
    blo_u->pre_h = _ne_las_h;
    _ne_las_h    = len_h;
    _ne_top_z   += len_h;
    _ne_use_z   += len_h;
    _ne_pek_z    = c3_max(_ne_pek_z, _ne_top_z);
    return cod_y;
  }
}

/* _ne_free(): a piece is free: it joins the free pieces beside it, and
**             the arena shrinks if it was the last.
*/
static void
_ne_free(void* cod_v)
{
  c3_h     off_h = ((c3_y*)cod_v - _ne_exe_y) - sizeof(ne_blob);
  ne_blob* blo_u = _ne_blo(off_h);
  c3_h     len_h = blo_u->len_h;

  u3_assert( !(len_h & 1) );
  _ne_use_z -= len_h;

  if ( ((c3_z)off_h + len_h) < _ne_top_z ) {
    ne_blob* nex_u = _ne_blo(off_h + len_h);

    if ( nex_u->len_h & 1 ) {
      _ne_bin_cut(off_h + len_h);
      len_h += nex_u->len_h;
    }
  }
  if ( blo_u->pre_h ) {
    ne_blob* pre_u = _ne_blo(off_h - blo_u->pre_h);

    if ( pre_u->len_h & 1 ) {
      _ne_bin_cut(off_h - blo_u->pre_h);
      off_h -= blo_u->pre_h;
      len_h += pre_u->len_h;
      blo_u  = pre_u;
    }
  }
  blo_u->len_h = len_h;

  if ( ((c3_z)off_h + len_h) == _ne_top_z ) {
    _ne_top_z = off_h;
    _ne_las_h = blo_u->pre_h;
  }
  else {
    _ne_blo(off_h + len_h)->pre_h = len_h;
    _ne_bin_put(off_h);
  }
}

/* _ne_evict(): no program has a machine function or a thunk, and the
**              arena is as it was before the first was made.  For when
**              no machine function is running.
*/
static void
_ne_evict(void)
{
  c3_h i_h;

  _ne_att_y |= _ne_att_bus;

  for ( i_h = 1; i_h < _nj_sid_top_h; i_h++ ) {
    nj_side* sid_u = &_nj_sid_u[i_h];

    if ( sid_u->pog_u && (sid_u->emi_f || sid_u->thk_f) ) {
      if ( sid_u->emi_f && (_ne_fail != sid_u->emi_f) ) {
        sid_u->nat_f[0] = (void*)_nc_burn_north_cold;
        sid_u->nat_f[1] = (void*)_nc_burn_south_cold;
        sid_u->emi_f    = 0;
      }
      sid_u->thk_f = 0;
      sid_u->hot_h = 0;
    }
  }

  _ne_top_z = _ne_bot_z;
  _ne_las_h = 0;
  _ne_use_z = 0;
  memset(_ne_bin_h, 0, sizeof(_ne_bin_h));
  _ne_evi_d++;
  _ne_att_y = 0;
}

/* _ne_open(), _ne_shut(): around every change of the arena and of what
**                         names its pieces: a piece taken or freed, a
**                         call site or a thunk rewritten.  The signal
**                         handlers of the runtime leave by a long jump
**                         from wherever the signal came, so a change can
**                         be abandoned half made.  Then the mark stays,
**                         no other change is made (_ne_open() says
**                         whether one may be), and the code is evicted
**                         before any of it runs again
**                         (_ne_evict_idle()): the signal has unwound
**                         every machine function.  No change calls what
**                         can bail.
*/
static inline c3_t
_ne_open(void)
{
  if ( _ne_att_y & _ne_att_bus ) {
    return 0;
  }
  _ne_att_y |= _ne_att_bus;
  return 1;
}

static inline void
_ne_shut(void)
{
  _ne_att_y &= ~_ne_att_bus;
}

/* _ne_place(): the code written in asm_u, in the arena: where it runs;
**              or 0, the arena being full.
*/
static c3_y*
_ne_place(nx_asm* asm_u)
{
  c3_y* cod_y = _ne_alloc(asm_u->len_w);

  if ( !cod_y ) {
    _ne_att_y |= _ne_att_ful;
    return 0;
  }
  memcpy(_ne_w(cod_y), asm_u->buf_y, asm_u->len_w);
  _nx_link(asm_u, _ne_w(cod_y), cod_y);
  return cod_y;
}

#define _ne_hel(i)  ((c3_d)(uintptr_t)&_ne_bas_u->hel_v[i])

/* _ne_thunk(): what native code calls to run a program that has no code:
**              the program in nx_t5, and on to the interpreter
**              (_ne_tin_y); once the program has code, the thunk is a
**              jump to that.  Or 0, the arena being full.
*/
static void*
_ne_thunk(u3nc_prog* pog_u)
{
  nx_asm asm_u = {0};
  c3_y*  cod_y;

  _nx_mov_ri64(&asm_u, nx_t5, (c3_d)(uintptr_t)pog_u);
  _nx_b(&asm_u, 0xe9);
  _nx_wait(&asm_u, nx_ext32, 0, (c3_d)(uintptr_t)_ne_tin_y);
  u3_assert( 15 == asm_u.len_w );

  cod_y = _ne_place(&asm_u);
  _nx_done(&asm_u);
  return cod_y;
}

/* _ne_target(): what a native caller of a program calls now: its machine
**               function, or its thunk; or, if the arena has no room for
**               a thunk, the trampoline that takes the program from
**               ne_data.gen_u, which the caller goes to at once and
**               must not remember.
*/
static void*
_ne_target(u3nc_prog* gop_u)
{
  nj_side* sid_u;

  if (  _ne_has(gop_u)
     || (((1 == u3nc_Emit) || (u3nc_Hot <= 1)) && _ne_lower(gop_u)) )
  {
    return _nj_side(gop_u)->emi_f;
  }
  sid_u = _nj_side_get(gop_u);
  if ( !sid_u->thk_f && _ne_open() ) {
    sid_u->thk_f = _ne_thunk(gop_u);
    _ne_shut();
  }
  if ( !sid_u->thk_f ) {
    _ne_dat_u->gen_u = gop_u;
    return _ne_gen_y;
  }
  return sid_u->thk_f;
}

/* _ne_callee(): the program of a site, linked as the bytecode links it.
*/
static u3nc_prog*
_ne_callee(u3nc_prog* pog_u, u3nc_dire* dir_u)
{
  u3nc_prog* gop_u;

  if ( dir_u->pog_p ) {
    return u3to(u3nc_prog, dir_u->pog_p);
  }
  u3t_off(noc_o);
  gop_u = _nc_callee(pog_u, dir_u);
  u3t_on(noc_o);
  return gop_u;
}

/* _ne_h_link(): the callee of the site of a cell, linked as the bytecode
**               links it; the cell remembers what to call if the site
**               remembers the callee.  The arguments past the registers
**               wait in the arena meanwhile: a compilation may run code.
*/
static _nc_cold void*
_ne_h_link(u3nc_prog* pog_u, ne_cell* cel_u)
{
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[(c3_h)cel_u->sit_d]);
  u3nc_prog* gop_u;
  void*      tar_v;
  u3_noun    arg[_ne_arg_max];
  c3_h       len_h = ( dir_u->len_h > _nx_sot_w ) ? (dir_u->len_h - _nx_sot_w)
                                                    : 0;

  memcpy(arg, _ne_dat_u->arg, len_h * sizeof(u3_noun));

  gop_u = _ne_callee(pog_u, dir_u);
  tar_v = _ne_target(gop_u);
  if ( dir_u->pog_p && (tar_v != (void*)_ne_gen_y) && _ne_open() ) {
    int32_t dis_i = (c3_y*)tar_v - (cel_u->ins_y + 6);
    c3_y*   ins_y = _ne_w(cel_u->ins_y);

    ((ne_cell*)_ne_w(cel_u))->tar_v = tar_v;

    //  the call through the cell becomes a call of the code, of the same
    //  length: nop, call; or jmp, trap
    //
    if ( cel_u->sit_d >> 32 ) {
      dis_i += 1;
      ins_y[0] = 0xe9;
      memcpy(ins_y + 1, &dis_i, 4);
      ins_y[5] = 0xcc;
    }
    else {
      ins_y[0] = 0x90;
      ins_y[1] = 0xe8;
      memcpy(ins_y + 2, &dis_i, 4);
    }
    _ne_shut();
  }

  memcpy(_ne_dat_u->arg, arg, len_h * sizeof(u3_noun));
  return tar_v;
}

/* _ne_h_nok(): what to call for .*(sub fol): the code of its entry
**              program, or its thunk; or, the formula having no program
**              (_nc_entry()), the trampoline that runs it in u3n, which
**              the caller goes to at once.  RETAINS.
*/
static _nc_cold void*
_ne_h_nok(u3_noun sub, u3_noun fol)
{
  u3nc_prog* gop_u;

  u3t_off(noc_o);
  gop_u = _nc_entry(sub, fol, 0);
  u3t_on(noc_o);

  if ( !gop_u ) {
    _ne_dat_u->gen_u = 0;
    _ne_dat_u->fol   = u3k(fol);
    return _ne_gen_y;
  }
  return _ne_target(gop_u);
}

/* _ne_h_cap(): the u3w jet of a call by subject on the subject x, which
**              it takes if it doesn't punt: its product, or u3_none.
*/
static _nc_cold u3_noun
_ne_h_cap(u3nc_prog* pog_u, c3_w sit_w, u3_noun x)
{
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[sit_w]);
  u3_noun    pro;

  if ( _nc_memo_jet(dir_u->arm_u) ) {
    u3nc_Gop = _ne_callee(pog_u, dir_u);
  }
  pro = u3j_kick_arm(x, dir_u->ham_u, dir_u->arm_u->mon_f, u3t(dir_u->ring));
  u3nc_Gop = 0;
  return pro;
}

/* _ne_h_cam(): the memo of a memoized call, its arguments in arg,
**              uncounted: the product, a hit; or u3_none, the key left
**              after the arguments for the save.
*/
static _nc_cold u3_noun
_ne_h_cam(u3nc_prog* pog_u, c3_w sit_w, u3_noun* arg)
{
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[sit_w]);
  u3nc_prog* gop_u = _ne_callee(pog_u, dir_u);
  c3_h       kni_h = 0;
  u3_noun    key, pro;

  key = _nc_key(dir_u->cid_h, c3n,
                u3nc(_nc_knit(gop_u->ned, arg, &kni_h), u3k(dir_u->bell)));
  pro = _nc_find(dir_u->cid_h, key);

  if ( u3_none != pro ) {
    u3z(key);
    return pro;
  }
  arg[dir_u->len_h] = key;
  return u3_none;
}

/* _ne_h_csm(): the same for a memoized call by subject, on the subject x.
*/
static _nc_cold u3_noun
_ne_h_csm(u3nc_prog* pog_u, c3_w sit_w, u3_noun x, u3_noun* key)
{
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[sit_w]);
  u3_noun    kee, pro;

  kee = _nc_key(dir_u->cid_h, c3y, u3nc(u3k(x), u3k(u3t(dir_u->bell))));
  pro = _nc_find(dir_u->cid_h, kee);

  if ( u3_none != pro ) {
    u3z(kee);
    return pro;
  }
  *key = kee;
  return u3_none;
}

static _nc_cold u3_noun
_ne_h_save(c3_w cid_w, u3_noun key, u3_noun pro)
{
  return _nc_save(cid_w, key, pro);
}

/* _ne_h_cac(): a total array jet under the flags of its arm, as CAC: its
**              product; or, the arm off, u3_none, the core rebuilt from
**              the arguments left in arg[0] for the unary program
**              (_ne_h_mono()).  RETAINS the arguments.
*/
static _nc_cold u3_noun
_ne_h_cac(u3nc_prog* pog_u, c3_w sit_w, u3_noun* arg)
{
  u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[sit_w]);

  if ( c3n == dir_u->ham_u->liv ) {
    arg[0] = _nc_core(dir_u, arg);
    return u3_none;
  }
  if ( c3y == dir_u->ham_u->ice ) {
    return dir_u->arm_u->arg_f(arg);
  }
  return ( c3y == u3a_is_north(u3R) )
       ? _nc_burn_north_check(dir_u, arg, dir_u->len_h)
       : _nc_burn_south_check(dir_u, arg, dir_u->len_h);
}

static _nc_cold void*
_ne_h_mono(u3nc_prog* pog_u, c3_w sit_w)
{
  return _ne_target(_nc_mono(&(pog_u->dir_u.dat_u[sit_w])));
}

/* _ne_h_deep(): a run has gone deep on the stack: once is enough to
**               know.
*/
static _nc_cold void
_ne_h_deep(ne_ctx* ctx_u)
{
  _ne_dep_t    = 1;
  _ne_now_t    = 1;
  ctx_u->lim_y = 0;
}

/* _ne_h_interp(): run the bytecode of a program for native code: the
**                 first arguments in arg, the rest in the arena.  The
**                 product, and above it, in the high half, the shape of a
**                 product that came in parts, which are in _nc_ret.
**                 Without a program, run the formula of ne_data in u3n on
**                 the subject in arg (_ne_h_nok()).
*/
static ne_duo
_ne_h_interp(u3nc_prog* pog_u, u3_noun* arg)
{
  u3_noun  buf[_ne_arg_max + _nx_sot_w];
  c3_h     len_h, i_h;
  u3_noun  pro, sap;

  if ( !pog_u ) {
    u3t_off(noc_o);
    pro = u3n_nock_on(arg[0], _ne_dat_u->fol);
    u3t_on(noc_o);
    return _ne_duo(pro, 0);
  }
  len_h = pog_u->arg_h;

  for ( i_h = 0; (i_h < len_h) && (i_h < _nx_sot_w); i_h++ ) {
    buf[i_h] = arg[i_h];
  }
  for ( ; i_h < len_h; i_h++ ) {
    buf[i_h] = _ne_dat_u->arg[i_h - _nx_sot_w];
  }

  _ne_raw_t = 1;
  pro = ( c3y == u3a_is_north(u3R) )
      ? _nc_burn_north(pog_u, buf, len_h)
      : _nc_burn_south(pog_u, buf, len_h);
  sap = _ne_sap;
  _ne_sap = 0;

  return _ne_duo(pro, sap);
}

/* _ne_arena(): map the arena: a file in memory, executable in one place
**              and writable in another, never both in one; before the
**              first, what the code writes.  Whether it could.
*/
static c3_t
_ne_arena(void)
{
  c3_z  dat_z = c3_align(sizeof(ne_data), (c3_z)1 << 16, C3_ALGHI);
  c3_y* bas_y;
  void* map_v;
  c3_i  fid_i;

  _ne_are_z = _ne_map_z = (c3_z)1024 << 20;

#ifndef MFD_EXEC
#  define MFD_EXEC  0x0010U
#endif
  //  a kernel that doesn't know the flag refuses it
  //
  if (  ((fid_i = memfd_create("u3nc-code", MFD_CLOEXEC | MFD_EXEC)) < 0)
     && ((fid_i = memfd_create("u3nc-code", MFD_CLOEXEC)) < 0) )
  {
    return 0;
  }
  if ( ftruncate(fid_i, _ne_are_z) ) {
    close(fid_i);
    return 0;
  }

  bas_y = mmap(0, dat_z + _ne_are_z, PROT_NONE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if ( MAP_FAILED == (void*)bas_y ) {
    close(fid_i);
    return 0;
  }

  map_v = mmap(bas_y, dat_z, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if ( MAP_FAILED != map_v ) {
    _ne_dat_u = map_v;
    map_v = mmap(bas_y + dat_z, _ne_are_z, PROT_READ | PROT_EXEC,
                 MAP_SHARED | MAP_FIXED, fid_i, 0);
  }
  if ( MAP_FAILED != map_v ) {
    _ne_exe_y = map_v;
    map_v = mmap(0, _ne_are_z, PROT_READ | PROT_WRITE, MAP_SHARED, fid_i, 0);
  }
  close(fid_i);

  if ( MAP_FAILED == map_v ) {
    munmap(bas_y, dat_z + _ne_are_z);
    _ne_exe_y = 0;
    return 0;
  }
  _ne_wri_y = map_v;
  return 1;
}

/* _ne_init(): the arena, the stack and the trampolines, once.  Whether
**             there is an arena.
*/
static c3_t
_ne_init(void)
{
  nx_asm asm_u = {0};
  c3_w   i_w;

  if ( _ne_exe_y ) {
    return 1;
  }
  if ( _ne_off_t ) {
    return 0;
  }
  if ( !_ne_arena() ) {
    fprintf(stderr, "u3nc: no arena for machine code: %s\r\n",
            strerror(errno));
    _ne_off_t = 1;
    return 0;
  }

  _ne_bas_u = (ne_base*)_ne_exe_y;
  _ne_top_z = c3_align(sizeof(ne_base), 64, C3_ALGHI);

#define HEL  (((ne_base*)_ne_w(_ne_bas_u))->hel_v)
  HEL[ne_h_gain]   = (void*)_ne_h_gain;
  HEL[ne_h_lose]   = (void*)_ne_h_lose;
  HEL[ne_h_bail]   = (void*)_ne_h_bail;
  HEL[ne_h_link]   = (void*)_ne_h_link;
  HEL[ne_h_interp] = (void*)_ne_h_interp;
  HEL[ne_h_inc]    = (void*)_ne_h_inc;
  HEL[ne_h_dec]    = (void*)_ne_h_dec;
  HEL[ne_h_add]    = (void*)_ne_h_add;
  HEL[ne_h_cons]   = (void*)_ne_h_cons;
  HEL[ne_h_sing]   = (void*)_ne_h_sing;
  HEL[ne_h_ret]    = (void*)_nc_ret;
  HEL[ne_h_box]    = (void*)_ne_h_box;
  HEL[ne_h_nok]    = (void*)_ne_h_nok;
  HEL[ne_h_cap]    = (void*)_ne_h_cap;
  HEL[ne_h_cam]    = (void*)_ne_h_cam;
  HEL[ne_h_csm]    = (void*)_ne_h_csm;
  HEL[ne_h_save]   = (void*)_ne_h_save;
  HEL[ne_h_hmp]    = (void*)_ne_h_hmp;
  HEL[ne_h_tdrop]  = (void*)_ne_h_tdrop;
  HEL[ne_h_hcp]    = (void*)_ne_h_hcp;
  HEL[ne_h_hlp]    = (void*)_ne_h_hlp;
  HEL[ne_h_hsp]    = (void*)_ne_h_hsp;
  HEL[ne_h_hse]    = (void*)_ne_h_hse;
  HEL[ne_h_hdp]    = (void*)_ne_h_hdp;
  HEL[ne_h_hde]    = (void*)_ne_h_hde;
  HEL[ne_h_spy]    = (void*)_ne_h_spy;
  HEL[ne_h_cac]    = (void*)_ne_h_cac;
  HEL[ne_h_mono]   = (void*)_ne_h_mono;
  HEL[ne_h_deep]   = (void*)_ne_h_deep;

  {
    _ne_stk_z = (c3_z)1024 << 20;
    _ne_stk_y = mmap(0, _ne_stk_z, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_STACK,
                     -1, 0);
    u3_assert( MAP_FAILED != (void*)_ne_stk_y );

    //  the guard: sixteen megabytes, or a quarter of a small stack: more
    //  than any frame of the C that runs on it
    //
    _ne_gua_z = c3_min((c3_z)16 << 20, _ne_stk_z >> 2);
    u3_assert( 0 == mprotect(_ne_stk_y, _ne_gua_z, PROT_NONE) );
    _ne_kep_z = c3_min((c3_z)64 << 20, (_ne_stk_z - _ne_gua_z) >> 1);

  }

  //  C to native code: (code, arguments, context, new stack or 0).  The
  //  registers C wants kept are saved, the stack is switched if the caller
  //  isn't on ours, the first arguments go to the slot registers; back,
  //  the parts go to _nc_ret, and the shape of the product comes above the
  //  product
  //
  {
    static const c3_y sav_y[6] = {
      nx_rbx, nx_rbp, nx_r12, nx_r13, nx_r14, nx_r15
    };
    c3_w on_w = _nx_label(&asm_u);

    for ( i_w = 0; i_w < 6; i_w++ ) {
      _nx_push(&asm_u, sav_y[i_w]);
    }
    _nx_mov_rr64(&asm_u, nx_rax, nx_rsp);
    _nx_test_rr64(&asm_u, nx_rcx, nx_rcx);
    _nx_jcc8(&asm_u, nx_e, on_w);
    _nx_mov_rr64(&asm_u, nx_rsp, nx_rcx);
    _nx_bind(&asm_u, on_w);
    _nx_push(&asm_u, nx_rax);
    _nx_mov_rr64(&asm_u, nx_ctx, nx_rdx);
    _nx_mov_ri64(&asm_u, nx_bas, (c3_d)(uintptr_t)u3_Loom);
    for ( i_w = 0; i_w < _nx_sot_w; i_w++ ) {
      _nx_ld(&asm_u, _nx_sot_y[i_w], nx_rsi, _ne_wb * i_w);
    }
    _nx_call_r(&asm_u, nx_rdi);
    _nx_ld64_l(&asm_u, nx_rdx, ~(c3_w)0, _ne_hel(ne_h_ret));
    for ( i_w = 0; i_w < _nx_sot_w; i_w++ ) {
      _nx_st(&asm_u, nx_rdx, _ne_wb * (i_w + 1), _nx_sot_y[i_w]);
    }
#ifdef VERE64
    //  the product and the shape as C returns two words (ne_duo)
    //
    _nx_mov_rr64(&asm_u, nx_rdx, nx_rcx);
#else
    //  the product in the low half, the shape in the high one
    //
    _nx_rr(&asm_u, 0, 0x89, nx_rax, nx_rax);
    _nx_rr(&asm_u, 1, 0xc1, 4, nx_rcx);
    _nx_b(&asm_u, 32);
    _nx_rr(&asm_u, 1, 0x09, nx_rcx, nx_rax);
#endif
    _nx_pop(&asm_u, nx_rsp);
    for ( i_w = 6; i_w-- > 0; ) {
      _nx_pop(&asm_u, sav_y[i_w]);
    }
    _nx_ret(&asm_u);

    _ne_ent_f = (void*)_ne_place(&asm_u);
    _nx_done(&asm_u);
    memset(&asm_u, 0, sizeof(asm_u));
  }

  //  native code to the interpreter: the program in nx_t5, the arguments
  //  where a native function takes them; back, the parts in the slot
  //  registers and the shape in nx_t1, as a native function returns
  //
  {
    //  the frame: the slots, and the stack as a call wants it
    //
    c3_w fra_w = ((((_ne_wb * _nx_sot_w) + 8 + 15) & ~15) - 8);

    _nx_alu_ri64(&asm_u, nx_sub, nx_rsp, fra_w);
    for ( i_w = 0; i_w < _nx_sot_w; i_w++ ) {
      _nx_st(&asm_u, nx_rsp, _ne_wb * i_w, _nx_sot_y[i_w]);
    }
    _nx_mov_rr64(&asm_u, nx_rdi, nx_t5);
    _nx_mov_rr64(&asm_u, nx_rsi, nx_rsp);
    _nx_call_l(&asm_u, ~(c3_w)0, _ne_hel(ne_h_interp));
#ifdef VERE64
    _nx_mov_rr64(&asm_u, nx_rcx, nx_rdx);
#endif
    _nx_ld64_l(&asm_u, nx_rdx, ~(c3_w)0, _ne_hel(ne_h_ret));
    for ( i_w = 0; i_w < _nx_sot_w; i_w++ ) {
      _nx_ld(&asm_u, _nx_sot_y[i_w], nx_rdx, _ne_wb * (i_w + 1));
    }
#ifndef VERE64
    _nx_mov_rr64(&asm_u, nx_rcx, nx_rax);
    _nx_shr_ri64(&asm_u, nx_rcx, 32);
#endif
    _nx_alu_ri64(&asm_u, nx_add, nx_rsp, fra_w);
    _nx_ret(&asm_u);

    _ne_tin_y = _ne_place(&asm_u);
    _nx_done(&asm_u);
  }
  HEL[ne_h_tin] = _ne_tin_y;
#undef HEL

  //  the same for a program that has no thunk: the arena was full
  //
  {
    memset(&asm_u, 0, sizeof(asm_u));
    _nx_ld64_l(&asm_u, nx_t5, ~(c3_w)0, (c3_d)(uintptr_t)&_ne_dat_u->gen_u);
    _nx_b(&asm_u, 0xe9);
    _nx_wait(&asm_u, nx_ext32, 0, (c3_d)(uintptr_t)_ne_tin_y);
    _ne_gen_y = _ne_place(&asm_u);
    _nx_done(&asm_u);
  }
  u3_assert( _ne_ent_f && _ne_tin_y && _ne_gen_y );

  //  what is in the arena now is never freed
  //
  _ne_bot_z = _ne_top_z;
  _ne_las_h = 0;
  _ne_use_z = 0;

  return 1;
}

/* _ne_run(): run the code of a program on its arguments, for C: the
**            product, and its shape above it if it came in parts.
*/
static ne_duo
_ne_run(u3nc_prog* pog_u, u3_noun* arg)
{
  u3a_road* rod_u = u3R;
  ne_ctx    ctx_u;
  c3_y*     her_y = (c3_y*)__builtin_frame_address(0);
  c3_y*     new_y = 0;
  c3_w      lo_w, hi_w;

  if ( c3y == u3a_is_north(rod_u) ) {
    lo_w = rod_u->rut_p;
    hi_w = rod_u->cap_p;
  }
  else {
    lo_w = rod_u->cap_p;
    hi_w = rod_u->rut_p;
  }
  ctx_u.lo_w  = _ne_off(lo_w);
  ctx_u.spa_w = _ne_off(hi_w - lo_w);
  ctx_u.reu_w = ( rod_u == &(u3H->rod_u) ) ? 0 : ctx_u.spa_w;
  ctx_u.cel_w = u3a_cell_flag;
  ctx_u.dmx_w = u3a_direct_max;
  ctx_u.dm1_w = u3a_direct_max - 1;
  ctx_u.lim_y = _ne_now_t ? 0 : (_ne_stk_y + _ne_stk_z - _ne_kep_z);
  ctx_u.rod_u = rod_u;

  if ( (c3_z)((_ne_stk_y + _ne_stk_z) - her_y) >= _ne_stk_z ) {
    new_y = _ne_stk_y + _ne_stk_z - 8;

    //  nothing is on the stack: the pages a deep run touched go back to
    //  the system, once a run has not gone deep after one that had: a
    //  computation that goes deep every time keeps them
    //
    if ( _ne_dep_t && !_ne_now_t ) {
      madvise(_ne_stk_y + _ne_gua_z, _ne_stk_z - _ne_gua_z - _ne_kep_z,
              MADV_DONTNEED);
      _ne_dep_t = 0;
      _ne_tri_d++;
    }
    _ne_now_t   = 0;
    ctx_u.lim_y = _ne_stk_y + _ne_stk_z - _ne_kep_z;
  }

  if ( pog_u->arg_h > _nx_sot_w ) {
    memcpy(_ne_dat_u->arg, arg + _nx_sot_w,
           (pog_u->arg_h - _nx_sot_w) * sizeof(u3_noun));
  }

  //  the trampoline loads every slot register: from a copy, since the
  //  arguments may be the last words of the loom
  //
  {
    u3_noun buf[_nx_sot_w] = {0};
    c3_h    i_h;

    for ( i_h = 0; (i_h < pog_u->arg_h) && (i_h < _nx_sot_w); i_h++ ) {
      buf[i_h] = arg[i_h];
    }
    return _ne_ent_f(_nj_side(pog_u)->emi_f, buf, &ctx_u, new_y);
  }
}

/* _ne_evict_idle(): if the arena is full, or a change of it was
**                   abandoned, and no machine function is running, i.e.
**                   the caller is not on the stack of the code, evict:
**                   whether it did.
*/
static c3_t
_ne_evict_idle(void)
{
  c3_y* her_y = (c3_y*)__builtin_frame_address(0);

  if ( !_ne_att_y || !_ne_exe_y ) {
    return 0;
  }
  if ( (c3_z)((_ne_stk_y + _ne_stk_z) - her_y) < _ne_stk_z ) {
    return 0;
  }
  _ne_evict();
  return 1;
}

/* _nj_reset(): no program has native code of either kind, and both arenas
**              are empty: the programs were dropped wholesale, or their
**              records are no longer theirs (a restored image).
*/
static void
_nj_reset(void)
{
  _nj_side_wipe();
  _nj_top_z = 0;
  _nj_fre_u = 0;

  if ( _ne_exe_y ) {
    _ne_evict();
  }
}

/* _ne_lose(): free the machine function and the thunk of a record; what
**            runs its program is the stub for the bytecode again.
*/
static void
_ne_lose(nj_side* sid_u)
{
  //  if a change was abandoned the pieces are left where they are: they
  //  go when the code is evicted
  //
  c3_t fre_t = _ne_open();

  if ( sid_u->emi_f && (_ne_fail != sid_u->emi_f) ) {
    if ( fre_t ) {
      _ne_free(sid_u->emi_f);
    }
    sid_u->nat_f[0] = (void*)_nc_burn_north_cold;
    sid_u->nat_f[1] = (void*)_nc_burn_south_cold;
  }
  if ( sid_u->thk_f && fre_t ) {
    _ne_free(sid_u->thk_f);
  }
  sid_u->emi_f = 0;
  sid_u->thk_f = 0;

  if ( fre_t ) {
    _ne_shut();
  }
}

/**  The lowering.
**/

/* ne_op: an op of the bytecode, decoded.
*/
typedef struct {
  c3_y fam_y;       //  its family
  c3_y kon_y;       //  its variant bits
  c3_y led_y;       //  1: a jump goes here
  c3_h imm_h;       //  its immediate
  c3_h src_h[2];    //  its source slots
  c3_h dst_h;       //  its destination slot
  c3_h tar_h;       //  its target, an op
  c3_h off_h;       //  where it is in the bytecode
  c3_h dep_h;       //  tokens on the stack before it (HER, the hints)
  c3_w lab_w;       //  its label, if a jump goes here
} ne_op;

/* ne_stub: a slow path, emitted after the function.
*/
enum {
  ne_s_gain, ne_s_lose, ne_s_inc, ne_s_dec, ne_s_add, ne_s_eqq, ne_s_eql,
  ne_s_edge, ne_s_link, ne_s_bail, ne_s_box, ne_s_cons, ne_s_deep
};

typedef struct {
  c3_y kin_y;       //  ne_s_
  c3_y a_y, b_y;    //  registers it reads
  c3_y d_y;         //  the register its product goes to
  c3_w imm_w;       //  flags, a mote, a literal
  c3_h s_h[2];      //  slots (ne_s_edge, ne_s_eqq)
  c3_w ent_w;       //  its label
  c3_w bak_w;       //  where it goes back
  c3_w tar_w;       //  where else it goes
  c3_w one_w;       //  another way in, and
  c3_w two_w;       //  another way back (ne_s_cons)
} ne_stub;

/* ne_gen: the lowering of a program.
*/
typedef struct {
  nx_asm     asm_u;
  u3nc_prog* pog_u;
  ne_op*     ops_u;       //  its ops
  c3_w       opn_w;
  c3_w*      map_w;       //  offset in the bytecode -> op
  c3_ws*     hom_ws;      //  slot -> its home: a register, or -(cell + 1)
                          //  in the frame
  c3_w       cel_w;       //  cells of the frame (four bytes each)
  c3_w       tok_w;       //    the first for the tokens
  c3_w       jet_w;       //    the first for the arguments of a jet
  c3_w       stg_w;       //    the first for parts of a product that wait
  c3_t       pog_t;       //  the cell of the program is used
  c3_w       fra_w;       //  bytes of the frame
  ne_stub*   stu_u;       //  the slow paths
  c3_w       stn_w;
  c3_w       stc_w;
  c3_w       bai_w[3];    //  labels of the bails, by mote: exit, fail, meme
  c3_t       bai_t[3];
  c3_w       pog_w;       //  label: the cell of the program
  c3_w       lin_w;       //  label: the common part of the link stubs
  c3_t       lin_t;
  c3_w*      sit_w;       //  site -> the label of its cell, or 0
  c3_w*      ins_w;       //  site -> the label of its call
  c3_y*      tai_y;       //  site -> it is a tail call
  c3_w       sin_w;       //  cells made
  c3_y       why_y;       //  the family it failed on
  c3_t       bad_t;       //  it failed
  c3_t       cal_t;       //  it calls a nock function
} ne_gen;

#define _ne_sot_kep     4         //  the first slot registers are the ones a
                                  //  C function keeps
#define _ne_isreg(hom)  ((hom) >= 0)
#define _ne_cell(hom)   (_ne_wb * (-(hom) - 1))
#define _ne_argm        0x100     //  a place: argument i past the registers

/* _ne_nock_call(): whether an op of the family calls a nock function, and
**                  goes on: the slot registers do not survive it.
*/
static c3_t
_ne_nock_call(c3_y fam_y)
{
  switch ( fam_y ) {
    default: return 0;
    case _nc_cal: case _nc_cam: case _nc_cap: case _nc_csm: case _nc_nok:
    case _nc_cac: return 1;
  }
}

/* _ne_ends(): whether a family ends the activation, or crashes.
*/
static c3_t
_ne_ends(c3_y fam_y)
{
  switch ( fam_y ) {
    default: return 0;
    case _nc_don: case _nc_dox: case _nc_jmp: case _nc_jms: case _nc_jmf:
    case _nc_jmx: case _nc_jsp: case _nc_jmc: case _nc_bom: return 1;
  }
}

/* _ne_decode(): the ops of the program.
*/
static void
_ne_decode(ne_gen* gen_u)
{
  u3nc_prog*  pog_u = gen_u->pog_u;
  const c3_y* byc_y = pog_u->byc_u.ops_y;
  c3_h        len_h = pog_u->byc_u.len_h, ip_h;
  c3_w        cap_w = 0, i_w;

  gen_u->map_w = c3_malloc(((c3_z)len_h + 1) * sizeof(c3_w));
  memset(gen_u->map_w, 0xff, ((c3_z)len_h + 1) * sizeof(c3_w));

  for ( ip_h = 0; ip_h < len_h; ) {
    ne_op* op_u;
    c3_y   wid_y;

    _nx_grow(gen_u->ops_u, gen_u->opn_w, cap_w, ne_op);
    gen_u->map_w[ip_h] = gen_u->opn_w;
    op_u = &gen_u->ops_u[gen_u->opn_w++];
    memset(op_u, 0, sizeof(*op_u));
    op_u->off_h = ip_h;
    op_u->dst_h = op_u->tar_h = _nc_none;

    if ( BOM == byc_y[ip_h] ) {
      ip_h++;
      op_u->fam_y = _nc_bom;
      continue;
    }

    op_u->fam_y = _nc_decode(byc_y, &ip_h, &op_u->kon_y, &wid_y);
    if ( _nc_fam[op_u->fam_y].imm_y ) {
      op_u->imm_h = _nc_read(byc_y, &ip_h, wid_y);
    }
    for ( i_w = 0; i_w < _nc_fam[op_u->fam_y].src_y; i_w++ ) {
      op_u->src_h[i_w] = _nc_read(byc_y, &ip_h, wid_y);
    }
    if ( _nc_fam[op_u->fam_y].dst_y ) {
      op_u->dst_h = _nc_read(byc_y, &ip_h, wid_y);
    }
    if ( _nc_fam[op_u->fam_y].tar_y ) {
      op_u->tar_h = _nc_read(byc_y, &ip_h, wid_y);
    }
    if ( _nc_fam[op_u->fam_y].sot_y ) {
      op_u->dst_h = pog_u->dir_u.dat_u[op_u->imm_h].des_h;
    }
  }
  gen_u->map_w[len_h] = gen_u->opn_w;

  //  targets: from offsets to ops
  //
  for ( i_w = 0; i_w < gen_u->opn_w; i_w++ ) {
    ne_op* op_u = &gen_u->ops_u[i_w];

    if ( _nc_none != op_u->tar_h ) {
      c3_w tar_w = gen_u->map_w[op_u->tar_h];

      u3_assert( tar_w < gen_u->opn_w );
      op_u->tar_h = tar_w;
      if ( !gen_u->ops_u[tar_w].led_y ) {
        gen_u->ops_u[tar_w].led_y = 1;
        gen_u->ops_u[tar_w].lab_w = _nx_label(&gen_u->asm_u);
      }
    }
  }
}

/* _ne_back(): the slots live before an op, from those live after it, in
**             place.
*/
static void
_ne_back(ne_gen* gen_u, const ne_op* op_u, c3_d* liv_d)
{
  c3_y fam_y = op_u->fam_y;
  c3_w i_w;

  if ( _nc_bom == fam_y ) {
    return;
  }
  if ( _nc_none != op_u->dst_h ) {
    liv_d[op_u->dst_h >> 6] &= ~((c3_d)1 << (op_u->dst_h & 63));
  }
  for ( i_w = 0; i_w < _nc_fam[fam_y].src_y; i_w++ ) {
    liv_d[op_u->src_h[i_w] >> 6] |= (c3_d)1 << (op_u->src_h[i_w] & 63);
  }
  if ( _nc_fam[fam_y].sot_y ) {
    const u3nc_dire* dir_u = &(gen_u->pog_u->dir_u.dat_u[op_u->imm_h]);
    const c3_h*      sot_h = gen_u->pog_u->sot_u.sot_h + dir_u->sot_h;

    for ( i_w = 0; i_w < dir_u->len_h; i_w++ ) {
      liv_d[sot_h[i_w] >> 6] |= (c3_d)1 << (sot_h[i_w] & 63);
    }
  }
}

/* _ne_rts_run(): if the ops from i_w on are RTS ops and then their DOX,
**                with no jump into them past the first: the number of the
**                RTS ops; else ~0.
*/
static c3_w
_ne_rts_run(const ne_gen* gen_u, c3_w i_w)
{
  const ne_op* ops_u = gen_u->ops_u;
  c3_w         len_w = 0;

  while (  ((i_w + len_w) < gen_u->opn_w)
        && (_nc_rts == ops_u[i_w + len_w].fam_y)
        && (!len_w || !ops_u[i_w + len_w].led_y) )
  {
    len_w++;
  }
  if (  ((i_w + len_w) < gen_u->opn_w)
     && (_nc_dox == ops_u[i_w + len_w].fam_y)
     && (!len_w || !ops_u[i_w + len_w].led_y) )
  {
    return len_w;
  }
  return ~(c3_w)0;
}

/* _ne_rtl_run(): the ops after the call i_w that take its product apart:
**                RTL ops, and drops among them: their number, to the last
**                RTL.  mix_t: there are drops among them.
*/
static c3_w
_ne_rtl_run(const ne_gen* gen_u, c3_w i_w, c3_t* mix_t)
{
  const ne_op* ops_u = gen_u->ops_u;
  c3_w         len_w = 0, k_w;

  for ( k_w = i_w + 1; (k_w < gen_u->opn_w) && !ops_u[k_w].led_y; k_w++ ) {
    if ( _nc_rtl == ops_u[k_w].fam_y ) {
      len_w = k_w - i_w;
    }
    else if ( _nc_dro != ops_u[k_w].fam_y ) {
      break;
    }
  }

  *mix_t = 0;
  for ( k_w = i_w + 1; k_w <= (i_w + len_w); k_w++ ) {
    if ( _nc_dro == ops_u[k_w].fam_y ) {
      *mix_t = 1;
    }
  }
  return len_w;
}

/* _ne_sap_len(): the number of parts of a product of a shape, as
**                _nc_box() counts them, or -1 if the shape is not one.
*/
static c3_ws
_ne_sap_len(u3_noun sap)
{
  if ( u3_nul == sap ) {
    return 1;
  }
  if ( (c3n == u3du(sap)) || (c3n == u3ud(u3h(sap))) ) {
    return -1;
  }
  switch ( u3h(sap) ) {
    default:  return -1;
    case 'k': return 0;
    case 'c':
    case 'm': {
      c3_ws one_ws, two_ws;

      if ( c3n == u3du(u3t(sap)) ) {
        return -1;
      }
      one_ws = _ne_sap_len(u3h(u3t(sap)));
      two_ws = _ne_sap_len(u3t(u3t(sap)));
      if ( (one_ws < 0) || (two_ws < 0) ) {
        return -1;
      }
      return (('m' == u3h(sap)) ? 1 : 0) + one_ws + two_ws;
    }
  }
}

/* _ne_homes(): where each slot lives.  A slot that is live across a call
**              of a nock function lives in the frame.  The others get
**              registers while there are any: a parameter the register it
**              comes in, the rest by how much they are used, two that are
**              live at once never the same.  Also the depth of the tokens
**              at each op, and the frame.
*/
static void
_ne_homes(ne_gen* gen_u)
{
  u3nc_prog* pog_u = gen_u->pog_u;
  ne_op*     ops_u = gen_u->ops_u;
  c3_w       opn_w = gen_u->opn_w;
  c3_w       tot_w = pog_u->tot_h;
  c3_w       wor_w = (tot_w + 63) >> 6;
  c3_w       blk_w = 0, i_w, j_w, b_w;
  c3_w*      fir_w;                 //  block -> its first op
  c3_w*      blo_w;                 //  op -> its block
  c3_d*      inn_d;                 //  block -> the slots live into it
  c3_d*      liv_d;                 //  scratch
  c3_y*      xca_y;                 //  slot: live across a call
  c3_y*      xje_y;                 //  slot: live across a jet, called as C
  c3_w*      use_w;                 //  slot: times it is named
  c3_d*      mat_d = 0;             //  slot x slot: live at once
  c3_t       mat_t = (tot_w <= 1024);
  c3_t       chg_t;

  //  blocks: a leader is the first op, a target, or what follows a branch
  //  or an end
  //
  blo_w = c3_malloc((opn_w + 1) * sizeof(c3_w));
  fir_w = c3_malloc((opn_w + 2) * sizeof(c3_w));
  for ( i_w = 0; i_w < opn_w; i_w++ ) {
    if (  (0 == i_w) || ops_u[i_w].led_y
       || (_nc_none != ops_u[i_w - 1].tar_h) || _ne_ends(ops_u[i_w - 1].fam_y) )
    {
      fir_w[blk_w++] = i_w;
    }
    blo_w[i_w] = blk_w - 1;
  }
  fir_w[blk_w] = opn_w;

  inn_d = c3_calloc((c3_z)(blk_w + 1) * wor_w * sizeof(c3_d));
  liv_d = c3_malloc((c3_z)wor_w * sizeof(c3_d));
  xca_y = c3_calloc(tot_w + 1);
  xje_y = c3_calloc(tot_w + 1);
  use_w = c3_calloc((tot_w + 1) * sizeof(c3_w));
  if ( mat_t ) {
    mat_d = c3_calloc((c3_z)tot_w * wor_w * sizeof(c3_d) + 8);
  }

#define OUT(b)  do {                                                            \
    const ne_op* _las = &ops_u[fir_w[(b) + 1] - 1];                             \
    memset(liv_d, 0, wor_w * sizeof(c3_d));                                     \
    if ( !_ne_ends(_las->fam_y) ) {                                             \
      if ( _nc_none != _las->tar_h ) {                                          \
        c3_d* _t = inn_d + ((c3_z)blo_w[_las->tar_h] * wor_w);                  \
        for ( c3_w _k = 0; _k < wor_w; _k++ ) { liv_d[_k] |= _t[_k]; }          \
      }                                                                         \
      if ( (_nc_hop != _las->fam_y) && (((b) + 1) < blk_w) ) {                  \
        c3_d* _t = inn_d + ((c3_z)((b) + 1) * wor_w);                           \
        for ( c3_w _k = 0; _k < wor_w; _k++ ) { liv_d[_k] |= _t[_k]; }          \
      }                                                                         \
    }                                                                           \
  } while ( 0 )

  //  liveness, to a fixed point
  //
  do {
    chg_t = 0;
    for ( b_w = blk_w; b_w-- > 0; ) {
      c3_d* inn = inn_d + ((c3_z)b_w * wor_w);

      OUT(b_w);
      for ( i_w = fir_w[b_w + 1]; i_w-- > fir_w[b_w]; ) {
        _ne_back(gen_u, &ops_u[i_w], liv_d);
      }
      if ( memcmp(inn, liv_d, wor_w * sizeof(c3_d)) ) {
        memcpy(inn, liv_d, wor_w * sizeof(c3_d));
        chg_t = 1;
      }
    }
  } while ( chg_t );

  //  what is live across calls, and at once
  //
  for ( b_w = 0; b_w < blk_w; b_w++ ) {
    OUT(b_w);
    for ( i_w = fir_w[b_w + 1]; i_w-- > fir_w[b_w]; ) {
      const ne_op* op_u = &ops_u[i_w];
      c3_h         d_h  = op_u->dst_h;

      if ( _ne_nock_call(op_u->fam_y) ) {
        gen_u->cal_t = 1;
        for ( j_w = 0; j_w < tot_w; j_w++ ) {
          if ( (liv_d[j_w >> 6] >> (j_w & 63)) & 1 ) {
            if ( j_w != d_h ) {
              xca_y[j_w] = 1;
            }
          }
        }
      }
      //  a jet is a C function: it keeps four of the slot registers only.
      //  Its arguments are read after it, for their release
      //
      if (  (_nc_caf == op_u->fam_y) || (_nc_cax == op_u->fam_y)
         || (_nc_jmf == op_u->fam_y) || (_nc_jmx == op_u->fam_y) )
      {
        const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[op_u->imm_h]);

        for ( j_w = 0; j_w < tot_w; j_w++ ) {
          if ( ((liv_d[j_w >> 6] >> (j_w & 63)) & 1) && (j_w != d_h) ) {
            xje_y[j_w] = 1;
          }
        }
        for ( j_w = 0; j_w < dir_u->len_h; j_w++ ) {
          xje_y[pog_u->sot_u.sot_h[dir_u->sot_h + j_w]] = 1;
        }
      }
      if ( (_nc_none != d_h) && mat_t ) {
        c3_d* row_d = mat_d + ((c3_z)d_h * wor_w);

        for ( j_w = 0; j_w < wor_w; j_w++ ) {
          row_d[j_w] |= liv_d[j_w];
        }
      }
      if ( _nc_none != d_h ) {
        use_w[d_h]++;
      }
      if ( _nc_bom != op_u->fam_y ) {
        for ( j_w = 0; j_w < _nc_fam[op_u->fam_y].src_y; j_w++ ) {
          use_w[op_u->src_h[j_w]]++;
        }
        if ( _nc_fam[op_u->fam_y].sot_y ) {
          const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[op_u->imm_h]);

          for ( j_w = 0; j_w < dir_u->len_h; j_w++ ) {
            use_w[pog_u->sot_u.sot_h[dir_u->sot_h + j_w]]++;
          }
        }
      }
      _ne_back(gen_u, op_u, liv_d);
    }
  }
#undef OUT

  //  homes
  //
  gen_u->hom_ws = c3_malloc((tot_w + 1) * sizeof(c3_ws));
  gen_u->cel_w  = 0;
  {
    c3_w* reg_w = c3_malloc((tot_w + 1) * sizeof(c3_w));   //  0 none, r + 1

    for ( i_w = 0; i_w < tot_w; i_w++ ) {
      reg_w[i_w] = 0;
      if (  !xca_y[i_w] && (i_w < pog_u->arg_h) && (i_w < _nx_sot_w)
         && !(xje_y[i_w] && (i_w >= _ne_sot_kep)) )
      {
        reg_w[i_w] = i_w + 1;
      }
    }

    if ( mat_t ) {
      //  the rest, the most used first: an insertion sort of the few
      //
      c3_w* ord_w = c3_malloc((tot_w + 1) * sizeof(c3_w));
      c3_w  orn_w = 0;

      for ( i_w = 0; i_w < tot_w; i_w++ ) {
        if ( !xca_y[i_w] && !reg_w[i_w] && use_w[i_w] ) {
          c3_w k_w = orn_w++;

          while ( k_w && (use_w[ord_w[k_w - 1]] < use_w[i_w]) ) {
            ord_w[k_w] = ord_w[k_w - 1];
            k_w--;
          }
          ord_w[k_w] = i_w;
        }
      }

      for ( i_w = 0; i_w < orn_w; i_w++ ) {
        c3_w  s_w   = ord_w[i_w];
        c3_d* row_d = mat_d + ((c3_z)s_w * wor_w);
        c3_w  bus_w = 0;

        //  the registers of the slots it is live with: those live after
        //  its definitions, and those it is live after the definitions of
        //
        for ( j_w = 0; j_w < tot_w; j_w++ ) {
          if ( reg_w[j_w] && (j_w != s_w) ) {
            if (  ((row_d[j_w >> 6] >> (j_w & 63)) & 1)
               || ((mat_d[((c3_z)j_w * wor_w) + (s_w >> 6)] >> (s_w & 63)) & 1) )
            {
              bus_w |= 1 << (reg_w[j_w] - 1);
            }
          }
        }
        if ( xje_y[s_w] ) {
          bus_w |= ((1 << _nx_sot_w) - 1) & ~((1 << _ne_sot_kep) - 1);
        }
        //  a parameter slot holds its argument from the start
        //
        if ( s_w < pog_u->arg_h ) {
          for ( j_w = 0; (j_w < pog_u->arg_h) && (j_w < _nx_sot_w); j_w++ ) {
            bus_w |= 1 << j_w;
          }
        }
        for ( j_w = 0; j_w < _nx_sot_w; j_w++ ) {
          if ( !((bus_w >> j_w) & 1) ) {
            reg_w[s_w] = j_w + 1;
            break;
          }
        }
      }
      c3_free(ord_w);
    }

    for ( i_w = 0; i_w < tot_w; i_w++ ) {
      gen_u->hom_ws[i_w] = reg_w[i_w] ? (c3_ws)_nx_sot_y[reg_w[i_w] - 1]
                                      : -(c3_ws)(++gen_u->cel_w);
    }
    c3_free(reg_w);
  }

  //  the depth of the tokens: the same on every way to an op.  A block
  //  may be reached only by a jump back, so the walk repeats until it
  //  finds nothing new
  //
  {
    c3_w max_w = 0, jet_w = 0;
    c3_t new_t, stg_t = 0;

    for ( i_w = 0; i_w < opn_w; i_w++ ) {
      ops_u[i_w].dep_h = _nc_none;
      switch ( ops_u[i_w].fam_y ) {
        default: break;
        //  the arguments in memory, for a jet or a helper; a memoized
        //  call keeps its key after them
        //
        case _nc_caf: case _nc_jmf: case _nc_cax: case _nc_jmx:
        case _nc_cac: case _nc_jmc: case _nc_hmp: case _nc_cam:
        case _nc_csm: {
          jet_w = c3_max(jet_w,
                         1 + pog_u->dir_u.dat_u[ops_u[i_w].imm_h].len_h);
        } break;
      }
      //  parts of a product wait in the frame where they can't go straight
      //  between their registers and their slots
      //
      if ( _nc_rts == ops_u[i_w].fam_y ) {
        if ( ~(c3_w)0 == _ne_rts_run(gen_u, i_w) ) {
          stg_t = 1;
        }
      }
      else if ( _ne_nock_call(ops_u[i_w].fam_y) ) {
        c3_t mix_t;

        (void)_ne_rtl_run(gen_u, i_w, &mix_t);
        stg_t |= mix_t;
      }
    }
    ops_u[0].dep_h = 0;

    do {
      new_t = 0;

      for ( i_w = 0; i_w < opn_w; i_w++ ) {
        ne_op* op_u  = &ops_u[i_w];
        c3_h   dep_h = op_u->dep_h;
        c3_w   k_w;

        if ( _nc_none == dep_h ) {
          continue;
        }
        switch ( op_u->fam_y ) {
          default: break;
          case _nc_her: case _nc_tar: case _nc_hsp: case _nc_hdp:
          case _nc_hlp: {
            dep_h++;
          } break;
          case _nc_cor: case _nc_hse: case _nc_hde: {
            if ( !dep_h ) {
              gen_u->bad_t = 1;
              gen_u->why_y = op_u->fam_y;
            }
            else {
              dep_h--;
            }
          } break;
        }
        max_w = c3_max(max_w, dep_h);

        //  its successors: the target, and the next op
        //
        for ( k_w = 0; k_w < 2; k_w++ ) {
          ne_op* nex_u;

          if ( 0 == k_w ) {
            if ( _nc_none == op_u->tar_h ) {
              continue;
            }
            nex_u = &ops_u[op_u->tar_h];
          }
          else {
            if (  _ne_ends(op_u->fam_y) || (_nc_hop == op_u->fam_y)
               || ((i_w + 1) >= opn_w) )
            {
              continue;
            }
            nex_u = &ops_u[i_w + 1];
          }

          if ( _nc_none == nex_u->dep_h ) {
            nex_u->dep_h = dep_h;
            new_t = 1;
          }
          else if ( nex_u->dep_h != dep_h ) {
            gen_u->bad_t = 1;
            gen_u->why_y = _nc_hop;
          }
        }
      }
    } while ( new_t && !gen_u->bad_t );

    //  what nothing reaches is dead
    //
    for ( i_w = 0; i_w < opn_w; i_w++ ) {
      if ( _nc_none == ops_u[i_w].dep_h ) {
        ops_u[i_w].dep_h = 0;
      }
    }

    gen_u->tok_w  = gen_u->cel_w;
    gen_u->cel_w += max_w;
    gen_u->jet_w  = gen_u->cel_w;
    gen_u->cel_w += jet_w;
    gen_u->stg_w  = gen_u->cel_w;
    gen_u->cel_w += stg_t ? (_nc_ret_max - 1) : 0;
  }

  //  the frame keeps the stack aligned for the helpers
  //
  gen_u->fra_w = (((_ne_wb * gen_u->cel_w) + 8 + 15) & ~(c3_w)15) - 8;

  c3_free(blo_w);
  c3_free(fir_w);
  c3_free(inn_d);
  c3_free(liv_d);
  c3_free(xca_y);
  c3_free(xje_y);
  c3_free(use_w);
  c3_free(mat_d);
}

/*  code
*/
#define A  (&gen_u->asm_u)

static ne_stub*
_ne_stub(ne_gen* gen_u, c3_y kin_y)
{
  ne_stub* stu_u;

  _nx_grow(gen_u->stu_u, gen_u->stn_w, gen_u->stc_w, ne_stub);
  stu_u = &gen_u->stu_u[gen_u->stn_w++];
  memset(stu_u, 0, sizeof(*stu_u));
  stu_u->kin_y  = kin_y;
  stu_u->s_h[0] = stu_u->s_h[1] = _nc_none;
  stu_u->ent_w  = _nx_label(A);
  return stu_u;
}

/* _ne_bail(): the label of the bail with a mote: 0 exit, 1 fail, 2 meme.
*/
static c3_w
_ne_bail(ne_gen* gen_u, c3_w mot_w)
{
  if ( !gen_u->bai_t[mot_w] ) {
    ne_stub* stu_u = _ne_stub(gen_u, ne_s_bail);

    stu_u->imm_w = mot_w;
    gen_u->bai_t[mot_w] = 1;
    gen_u->bai_w[mot_w] = stu_u->ent_w;
  }
  return gen_u->bai_w[mot_w];
}

/* _ne_get(): the register a slot's value is in: its home, or tmp_y,
**            loaded from the frame.
** _ne_put(): the value in a register to the home of a slot.
** _ne_out(): the register to make the value of a slot in: its home, or
**            tmp_y, to be put.
*/
static c3_y
_ne_get(ne_gen* gen_u, c3_h sot_h, c3_y tmp_y)
{
  c3_ws hom_ws = gen_u->hom_ws[sot_h];

  if ( _ne_isreg(hom_ws) ) {
    return hom_ws;
  }
  _nx_ld(A, tmp_y, nx_sp, _ne_cell(hom_ws));
  return tmp_y;
}

static void
_ne_put(ne_gen* gen_u, c3_h sot_h, c3_y reg_y)
{
  c3_ws hom_ws = gen_u->hom_ws[sot_h];

  if ( _ne_isreg(hom_ws) ) {
    _nx_mov_rr(A, hom_ws, reg_y);
  }
  else {
    _nx_st(A, nx_sp, _ne_cell(hom_ws), reg_y);
  }
}

static c3_y
_ne_out(ne_gen* gen_u, c3_h sot_h, c3_y tmp_y)
{
  c3_ws hom_ws = gen_u->hom_ws[sot_h];

  return _ne_isreg(hom_ws) ? (c3_y)hom_ws : tmp_y;
}

/* _ne_alu_k(): reg op= a constant word: in the instruction if it can hold
**              it, which on words of 64 bits is 32 bits extended by their
**              sign; else from the context, or through nx_t5.
*/
static void
_ne_alu_k(ne_gen* gen_u, c3_y op_y, c3_y reg_y, c3_w imm_w)
{
  if ( _nx_fits(imm_w) ) {
    _nx_alu_ri(A, op_y, reg_y, imm_w);
  }
  else if ( u3a_cell_flag == imm_w ) {
    _nx_alu_rm(A, op_y, reg_y, nx_ctx, _ne_ctx_cel);
  }
  else if ( u3a_direct_max == imm_w ) {
    _nx_alu_rm(A, op_y, reg_y, nx_ctx, _ne_ctx_dmx);
  }
  else if ( (u3a_direct_max - 1) == imm_w ) {
    _nx_alu_rm(A, op_y, reg_y, nx_ctx, _ne_ctx_dm1);
  }
  else {
    _nx_mov_ri(A, nx_t5, imm_w);
    _nx_alu_rr(A, op_y, reg_y, nx_t5);
  }
}

/* _ne_st_i(): the word at [sp + dis] <- a constant word.
*/
static void
_ne_st_i(ne_gen* gen_u, int32_t dis_i, c3_w imm_w)
{
  if ( _nx_fits(imm_w) ) {
    _nx_st_i(A, nx_sp, dis_i, imm_w);
  }
  else {
    _nx_mov_ri(A, nx_t5, imm_w);
    _nx_st(A, nx_sp, dis_i, nx_t5);
  }
}

/* _ne_cmp_i(): compare a slot with an immediate, where it lives.
*/
static void
_ne_cmp_i(ne_gen* gen_u, c3_h sot_h, c3_w imm_w)
{
  c3_ws hom_ws = gen_u->hom_ws[sot_h];

  if ( _ne_isreg(hom_ws) ) {
    if ( !imm_w ) {
      _nx_test_rr(A, hom_ws, hom_ws);
    }
    else {
      _ne_alu_k(gen_u, nx_cmp, hom_ws, imm_w);
    }
  }
  else if ( _nx_fits(imm_w) ) {
    _nx_alu_mi(A, nx_cmp, nx_sp, _ne_cell(hom_ws), imm_w);
  }
  else {
    _nx_mov_ri(A, nx_t5, imm_w);
    _nx_alu_mr(A, nx_cmp, nx_sp, _ne_cell(hom_ws), nx_t5);
  }
}

/* _ne_normal(): jump to out_w unless the noun whose word offset is in
**               off_y is the road's own; tmp_y is lost.
*/
static void
_ne_normal(ne_gen* gen_u, c3_y off_y, c3_y tmp_y, c3_w out_w)
{
  _nx_mov_rr(A, tmp_y, off_y);
  _nx_alu_rm(A, nx_sub, tmp_y, nx_ctx, _ne_ctx_lo);
  _nx_alu_rm(A, nx_cmp, tmp_y, nx_ctx, _ne_ctx_spa);
  _nx_jcc8(A, nx_ae, out_w);
}

/* _ne_gain(): a reference more to the noun in reg_y, which stays; one_y
**             and two_y are lost.  dog_t: it is known to be indirect.
*/
static void
_ne_gain(ne_gen* gen_u, c3_y reg_y, c3_y one_y, c3_y two_y, c3_t dog_t)
{
  c3_w     out_w = _nx_label(A);
  ne_stub* stu_u;

  if ( !dog_t ) {
    _nx_test_rr(A, reg_y, reg_y);
    _nx_jcc8(A, nx_ns, out_w);
  }
  _nx_mov_rr(A, one_y, reg_y);
  _nx_shl_ri(A, one_y, 2);
  _ne_normal(gen_u, one_y, two_y, out_w);
  _nx_ldx(A, two_y, nx_bas, one_y, _ne_c_use);
  _nx_dec_r(A, two_y);
  _ne_alu_k(gen_u, nx_cmp, two_y, u3a_direct_max - 1);
  stu_u = _ne_stub(gen_u, ne_s_gain);
  stu_u->a_y   = reg_y;
  stu_u->bak_w = out_w;
  _nx_jcc(A, nx_ae, stu_u->ent_w);
  _nx_incx(A, nx_bas, one_y, _ne_c_use);
  _nx_bind(A, out_w);
}

/* _ne_less_off(): a reference less to the indirect noun in reg_y, whose
**                 word offset is in off_y; off_y and two_y are lost.
*/
static void
_ne_less_off(ne_gen* gen_u, c3_y reg_y, c3_y off_y, c3_y two_y, c3_w out_w)
{
  ne_stub* stu_u;

  _ne_normal(gen_u, off_y, two_y, out_w);
  _nx_alu_xi(A, nx_cmp, nx_bas, off_y, _ne_c_use, 1);
  stu_u = _ne_stub(gen_u, ne_s_lose);
  stu_u->a_y   = reg_y;
  stu_u->bak_w = out_w;
  _nx_jcc(A, nx_be, stu_u->ent_w);
  _nx_decx(A, nx_bas, off_y, _ne_c_use);
}

/* _ne_less(): a reference less to the noun in reg_y, which stays (the
**             register does); one_y and two_y are lost.
*/
static void
_ne_less(ne_gen* gen_u, c3_y reg_y, c3_y one_y, c3_y two_y, c3_t dog_t)
{
  c3_w out_w = _nx_label(A);

  if ( !dog_t ) {
    _nx_test_rr(A, reg_y, reg_y);
    _nx_jcc8(A, nx_ns, out_w);
  }
  _nx_mov_rr(A, one_y, reg_y);
  _nx_shl_ri(A, one_y, 2);
  _ne_less_off(gen_u, reg_y, one_y, two_y, out_w);
  _nx_bind(A, out_w);
}

/* _ne_drop(): release a slot.
*/
static void
_ne_drop(ne_gen* gen_u, c3_h sot_h, c3_t dog_t)
{
  c3_y reg_y = _ne_get(gen_u, sot_h, nx_t2);

  _ne_less(gen_u, reg_y, nx_t0, nx_t1, dog_t);
}

/* _ne_move(): one move between places: a register, a cell of the frame
**             (-(cell + 1)), or an argument past the registers
**             (_ne_argm + i).
*/
static void
_ne_move(ne_gen* gen_u, c3_ws dst_ws, c3_ws src_ws)
{
  c3_y reg_y;

  if ( dst_ws == src_ws ) {
    return;
  }

  if ( src_ws < 0 ) {
    reg_y = ( (dst_ws >= 0) && (dst_ws < _ne_argm) ) ? (c3_y)dst_ws : nx_t5;
    _nx_ld(A, reg_y, nx_sp, _ne_cell(src_ws));
  }
  else {
    u3_assert( src_ws < _ne_argm );
    reg_y = src_ws;
  }

  if ( dst_ws < 0 ) {
    _nx_st(A, nx_sp, _ne_cell(dst_ws), reg_y);
  }
  else if ( dst_ws >= _ne_argm ) {
    _nx_st_l(A, ~(c3_w)0,
             (c3_d)(uintptr_t)&_ne_dat_u->arg[dst_ws - _ne_argm], reg_y);
  }
  else {
    _nx_mov_rr(A, dst_ws, reg_y);
  }
}

/* _ne_moves(): moves that happen at once: none reads what another has
**              written.  nx_t1 breaks the cycles, nx_t5 carries between
**              two places in memory.
*/
static void
_ne_moves(ne_gen* gen_u, c3_w len_w, c3_ws* dst_ws, c3_ws* src_ws)
{
  c3_t* don_t = c3_calloc(len_w + 1);
  c3_w  lef_w = len_w, i_w, j_w;

  while ( lef_w ) {
    c3_t any_t = 0;

    for ( i_w = 0; i_w < len_w; i_w++ ) {
      c3_t use_t = 0;

      if ( don_t[i_w] ) {
        continue;
      }
      for ( j_w = 0; j_w < len_w; j_w++ ) {
        if (  !don_t[j_w] && (j_w != i_w) && (src_ws[j_w] == dst_ws[i_w])
           && (src_ws[j_w] != dst_ws[j_w]) )
        {
          use_t = 1;
          break;
        }
      }
      if ( !use_t ) {
        _ne_move(gen_u, dst_ws[i_w], src_ws[i_w]);
        don_t[i_w] = 1;
        lef_w--;
        any_t = 1;
      }
    }

    if ( !any_t ) {
      //  only cycles are left: one source steps aside
      //
      for ( i_w = 0; don_t[i_w]; i_w++ ) { }
      {
        c3_ws old_ws = src_ws[i_w];

        _ne_move(gen_u, nx_t1, old_ws);
        for ( j_w = 0; j_w < len_w; j_w++ ) {
          if ( !don_t[j_w] && (src_ws[j_w] == old_ws) ) {
            src_ws[j_w] = nx_t1;
          }
        }
      }
    }
  }

  c3_free(don_t);
}

/* _ne_site(): the label of the cell of a call site.
*/
static c3_w
_ne_site(ne_gen* gen_u, c3_h sit_h)
{
  if ( !gen_u->sit_w[sit_h] ) {
    gen_u->sit_w[sit_h] = _nx_label(A) + 1;
    gen_u->sin_w++;
  }
  return gen_u->sit_w[sit_h] - 1;
}

/* _ne_leave(): the end of the activation: the frame comes off.
*/
static void
_ne_leave(ne_gen* gen_u)
{
  _nx_alu_ri64(A, nx_add, nx_sp, gen_u->fra_w);
}

/* _ne_stage(): the cell of the frame where part i_w of a product waits.
*/
#define _ne_stage(gen_u, i_w)  (_ne_wb * ((gen_u)->stg_w + (i_w) - 1))

/* _ne_result(): the product of the call i_w (nx_t0) to its slot, and its
**               parts (the slot registers) to theirs, by the RTL ops that
**               follow: the number of ops taken with the call.  With
**               nothing among the RTL ops, all move at once.  With drops
**               among them, of parts nobody wants, they go one by one, and
**               a part whose register another's slot lives in waits in the
**               frame.
*/
static c3_w
_ne_result(ne_gen* gen_u, c3_w i_w)
{
  ne_op* op_u = &gen_u->ops_u[i_w];
  c3_t   mix_t;
  c3_w   num_w = _ne_rtl_run(gen_u, i_w, &mix_t);
  c3_w   k_w;

  if ( !mix_t ) {
    c3_ws  dst_ws[_nc_ret_max + 1], src_ws[_nc_ret_max + 1];
    c3_w   len_w = 0;

    if ( _nc_none != op_u->dst_h ) {
      dst_ws[len_w] = gen_u->hom_ws[op_u->dst_h];
      src_ws[len_w] = nx_t0;
      len_w++;
    }
    for ( k_w = 1; k_w <= num_w; k_w++ ) {
      const ne_op* rtl_u = &op_u[k_w];

      if ( !rtl_u->imm_h || (rtl_u->imm_h > _nx_sot_w) || (len_w > _nc_ret_max) ) {
        gen_u->bad_t = 1;
        gen_u->why_y = _nc_rtl;
        return num_w;
      }
      dst_ws[len_w] = gen_u->hom_ws[rtl_u->dst_h];
      src_ws[len_w] = _nx_sot_y[rtl_u->imm_h - 1];
      len_w++;
    }
    _ne_moves(gen_u, len_w, dst_ws, src_ws);

    if ( _nc_none == op_u->dst_h ) {
      //  a product nobody takes
      //
      _nx_mov_rr(A, nx_t2, nx_t0);
      _ne_less(gen_u, nx_t2, nx_t0, nx_t1, 0);
    }
  }
  else {
    //  sta_y: part i: 1, it waits in its register; 2, in the frame
    //
    c3_y sta_y[_nc_ret_max + 1] = {0};

#define CLEAR(hom)  do {                                                        \
      for ( c3_w _j = 1; _j <= _nx_sot_w; _j++ ) {                             \
        if ( (1 == sta_y[_j]) && ((hom) == (c3_ws)_nx_sot_y[_j - 1]) ) {        \
          _nx_st(A, nx_sp, _ne_stage(gen_u, _j), _nx_sot_y[_j - 1]);            \
          sta_y[_j] = 2;                                                        \
        }                                                                       \
      }                                                                         \
    } while ( 0 )

    for ( k_w = 1; k_w <= num_w; k_w++ ) {
      if ( _nc_rtl == op_u[k_w].fam_y ) {
        if ( !op_u[k_w].imm_h || (op_u[k_w].imm_h > _nx_sot_w) ) {
          gen_u->bad_t = 1;
          gen_u->why_y = _nc_rtl;
          return num_w;
        }
        sta_y[op_u[k_w].imm_h] = 1;
      }
    }

    if ( _nc_none != op_u->dst_h ) {
      CLEAR(gen_u->hom_ws[op_u->dst_h]);
      _ne_move(gen_u, gen_u->hom_ws[op_u->dst_h], nx_t0);
    }
    else {
      _nx_mov_rr(A, nx_t2, nx_t0);
      _ne_less(gen_u, nx_t2, nx_t0, nx_t1, 0);
    }

    for ( k_w = 1; k_w <= num_w; k_w++ ) {
      const ne_op* nex_u = &op_u[k_w];

      if ( _nc_dro == nex_u->fam_y ) {
        _ne_drop(gen_u, nex_u->src_h[0], 0);
      }
      else {
        c3_w  par_w  = nex_u->imm_h;
        c3_ws src_ws = ( 2 == sta_y[par_w] )
                     ? -(c3_ws)(gen_u->stg_w + par_w - 1 + 1)
                     : (c3_ws)_nx_sot_y[par_w - 1];

        sta_y[par_w] = 0;
        CLEAR(gen_u->hom_ws[nex_u->dst_h]);
        _ne_move(gen_u, gen_u->hom_ws[nex_u->dst_h], src_ws);
      }
    }
#undef CLEAR
  }
  return num_w;
}

/* _ne_boxed(): after a call that takes one noun: if the product came in
**              parts (the shape in nx_t1), they are consed.
*/
static void
_ne_boxed(ne_gen* gen_u)
{
  ne_stub* stu_u = _ne_stub(gen_u, ne_s_box);

  _nx_test_rr(A, nx_t1, nx_t1);
  _nx_jcc(A, nx_ne, stu_u->ent_w);
  stu_u->bak_w = _nx_label(A);
  _nx_bind(A, stu_u->bak_w);
}

/* _ne_gather(): the arguments of a site to the cells of the frame kept
**               for it, uncounted; nx_a2 is their address.
*/
static void
_ne_gather(ne_gen* gen_u, const u3nc_dire* dir_u)
{
  const c3_h* sot_h = gen_u->pog_u->sot_u.sot_h + dir_u->sot_h;
  c3_w        j_w;

  for ( j_w = 0; j_w < dir_u->len_h; j_w++ ) {
    c3_y reg_y = _ne_get(gen_u, sot_h[j_w], nx_t0);

    _nx_st(A, nx_sp, _ne_wb * (gen_u->jet_w + j_w), reg_y);
  }
  _nx_lea64(A, nx_a2, nx_sp, _ne_wb * gen_u->jet_w);
}

/* _ne_release(): release the arguments a site consumes, after a jet or
**                a memo that retained them; nx_t3 is kept.
*/
static void
_ne_release(ne_gen* gen_u, const u3nc_dire* dir_u)
{
  const c3_h* sot_h = gen_u->pog_u->sot_u.sot_h + dir_u->sot_h;
  const c3_y* kos_y = gen_u->pog_u->sot_u.kon_y + dir_u->sot_h;
  c3_w        j_w;

  for ( j_w = 0; j_w < dir_u->len_h; j_w++ ) {
    if ( kos_y[j_w] ) {
      _ne_drop(gen_u, sot_h[j_w], 0);
    }
  }
}

/* _ne_helper(): call a helper with the program in nx_a0 and the index of
**               a site in nx_a1.
*/
static void
_ne_helper(ne_gen* gen_u, c3_w hel_w, c3_h sit_h)
{
  gen_u->pog_t = 1;
  _nx_ld64_l(A, nx_a0, gen_u->pog_w, 0);
  _nx_mov_ri(A, nx_a1, sit_h);
  _nx_call_l(A, ~(c3_w)0, _ne_hel(hel_w));
}

/* _ne_call(): the call through the cell of a site, or the jump, in tail
**             position, after the frame has come off.
*/
static void
_ne_call(ne_gen* gen_u, c3_h sit_h, c3_t tal_t)
{
  if ( gen_u->sit_w[sit_h] ) {
    //  one call for a site: its cell names the instruction
    //
    gen_u->bad_t = 1;
    gen_u->why_y = _nc_nop;
    return;
  }
  gen_u->ins_w[sit_h] = _nx_label(A);
  gen_u->tai_y[sit_h] = tal_t;
  if ( tal_t ) {
    _ne_leave(gen_u);
    _nx_bind(A, gen_u->ins_w[sit_h]);
    _nx_jmp_l(A, _ne_site(gen_u, sit_h), 0);
  }
  else {
    _nx_bind(A, gen_u->ins_w[sit_h]);
    _nx_call_l(A, _ne_site(gen_u, sit_h), 0);
  }
}

/* _ne_return(): return the noun in nx_t0, not in parts.
*/
static void
_ne_return(ne_gen* gen_u)
{
  _nx_mov_ri(A, nx_t1, 0);
  _ne_leave(gen_u);
  _nx_ret(A);
}

/* _ne_args(): the arguments of a site to where a nock function takes
**             them, the borrowed ones with a reference more.
*/
static void
_ne_args(ne_gen* gen_u, const u3nc_dire* dir_u)
{
  const c3_h* sot_h = gen_u->pog_u->sot_u.sot_h + dir_u->sot_h;
  const c3_y* kon_y = gen_u->pog_u->sot_u.kon_y + dir_u->sot_h;
  c3_w        len_w = dir_u->len_h, i_w;
  c3_ws*      dst_ws = c3_malloc((len_w + 1) * sizeof(c3_ws));
  c3_ws*      src_ws = c3_malloc((len_w + 1) * sizeof(c3_ws));

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    src_ws[i_w] = gen_u->hom_ws[sot_h[i_w]];
    dst_ws[i_w] = ( i_w < _nx_sot_w ) ? (c3_ws)_nx_sot_y[i_w]
                                      : (c3_ws)(_ne_argm + i_w - _nx_sot_w);
  }
  _ne_moves(gen_u, len_w, dst_ws, src_ws);

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    if ( !kon_y[i_w] ) {
      if ( i_w < _nx_sot_w ) {
        _ne_gain(gen_u, _nx_sot_y[i_w], nx_t0, nx_t1, 0);
      }
      else {
        _nx_ld_l(A, nx_t2, ~(c3_w)0,
                 (c3_d)(uintptr_t)&_ne_dat_u->arg[i_w - _nx_sot_w]);
        _ne_gain(gen_u, nx_t2, nx_t0, nx_t1, 0);
      }
    }
  }

  c3_free(dst_ws);
  c3_free(src_ws);
}

/* _ne_edge(): the label a branch jumps to: its target or, if the jump
**             consumes sources, a stub that releases them and goes on.
*/
static c3_w
_ne_edge(ne_gen* gen_u, const ne_op* op_u, c3_t one_t, c3_t two_t)
{
  c3_w tar_w = gen_u->ops_u[op_u->tar_h].lab_w;

  if ( one_t || two_t ) {
    ne_stub* stu_u = _ne_stub(gen_u, ne_s_edge);

    stu_u->s_h[0] = one_t ? op_u->src_h[0] : _nc_none;
    stu_u->s_h[1] = two_t ? op_u->src_h[1] : _nc_none;
    stu_u->tar_w  = tar_w;
    return stu_u->ent_w;
  }
  return tar_w;
}

/* _ne_cons(): the cell of the nouns in one_y and two_y, transferred, to
**             nx_t0: off the road's stock of cells, as u3a_celloc() takes
**             one, if it has any; else by the helper.  one_y and two_y are
**             not nx_t0, nx_t1 or nx_t4.
*/
static void
_ne_cons(ne_gen* gen_u, c3_y one_y, c3_y two_y)
{
  //  the builds that count cells count them in the allocator: no cell is
  //  made in line there, as u3a_celloc_try() makes none
  //
#if defined(U3NC_STAT) || defined(U3_CPU_DEBUG)
  const c3_t hel_t = 1;
#else
  const c3_t hel_t = 0;
#endif

  if ( hel_t ) {
    _nx_mov_rr(A, nx_a0, one_y);
    _nx_mov_rr(A, nx_a1, two_y);
    _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_cons));
  }
  else {
    ne_stub* stu_u = _ne_stub(gen_u, ne_s_cons);

    stu_u->one_w = _nx_label(A);
    stu_u->two_w = _nx_label(A);
    stu_u->tar_w = _nx_label(A);
    stu_u->bak_w = _nx_label(A);
    stu_u->a_y   = one_y;
    stu_u->b_y   = two_y;

    //  the cell: the next words of the sand zone of the road, if it has
    //  one with room, as u3a_celloc_try() takes them: here for a north
    //  road, whose zone grows up, and out of line for a south one.
    //  Else, also out of line, a cell off the road's cache, or the
    //  helper
    //
    _nx_ld64(A, nx_t4, nx_ctx, _ne_ctx_rod);
    _nx_test_mi8(A, nx_t4, offsetof(u3a_road, how.fag_w), u3a_flag_sand);
    _nx_jcc(A, nx_e, stu_u->ent_w);
    _nx_ld(A, nx_t0, nx_t4, offsetof(u3a_road, san_p));
    _nx_alu_mi(A, nx_cmp, nx_t4, offsetof(u3a_road, hep.dir_ws), 0);
    _nx_jcc(A, nx_s, stu_u->one_w);
    _nx_lea(A, nx_t1, nx_t0, c3_wiseof(u3a_cell));
    _nx_alu_rm(A, nx_cmp, nx_t1, nx_t4, offsetof(u3a_road, rut_p));
    _nx_jcc(A, nx_a, stu_u->tar_w);
    _nx_st(A, nx_t4, offsetof(u3a_road, san_p), nx_t1);
    //  its word offset is in nx_t0
    //
    _nx_bind(A, stu_u->two_w);
#ifdef VERE64
    _nx_shl_ri(A, nx_t0, 2);
#endif
    _nx_stx_i(A, nx_bas, nx_t0, _ne_c_use, 1);
    _nx_stx_i(A, nx_bas, nx_t0, _ne_c_mug, 0);
    _nx_stx(A, nx_bas, nx_t0, _ne_c_hed, one_y);
    _nx_stx(A, nx_bas, nx_t0, _ne_c_tel, two_y);
    _nx_shr_ri(A, nx_t0, 2);
    _ne_alu_k(gen_u, nx_or, nx_t0, u3a_cell_flag);
    _nx_bind(A, stu_u->bak_w);
  }
}

/* _ne_op(): the code of the op i_w; the number of ops after it that it
**           took with it.
*/
static c3_w
_ne_op(ne_gen* gen_u, c3_w i_w)
{
  u3nc_prog* pog_u = gen_u->pog_u;
  ne_op*     op_u  = &gen_u->ops_u[i_w];
  c3_y       kon_y = op_u->kon_y;
  c3_h       a_h   = op_u->src_h[0], b_h = op_u->src_h[1], d_h = op_u->dst_h;
  c3_w       tok_w = _ne_wb * (gen_u->tok_w + op_u->dep_h);

  switch ( op_u->fam_y ) {
    default: {
      gen_u->bad_t = 1;
      gen_u->why_y = op_u->fam_y;
    } break;

    case _nc_bom: {
      _nx_jmp(A, _ne_bail(gen_u, 0));
    } break;

    case _nc_imm: {
      c3_ws hom_ws = gen_u->hom_ws[d_h];

      if ( _ne_isreg(hom_ws) ) {
        _nx_mov_ri(A, hom_ws, op_u->imm_h);
      }
      else {
        _ne_st_i(gen_u, _ne_cell(hom_ws), op_u->imm_h);
      }
    } break;

    case _nc_iml: {
      u3_noun lit   = pog_u->lit_u.non[op_u->imm_h];
      c3_y    out_y = _ne_out(gen_u, d_h, nx_t2);

      _nx_mov_ri(A, out_y, lit);
      if ( c3n == u3a_is_cat(lit) ) {
        _ne_gain(gen_u, out_y, nx_t0, nx_t1, 1);
      }
      _ne_put(gen_u, d_h, out_y);
    } break;

    case _nc_mov: {
      if ( kon_y ) {
        _ne_move(gen_u, gen_u->hom_ws[d_h], gen_u->hom_ws[a_h]);
      }
      else {
        c3_y out_y = _ne_out(gen_u, d_h, nx_t2);

        _ne_move(gen_u, out_y, gen_u->hom_ws[a_h]);
        _ne_gain(gen_u, out_y, nx_t0, nx_t1, 0);
        _ne_put(gen_u, d_h, out_y);
      }
    } break;

    case _nc_inc: {
      c3_y     reg_y = _ne_get(gen_u, a_h, nx_t2);
      c3_y     out_y = _ne_out(gen_u, d_h, nx_t0);
      ne_stub* stu_u = _ne_stub(gen_u, ne_s_inc);

      _ne_alu_k(gen_u, nx_cmp, reg_y, u3a_direct_max);
      _nx_jcc(A, nx_ae, stu_u->ent_w);
      _nx_lea(A, out_y, reg_y, 1);
      stu_u->a_y   = reg_y;
      stu_u->d_y   = out_y;
      stu_u->imm_w = kon_y;
      stu_u->bak_w = _nx_label(A);
      _nx_bind(A, stu_u->bak_w);
      _ne_put(gen_u, d_h, out_y);
    } break;

    case _nc_dec: {
      c3_y     reg_y = _ne_get(gen_u, a_h, nx_t2);
      c3_y     out_y = _ne_out(gen_u, d_h, nx_t0);
      ne_stub* stu_u = _ne_stub(gen_u, ne_s_dec);

      //  one less, and the slow path gets the noun back from it
      //
      _nx_lea(A, out_y, reg_y, -1);
      _ne_alu_k(gen_u, nx_cmp, out_y, u3a_direct_max);
      _nx_jcc(A, nx_ae, stu_u->ent_w);
      stu_u->a_y   = out_y;
      stu_u->d_y   = out_y;
      stu_u->imm_w = kon_y;
      stu_u->bak_w = _nx_label(A);
      _nx_bind(A, stu_u->bak_w);
      _ne_put(gen_u, d_h, out_y);
    } break;

    case _nc_add: {
      c3_y     one_y = _ne_get(gen_u, a_h, nx_t2);
      c3_y     two_y = _ne_get(gen_u, b_h, nx_t3);
      ne_stub* stu_u = _ne_stub(gen_u, ne_s_add);

      _nx_lea2(A, nx_t0, one_y, two_y);
      _nx_mov_rr(A, nx_t1, one_y);
      _nx_alu_rr(A, nx_or, nx_t1, two_y);
      _nx_alu_rr(A, nx_or, nx_t1, nx_t0);
      _nx_jcc(A, nx_s, stu_u->ent_w);
      stu_u->a_y   = one_y;
      stu_u->b_y   = two_y;
      stu_u->imm_w = kon_y;
      stu_u->bak_w = _nx_label(A);
      _nx_bind(A, stu_u->bak_w);
      _ne_put(gen_u, d_h, nx_t0);
    } break;

    //  a slow path of a gain loses nx_a0, so the gains come first
    //
    case _nc_con: {
      c3_y one_y = _ne_get(gen_u, a_h, nx_t2);
      c3_y two_y;

      if ( !(kon_y & 1) ) {
        _ne_gain(gen_u, one_y, nx_t0, nx_t1, 0);
      }
      two_y = _ne_get(gen_u, b_h, nx_t3);
      if ( !(kon_y & 2) ) {
        _ne_gain(gen_u, two_y, nx_t0, nx_t1, 0);
      }
      _ne_cons(gen_u, one_y, two_y);
      _ne_put(gen_u, d_h, nx_t0);
    } break;

    case _nc_hed:
    case _nc_tal: {
      c3_y reg_y = _ne_get(gen_u, a_h, nx_t3);
      c3_y out_y = _ne_out(gen_u, d_h, nx_t2);

      if ( out_y == reg_y ) {
        out_y = nx_t2;
      }
      _nx_mov_rr(A, nx_t0, reg_y);
      _nx_shl_ri(A, nx_t0, 2);
      _nx_ldx(A, out_y, nx_bas, nx_t0,
              (_nc_hed == op_u->fam_y) ? _ne_c_hed : _ne_c_tel);
      _ne_gain(gen_u, out_y, nx_t1, nx_t4, 0);
      if ( kon_y ) {
        c3_w out_w = _nx_label(A);

        _ne_less_off(gen_u, reg_y, nx_t0, nx_t1, out_w);
        _nx_bind(A, out_w);
      }
      _ne_put(gen_u, d_h, out_y);
    } break;

    //  a part of a cell, uncounted: an alias of the cell's reference to
    //  it (_nc_alias()), good while the ops that read it read as the
    //  bytecode does: the sources before any release, and a gain of one
    //  that is kept first thing
    //
    case _nc_hea:
    case _nc_taa: {
      c3_y reg_y = _ne_get(gen_u, a_h, nx_t3);
      c3_y out_y = _ne_out(gen_u, d_h, nx_t2);

      _nx_mov_rr(A, nx_t0, reg_y);
      _nx_shl_ri(A, nx_t0, 2);
      _nx_ldx(A, out_y, nx_bas, nx_t0,
              (_nc_hea == op_u->fam_y) ? _ne_c_hed : _ne_c_tel);
      _ne_put(gen_u, d_h, out_y);
    } break;

    case _nc_her:
    case _nc_tar: {
      c3_y reg_y = _ne_get(gen_u, a_h, nx_t3);
      c3_w sha_w = _nx_label(A), don_w = _nx_label(A), out_w = _nx_label(A);
      c3_t hed_t = (_nc_her == op_u->fam_y);

      _nx_mov_rr(A, nx_t0, reg_y);
      _nx_shl_ri(A, nx_t0, 2);
      _nx_ldx(A, nx_t2, nx_bas, nx_t0, hed_t ? _ne_c_hed : _ne_c_tel);
      _nx_mov_rr(A, nx_t1, nx_t0);
      _nx_alu_rm(A, nx_sub, nx_t1, nx_ctx, _ne_ctx_lo);
      _nx_alu_rm(A, nx_cmp, nx_t1, nx_ctx, _ne_ctx_reu);
      _nx_jcc8(A, nx_ae, sha_w);
      _nx_alu_xi(A, nx_cmp, nx_bas, nx_t0, _ne_c_use, 1);
      _nx_jcc8(A, nx_ne, sha_w);
      //  the only reference: the cell is the token, and loses its other
      //  part
      //
      _nx_st(A, nx_sp, tok_w, reg_y);
      _nx_ldx(A, nx_t4, nx_bas, nx_t0, hed_t ? _ne_c_tel : _ne_c_hed);
      _ne_less(gen_u, nx_t4, nx_t0, nx_t1, 0);
      _nx_jmp8(A, don_w);
      _nx_bind(A, sha_w);
      _nx_st_i(A, nx_sp, tok_w, 0);
      _ne_gain(gen_u, nx_t2, nx_t1, nx_t4, 0);
      _ne_less_off(gen_u, reg_y, nx_t0, nx_t1, out_w);
      _nx_bind(A, out_w);
      _nx_bind(A, don_w);
      _ne_put(gen_u, d_h, nx_t2);
    } break;

    case _nc_cor: {
      c3_w con_w = _nx_label(A), don_w = _nx_label(A);

      c3_y one_y = _ne_get(gen_u, a_h, nx_t2);
      c3_y two_y;

      if ( !(kon_y & 1) ) {
        _ne_gain(gen_u, one_y, nx_t0, nx_t1, 0);
      }
      two_y = _ne_get(gen_u, b_h, nx_t3);
      if ( !(kon_y & 2) ) {
        _ne_gain(gen_u, two_y, nx_t0, nx_t1, 0);
      }
      //  the token is under the top: COR pops it.  The parts wait where
      //  the cons wants them: not in nx_t0, nx_t1 or nx_t4
      //
      _nx_ld(A, nx_t0, nx_sp, tok_w - _ne_wb);
      _nx_test_rr(A, nx_t0, nx_t0);
      _nx_jcc8(A, nx_e, con_w);
      _nx_mov_rr(A, nx_t1, nx_t0);
      _nx_shl_ri(A, nx_t1, 2);
      _nx_stx_i(A, nx_bas, nx_t1, _ne_c_mug, 0);
      _nx_stx(A, nx_bas, nx_t1, _ne_c_hed, one_y);
      _nx_stx(A, nx_bas, nx_t1, _ne_c_tel, two_y);
      _nx_jmp8(A, don_w);
      _nx_bind(A, con_w);
      _ne_cons(gen_u, one_y, two_y);
      _nx_bind(A, don_w);
      _ne_put(gen_u, d_h, nx_t0);
    } break;

    case _nc_cel: {
      _ne_cmp_i(gen_u, a_h, u3a_cell_flag);
      _nx_jcc(A, nx_b, _ne_bail(gen_u, 0));
      if ( kon_y ) {
        _ne_drop(gen_u, a_h, 1);
      }
    } break;

    case _nc_lob: {
      _ne_cmp_i(gen_u, a_h, 1);
      _nx_jcc(A, nx_a, _ne_bail(gen_u, 0));
    } break;

    case _nc_clq: {
      _ne_cmp_i(gen_u, a_h, u3a_cell_flag);
      _nx_jcc(A, nx_b, _ne_edge(gen_u, op_u, kon_y & 2, 0));
      if ( kon_y & 1 ) {
        _ne_drop(gen_u, a_h, 1);
      }
    } break;

    case _nc_eqi: {
      _ne_cmp_i(gen_u, a_h, op_u->imm_h);
      _nx_jcc(A, nx_ne, _ne_edge(gen_u, op_u, kon_y & 2, 0));
    } break;

    case _nc_lti: {
      _ne_cmp_i(gen_u, a_h, pog_u->lit_u.non[op_u->imm_h]);
      _nx_jcc(A, nx_ae, _ne_edge(gen_u, op_u, kon_y & 2, 0));
    } break;

    case _nc_eql: {
      u3_noun  lit   = pog_u->lit_u.non[op_u->imm_h];
      c3_y     reg_y = _ne_get(gen_u, a_h, nx_t2);
      ne_stub* stu_u = _ne_stub(gen_u, ne_s_eql);

      _ne_alu_k(gen_u, nx_cmp, reg_y, lit);
      _nx_jcc(A, nx_ne, stu_u->ent_w);
      stu_u->a_y    = reg_y;
      stu_u->imm_w  = lit;
      stu_u->s_h[0] = (kon_y & 2) ? a_h : _nc_none;
      stu_u->tar_w  = gen_u->ops_u[op_u->tar_h].lab_w;
      stu_u->bak_w  = _nx_label(A);
      _nx_bind(A, stu_u->bak_w);
      if ( kon_y & 1 ) {
        _ne_less(gen_u, reg_y, nx_t0, nx_t1, 0);
      }
    } break;

    case _nc_eqq: {
      c3_y     one_y = _ne_get(gen_u, a_h, nx_t2);
      c3_y     two_y = _ne_get(gen_u, b_h, nx_t3);
      ne_stub* stu_u = _ne_stub(gen_u, ne_s_eqq);

      _nx_alu_rr(A, nx_cmp, one_y, two_y);
      _nx_jcc(A, nx_ne, stu_u->ent_w);
      stu_u->a_y    = one_y;
      stu_u->b_y    = two_y;
      stu_u->s_h[0] = (kon_y & 2) ? a_h : _nc_none;
      stu_u->s_h[1] = (kon_y & 8) ? b_h : _nc_none;
      stu_u->tar_w  = gen_u->ops_u[op_u->tar_h].lab_w;
      stu_u->bak_w  = _nx_label(A);
      _nx_bind(A, stu_u->bak_w);
      if ( kon_y & 1 ) {
        _ne_less(gen_u, one_y, nx_t0, nx_t1, 0);
      }
      if ( kon_y & 4 ) {
        _ne_less(gen_u, two_y, nx_t0, nx_t1, 0);
      }
    } break;

    case _nc_brn: {
      _ne_cmp_i(gen_u, a_h, 1);
      _nx_jcc(A, nx_a, _ne_bail(gen_u, 0));
      _nx_jcc(A, nx_e, gen_u->ops_u[op_u->tar_h].lab_w);
    } break;

    case _nc_brz: {
      _ne_cmp_i(gen_u, a_h, 0);
      _nx_jcc(A, nx_ne, _ne_edge(gen_u, op_u, kon_y & 2, 0));
    } break;

    case _nc_hop: {
      if ( op_u->tar_h != (i_w + 1) ) {
        _nx_jmp(A, gen_u->ops_u[op_u->tar_h].lab_w);
      }
    } break;

    case _nc_dro: {
      _ne_drop(gen_u, a_h, 0);
    } break;

    case _nc_don: {
      _ne_move(gen_u, nx_t0, gen_u->hom_ws[a_h]);
      _nx_mov_ri(A, nx_t1, 0);
      _ne_leave(gen_u);
      _nx_ret(A);
    } break;

    //  RTS.. DOX: the parts to the slot registers and the product to its
    //  own, at once; the shape tells the caller it came in parts.  An RTS
    //  with other ops between it and the DOX leaves its part in the
    //  frame, where the DOX finds it.
    //
    case _nc_rts:
    case _nc_dox: {
      c3_ws dst_ws[_nc_ret_max + 1], src_ws[_nc_ret_max + 1];
      c3_y  gan_y[_nc_ret_max + 1] = {0};
      c3_y  hav_y[_nc_ret_max + 1] = {0};
      c3_w  run_w = _ne_rts_run(gen_u, i_w);
      c3_w  len_w = 0, j_w;
      c3_ws sap_ws;
      u3_noun sap;

      if ( ~(c3_w)0 == run_w ) {
        c3_y reg_y = _ne_get(gen_u, a_h, nx_t2);

        if ( !op_u->imm_h || (op_u->imm_h > _nx_sot_w) ) {
          gen_u->bad_t = 1;
          gen_u->why_y = _nc_rts;
          break;
        }
        if ( !kon_y ) {
          _ne_gain(gen_u, reg_y, nx_t0, nx_t1, 0);
        }
        _nx_st(A, nx_sp, _ne_stage(gen_u, op_u->imm_h), reg_y);
        break;
      }

      sap    = pog_u->lit_u.non[op_u[run_w].imm_h];
      sap_ws = _ne_sap_len(sap);
      if ( (sap_ws < 0) || (sap_ws > (c3_ws)_nc_ret_max) ) {
        gen_u->bad_t = 1;
        gen_u->why_y = _nc_dox;
        break;
      }

      for ( j_w = 0; j_w < run_w; j_w++ ) {
        c3_h par_h = op_u[j_w].imm_h;

        if ( !par_h || (par_h > _nx_sot_w) || hav_y[par_h] ) {
          gen_u->bad_t = 1;
          gen_u->why_y = _nc_rts;
          break;
        }
        hav_y[par_h]  = 1;
        dst_ws[len_w] = _nx_sot_y[par_h - 1];
        src_ws[len_w] = gen_u->hom_ws[op_u[j_w].src_h[0]];
        gan_y[len_w]  = !op_u[j_w].kon_y;
        len_w++;
      }
      if ( gen_u->bad_t ) {
        break;
      }
      //  the parts no RTS of the run gave are in the frame
      //
      for ( j_w = 1; (c3_ws)j_w < sap_ws; j_w++ ) {
        if ( !hav_y[j_w] ) {
          if ( gen_u->stg_w == gen_u->cel_w ) {
            gen_u->bad_t = 1;
            gen_u->why_y = _nc_dox;
            break;
          }
          dst_ws[len_w] = _nx_sot_y[j_w - 1];
          src_ws[len_w] = -(c3_ws)(gen_u->stg_w + j_w - 1 + 1);
          len_w++;
        }
      }
      if ( gen_u->bad_t ) {
        break;
      }
      dst_ws[len_w] = nx_t0;
      src_ws[len_w] = gen_u->hom_ws[op_u[run_w].src_h[0]];
      _ne_moves(gen_u, len_w + 1, dst_ws, src_ws);
      for ( j_w = 0; j_w < len_w; j_w++ ) {
        if ( gan_y[j_w] ) {
          //  the product waits in nx_t0
          //
          _ne_gain(gen_u, dst_ws[j_w], nx_t1, nx_t2, 0);
        }
      }
      _nx_mov_ri(A, nx_t1, sap);
      _ne_leave(gen_u);
      _nx_ret(A);
      return run_w;
    }

    case _nc_cal:
    case _nc_jmp: {
      const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[op_u->imm_h]);

      if (  (dir_u->len_h > _ne_arg_max)
         || ((_nc_jmp == op_u->fam_y) && kon_y) )
      {
        gen_u->bad_t = 1;
        gen_u->why_y = op_u->fam_y;
        break;
      }
      _ne_args(gen_u, dir_u);
      _ne_call(gen_u, op_u->imm_h, _nc_jmp == op_u->fam_y);
      if ( _nc_cal == op_u->fam_y ) {
        return _ne_result(gen_u, i_w);
      }
    } break;

    //  a memoized call: the memo looked up on the arguments, uncounted, in
    //  the frame; a hit releases the ones the site consumes; a miss calls,
    //  and saves the product, consed if it came in parts, under the key
    //  the lookup left after the arguments
    //
    case _nc_cam: {
      const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[op_u->imm_h]);
      c3_w             mis_w = _nx_label(A), don_w = _nx_label(A);

      if ( dir_u->len_h > _ne_arg_max ) {
        gen_u->bad_t = 1;
        gen_u->why_y = op_u->fam_y;
        break;
      }
      _ne_gather(gen_u, dir_u);
      _ne_helper(gen_u, ne_h_cam, op_u->imm_h);
      _nx_alu_ri(A, nx_cmp, nx_t0, u3_none);
      _nx_jcc(A, nx_e, mis_w);
      _nx_mov_rr(A, nx_t3, nx_t0);
      _ne_release(gen_u, dir_u);
      _nx_jmp(A, don_w);
      _nx_bind(A, mis_w);
      _ne_args(gen_u, dir_u);
      _ne_call(gen_u, op_u->imm_h, 0);
      _ne_boxed(gen_u);
      _nx_mov_rr(A, nx_a2, nx_t0);
      _nx_ld(A, nx_a1, nx_sp, _ne_wb * (gen_u->jet_w + dir_u->len_h));
      _nx_mov_ri(A, nx_a0, dir_u->cid_h);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_save));
      _nx_mov_rr(A, nx_t3, nx_t0);
      _nx_bind(A, don_w);
      if ( _nc_none != d_h ) {
        _ne_put(gen_u, d_h, nx_t3);
      }
    } break;

    //  a memoized call by subject
    //
    case _nc_csm: {
      const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[op_u->imm_h]);
      c3_h             sub_h = pog_u->sot_u.sot_h[dir_u->sot_h];
      c3_t             kos_t = pog_u->sot_u.kon_y[dir_u->sot_h];
      c3_w             mis_w = _nx_label(A), don_w = _nx_label(A);
      c3_y             reg_y = _ne_get(gen_u, sub_h, nx_t2);

      _nx_mov_rr(A, nx_a2, reg_y);
      _nx_lea64(A, nx_a3, nx_sp, _ne_wb * gen_u->jet_w);
      _ne_helper(gen_u, ne_h_csm, op_u->imm_h);
      _nx_alu_ri(A, nx_cmp, nx_t0, u3_none);
      _nx_jcc(A, nx_e, mis_w);
      _nx_mov_rr(A, nx_t3, nx_t0);
      if ( kos_t ) {
        _ne_drop(gen_u, sub_h, 0);
      }
      _nx_jmp(A, don_w);
      _nx_bind(A, mis_w);
      reg_y = _ne_get(gen_u, sub_h, nx_t2);
      if ( !kos_t ) {
        _ne_gain(gen_u, reg_y, nx_t0, nx_t1, 0);
      }
      _nx_mov_rr(A, _nx_sot_y[0], reg_y);
      _ne_call(gen_u, op_u->imm_h, 0);
      _ne_boxed(gen_u);
      _nx_mov_rr(A, nx_a2, nx_t0);
      _nx_ld(A, nx_a1, nx_sp, _ne_wb * gen_u->jet_w);
      _nx_mov_ri(A, nx_a0, dir_u->cid_h);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_save));
      _nx_mov_rr(A, nx_t3, nx_t0);
      _nx_bind(A, don_w);
      if ( _nc_none != d_h ) {
        _ne_put(gen_u, d_h, nx_t3);
      }
    } break;

    //  a call by subject: the u3w jet of the site, if it has one, on the
    //  subject, which it takes unless it punts; else the unary program
    //
    case _nc_cap:
    case _nc_jsp: {
      const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[op_u->imm_h]);
      c3_h             sub_h = pog_u->sot_u.sot_h[dir_u->sot_h];
      c3_t             kos_t = pog_u->sot_u.kon_y[dir_u->sot_h];
      c3_t             tal_t = (_nc_jsp == op_u->fam_y);
      c3_t             jet_t = dir_u->arm_u && dir_u->arm_u->mon_f;
      c3_w             don_w = _nx_label(A);
      c3_y             reg_y = _ne_get(gen_u, sub_h, nx_t2);

      if ( tal_t && kon_y ) {
        gen_u->bad_t = 1;
        gen_u->why_y = op_u->fam_y;
        break;
      }
      if ( !tal_t && !kos_t ) {
        _ne_gain(gen_u, reg_y, nx_t0, nx_t1, 0);
      }
      if ( jet_t ) {
        _nx_mov_rr(A, nx_a2, reg_y);
        _ne_helper(gen_u, ne_h_cap, op_u->imm_h);
        _nx_alu_ri(A, nx_cmp, nx_t0, u3_none);
        if ( tal_t ) {
          c3_w pun_w = _nx_label(A);

          _nx_jcc8(A, nx_e, pun_w);
          _ne_return(gen_u);
          _nx_bind(A, pun_w);
        }
        else {
          _nx_jcc(A, nx_ne, don_w);
        }
      }
      _nx_mov_rr(A, _nx_sot_y[0], reg_y);
      _ne_call(gen_u, op_u->imm_h, tal_t);
      if ( !tal_t ) {
        _ne_boxed(gen_u);
        _nx_bind(A, don_w);
        if ( _nc_none != d_h ) {
          _ne_put(gen_u, d_h, nx_t0);
        }
        else {
          _nx_mov_rr(A, nx_t2, nx_t0);
          _ne_less(gen_u, nx_t2, nx_t0, nx_t1, 0);
        }
      }
    } break;

    //  .*(u f): the entry program looked up, by a helper that tells what
    //  to call; then the references change hands, and the call
    //
    case _nc_nok: {
      c3_y one_y = _ne_get(gen_u, a_h, nx_t2);
      c3_y two_y = _ne_get(gen_u, b_h, nx_t3);

      _nx_mov_rr(A, nx_a0, one_y);
      _nx_mov_rr(A, nx_a1, two_y);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_nok));
      //  the address waits in nx_t0: the slow paths of the counts keep it
      //
      if ( !(kon_y & 1) ) {
        _ne_gain(gen_u, one_y, nx_t1, nx_t4, 0);
      }
      if ( kon_y & 2 ) {
        _ne_less(gen_u, two_y, nx_t1, nx_t4, 0);
      }
      _nx_mov_rr(A, _nx_sot_y[0], one_y);
      _nx_call_r(A, nx_t0);
      _ne_boxed(gen_u);
      _ne_put(gen_u, d_h, nx_t0);
    } break;

    case _nc_equ: {
      c3_y one_y = _ne_get(gen_u, a_h, nx_t2);
      c3_y two_y = _ne_get(gen_u, b_h, nx_t3);

      _nx_mov_rr(A, nx_a0, one_y);
      _nx_mov_rr(A, nx_a1, two_y);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_sing));
      if ( kon_y & 1 ) {
        _ne_less(gen_u, one_y, nx_t0, nx_t1, 0);
      }
      if ( kon_y & 2 ) {
        _ne_less(gen_u, two_y, nx_t0, nx_t1, 0);
      }
    } break;

    case _nc_spy: {
      c3_y one_y = _ne_get(gen_u, a_h, nx_t2);
      c3_y two_y = _ne_get(gen_u, b_h, nx_t3);

      _nx_mov_rr(A, nx_a0, one_y);
      _nx_mov_rr(A, nx_a1, two_y);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_spy));
      //  the slow paths of the drops keep nx_t0
      //
      if ( kon_y & 1 ) {
        _ne_less(gen_u, one_y, nx_t1, nx_t4, 0);
      }
      if ( kon_y & 2 ) {
        _ne_less(gen_u, two_y, nx_t1, nx_t4, 0);
      }
      _ne_put(gen_u, d_h, nx_t0);
    } break;

    //  the hints: helpers; a token goes to its cell of the frame
    //
    case _nc_hmp: {
      const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[op_u->imm_h]);

      _ne_gather(gen_u, dir_u);
      _ne_helper(gen_u, ne_h_hmp, op_u->imm_h);
    } break;

    case _nc_hme:
    case _nc_hce: {
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_tdrop));
    } break;

    case _nc_hcp:
    case _nc_hlp: {
      _nx_mov_ri(A, nx_a0, pog_u->lit_u.non[op_u->imm_h]);
      _nx_call_l(A, ~(c3_w)0,
                 _ne_hel((_nc_hcp == op_u->fam_y) ? ne_h_hcp : ne_h_hlp));
      if ( _nc_hlp == op_u->fam_y ) {
        _nx_st_i(A, nx_sp, tok_w, 0);
      }
    } break;

    case _nc_hsp: {
      _nx_mov_ri(A, nx_a0, pog_u->lit_u.non[op_u->imm_h]);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_hsp));
      _nx_st(A, nx_sp, tok_w, nx_t0);
    } break;

    case _nc_hse: {
      _nx_mov_ri(A, nx_a0, pog_u->lit_u.non[op_u->imm_h]);
      _nx_ld(A, nx_a1, nx_sp, tok_w - _ne_wb);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_hse));
    } break;

    case _nc_hdp: {
      c3_y reg_y = _ne_get(gen_u, a_h, nx_t2);

      if ( !kon_y ) {
        _ne_gain(gen_u, reg_y, nx_t0, nx_t1, 0);
      }
      _nx_mov_rr(A, nx_a1, reg_y);
      _nx_mov_ri(A, nx_a0, pog_u->lit_u.non[op_u->imm_h]);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_hdp));
      _nx_st(A, nx_sp, tok_w, nx_t0);
    } break;

    case _nc_hde: {
      _nx_mov_ri(A, nx_a0, pog_u->lit_u.non[op_u->imm_h]);
      _nx_ld(A, nx_a1, nx_sp, tok_w - _ne_wb);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_hde));
      if ( kon_y ) {
        _ne_drop(gen_u, a_h, 0);
      }
    } break;

    //  a total array jet: the arguments in the frame, uncounted; the
    //  consumed ones are released after it.  A transferring one takes
    //  the references of the consumed ones itself.
    //
    case _nc_caf:
    case _nc_jmf:
    case _nc_cax:
    case _nc_jmx: {
      const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[op_u->imm_h]);
      c3_t             tal_t = (_nc_jmf == op_u->fam_y)
                            || (_nc_jmx == op_u->fam_y);
      c3_t             own_t = (_nc_cax == op_u->fam_y)
                            || (_nc_jmx == op_u->fam_y);

      if ( tal_t && kon_y ) {
        gen_u->bad_t = 1;
        gen_u->why_y = op_u->fam_y;
        break;
      }
      _ne_gather(gen_u, dir_u);
      _nx_mov_rr64(A, nx_a0, nx_a2);
      //  the driver called as the C function it is: no slot that is
      //  live here is in a register it may use (_ne_homes())
      //
      if ( own_t ) {
        _nx_mov_ri64(A, nx_a1,
                     (c3_d)(uintptr_t)(pog_u->sot_u.kon_y + dir_u->sot_h));
        _nx_mov_ri64(A, nx_t0, (c3_d)(uintptr_t)dir_u->arm_u->own_f);
      }
      else {
        _nx_mov_ri64(A, nx_t0, (c3_d)(uintptr_t)dir_u->arm_u->arg_f);
      }
      _nx_call_r(A, nx_t0);
      _nx_mov_rr(A, nx_t3, nx_t0);
      if ( !own_t ) {
        _ne_release(gen_u, dir_u);
      }
      if ( tal_t ) {
        _nx_mov_rr(A, nx_t0, nx_t3);
        _ne_return(gen_u);
      }
      else if ( _nc_none != d_h ) {
        _ne_put(gen_u, d_h, nx_t3);
      }
    } break;

    //  a total array jet under the flags of its arm: a helper runs the
    //  driver, tested or not, or, the arm off, leaves the core for the
    //  unary program, which is called as by subject
    //
    case _nc_cac:
    case _nc_jmc: {
      const u3nc_dire* dir_u = &(pog_u->dir_u.dat_u[op_u->imm_h]);
      c3_t             tal_t = (_nc_jmc == op_u->fam_y);
      c3_w             off_w = _nx_label(A), don_w = _nx_label(A);

      if ( tal_t && kon_y ) {
        gen_u->bad_t = 1;
        gen_u->why_y = op_u->fam_y;
        break;
      }
      _ne_gather(gen_u, dir_u);
      _ne_helper(gen_u, ne_h_cac, op_u->imm_h);
      _nx_mov_rr(A, nx_t3, nx_t0);
      _ne_release(gen_u, dir_u);
      _nx_alu_ri(A, nx_cmp, nx_t3, u3_none);
      _nx_jcc(A, nx_e, off_w);
      if ( tal_t ) {
        _nx_mov_rr(A, nx_t0, nx_t3);
        _ne_return(gen_u);
      }
      else {
        _nx_mov_rr(A, nx_t0, nx_t3);
        _nx_jmp(A, don_w);
      }
      _nx_bind(A, off_w);
      _ne_helper(gen_u, ne_h_mono, op_u->imm_h);
      _nx_ld(A, _nx_sot_y[0], nx_sp, _ne_wb * gen_u->jet_w);
      if ( tal_t ) {
        _ne_leave(gen_u);
        _nx_jmp_r(A, nx_t0);
      }
      else {
        _nx_call_r(A, nx_t0);
        _ne_boxed(gen_u);
        _nx_bind(A, don_w);
        if ( _nc_none != d_h ) {
          _ne_put(gen_u, d_h, nx_t0);
        }
        else {
          _nx_mov_rr(A, nx_t2, nx_t0);
          _ne_less(gen_u, nx_t2, nx_t0, nx_t1, 0);
        }
      }
    } break;
  }

  return 0;
}

/* _ne_stubs(): the slow paths.
*/
static void
_ne_stubs(ne_gen* gen_u)
{
  c3_w i_w;

  for ( i_w = 0; i_w < gen_u->stn_w; i_w++ ) {
    ne_stub stu_u = gen_u->stu_u[i_w];

    _nx_bind(A, stu_u.ent_w);

    switch ( stu_u.kin_y ) {
      case ne_s_gain:
      case ne_s_lose: {
        _nx_mov_rr(A, nx_a0, stu_u.a_y);
        _nx_call_l(A, ~(c3_w)0,
                   _ne_hel((ne_s_gain == stu_u.kin_y) ? ne_h_gain : ne_h_lose));
        _nx_jmp(A, stu_u.bak_w);
      } break;

      case ne_s_inc: {
        _nx_mov_rr(A, nx_a0, stu_u.a_y);
        _nx_mov_ri(A, nx_a1, stu_u.imm_w);
        _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_inc));
        _nx_mov_rr(A, stu_u.d_y, nx_t0);
        _nx_jmp(A, stu_u.bak_w);
      } break;

      case ne_s_dec: {
        _nx_lea(A, nx_a0, stu_u.a_y, 1);
        _nx_mov_ri(A, nx_a1, stu_u.imm_w);
        _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_dec));
        _nx_mov_rr(A, stu_u.d_y, nx_t0);
        _nx_jmp(A, stu_u.bak_w);
      } break;

      case ne_s_add: {
        _nx_mov_rr(A, nx_a0, stu_u.a_y);
        _nx_mov_rr(A, nx_a1, stu_u.b_y);
        _nx_mov_ri(A, nx_a2, stu_u.imm_w);
        _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_add));
        _nx_jmp(A, stu_u.bak_w);
      } break;

      //  not the same noun: equal if both are indirect and u3r_sing()
      //  says so
      //
      case ne_s_eqq:
      case ne_s_eql: {
        c3_w no_w  = _nx_label(A);
        c3_t eql_t = (ne_s_eql == stu_u.kin_y);

        if ( eql_t ) {
          if ( c3n == u3a_is_cat(stu_u.imm_w) ) {
            _nx_test_rr(A, stu_u.a_y, stu_u.a_y);
            _nx_jcc8(A, nx_ns, no_w);
            _nx_mov_rr(A, nx_a1, stu_u.a_y);
            _nx_mov_ri(A, nx_a0, stu_u.imm_w);
            _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_sing));
            _nx_test_rr(A, nx_t0, nx_t0);
            _nx_jcc(A, nx_e, stu_u.bak_w);
          }
        }
        else {
          _nx_mov_rr(A, nx_t0, stu_u.a_y);
          _nx_alu_rr(A, nx_and, nx_t0, stu_u.b_y);
          _nx_jcc8(A, nx_ns, no_w);
          _nx_mov_rr(A, nx_a0, stu_u.a_y);
          _nx_mov_rr(A, nx_a1, stu_u.b_y);
          _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_sing));
          _nx_test_rr(A, nx_t0, nx_t0);
          _nx_jcc(A, nx_e, stu_u.bak_w);
        }
        _nx_bind(A, no_w);
        if ( _nc_none != stu_u.s_h[0] ) {
          _ne_drop(gen_u, stu_u.s_h[0], 0);
        }
        if ( _nc_none != stu_u.s_h[1] ) {
          _ne_drop(gen_u, stu_u.s_h[1], 0);
        }
        _nx_jmp(A, stu_u.tar_w);
      } break;

      case ne_s_edge: {
        if ( _nc_none != stu_u.s_h[0] ) {
          _ne_drop(gen_u, stu_u.s_h[0], 0);
        }
        if ( _nc_none != stu_u.s_h[1] ) {
          _ne_drop(gen_u, stu_u.s_h[1], 0);
        }
        _nx_jmp(A, stu_u.tar_w);
      } break;

      case ne_s_link: {
        _nx_lea64_l(A, nx_a1, stu_u.tar_w);
        _nx_jmp(A, gen_u->lin_w);
      } break;

      //  the parts to _nc_ret, and the product consed of them
      //
      case ne_s_box: {
        c3_w j_w;

        _nx_ld64_l(A, nx_t2, ~(c3_w)0, _ne_hel(ne_h_ret));
        for ( j_w = 0; j_w < _nx_sot_w; j_w++ ) {
          _nx_st(A, nx_t2, _ne_wb * (j_w + 1), _nx_sot_y[j_w]);
        }
        _nx_mov_rr(A, nx_a0, nx_t1);
        _nx_mov_rr(A, nx_a1, nx_t0);
        _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_box));
        _nx_jmp(A, stu_u.bak_w);
      } break;

      //  no zone with room: a cell off the cache of the road, whose
      //  word offset goes back in nx_t0; a south road: its zone grows
      //  down; neither: the helper, whose cell goes back whole
      //
      case ne_s_cons: {
        c3_w hel_w = stu_u.tar_w;

        _nx_ld(A, nx_t1, nx_t4, offsetof(u3a_road, cel.hav_w));
        _nx_test_rr(A, nx_t1, nx_t1);
        _nx_jcc8(A, nx_e, hel_w);
        _nx_dec_r(A, nx_t1);
        _nx_st(A, nx_t4, offsetof(u3a_road, cel.hav_w), nx_t1);
        _nx_ld(A, nx_t0, nx_t4, offsetof(u3a_road, cel.cel_p));
        _nx_lea64w(A, nx_t0, nx_bas, nx_t0);
        _nx_ldw(A, nx_t0, nx_t0, nx_t1, 0);
        _nx_jmp(A, stu_u.two_w);

        _nx_bind(A, stu_u.one_w);
        _nx_lea(A, nx_t1, nx_t0, -(int32_t)c3_wiseof(u3a_cell));
        _nx_alu_rm(A, nx_cmp, nx_t1, nx_t4, offsetof(u3a_road, rut_p));
        _nx_jcc8(A, nx_b, hel_w);
        _nx_st(A, nx_t4, offsetof(u3a_road, san_p), nx_t1);
        _nx_mov_rr(A, nx_t0, nx_t1);
        _nx_jmp(A, stu_u.two_w);

        _nx_bind(A, hel_w);
        _nx_mov_rr(A, nx_a0, stu_u.a_y);
        _nx_mov_rr(A, nx_a1, stu_u.b_y);
        _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_cons));
        _nx_jmp(A, stu_u.bak_w);
      } break;

      //  the run has gone deep: noted, once
      //
      case ne_s_deep: {
        _nx_mov_rr64(A, nx_a0, nx_ctx);
        _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_deep));
        _nx_jmp(A, stu_u.bak_w);
      } break;

      case ne_s_bail: {
        static const c3_h mot_h[3] = { c3__exit, c3__fail, c3__meme };

        _nx_mov_ri(A, nx_a0, mot_h[stu_u.imm_w]);
        _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_bail));
        _nx_trap(A);
      } break;
    }
  }
}

/* _ne_code(): the function: the start, the ops, the slow paths, the cells.
*/
static void
_ne_code(ne_gen* gen_u)
{
  u3nc_prog* pog_u = gen_u->pog_u;
  c3_w       i_w;

  gen_u->lin_w = _nx_label(A);
  gen_u->pog_w = _nx_label(A);

  //  the frame; the stack checked, in a function that calls; the
  //  parameters that live in the frame, and those that came in memory
  //
  _nx_alu_ri64(A, nx_sub, nx_sp, gen_u->fra_w);
  if ( gen_u->cal_t ) {
    ne_stub* stu_u = _ne_stub(gen_u, ne_s_deep);

    _nx_alu_rm64(A, nx_cmp, nx_sp, nx_ctx, _ne_ctx_lim);
    _nx_jcc(A, nx_b, stu_u->ent_w);
    stu_u->bak_w = _nx_label(A);
    _nx_bind(A, stu_u->bak_w);
  }
  for ( i_w = 0; (i_w < pog_u->arg_h) && (i_w < _nx_sot_w); i_w++ ) {
    _ne_move(gen_u, gen_u->hom_ws[i_w], _nx_sot_y[i_w]);
  }
  for ( ; i_w < pog_u->arg_h; i_w++ ) {
    c3_y reg_y = _ne_out(gen_u, i_w, nx_t0);

    _nx_ld_l(A, reg_y, ~(c3_w)0,
             (c3_d)(uintptr_t)&_ne_dat_u->arg[i_w - _nx_sot_w]);
    _ne_put(gen_u, i_w, reg_y);
  }

  for ( i_w = 0; (i_w < gen_u->opn_w) && !gen_u->bad_t; i_w++ ) {
    ne_op* op_u = &gen_u->ops_u[i_w];

    if ( op_u->led_y ) {
      _nx_bind(A, op_u->lab_w);
    }
    if ( _ne_cnt_t ) {
      //  the ops it will take with it are counted with it, before it:
      //  it may end in a return
      //
      c3_w mor_w = 0, k_w;
      c3_t mix_t;

      if ( (_nc_rts == op_u->fam_y) || (_nc_dox == op_u->fam_y) ) {
        mor_w = _ne_rts_run(gen_u, i_w);
        mor_w = ( ~(c3_w)0 == mor_w ) ? 0 : mor_w;
      }
      else if ( (_nc_cal == op_u->fam_y) ) {
        mor_w = _ne_rtl_run(gen_u, i_w, &mix_t);
      }
      for ( k_w = 0; k_w <= mor_w; k_w++ ) {
        _nx_inc64_l(A, (c3_d)(uintptr_t)&_ne_dat_u->cnt_d[op_u[k_w].fam_y]);
      }
    }
    {
      i_w += _ne_op(gen_u, i_w);
    }
  }
  _nx_trap(A);

  if ( gen_u->bad_t ) {
    return;
  }

  //  the link stubs of the sites, then the slow paths
  //
  {
    c3_w* stu_w = c3_calloc((pog_u->dir_u.len_h + 1) * sizeof(c3_w));

    for ( i_w = 0; i_w < pog_u->dir_u.len_h; i_w++ ) {
      if ( gen_u->sit_w[i_w] ) {
        ne_stub* stu_u = _ne_stub(gen_u, ne_s_link);

        stu_u->tar_w = gen_u->sit_w[i_w] - 1;
        stu_w[i_w]   = stu_u->ent_w;
      }
    }

    _ne_stubs(gen_u);

    if ( gen_u->sin_w ) {
      //  the common part of the link stubs: the cell in nx_a1
      //
      _nx_bind(A, gen_u->lin_w);
      _nx_alu_ri64(A, nx_sub, nx_sp, 8);
      _nx_ld64_l(A, nx_a0, gen_u->pog_w, 0);
      _nx_call_l(A, ~(c3_w)0, _ne_hel(ne_h_link));
      _nx_alu_ri64(A, nx_add, nx_sp, 8);
      _nx_jmp_r(A, nx_t0);
    }
    if ( gen_u->sin_w || gen_u->pog_t ) {
      while ( A->len_w & 7 ) {
        _nx_b(A, 0xcc);
      }
      _nx_bind(A, gen_u->pog_w);
      _nx_quad(A, (c3_d)(uintptr_t)pog_u);
      for ( i_w = 0; i_w < pog_u->dir_u.len_h; i_w++ ) {
        if ( gen_u->sit_w[i_w] ) {
          _nx_bind(A, gen_u->sit_w[i_w] - 1);
          _nx_quad_l(A, stu_w[i_w]);
          _nx_quad(A, i_w | ((c3_d)gen_u->tai_y[i_w] << 32));
          _nx_quad_l(A, gen_u->ins_w[i_w]);
        }
      }
    }
    c3_free(stu_w);
  }
}

/* _ne_lower(): make the code of a program; whether it could.
*/
static c3_t
_ne_lower(u3nc_prog* pog_u)
{
  ne_gen          gen_u = {0};
  struct timespec beg_u, end_u;
  c3_t            bad_t;
  c3_y*           cod_y = 0;

  {
    const nj_side* sid_u = _nj_side(pog_u);

    if ( sid_u && sid_u->emi_f ) {
      return (_ne_fail != sid_u->emi_f);
    }
  }
  //  machine code keeps no empty slot zero, which a build that zeroes
  //  them relies on (a drop of a slot that may be empty, FGT): no program
  //  is lowered there
  //
  if ( _nc_zer_t || !_ne_init() ) {
    _nj_side_get(pog_u)->emi_f = _ne_fail;
    return 0;
  }
  //  the arena is full, or a change of it was abandoned: nothing more
  //  until it is evicted, which a caller that is not machine code does
  //  here
  //
  if ( _ne_att_y && !_ne_evict_idle() ) {
    return 0;
  }
  clock_gettime(CLOCK_MONOTONIC, &beg_u);

  gen_u.pog_u = pog_u;
  gen_u.sit_w = c3_calloc((pog_u->dir_u.len_h + 1) * sizeof(c3_w));
  gen_u.ins_w = c3_calloc((pog_u->dir_u.len_h + 1) * sizeof(c3_w));
  gen_u.tai_y = c3_calloc(pog_u->dir_u.len_h + 1);

  if ( (pog_u->arg_h > _ne_arg_max) || !pog_u->byc_u.len_h ) {
    gen_u.bad_t = 1;
    gen_u.why_y = _nc_nop;
  }
  else {
    _ne_decode(&gen_u);
    _ne_homes(&gen_u);
    if ( !gen_u.bad_t ) {
      _ne_code(&gen_u);
    }
  }
  bad_t = gen_u.bad_t;

  if ( bad_t ) {
    _nj_side_get(pog_u)->emi_f = _ne_fail;
  }
  else if ( !_ne_open() ) {
    bad_t = 1;
  }
  else if ( !(cod_y = _ne_place(&gen_u.asm_u)) ) {
    //  no room: the program stays bytecode, and may be lowered later
    //
    _ne_shut();
    bad_t = 1;
  }
  else {
    nj_side* sid_u = _nj_side_get(pog_u);

    sid_u->emi_f    = cod_y;
    sid_u->nat_f[0] = (void*)_nc_burn_north_emit;
    sid_u->nat_f[1] = (void*)_nc_burn_south_emit;
    //  its thunk goes to the code from now on: a jump
    //
    if ( sid_u->thk_f ) {
      c3_y*   thk_y = sid_u->thk_f;
      int32_t dis_i = cod_y - (thk_y + 5);

      thk_y = _ne_w(thk_y);
      thk_y[0] = 0xe9;
      memcpy(thk_y + 1, &dis_i, 4);
    }
    _ne_shut();

    clock_gettime(CLOCK_MONOTONIC, &end_u);
    u3nc_Stat.low_d++;
    u3nc_Stat.lob_d += pog_u->byc_u.len_h;
    u3nc_Stat.lon_d += gen_u.asm_u.len_w;
    u3nc_Stat.lot_d += ((end_u.tv_sec - beg_u.tv_sec) * 1000000000LL)
                     + (end_u.tv_nsec - beg_u.tv_nsec);
  }

  _nx_done(&gen_u.asm_u);
  c3_free(gen_u.ops_u);
  c3_free(gen_u.map_w);
  c3_free(gen_u.hom_ws);
  c3_free(gen_u.stu_u);
  c3_free(gen_u.sit_w);
  c3_free(gen_u.ins_w);
  c3_free(gen_u.tai_y);

  return !bad_t;
}

#undef A

/* u3nc_emit_usage(): of the arena of machine functions: bytes in use,
**                    bytes between its start and its top, the most that
**                    has been, evictions; and the records of native code
**                    in use.
*/
void
u3nc_emit_usage(c3_d* use_d, c3_d* top_d, c3_d* pek_d, c3_d* evi_d,
                c3_d* sid_d)
{
  *use_d = _ne_use_z;
  *top_d = _ne_top_z - _ne_bot_z;
  *pek_d = _ne_pek_z - _ne_bot_z;
  *evi_d = _ne_evi_d;
  *sid_d = _nj_sid_use_d;
}

/* u3nc_emit_stray(): the records of native code that name no program of
**                    the home road: none, when it is the only road.
*/
c3_d
u3nc_emit_stray(void)
{
  c3_d num_d = 0;
  c3_h i_h;

  for ( i_h = 1; i_h < _nj_sid_top_h; i_h++ ) {
    const nj_side* sid_u = &_nj_sid_u[i_h];

    if ( sid_u->pog_u && !_nc_here(sid_u->pog_u, &(u3H->rod_u)) ) {
      num_d++;
    }
  }
  return num_d;
}

/* _ne_fault(): whether a fault at an address is the stack of the code
**              running into its guard.
*/
static c3_o
_ne_fault(void* adr_v)
{
  c3_y* adr_y = adr_v;

  return __( _ne_stk_y && (adr_y >= _ne_stk_y)
                       && (adr_y < (_ne_stk_y + _ne_gua_z)) );
}

/* u3nc_emit_count(): from now on the code counts the ops it runs, by
**                    family, or doesn't.  The code made so far is
**                    dropped, so no machine function may be running.
** u3nc_emit_ran(): how often the ops of a family, by its name, have run
**                  as machine code since.
*/
void
u3nc_emit_count(c3_t on_t)
{
  if ( _ne_init() ) {
    _ne_cnt_t = on_t;
    _ne_evict();
    memset(_ne_dat_u->cnt_d, 0, sizeof(_ne_dat_u->cnt_d));
  }
}

c3_d
u3nc_emit_ran(const c3_c* nam_c)
{
  c3_w i_w;

  if ( _ne_exe_y ) {
    for ( i_w = 0; i_w <= _nc_bom; i_w++ ) {
      if ( !strcmp(_nc_fam_c[i_w], nam_c) ) {
        return _ne_dat_u->cnt_d[i_w];
      }
    }
  }
  return 0;
}

/* u3nc_emit_guard(): an address in the guard of the stack of the code,
**                    and how often the pages of the stack were given
**                    back: for tests.
*/
void*
u3nc_emit_guard(c3_d* tri_d)
{
  *tri_d = _ne_tri_d;
  return ( _ne_init() && _ne_stk_y ) ? (_ne_stk_y + (_ne_gua_z >> 1)) : 0;
}

/* u3nc_emit_limit(): the arena may hold lim_d bytes of code, or, with 0,
**                    what it was mapped for: to test a full arena.
*/
void
u3nc_emit_limit(c3_d lim_d)
{
  if ( _ne_init() ) {
    _ne_are_z = lim_d ? c3_min(_ne_map_z, _ne_bot_z + lim_d) : _ne_map_z;
  }
}

/* u3nc_emit_break(): as if a signal had abandoned a change of the arena
**                    half made: for tests.
*/
void
u3nc_emit_break(void)
{
  if ( _ne_init() ) {
    _ne_att_y |= _ne_att_bus;
  }
}
