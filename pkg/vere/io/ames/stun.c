#include "vere.h"
#include "zlib.h"
#include "ent/ent.h"
#include "io/ames/stun.h"

static c3_y*
_stun_add_fingerprint(c3_y *message, c3_w index)
{
  // Compute FINGERPRINT value as CRC-32 of the STUN message
  // up to (but excluding) the FINGERPRINT attribute itself,
  // XOR'ed with the 32-bit value 0x5354554e
  c3_w init = crc32(0L, Z_NULL, 0);
  c3_w crc = htonl(crc32(init, message, index) ^ 0x5354554e);

  // STUN attribute type: "FINGERPRINT"
  message[index] = 0x80;  message[index + 1] = 0x28;
  // STUN attribute length: 4 bytes
  message[index + 2] = 0x00; message[index + 3] = 0x04;

  memcpy(message + index + 4, &crc, 4);

  return message;
}

static c3_o
_stun_has_fingerprint(c3_y* buf_y, c3_w buf_len_w)
{
  c3_y ned_y[4] = {0x80, 0x28, 0x00, 0x04};
  if ( buf_len_w < 28 ) { // At least STUN header and FINGERPRINT
    return c3n;
  }

  {
    c3_y* fin_y = 0;
    c3_w i = 20; // start after the header

    fin_y = memmem(buf_y + i, buf_len_w - i, ned_y, sizeof(ned_y));
    if ( fin_y != 0 ) {
      c3_w lin_w = fin_y - buf_y;
      // Skip attribute type and length
      c3_w fingerprint = c3_sift_word(fin_y + sizeof(ned_y));
      c3_w init = crc32(0L, Z_NULL, 0);
      c3_w crc = htonl(crc32(init, buf_y, lin_w) ^ 0x5354554e);
      if ((fingerprint == crc) && (fin_y - buf_y + 8) == buf_len_w) {
        return c3y;
      }
    }

    return c3n;
  }
}

/* _stun_is_request(): buffer is a stun request.
*/
c3_o
_stun_is_request(c3_y* buf_y, c3_w len_w)
{
  c3_w cookie = htonl(0x2112A442);

  // Expects at least:
  //   STUN header and 8 byte FINGERPRINT
  if ( (len_w >= 28) &&
       (buf_y[0] == 0x0 && buf_y[1] == 0x01) &&
       (memcmp(&cookie, buf_y + 4, 4) == 0) &&
       (c3y == _stun_has_fingerprint(buf_y, len_w)) )
  {
    return c3y;
  }
  return c3n;
}

/* _stun_is_our_response(): buffer is a response to our request.
*/
c3_o
_stun_is_our_response(c3_y* buf_y, c3_y tid_y[12], c3_w len_w)
{
  c3_w cookie = htonl(0x2112A442);

  // Expects at least:
  //   STUN header, 12 byte XOR-MAPPED-ADDRESS and 8 byte FINGERPRINT
  if ( (len_w == 40) &&
       (buf_y[0] == 0x01 && buf_y[1] == 0x01) &&
       (memcmp(&cookie, buf_y + 4, 4) == 0) &&
       (memcmp(tid_y, buf_y + 8, 12) == 0) &&
       (c3y == _stun_has_fingerprint(buf_y, len_w)) )
  {
    return c3y;
  }
  return c3n;
}

/* _stun_make_request(): serialize stun request.
*/
void
_stun_make_request(c3_y buf_y[28], c3_y tid_y[12])
{
  // see STUN RFC 8489
  // https://datatracker.ietf.org/doc/html/rfc8489#section-5
  memset(buf_y, 0, 28);

  // STUN message type: "binding request"
  buf_y[1] = 0x01;

  // STUN message length: 8 (header and 32-bit FINGERPRINT)
  buf_y[2] = 0x00; buf_y[3] = 0x08;

  // STUN "magic cookie" 0x2112A442 in network byte order
  buf_y[4] = 0x21; buf_y[5] = 0x12; buf_y[6] = 0xa4; buf_y[7] = 0x42;

  // STUN "transaction id"
  memcpy(buf_y + 8, tid_y, 12);

  // FINGERPRINT
  _stun_add_fingerprint(buf_y, 20);
}

/* _stun_make_response(): serialize stun response from request.
*/
void
_stun_make_response(const c3_y  req_y[20],
                      const sockaddr_in* lan_u,
                      c3_y        buf_y[40])
{
  c3_w cok_w = 0x2112A442;
  c3_w cur_w = 20;

  //  XX hardcoded to match the requests we produce
  //
  memcpy(buf_y, req_y, cur_w);
  buf_y[0] = 0x01; buf_y[1] = 0x01; // 0x0101 SUCCESS RESPONSE
  buf_y[2] = 0x00; buf_y[3] = 0x14; // Length: 20 bytes

  buf_y[0] = 0x01; buf_y[1] = 0x01; // 0x0101 SUCCESS RESPONSE
  buf_y[2] = 0x00; buf_y[3] = 0x14; // Length: 20 bytes

  memset(buf_y + cur_w, 0, cur_w);

  // XOR-MAPPED-ADDRESS
  buf_y[cur_w + 0] = 0x00;  //
  buf_y[cur_w + 1] = 0x20;  // attribute type 0x00020
  buf_y[cur_w + 2] = 0x00;  //
  buf_y[cur_w + 3] = 0x08;  // STUN attribute length
  buf_y[cur_w + 4] = 0x00;  // extra reserved 0x0 byte
  buf_y[cur_w + 5] = 0x01;  // family  0x01:IPv4

  c3_s por_s = ntohs(lan_u->sin_port);
  c3_w pip_w = ntohl(lan_u->sin_addr.s_addr);
  por_s = htons(por_s ^ (cok_w >> 16));
  pip_w = htonl(pip_w ^ cok_w);

  memcpy(buf_y + cur_w + 6, &por_s, 2);  // X-Port
  memcpy(buf_y + cur_w + 8, &pip_w, 4);  // X-IP Addres

  // FINGERPRINT
  _stun_add_fingerprint(buf_y, cur_w + 12);
}

/* _stun_find_xor_mapped_address(): extract lane from response.
*/
c3_o
_stun_find_xor_mapped_address(c3_y*        buf_y,
                                c3_w         len_w,
                                sockaddr_in* lan_u)
{
  c3_y xor_y[4] = {0x00, 0x20, 0x00, 0x08};
  c3_w cookie = 0x2112A442;

  if ( len_w < 40 ) { // At least STUN header, XOR-MAPPED-ADDRESS & FINGERPRINT
    return c3n;
  }

  c3_w i = 20;  // start after header

  c3_y* fin_y = memmem(buf_y + i, len_w - i, xor_y, sizeof(xor_y));
  if ( fin_y != 0 ) {
    c3_w cur = (c3_w)(fin_y - buf_y) + sizeof(xor_y);

    if ( (buf_y[cur] != 0x0) && (buf_y[cur+1] != 0x1) ) {
      return c3n;
    }

    cur += 2;

    c3_s por_s = ntohs(c3_sift_short(buf_y + cur)) ^ (cookie >> 16);
    c3_w pip_w = ntohl(c3_sift_word(buf_y + cur + 2)) ^ cookie;

    lan_u->sin_family = AF_INET;
    lan_u->sin_port = htons(por_s);
    lan_u->sin_addr.s_addr = htonl(pip_w);

    if ( u3C.wag_w & u3o_verbose ) {
      c3_w nip_w = lan_u->sin_addr.s_addr;
      c3_c nip_c[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &nip_w, nip_c, INET_ADDRSTRLEN);
      u3l_log("stun: hear ip:port %s:%u", nip_c, ntohs(lan_u->sin_port));
    }
    return c3y;
  }
  return c3n;
}

typedef struct _stun_send {
  uv_udp_send_t   req_u;     //  uv udp request handle
  u3_stun_client* sun_u;     //  backpointer
  c3_y            hun_y[0];  //  buffer
} _stun_send;

/* _stun_send_cb(): stun udp send callback.
 */
static void
_stun_send_cb(uv_udp_send_t *rep_u, c3_i sas_i)
{
  _stun_send* snd_u = (_stun_send*)rep_u;

  if ( !sas_i ) {
    snd_u->sun_u->net_o = c3y;
  }
  else if ( c3y == snd_u->sun_u->net_o ) {
    u3l_log("stun: send response fail: %s", uv_strerror(sas_i));
    snd_u->sun_u->net_o = c3n;
  }

  c3_free(snd_u);
}

/* _stun_on_request(): hear stun request, send response.
 */
static void
_stun_on_request(u3_stun_client*   sun_u,
                 const c3_y*        req_y,
                 const sockaddr_in* lan_u)
{
  _stun_send* snd_u = c3_malloc(sizeof(*snd_u) + 40);
  snd_u->sun_u = sun_u;

  _stun_make_response(req_y, lan_u, snd_u->hun_y);

  uv_buf_t buf_u = uv_buf_init((c3_c*)snd_u->hun_y, 40);
  c3_i     sas_i = uv_udp_send(&snd_u->req_u, sun_u->wax_u,
                               &buf_u, 1, (const struct sockaddr*)lan_u, _stun_send_cb);

  if ( sas_i ) {
    _stun_send_cb(&snd_u->req_u, sas_i);
  }
}

void
u3_stun_start(u3_stun_client* sun_u, c3_w tim_w);

/* _stun_on_response(): hear stun response from galaxy.
 */
static void
_stun_on_response(u3_stun_client* sun_u, c3_y* buf_y, c3_w buf_len)
{
  sockaddr_in lan_u;

  //  Ignore STUN responses that dont' have the XOR-MAPPED-ADDRESS attribute
  if ( c3n == _stun_find_xor_mapped_address(buf_y, buf_len, &lan_u) ) {
    return;
  }

  if ( (sun_u->sef_u.sin_addr.s_addr != lan_u.sin_addr.s_addr) ||
       (sun_u->sef_u.sin_port != lan_u.sin_port) )
  {
    // lane changed
    u3_noun wir = u3nc(c3__ames, u3_nul);
    u3_noun cad = u3nq(c3__stun, c3__once, u3_ship_to_noun(sun_u->dad_u),
                       u3nc(c3n, u3_ames_encode_lane(lan_u)));
    u3_auto_plan(sun_u->car_u,
                 u3_ovum_init(0, c3__ames, wir, cad));
  }
  else if ( c3n == sun_u->wok_o ) {
    // stop %ping app
    u3_noun wir = u3nc(c3__ames, u3_nul);
    u3_noun cad = u3nq(c3__stun, c3__stop, u3_ship_to_noun(sun_u->dad_u),
                       u3nc(c3n, u3_ames_encode_lane(lan_u)));
    u3_auto_plan(sun_u->car_u,
                 u3_ovum_init(0, c3__ames, wir, cad));
    sun_u->wok_o = c3y;
  }

  sun_u->sef_u = lan_u;

  //  XX should no-op early
  //
  switch ( sun_u->sat_y ) {
    case STUN_OFF:       break; //  ignore; stray response
    case STUN_KEEPALIVE: break; //  ignore; duplicate response

    case STUN_TRYING: {
      u3_stun_start(sun_u, 25000);
    } break;

    default: u3_assert(!"programmer error");
  }
}

/* _stun_send_request(): send stun request to galaxy lane.
 */
static void
_stun_send_request(u3_stun_client* sun_u)
{
  u3_assert( STUN_OFF != sun_u->sat_y );

  _stun_send* snd_u = c3_malloc(sizeof(*snd_u) + 28);
  snd_u->sun_u = sun_u;

  _stun_make_request(snd_u->hun_y, sun_u->tid_y);

  uv_buf_t buf_u = uv_buf_init((c3_c*)snd_u->hun_y, 28);
  c3_i     sas_i = uv_udp_send(&snd_u->req_u, sun_u->wax_u, &buf_u, 1,
                               (const struct sockaddr*)&sun_u->lan_u, _stun_send_cb);

  if ( sas_i ) {
    _stun_send_cb(&snd_u->req_u, sas_i);
  }
}

/* _stun_reset(): stun failed. start again using max backoff
 */
static void
_stun_reset(uv_timer_t* tim_u)
{
  u3_stun_client* sun_u = (u3_stun_client*)(tim_u->data);

  u3_stun_start(sun_u, 39000);
}

/* _stun_on_lost(): stun failed (timeout); capture and reset.
 */
static void
_stun_on_lost(u3_stun_client* sun_u)
{
  sun_u->sat_y = STUN_OFF;

  //  only inject event into arvo to %kick ping app on first failure
  //
  if ( c3y == sun_u->wok_o ) {
    u3_noun wir = u3nc(c3__ames, u3_nul);
    u3_noun cad = u3nq(c3__stun, c3__fail, u3_ship_to_noun(sun_u->dad_u),
                       u3nc(c3n, u3_ames_encode_lane(sun_u->sef_u)));
    u3_auto_plan(sun_u->car_u,
                 u3_ovum_init(0, c3__ames, wir, cad));
    sun_u->wok_o = c3n;
  }

  uv_timer_start(&sun_u->tim_u, _stun_reset, 5*1000, 0);
}

/* _stun_time_gap(): elapsed milliseconds.
 */
static c3_d
_stun_time_gap(struct timeval sar_tv)
{
  struct timeval tim_tv;
  gettimeofday(&tim_tv, 0);
  u3_noun now = u3_time_in_tv(&tim_tv);
  u3_noun den = u3_time_in_tv(&sar_tv);
  return u3_time_gap_ms(den, now);
}

c3_o _ames_lamp_lane(u3_auto*, u3_ship, sockaddr_in*);

/* _stun_timer_cb(): advance stun state machine.
 */
static void
_stun_timer_cb(uv_timer_t* tim_u)
{
  u3_stun_client* sun_u = (u3_stun_client*)(tim_u->data);
  c3_w     rto_w = 500;

  switch ( sun_u->sat_y ) {
    case STUN_OFF: {
      //  ignore; stray timer (although this shouldn't happen)
      u3l_log("stun: stray timer STUN_OFF");
    } break;

    case STUN_KEEPALIVE: {
      sockaddr_in* lan_u = &(sun_u->lan_u);
      u3_ship     lam_u = sun_u->dad_u;

      if ( c3n == _ames_lamp_lane(sun_u->car_u, lam_u, lan_u) ) {
        uv_timer_start(&sun_u->tim_u, _stun_timer_cb, 25*1000, 0);
      }
      else {
        sun_u->sat_y = STUN_TRYING;
        gettimeofday(&sun_u->sar_u, 0);  //  set start time to now
        uv_timer_start(&sun_u->tim_u, _stun_timer_cb, rto_w, 0);
        _stun_send_request(sun_u);
      }
    } break;

    case STUN_TRYING: {
      c3_d gap_d = _stun_time_gap(sun_u->sar_u);
      c3_d nex_d = (gap_d * 2) + rto_w - gap_d;

      if ( gap_d >= 39500 ) {
        _stun_on_lost(sun_u);
      }
      else {
        //  wait ~s8 for the last STUN request
        //
        //    https://datatracker.ietf.org/doc/html/rfc5389#section-7.2.1
        //
        c3_w tim_w = (gap_d >= 31500) ? 8000 : c3_max(nex_d, 31500);

        uv_timer_start(&sun_u->tim_u, _stun_timer_cb, tim_w, 0);
        _stun_send_request(sun_u);
      }
    } break;

    default: u3_assert(!"programmer error");
  }
}

/* u3_stun_start(): begin/restart STUN state machine.
*/
void
u3_stun_start(u3_stun_client* sun_u, c3_w tim_w)
{
  if ( ent_getentropy(sun_u->tid_y, 12) ) {
    u3l_log("stun: getentropy fail: %s", strerror(errno));
    u3_king_bail();
  }

  sun_u->sat_y = STUN_KEEPALIVE;
  uv_timer_start(&sun_u->tim_u, _stun_timer_cb, tim_w, 0);
}


/* u3_stun_hear(): maybe hear stun packet
 */
c3_o
u3_stun_hear(u3_stun_client* sun_u,
           const struct sockaddr_in* lan_u,
           c3_w     len_w,
           c3_y*    hun_y)
{
  // XX reorg, check if a STUN req/resp can look like an ames packet
  //  check the mug hash of the body of the packet, if not check if STUN
  //  otherwise , invalid packet, log failure
  //    check ames first, assume that STUN could maybe (not likely) overlap with ames
  //    for next protocol version, have an urbit cookie
  //
  if ( c3y == _stun_is_request(hun_y, len_w) ) {
    _stun_on_request(sun_u, hun_y, lan_u);
    //c3_free(hun_y);
    return c3y;
  }
  else if ( c3y == _stun_is_our_response(hun_y,
                                           sun_u->tid_y, len_w) )
  {
    _stun_on_response(sun_u, hun_y, len_w);
    //c3_free(hun_y);
    return c3y;
  }
  return c3n;
}

