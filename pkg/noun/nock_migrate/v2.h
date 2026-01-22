#ifndef U3_NOCK_MIGRATE_V2_H
#define U3_NOCK_MIGRATE_V2_H

#include "allocate.h"
#include "hashtable.h"
#include "jets.h"
#include "nock.h"
#include "vortex.h"

#define u3h_nv2_new         u3h_new
#define u3h_nv2_walk        u3h_walk
#define u3h_nv2_free        u3h_free
#define u3t_nv2             u3t
#define u3z_nv2             u3z
#define u3to_nv2            u3to
#define u3j_nv2_site_lose   u3j_site_lose
#define u3j_nv2_rite_lose   u3j_rite_lose
#define u3a_nv2_free        u3a_free
#define u3j_nv2_site_ream   u3j_site_ream
#define u3j_nv2_ream        u3j_ream
#define u3j_nv2_reclaim     u3j_reclaim

#define u3_nv2_noun     u3_noun
#define u3n_nv2_memo    u3n_memo
#define u3j_nv2_site    u3j_site
#define u3j_nv2_rite    u3j_rite

typedef struct _u3n_nv2_prog {
    struct {
      c3_o      own_o;                // program owns ops_y?
      c3_w      len_w;                // length of bytecode (bytes)
      c3_y*     ops_y;                // actual array of bytes
    } byc_u;                          // bytecode
    struct {
      c3_w      len_w;                // number of literals
      u3_nv2_noun*  non;                  // array of literals
    } lit_u;                          // literals
    struct {
      c3_w      len_w;                // number of memo slots
      u3n_nv2_memo* sot_u;                // array of memo slots
    } mem_u;                          // memo slot data
    struct {
      c3_w      len_w;                // number of calls sites
      u3j_nv2_site* sit_u;                // array of sites
    } cal_u;                          // call site data
    struct {
      c3_w      len_w;                // number of registration sites
      u3j_nv2_rite* rit_u;                // array of sites
    } reg_u;                          // registration site data
  } u3n_nv2_prog;

void
u3n_nv2_reclaim(void);

void
u3n_nv2_ream(void);

#endif /* #ifndef U3_NOCK_MIGRATE_V2_H */
