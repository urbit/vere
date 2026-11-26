/// @file

#include "./mars.c"

/* _setup(): prepare for tests. with no snapshot, every page of the
**           heap counts as dirty.
*/
static void
_setup(void)
{
  u3m_boot_lite((size_t)1 << 28);
}

/* _mars(): a serf at work, [eve_d] events past its last snapshot.
*/
static void
_mars(u3_mars* mar_u, c3_d eve_d)
{
  memset(mar_u, 0, sizeof(*mar_u));
  mar_u->sat_e       = u3_mars_work_e;
  mar_u->sav_u.eve_d = 100;
  mar_u->dun_d       = 100 + eve_d;
}

/* _pages(): megabytes as loom pages, as _mars_save_due() has it.
*/
static c3_d
_pages(c3_h meg_h)
{
  return ((c3_d)meg_h << 20) >> (u3a_page + u3a_word_bytes_shift);
}

/* _test_save_time(): a snapshot is due once the events since the last
**                    one have taken --snap-time, and never without one.
*/
static c3_i
_test_save_time(void)
{
  u3_mars mar_u;
  c3_i    ret_i = 1;

  u3_Host.ops_u.sap_h = 2;      //  seconds
  u3_Host.ops_u.sad_h = 0;
  u3_Host.ops_u.sat_h = 1;

  //  one microsecond short
  //
  _mars(&mar_u, 1);
  mar_u.sav_u.use_d = 1999999;
  _mars_save_due(&mar_u);

  if ( u3_mars_work_e != mar_u.sat_e ) {
    fprintf(stderr, "mars: save time: early\r\n");
    ret_i = 0;
  }

  //  at the budget
  //
  mar_u.sav_u.use_d = 2000000;
  _mars_save_due(&mar_u);

  if ( u3_mars_save_e != mar_u.sat_e ) {
    fprintf(stderr, "mars: save time: not due at the budget\r\n");
    ret_i = 0;
  }

  //  no event since the last snapshot: nothing to replay
  //
  _mars(&mar_u, 0);
  mar_u.sav_u.use_d = 5000000;
  _mars_save_due(&mar_u);

  if ( u3_mars_work_e != mar_u.sat_e ) {
    fprintf(stderr, "mars: save time: due with no event\r\n");
    ret_i = 0;
  }

  //  a serf that is saving or exiting is left alone
  //
  _mars(&mar_u, 1);
  mar_u.sav_u.use_d = 5000000;
  mar_u.sat_e       = u3_mars_exit_e;
  _mars_save_due(&mar_u);

  if ( u3_mars_exit_e != mar_u.sat_e ) {
    fprintf(stderr, "mars: save time: exit overridden\r\n");
    ret_i = 0;
  }

  return ret_i;
}

/* _test_save_dirty(): dirty pages are counted every --snap-tasks tasks,
**                     and a snapshot is due at --snap-dirty.
*/
static c3_i
_test_save_dirty(void)
{
  u3_mars mar_u;
  c3_i    ret_i = 1;
  c3_h    i_h;

  //  grow the heap past 2 MB, then a page at a time until a whole
  //  number of megabytes is dirty, to test the threshold exactly
  //
  c3_d    pag_d = _pages(1);
  void*   ptr_v = u3a_malloc((size_t)1 << 21);
  void*   mor_v[256];
  c3_h    mor_h = 0;
  c3_d    dir_d;
  c3_h    meg_h;

  while (  (0 != (u3e_dirty() % pag_d))
        && (mor_h < 256) )
  {
    mor_v[mor_h++] = u3a_malloc((size_t)1 << (u3a_page + u3a_word_bytes_shift));
  }

  dir_d = u3e_dirty();
  meg_h = (c3_h)(dir_d / pag_d);

  if (  (meg_h < 2)
     || (0 != (dir_d % pag_d)) )
  {
    fprintf(stderr, "mars: save dirty: %" PRIu64 " pages dirty\r\n", dir_d);
    return 0;
  }

  u3_Host.ops_u.sap_h = 120;
  u3_Host.ops_u.sat_h = 4;

  //  at the threshold: due at the fourth task, not before; peeks
  //  count, so no event is needed
  //
  u3_Host.ops_u.sad_h = meg_h;
  _mars(&mar_u, 0);

  for ( i_h = 1; i_h < 4; i_h++ ) {
    _mars_save_due(&mar_u);

    if (  (u3_mars_work_e != mar_u.sat_e)
       || (i_h != mar_u.sav_u.tak_d) )
    {
      fprintf(stderr, "mars: save dirty: early at task %u\r\n", i_h);
      ret_i = 0;
    }
  }

  _mars_save_due(&mar_u);

  if (  (u3_mars_save_e != mar_u.sat_e)
     || (0 != mar_u.sav_u.tak_d) )
  {
    fprintf(stderr, "mars: save dirty: not due at the threshold\r\n");
    ret_i = 0;
  }

  //  a threshold below what stays dirty: one snapshot per four tasks
  //
  u3_Host.ops_u.sad_h = 1;
  _mars(&mar_u, 0);

  {
    c3_h sav_h = 0;

    for ( i_h = 0; i_h < 40; i_h++ ) {
      _mars_save_due(&mar_u);

      if ( u3_mars_save_e == mar_u.sat_e ) {
        sav_h++;
        mar_u.sat_e = u3_mars_work_e;
      }
    }

    if ( 10 != sav_h ) {
      fprintf(stderr, "mars: save dirty: %u snapshots in 40 tasks\r\n", sav_h);
      ret_i = 0;
    }
  }

  //  one megabyte over what is dirty: checked, not due
  //
  u3_Host.ops_u.sad_h = meg_h + 1;
  _mars(&mar_u, 0);

  for ( i_h = 0; i_h < 8; i_h++ ) {
    _mars_save_due(&mar_u);

    if ( u3_mars_work_e != mar_u.sat_e ) {
      fprintf(stderr, "mars: save dirty: due under the threshold\r\n");
      ret_i = 0;
      break;
    }
  }

  if ( 0 != mar_u.sav_u.tak_d ) {
    fprintf(stderr, "mars: save dirty: count kept past a check\r\n");
    ret_i = 0;
  }

  //  0: never, and nothing is counted
  //
  u3_Host.ops_u.sad_h = 0;
  _mars(&mar_u, 0);

  for ( i_h = 0; i_h < 8; i_h++ ) {
    _mars_save_due(&mar_u);
  }

  if (  (u3_mars_work_e != mar_u.sat_e)
     || (0 != mar_u.sav_u.tak_d) )
  {
    fprintf(stderr, "mars: save dirty: due when off\r\n");
    ret_i = 0;
  }

  //  the time budget comes first, and leaves the task count alone
  //
  u3_Host.ops_u.sad_h = meg_h;
  _mars(&mar_u, 1);
  mar_u.sav_u.tak_d = 2;
  mar_u.sav_u.use_d = 120000000;
  _mars_save_due(&mar_u);

  if (  (u3_mars_save_e != mar_u.sat_e)
     || (2 != mar_u.sav_u.tak_d) )
  {
    fprintf(stderr, "mars: save dirty: time budget not first\r\n");
    ret_i = 0;
  }

  while ( mor_h ) {
    u3a_free(mor_v[--mor_h]);
  }
  u3a_free(ptr_v);

  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  c3_i ret_i = 0;

  _setup();

  if ( !_test_save_time() ) {
    fprintf(stderr, "mars: save time tests failed\r\n");
    ret_i = 1;
  }

  if ( !_test_save_dirty() ) {
    fprintf(stderr, "mars: save dirty tests failed\r\n");
    ret_i = 1;
  }

  if ( ret_i ) {
    return ret_i;
  }

  //  GC, failing on a leak
  //
  u3C.wag_h |= u3o_leak_crash;
  u3m_grab();

  fprintf(stderr, "mars okeedokee\r\n");
  return 0;
}
