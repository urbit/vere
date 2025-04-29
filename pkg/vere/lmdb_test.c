
/// @file

#include "noun.h"
#include "vere.h"
#include "ivory.h"
#include "ur/ur.h"
#include "db/lmdb.h"
// #include "pact.h"


// defined in noun/hashtable.c
c3_w _ch_skip_slot(c3_w mug_w, c3_w lef_w);

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  c3_d          len_d = u3_Ivory_pill_len;
  c3_y*         byt_y = u3_Ivory_pill;
  u3_cue_xeno*  sil_u;
  u3_weak       pil;

  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 29);
  sil_u = u3s_cue_xeno_init_with(ur_fib27, ur_fib28);
  if ( u3_none == (pil = u3s_cue_xeno_with(sil_u, len_d, byt_y)) ) {
    printf("*** fail _setup 1\n");
    exit(1);
  }
  u3s_cue_xeno_done(sil_u);
  if ( c3n == u3v_boot_lite(pil) ) {
    printf("*** fail _setup 2\n");
    exit(1);
  }
}


/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  int e;
  MDB_env *env;

  if ((e = mdb_env_create(&env))) {
    fprintf(stderr, "unable to create lmdb environment\n");
    exit(1);
  }
  mdb_env_set_maxdbs(env, 1);

  mdb_env_set_mapsize(env, (size_t)1048576 * (size_t)100000); // 1MB * 100000

  if ((e = mdb_env_open(env, "/Users/pkova/Desktop/vere", 0, 0664))) {
    fprintf(stderr, "unable to open lmdb database\n");
    exit(1);
  }

  MDB_txn *transaction;
  MDB_dbi  mdb;

  int err;

  if ( (err = mdb_txn_begin(env, 0, MDB_RDONLY, &transaction)) ) {
    fprintf(stderr, "lmdb read txn_begin fail\n");
    return -1;
  }


  if ( (err = mdb_dbi_open(transaction, "EVENTS", 0, &mdb)) ) {
    fprintf(stderr, "unable to open purchases db\n");
    mdb_txn_abort(transaction);
    return -1;
  }

  MDB_val value;
  for (c3_d key_d = 13319500687; key_d < 23319500788; key_d++) {
    MDB_val key = { .mv_size = 8, .mv_data = &key_d };

    if ( (err = mdb_get(transaction, mdb, &key, &value)) ) {
      fprintf(stderr, "lmdb could not find key %i\n", err);
      mdb_txn_abort(transaction);
      return -1;
    }


    u3_noun jamd = u3i_bytes(value.mv_size-4, value.mv_data+4);
    u3_noun cued = u3qe_cue(jamd);

    u3_noun duct = u3h(u3t(cued));
    u3_noun task = u3h(u3t(u3t(cued)));

    if ( (task == c3__heer) &&
         (44389 == u3qc_cut(3, 9, 2, u3t(u3t(u3t(u3t(cued)))))) )
      {
      u3m_p("", cued);
      }

    u3z(jamd);
    u3z(cued);

    /* if ( 105 == u3h(u3h(u3t(cued))) ) { */
      /* u3m_p("", cued); */
    /* } */
  }
}
