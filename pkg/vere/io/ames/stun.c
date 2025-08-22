#include "vere.h"
#include "zlib.h"

#ifdef U3_OS_windows
static char *twobyte_memmem(const unsigned char *h, size_t k, const unsigned char *n)
{
	uint16_t nw = n[0]<<8 | n[1], hw = h[0]<<8 | h[1];
	for (h++, k--; k; k--, hw = hw<<8 | *++h)
		if (hw == nw) return (char *)h-1;
	return 0;
}

static char *threebyte_memmem(const unsigned char *h, size_t k, const unsigned char *n)
{
	uint32_t nw = n[0]<<24 | n[1]<<16 | n[2]<<8;
	uint32_t hw = h[0]<<24 | h[1]<<16 | h[2]<<8;
	for (h+=2, k-=2; k; k--, hw = (hw|*++h)<<8)
		if (hw == nw) return (char *)h-2;
	return 0;
}

static char *fourbyte_memmem(const unsigned char *h, size_t k, const unsigned char *n)
{
	uint32_t nw = n[0]<<24 | n[1]<<16 | n[2]<<8 | n[3];
	uint32_t hw = h[0]<<24 | h[1]<<16 | h[2]<<8 | h[3];
	for (h+=3, k-=3; k; k--, hw = hw<<8 | *++h)
		if (hw == nw) return (char *)h-3;
	return 0;
}

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define BITOP(a,b,op) \
 ((a)[(size_t)(b)/(8*sizeof *(a))] op (size_t)1<<((size_t)(b)%(8*sizeof *(a))))

static char *twoway_memmem(const unsigned char *h, const unsigned char *z, const unsigned char *n, size_t l)
{
	size_t i, ip, jp, k, p, ms, p0, mem, mem0;
	size_t byteset[32 / sizeof(size_t)] = { 0 };
	size_t shift[256];

	/* Computing length of needle and fill shift table */
	for (i=0; i<l; i++)
		BITOP(byteset, n[i], |=), shift[n[i]] = i+1;

	/* Compute maximal suffix */
	ip = -1; jp = 0; k = p = 1;
	while (jp+k<l) {
		if (n[ip+k] == n[jp+k]) {
			if (k == p) {
				jp += p;
				k = 1;
			} else k++;
		} else if (n[ip+k] > n[jp+k]) {
			jp += k;
			k = 1;
			p = jp - ip;
		} else {
			ip = jp++;
			k = p = 1;
		}
	}
	ms = ip;
	p0 = p;

	/* And with the opposite comparison */
	ip = -1; jp = 0; k = p = 1;
	while (jp+k<l) {
		if (n[ip+k] == n[jp+k]) {
			if (k == p) {
				jp += p;
				k = 1;
			} else k++;
		} else if (n[ip+k] < n[jp+k]) {
			jp += k;
			k = 1;
			p = jp - ip;
		} else {
			ip = jp++;
			k = p = 1;
		}
	}
	if (ip+1 > ms+1) ms = ip;
	else p = p0;

	/* Periodic needle? */
	if (memcmp(n, n+p, ms+1)) {
		mem0 = 0;
		p = MAX(ms, l-ms-1) + 1;
	} else mem0 = l-p;
	mem = 0;

	/* Search loop */
	for (;;) {
		/* If remainder of haystack is shorter than needle, done */
		if (z-h < l) return 0;

		/* Check last byte first; advance by shift on mismatch */
		if (BITOP(byteset, h[l-1], &)) {
			k = l-shift[h[l-1]];
			if (k) {
				if (mem0 && mem && k < p) k = l-p;
				h += k;
				mem = 0;
				continue;
			}
		} else {
			h += l;
			mem = 0;
			continue;
		}

		/* Compare right half */
		for (k=MAX(ms+1,mem); k<l && n[k] == h[k]; k++);
		if (k < l) {
			h += k-ms;
			mem = 0;
			continue;
		}
		/* Compare left half */
		for (k=ms+1; k>mem && n[k-1] == h[k-1]; k--);
		if (k <= mem) return (char *)h;
		h += p;
		mem = mem0;
	}
}

void *memmem(const void *h0, size_t k, const void *n0, size_t l)
{
	const unsigned char *h = h0, *n = n0;

	/* Return immediately on empty needle */
	if (!l) return (void *)h;

	/* Return immediately when needle is longer than haystack */
	if (k<l) return 0;

	/* Use faster algorithms for short needles */
	h = memchr(h0, *n, k);
	if (!h || l==1) return (void *)h;
	k -= h - (const unsigned char *)h0;
	if (l==2) return twobyte_memmem(h, k, n);
	if (l==3) return threebyte_memmem(h, k, n);
	if (l==4) return fourbyte_memmem(h, k, n);

	return twoway_memmem(h, h+k, n, l);
}
#endif


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
