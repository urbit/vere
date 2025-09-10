/// @file

#include "vere.h"
#include "io/ames/stun.h"
#include "io/ames/lamp.h"
#include "io/mesa/mesa.h"

#include "noun.h"
#include "ur/ur.h"

#include "zlib.h"

#include "ent/ent.h"

#include <arpa/inet.h>

#define FINE_PAGE      4096             //  packets per page
#define FINE_FRAG      1024             //  bytes per fragment packet
#define FINE_PATH_MAX   384             //  longest allowed scry path
#define HEAD_SIZE         4             //  header size in bytes

//  a hack to work around the inability to delete from a hashtable
//
#define FINE_PEND         1             //  scry cache sentinel value: "pending"
#define FINE_DEAD         2             //  scry cache sentinel value: "dead"

#define QUEUE_MAX        30             //  max number of packets in queue

#define DIRECT_ROUTE_TIMEOUT_MICROS 120000000

  typedef struct _u3_ames u3_ames;
  typedef struct _u3_mesa_auto {
    u3_auto  car_u;
    u3_ames* sam_u;
    u3_pier* pir_u;
    union {                             //  uv udp handle
      uv_udp_t         wax_u;             //
      uv_handle_t      had_u;             //
    };                                  //
  } u3_mesa_auto;

/* u3_fine: fine networking
*/
  typedef struct _u3_fine {
    c3_y              ver_y;            //  fine protocol
    u3p(u3h_root)     sac_p;            //  scry cache hashtable
    struct _u3_ames*  sam_u;            //  ames backpointer
  } u3_fine;

/* u3_ames: ames networking.
*/
  typedef struct _u3_ames {             //  packet network state
    u3_mesa_auto*    mes_u;             //  ames driver
    u3_pier*         pir_u;
    u3_fine          fin_s;             //  fine networking
    union {                             //  uv udp handle
      uv_udp_t*      wax_u;             //
      uv_handle_t*   had_u;             //
    };                                  //
    c3_l             sev_l;             //  instance number
    ur_cue_test_t*   tes_u;             //  cue-test handle
    u3_cue_xeno*     sil_u;             //  cue handle
    c3_y             ver_y;             //  protocol version
    struct {                            //    config:
      c3_o           see_o;             //  can scry
      c3_o           fit_o;             //  filtering active
    } fig_u;                            //
    struct {                            //    stats:
      c3_d           dop_d;             //  drop count
      c3_d           fod_d;             //  forwards dropped count
      c3_d           foq_d;             //  forward queue size
      c3_d           fow_d;             //  forwarded count
      c3_o           for_o;             //  forwarding enabled
      c3_d           hed_d;             //  failed to read header
      c3_d           vet_d;             //  version mismatches filtered
      c3_d           mut_d;             //  invalid mugs filtered
      c3_d           pre_d;             //  failed to read prelude
      c3_d           wal_d;             //  failed to read wail
      c3_d           wap_d;             //  failed to read wail path
      c3_d           fal_d;             //  crash count
      c3_d           vil_d;             //  encryption failures
      c3_d           saw_d;             //  successive scry failures
    } sat_u;                            //
  } u3_ames;

STATIC_ASSERT(
    ( ((void*)(u3_ames*)(void*)0) ==
      ((void*)(u3_ames*)(void*)&(((u3_ames*)(void*)0)->mes_u)) ),
    "u3_ames struct alignment" );

/* u3_head: ames or fine packet header
*/
  typedef struct _u3_head {
    c3_o req_o;                         //  is request (fine only)
    c3_o sim_o;                         //  is ames protocol?
    c3_y ver_y;                         //  protocol version
    c3_y sac_y;                         //  sender class
    c3_y rac_y;                         //  receiver class
    c3_l mug_l;                         //  truncated mug hash of u3_body
    c3_o rel_o;                         //  relayed?
  } u3_head;

/* u3_prel: ames/fine packet prelude
*/
  typedef struct _u3_prel {
    c3_y     sic_y;                        //  sender life tick
    c3_y     ric_y;                        //  receiver life tick
    u3_ship  sen_u;                        //  sender/requester
    u3_ship  rec_u;                        //  receiver/responder
    c3_d     rog_d;                        //  origin lane (optional)
  } u3_prel;

/* u3_peep: unsigned fine request body
*/
  typedef struct _u3_peep {
    c3_w    fra_w;                      //  fragment number
    c3_s    len_s;                      //  path length
    c3_c*   pat_c;                      //  path as ascii
  } u3_peep;

/*  u3_wail: fine request body
*/
  typedef struct _u3_wail {
    c3_y    tag_y;                      //  tag (always 0, unsigned)
    u3_peep pep_u;                      //  request payload
  } u3_wail;

/* u3_meow: response portion of purr packet
 *
 *   siz_s: number of bytes in the packet
*/
  typedef struct _u3_meow {
    c3_y    sig_y[64];                  //  host signature
    c3_w    num_w;                      //  number of fragments
    c3_s    siz_s;                      //  datum size (actual)
    c3_y*   dat_y;                      //  datum (0 if null response)
  } u3_meow;

/* u3_purr: fine packet response
*/
  typedef struct _u3_purr {
    u3_peep pep_u;                      //  fragment number + path
    u3_meow mew_u;                      //  fragment
  } u3_purr;

/* u3_body: ames packet body
*/
  typedef struct _u3_body {
    c3_s    con_s;                      //  content size
    c3_y*   con_y;                      //  content
    c3_l    mug_l;                      //  checksum
  } u3_body;

/* u3_ptag: packet-type tag
*/
  typedef enum _u3_ptag {
    PACT_AMES = 1,                      //  ames packet
    PACT_WAIL = 2,                      //  fine request packet
    PACT_PURR = 3                       //  fine response packet
  } u3_ptag;

  typedef struct _u3_ref_hun {
    c3_w             ref_w;
    c3_w             len_w;             //  length in bytes
    c3_y             hun_y[0];          //  packet buffer
  } u3_ref_hun;

  typedef struct _u3_send_handle {
    uv_udp_send_t    snd_u;             //  udp send request
    u3_ref_hun       *hun_u;
  } u3_send_handle;

/* u3_pact: ames packet
 *
 *   Filled in piece by piece as we parse or construct it.
*/
  typedef struct _u3_pact {
    struct _u3_ames* sam_u;             //  ames backpointer
    sockaddr_in      lan_u;             //  destination/origin lane
    u3_ref_hun*      hun_u;
    u3_head          hed_u;             //  head of packet
    u3_prel          pre_u;             //  packet prelude
    u3_ptag          typ_y;             //  packet type tag
    c3_d             her_d;             //  time packet was heard
    c3_o             for_o;             //  are we forwarding
    union {
      u3_body bod_u;                    //  tagged by PACT_AMES
      u3_wail wal_u;                    //  tagged by PACT_WAIL
      u3_purr pur_u;                    //  tagged by PACT_PURR
    };
  } u3_pact;

#define _str_typ(typ_y) (           \
    ( PACT_AMES == typ_y ) ? "ames" \
  : ( PACT_WAIL == typ_y ) ? "wail" \
  : ( PACT_PURR == typ_y ) ? "purr" : "????")

const c3_c* PATH_PARSER =
  ";~(pfix fas (most fas (cook crip (star ;~(less fas prn)))))";

static c3_o net_o = c3y;  // online heuristic to limit verbosity

///* _ames_alloc(): libuv buffer allocator.
//*/
//static void
//_ames_alloc(uv_handle_t* had_u,
//            size_t len_i,
//            uv_buf_t* buf
//            )
//{
//  //  we allocate 2K, which gives us plenty of space
//  //  for a single ames packet (max size 1060 bytes)
//  //
//  void* ptr_v = c3_malloc(2048);
//  *buf = uv_buf_init(ptr_v, 2048);
//}

static void
_ames_ref_hun_lose(u3_ref_hun* hun_u) {
  assert(0 != hun_u->ref_w);
  hun_u->ref_w--;
  if (0 == hun_u->ref_w) c3_free(hun_u);
}

static u3_ref_hun*
_ames_ref_hun_gain(u3_ref_hun* hun_u) {
  assert(0 != hun_u->ref_w);
  assert(UINT32_MAX != hun_u->ref_w);
  hun_u->ref_w++;
  return hun_u;
}

static u3_ref_hun*
_ames_ref_hun_new(c3_w len_w) {
  u3_ref_hun* hun_u = c3_malloc(sizeof(hun_u) + len_w);
  hun_u->ref_w = 1;
  hun_u->len_w = len_w;
  return hun_u;
}

static void
_ames_pact_free(u3_pact* pac_u)
{
  switch ( pac_u->typ_y ) {
    case PACT_AMES:
      c3_free(pac_u->bod_u.con_y);
      break;

    case PACT_WAIL:
      c3_free(pac_u->wal_u.pep_u.pat_c);
      break;

    case PACT_PURR:
      c3_free(pac_u->pur_u.pep_u.pat_c);
      c3_free(pac_u->pur_u.mew_u.dat_y);
      break;

    default:
      u3l_log("ames_pact_free: bad packet type %s",
              _str_typ(pac_u->typ_y));
      u3_pier_bail(u3_king_stub());
  }

  _ames_ref_hun_lose(pac_u->hun_u);
  c3_free(pac_u);
}

static inline u3_ptag
_ames_pact_typ(u3_head* hed_u)
{
  return (( c3y == hed_u->sim_o ) ? PACT_AMES :
          ( c3y == hed_u->req_o ) ? PACT_WAIL : PACT_PURR);
}

static inline c3_y
_ames_origin_size(u3_head* hed_u)
{
  return ( c3y == hed_u->rel_o ) ? 6 : 0;  //  origin is 6 bytes
}

static c3_y
_ames_prel_size(u3_head* hed_u)
{
  c3_y lif_y = 1;
  c3_y sen_y = 2 << hed_u->sac_y;
  c3_y rec_y = 2 << hed_u->rac_y;
  c3_y rog_y = _ames_origin_size(hed_u);
  return lif_y + sen_y + rec_y + rog_y;
}

static inline c3_s
_ames_body_size(u3_body* bod_u)
{
  return bod_u->con_s;
}

static inline c3_s
_fine_peep_size(u3_peep* pep_u)
{
  return (
    sizeof(pep_u->fra_w) +
    sizeof(pep_u->len_s) +
    pep_u->len_s);
}

static inline c3_y
_fine_bytes_word(c3_w num_w)
{
  return (c3_bits_word(num_w) + 7) >> 3;
}

static inline c3_s
_fine_meow_size(u3_meow* mew_u)
{
  c3_y cur_y = 0;
  if (mew_u->siz_s != 0) {
    cur_y = sizeof(mew_u->num_w);
  }
  else {
    cur_y = _fine_bytes_word(mew_u->num_w);
  }
  return (
    sizeof(mew_u->sig_y) +
    cur_y +
    mew_u->siz_s);
}

static c3_s
_fine_purr_size(u3_purr* pur_u)
{
  c3_s pur_s = _fine_peep_size(&pur_u->pep_u);
  c3_s mew_s = _fine_meow_size(&pur_u->mew_u);
  return pur_s + mew_s;
}

static c3_o
_ames_check_mug(u3_pact* pac_u)
{
  c3_w rog_w = HEAD_SIZE + _ames_origin_size(&pac_u->hed_u);
  c3_l mug_l = u3r_mug_bytes(pac_u->hun_u->hun_y + rog_w,
                             pac_u->hun_u->len_w - rog_w);
  //  u3l_log("len_w: %u, rog_w: %u, bod_l 0x%05x, hed_l 0x%05x",
  //          pac_u->hun_u->len_w, rog_w,
  //          (mug_l & 0xfffff),
  //          (pac_u->hed_u.mug_l & 0xfffff));
  return (
    ((mug_l & 0xfffff) == (pac_u->hed_u.mug_l & 0xfffff))
    ? c3y : c3n);
}

/* _ames_sift_head(): parse packet header.
*/
static void
_ames_sift_head(u3_head* hed_u, c3_y buf_y[4])
{
  c3_w hed_w = c3_sift_word(buf_y);

  //  first two bits are reserved
  //
  hed_u->req_o = (hed_w >>  2) & 0x1;
  hed_u->sim_o = (hed_w >>  3) & 0x1;
  hed_u->ver_y = (hed_w >>  4) & 0x7;
  hed_u->sac_y = (hed_w >>  7) & 0x3;
  hed_u->rac_y = (hed_w >>  9) & 0x3;
  hed_u->mug_l = (hed_w >> 11) & 0xfffff; // 20 bits
  hed_u->rel_o = (hed_w >> 31) & 0x1;
}

/* _ames_sift_prel(): parse prelude,
*/
static void
_ames_sift_prel(u3_head* hed_u,
                u3_prel* pre_u,
                c3_y*    buf_y)
{
  c3_y sen_y, rec_y;
  c3_w cur_w = 0;

  //  if packet is relayed, parse 6-byte origin field
  //
  if ( c3y == hed_u->rel_o ) {
    c3_y rag_y[8] = {0};
    memcpy(rag_y, buf_y + cur_w, 6);
    pre_u->rog_d = c3_sift_chub(rag_y);
    cur_w += 6;
  }
  else {
    pre_u->rog_d = 0;
  }

  //  parse life ticks
  //
  pre_u->sic_y = buf_y[cur_w]        & 0xf;
  pre_u->ric_y = (buf_y[cur_w] >> 4) & 0xf;
  cur_w++;

  //  parse sender ship
  //
  sen_y = 2 << hed_u->sac_y;
  pre_u->sen_u = u3_ship_of_bytes(sen_y, buf_y + cur_w);
  cur_w += sen_y;

  //  parse receiver ship
  //
  rec_y = 2 << hed_u->rac_y;
  pre_u->rec_u = u3_ship_of_bytes(rec_y, buf_y + cur_w);
  cur_w += rec_y;
}

/* _fine_sift_wail(): parse request body, returning success
*/
static c3_o
_fine_sift_wail(u3_pact* pac_u, c3_w cur_w)
{
  c3_w fra_w = sizeof(pac_u->wal_u.pep_u.fra_w);
  c3_w len_w = sizeof(pac_u->wal_u.pep_u.len_s);
  c3_w exp_w = fra_w + len_w;
  c3_s len_s;

  if ( cur_w + exp_w > pac_u->hun_u->len_w ) {
    u3l_log("fine: wail not big enough");
    return c3n;
  }

  //  parse tag
  //
  pac_u->wal_u.tag_y = *(pac_u->hun_u->hun_y + cur_w);
  cur_w++;

  if ( 0 != pac_u->wal_u.tag_y ) {
    u3l_log("fine: wail tag unknown %u", pac_u->wal_u.tag_y);
    return c3n;
  }

  //  parse fragment number
  //
  pac_u->wal_u.pep_u.fra_w = c3_sift_word(pac_u->hun_u->hun_y + cur_w);
  cur_w += fra_w;

  //  parse path length field
  //
  len_s = c3_sift_short(pac_u->hun_u->hun_y + cur_w);
  pac_u->wal_u.pep_u.len_s = len_s;
  cur_w += len_w;

  if ( len_s > FINE_PATH_MAX ) {
    u3l_log("ames wail len: %u, max %u", len_s, FINE_PATH_MAX);
    return c3n;
  }

  {
    c3_w tot_w = cur_w + len_s;
    if ( tot_w != pac_u->hun_u->len_w ) {
      u3l_log("fine: wail expected total len: %u, actual %u",
              tot_w, pac_u->hun_u->len_w);
      return c3n;
    }
  }

  //  parse request path
  //
  pac_u->wal_u.pep_u.pat_c = c3_calloc(len_s + 1);
  memcpy(pac_u->wal_u.pep_u.pat_c, pac_u->hun_u->hun_y + cur_w, len_s);
  pac_u->wal_u.pep_u.pat_c[len_s] = '\0';
  return c3y;
}

/* _fine_sift_meow(): parse signed scry response fragment
*/
static c3_o
_fine_sift_meow(u3_meow* mew_u, u3_noun mew)
{
  c3_o ret_o;
  c3_w len_w = u3r_met(3, mew);
  c3_w sig_w = sizeof(mew_u->sig_y);
  c3_w num_w = sizeof(mew_u->num_w);
  c3_w min_w = sig_w + 1;
  c3_w max_w = sig_w + num_w + FINE_FRAG;

  if ( (len_w < min_w) || (len_w > max_w) ) {
    u3l_log("sift_meow len_w %u (min_w %u, max_w %u)", len_w, min_w, max_w);
    ret_o = c3n;
  }
  else {
    c3_w cur_w = 0;

    //  parse signature
    //
    u3r_bytes(cur_w, sig_w, mew_u->sig_y, mew);
    cur_w += sig_w;

    //  parse number of fragments
    //
    u3r_bytes(cur_w, num_w, (c3_y*)&mew_u->num_w, mew);
    num_w = c3_min(num_w, (len_w - cur_w));
    cur_w += num_w;
    u3_assert(len_w >= cur_w);

    //  parse data payload
    //
    mew_u->siz_s = len_w - cur_w;

    if ( !mew_u->siz_s ) {
      mew_u->dat_y = 0;
    }
    else {
      mew_u->dat_y = c3_calloc(mew_u->siz_s);
      u3r_bytes(cur_w, mew_u->siz_s, mew_u->dat_y, mew);
    }

    ret_o = c3y;
  }

  u3z(mew);
  return ret_o;
}

/* _ames_etch_head(): serialize packet header.
*/
static void
_ames_etch_head(u3_head* hed_u, c3_y buf_y[4])
{
  //  only version 0 currently recognized
  //
  u3_assert( 0 == hed_u->ver_y );  //  XX remove after testing

  c3_w hed_w = ((hed_u->req_o &     0x1) <<  2)
             ^ ((hed_u->sim_o &     0x1) <<  3)
             ^ ((hed_u->ver_y &     0x7) <<  4)
             ^ ((hed_u->sac_y &     0x3) <<  7)
             ^ ((hed_u->rac_y &     0x3) <<  9)
             ^ ((hed_u->mug_l & 0xfffff) << 11)
             ^ ((hed_u->rel_o &     0x1) << 31);

  c3_etch_word(buf_y, hed_w);
}

static void
_ames_etch_origin(c3_d rog_d, c3_y* buf_y)
{
  c3_y rag_y[8] = {0};
  c3_etch_chub(rag_y, rog_d);
  memcpy(buf_y, rag_y, 6);
}

/* _ames_etch_prel(): serialize packet prelude
*/
static void
_ames_etch_prel(u3_head* hed_u, u3_prel* pre_u, c3_y* buf_y)
{
  c3_w cur_w = 0;

  //  if packet is relayed, write the 6-byte origin field
  //
  if ( c3y == hed_u->rel_o ) {
    _ames_etch_origin(pre_u->rog_d, buf_y + cur_w);
    cur_w += 6;
  }

  //  write life ticks
  //
  buf_y[cur_w] = (pre_u->sic_y & 0xf) ^ ((pre_u->ric_y & 0xf) << 4);
  cur_w++;

  //  write sender ship
  //
  c3_y sen_y = 2 << hed_u->sac_y;
  u3_ship_to_bytes(pre_u->sen_u, sen_y, buf_y + cur_w);
  cur_w += sen_y;

  //  write receiver ship
  //
  c3_y rec_y = 2 << hed_u->rac_y;
  u3_ship_to_bytes(pre_u->rec_u, sen_y, buf_y + cur_w);
  cur_w += rec_y;
}

/* _fine_etch_peep(): serialize unsigned scry request
*/
static void
_fine_etch_peep(u3_peep* pep_u, c3_y* buf_y)
{
  c3_w cur_w = 0;

  //  write fragment number
  //
  c3_etch_word(buf_y + cur_w, pep_u->fra_w);
  cur_w += sizeof(pep_u->fra_w);

  //  write path length
  //
  c3_etch_short(buf_y + cur_w, pep_u->len_s);
  cur_w += sizeof(pep_u->len_s);

  //  write request path
  //
  memcpy(buf_y + cur_w, pep_u->pat_c, pep_u->len_s);
}

/* fine_etch_meow(): serialize signed scry response fragment
*/
static void
_fine_etch_meow(u3_meow* mew_u, c3_y* buf_y)
{
  c3_w cur_w = 0;

  //  write signature
  //
  c3_w sig_w = sizeof(mew_u->sig_y);
  memcpy(buf_y + cur_w, mew_u->sig_y, sig_w);
  cur_w += sig_w;

  {
    c3_y num_y[4];
    c3_y len_y = _fine_bytes_word(mew_u->num_w);

    //  write number of fragments
    //
    c3_etch_word(num_y, mew_u->num_w);
    memcpy(buf_y + cur_w, num_y, len_y);

    if (mew_u->siz_s != 0) {
      cur_w += sizeof(mew_u->num_w);
    }
    else {
      cur_w += len_y;
    }
  }

  //  write response fragment data
  //
  memcpy(buf_y + cur_w, mew_u->dat_y, mew_u->siz_s);
}

/* _fine_etch_purr(): serialise response packet
 */
static void
_fine_etch_purr(u3_purr* pur_u, c3_y* buf_y)
{
  c3_w cur_w = 0;

  //  write unsigned scry request
  //
  _fine_etch_peep(&pur_u->pep_u, buf_y + cur_w);
  cur_w += _fine_peep_size(&pur_u->pep_u);

  //  write signed response fragment
  _fine_etch_meow(&pur_u->mew_u, buf_y + cur_w);
}

/* _fine_etch_response(): serialize scry response packet
*/
static void
_fine_etch_response(u3_pact* pac_u)
{
  c3_w pre_w, pur_w, cur_w, rog_w;

  pre_w = _ames_prel_size(&pac_u->hed_u);
  pur_w = _fine_purr_size(&pac_u->pur_u);
  pac_u->hun_u = _ames_ref_hun_new(HEAD_SIZE + pre_w + pur_w);

  //  skip the header until we know what the mug should be
  //
  cur_w = HEAD_SIZE;

  //  write prelude
  //
  _ames_etch_prel(&pac_u->hed_u, &pac_u->pre_u, pac_u->hun_u->hun_y + cur_w);
  cur_w += pre_w;

  //  write body
  //
  _fine_etch_purr(&pac_u->pur_u, pac_u->hun_u->hun_y + cur_w);

  //  calculate mug and write header
  //
  rog_w = HEAD_SIZE + _ames_origin_size(&pac_u->hed_u);
  pac_u->hed_u.mug_l = u3r_mug_bytes(pac_u->hun_u->hun_y + rog_w,
                                     pac_u->hun_u->len_w - rog_w);
  _ames_etch_head(&pac_u->hed_u, pac_u->hun_u->hun_y);

  u3_assert( c3y == _ames_check_mug(pac_u) );
}

/* _lane_scry_path(): format scry path for retrieving a lane
*/
static inline u3_noun
_lane_scry_path(u3_noun who)
{
  return u3nq(u3i_string("peers"),
              u3dc("scot", 'p', who),
              u3i_string("forward-lane"),
              u3_nul);
}

/* _ames_send_cb(): send callback.
*/
static void
_ames_send_cb(uv_udp_send_t* req_u, c3_i sas_i)
{

  u3_send_handle* snd_u = (u3_send_handle*)req_u;

  if ( !sas_i ) {
    net_o = c3y;
  }
  else if ( c3y == net_o ) {
    u3l_log("ames: send fail: %s", uv_strerror(sas_i));
    net_o = c3n;
  }

  _ames_ref_hun_lose(snd_u->hun_u);
  c3_free(req_u);
}

u3_peer*
_mesa_get_peer(u3_mesa_auto* sam_u, u3_ship her_u);

u3_peer*
_mesa_gut_peer(u3_mesa_auto* sam_u, u3_ship her_u);

c3_o
_mesa_is_lane_zero(sockaddr_in lan_u);

/* _ames_send(): send buffer to address on port.
*/
static void
_ames_send(u3_ames* sam_u, sockaddr_in lan_u, u3_ref_hun* hun_u)
{
  if ( c3n == sam_u->mes_u->car_u.liv_o ) {
    u3l_log("ames: not yet live, dropping outbound\r");
    if ( NULL != hun_u)
      _ames_ref_hun_lose(hun_u);
    return;
  }

  assert( (NULL == hun_u) || (0 != hun_u->ref_w) );
  if ( !hun_u
    || !hun_u->len_w
    || c3y == _mesa_is_lane_zero(lan_u) )
  {
    u3l_log("ames: _ames_send null");
    if ( NULL != hun_u)
      _ames_ref_hun_lose(hun_u);
  }
  else {
    u3_send_handle* snd_u = c3_calloc(sizeof(*snd_u));
    snd_u->hun_u = hun_u;
    uv_buf_t buf_u = uv_buf_init((c3_c*)hun_u->hun_y, hun_u->len_w);
    c3_w nip_w = lan_u.sin_addr.s_addr;
    c3_c nip_c[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &nip_w, nip_c, INET_ADDRSTRLEN);
      u3l_log("send ip .%s port %u",
          nip_c, ntohs(lan_u.sin_port));

    c3_i     sas_i = uv_udp_send(&snd_u->snd_u,
                                 sam_u->wax_u,
                                 &buf_u, 1,
                                 (const struct sockaddr*)&lan_u,
                                 _ames_send_cb);

    if ( sas_i ) {
      _ames_send_cb(&snd_u->snd_u, sas_i);
    }
  }
}

sockaddr_in
u3_ames_chub_to_lane(c3_d lan_d) {
  sockaddr_in lan_u;
  lan_u.sin_family = AF_INET;
  lan_u.sin_addr.s_addr = htonl((c3_w)lan_d);
  lan_u.sin_port = htons((c3_s)(lan_d >> 32));
  return lan_u;
}

/* u3_ames_decode_lane(): deserialize noun to lane; 0.0.0.0:0 if invalid
*/
sockaddr_in
u3_ames_decode_lane(u3_atom lan) {
  c3_d lan_d;

  if ( c3n == u3r_safe_chub(lan, &lan_d) || (lan_d >> 48) != 0 ) {
    return (sockaddr_in){0};
  }

  u3z(lan);

  return u3_ames_chub_to_lane(lan_d);
}

/* u3_ames_lane_to_chub(): serialize lane to double-word
*/
c3_d
u3_ames_lane_to_chub(sockaddr_in lan) {
  return ((c3_d)ntohs(lan.sin_port) << 32)
    ^ (c3_d)ntohl(lan.sin_addr.s_addr);
}

/* u3_ames_encode_lane(): serialize lane to noun
*/
u3_atom
u3_ames_encode_lane(sockaddr_in lan) {
  // [%| p=@]
  // [%& p=@pC]
  return u3i_chub(u3_ames_lane_to_chub(lan));
}

/* _ames_lane_into_cache(): put las for who into cache, including timestamp
 * TODO: remove? use callbacks
*/
u3_peer*
_ames_lane_into_cache(u3_ames* sam_u, u3_noun who, u3_noun las)
{
  u3_ship who_u = u3_ship_of_noun(who);
  u3_peer* per_u = _mesa_gut_peer(sam_u->mes_u, who_u);
  if ( c3y == per_u->lam_o ) return per_u;

  // XX the format of the lane %nail gives is (list (each @p address))

  if ( las == u3_nul ) {
    per_u->dan_u = (sockaddr_in){0};
  }
  else {
    u3_noun lan = u3h(las);
    // we either have a direct route, and a galaxy, or just one lane
    if ( c3n == u3h(lan) ) {
      per_u->dan_u = u3_ames_decode_lane(u3k(u3t(lan)));
      if ( c3y == u3du(u3t(las)) ) {
        u3_noun lan = u3h(u3t(las));
        if ( c3n == u3h(lan) )
          u3m_p("strange second lane", lan);
        else
          per_u->lam_u = u3_ship_of_noun(u3t(lan));
      }
    } else {
      u3_ship who_u = u3_ship_of_noun(u3t(lan));
      if ( c3n == u3_ships_equal(sam_u->pir_u->who_u, who_u) ) {
        // delete direct lane if galaxy
        per_u->dan_u = (sockaddr_in){0};
      } else {
        per_u->lam_o = c3y;
        per_u->dan_u = u3_ames_decode_lane(u3k(u3t(lan)));
      }
      per_u->lam_u = who_u;
    }
  }
  u3z(who); u3z(las);
  return per_u;
}

c3_d
_get_now_micros();

c3_o
_ames_is_direct_mode(u3_peer* per_u)
{
  c3_d now_d = _get_now_micros();
  return __( (c3y == per_u->lam_o) ||
             (per_u->dir_u.her_d + DIRECT_ROUTE_TIMEOUT_MICROS > now_d) );
}

static c3_o
_ames_lane_from_peer(u3_ames* sam_u,
                     u3_peer* per_u,
                     sockaddr_in lan_u[2]) {
  if ( NULL == per_u ) return c3n;
  if ( u3_peer_full != per_u->liv_e ) return c3n;
  if ( c3y == u3_ships_equal(per_u->her_u, sam_u->pir_u->who_u) )
    return c3y;
  if ( c3y == _mesa_is_lane_zero(per_u->dan_u) ) {
    if ( c3y == u3_ships_equal(per_u->lam_u, sam_u->pir_u->who_u) )
      return c3y;
    u3_peer* lam_u = _mesa_get_peer(sam_u->mes_u, per_u->lam_u);
    if ( (NULL == lam_u) ||
         (c3y == _mesa_is_lane_zero(lam_u->dan_u)) )
      return c3y;
    lan_u[0] = lam_u->dan_u;
    return c3y;
  } else if ( c3y == _ames_is_direct_mode(per_u) ) {
    lan_u[0] = per_u->dan_u;
    return c3y;
  } else {
    lan_u[0] = per_u->dan_u;
    if ( c3y == u3_ships_equal(per_u->lam_u, sam_u->pir_u->who_u) )
      return c3y;
    u3_peer* lam_u = _mesa_get_peer(sam_u->mes_u, per_u->lam_u);
    if ( (NULL == lam_u) ||
         (c3y == _mesa_is_lane_zero(lam_u->dan_u)) )
      return c3y;
    lan_u[1] = lam_u->dan_u;
    return c3y;
  }
}

/* _ames_lane_from_cache(): retrieve lane for who from cache, if any
*/
static c3_o
_ames_lane_from_cache(u3_ames* sam_u,
                      u3_noun who,
                      sockaddr_in lan_u[2]) {
  memset(lan_u, 0, sizeof(sockaddr_in) * 2);
  u3_ship who_u = u3_ship_of_noun(who);
  u3_peer* per_u = _mesa_get_peer(sam_u->mes_u, who_u);
  return _ames_lane_from_peer(sam_u, per_u, lan_u);
}

static u3_noun
_ames_pact_to_noun(u3_pact* pac_u)
{
  u3_noun pac = u3i_bytes(pac_u->hun_u->len_w, pac_u->hun_u->hun_y);
  return pac;
}

/* _fine_get_cache(): get packet list or status from cache. RETAIN
 */
static u3_weak
_fine_get_cache(u3_ames* sam_u, u3_noun pax, c3_w fra_w)
{
  u3_noun key = u3nc(u3k(pax), u3i_word(fra_w));
  u3_weak pro = u3h_git(sam_u->fin_s.sac_p, key);
  u3z(key);
  return pro;
}

/* _fine_put_cache(): put packet list or status into cache. RETAIN.
 */
static void
_fine_put_cache(u3_ames* sam_u, u3_noun pax, c3_w lop_w, u3_noun lis)
{
  if ( (FINE_PEND == lis) || (FINE_DEAD == lis) ) {
    u3_noun key = u3nc(u3k(pax), u3i_word(lop_w));
    u3h_put(sam_u->fin_s.sac_p, key, lis);
    u3z(key);
  }
  else {
    while ( u3_nul != lis ) {
      u3_noun key = u3nc(u3k(pax), u3i_word(lop_w));
      u3h_put(sam_u->fin_s.sac_p, key, u3k(u3h(lis)));
      u3z(key);

      lis = u3t(lis);
      lop_w++;
    }
  }
}

/* _ames_is_czar(): [who] is galaxy.
*/
static c3_o
_ames_is_czar(u3_noun who)
{
  u3_noun rac = u3do("clan:title", u3k(who));
  c3_o zar = ( c3y == (c3__czar == rac) );
  u3z(rac);
  return zar;
}

c3_o
_ames_lamp_lane(u3_mesa_auto* mes_u, u3_ship her_u, sockaddr_in* lan_u);

static void
_saxo_cb(void* vod_p, u3_noun nun)
{
  u3_pact* pac_u = vod_p;
  u3_ames* sam_u = pac_u->sam_u;

  u3_weak sax    = u3r_at(7, nun);

  u3_peer* per_u = NULL;
  if ( sax != u3_none ) {
    per_u = _mesa_gut_peer(sam_u->mes_u, pac_u->pre_u.rec_u);
    u3_noun her = u3h(sax);
    u3_ship her_u = u3_ship_of_noun(her);
    u3_noun lam = u3do("rear", u3k(sax));
    //u3_assert( c3y == u3a_is_cat(gal) && gal < 256 );
    // both atoms guaranteed to be cats, bc we don't call unless forwarding
    per_u->lam_u = u3_ship_of_noun(lam);
    per_u->liv_e |= u3_peer_lamp;
    u3z(lam);
  }

  u3z(nun);
}

sockaddr_in
_mesa_realise_lane(u3_mesa_auto* mes_u, u3_noun lan);

static void
_ames_lane_scry_cb(u3_pact* pac_u, u3_peer* per_u);

static void
_forward_lanes_cb(void* vod_p, u3_noun nun)
{
  u3_pact* pac_u = vod_p;
  u3_ames* sam_u = pac_u->sam_u;

  u3_weak las    = u3r_at(7, nun);
  // u3m_p("_forward_lanes_cb", las);

  u3_peer* per_u = NULL;
  if ( las != u3_none ) {
    per_u = _mesa_gut_peer(sam_u->mes_u, pac_u->pre_u.rec_u);
    u3_noun gal = u3h(las);
    //u3_assert( c3y == u3a_is_cat(gal) && gal < 256 );
    // both atoms guaranteed to be cats, bc we don't call unless forwarding
    per_u->liv_e |= u3_peer_lane;
    per_u->lam_u = u3_ship_of_noun(gal);
    u3_noun sal, tal;

    if ( c3n == per_u->lam_o ) {
      if ( (c3y == u3r_cell(u3t(las), &sal, &tal)) &&
           (c3y == u3du(sal)) ) {
        per_u->dan_u = _mesa_realise_lane(sam_u->mes_u, u3k(sal));
      } else {
        per_u->dan_u = (sockaddr_in){0};
      }
    }
  }
  _ames_lane_scry_cb(pac_u, per_u);

  u3z(nun);

}

static void
_meet_peer(u3_ames* sam_u, u3_pact* pac_u)
{
  u3_peer* per_u = _mesa_get_peer(sam_u->mes_u, pac_u->pre_u.rec_u);
  u3_noun her = u3_ship_to_noun(pac_u->pre_u.rec_u);
  u3_noun gan = u3nc(u3_nul, u3_nul);

  if ( (NULL == per_u) || !(u3_peer_lamp & per_u->liv_e) ) {
    u3_noun pax = u3nc(u3dc("scot", c3__p, u3k(her)), u3_nul);
    u3_pier_peek_last(sam_u->mes_u->pir_u, u3k(gan), c3__j, c3__saxo, pax, pac_u, _saxo_cb);
  }

  if ( (NULL == per_u) || !(u3_peer_lane & per_u->liv_e) ) {
    u3_noun pax = u3nq(u3i_string("chums"),
                    u3dc("scot", 'p', u3k(her)),
                    u3i_string("lanes"),
                    u3_nul);
    u3_pier_peek_last(sam_u->mes_u->pir_u, u3k(gan), c3__ax, u3_nul, pax, pac_u, _forward_lanes_cb);
  }
  u3z(her); u3z(gan);
}

/* _ames_send_lane(): resolve/decode lane. RETAIN
*/
static c3_o
_ames_send_lane(u3_ames* sam_u, u3_noun lan, sockaddr_in* lan_u)
{

  u3_noun tag, val;

  if ( c3n == u3r_cell(lan, &tag, &val) ) {
    u3l_log("ames: bad lane; not a cell");
    return c3n;
  }

  switch ( tag ) {
    case c3y: {  //  galaxy
      return _ames_lamp_lane(sam_u->mes_u, u3_ship_of_noun(val), lan_u);
    }

    case c3n: {  //  ip:port
      sockaddr_in nal_u = u3_ames_decode_lane(u3k(val));

      //  convert incoming localhost to outgoing localhost
      //
      //    XX this looks like en/de-coding problems ...
      //
      if (!nal_u.sin_addr.s_addr)
        nal_u.sin_addr.s_addr = NLOCALHOST;

      //  if in local-only mode, don't send remote packets
      //
      if ( (c3n == u3_Host.ops_u.net) && (NLOCALHOST != nal_u.sin_addr.s_addr) ) {
        return c3n;
      }
      //  if the lane is uninterpretable, silently drop the packet
      //
      else if ( !nal_u.sin_port ) {
        if ( u3C.wag_w & u3o_verbose ) {
          u3l_log("ames: inscrutable lane");
        }
        return c3n;
      }

      *lan_u = nal_u;
      return c3y;
    }

    default: {
      u3l_log("ames: bad lane tag");
      return c3n;
    }
  }
}

/* _ames_ef_send(): send packet to network (v4).
*/
static void
_ames_ef_send(u3_ames* sam_u, sockaddr_in lan_u, u3_noun pac)
{
  if ( c3n == sam_u->mes_u->car_u.liv_o ) {
    u3l_log("ames: not yet live, dropping outbound\r");
    u3z(pac);
    return;
  }

  u3_ref_hun* hun_u = _ames_ref_hun_new(u3r_met(3, pac));
  u3r_bytes(0, hun_u->len_w, hun_u->hun_y, pac);

  _ames_send(sam_u, lan_u, hun_u);

  u3z(pac);
}

/* _ames_cap_queue(): cap ovum queue at QUEUE_MAX, dropping oldest packets.
*/
static void
_ames_cap_queue(u3_ames* sam_u)
{
  u3_ovum* egg_u = sam_u->mes_u->car_u.ext_u;
  c3_d     old_d = sam_u->sat_u.dop_d;

  while ( egg_u && (QUEUE_MAX < sam_u->mes_u->car_u.dep_w) ) {
    u3_ovum* nex_u = egg_u->nex_u;

    if ( c3__hear == u3h(egg_u->cad) ) {
      u3_auto_drop(&sam_u->mes_u->car_u, egg_u);
      sam_u->sat_u.dop_d++;

      if ( u3C.wag_w & u3o_verbose ) {
        u3l_log("ames: packet dropped (%" PRIu64 " total)", sam_u->sat_u.dop_d);
      }
    }

    egg_u = nex_u;
  }

  if (  !(u3C.wag_w & u3o_verbose)
     && (old_d != sam_u->sat_u.dop_d)
     && !(sam_u->sat_u.dop_d % 1000) )
  {
    u3l_log("ames: packet dropped (%" PRIu64 " total)", sam_u->sat_u.dop_d);
  }
}

static void
_ames_hear_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3_pact* pac_u = egg_u->ptr_v;
  u3_ames* sam_u = pac_u->sam_u;
  if ( u3_ovum_exit == new_e ) {
    _ames_pact_free(pac_u);
  }
  else if ( u3_ovum_done == new_e ) {
    if ( c3n == pac_u->for_o) {
      c3_o dir_o = (pac_u->pre_u.rog_d == 0) ? c3y : c3n;
      u3_peer* per_u = _mesa_gut_peer(sam_u->mes_u, pac_u->pre_u.sen_u);
      if (c3n == dir_o) {
        if ( c3n == per_u->lam_o )
          per_u->dan_u = u3_ames_chub_to_lane(pac_u->pre_u.rog_d);
        per_u->ind_u.her_d = pac_u->her_d;
      } else {
        per_u->dir_u.her_d = pac_u->her_d;
      }
    }
    _ames_pact_free(pac_u);
  }
}

/* _ames_hear_bail(): handle packet failure.
*/
static void
_ames_hear_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3_pact* pac_u = egg_u->ptr_v;
  u3_ames* sam_u = pac_u->sam_u;
  c3_w     len_w = u3qb_lent(lud);

  if ( (1 == len_w) && c3__evil == u3h(u3h(lud)) ) {
    sam_u->sat_u.vil_d++;

    if (  (u3C.wag_w & u3o_verbose)
       || (0 == (sam_u->sat_u.vil_d % 100)) )
    {
      u3l_log("ames: heard bad crypto (%" PRIu64 " total), "
              "check azimuth state\r\n",
              sam_u->sat_u.vil_d);
    }
  }
  else {
    sam_u->sat_u.fal_d++;

    if (  (u3C.wag_w & u3o_verbose)
       || (0 == (sam_u->sat_u.fal_d % 100)) )
    {
      if ( 2 == len_w ) {
        u3_pier_punt_goof("hear", u3k(u3h(lud)));
        u3_pier_punt_goof("crud", u3k(u3h(u3t(lud))));
      }
      //  !2 traces is unusual, just print the first if present
      //
      else if ( len_w ) {
        u3_pier_punt_goof("hear", u3k(u3h(lud)));
      }

      u3l_log("ames: packet failed (%" PRIu64 " total)\r\n",
              sam_u->sat_u.fal_d);
    }
  }

  _ames_pact_free(pac_u);
  u3z(lud);
  u3_ovum_free(egg_u);
}

/* _ames_put_packet(): add packet to queue, drop old packets on pressure
*/
static void
_ames_put_packet(u3_ames* sam_u,
                 u3_pact* pac_u,
                 sockaddr_in  lan_u)
{
  u3_noun wir = u3nc(c3__ames, u3_nul);
  u3_noun cad = u3nt(c3__hear, u3nc(c3n, u3_ames_encode_lane(lan_u)),
                     _ames_pact_to_noun(pac_u));

  u3_auto_peer(
    u3_auto_plan(&sam_u->mes_u->car_u,
                 u3_ovum_init(0, c3__a, wir, cad)),
    pac_u, _ames_hear_news, _ames_hear_bail);

  _ames_cap_queue(sam_u);
}

/* _ames_send_many(): send pac_u on the (list lane) las; retains pac_u
*/
static void
_ames_send_many(u3_pact* pac_u, sockaddr_in lan_u[2], c3_o for_o)
{
  u3_ames* sam_u = pac_u->sam_u;
  //  if forwarding, track metrics
  //
  if ( c3y == for_o ) {
    u3_ames* sam_u = pac_u->sam_u;

    sam_u->sat_u.fow_d++;
    if ( 0 == (sam_u->sat_u.fow_d % 1000000) ) {
      u3l_log("ames: forwarded %" PRIu64 " total", sam_u->sat_u.fow_d);
    }

    if ( u3C.wag_w & u3o_verbose ) {
      u3_noun sen = u3dc("scot", 'p', u3_ship_to_noun(pac_u->pre_u.sen_u));
      u3_noun rec = u3dc("scot", 'p', u3_ship_to_noun(pac_u->pre_u.rec_u));
      c3_c* sen_c = u3r_string(sen);
      c3_c* rec_c = u3r_string(rec);

      c3_w nip_w = pac_u->lan_u.sin_addr.s_addr;
      c3_c nip_c[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &nip_w, nip_c, INET_ADDRSTRLEN);


      //NOTE ip byte order assumes little-endian
      u3l_log("ames: forwarding for %s to %s from %s:%d",
              sen_c, rec_c, nip_c,
              ntohs(pac_u->lan_u.sin_port));

      c3_free(sen_c); c3_free(rec_c);
      u3z(sen); u3z(rec);
    }
  }

  for ( c3_w i = 0;
        ( (i < 2) && _mesa_is_lane_zero(lan_u[i]) );
        i++) {
    _ames_send(sam_u, lan_u[i], _ames_ref_hun_gain(pac_u->hun_u));
  }
}


/*  _ames_lane_scry_cb(): learn lanes to send packet on
*/
static void
_ames_lane_scry_cb(u3_pact* pac_u, u3_peer* per_u)
{
  u3_ames* sam_u = pac_u->sam_u;

  if ( c3y == pac_u->for_o )
    sam_u->sat_u.foq_d--;

  //  if scry fails, remember we can't scry, and just inject the packet
  //
  if ( (NULL == per_u) || (u3_peer_full != per_u->liv_e) ) {
    if ( 5 < ++sam_u->sat_u.saw_d ) {
      u3l_log("ames: giving up scry");
      sam_u->fig_u.see_o = c3n;
    }
    _ames_put_packet(sam_u,
                     pac_u,
                     pac_u->lan_u);
  }
  else {
    sam_u->sat_u.saw_d = 0;
    sockaddr_in lan_u[2];
    _ames_lane_from_peer(sam_u, per_u, lan_u);
    
    //  if there are lanes, send the packet on them; otherwise drop it
    //
    if ( c3n == _mesa_is_lane_zero(lan_u[0]) ) {
      _ames_send_many(pac_u, lan_u, pac_u->for_o);
    }
    _ames_pact_free(pac_u);
  }
}

/* _ames_try_send(): try to send a packet to a ship and its sponsors
*/
static void
_ames_try_send(u3_pact* pac_u)
{
  u3_ames* sam_u = pac_u->sam_u;
  if ( c3n == sam_u->mes_u->car_u.liv_o ) {
    u3l_log("ames: not yet live, dropping outbound\r");
    _ames_pact_free(pac_u);
    return;
  }

  u3_noun key = u3_ship_to_noun(pac_u->pre_u.rec_u);
  sockaddr_in lan_u[2];
  c3_o lan_o = _ames_lane_from_cache(sam_u, key, lan_u);

  //  if we know there's no lane, drop the packet
  //
  if ( (c3y == lan_o) && ( c3y == _mesa_is_lane_zero(lan_u[0]) ) ) {
    _ames_pact_free(pac_u);
    return;
  }
  //  if we don't know the lane, and the lane scry queue is full,
  //  just drop the packet
  //TODO cap queue for lane lookups for scry responses, not just forwards
  //
  //TODO  drop oldest item in forward queue in favor of this one.
  //      ames.c doesn't/shouldn't know about the shape of scry events,
  //      so can't pluck these out of the event queue like it does in
  //      _ames_cap_queue. as such, blocked on u3_lord_peek_cancel or w/e.
  //
  if ( (c3y == pac_u->for_o)
       && (c3n == lan_o)
       && (1000 < sam_u->sat_u.foq_d) )
  {
    sam_u->sat_u.fod_d++;
    if ( 0 == (sam_u->sat_u.fod_d % 10000) ) {
      u3l_log("ames: dropped %" PRIu64 " forwards total",
              sam_u->sat_u.fod_d);
    }

    _ames_pact_free(pac_u);
    return;
  }

  //  if we already know the lane, just send
  //
  if ( c3y == lan_o ) {
    _ames_send_many(pac_u, lan_u, pac_u->for_o);
    _ames_pact_free(pac_u);
  }
  //  store the packet to be sent later when the lane scry completes
  //
  else {
    if ( c3y == pac_u->for_o ) {
      sam_u->sat_u.foq_d++;
    }
    _meet_peer(sam_u, pac_u);
  }
}

#undef AMES_SKIP
#ifdef AMES_SKIP
/* _ames_skip(): decide whether to skip this packet, for rescue
*/
static c3_o
_ames_skip(u3_prel* pre_u)
{
  if ( pre_u->sen_u[1] == 0 &&
       ( pre_u->sen_u[0] == 0x743a17a6
         || pre_u->sen_u[0] == 0xea99acb6
         || pre_u->sen_u[0] == 0x10100
     ) ) {
    return c3n;
  }
  else {
    return c3y;
  }
}
#endif

/* _fine_lop(): find beginning of page containing fra_w
*/
static inline c3_w
_fine_lop(c3_w fra_w)
{
  return 1 + (((fra_w - 1) / FINE_PAGE) * FINE_PAGE);
}

/* _fine_scry_path(): parse path from wail or purr.
*/
static u3_weak
_fine_scry_path(u3_pact* pac_u)
{
  u3_peep* pep_u = ( PACT_WAIL == pac_u->typ_y )
                   ? &pac_u->wal_u.pep_u
                   : &pac_u->pur_u.pep_u;
  u3_noun  ful = u3dc("rush", u3i_string(pep_u->pat_c), u3v_wish(PATH_PARSER));

  if ( u3_nul == ful ) {
    return u3_none;
  }
  else {
    u3_noun pro = u3k(u3t(ful));
    u3z(ful);
    return pro;
  }
}

/* _fine_hunk_scry_cb(): receive packets for datum out of fine
 */
static void
_fine_hunk_scry_cb(void* vod_p, u3_noun nun)
{
  u3_pact* pac_u = vod_p;
  u3_ames* sam_u = pac_u->sam_u;
  u3_peep* pep_u = &pac_u->pur_u.pep_u;
  u3_weak    fra = u3_none;

  u3_assert( PACT_PURR == pac_u->typ_y );

  {
    //  XX virtualize
    u3_noun pax = u3dc("rash", u3i_string(pep_u->pat_c), u3v_wish(PATH_PARSER));
    c3_w  lop_w = _fine_lop(pep_u->fra_w);
    u3_weak pas = u3r_at(7, nun);

    //  if not [~ ~ fragments], mark as dead
    //
    if( u3_none == pas ) {
      _fine_put_cache(sam_u, pax, lop_w, FINE_DEAD);
      _ames_pact_free(pac_u);

      u3z(nun);
      return;
    }

    _fine_put_cache(sam_u, pax, lop_w, pas);

    //  find requested fragment
    //
    while ( u3_nul != pas ) {
      if ( pep_u->fra_w == lop_w ) {
        fra = u3k(u3h(pas));
        break;
      }
      lop_w++;
      pas = u3t(pas);
    }

    u3z(pax);
  }

  if ( fra == u3_none ) {
    u3l_log("fine: fragment number out of range");
    _ames_pact_free(pac_u);
  }
  else if ( c3y == _fine_sift_meow(&pac_u->pur_u.mew_u, fra) ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("fine: send %u %s", pac_u->pur_u.pep_u.fra_w,
                                  pac_u->pur_u.pep_u.pat_c);
    }

    _fine_etch_response(pac_u);
    _ames_try_send(pac_u);
  }
  else {
    u3l_log("fine: bad meow");
    _ames_pact_free(pac_u);
  }

  u3z(nun);
}

/* _fine_hear_request(): hear wail (fine request packet packet).
*/
static void
_fine_hear_request(u3_pact* req_u, c3_w cur_w)
{
  u3_ames* sam_u = req_u->sam_u;
  u3_pact* res_u;
  u3_noun    key;

  if ( c3n == _fine_sift_wail(req_u, cur_w) ) {
    sam_u->sat_u.wal_d++;
    if ( 0 == (sam_u->sat_u.wal_d % 100) ) {
      u3l_log("fine: %" PRIu64 " dropped wails",
              sam_u->sat_u.wal_d);
    }
    _ames_pact_free(req_u);
    return;
  }
  //  make scry cache key
  //
  else {
    u3_weak yek = _fine_scry_path(req_u);

    if ( u3_none == yek  ) {
      sam_u->sat_u.wap_d++;
      if ( 0 == (sam_u->sat_u.wap_d % 100) ) {
        u3l_log("fine: %" PRIu64 " dropped wails (path)",
                sam_u->sat_u.wap_d);
      }
      _ames_pact_free(req_u);
      return;
    }

    key = yek;
  }

  //  fill in the parts of res_u that we know from req_u
  {
    res_u = c3_calloc(sizeof(*res_u));
    res_u->sam_u = req_u->sam_u;
    res_u->typ_y = PACT_PURR;
    res_u->lan_u = req_u->lan_u;

    //  copy header, swapping sender and receiver
    //
    res_u->hed_u = (u3_head) {
      .req_o = c3n,
      .sim_o = c3n,
      .ver_y = req_u->hed_u.ver_y,
      .sac_y = req_u->hed_u.rac_y,
      .rac_y = req_u->hed_u.sac_y,
      .mug_l = 0,  //  filled in later
      .rel_o = c3n
    };

    //  copy prelude, swapping sender and receiver
    //
    res_u->pre_u = (u3_prel) {
      .sic_y = req_u->pre_u.ric_y,
      .ric_y = req_u->pre_u.sic_y,
      .sen_u = req_u->pre_u.rec_u,
      .rec_u = req_u->pre_u.sen_u,
      .rog_d = 0
    };

    //  copy unsigned request payload into response body
    //
    res_u->pur_u = (u3_purr) {
      .pep_u = req_u->wal_u.pep_u,
      .mew_u = {0}  //  filled in later
    };

    //  copy path string from request into response body
    {
      c3_s len_s = req_u->wal_u.pep_u.len_s;
      res_u->pur_u.pep_u.pat_c = c3_calloc(len_s + 1);
      memcpy(res_u->pur_u.pep_u.pat_c, req_u->wal_u.pep_u.pat_c, len_s);
    }

    //  free incoming request
    //
    _ames_pact_free(req_u);
  }

  //  look up request in scry cache
  //
  c3_w  fra_w = res_u->pur_u.pep_u.fra_w;
  c3_w  lop_w = _fine_lop(fra_w);
  u3_weak pec = _fine_get_cache(sam_u, key, lop_w);

  //  already pending; drop
  //
  if ( FINE_PEND == pec ) {
    if ( u3C.wag_w & u3o_verbose ) {
      u3l_log("fine: pend %u %s", res_u->pur_u.pep_u.fra_w,
                                  res_u->pur_u.pep_u.pat_c);
    }
    _ames_pact_free(res_u);
  }
  //  cache miss or a previous scry blocked; try again
  //
  else {
    u3_weak cac = _fine_get_cache(sam_u, key, fra_w);

    if ( (u3_none == cac) || (FINE_DEAD == cac) ) {
      if ( u3C.wag_w & u3o_verbose ) {
        u3l_log("fine: miss %u %s", res_u->pur_u.pep_u.fra_w,
                                    res_u->pur_u.pep_u.pat_c);
      }

      u3_noun pax =
        u3nc(c3__fine,
        u3nq(c3__hunk,
             u3dc("scot", c3__ud, u3i_word(lop_w)),
             u3dc("scot", c3__ud, FINE_PAGE),
             u3k(key)));

      //  mark as pending in the scry cache
      //
      _fine_put_cache(res_u->sam_u, key, lop_w, FINE_PEND);

      //  scry into arvo for a page of packets
      //
      u3_pier_peek_last(res_u->sam_u->pir_u, u3_nul, c3__ax, u3_nul,
                        pax, res_u, _fine_hunk_scry_cb);
    }
    //  cache hit, fill in response meow and send
    //
    else if ( c3y == _fine_sift_meow(&res_u->pur_u.mew_u, u3k(cac)) ) {
      if ( u3C.wag_w & u3o_verbose ) {
        u3l_log("fine: hit  %u %s", res_u->pur_u.pep_u.fra_w,
                                    res_u->pur_u.pep_u.pat_c);
      }
      _fine_etch_response(res_u);
      _ames_try_send(res_u);
    }
    else {
      u3l_log("fine: _fine_hear_request meow bad");
      _ames_pact_free(res_u);
    }
  }

  u3z(key);
}

/* _fine_hear_response(): hear purr (fine response packet).
*/
static void
_fine_hear_response(u3_pact* pac_u, c3_w cur_w)
{
  u3_noun wir = u3nc(c3__fine, u3_nul);
  u3_noun cad = u3nt(c3__hear,
                     u3nc(c3n, u3_ames_encode_lane(pac_u->lan_u)),
                     u3i_bytes(pac_u->hun_u->len_w, pac_u->hun_u->hun_y));

  u3_ovum* ovo_u = u3_ovum_init(0, c3__ames, wir, cad);
  u3_auto_peer(
    u3_auto_plan(&pac_u->sam_u->mes_u->car_u, ovo_u),
    pac_u, _ames_hear_news, _ames_hear_bail);

  _ames_cap_queue(pac_u->sam_u);
  _ames_pact_free(pac_u);
}

/* _ames_hear_ames(): hear ames packet.
*/
static void
_ames_hear_ames(u3_pact* pac_u, c3_w cur_w)
{

#ifdef AMES_SKIP
  if ( c3_y == _ames_skip(&pac_u->pre_u) ) {
    _ames_pact_free(pac_u);
    return;
  }
#endif

  _ames_put_packet(pac_u->sam_u, pac_u, pac_u->lan_u);
}

/* _ames_try_forward(): forward packet, updating lane if needed.
*/
static void
_ames_try_forward(u3_pact* pac_u)
{
  //  insert origin lane if needed
  //
  if ( c3n == pac_u->hed_u.rel_o )
       //&& ( c3__czar == u3_ship_rank(pac_u->pre_u.sen_u) ) )
  {
    c3_w cur_w;
    u3_ref_hun* old_u;

    pac_u->hed_u.rel_o = c3y;
    pac_u->pre_u.rog_d = u3_ames_lane_to_chub(pac_u->lan_u);

    old_u = pac_u->hun_u;

    pac_u->hun_u = _ames_ref_hun_new(pac_u->hun_u->len_w + 6);

    cur_w = 0;

    _ames_etch_head(&pac_u->hed_u, pac_u->hun_u->hun_y);
    cur_w += HEAD_SIZE;

    _ames_etch_origin(pac_u->pre_u.rog_d, pac_u->hun_u->hun_y + cur_w);
    cur_w += 6;

    memcpy(pac_u->hun_u->hun_y + cur_w,
           old_u->hun_y + HEAD_SIZE,
           old_u->len_w - HEAD_SIZE);

    _ames_ref_hun_lose(old_u);
  }

  _ames_try_send(pac_u);
}

/* _ames_hear(): parse a (potential) packet, dispatch appropriately.
**
**    packet filtering needs to revised for two protocol-change scenarios
**
**    - packets using old protocol versions from our sponsees
**      these must be let through, and this is a transitive condition;
**      they must also be forwarded where appropriate
**      they can be validated, as we know their semantics
**
**    - packets using newer protocol versions
**      these should probably be let through, or at least
**      trigger printfs suggesting upgrade.
**      they cannot be filtered, as we do not know their semantics
*/
void
_ames_hear(u3_ames* sam_u,
           const struct sockaddr_in* lan_u,
           c3_w     len_w,
           c3_y*    hun_y)
{
  u3_pact* pac_u;
  c3_w     pre_w;
  c3_w     cur_w = 0;  //  cursor: how many bytes we've read from hun_y

  //  make sure packet is big enough to have a header
  //
  if ( HEAD_SIZE > len_w ) {
    sam_u->sat_u.hed_d++;
    if ( 0 == (sam_u->sat_u.hed_d % 100000) ) {
      u3l_log("ames: %" PRIu64 " dropped, failed to read header",
              sam_u->sat_u.hed_d);
    }

    c3_free(hun_y);
    return;
  }

  pac_u = c3_calloc(sizeof(*pac_u));
  pac_u->sam_u = sam_u;
  pac_u->hun_u = _ames_ref_hun_new(len_w);
  memcpy(pac_u->hun_u->hun_y, hun_y, len_w);

  pac_u->lan_u = *lan_u;
  cur_w = 0;

  //  parse the header
  //
  _ames_sift_head(&pac_u->hed_u, pac_u->hun_u->hun_y);
  cur_w += HEAD_SIZE;

  pac_u->typ_y = _ames_pact_typ(&pac_u->hed_u);

  //  ensure the protocol version matches ours
  //
  //    XX rethink use of [fit_o] here and elsewhere
  //
  if (  (c3y == sam_u->fig_u.fit_o)
    && (sam_u->ver_y != pac_u->hed_u.ver_y) )
  {
    sam_u->sat_u.vet_d++;
    if ( 0 == (sam_u->sat_u.vet_d % 100000) ) {
      u3l_log("ames: %" PRIu64 " dropped for version mismatch",
              sam_u->sat_u.vet_d);
    }
    _ames_pact_free(pac_u);
    return;
  }

  //  check contents match mug in header
  //
  if ( c3n == _ames_check_mug(pac_u) ) {
    // _log_head(&pac_u->hed_u);
    sam_u->sat_u.mut_d++;
    if ( 0 == (sam_u->sat_u.mut_d % 100000) ) {
      u3l_log("ames: %" PRIu64 " dropped for invalid mug",
              sam_u->sat_u.mut_d);
    }
    _ames_pact_free(pac_u);
    return;
  }

  //  check that packet is big enough for prelude
  //
  pre_w = _ames_prel_size(&pac_u->hed_u);
  if ( len_w < cur_w + pre_w ) {
    sam_u->sat_u.pre_d++;
    if ( 0 == (sam_u->sat_u.pre_d % 100000) ) {
      u3l_log("ames: %" PRIu64 " dropped, failed to read prelude",
              sam_u->sat_u.pre_d);
    }
    _ames_pact_free(pac_u);
    return;
  }

  //  parse prelude
  //
  _ames_sift_prel(&pac_u->hed_u, &pac_u->pre_u, pac_u->hun_u->hun_y + cur_w);
  cur_w += pre_w;

  pac_u->her_d = _get_now_micros();
  pac_u->for_o = (c3y == u3_ships_equal(pac_u->pre_u.rec_u,
                                        sam_u->pir_u->who_u))
                 ? c3n : c3y;

  //  if we can scry for lanes,
  //  and we are not the recipient,
  //  we might want to forward statelessly
  //
  if ( (c3y == sam_u->fig_u.see_o)
    && ( c3y == pac_u->for_o ) )
  {
    if ( c3y == sam_u->sat_u.for_o ) {
      _ames_try_forward(pac_u);
    }
  }
  else {
    //  enter protocol-specific packet handling
    //
    switch ( pac_u->typ_y ) {
      case PACT_WAIL: {
        _fine_hear_request(pac_u, cur_w);
      } break;

      case PACT_PURR: {
        _fine_hear_response(pac_u, cur_w);
      } break;

      case PACT_AMES: {
        _ames_hear_ames(pac_u, cur_w);
      } break;

      default: {
        u3l_log("ames_hear: bad packet type %d", pac_u->typ_y);
        u3_pier_bail(u3_king_stub());
      }
    }
  }
}

/* _ames_prot_scry_cb(): receive ames protocol version
*/
static void
_ames_prot_scry_cb(void* vod_p, u3_noun nun)
{
  u3_ames* sam_u = vod_p;
  u3_weak    ver = u3r_at(7, nun);

  if ( u3_none == ver ) {
    //  assume protocol version 0
    //
    sam_u->ver_y = 0;
    sam_u->fin_s.ver_y = 0;
  }
  else if ( (c3n == u3a_is_cat(ver))
         || (7 < ver) ) {
    u3m_p("ames: strange protocol", nun);
    sam_u->ver_y = 0;
    sam_u->fin_s.ver_y = 0;
  }
  else {
    sam_u->ver_y = ver;
    sam_u->fin_s.ver_y = ver;
  }

  //  XX revise: filtering should probably be disabled if
  //  we get a protocol version above the latest one we know
  //
  sam_u->fig_u.fit_o = c3y;
  u3z(nun);
}

static void
_saxo_self_cb(void* vod_p, u3_noun nun)
{
  u3_ames* sam_u = vod_p;
  u3_weak sax    = u3r_at(7, nun);

  if ( sax != u3_none ) {
    u3_noun lam = u3do("rear", u3k(sax));
    u3_ship lam_u = u3_ship_of_noun(lam);
    if ( c3y == u3_ships_equal(lam_u, sam_u->mes_u->pir_u->who_u) )
      sam_u->sat_u.for_o = c3y;
    u3z(lam);
  }

  u3z(nun);
}

static void
_meet_self(u3_ames* sam_u)
{
  u3_noun her = u3_ship_to_noun(sam_u->mes_u->pir_u->who_u);
  u3_noun gan = u3nc(u3_nul, u3_nul);

  u3_noun pax = u3nc(u3dc("scot", c3__p, her), u3_nul);
  u3_pier_peek_last(sam_u->mes_u->pir_u, gan, c3__j, c3__saxo, pax, sam_u, _saxo_self_cb);
}

/* _ames_io_talk(): start receiving ames traffic.
*/
void
_ames_io_talk(u3_ames* sam_u)
{
  //  send born event
  //
  // {
  //   //  XX remove [sev_l]
  //   //
  //   u3_noun wir = u3nt(c3__newt,
  //                      u3dc("scot", c3__uv, sam_u->sev_l),
  //                      u3_nul);
  //   u3_noun cad = u3nc(c3__born, u3_nul);

  //   u3_auto_plan(car_u, u3_ovum_init(0, c3__a, wir, cad));
  // }

  //  scry the protocol version out of arvo
  //
  //    XX this should be re-triggered periodically,
  //    or, better yet, %ames should emit a %turf
  //    (or some other reconfig) effect when it is reset.
  //
  u3_noun gang = u3nc(u3_nul, u3_nul);
  //  XX  drop this; done at another level
  //
  u3_pier_peek_last(sam_u->pir_u, gang, c3__ax, u3_nul,
                    u3nt(u3i_string("protocol"), u3i_string("version"), u3_nul),
                    sam_u, _ames_prot_scry_cb);
  if ( c3n == sam_u->sat_u.for_o )
    _meet_self(sam_u);
}

/* _ames_kick_newt(): apply packet network outputs.
*/
c3_o
_ames_kick_newt(u3_ames* sam_u, u3_noun tag, u3_noun dat)
{
  c3_o ret_o;

  switch ( tag ) {
    default: {
      ret_o = c3n;
    } break;

    case c3__send: {
      u3_noun pac = u3k(u3t(dat));
      u3_noun lan = u3k(u3h(dat));
      sockaddr_in lan_u;
      if ( c3y == _ames_send_lane(sam_u, lan, &lan_u) ) {
        _ames_ef_send(sam_u, lan_u, pac);
      }
      ret_o = c3y;
    } break;
  }

  u3z(tag); u3z(dat);
  return ret_o;
}

/* _ames_io_kick(): apply effects
*/
c3_o
_ames_io_kick(u3_ames* sam_u, u3_noun wir, u3_noun cad)
{

  u3_noun tag, dat, i_wir;
  c3_o ret_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3n == u3r_cell(cad, &tag, &dat)) )
  {
    ret_o = c3n;
  }
  else {
    switch ( i_wir ) {
      default: {
        ret_o = c3n;
      } break;

      //  XX should also be c3__ames
      //
      case c3__newt: {
        ret_o = _ames_kick_newt(sam_u, u3k(tag), u3k(dat));
      } break;

      //  XX obsolete
      //
      //    used to also handle %west and %woot for tcp proxy setup
      //
      case c3__ames: {
        ret_o = _( c3__init == tag);
      } break;

      //  this can return through dill due to our fscked up boot sequence
      //
      //    XX s/b obsolete, verify
      //
      case c3__term: {
        if ( c3__send != tag ) {
          ret_o = c3n;
        }
        else {
          u3l_log("kick: strange send\r");
          ret_o = _ames_kick_newt(sam_u, u3k(tag), u3k(dat));
        }
      } break;
    }
  }

  u3z(wir); u3z(cad);
  return ret_o;
}

/* _ames_exit_cb(): dispose resources aftr close.
*/
void
_ames_exit_cb(u3_ames* sam_u)
{
  u3s_cue_xeno_done(sam_u->sil_u);
  ur_cue_test_done(sam_u->tes_u);

  c3_free(sam_u);
}

/* _ames_io_info(): produce status info.
*/
u3_noun
_ames_io_info(u3_ames* sam_u)
{
  c3_w sac_w;

  sac_w = u3h_count(sam_u->fin_s.sac_p) * 4;
  u3h_discount(sam_u->fin_s.sac_p);

  return u3i_list(
    u3_pier_mase("filtering",        sam_u->fig_u.fit_o),
    u3_pier_mase("can-send",         net_o),
    u3_pier_mase("can-scry",         sam_u->fig_u.see_o),
    u3_pier_mase("scry-cache",       u3i_word(u3h_wyt(sam_u->fin_s.sac_p))),
    u3_pier_mase("scry-cache-size",  u3i_word(sac_w)),
    u3_pier_mase("dropped",          u3i_chub(sam_u->sat_u.dop_d)),
    u3_pier_mase("forwards-dropped", u3i_chub(sam_u->sat_u.fod_d)),
    u3_pier_mase("forwards-pending", u3i_chub(sam_u->sat_u.foq_d)),
    u3_pier_mase("forwarded",        u3i_chub(sam_u->sat_u.fow_d)),
    u3_pier_mase("filtered-hed",     u3i_chub(sam_u->sat_u.hed_d)),
    u3_pier_mase("filtered-ver",     u3i_chub(sam_u->sat_u.vet_d)),
    u3_pier_mase("filtered-mug",     u3i_chub(sam_u->sat_u.mut_d)),
    u3_pier_mase("filtered-pre",     u3i_chub(sam_u->sat_u.pre_d)),
    u3_pier_mase("filtered-wal",     u3i_chub(sam_u->sat_u.wal_d)),
    u3_pier_mase("filtered-wap",     u3i_chub(sam_u->sat_u.wap_d)),
    u3_pier_mase("crashed",          u3i_chub(sam_u->sat_u.fal_d)),
    u3_pier_mase("evil",             u3i_chub(sam_u->sat_u.vil_d)),
    u3_pier_mase("lane-scry-fails",  u3i_chub(sam_u->sat_u.saw_d)),
    u3_none);
}

/* _ames_io_slog(): print status info.
*/
void
_ames_io_slog(u3_ames* sam_u)
{
  c3_w sac_w;

  sac_w = u3h_count(sam_u->fin_s.sac_p) * 4;
  u3h_discount(sam_u->fin_s.sac_p);

# define FLAG(a) ( (c3y == a) ? "&" : "|" )

  //  TODO  rewrite in terms of info_f
  //
  u3l_log("      config:");
  u3l_log("        filtering: %s", FLAG(sam_u->fig_u.fit_o));
  u3l_log("         can send: %s", FLAG(net_o));
  u3l_log("         can scry: %s", FLAG(sam_u->fig_u.see_o));
  u3l_log("      caches:");
  u3l_log("        cached meows: %u, %u B", u3h_wyt(sam_u->fin_s.sac_p), sac_w);
  u3l_log("      counters:");
  u3l_log("                 dropped: %" PRIu64, sam_u->sat_u.dop_d);
  u3l_log("        forwards dropped: %" PRIu64, sam_u->sat_u.fod_d);
  u3l_log("        forwards pending: %" PRIu64, sam_u->sat_u.foq_d);
  u3l_log("               forwarded: %" PRIu64, sam_u->sat_u.fow_d);
  u3l_log("          filtered (hed): %" PRIu64, sam_u->sat_u.hed_d);
  u3l_log("          filtered (ver): %" PRIu64, sam_u->sat_u.vet_d);
  u3l_log("          filtered (mug): %" PRIu64, sam_u->sat_u.mut_d);
  u3l_log("          filtered (pre): %" PRIu64, sam_u->sat_u.pre_d);
  u3l_log("          filtered (wal): %" PRIu64, sam_u->sat_u.wal_d);
  u3l_log("          filtered (wap): %" PRIu64, sam_u->sat_u.wap_d);
  u3l_log("                 crashed: %" PRIu64, sam_u->sat_u.fal_d);
  u3l_log("                    evil: %" PRIu64, sam_u->sat_u.vil_d);
  u3l_log("         lane scry fails: %" PRIu64, sam_u->sat_u.saw_d);
}

/* u3_ames_io_init(): initialize ames I/O.
*/
u3_ames*
u3_ames_io_init(u3_mesa_auto* mes_u)
{
  u3_ames* sam_u  = c3_calloc(sizeof(*sam_u));
  sam_u->mes_u    = mes_u;
  sam_u->wax_u    = &mes_u->wax_u;
  sam_u->mes_u    = mes_u;
  sam_u->pir_u    = mes_u->pir_u;
  sam_u->fig_u.see_o = c3y;
  sam_u->fig_u.fit_o = c3n;

  //  enable forwarding on galaxies only
  u3_noun who = u3_ship_to_noun(sam_u->mes_u->pir_u->who_u);
  u3_noun rac = u3do("clan:title", who);
  // XX: fix for groundwire
  sam_u->sat_u.for_o = ( c3__czar == rac ) ? c3y : c3n;

  // hashtable for scry cache
  //
  // 1500 bytes per packet * 100_000 = 150MB
  // 50 bytes (average) per path * 100_000 = 5MB
  sam_u->fin_s.sac_p = u3h_new_cache(100000);

  //NOTE  some numbers on memory usage for the lane cache
  //
  //    assuming we store:
  //    a (list lane) with 1 item, 1+8 + 1 + (6*2) = 22 words
  //    and a @da as timestamp,                       8 words
  //    consed together,                              6 words
  //    with worst-case (128-bit) @p keys,            8 words
  //    and an additional cell for the k-v pair,      6 words
  //    that makes for a per-entry memory use of     50 words => 200 bytes
  //
  //    the 500k entries below would take about 100mb (in the worst case, but
  //    not accounting for hashtable overhead).
  //    we could afford more, but 500k entries is more than we'll likely use
  //    in the near future.
  //

  //XX: zif what?
  //u3_assert( !uv_udp_init(u3L, &sam_u->wax_u) );
  //u3_assert( !uv_udp_init_ex(u3L, sam_u->wax_u, UV_UDP_RECVMMSG) );
  //sam_u->wax_u->data = sam_u;

  sam_u->sil_u = u3s_cue_xeno_init();
  sam_u->tes_u = ur_cue_test_init();

  //  Disable networking for fake ships
  //
  if ( c3y == sam_u->pir_u->fak_o ) {
    u3_Host.ops_u.net = c3n;
  }


  sam_u->fin_s.sam_u = sam_u;

  {
    u3_noun now;
    struct timeval tim_u;
    gettimeofday(&tim_u, 0);

    now = u3_time_in_tv(&tim_u);
    sam_u->sev_l = u3r_mug(now);
    u3z(now);
  }

  return sam_u;
}
