/// @file
///
/// An x86-64 encoder for the machine code of u3nc programs
/// (nock-compile-emit.c): the instruction forms the lowering uses and no
/// others, written into a buffer, with labels.  Included by
/// nock-compile-emit.c.  Not a standalone header.
///
/// The lowering names machine registers only through the roles below
/// (nx_t0.., nx_s0.., nx_a0.., nx_bas, nx_ctx, nx_sp) and emits code only
/// through the _nx_ functions, whose operands are registers, a base with a
/// displacement, a base with an index of four bytes, immediates and
/// labels.  An encoder for another machine gives the same roles and the
/// same functions: where the machine has no form for one (an operation on
/// a word in memory, a compare with a wide immediate), its function is two
/// or three instructions through the register it keeps for that.

/* the registers, by their numbers in the encoding
*/
enum {
  nx_rax, nx_rcx, nx_rdx, nx_rbx, nx_rsp, nx_rbp, nx_rsi, nx_rdi,
  nx_r8,  nx_r9,  nx_r10, nx_r11, nx_r12, nx_r13, nx_r14, nx_r15
};

/* their roles:
**
**   nx_t0..nx_t5: scratch, of an op; nx_t5 is lost in a call of a helper;
**   nx_a0..nx_a3: the arguments of a helper (the C convention), which are
**                 among the scratch;
**   nx_s0..nx_s6: the registers slots live in, the arguments and the
**                 parts of the product of a nock function among them
**                 (_nx_sot_y): a helper keeps them, a nock function does
**                 not;
**   nx_bas:       the base of the loom;
**   nx_ctx:       the context of the run (ne_ctx);
**   nx_sp:        the stack.
*/
enum {
  nx_t0 = nx_rax, nx_t1 = nx_rcx, nx_t2 = nx_rdx, nx_t3 = nx_rsi,
  nx_t4 = nx_rdi, nx_t5 = nx_r11,
  nx_a0 = nx_rdi, nx_a1 = nx_rsi, nx_a2 = nx_rdx, nx_a3 = nx_rcx,
  nx_bas = nx_r14, nx_ctx = nx_r15, nx_sp = nx_rsp
};

#define _nx_sot_w  7

/* the word of the loom: a noun, a count, an offset.  The instructions
** below that have no width in their names work on words.
**
**   _nx_W:   the word is 64 bits;
**   _nx_ws:  the scale of an address by an index of words, as the SIB
**            byte has it;
**   _nx_os:  the scale of an address by the offset of a noun, which is the
**            noun shifted left by two: the reference without its two flag
**            bits, in words of the 32-bit loom and in quarter words of
**            the 64-bit one, whose references are not shifted.
*/
#ifdef VERE64
#  define _nx_W   1
#  define _nx_ws  3
#  define _nx_os  1
#else
#  define _nx_W   0
#  define _nx_ws  2
#  define _nx_os  2
#endif
static const c3_y _nx_sot_y[_nx_sot_w] = {
  nx_rbx, nx_rbp, nx_r12, nx_r13, nx_r8, nx_r9, nx_r10
};

/* conditions, as the low four bits of the jump opcodes
*/
enum {
  nx_o = 0, nx_no = 1, nx_b = 2, nx_ae = 3, nx_e = 4, nx_ne = 5, nx_be = 6,
  nx_a = 7, nx_s = 8, nx_ns = 9
};

/* arithmetic, as the digit of the opcode group
*/
enum { nx_add = 0, nx_or = 1, nx_and = 4, nx_sub = 5, nx_xor = 6, nx_cmp = 7 };

/* nx_fix: a place in the code that waits for an address.
*/
enum {
  nx_rel8,    //  a signed byte, the distance to a label
  nx_rel32,   //  four bytes, the distance to a label
  nx_ext32,   //  four bytes, the distance to an address outside the
              //  code: known when the code has its place
  nx_abs64    //  eight bytes, the address of a label
};

typedef struct {
  c3_w  pos_w;    //  where
  c3_y  kin_y;    //  nx_rel8..
  c3_w  lab_w;    //  the label
  c3_d  ext_d;    //  the address, for nx_ext32
} nx_fix;

/* nx_asm: code being written.
*/
typedef struct {
  c3_y*   buf_y;    //  the code
  c3_w    len_w;    //    its length
  c3_w    cap_w;    //    and its capacity
  c3_w*   lab_w;    //  label -> offset, or ~0 until bound
  c3_w    lan_w;
  c3_w    lac_w;
  nx_fix* fix_u;    //  the places that wait
  c3_w    fin_w;
  c3_w    fic_w;
} nx_asm;

#define _nx_grow(ptr, len, cap, typ)  do {                                      \
    if ( (len) >= (cap) ) {                                                     \
      (cap) = (cap) ? (2 * (cap)) : 64;                                         \
      (ptr) = c3_realloc((ptr), (cap) * sizeof(typ));                           \
    }                                                                           \
  } while ( 0 )

static inline void
_nx_b(nx_asm* asm_u, c3_y byt_y)
{
  if ( asm_u->len_w >= asm_u->cap_w ) {
    asm_u->cap_w = asm_u->cap_w ? (2 * asm_u->cap_w) : 1024;
    asm_u->buf_y = c3_realloc(asm_u->buf_y, asm_u->cap_w);
  }
  asm_u->buf_y[asm_u->len_w++] = byt_y;
}

static inline void
_nx_h(nx_asm* asm_u, c3_h val_h)
{
  _nx_b(asm_u, val_h);       _nx_b(asm_u, val_h >> 8);
  _nx_b(asm_u, val_h >> 16); _nx_b(asm_u, val_h >> 24);
}

static inline void
_nx_d(nx_asm* asm_u, c3_d val_d)
{
  _nx_h(asm_u, (c3_h)val_d);
  _nx_h(asm_u, (c3_h)(val_d >> 32));
}

/* _nx_label(): a new label, not bound.  _nx_bind(): it is here.
*/
static c3_w
_nx_label(nx_asm* asm_u)
{
  _nx_grow(asm_u->lab_w, asm_u->lan_w, asm_u->lac_w, c3_w);
  asm_u->lab_w[asm_u->lan_w] = ~(c3_w)0;
  return asm_u->lan_w++;
}

static void
_nx_bind(nx_asm* asm_u, c3_w lab_w)
{
  u3_assert( ~(c3_w)0 == asm_u->lab_w[lab_w] );
  asm_u->lab_w[lab_w] = asm_u->len_w;
}

static void
_nx_wait(nx_asm* asm_u, c3_y kin_y, c3_w lab_w, c3_d ext_d)
{
  nx_fix* fix_u;

  _nx_grow(asm_u->fix_u, asm_u->fin_w, asm_u->fic_w, nx_fix);
  fix_u = &asm_u->fix_u[asm_u->fin_w++];
  fix_u->pos_w = asm_u->len_w;
  fix_u->kin_y = kin_y;
  fix_u->lab_w = lab_w;
  fix_u->ext_d = ext_d;

  if ( nx_rel8 == kin_y ) {
    _nx_b(asm_u, 0);
  }
  else if ( nx_abs64 == kin_y ) {
    _nx_d(asm_u, 0);
  }
  else {
    _nx_h(asm_u, 0);
  }
}

/* _nx_link(): the code has its place: it will run at exe_y, and is
**             written at cod_y, which may be another view of the same
**             memory: fill what waited.
*/
static void
_nx_link(nx_asm* asm_u, c3_y* cod_y, c3_y* exe_y)
{
  c3_w i_w;

  for ( i_w = 0; i_w < asm_u->fin_w; i_w++ ) {
    nx_fix* fix_u = &asm_u->fix_u[i_w];
    c3_y*   her_y = cod_y + fix_u->pos_w;

    switch ( fix_u->kin_y ) {
      case nx_rel8: {
        c3_ds dis_ds = (c3_ds)asm_u->lab_w[fix_u->lab_w]
                     - (c3_ds)(fix_u->pos_w + 1);

        u3_assert( ~(c3_w)0 != asm_u->lab_w[fix_u->lab_w] );
        u3_assert( (dis_ds >= -128) && (dis_ds <= 127) );
        *her_y = (c3_y)(int8_t)dis_ds;
      } break;

      case nx_rel32: {
        int32_t dis_i = (int32_t)asm_u->lab_w[fix_u->lab_w]
                      - (int32_t)(fix_u->pos_w + 4);

        u3_assert( ~(c3_w)0 != asm_u->lab_w[fix_u->lab_w] );
        memcpy(her_y, &dis_i, 4);
      } break;

      case nx_ext32: {
        c3_ds   dis_ds = (c3_ds)fix_u->ext_d
                       - (c3_ds)(uintptr_t)(exe_y + fix_u->pos_w + 4);
        int32_t dis_i  = (int32_t)dis_ds;

        u3_assert( dis_ds == (c3_ds)dis_i );
        memcpy(her_y, &dis_i, 4);
      } break;

      case nx_abs64: {
        c3_d adr_d = (c3_d)(uintptr_t)(exe_y + asm_u->lab_w[fix_u->lab_w]);

        u3_assert( ~(c3_w)0 != asm_u->lab_w[fix_u->lab_w] );
        memcpy(her_y, &adr_d, 8);
      } break;
    }
  }
}

static void
_nx_done(nx_asm* asm_u)
{
  c3_free(asm_u->buf_y);
  c3_free(asm_u->lab_w);
  c3_free(asm_u->fix_u);
}

/* _nx_rex(): the prefix, if the instruction needs one: wid_t, a 64-bit
**            operand; the high bits of the register field, the index and
**            the base.
*/
static inline void
_nx_rex(nx_asm* asm_u, c3_t wid_t, c3_y reg_y, c3_y idx_y, c3_y bas_y)
{
  c3_y rex_y = (wid_t ? 8 : 0) | ((reg_y & 8) ? 4 : 0)
             | ((idx_y & 8) ? 2 : 0) | ((bas_y & 8) ? 1 : 0);

  if ( rex_y ) {
    _nx_b(asm_u, 0x40 | rex_y);
  }
}

/* _nx_rr(): an instruction on two registers: the opcode, the register in
**           the register field, the one in the r/m field.
*/
static void
_nx_rr(nx_asm* asm_u, c3_t wid_t, c3_y opc_y, c3_y reg_y, c3_y rm_y)
{
  _nx_rex(asm_u, wid_t, reg_y, 0, rm_y);
  _nx_b(asm_u, opc_y);
  _nx_b(asm_u, 0xc0 | ((reg_y & 7) << 3) | (rm_y & 7));
}

/* _nx_rm(): an instruction on a register (or the digit of its group) and
**           the word at [bas + dis].
*/
static void
_nx_rm(nx_asm* asm_u, c3_t wid_t, c3_y opc_y, c3_y reg_y, c3_y bas_y,
       int32_t dis_i)
{
  c3_y mod_y = ( (0 == dis_i) && (5 != (bas_y & 7)) ) ? 0
             : ( (dis_i >= -128) && (dis_i <= 127) ) ? 1 : 2;

  _nx_rex(asm_u, wid_t, reg_y, 0, bas_y);
  _nx_b(asm_u, opc_y);
  _nx_b(asm_u, (mod_y << 6) | ((reg_y & 7) << 3) | (bas_y & 7));
  if ( 4 == (bas_y & 7) ) {
    _nx_b(asm_u, 0x24);
  }
  if ( 1 == mod_y ) {
    _nx_b(asm_u, (c3_y)(int8_t)dis_i);
  }
  else if ( 2 == mod_y ) {
    _nx_h(asm_u, (c3_h)dis_i);
  }
}

/* _nx_rs(): the same, the word at [bas + (idx << sca) + dis].
** _nx_rx(): by the offset of a noun.
*/
static void
_nx_rs(nx_asm* asm_u, c3_t wid_t, c3_y sca_y, c3_y opc_y, c3_y reg_y,
       c3_y bas_y, c3_y idx_y, int32_t dis_i)
{
  c3_y mod_y = ( (0 == dis_i) && (5 != (bas_y & 7)) ) ? 0
             : ( (dis_i >= -128) && (dis_i <= 127) ) ? 1 : 2;

  u3_assert( nx_rsp != idx_y );
  _nx_rex(asm_u, wid_t, reg_y, idx_y, bas_y);
  _nx_b(asm_u, opc_y);
  _nx_b(asm_u, (mod_y << 6) | ((reg_y & 7) << 3) | 4);
  _nx_b(asm_u, (sca_y << 6) | ((idx_y & 7) << 3) | (bas_y & 7));
  if ( 1 == mod_y ) {
    _nx_b(asm_u, (c3_y)(int8_t)dis_i);
  }
  else if ( 2 == mod_y ) {
    _nx_h(asm_u, (c3_h)dis_i);
  }
}

static void
_nx_rx(nx_asm* asm_u, c3_t wid_t, c3_y opc_y, c3_y reg_y, c3_y bas_y,
       c3_y idx_y, int32_t dis_i)
{
  _nx_rs(asm_u, wid_t, _nx_os, opc_y, reg_y, bas_y, idx_y, dis_i);
}

/* _nx_rl(): the same, the word at a label of the code, or at an address
**           outside it (lab_w ~0), by its distance from here.
*/
static void
_nx_rl(nx_asm* asm_u, c3_t wid_t, c3_y opc_y, c3_y reg_y, c3_w lab_w,
       c3_d ext_d)
{
  _nx_rex(asm_u, wid_t, reg_y, 0, 0);
  _nx_b(asm_u, opc_y);
  _nx_b(asm_u, ((reg_y & 7) << 3) | 5);
  _nx_wait(asm_u, (~(c3_w)0 == lab_w) ? nx_ext32 : nx_rel32, lab_w, ext_d);
}

/*  moves
*/
static void
_nx_mov_rr(nx_asm* asm_u, c3_y dst_y, c3_y src_y)
{
  if ( dst_y != src_y ) {
    _nx_rr(asm_u, _nx_W, 0x89, src_y, dst_y);
  }
}

static void
_nx_mov_rr64(nx_asm* asm_u, c3_y dst_y, c3_y src_y)
{
  _nx_rr(asm_u, 1, 0x89, src_y, dst_y);
}

static void
_nx_mov_ri64(nx_asm* asm_u, c3_y dst_y, c3_d imm_d)
{
  _nx_rex(asm_u, 1, 0, 0, dst_y);
  _nx_b(asm_u, 0xb8 | (dst_y & 7));
  _nx_d(asm_u, imm_d);
}

//  a word: the shortest instruction that makes it.  Four bytes written to
//  a register clear its other four
//
static void
_nx_mov_ri(nx_asm* asm_u, c3_y dst_y, c3_w imm_w)
{
  if ( !imm_w ) {
    _nx_rr(asm_u, 0, 0x31, dst_y, dst_y);
  }
  else if ( (c3_d)imm_w <= 0xffffffffULL ) {
    _nx_rex(asm_u, 0, 0, 0, dst_y);
    _nx_b(asm_u, 0xb8 | (dst_y & 7));
    _nx_h(asm_u, (c3_h)imm_w);
  }
  else if ( (int64_t)(c3_d)imm_w == (int64_t)(int32_t)(c3_h)imm_w ) {
    _nx_rr(asm_u, 1, 0xc7, 0, dst_y);
    _nx_h(asm_u, (c3_h)imm_w);
  }
  else {
    _nx_mov_ri64(asm_u, dst_y, (c3_d)imm_w);
  }
}

/* _nx_fits(): whether a word can be the immediate of an instruction on
**             words: four bytes, which the instruction extends by their
**             sign.
*/
static inline c3_t
_nx_fits(c3_w imm_w)
{
#ifdef VERE64
  return (int64_t)imm_w == (int64_t)(int32_t)(c3_h)imm_w;
#else
  return 1;
#endif
}

//  _nx_ld(): dst <- [bas + dis]; _nx_st(): [bas + dis] <- src;
//  _nx_st_i(): [bas + dis] <- imm; _nx_ldx(), _nx_stx(): with an index;
//  the 64 ones: eight bytes
//
static void
_nx_ld(nx_asm* asm_u, c3_y dst_y, c3_y bas_y, int32_t dis_i)
{
  _nx_rm(asm_u, _nx_W, 0x8b, dst_y, bas_y, dis_i);
}

static void
_nx_st(nx_asm* asm_u, c3_y bas_y, int32_t dis_i, c3_y src_y)
{
  _nx_rm(asm_u, _nx_W, 0x89, src_y, bas_y, dis_i);
}

//  the immediate of a store is four bytes, extended by their sign
//
static void
_nx_st_i(nx_asm* asm_u, c3_y bas_y, int32_t dis_i, c3_w imm_w)
{
  u3_assert( _nx_fits(imm_w) );
  _nx_rm(asm_u, _nx_W, 0xc7, 0, bas_y, dis_i);
  _nx_h(asm_u, (c3_h)imm_w);
}

//  the four-byte ones: a field that is not a word
//
static void
_nx_ld32(nx_asm* asm_u, c3_y dst_y, c3_y bas_y, int32_t dis_i)
{
  _nx_rm(asm_u, 0, 0x8b, dst_y, bas_y, dis_i);
}

static void
_nx_st32(nx_asm* asm_u, c3_y bas_y, int32_t dis_i, c3_y src_y)
{
  _nx_rm(asm_u, 0, 0x89, src_y, bas_y, dis_i);
}

static void
_nx_ld64(nx_asm* asm_u, c3_y dst_y, c3_y bas_y, int32_t dis_i)
{
  _nx_rm(asm_u, 1, 0x8b, dst_y, bas_y, dis_i);
}

static void
_nx_st64(nx_asm* asm_u, c3_y bas_y, int32_t dis_i, c3_y src_y)
{
  _nx_rm(asm_u, 1, 0x89, src_y, bas_y, dis_i);
}

static void
_nx_ldx(nx_asm* asm_u, c3_y dst_y, c3_y bas_y, c3_y idx_y, int32_t dis_i)
{
  _nx_rx(asm_u, _nx_W, 0x8b, dst_y, bas_y, idx_y, dis_i);
}

static void
_nx_stx(nx_asm* asm_u, c3_y bas_y, c3_y idx_y, int32_t dis_i, c3_y src_y)
{
  _nx_rx(asm_u, _nx_W, 0x89, src_y, bas_y, idx_y, dis_i);
}

static void
_nx_stx_i(nx_asm* asm_u, c3_y bas_y, c3_y idx_y, int32_t dis_i, c3_w imm_w)
{
  u3_assert( _nx_fits(imm_w) );
  _nx_rx(asm_u, _nx_W, 0xc7, 0, bas_y, idx_y, dis_i);
  _nx_h(asm_u, (c3_h)imm_w);
}

//  the word at a label, or outside the code
//
static void
_nx_ld_l(nx_asm* asm_u, c3_y dst_y, c3_w lab_w, c3_d ext_d)
{
  _nx_rl(asm_u, _nx_W, 0x8b, dst_y, lab_w, ext_d);
}

static void
_nx_st_l(nx_asm* asm_u, c3_w lab_w, c3_d ext_d, c3_y src_y)
{
  _nx_rl(asm_u, _nx_W, 0x89, src_y, lab_w, ext_d);
}

static void
_nx_ld64_l(nx_asm* asm_u, c3_y dst_y, c3_w lab_w, c3_d ext_d)
{
  _nx_rl(asm_u, 1, 0x8b, dst_y, lab_w, ext_d);
}

//  _nx_lea(): dst <- bas + dis; _nx_lea2(): dst <- bas + idx;
//  _nx_lea64(): the address [bas + dis]
//
static void
_nx_lea(nx_asm* asm_u, c3_y dst_y, c3_y bas_y, int32_t dis_i)
{
  _nx_rm(asm_u, _nx_W, 0x8d, dst_y, bas_y, dis_i);
}

static void
_nx_lea2(nx_asm* asm_u, c3_y dst_y, c3_y bas_y, c3_y idx_y)
{
  c3_y mod_y = (5 == (bas_y & 7)) ? 1 : 0;

  _nx_rex(asm_u, _nx_W, dst_y, idx_y, bas_y);
  _nx_b(asm_u, 0x8d);
  _nx_b(asm_u, (mod_y << 6) | ((dst_y & 7) << 3) | 4);
  _nx_b(asm_u, ((idx_y & 7) << 3) | (bas_y & 7));
  if ( mod_y ) {
    _nx_b(asm_u, 0);
  }
}

static void
_nx_lea64(nx_asm* asm_u, c3_y dst_y, c3_y bas_y, int32_t dis_i)
{
  _nx_rm(asm_u, 1, 0x8d, dst_y, bas_y, dis_i);
}

//  by an index of words: the address [bas + word * idx], and the word
//  there
//
static void
_nx_lea64w(nx_asm* asm_u, c3_y dst_y, c3_y bas_y, c3_y idx_y)
{
  _nx_rs(asm_u, 1, _nx_ws, 0x8d, dst_y, bas_y, idx_y, 0);
}

static void
_nx_ldw(nx_asm* asm_u, c3_y dst_y, c3_y bas_y, c3_y idx_y, int32_t dis_i)
{
  _nx_rs(asm_u, _nx_W, _nx_ws, 0x8b, dst_y, bas_y, idx_y, dis_i);
}

static void
_nx_lea64_l(nx_asm* asm_u, c3_y dst_y, c3_w lab_w)
{
  _nx_rl(asm_u, 1, 0x8d, dst_y, lab_w, 0);
}

/*  arithmetic and tests: op is nx_add..nx_cmp
*/
static void
_nx_alu_rr(nx_asm* asm_u, c3_y op_y, c3_y dst_y, c3_y src_y)
{
  _nx_rr(asm_u, _nx_W, (op_y << 3) | 1, src_y, dst_y);
}

static void
_nx_alu_ri(nx_asm* asm_u, c3_y op_y, c3_y dst_y, c3_w imm_w)
{
  int32_t imm_i = (int32_t)(c3_h)imm_w;

  u3_assert( _nx_fits(imm_w) );
  if ( (imm_i >= -128) && (imm_i <= 127) ) {
    _nx_rr(asm_u, _nx_W, 0x83, op_y, dst_y);
    _nx_b(asm_u, (c3_y)imm_i);
  }
  else {
    _nx_rr(asm_u, _nx_W, 0x81, op_y, dst_y);
    _nx_h(asm_u, (c3_h)imm_i);
  }
}

//  dst op= [bas + dis]
//
static void
_nx_alu_rm(nx_asm* asm_u, c3_y op_y, c3_y dst_y, c3_y bas_y, int32_t dis_i)
{
  _nx_rm(asm_u, _nx_W, (op_y << 3) | 3, dst_y, bas_y, dis_i);
}

//  [bas + dis] op= src
//
static void
_nx_alu_mr(nx_asm* asm_u, c3_y op_y, c3_y bas_y, int32_t dis_i, c3_y src_y)
{
  _nx_rm(asm_u, _nx_W, (op_y << 3) | 1, src_y, bas_y, dis_i);
}

//  [bas + dis] op= imm
//
static void
_nx_alu_mi(nx_asm* asm_u, c3_y op_y, c3_y bas_y, int32_t dis_i, c3_w imm_w)
{
  int32_t imm_i = (int32_t)(c3_h)imm_w;

  u3_assert( _nx_fits(imm_w) );
  if ( (imm_i >= -128) && (imm_i <= 127) ) {
    _nx_rm(asm_u, _nx_W, 0x83, op_y, bas_y, dis_i);
    _nx_b(asm_u, (c3_y)imm_i);
  }
  else {
    _nx_rm(asm_u, _nx_W, 0x81, op_y, bas_y, dis_i);
    _nx_h(asm_u, (c3_h)imm_i);
  }
}

//  whether a bit of the byte at [bas + dis] is set: test, a byte
//
static void
_nx_test_mi8(nx_asm* asm_u, c3_y bas_y, int32_t dis_i, c3_y imm_y)
{
  _nx_rm(asm_u, 0, 0xf6, 0, bas_y, dis_i);
  _nx_b(asm_u, imm_y);
}

//  [bas + idx (the offset of a noun) + dis] op= imm
//
static void
_nx_alu_xi(nx_asm* asm_u, c3_y op_y, c3_y bas_y, c3_y idx_y, int32_t dis_i,
           c3_w imm_w)
{
  int32_t imm_i = (int32_t)(c3_h)imm_w;

  u3_assert( _nx_fits(imm_w) );
  if ( (imm_i >= -128) && (imm_i <= 127) ) {
    _nx_rx(asm_u, _nx_W, 0x83, op_y, bas_y, idx_y, dis_i);
    _nx_b(asm_u, (c3_y)imm_i);
  }
  else {
    _nx_rx(asm_u, _nx_W, 0x81, op_y, bas_y, idx_y, dis_i);
    _nx_h(asm_u, (c3_h)imm_i);
  }
}

//  the 64-bit ones the trampolines and the frames use
//
static void
_nx_alu_ri64(nx_asm* asm_u, c3_y op_y, c3_y dst_y, int32_t imm_i)
{
  if ( (imm_i >= -128) && (imm_i <= 127) ) {
    _nx_rr(asm_u, 1, 0x83, op_y, dst_y);
    _nx_b(asm_u, (c3_y)imm_i);
  }
  else {
    _nx_rr(asm_u, 1, 0x81, op_y, dst_y);
    _nx_h(asm_u, (c3_h)imm_i);
  }
}

static void
_nx_alu_rm64(nx_asm* asm_u, c3_y op_y, c3_y dst_y, c3_y bas_y, int32_t dis_i)
{
  _nx_rm(asm_u, 1, (op_y << 3) | 3, dst_y, bas_y, dis_i);
}

static void
_nx_test_rr(nx_asm* asm_u, c3_y one_y, c3_y two_y)
{
  _nx_rr(asm_u, _nx_W, 0x85, two_y, one_y);
}

static void
_nx_test_rr64(nx_asm* asm_u, c3_y one_y, c3_y two_y)
{
  _nx_rr(asm_u, 1, 0x85, two_y, one_y);
}

static void
_nx_shl_ri(nx_asm* asm_u, c3_y dst_y, c3_y imm_y)
{
  _nx_rr(asm_u, _nx_W, 0xc1, 4, dst_y);
  _nx_b(asm_u, imm_y);
}

static void
_nx_shr_ri(nx_asm* asm_u, c3_y dst_y, c3_y imm_y)
{
  _nx_rr(asm_u, _nx_W, 0xc1, 5, dst_y);
  _nx_b(asm_u, imm_y);
}

static void
_nx_shr_ri64(nx_asm* asm_u, c3_y dst_y, c3_y imm_y)
{
  _nx_rr(asm_u, 1, 0xc1, 5, dst_y);
  _nx_b(asm_u, imm_y);
}

static void
_nx_dec_r(nx_asm* asm_u, c3_y dst_y)
{
  _nx_rr(asm_u, _nx_W, 0xff, 1, dst_y);
}

//  the word at [bas + 4 * idx + dis], one more or one less
//
static void
_nx_incx(nx_asm* asm_u, c3_y bas_y, c3_y idx_y, int32_t dis_i)
{
  _nx_rx(asm_u, _nx_W, 0xff, 0, bas_y, idx_y, dis_i);
}

static void
_nx_decx(nx_asm* asm_u, c3_y bas_y, c3_y idx_y, int32_t dis_i)
{
  _nx_rx(asm_u, _nx_W, 0xff, 1, bas_y, idx_y, dis_i);
}

//  a counter of eight bytes outside the code, one more
//
static void
_nx_inc64_l(nx_asm* asm_u, c3_d ext_d)
{
  _nx_rl(asm_u, 1, 0xff, 0, ~(c3_w)0, ext_d);
}

/*  jumps, calls and the stack
*/
//  _nx_jcc(), _nx_jmp(): to a label anywhere in the code; _nx_jcc8(),
//  _nx_jmp8(): to one at most 127 bytes away
//
static void
_nx_jcc(nx_asm* asm_u, c3_y con_y, c3_w lab_w)
{
  _nx_b(asm_u, 0x0f);
  _nx_b(asm_u, 0x80 | con_y);
  _nx_wait(asm_u, nx_rel32, lab_w, 0);
}

static void
_nx_jcc8(nx_asm* asm_u, c3_y con_y, c3_w lab_w)
{
  _nx_b(asm_u, 0x70 | con_y);
  _nx_wait(asm_u, nx_rel8, lab_w, 0);
}

static void
_nx_jmp(nx_asm* asm_u, c3_w lab_w)
{
  _nx_b(asm_u, 0xe9);
  _nx_wait(asm_u, nx_rel32, lab_w, 0);
}

static void
_nx_jmp8(nx_asm* asm_u, c3_w lab_w)
{
  _nx_b(asm_u, 0xeb);
  _nx_wait(asm_u, nx_rel8, lab_w, 0);
}

static void
_nx_jmp_r(nx_asm* asm_u, c3_y reg_y)
{
  _nx_rr(asm_u, 0, 0xff, 4, reg_y);
}

static void
_nx_call_r(nx_asm* asm_u, c3_y reg_y)
{
  _nx_rr(asm_u, 0, 0xff, 2, reg_y);
}

//  through the address at a label of the code, or outside it
//
static void
_nx_jmp_l(nx_asm* asm_u, c3_w lab_w, c3_d ext_d)
{
  _nx_rl(asm_u, 0, 0xff, 4, lab_w, ext_d);
}

static void
_nx_call_l(nx_asm* asm_u, c3_w lab_w, c3_d ext_d)
{
  _nx_rl(asm_u, 0, 0xff, 2, lab_w, ext_d);
}

static void
_nx_ret(nx_asm* asm_u)
{
  _nx_b(asm_u, 0xc3);
}

static void
_nx_trap(nx_asm* asm_u)
{
  _nx_b(asm_u, 0x0f);
  _nx_b(asm_u, 0x0b);
}

static void
_nx_push(nx_asm* asm_u, c3_y reg_y)
{
  _nx_rex(asm_u, 0, 0, 0, reg_y);
  _nx_b(asm_u, 0x50 | (reg_y & 7));
}

static void
_nx_pop(nx_asm* asm_u, c3_y reg_y)
{
  _nx_rex(asm_u, 0, 0, 0, reg_y);
  _nx_b(asm_u, 0x58 | (reg_y & 7));
}

//  eight bytes of data: an address, or the address of a label
//
static void
_nx_quad(nx_asm* asm_u, c3_d val_d)
{
  _nx_d(asm_u, val_d);
}

static void
_nx_quad_l(nx_asm* asm_u, c3_w lab_w)
{
  _nx_wait(asm_u, nx_abs64, lab_w, 0);
}
