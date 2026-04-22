/* one-shot probe: which kernel names does u3v_wish resolve? */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ur/ur.h"
#include "vere.h"
#include "ivory.h"

static const char* _names[] = {
  /* Map/set candidates */
  "get:by","put:by","has:by","del:by","apt:by","gas:by","uni:by",
  "dif:by","int:by","all:by","any:by",
  "put:in","has:in","del:in","apt:in","gas:in","uni:in","tap:in",
  "wyt:in","int:in","dif:in",
  /* Float candidates */
  "add:rd","sub:rd","mul:rd","div:rd","lth:rd","equ:rd","sun:rd",
  "add:rs","sub:rs","mul:rs","div:rs","lth:rs","equ:rs",
  "add:rh","sub:rh","mul:rh","div:rh",
  "add:rq","sub:rq","mul:rq","div:rq",
  /* Parser combinators */
  "bend","cold","cook","easy","fail","glue","here","just","mask",
  "pfix","plug","pose","sfix","stag","shim","stew","stir",
  /* Nock evaluator (already fuzzed as fuzz_nock_mink C-only) */
  "mink","mole","mule","mice",
  /* Text / parse */
  "trip","tape","leer","lune","loss",
  /* ed25519 */
  "puck:ed","veri:ed","sign:ed","shar:ed",
  /* hmac, keccak */
  "hmac","keccak-256","keccak-512","keccak-224","keccak-384",
  0
};

static const char* g_cur_name = 0;

static u3_noun
_wish_soft(u3_noun ignored)
{
  (void)ignored;
  return u3v_wish(g_cur_name);
}

int main(void) {
  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);
  u3_cue_xeno* sil = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  u3_weak pil = u3s_cue_xeno_with(sil, u3_Ivory_pill_len, u3_Ivory_pill);
  u3s_cue_xeno_done(sil);
  u3v_boot_lite(pil);

  for (int i = 0; _names[i]; i++) {
    g_cur_name = _names[i];
    u3_noun g = u3m_soft(0, _wish_soft, u3_nul);
    u3_noun how = u3h(g);
    if (0 != how) {
      fprintf(stderr, "  FAIL  '%s' (bail %x)\n", _names[i], u3r_word(0, how));
    } else if (u3_none == u3t(g)) {
      fprintf(stderr, "  none  '%s'\n", _names[i]);
    } else {
      fprintf(stderr, "  ok    '%s'\n", _names[i]);
    }
    u3z(g);
  }
  return 0;
}
