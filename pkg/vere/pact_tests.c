/// @file

#include "./io/xmas/pact.c"

#define cmp_scalar(nam, str, fmt)                       \
  if ( hav_u->nam != ned_u->nam ) {                     \
    fprintf(stderr, "xmas test cmp " str " differ:\r\n" \
                    "    have: " fmt "\r\n"             \
                    "    need: " fmt "\r\n",            \
                    hav_u->nam, ned_u->nam);            \
    ret_i = 1;                                          \
  }

#define cmp_string(nam, siz, str)                       \
  if ( memcmp(hav_u->nam, ned_u->nam, siz) ) {          \
    fprintf(stderr, "xmas test cmp " str " differ:\r\n" \
                    "    have: %*.s\r\n"                \
                    "    need: %*.s\r\n",               \
                    siz, hav_u->nam, siz, ned_u->nam);  \
    ret_i = 1;                                          \
  }

#define cmp_buffer(nam, siz, str)                 \
  if ( memcmp(hav_u->nam, ned_u->nam, siz) ) {    \
    fprintf(stderr, "xmas test cmp " str "\r\n"); \
    ret_i = 1;                                    \
  }

static c3_i
_test_cmp_head(u3_xmas_head* hav_u, u3_xmas_head* ned_u)
{
  c3_i ret_i = 0;

  cmp_scalar(nex_y, "head: next", "%u");
  cmp_scalar(pro_y, "head: protocol", "%u");
  cmp_scalar(typ_y, "head: type", "%u");
  cmp_scalar(hop_y, "head: hop", "%u");
  cmp_scalar(mug_w, "head: mug", "%u");

  return ret_i;
}

static c3_i
_test_cmp_name(u3_xmas_name* hav_u, u3_xmas_name* ned_u)
{
  c3_i ret_i = 0;

  cmp_buffer(her_d, sizeof(ned_u->her_d), "name: ships differ");

  cmp_scalar(rif_w, "name: rifts", "%u");
  cmp_scalar(boq_y, "name: bloqs", "%u");
  cmp_scalar(nit_o, "name: inits", "%u");
  cmp_scalar(aut_o, "name: auths", "%u");
  cmp_scalar(fra_w, "name: fragments", "%u");
  cmp_scalar(pat_s, "name: path-lengths", "%u");

  cmp_string(pat_c, ned_u->pat_s, "name: paths");

  return ret_i;
}

static c3_i
_test_cmp_data(u3_xmas_data* hav_u, u3_xmas_data* ned_u)
{
  c3_i ret_i = 0;

  cmp_scalar(tot_w, "data: total packets", "%u");

  cmp_scalar(aum_u.typ_e, "data: auth-types", "%u");
  cmp_buffer(aum_u.sig_y, 64, "data: sig|hmac|null");

  cmp_scalar(aup_u.len_y, "data: hash-lengths", "%u");
  cmp_buffer(aup_u.has_y, ned_u->aup_u.len_y, "data: hashes");

  cmp_scalar(len_w, "data: fragments-lengths", "%u");
  cmp_buffer(fra_y, ned_u->len_w, "data: fragments");

  return ret_i;
}

static c3_i
_test_pact(u3_xmas_pact* pac_u)
{
  c3_y* buf_y = c3_calloc(PACT_SIZE);
  c3_w  len_w = xmas_etch_pact(buf_y, pac_u);
  c3_i  ret_i = 0;
  c3_i  bot_i = 0;
  c3_w  sif_w;

  u3_xmas_pact nex_u;
  memset(&nex_u, 0, sizeof(u3_xmas_pact));

  if ( !len_w ) {
    fprintf(stderr, "pact: etch failed\r\n");
    ret_i = 1; goto done;
  }
  else if ( len_w > PACT_SIZE ) {
    fprintf(stderr, "pact: etch overflowed: %u\r\n", len_w);
    ret_i = 1; goto done;
  }

  if ( len_w != (sif_w = xmas_sift_pact(&nex_u, buf_y, len_w)) ) {
    fprintf(stderr, "pact: sift failed len=%u sif=%u\r\n", len_w, sif_w);
    _log_buf(buf_y, len_w);
    ret_i = 1; goto done;
  }

  bot_i = 1;

  if ( _test_cmp_head(&nex_u.hed_u, &pac_u->hed_u) ) {
    fprintf(stderr, "pact: head cmp fail\r\n");
    ret_i = 1;
  }

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      if ( _test_cmp_name(&nex_u.pek_u.nam_u, &pac_u->pek_u.nam_u) ) {
        fprintf(stderr, "%%peek name cmp fail\r\n");
        ret_i = 1;
      }
    } break;

    case PACT_PAGE: {
      if ( _test_cmp_name(&nex_u.pag_u.nam_u, &pac_u->pag_u.nam_u) ) {
        fprintf(stderr, "%%page name cmp fail\r\n");
        ret_i = 1;
      }
      else if ( _test_cmp_data(&nex_u.pag_u.dat_u, &pac_u->pag_u.dat_u) ) {
        fprintf(stderr, "%%page data cmp fail\r\n");
        ret_i = 1;
      }
    } break;

    case PACT_POKE: {
      if ( _test_cmp_name(&nex_u.pok_u.nam_u, &pac_u->pok_u.nam_u) ) {
        fprintf(stderr, "%%poke name cmp fail\r\n");
        ret_i = 1;
      }
      else if ( _test_cmp_name(&nex_u.pok_u.pay_u, &pac_u->pok_u.pay_u) ) {
        fprintf(stderr, "%%poke pay-name cmp fail\r\n");
        ret_i = 1;
      }
      else if ( _test_cmp_data(&nex_u.pok_u.dat_u, &pac_u->pok_u.dat_u) ) {
        fprintf(stderr, "%%poke data cmp fail\r\n");
        ret_i = 1;
      }
    } break;

    default: {
      fprintf(stderr, "test: strange packet\r\n");
      ret_i = 1;
    }
  }

done:
  if ( ret_i ) {
    _log_head(&pac_u->hed_u);
    _log_pact(pac_u);
    _log_buf(buf_y, len_w);

    if ( bot_i ) {
      u3l_log(RED_TEXT);
      _log_head(&nex_u.hed_u);
      _log_pact(&nex_u);
      u3l_log(DEF_TEXT);
    }
  }

  c3_free(buf_y);

  return ret_i;
}

static c3_y
_test_rand_bit(void* ptr_v)
{
  return rand() & 1;
}

static c3_y
_test_rand_bits(void* ptr_v, c3_y len_y)
{
  assert( 8 >= len_y );
  return rand() & ((1 << len_y) - 1);
}

static c3_w
_test_rand_word(void* ptr_v)
{
  c3_w low_w = rand();
  c3_w hig_w = rand();
  return (hig_w << 16) ^ (low_w & ((1 << 16) - 1));
}

static c3_y
_test_rand_gulf_y(void* ptr_v, c3_y top_y)
{
  c3_y bit_y = c3_bits_word(top_y);
  c3_y res_y = 0;

  if ( !bit_y ) return res_y;

  while ( 1 ) {
    res_y = _test_rand_bits(ptr_v, bit_y);

    if ( res_y < top_y ) {
      return res_y;
    }
  }
}

static c3_w
_test_rand_gulf_w(void* ptr_v, c3_w top_w)
{
  c3_w bit_w = c3_bits_word(top_w);
  c3_w res_w = 0;

  if ( !bit_w ) return res_w;

  while ( 1 ) {
    res_w  = _test_rand_word(ptr_v);
    res_w &= (1 << bit_w) - 1;

    if ( res_w < top_w ) {
      return res_w;
    }
  }
}

static void
_test_rand_bytes(void* ptr_v, c3_w len_w, c3_y* buf_y)
{
  c3_w max_w = len_w / 2;

  while ( max_w-- ) {
    c3_w wor_w = rand();
    *buf_y++ = (wor_w >> 0) & 0xff;
    *buf_y++ = (wor_w >> 8) & 0xff;
  }

  if ( len_w & 1 ) {
    *buf_y = rand() & 0xff;
  }
}

const char* ta_c = "-~_.0123456789abcdefghijklmnopqrstuvwxyz";

static void
_test_rand_knot(void* ptr_v, c3_w len_w, c3_c* not_c)
{
  for ( c3_w i_w = 0; i_w < len_w; i_w++ ) {
    *not_c++ = ta_c[_test_rand_gulf_y(ptr_v, 40)];
  }
}

static void
_test_rand_path(void* ptr_v, c3_s len_s, c3_c* pat_c)
{
  c3_s not_s = 0;

  while ( len_s ) {
    not_s = _test_rand_gulf_w(ptr_v, len_s);
    _test_rand_knot(ptr_v, not_s, pat_c);
    pat_c[not_s] = '/';
    pat_c += not_s + 1;
    len_s -= not_s + 1;
  }
}

static void
_test_make_head(void* ptr_v, u3_xmas_head* hed_u)
{
  hed_u->nex_y = NEXH_NONE; // XX
  hed_u->pro_y = 1;
  hed_u->typ_y = _test_rand_gulf_y(ptr_v, 3) + 1;
  hed_u->hop_y = _test_rand_gulf_y(ptr_v, 8);
  hed_u->mug_w = 0;
  // XX set mug_w in etch?
}

static void
_test_make_name(void* ptr_v, c3_s pat_s, u3_xmas_name* nam_u)
{
  _test_rand_bytes(ptr_v, 16, (c3_y*)nam_u->her_d);
  nam_u->rif_w = _test_rand_word(ptr_v);

  nam_u->pat_s = _test_rand_gulf_w(ptr_v, pat_s);
  nam_u->pat_c = c3_malloc(nam_u->pat_s + 1);
  _test_rand_path(ptr_v, nam_u->pat_s, nam_u->pat_c);
  nam_u->pat_c[nam_u->pat_s] = 0;

  nam_u->boq_y = _test_rand_bits(ptr_v, 8);
  nam_u->nit_o = _test_rand_bits(ptr_v, 1);

  if ( c3y == nam_u->nit_o ) {
    nam_u->aut_o = c3n;
    nam_u->fra_w = 0;
  }
  else {
    nam_u->aut_o = _test_rand_bits(ptr_v, 1);
    nam_u->fra_w = _test_rand_word(ptr_v);
  }
}

static void
_test_make_data(void* ptr_v, u3_xmas_data* dat_u)
{
  dat_u->tot_w = _test_rand_word(ptr_v);

  memset(dat_u->aum_u.sig_y, 0, 64);
  dat_u->aup_u.len_y = 0;
  memset(dat_u->aup_u.has_y, 0, sizeof(dat_u->aup_u.has_y));

  switch ( dat_u->aum_u.typ_e = _test_rand_bits(ptr_v, 2) ) {
    case AUTH_NEXT: {
      dat_u->aup_u.len_y = 2;
    } break;

    case AUTH_SIGN: {
      dat_u->aup_u.len_y = _test_rand_gulf_y(ptr_v, 3);
      _test_rand_bytes(ptr_v, 64, dat_u->aum_u.sig_y);
    } break;

    case AUTH_HMAC: {
      dat_u->aup_u.len_y = _test_rand_gulf_y(ptr_v, 3);
      _test_rand_bytes(ptr_v, 32, dat_u->aum_u.mac_y);
    } break;

    default: break;
  }

  for ( c3_w i_w = 0; i_w < dat_u->aup_u.len_y; i_w++ ) {
    _test_rand_bytes(ptr_v, 32, dat_u->aup_u.has_y[i_w]);
  }

  dat_u->len_w = _test_rand_gulf_w(ptr_v, 1024);
  dat_u->fra_y = c3_calloc(dat_u->len_w);
  _test_rand_bytes(ptr_v, dat_u->len_w, dat_u->fra_y);
}

static void
_test_make_pact(void* ptr_v, u3_xmas_pact* pac_u)
{
  _test_make_head(ptr_v, &pac_u->hed_u);

  switch ( pac_u->hed_u.typ_y ) {
    case PACT_PEEK: {
      _test_make_name(ptr_v, 277, &pac_u->pek_u.nam_u);
    } break;

    case PACT_PAGE: {
      _test_make_name(ptr_v, 277, &pac_u->pag_u.nam_u);
      _test_make_data(ptr_v, &pac_u->pag_u.dat_u);
    } break;

    case PACT_POKE: {
      _test_make_name(ptr_v, 124, &pac_u->pok_u.nam_u);
      _test_make_name(ptr_v, 124, &pac_u->pok_u.pay_u);
      _test_make_data(ptr_v, &pac_u->pok_u.dat_u);
    } break;

    default: break; // XX
  }
}

static c3_i
_test_rand_pact(c3_w bat_w)
{
  u3_xmas_pact pac_u;
  void*        ptr_v = 0;

  fprintf(stderr, "pact: test roundtrip %u random packets\r\n", bat_w);

  for ( c3_w i_w = 0; i_w < bat_w; i_w++ ) {
    _test_make_pact(ptr_v, &pac_u);

    if ( _test_pact(&pac_u) ) {
      fprintf(stderr, RED_TEXT "pact: roundtrip attempt %u failed\r\n", i_w);
      exit(1);
    }

    // XX free pat_s / buf_y
  }

  return 0;
}

static void
_test_sift_page()
{
  u3_xmas_pact pac_u;
  memset(&pac_u,0, sizeof(u3_xmas_pact));
  pac_u.hed_u.typ_y = PACT_PAGE;
  pac_u.hed_u.pro_y = 1;
  u3l_log("%%page checking sift/etch idempotent");
  u3_xmas_name* nam_u = &pac_u.pag_u.nam_u;

  {
    u3_noun her = u3v_wish("~hastuc-dibtux");
    u3r_chubs(0, 2, nam_u->her_d, her);
    u3z(her);
  }
  nam_u->rif_w = 15;
  nam_u->pat_c = "foo/bar";
  nam_u->pat_s = strlen(nam_u->pat_c);
  nam_u->boq_y = 13;
  nam_u->fra_w = 54;
  nam_u->nit_o = c3n;

  u3_xmas_data* dat_u = &pac_u.pag_u.dat_u;
  dat_u->aum_u.typ_e = AUTH_NONE;
  dat_u->tot_w = 1000;
  dat_u->len_w = 1024;
  dat_u->fra_y = c3_calloc(1024);
  // dat_u->fra_y[1023] = 1;

  if ( _test_pact(&pac_u) ) {
    fprintf(stderr, RED_TEXT "%%page failed\r\n");
    exit(1);
  }
}


static void
_test_encode_path(c3_c* pat_c)
{
  u3_noun wan = u3do("stab", u3dt("cat", 3, '/', u3i_string(pat_c)));
  u3_noun hav = u3_xmas_encode_path(strlen(pat_c), (c3_y*)pat_c);

  if ( c3n == u3r_sing(wan, hav) ) {
    u3l_log(RED_TEXT);
    u3l_log("path encoding mismatch");
    u3m_p("want", wan);
    u3m_p("have", hav);
    exit(1);
  }
}


static void
_setup()
{
  c3_d          len_d = u3_Ivory_pill_len;
  c3_y*         byt_y = u3_Ivory_pill;
  u3_cue_xeno*  sil_u;
  u3_weak       pil;

  u3C.wag_w |= u3o_hashless;
  u3m_boot_lite(1 << 26);
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

  {
    c3_w pid_w = getpid();
    srand(pid_w);
    fprintf(stderr, "test: seeding rand() with pid %u\r\n", pid_w);
  }
}

int main()
{
  _setup();

  _test_rand_pact(100000);

  _test_encode_path("foo/bar/baz");
  _test_encode_path("publ/0/xx//1/foo/g");
  return 0;
}
