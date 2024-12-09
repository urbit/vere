#include "vere.h"
#include "zlib.h"

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

/* u3_stun_is_request(): buffer is a stun request.
*/
c3_o
u3_stun_is_request(c3_y* buf_y, c3_w len_w)
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

/* u3_stun_is_our_response(): buffer is a response to our request.
*/
c3_o
u3_stun_is_our_response(c3_y* buf_y, c3_y tid_y[12], c3_w len_w)
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

/* u3_stun_make_request(): serialize stun request.
*/
void
u3_stun_make_request(c3_y buf_y[28], c3_y tid_y[12])
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

/* u3_stun_make_response(): serialize stun response from request.
*/
void
u3_stun_make_response(const c3_y req_y[20],
                      u3_lane*   lan_u,
                      c3_y       buf_y[40])
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

  c3_s por_s = htons(lan_u->por_s ^ (cok_w >> 16));
  c3_w pip_w = htonl(lan_u->pip_w ^ cok_w);

  memcpy(buf_y + cur_w + 6, &por_s, 2);  // X-Port
  memcpy(buf_y + cur_w + 8, &pip_w, 4);  // X-IP Addres

  // FINGERPRINT
  _stun_add_fingerprint(buf_y, cur_w + 12);
}

/* u3_stun_find_xor_mapped_address(): extract lane from response.
*/
c3_o
u3_stun_find_xor_mapped_address(c3_y*    buf_y,
                                c3_w     len_w,
                                u3_lane* lan_u)
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

    lan_u->por_s = ntohs(c3_sift_short(buf_y + cur)) ^ (cookie >> 16);
    lan_u->pip_w = ntohl(c3_sift_word(buf_y + cur + 2)) ^ cookie;

    if ( u3C.wag_w & u3o_verbose ) {
      c3_w nip_w = htonl(lan_u->pip_w);
      c3_c nip_c[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &nip_w, nip_c, INET_ADDRSTRLEN);
      u3l_log("stun: hear ip:port %s:%u", nip_c, lan_u->por_s);
    }
    return c3y;
  }
  return c3n;
}
