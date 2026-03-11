#include "db/lmdb.h"

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define _alloc(sz)    malloc(sz)
#define _free(ptr)    free(ptr)

//  default mmap size for lmdb (2GB)
#define LMDB_MAP_SIZE (1ULL << 31)

/* _test_make_tmpdir(): create unique temporary directory for lmdb.
**
**   creates /tmp/lmdb_test_XXXXXX and returns the path.
**   returns: heap-allocated path (caller must free)
*/
static c3_c*
_test_make_tmpdir(void)
{
  c3_c  pat_c[] = "/tmp/lmdb_test_XXXXXX";
  c3_c* dir_c = mkdtemp(pat_c);

  if ( !dir_c ) {
    fprintf(stderr, "lmdb_test: mkdtemp failed: %s\r\n", strerror(errno));
    return 0;
  }

  c3_c* ret_c = _alloc(strlen(dir_c) + 1);
  strcpy(ret_c, dir_c);
  return ret_c;
}

/* _test_rm_rf(): recursively remove directory contents.
**
**   expects path like /tmp/lmdb_test_XXXXXX
**   removes the directory and all contents
*/
static void
_test_rm_rf(const c3_c* pax_c)
{
  if ( !pax_c || strncmp(pax_c, "/tmp", 4) != 0 ) {
    fprintf(stderr, "lmdb_test: refusing to remove non-/tmp path: %s\r\n", pax_c);
    exit(1);
  }

  c3_c cmd_c[8192];
  snprintf(cmd_c, sizeof(cmd_c), "rm -rf %s", pax_c);
  system(cmd_c);
}

//==============================================================================
// Benchmarks
//==============================================================================

/* _bench_make_event(): create a dummy event of specified size.
**
**   creates a buffer filled with a pattern based on the event number.
**
**   returns: heap-allocated buffer (caller must free)
*/
static c3_y*
_bench_make_event(c3_z siz_z, c3_d eve_d)
{
  c3_y* buf_y = _alloc(siz_z);

  //  mug: simple hash from event number
  c3_w mug_w = (c3_w)(eve_d * 0x12345678);
  memcpy(buf_y, &mug_w, 4);

  //  fill remaining bytes with pattern
  for ( c3_z i = 4; i < siz_z; i++ ) {
    buf_y[i] = (c3_y)((eve_d + i) & 0xFF);
  }

  return buf_y;
}

/* _bench_get_time_ns(): get current time in nanoseconds.
*/
static c3_d
_bench_get_time_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (c3_d)ts.tv_sec * 1000000000ULL + (c3_d)ts.tv_nsec;
}

/* _bench_write_speed(): benchmark write performance.
**
**   writes [num_d] events of [siz_z] bytes each, one at a time.
**   reports total time, events/sec, MB/s, and per-event latency.
*/
static c3_i
_bench_write_speed(c3_d num_d, c3_z siz_z)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  MDB_env* env_u = u3_lmdb_init(dir_c, LMDB_MAP_SIZE);

  if ( !env_u ) {
    fprintf(stderr, "  write_speed: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  pre-allocate event buffer (reuse for all writes)
  c3_y* evt_y = _bench_make_event(siz_z, 1);

  //  start timing
  c3_d beg_d = _bench_get_time_ns();

  //  write events one at a time (single-event transactions)
  for ( c3_d i = 0; i < num_d; i++ ) {
    //  update event data pattern for variety
    c3_w mug_w = (c3_w)((i + 1) * 0x12345678);
    memcpy(evt_y, &mug_w, 4);

    void*  byt_p[1] = { evt_y };
    size_t siz_i[1] = { siz_z };

    c3_o sav_o = u3_lmdb_save(env_u, i + 1, 1, byt_p, siz_i);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  write_speed: save failed at event %" PRIu64 "\r\n", i + 1);
      ret_i = 0;
      _free(evt_y);
      goto cleanup;
    }
  }

  //  end timing
  c3_d end_d = _bench_get_time_ns();
  c3_d lap_d = end_d - beg_d;  //  elapsed nanoseconds

  //  calculate metrics
  double elapsed_sec = (double)lap_d / 1e9;
  double events_per_sec = (double)num_d / elapsed_sec;
  double total_bytes = (double)num_d * (double)siz_z;
  double mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / elapsed_sec;
  double us_per_event = ((double)lap_d / 1000.0) / (double)num_d;

  //  report results
  fprintf(stderr, "\r\n");
  fprintf(stderr, "  write_speed benchmark (single-event writes):\r\n");
  fprintf(stderr, "    events written:  %" PRIu64 "\r\n", num_d);
  fprintf(stderr, "    event size:      %" PRIu64 " bytes\r\n", (c3_d)siz_z);
  fprintf(stderr, "    total data:      %.2f MB\r\n", total_bytes / (1024.0 * 1024.0));
  fprintf(stderr, "    total time:      %.3f seconds\r\n", elapsed_sec);
  fprintf(stderr, "    write speed:     %.0f events/sec\r\n", events_per_sec);
  fprintf(stderr, "    throughput:      %.2f MB/sec\r\n", mb_per_sec);
  fprintf(stderr, "    latency:         %.1f us/event\r\n", us_per_event);
  fprintf(stderr, "\r\n");

  _free(evt_y);
  u3_lmdb_exit(env_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  write_speed_benchmark: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _bench_write_speed_batched(): benchmark batched write performance.
**
**   writes [num_d] events of [siz_z] bytes in batches of [bat_d].
**   reports total time, events/sec, MB/s, and per-event latency.
*/
static c3_i
_bench_write_speed_batched(c3_d num_d, c3_z siz_z, c3_d bat_d)
{
  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  MDB_env* env_u = u3_lmdb_init(dir_c, LMDB_MAP_SIZE);

  if ( !env_u ) {
    fprintf(stderr, "  write_speed_batched: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  allocate batch arrays
  c3_y**  evt_y = _alloc(bat_d * sizeof(c3_y*));
  void**  byt_p = _alloc(bat_d * sizeof(void*));
  size_t* siz_i = _alloc(bat_d * sizeof(size_t));

  //  pre-allocate event buffers for batch
  for ( c3_d i = 0; i < bat_d; i++ ) {
    evt_y[i] = _bench_make_event(siz_z, i + 1);
    byt_p[i] = evt_y[i];
    siz_i[i] = siz_z;
  }

  //  start timing
  c3_d start_d = _bench_get_time_ns();

  //  write events in batches
  c3_d written_d = 0;
  while ( written_d < num_d ) {
    c3_d remaining = num_d - written_d;
    c3_d batch_size = (remaining < bat_d) ? remaining : bat_d;

    //  update event data patterns
    for ( c3_d i = 0; i < batch_size; i++ ) {
      c3_w mug_w = (c3_w)((written_d + i + 1) * 0x12345678);
      memcpy(evt_y[i], &mug_w, 4);
    }

    c3_o sav_o = u3_lmdb_save(env_u, written_d + 1, batch_size, byt_p, siz_i);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  write_speed_batched: save failed at event %" PRIu64 "\r\n",
              written_d + 1);
      ret_i = 0;
      goto cleanup_buffers;
    }

    written_d += batch_size;
  }

  //  end timing
  c3_d end_d = _bench_get_time_ns();
  c3_d elapsed_ns = end_d - start_d;

  //  calculate metrics
  double elapsed_sec = (double)elapsed_ns / 1e9;
  double events_per_sec = (double)num_d / elapsed_sec;
  double total_bytes = (double)num_d * (double)siz_z;
  double mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / elapsed_sec;
  double us_per_event = ((double)elapsed_ns / 1000.0) / (double)num_d;

  //  report results
  fprintf(stderr, "\r\n");
  fprintf(stderr, "  write_speed benchmark (batched writes, batch=%" PRIu64 "):\r\n", bat_d);
  fprintf(stderr, "    events written:  %" PRIu64 "\r\n", num_d);
  fprintf(stderr, "    event size:      %" PRIu64 " bytes\r\n", (c3_d)siz_z);
  fprintf(stderr, "    total data:      %.2f MB\r\n", total_bytes / (1024.0 * 1024.0));
  fprintf(stderr, "    total time:      %.3f seconds\r\n", elapsed_sec);
  fprintf(stderr, "    write speed:     %.0f events/sec\r\n", events_per_sec);
  fprintf(stderr, "    throughput:      %.2f MB/sec\r\n", mb_per_sec);
  fprintf(stderr, "    latency:         %.1f us/event\r\n", us_per_event);
  fprintf(stderr, "\r\n");

cleanup_buffers:
  for ( c3_d i = 0; i < bat_d; i++ ) {
    _free(evt_y[i]);
  }
  _free(evt_y);
  _free(byt_p);
  _free(siz_i);

  u3_lmdb_exit(env_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  write_speed_batched_benchmark: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

/* _bench_write_speed_mixed(): benchmark mixed batch-size write performance.
**
**   writes [num_d] events of [siz_z] bytes using a realistic distribution
**   of batch sizes (1-9), interleaved via deterministic PRNG.
**   reports total time, events/sec, MB/s, per-event latency, and save calls.
*/
static c3_i
_bench_write_speed_mixed(c3_d num_d, c3_z siz_z)
{
  //  batch size distribution from production telemetry
  //
  static const c3_d bat_d[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  static const c3_d cnt_d[9] = {
    2128433, 407761, 234541, 89359, 41390, 21376, 10945, 5399, 5466
  };

  //  compute original total events for scaling
  //
  c3_d ori_d = 0;
  for ( c3_d i = 0; i < 9; i++ ) {
    ori_d += bat_d[i] * cnt_d[i];
  }

  //  scale counts proportionally to num_d
  //
  c3_d rem_d[9];
  c3_d tot_d = 0;
  for ( c3_d i = 0; i < 9; i++ ) {
    rem_d[i] = (cnt_d[i] * num_d) / ori_d;
    if ( (0 == rem_d[i]) && (cnt_d[i] > 0) ) {
      rem_d[i] = 1;
    }
    tot_d += rem_d[i];
  }

  c3_c* dir_c = _test_make_tmpdir();
  if ( !dir_c ) return 0;

  c3_i     ret_i = 1;
  MDB_env* env_u = u3_lmdb_init(dir_c, LMDB_MAP_SIZE);

  if ( !env_u ) {
    fprintf(stderr, "  write_speed_mixed: init failed\r\n");
    ret_i = 0;
    goto cleanup;
  }

  //  pre-allocate event buffers for max batch size (9)
  //
  c3_y*  evt_y[9];
  void*  byt_p[9];
  size_t siz_i[9];

  for ( c3_d i = 0; i < 9; i++ ) {
    evt_y[i] = _bench_make_event(siz_z, i + 1);
    byt_p[i] = evt_y[i];
    siz_i[i] = siz_z;
  }

  //  deterministic xorshift32 PRNG
  //
  c3_w rng_w = 12345;

  c3_d wit_d = 0;  //  events written
  c3_d cal_d = 0;  //  save calls made

  //  start timing
  //
  c3_d beg_d = _bench_get_time_ns();

  while ( tot_d > 0 ) {
    //  xorshift32 step
    //
    rng_w ^= rng_w << 13;
    rng_w ^= rng_w >> 17;
    rng_w ^= rng_w << 5;

    //  weighted selection from remaining counts
    //
    c3_d pick = (c3_d)rng_w % tot_d;
    c3_d acc  = 0;
    c3_d idx  = 0;

    for ( idx = 0; idx < 9; idx++ ) {
      acc += rem_d[idx];
      if ( pick < acc ) break;
    }

    c3_d bsz = bat_d[idx];
    rem_d[idx]--;
    tot_d--;

    //  update mug patterns in event buffers
    //
    for ( c3_d j = 0; j < bsz; j++ ) {
      c3_w mug_w = (c3_w)((wit_d + j + 1) * 0x12345678);
      memcpy(evt_y[j], &mug_w, 4);
    }

    c3_o sav_o = u3_lmdb_save(env_u, wit_d + 1, bsz, byt_p, siz_i);
    if ( c3n == sav_o ) {
      fprintf(stderr, "  write_speed_mixed: save failed at event %" PRIu64 "\r\n",
              wit_d + 1);
      ret_i = 0;
      goto cleanup_buffers;
    }

    wit_d += bsz;
    cal_d++;
  }

  //  end timing
  //
  c3_d end_d = _bench_get_time_ns();
  c3_d lap_d = end_d - beg_d;

  //  calculate metrics
  //
  double elapsed_sec = (double)lap_d / 1e9;
  double events_per_sec = (double)wit_d / elapsed_sec;
  double total_bytes = (double)wit_d * (double)siz_z;
  double mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / elapsed_sec;
  double us_per_event = ((double)lap_d / 1000.0) / (double)wit_d;

  //  report results
  //
  fprintf(stderr, "\r\n");
  fprintf(stderr, "  write_speed benchmark (mixed batch sizes 1-9):\r\n");
  fprintf(stderr, "    events written:  %" PRIu64 "\r\n", wit_d);
  fprintf(stderr, "    save calls:      %" PRIu64 "\r\n", cal_d);
  fprintf(stderr, "    event size:      %" PRIu64 " bytes\r\n", (c3_d)siz_z);
  fprintf(stderr, "    total data:      %.2f MB\r\n", total_bytes / (1024.0 * 1024.0));
  fprintf(stderr, "    total time:      %.3f seconds\r\n", elapsed_sec);
  fprintf(stderr, "    write speed:     %.0f events/sec\r\n", events_per_sec);
  fprintf(stderr, "    throughput:      %.2f MB/sec\r\n", mb_per_sec);
  fprintf(stderr, "    latency:         %.1f us/event\r\n", us_per_event);
  fprintf(stderr, "\r\n");

cleanup_buffers:
  for ( c3_d i = 0; i < 9; i++ ) {
    _free(evt_y[i]);
  }

  u3_lmdb_exit(env_u);

cleanup:
  _test_rm_rf(dir_c);
  _free(dir_c);

  fprintf(stderr, "  write_speed_mixed_benchmark: %s\r\n", ret_i ? "ok" : "FAILED");
  return ret_i;
}

//==============================================================================
// Main
//==============================================================================

int
main(int argc, char* argv[])
{
  c3_i ret_i = 1;

  //  benchmarks
  // ret_i &= _bench_write_speed(1000, 128);
  // ret_i &= _bench_write_speed_batched(100000, 1280, 1000);
  ret_i &= _bench_write_speed_mixed(10000, 128);

  fprintf(stderr, "\r\n");
  if ( ret_i ) {
    fprintf(stderr, "lmdb_tests: ok\n");
    return 0;
  }
  else {
    fprintf(stderr, "lmdb_tests: failed\n");
    return 1;
  }
}
