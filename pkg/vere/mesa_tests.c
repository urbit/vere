/// @file
///
/// Integration tests for the mesa fragment-reassembly path. These drive the
/// request state machine directly (mirroring the state that
/// _mesa_req_pact_init builds for a first PAGE) and feed it the kind of
/// attacker-controlled follow-up fragment a malicious peer could send.

#include "./io/mesa.c"

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 22);
  u3m_pave(c3y);
}

/* _mk_req(): build the pending-request state a normal first PAGE produces.
**
**  Mirrors the relevant allocations in _mesa_req_pact_init: a destination
**  buffer [dat_y] sized by the first page's [tob_d] assuming 1024-byte
**  (boq 13) fragments, plus the received-bitset. The lss verifier, gauge,
**  and per-fragment state are left null: the malicious fragments below are
**  rejected before any of those are touched.
*/
static void
_mk_req(u3_pend_req* req_u,
        u3_peer*     per_u,
        u3_mesa*     sam_u,
        arena*       are_u,
        c3_d         tob_d)
{
  memset(sam_u, 0, sizeof(*sam_u));
  memset(per_u, 0, sizeof(*per_u));
  memset(req_u, 0, sizeof(*req_u));

  per_u->sam_u = sam_u;

  req_u->per_u = per_u;
  req_u->tob_d = tob_d;
  req_u->tof_d = mesa_num_leaves(tob_d);
  req_u->dat_y = new(are_u, c3_y, tob_d);   //  destination sized by first page
  bitset_init(&req_u->was_u, 1024, are_u);
}

/* _test_frag_heap_overflow(): C2 (SECURITY-AUDIT) — remote heap overflow.
**
**  The destination buffer [req_u->dat_y] is allocated once, sized by the
**  first page's [tob_d] assuming boq 13. Requests are keyed only on the path
**  string, so every subsequent fragment maps to the same request while
**  carrying its own attacker-controlled [tob_d]/[boq_y]/[fra_d]/[len_w]. The
**  write in _mesa_req_pact_done uses those fields with no check that they
**  match what the buffer was sized for:
**
**    c3_w siz_w = (1 << (nam_u->boq_y - 3));
**    memcpy(req_u->dat_y + (siz_w * nam_u->fra_d), dat_u->fra_y, dat_u->len_w);
**
**  Here boq_y = 31 makes siz_w = 1<<28 (256 MB) and fra_d = 1, so the write
**  lands ~256 MB past a 2 KB buffer: a wild heap write with attacker bytes
**  and offset. Pre-fix this crashes; post-fix the fragment must be dropped.
*/
static c3_i
_test_frag_heap_overflow(void)
{
  arena       are_u = arena_create(1 << 20);
  u3_mesa     sam_u;
  u3_peer     per_u;
  u3_pend_req req_u;

  //  first page set up a 2048-byte (2-leaf, boq 13) message
  //
  _mk_req(&req_u, &per_u, &sam_u, &are_u, 2048);

  //  malicious follow-up fragment
  //
  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.fra_d = 1;
  nam_u.boq_y = 31;                 //  siz_w = 1<<28 == 256 MB

  c3_y fra_y[16];
  memset(fra_y, 0x41, sizeof(fra_y));

  u3_mesa_data dat_u;
  memset(&dat_u, 0, sizeof(dat_u));
  dat_u.tob_d        = 1u << 24;    //  16 MB -> num_leaves >> fra_d, clears 1132 check
  dat_u.len_w        = sizeof(fra_y);
  dat_u.fra_y        = fra_y;
  dat_u.aut_u.typ_e  = AUTH_SIGN;   //  not AUTH_PAIR

  sockaddr_in lan_u;
  memset(&lan_u, 0, sizeof(lan_u));

  //  pre-fix: wild heap write ~256 MB past dat_y -> crash
  //  post-fix: rejected (boq != 13 / tob mismatch / offset OOB) -> drop
  //
  _mesa_req_pact_done(&req_u, &nam_u, &dat_u, 0, lan_u);

  //  if we survive, the malicious fragment must have been dropped, not
  //  processed (the accept path decrements out_d / records the fragment)
  //
  if ( 0 != req_u.out_d ) {
    fprintf(stderr, "mesa: C2 — malicious fragment was accepted\r\n");
    arena_free(&are_u);
    return 1;
  }

  arena_free(&are_u);
  return 0;
}

/* _test_frag_boq_overflow(): C2 variant — small boq, large fra_d.
**
**  The same write with boq_y = 13 (siz_w = 1024) and fra_d = 5000 lands at
**  offset 5,120,000 into the 2 KB buffer. [fra_d] is bounded only against
**  num_leaves(dat_u->tob_d) — *this* packet's tob_d — so an attacker sets
**  tob_d huge to clear that gate while fra_d still points far past dat_y.
*/
static c3_i
_test_frag_boq_overflow(void)
{
  arena       are_u = arena_create(1 << 20);
  u3_mesa     sam_u;
  u3_peer     per_u;
  u3_pend_req req_u;

  _mk_req(&req_u, &per_u, &sam_u, &are_u, 2048);

  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.fra_d = 5000;               //  offset 1024 * 5000 = 5,120,000
  nam_u.boq_y = 13;

  c3_y fra_y[16];
  memset(fra_y, 0x42, sizeof(fra_y));

  u3_mesa_data dat_u;
  memset(&dat_u, 0, sizeof(dat_u));
  dat_u.tob_d        = 16u << 20;   //  16 MB -> num_leaves ~16384 > 5000
  dat_u.len_w        = sizeof(fra_y);
  dat_u.fra_y        = fra_y;
  dat_u.aut_u.typ_e  = AUTH_SIGN;

  sockaddr_in lan_u;
  memset(&lan_u, 0, sizeof(lan_u));

  _mesa_req_pact_done(&req_u, &nam_u, &dat_u, 0, lan_u);

  if ( 0 != req_u.out_d ) {
    fprintf(stderr, "mesa: C2 — oversize fra_d fragment was accepted\r\n");
    arena_free(&are_u);
    return 1;
  }

  arena_free(&are_u);
  return 0;
}

/* _test_frag_valid_accepted(): the C2 fix must not reject legitimate traffic.
**
**  A well-formed fragment — matching tob_d, boq 13, in-range fra_d, fitting
**  within the buffer — must still be written into dat_y and recorded. Drives
**  the misordered-queue branch (los_u->counter != fra_d) so no lss crypto or
**  resend timer is needed.
*/
static c3_i
_test_frag_valid_accepted(void)
{
  arena       are_u = arena_create(1 << 20);
  u3_mesa     sam_u;
  u3_peer     per_u;
  u3_pend_req req_u;

  _mk_req(&req_u, &per_u, &sam_u, &are_u, 2048);

  //  zero the destination (arena memory is not zeroed) so we can see the write
  //
  memset(req_u.dat_y, 0, 2048);
  req_u.out_d = 1;                       //  accept path decrements this to 0

  //  minimal state for the misordered-queue branch
  //
  req_u.los_u = new(&are_u, lss_verifier, 1);
  memset(req_u.los_u, 0, sizeof(*req_u.los_u));
  req_u.los_u->counter = 0;              //  != fra_d below -> misordered branch

  req_u.gag_u = new(&are_u, u3_gage, 1);
  _init_gage(req_u.gag_u);

  req_u.wat_u = new(&are_u, u3_pact_stat, req_u.tof_d + 2);
  memset(req_u.wat_u, 0, sizeof(u3_pact_stat) * (req_u.tof_d + 2));

  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.fra_d = 1;                       //  in range (tof_d == 2)
  nam_u.boq_y = 13;

  c3_y fra_y[16];
  memset(fra_y, 0x43, sizeof(fra_y));

  u3_mesa_data dat_u;
  memset(&dat_u, 0, sizeof(dat_u));
  dat_u.tob_d        = 2048;             //  matches the request
  dat_u.len_w        = sizeof(fra_y);
  dat_u.fra_y        = fra_y;
  dat_u.aut_u.typ_e  = AUTH_SIGN;

  sockaddr_in lan_u;
  memset(&lan_u, 0, sizeof(lan_u));

  _mesa_req_pact_done(&req_u, &nam_u, &dat_u, 0, lan_u);

  c3_i ret_i = 0;

  if ( c3n == bitset_has(&req_u.was_u, nam_u.fra_d) ) {
    fprintf(stderr, "mesa: valid fragment was not recorded\r\n");
    ret_i = 1;
  }

  //  written at offset siz_w * fra_d == 1024 * 1
  //
  if ( 0 != memcmp(req_u.dat_y + 1024, fra_y, sizeof(fra_y)) ) {
    fprintf(stderr, "mesa: valid fragment was not written to dat_y\r\n");
    ret_i = 1;
  }

  arena_free(&are_u);
  return ret_i;
}

/* _mk_driver(): stand up the minimal mesa driver state for hear-path tests.
**
**  Mirrors the parts of u3_mesa_io_init the receive path actually touches:
**  permanent + per-packet arenas, the verstable maps, and a pier stub whose
**  only field read here is who_d. The pit-clear libuv timer is skipped — the
**  paths under test never arm it.
*/
static u3_mesa*
_mk_driver(u3_ship who_u)
{
  arena    par_u = arena_create(1 << 20);
  u3_mesa* sam_u = new(&par_u, u3_mesa, 1);
  memset(sam_u, 0, sizeof(*sam_u));
  sam_u->par_u = par_u;
  sam_u->are_u = arena_create(1 << 20);

  u3_pier* pir_u = c3_calloc(sizeof(*pir_u));
  pir_u->who_d[0] = who_u[0];
  pir_u->who_d[1] = who_u[1];
  pir_u->fak_o    = c3y;
  sam_u->pir_u    = pir_u;

  vt_init(&sam_u->pit_u);
  vt_init(&sam_u->per_u);
  vt_init(&sam_u->gag_u);
  vt_init(&sam_u->jum_u);
  vt_init(&sam_u->req_u);

  return sam_u;
}

/* _test_page_init_bad_boq(): H2 (SECURITY-AUDIT) — driver-level harness.
**
**  Drives the real receive dispatch: a PAGE responding to a peek we have
**  outstanding (request in CTAG_WAIT) flows through _mesa_hear_page. The
**  leaf-count math (mesa_num_leaves) assumes boq 13 (1024-byte leaves), and
**  _mesa_req_pact_init's buffer sizing relies on it; a first page carrying
**  any other block size is now rejected in _mesa_hear_page before that math
**  runs, rather than aborting deeper in the request path.
**
**  We pre-insert the peer so _meet_peer (which scries the pier) is skipped,
**  and register the CTAG_WAIT request under the packet's path.
*/
static c3_i
_test_page_init_bad_boq(void)
{
  u3_ship  who_u = { 0x42, 0 };
  u3_mesa* sam_u = _mk_driver(who_u);

  //  pre-existing peer for the sender (== us), so no pier scry is needed
  //
  u3_peer* per_u = new(&sam_u->par_u, u3_peer, 1);
  _init_peer(sam_u, per_u);
  per_u->her_u[0] = who_u[0];
  per_u->her_u[1] = who_u[1];
  _mesa_put_peer(sam_u, who_u, per_u);

  //  the path keys the request; the incoming page must carry the same path
  //
  c3_c* pax_c = "/test/path";

  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.her_u[0] = who_u[0];
  nam_u.her_u[1] = who_u[1];
  nam_u.boq_y    = 31;             //  H2 trigger: not boq 13
  nam_u.fra_d    = 0;
  nam_u.nit_o    = c3y;
  nam_u.pat_c    = pax_c;
  nam_u.pat_s    = strlen(pax_c);

  //  we sent a peek for this path -> request is waiting
  //
  _mesa_put_request(sam_u, &nam_u, (u3_pend_req*)CTAG_WAIT);

  //  craft the malicious first PAGE: bad block size, multi-fragment size
  //
  u3_mesa_pict* pic_u = new(&sam_u->are_u, u3_mesa_pict, 1);
  memset(pic_u, 0, sizeof(*pic_u));
  pic_u->sam_u                       = sam_u;
  pic_u->pac_u.hed_u.typ_y           = PACT_PAGE;
  pic_u->pac_u.hed_u.hop_y           = 0;
  pic_u->pac_u.pag_u.nam_u           = nam_u;
  pic_u->pac_u.pag_u.dat_u.tob_d     = 1u << 20;   //  multi-fragment
  pic_u->pac_u.pag_u.dat_u.len_w     = 0;
  pic_u->pac_u.pag_u.dat_u.aut_u.typ_e = AUTH_SIGN;

  sockaddr_in lan_u;
  memset(&lan_u, 0, sizeof(lan_u));
  lan_u.sin_family = AF_INET;

  //  post-fix: dropped at the boq guard (STRANGE bumps dop_w); the request
  //  stays CTAG_WAIT and never reaches _mesa_req_pact_init
  //
  _mesa_hear_page(pic_u, lan_u);

  c3_i ret_i = 0;
  if ( 1 != sam_u->sat_u.dop_w ) {
    fprintf(stderr, "mesa: H2 — bad-boq page not flagged STRANGE "
                    "(dop_w=%u)\r\n", sam_u->sat_u.dop_w);
    ret_i = 1;
  }
  if ( (u3_pend_req*)CTAG_WAIT != _mesa_get_request(sam_u, &nam_u) ) {
    fprintf(stderr, "mesa: H2 — bad-boq page was not dropped cleanly\r\n");
    ret_i = 1;
  }

  c3_free(sam_u->pir_u);
  arena_free(&sam_u->are_u);
  arena_free(&sam_u->par_u);
  return ret_i;
}

/* _test_wire_page_init_bad_boq(): H2 — full wire-level harness.
**
**  Like _test_page_init_bad_boq, but drives the true UDP entry point
**  _mesa_hear from raw bytes: a PAGE pact is encoded with
**  mesa_etch_pact_to_buf (which computes the header mug), then handed to
**  _mesa_hear, which runs mesa_is_new_pact + mesa_sift_pact_from_buf before
**  dispatching to _mesa_hear_page. This exercises the packet codec in
**  addition to the request path. The encoded bad block size must round-trip
**  through sift and then be dropped by the H2 boq guard.
*/
static c3_i
_test_wire_page_init_bad_boq(void)
{
  u3_ship  who_u = { 0x42, 0 };
  u3_mesa* sam_u = _mk_driver(who_u);

  u3_peer* per_u = new(&sam_u->par_u, u3_peer, 1);
  _init_peer(sam_u, per_u);
  per_u->her_u[0] = who_u[0];
  per_u->her_u[1] = who_u[1];
  _mesa_put_peer(sam_u, who_u, per_u);

  c3_c* pax_c = "/test/path";

  u3_mesa_name nam_u;
  memset(&nam_u, 0, sizeof(nam_u));
  nam_u.her_u[0] = who_u[0];
  nam_u.her_u[1] = who_u[1];
  nam_u.rif_w    = 0;
  nam_u.boq_y    = 31;            //  H2 trigger: not boq 13
  nam_u.nit_o    = c3y;           //  init page; fra_d implicitly 0
  nam_u.fra_d    = 0;
  nam_u.pat_c    = pax_c;
  nam_u.pat_s    = strlen(pax_c);

  _mesa_put_request(sam_u, &nam_u, (u3_pend_req*)CTAG_WAIT);

  //  build a real PAGE pact and encode it to wire bytes
  //
  c3_y sig_y[64];
  memset(sig_y, 0, sizeof(sig_y));

  u3_mesa_pact pac_u;
  memset(&pac_u, 0, sizeof(pac_u));
  pac_u.hed_u.pro_y              = MESA_VER;   //  required by etch/sift
  pac_u.hed_u.nex_y              = HOP_NONE;
  pac_u.hed_u.typ_y              = PACT_PAGE;
  pac_u.hed_u.hop_y              = 0;
  pac_u.pag_u.nam_u              = nam_u;
  pac_u.pag_u.dat_u.tob_d        = 1u << 20;   //  multi-fragment
  pac_u.pag_u.dat_u.len_w        = 0;
  pac_u.pag_u.dat_u.fra_y        = sig_y;      //  unused (len 0), non-NULL
  pac_u.pag_u.dat_u.aut_u.typ_e  = AUTH_SIGN;
  memcpy(pac_u.pag_u.dat_u.aut_u.sig_y, sig_y, sizeof(sig_y));

  c3_y buf_y[PACT_SIZE];
  memset(buf_y, 0, sizeof(buf_y));
  c3_w len_w = mesa_etch_pact_to_buf(buf_y, PACT_SIZE, &pac_u);

  c3_i ret_i = 0;

  //  the encoded bytes must be recognized as a new mesa packet
  //
  if ( c3n == mesa_is_new_pact(buf_y, len_w) ) {
    fprintf(stderr, "mesa: wire — encoded packet not recognized as mesa\r\n");
    ret_i = 1;
  }
  else {
    sockaddr_in lan_u;
    memset(&lan_u, 0, sizeof(lan_u));
    lan_u.sin_family = AF_INET;

    //  true wire entry: sift + dispatch into _mesa_hear_page
    //
    _mesa_hear(sam_u, (const struct sockaddr*)&lan_u, len_w, buf_y);

    //  the packet must actually have traversed sift -> dispatch and hit the
    //  H2 boq guard (STRANGE bumps dop_w). Without this, a silent sift
    //  failure would also leave the request CTAG_WAIT and masquerade as a
    //  pass.
    //
    if ( 1 != sam_u->sat_u.dop_w ) {
      fprintf(stderr, "mesa: H2 (wire) — page did not reach the boq guard "
                      "(dop_w=%u)\r\n", sam_u->sat_u.dop_w);
      ret_i = 1;
    }

    if ( (u3_pend_req*)CTAG_WAIT != _mesa_get_request(sam_u, &nam_u) ) {
      fprintf(stderr, "mesa: H2 (wire) — bad-boq page not dropped cleanly\r\n");
      ret_i = 1;
    }
  }

  c3_free(sam_u->pir_u);
  arena_free(&sam_u->are_u);
  arena_free(&sam_u->par_u);
  return ret_i;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  c3_i ret_i = 0;

  ret_i |= _test_frag_heap_overflow();
  ret_i |= _test_frag_boq_overflow();
  ret_i |= _test_frag_valid_accepted();
  ret_i |= _test_page_init_bad_boq();
  ret_i |= _test_wire_page_init_bad_boq();

  if ( ret_i ) {
    fprintf(stderr, "test mesa: failed\r\n");
    exit(1);
  }

  u3m_grab(u3_none);

  fprintf(stderr, "test mesa: ok\r\n");
  return 0;
}
