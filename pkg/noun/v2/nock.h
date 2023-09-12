/// @file

#ifndef U3_NOCK_V2_H
#define U3_NOCK_V2_H

  /**  Functions.
  **/
    /* u3n_v2_free(): free bytecode cache.
     */
      void
      u3n_v2_free(void);

    /* u3n_v2_rewrite_compact(): rewrite bytecode cache for compaction.
     */
      void
      u3n_v2_rewrite_compact();

#endif /* ifndef U3_NOCK_V2_H */