/// @file blake2b_probe.c
///
/// Standalone probe for finding 018. Reads a repro file (same shape as
/// fuzz_jet_l11_base input bytes), builds the blake2b sample, then runs
/// it three ways:
///   1. C jet directly (u3we_blake2b on the synthetic core).
///   2. Hoon formula via nock, with jet suppressed (ice-flip + u3j_Fuzz_testing
///      off → force the compare path → capture `ame`).
///   3. Reference urcrypt_blake2 on the raw bytes we expect the jet to pass.
///
/// Prints each result's hex bytes and lengths so we can see exactly which
/// side disagrees and how.
///
/// Usage: ./blake2b_probe /path/to/repro.bin

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ur/ur.h"
#include "vere.h"
#include "urcrypt.h"

extern void u3m_init(size_t len_i);
extern void u3m_pave(c3_o nuu_o);
extern void u3t_init(void);
extern c3_w  u3j_boot(c3_o nuu_o);
extern void u3j_ream(void);
extern void u3n_ream(void);
extern c3_o u3e_live(c3_o nuu_o, c3_c* dir_c);
extern c3_o u3e_yolo(void);
extern c3_c* u3m_pier(c3_c* dir_c);
extern u3_noun u3we_blake2b(u3_noun cor);

#define PIER_DIR "/tmp/fuzz-pier-zod-v44"

static const char* g_wish_name = 0;
static u3_noun g_gate = 0;
static u3_noun g_sam  = 0;

static u3_noun
_wish_soft(u3_noun ignored)
{
  (void)ignored;
  return u3v_wish(g_wish_name);
}

static u3_noun
_slam_soft(u3_noun ignored)
{
  (void)ignored;
  return u3n_slam_on(u3k(g_gate), u3k(g_sam));
}

static void
_print_hex(const char* tag, u3_noun r)
{
  if ( u3_none == r ) {
    fprintf(stderr, "%-20s: (none)\n", tag);
    return;
  }
  if ( c3n == u3a_is_atom(r) ) {
    fprintf(stderr, "%-20s: (cell, not atom)\n", tag);
    return;
  }
  c3_w met_w = u3r_met(3, r);
  fprintf(stderr, "%-20s: %u bytes = ", tag, met_w);
  if ( met_w == 0 ) { fprintf(stderr, "(zero atom)\n"); return; }
  c3_y* buf = calloc(1, met_w + 16);
  u3r_bytes(0, met_w, buf, r);
  for ( c3_w i = 0; i < met_w; i++ ) fprintf(stderr, "%02x", buf[met_w-1-i]);
  fprintf(stderr, "\n");
  free(buf);
}

static void
_hexparse(const char* s, c3_y* out, c3_w* n_w)
{
  c3_w cnt = 0;
  while ( *s && *(s+1) ) {
    unsigned v;
    if ( sscanf(s, "%2x", &v) != 1 ) break;
    out[cnt++] = (c3_y)v;
    s += 2;
  }
  *n_w = cnt;
}

int
main(int argc, char** argv)
{
  c3_w   over_wid = 0, over_wik = 0, over_out = 0;
  c3_o   over_o   = c3n;
  c3_y   over_msg[1024] = {0};
  c3_y   over_key[64]   = {0};
  c3_w   over_msg_n = 0, over_key_n = 0;

  unsigned char* fbuf = 0;
  long flen = 0;

  if ( argc == 2 ) {
    FILE* fp = fopen(argv[1], "rb");
    if ( !fp ) { perror("fopen"); return 1; }
    fseek(fp, 0, SEEK_END);
    flen = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fbuf = malloc(flen);
    if ( (long)fread(fbuf, 1, flen, fp) != flen ) { perror("fread"); return 1; }
    fclose(fp);
    if ( flen < 2 ) { fprintf(stderr, "too short\n"); return 1; }
  }
  else if ( argc == 5 && !strcmp(argv[1], "--raw") ) {
    /* --raw <msg_hex> <key_hex> <out_len> — uses RFC natural byte order. */
    over_o = c3y;
    _hexparse(argv[2], over_msg, &over_msg_n);
    _hexparse(argv[3], over_key, &over_key_n);
    over_wid = over_msg_n;
    over_wik = over_key_n;
    over_out = (c3_w)atoi(argv[4]);
  }
  else {
    fprintf(stderr, "usage: %s <repro.bin>\n"
                    "       %s --raw <msg_hex_natural> <key_hex_natural> <out>\n",
            argv[0], argv[0]);
    return 1;
  }

  u3C.wag_w |= u3o_hashless;
  u3C.dir_c = (c3_c*)PIER_DIR;
  u3m_init(1UL << 31);
  c3_o nuu_o = u3e_live(c3n, u3m_pier((c3_c*)PIER_DIR));
  if ( c3y == nuu_o ) {
    fprintf(stderr, "pier %s empty\n", PIER_DIR);
    return 1;
  }
  u3e_yolo();
  u3C.slog_f = 0;
  u3C.sign_hold_f = 0;
  u3C.sign_move_f = 0;
  u3t_init();
  u3m_pave(nuu_o);
  u3j_boot(nuu_o);
  u3j_ream();
  u3n_ream();
  fprintf(stderr, "probe: booted eve %llu\n",
          (unsigned long long)u3A->eve_d);

  g_wish_name = "blake2b:blake:crypto";
  u3_noun wpro = u3m_soft(0, _wish_soft, u3_nul);
  if ( 0 != u3h(wpro) || u3_none == u3t(wpro) ) {
    fprintf(stderr, "wish failed\n");
    return 1;
  }
  u3_noun gate = u3k(u3t(wpro));
  u3z(wpro);
  fprintf(stderr, "probe: gate cached\n");

  /* Flip ice OFF on the blake arm — critical for the compare path. */
  (void)u3j_fuzz_arm("blake2b");

  u3_noun msg, key, sam;
  c3_w out_w;
  if ( c3y == over_o ) {
    /* --raw uses RFC natural-order bytes: reverse to LE for atom building. */
    c3_y rev[1024] = {0};
    for ( c3_w i = 0; i < over_msg_n; i++ ) rev[i] = over_msg[over_msg_n-1-i];
    u3_atom msg_dat = u3i_bytes(over_msg_n, rev);
    for ( c3_w i = 0; i < over_key_n; i++ ) rev[i] = over_key[over_key_n-1-i];
    u3_atom key_dat = u3i_bytes(over_key_n, rev);
    msg = u3nc(u3i_word(over_wid), msg_dat);
    key = u3nc(u3i_word(over_wik), key_dat);
    out_w = over_out;
    sam = u3nt(msg, key, u3i_word(out_w));
    fprintf(stderr, "probe: --raw sample: wid=%u wik=%u out=%u\n",
            over_wid, over_wik, out_w);
  }
  else {
    /* Replicate harness sample shape for S_BLAKE2B. */
    unsigned char* p = fbuf + 1;
    c3_w n = flen - 1;
    if ( n > 1024 ) n = 1024;
    c3_w third = n / 3;
    u3_atom msg_dat = u3i_bytes(third, p);
    msg = u3nc(u3i_word(third), msg_dat);
    u3_atom key_dat = u3i_bytes(third, p + third);
    key = u3nc(u3i_word(third), key_dat);
    out_w = (p[0] % 64) + 1;
    sam = u3nt(msg, key, u3i_word(out_w));
    fprintf(stderr, "probe: sample: wid=%u, wik=%u, out=%u\n", third, third, out_w);
  }

  fprintf(stderr, "probe: msg dat atom met=%u bytes\n", u3r_met(3, u3t(msg)));
  fprintf(stderr, "probe: key dak atom met=%u bytes\n", u3r_met(3, u3t(key)));
  _print_hex("msg dat", u3t(msg));
  _print_hex("key dak", u3t(key));

  /* 1. u3m_soft oracle path — prints 'test: ... mismatch' and bails. */
  g_gate = gate;
  g_sam  = sam;
  fprintf(stderr, "\n--- via oracle (will bail on mismatch) ---\n");
  u3_noun pro = u3m_soft(0, _slam_soft, u3_nul);
  u3_noun how = u3h(pro);
  u3_noun res = u3t(pro);
  if ( 0 != how ) {
    fprintf(stderr, "bail: %.4s\n", (char*)&how);
  } else {
    _print_hex("combined result", res);
  }
  u3z(pro);

  /* 2. Call C jet wrapper directly on a synthetic core. */
  fprintf(stderr, "\n--- direct C jet (u3we_blake2b) ---\n");
  /* core shape: [battery=0 [sample=sam context=0]] so that
   * axis 6 (head of axis 3) = sam. */
  u3_noun cor = u3nc(0, u3nc(u3k(sam), 0));
  u3_noun j = u3we_blake2b(cor);
  _print_hex("jet result", j);
  if ( u3_none != j ) u3z(j);
  u3z(cor);

  /* 3. Reference computation via urcrypt directly on raw bytes. */
  fprintf(stderr, "\n--- urcrypt_blake2 direct ---\n");
  c3_w  wid_w, wik_w;
  if ( c3y == over_o ) {
    wid_w = over_wid;
    wik_w = over_wik > 64 ? 64 : over_wik;
  } else {
    wid_w = u3r_met(3, u3t(msg));
    c3_w dak_met = u3r_met(3, u3t(key));
    wik_w = dak_met > 64 ? 64 : dak_met;
  }
  c3_y* dat_y = calloc(1, wid_w + 16);
  c3_y  dak_y[64] = {0};
  c3_w  out_b = (out_w < 1) ? 1 : ((out_w > 64) ? 64 : out_w);
  u3r_bytes(0, wid_w, dat_y, u3t(msg));
  u3r_bytes(0, wik_w, dak_y, u3t(key));
  c3_y out_y[64] = {0};
  int err = urcrypt_blake2(wid_w, dat_y, wik_w, dak_y, out_b, out_y);
  if ( err ) {
    fprintf(stderr, "urcrypt_blake2 err=%d\n", err);
  } else {
    fprintf(stderr, "%-20s: %u bytes = ", "urcrypt direct", out_b);
    for ( c3_w i = 0; i < out_b; i++ ) fprintf(stderr, "%02x", out_y[out_b-1-i]);
    fprintf(stderr, "\n");
  }
  free(dat_y);

  u3z(sam);
  u3z(gate);
  fprintf(stderr, "\nprobe: done\n");
  return 0;
}
