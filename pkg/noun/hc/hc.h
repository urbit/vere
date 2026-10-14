/// @file
///
/// hc, the hoon compiler in hoon.c, as vere holds it: hoon.c is built
/// into hc.c, with everything in it kept to that file, and this is what
/// the rest of vere sees. jets/f/hc.c is the host, which brings nouns
/// across and runs what the compiler asks of the kernel.

#ifndef U3_HC_H
#define U3_HC_H

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

    /* hc_noun: a noun as the compiler has it: one of ours, in the loom,
    ** with every bit turned, so that 0 is u3_none.
    */
      typedef uint32_t hc_noun;

#     define hc_of(som)  ((hc_noun)~(som))
#     define hc_to(som)  ((u3_noun)~(som))

    /* hc_boot(): whether the compiler is there.
    */
      int
      hc_boot(void);

    /* hc_wipe(): forget everything a call kept: to be called before the
    ** first, and after each. The memory it kept it in was from
    ** u3a_malloc(), on the road the call ran on.
    */
      void
      hc_wipe(void);

    /* hc_cells(): the cells in the heap.
    */
      uint32_t
      hc_cells(void);

    /* hc_bytes(): the bytes of arena chunks the call has had.
    */
      size_t
      hc_bytes(void);

    /* hc_fail: where the compiler goes when it can't go on, for want
    ** of memory: from any of what's below, having returned nothing.
    ** The heap must then be wiped before it's used again. To be set,
    ** with setjmp(), by whoever calls them, and hc_arm() told while
    ** it's good to go to; if it isn't, the process is ended.
    */
      extern jmp_buf hc_fail;

      void
      hc_arm(int arm);

    /* hc_same(): whether two nouns are equal.
    */
      int
      hc_same(hc_noun one, hc_noun two);

    /* hc_cons(): the cell of two nouns.
    */
      hc_noun
      hc_cons(hc_noun hed, hc_noun tel);

    /* hc_mint(): [type nock] of [gen] against [sut] with [gol] wanted,
    ** as +mint in +ut with [vet], [fan] and [rib], using at most [stk]
    ** bytes of stack. 0 where hoon would crash, or might not.
    */
      hc_noun
      hc_mint(hc_noun sut,
              hc_noun gol,
              hc_noun gen,
              int     vet,
              hc_noun fan,
              hc_noun rib,
              size_t  stk);

    /* hc_text(): [len] bytes for hc_vest() or hc_pile() to parse, to be
    ** filled: on the road the parse is to run on.
    */
      uint8_t*
      hc_text(size_t len);

    /* hc_vest(): the hoon of the text hc_text() gave, as +vest parses
    ** it from [1 1]. 0 if it doesn't parse, or might not.
    */
      hc_noun
      hc_vest(uint8_t* txt, size_t len);

    /* hc_pile(): the pile of the text hc_text() gave, a file at the path
    ** [pax] in a desk, as +pile-rule in clay parses it from [1 1]. 0 if
    ** it doesn't parse, or might not.
    */
      hc_noun
      hc_pile(uint8_t* txt, size_t len, hc_noun pax);

    /* The host's: called by the compiler while it compiles.
    **
    ** hc_host_laze(): the gate +laze makes of [fom],
    ** [sut nym hud dom vet fan rib].
    **
    ** hc_host_from(): what the gate [gat], found in a type, was made
    ** from, as above, if the kernel that's compiling made it; 0 if not,
    ** and the compile is then to fail.
    **
    ** hc_host_mack(): (mack cor [9 axe 0 1]), 0 for ~.
    **
    ** hc_host_over(): whether the compile has run too long, and is to
    ** fail: asked about once a millisecond.
    */
      hc_noun
      hc_host_laze(hc_noun fom);

      hc_noun
      hc_host_from(hc_noun gat);

      hc_noun
      hc_host_mack(hc_noun cor, hc_noun axe);

      int
      hc_host_over(void);

#endif /* ifndef U3_HC_H */
