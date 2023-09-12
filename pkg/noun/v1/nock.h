/// @file

#ifndef U3_NOCK_V1_H
#define U3_NOCK_V1_H

  /**  Functions.
  **/
    /* u3n_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
    */
      void
      u3n_v1_reclaim(void);

    /* u3n_v1_free(): free bytecode cache.
     */
      void
      u3n_v1_free(void);

#endif /* ifndef U3_NOCK_V1_H */