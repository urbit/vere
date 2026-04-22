/// One-shot test: boot the v3.5 brass pill and probe wish names that
/// weren't resolvable via the ivory pill.
/// Builds as a vere-harness but isn't intended for afl-fuzz; just
/// `./fuzz/out/pill_test.afl` and read stderr.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ur/ur.h"
#include "vere.h"

/* no __AFL_FUZZ_INIT — this is a one-shot CLI tool */

static const char* PILL_PATH = "/home/amadeo/git/vere/fuzz/pills/urbit-v3.5.pill";

int main(void) {
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1UL << 32);   /* 2 GiB — brass pill boot is hungry */

  fprintf(stderr, "pill: reading %s\n", PILL_PATH);
  time_t t0 = time(NULL);

  FILE* fp = fopen(PILL_PATH, "rb");
  if (!fp) { perror("fopen"); return 1; }
  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  uint8_t* buf = malloc(len);
  fread(buf, 1, len, fp);
  fclose(fp);

  fprintf(stderr, "pill: %ld bytes read, cueing...\n", len);
  u3_cue_xeno* sil = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  u3_weak pil = u3s_cue_xeno_with(sil, (c3_d)len, buf);
  u3s_cue_xeno_done(sil);
  free(buf);
  if (pil == u3_none) { fprintf(stderr, "cue failed\n"); return 1; }

  fprintf(stderr, "pill: first cue (%lds). Extracting inner bytes...\n",
          time(NULL) - t0);

  /* First cue yielded an ATOM — a jam of the real pill. Extract its
   * raw bytes and re-cue via u3s_cue_xeno for speed (off-loom, no
   * hashcons). u3ke_cue would hashcons 100M+ nouns and run for hours. */
  c3_w inner_len = u3r_met(3, pil);
  fprintf(stderr, "pill: inner atom is %u bytes\n", inner_len);
  uint8_t* inner_buf = malloc(inner_len);
  u3r_bytes(0, inner_len, inner_buf, pil);
  u3z(pil);

  fprintf(stderr, "pill: second cue (xeno) on %u bytes...\n", inner_len);
  time_t t2 = time(NULL);
  sil = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  pil = u3s_cue_xeno_with(sil, (c3_d)inner_len, inner_buf);
  u3s_cue_xeno_done(sil);
  free(inner_buf);
  if (pil == u3_none) { fprintf(stderr, "second cue failed\n"); return 1; }
  fprintf(stderr, "pill: second cue done (%lds).\n", time(NULL) - t2);

  u3_noun mot, tag, dat;
  if (c3n == u3r_trel(pil, &mot, &tag, &dat) || u3_blip != mot) {
    fprintf(stderr, "unexpected pill shape after double-cue\n");
    return 1;
  }
  if (c3__pill != tag) {
    fprintf(stderr, "pill tag is not %%pill\n");
    return 1;
  }

  u3_noun typ, bot, mod, use;
  if (c3n == u3r_qual(dat, &typ, &bot, &mod, &use)) {
    fprintf(stderr, "failed to extract bot/mod/use from pill body\n");
    return 1;
  }
  if (c3y == u3a_is_atom(typ)) {
    char* typc = u3r_string(typ);
    fprintf(stderr, "pill type: %%%s\n", typc);
    free(typc);
  }

  /* Build the boot event list: weld(bot, weld(mod, use)). */
  fprintf(stderr, "pill: building boot event list...\n");
  u3_noun eve = u3kb_weld(u3k(bot),
                 u3kb_weld(u3k(mod), u3k(use)));

  u3_noun lent = u3qb_lent(eve);
  c3_d eve_d;
  u3r_safe_chub(lent, &eve_d);
  u3z(lent);
  fprintf(stderr, "pill: %llu boot events, invoking u3v_boot...\n",
          (unsigned long long)eve_d);
  time_t t1 = time(NULL);

  if (c3n == u3v_boot(eve)) {
    fprintf(stderr, "u3v_boot failed\n");
    return 1;
  }
  fprintf(stderr, "pill: boot complete (%lds).\n", time(NULL) - t1);

  u3z(pil);

  /* Now test a batch of wish names previously unreachable via ivory. */
  const char* names[] = {
    "ripe", "crc32", "adler32", "sha1",
    "puck:ed", "veri:ed", "sign:ed", "shar:ed",
    "hmac", "keccak-256", "keccak-512", "keccak-224", "keccak-384",
    "en-base16", "de-base16",
    "lth:rq", "equ:rq",
    "argon2", "scrypt", "blake2b",
    0
  };
  fprintf(stderr, "--- wish probes ---\n");
  for (int i = 0; names[i]; i++) {
    u3_noun g = u3v_wish(names[i]);
    fprintf(stderr, "%s %s\n", (u3_none == g) ? "FAIL" : "OK  ", names[i]);
    if (u3_none != g) u3z(g);
  }

  fprintf(stderr, "done\n");
  return 0;
}
