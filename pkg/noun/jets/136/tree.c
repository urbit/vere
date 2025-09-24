#include "c3/c3.h"
#include "jets.h"
#include "jets/w.h"


static c3_c* no_hashes[] = { 0 };

static u3j_harm _136_hex_mimes_base16_en_a[] = {{".2", u3we_en_base16}, {}};
static u3j_harm _136_hex_mimes_base16_de_a[] = {{".2", u3we_de_base16}, {}};
static u3j_core _136_hex_mimes_base16_d[] =
  { { "en", 7, _136_hex_mimes_base16_en_a, 0, no_hashes },
    { "de", 7, _136_hex_mimes_base16_de_a, 0, no_hashes },
    {}
  };
static u3j_core _136_hex_mimes_d[] =
  { { "base16", 3, 0, _136_hex_mimes_base16_d, no_hashes },
    {}
  };

static u3j_harm _136_hex_aes_ecba_en_a[] = {{".2", u3wea_ecba_en}, {}};
static u3j_harm _136_hex_aes_ecba_de_a[] = {{".2", u3wea_ecba_de}, {}};
static u3j_core _136_hex_aes_ecba_d[] =
  { { "en", 7, _136_hex_aes_ecba_en_a, 0, no_hashes },
    { "de", 7, _136_hex_aes_ecba_de_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_aes_ecbb_en_a[] = {{".2", u3wea_ecbb_en}, {}};
static u3j_harm _136_hex_aes_ecbb_de_a[] = {{".2", u3wea_ecbb_de}, {}};
static u3j_core _136_hex_aes_ecbb_d[] =
  { { "en", 7, _136_hex_aes_ecbb_en_a, 0, no_hashes },
    { "de", 7, _136_hex_aes_ecbb_de_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_aes_ecbc_en_a[] = {{".2", u3wea_ecbc_en}, {}};
static u3j_harm _136_hex_aes_ecbc_de_a[] = {{".2", u3wea_ecbc_de}, {}};
static u3j_core _136_hex_aes_ecbc_d[] =
  { { "en", 7, _136_hex_aes_ecbc_en_a, 0, no_hashes },
    { "de", 7, _136_hex_aes_ecbc_de_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_aes_cbca_en_a[] = {{".2", u3wea_cbca_en}, {}};
static u3j_harm _136_hex_aes_cbca_de_a[] = {{".2", u3wea_cbca_de}, {}};
static u3j_core _136_hex_aes_cbca_d[] =
  { { "en", 7, _136_hex_aes_cbca_en_a, 0, no_hashes },
    { "de", 7, _136_hex_aes_cbca_de_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_aes_cbcb_en_a[] = {{".2", u3wea_cbcb_en}, {}};
static u3j_harm _136_hex_aes_cbcb_de_a[] = {{".2", u3wea_cbcb_de}, {}};
static u3j_core _136_hex_aes_cbcb_d[] =
  { { "en", 7, _136_hex_aes_cbcb_en_a, 0, no_hashes },
    { "de", 7, _136_hex_aes_cbcb_de_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_aes_cbcc_en_a[] = {{".2", u3wea_cbcc_en}, {}};
static u3j_harm _136_hex_aes_cbcc_de_a[] = {{".2", u3wea_cbcc_de}, {}};
static u3j_core _136_hex_aes_cbcc_d[] =
  { { "en", 7, _136_hex_aes_cbcc_en_a, 0, no_hashes },
    { "de", 7, _136_hex_aes_cbcc_de_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_aes_siva_en_a[] = {{".2", u3wea_siva_en}, {}};
static u3j_harm _136_hex_aes_siva_de_a[] = {{".2", u3wea_siva_de}, {}};
static u3j_core _136_hex_aes_siva_d[] =
  { { "en", 7, _136_hex_aes_siva_en_a, 0, no_hashes },
    { "de", 7, _136_hex_aes_siva_de_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_aes_sivb_en_a[] = {{".2", u3wea_sivb_en}, {}};
static u3j_harm _136_hex_aes_sivb_de_a[] = {{".2", u3wea_sivb_de}, {}};
static u3j_core _136_hex_aes_sivb_d[] =
  { { "en", 7, _136_hex_aes_sivb_en_a, 0, no_hashes },
    { "de", 7, _136_hex_aes_sivb_de_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_aes_sivc_en_a[] = {{".2", u3wea_sivc_en}, {}};
static u3j_harm _136_hex_aes_sivc_de_a[] = {{".2", u3wea_sivc_de}, {}};
static u3j_core _136_hex_aes_sivc_d[] =
  { { "en", 7, _136_hex_aes_sivc_en_a, 0, no_hashes },
    { "de", 7, _136_hex_aes_sivc_de_a, 0, no_hashes },
    {}
  };
static u3j_core _136_hex_aes_d[] =
  { { "ecba", 7, 0, _136_hex_aes_ecba_d, no_hashes },
    { "ecbb", 7, 0, _136_hex_aes_ecbb_d, no_hashes },
    { "ecbc", 7, 0, _136_hex_aes_ecbc_d, no_hashes },
    { "cbca", 7, 0, _136_hex_aes_cbca_d, no_hashes },
    { "cbcb", 7, 0, _136_hex_aes_cbcb_d, no_hashes },
    { "cbcc", 7, 0, _136_hex_aes_cbcc_d, no_hashes },
    { "siva", 7, 0, _136_hex_aes_siva_d, no_hashes },
    { "sivb", 7, 0, _136_hex_aes_sivb_d, no_hashes },
    { "sivc", 7, 0, _136_hex_aes_sivc_d, no_hashes },
    {}
  };

static u3j_harm _136_hex_leer_a[] = {{".2", u3we_leer}, {}};
static u3j_harm _136_hex_lore_a[] = {{".2", u3we_lore}, {}};
static u3j_harm _136_hex_loss_a[] = {{".2", u3we_loss}, {}};
static u3j_harm _136_hex_lune_a[] = {{".2", u3we_lune}, {}};


static u3j_harm _136_hex__adler32_a[] = {{".2", u3we_adler32, c3y}, {}};
static u3j_core  _136_hex__adler_d[] =
  { { "adler32", 7, _136_hex__adler32_a, 0, no_hashes },
    {}
  };
static u3j_harm _136_hex__crc32_a[] = {{".2", u3we_crc32}, {}};
static u3j_core _136_hex__crc_d[] =
  { {"crc32", 7, _136_hex__crc32_a, 0, no_hashes },
    {}
  };

static u3j_core _136_hex_checksum_d[] =
  { { "adler", 3, 0, _136_hex__adler_d, no_hashes },
    { "crc", 3, 0, _136_hex__crc_d, no_hashes},
    {}
  };


static u3j_harm _136_hex__decompress_zlib_a[] = {{".2", u3we_decompress_zlib}, {}};
static u3j_harm _136_hex__decompress_gzip_a[] = {{".2", u3we_decompress_gzip}, {}};
static u3j_core _136_hex__zlib_d[] = {
  {"decompress-zlib", 7, _136_hex__decompress_zlib_a, 0, no_hashes },
  {"decompress-gzip", 7, _136_hex__decompress_gzip_a, 0, no_hashes },
  {}};


static u3j_harm _136_hex_coed__ed_scad_a[] = {{".2", u3wee_scad}, {}};
static u3j_harm _136_hex_coed__ed_scas_a[] = {{".2", u3wee_scas}, {}};
static u3j_harm _136_hex_coed__ed_scap_a[] = {{".2", u3wee_scap}, {}};

static u3j_harm _136_hex_coed__ed_puck_a[] = {{".2", u3wee_puck}, {}};
static u3j_harm _136_hex_coed__ed_luck_a[] = {{".2", u3wee_luck}, {}};
static u3j_harm _136_hex_coed__ed_sign_a[] = {{".2", u3wee_sign}, {}};
static u3j_harm _136_hex_coed__ed_sign_raw_a[] = {{".2", u3wee_sign_raw}, {}};
static u3j_harm _136_hex_coed__ed_sign_octs_a[] = {{".2", u3wee_sign_octs}, {}};
static u3j_harm _136_hex_coed__ed_sign_octs_raw_a[] = {{".2", u3wee_sign_octs_raw}, {}};
static u3j_harm _136_hex_coed__ed_veri_octs_a[] = {{".2", u3wee_veri_octs}, {}};
static u3j_harm _136_hex_coed__ed_veri_a[] = {{".2", u3wee_veri}, {}};
static u3j_harm _136_hex_coed__ed_shar_a[] = {{".2", u3wee_shar}, {}};
static u3j_harm _136_hex_coed__ed_slar_a[] = {{".2", u3wee_slar}, {}};

static u3j_harm _136_hex_coed__ed_smac_a[] =
  {{".2", u3wee_smac}, {}};

static u3j_harm _136_hex_coed__ed_recs_a[] =
  {{".2", u3wee_recs}, {}};

static u3j_harm _136_hex_coed__ed_point_neg_a[] =
  {{".2", u3wee_point_neg}, {}};

static u3j_harm _136_hex_coed__ed_point_add_a[] =
  {{".2", u3wee_point_add}, {}};

static u3j_harm _136_hex_coed__ed_scalarmult_a[] =
  {{".2", u3wee_scalarmult}, {}};

static u3j_harm _136_hex_coed__ed_scalarmult_base_a[] =
  {{".2", u3wee_scalarmult_base}, {}};

static u3j_harm _136_hex_coed__ed_add_scalarmult_scalarmult_base_a[] =
  {{".2", u3wee_add_scalarmult_scalarmult_base}, {}};

static u3j_harm _136_hex_coed__ed_add_double_scalarmult_a[] =
  {{".2", u3wee_add_double_scalarmult}, {}};

static u3j_core _136_hex_coed__ed_d[] =
  { { "sign", 7, _136_hex_coed__ed_sign_a, 0, no_hashes },
    { "sign-raw", 7, _136_hex_coed__ed_sign_raw_a, 0, no_hashes },
    { "sign-octs", 7, _136_hex_coed__ed_sign_octs_a, 0, no_hashes },
    { "sign-octs-raw", 7, _136_hex_coed__ed_sign_octs_raw_a, 0, no_hashes },
    { "puck", 7, _136_hex_coed__ed_puck_a, 0, no_hashes },
    { "luck", 7, _136_hex_coed__ed_luck_a, 0, no_hashes },
    { "scad", 7, _136_hex_coed__ed_scad_a, 0, no_hashes },
    { "scas", 7, _136_hex_coed__ed_scas_a, 0, no_hashes },
    { "scap", 7, _136_hex_coed__ed_scap_a, 0, no_hashes },
    { "veri-octs", 7, _136_hex_coed__ed_veri_octs_a, 0, no_hashes },
    { "veri", 7, _136_hex_coed__ed_veri_a, 0, no_hashes },
    { "shar", 7, _136_hex_coed__ed_shar_a, 0, no_hashes },
    { "slar", 7, _136_hex_coed__ed_slar_a, 0, no_hashes },
    { "point-add", 7, _136_hex_coed__ed_point_add_a, 0, 0 },
    { "point-neg", 7, _136_hex_coed__ed_point_neg_a, 0, 0 },
    { "recs", 7, _136_hex_coed__ed_recs_a, 0, 0 },
    { "smac", 7, _136_hex_coed__ed_smac_a, 0, 0 },
    { "scalarmult", 7, _136_hex_coed__ed_scalarmult_a, 0,
      no_hashes },
    { "scalarmult-base", 7, _136_hex_coed__ed_scalarmult_base_a, 0,
      no_hashes },
    { "add-scalarmult-scalarmult-base", 7,
      _136_hex_coed__ed_add_scalarmult_scalarmult_base_a, 0,
      no_hashes },
    { "add-double-scalarmult", 7,
      _136_hex_coed__ed_add_double_scalarmult_a, 0,
      no_hashes },
    {}
  };

static u3j_core _136_hex_coed_d[] =
  { { "ed", 3, 0, _136_hex_coed__ed_d, no_hashes },
    {}
  };

static u3j_harm _136_hex_hmac_hmac_a[] = {{".2", u3we_hmac}, {}};
static u3j_core _136_hex_hmac_d[] =
  { { "hmac", 7, _136_hex_hmac_hmac_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_argon2_a[] = {{".2", u3we_argon2}, {}};
static u3j_core _136_hex_argon_d[] =
  { { "argon2", 511, _136_hex_argon2_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_scr_pbk_a[] = {{".2", u3wes_pbk, c3y}, {}};
static u3j_harm _136_hex_scr_pbl_a[] = {{".2", u3wes_pbl, c3y}, {}};
static u3j_harm _136_hex_scr_hsh_a[] = {{".2", u3wes_hsh, c3y}, {}};
static u3j_harm _136_hex_scr_hsl_a[] = {{".2", u3wes_hsl, c3y}, {}};
static u3j_core _136_hex_scr_d[] =
  { { "pbk", 7, _136_hex_scr_pbk_a, 0, no_hashes },
    { "pbl", 7, _136_hex_scr_pbl_a, 0, no_hashes },
    { "hsh", 7, _136_hex_scr_hsh_a, 0, no_hashes },
    { "hsl", 7, _136_hex_scr_hsl_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_secp_secp256k1_make_a[] = {{".2", u3we_make, c3y}, {}};
static u3j_harm _136_hex_secp_secp256k1_sign_a[] = {{".2", u3we_sign, c3y}, {}};
static u3j_harm _136_hex_secp_secp256k1_reco_a[] = {{".2", u3we_reco, c3y}, {}};

static u3j_harm _136_hex_secp_secp256k1_schnorr_sosi_a[] =
  {{".2", u3we_sosi}, {}};
static u3j_harm _136_hex_secp_secp256k1_schnorr_sove_a[] =
  {{".2", u3we_sove}, {}};
static u3j_core _136_hex_secp_secp256k1_schnorr_d[] =
  { { "sosi", 7,
      _136_hex_secp_secp256k1_schnorr_sosi_a, 0,
      no_hashes },
    { "sove", 7,
      _136_hex_secp_secp256k1_schnorr_sove_a, 0,
      no_hashes },
    {}
  };

static u3j_core _136_hex_secp_secp256k1_d[] =
  { { "make", 7, _136_hex_secp_secp256k1_make_a, 0, no_hashes },
    { "sign", 7, _136_hex_secp_secp256k1_sign_a, 0, no_hashes },
    { "reco", 7, _136_hex_secp_secp256k1_reco_a, 0, no_hashes },
    { "schnorr", 7, 0,
      _136_hex_secp_secp256k1_schnorr_d,
      no_hashes },
    {}
  };
static u3j_core _136_hex_secp_d[] =
  { { "secp256k1", 3, 0, _136_hex_secp_secp256k1_d, no_hashes },
    {}
  };


static u3j_harm _136_hex_kecc_k224_a[] =
  {{".2", u3we_kecc224, c3y, c3y, c3y}, {}};
static u3j_harm _136_hex_kecc_k256_a[] =
  {{".2", u3we_kecc256, c3y, c3y, c3y}, {}};
static u3j_harm _136_hex_kecc_k384_a[] =
  {{".2", u3we_kecc384, c3y, c3y, c3y}, {}};
static u3j_harm _136_hex_kecc_k512_a[] =
  {{".2", u3we_kecc512, c3y, c3y, c3y}, {}};
static u3j_core _136_hex_kecc_d[] =
  { { "k224", 7, _136_hex_kecc_k224_a, 0, no_hashes },
    { "k256", 7, _136_hex_kecc_k256_a, 0, no_hashes },
    { "k384", 7, _136_hex_kecc_k384_a, 0, no_hashes },
    { "k512", 7, _136_hex_kecc_k512_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_ripemd_160_a[] = {{".2", u3we_ripe, c3y}, {}};
static u3j_core _136_hex_ripe_d[] =
  { { "ripemd160", 7, _136_hex_ripemd_160_a, 0, no_hashes },
    {}
  };



/* layer five
 */
static u3j_harm _136_pen_cell_a[] = {{".2", u3wf_cell}, {}};
static u3j_harm _136_pen_comb_a[] = {{".2", u3wf_comb}, {}};
static u3j_harm _136_pen_cons_a[] = {{".2", u3wf_cons}, {}};
static u3j_harm _136_pen_core_a[] = {{".2", u3wf_core}, {}};
static u3j_harm _136_pen_face_a[] = {{".2", u3wf_face}, {}};
static u3j_harm _136_pen_fitz_a[] = {{".2", u3wf_fitz}, {}};
static u3j_harm _136_pen_fork_a[] = {{".2", u3wf_fork}, {}};

static u3j_harm _136_pen_look_a[] = {{".2", u3wf_look}, {}};
static u3j_harm _136_pen_loot_a[] = {{".2", u3wf_loot}, {}};

static u3j_harm _136_pen__ut_crop_a[] = {{".2", u3wfu_crop}, {}};
static u3j_harm _136_pen__ut_fish_a[] = {{".2", u3wfu_fish}, {}};
static u3j_harm _136_pen__ut_fuse_a[] = {{".2", u3wfu_fuse}, {}};
static u3j_harm _136_pen__ut_redo_a[] = {{".2", u3wfu_redo}, {}};
static u3j_harm _136_pen__ut_mint_a[] = {{".2", u3wfu_mint}, {}};
static u3j_harm _136_pen__ut_mull_a[] = {{".2", u3wfu_mull}, {}};

static u3j_harm _136_pen__ut_nest_dext_a[] = {{".2", u3wfu_nest_dext}, {}};
static u3j_core _136_pen__ut_nest_in_d[] =
  {
    { "nest-dext", 3, _136_pen__ut_nest_dext_a, 0, no_hashes },
    {}
  };
static u3j_core _136_pen__ut_nest_d[] =
  {
    { "nest-in", 7, 0, _136_pen__ut_nest_in_d, no_hashes },
    {}
  };

static u3j_harm _136_pen__ut_rest_a[] = {{".2", u3wfu_rest}, {}};

static u3j_core _136_pen__ut_d[] =
  {
    { "crop", 7, _136_pen__ut_crop_a, 0, no_hashes },
    { "fish", 7, _136_pen__ut_fish_a, 0, no_hashes },
    { "fuse", 7, _136_pen__ut_fuse_a, 0, no_hashes },
    { "redo", 7, _136_pen__ut_redo_a, 0, no_hashes },
    { "mint", 7, _136_pen__ut_mint_a, 0, no_hashes },
    { "mull", 7, _136_pen__ut_mull_a, 0, no_hashes },
    { "nest", 7, 0, _136_pen__ut_nest_d, no_hashes },
    { "rest", 7, _136_pen__ut_rest_a, 0, no_hashes },
    {}
  };

static u3j_hood _136_pen__ut_ho[] =
  { { "ar",     12282 },
    { "fan",       28, c3n },
    { "rib",       58, c3n },
    { "vet",       59, c3n },

    { "blow",    6015 },
    { "burp",     342 },
    { "busk",    1363 },
    { "buss",     374 },
    { "crop",    1494 },
    { "duck",    1524 },
    { "dune",    2991 },
    { "dunk",    3066 },
    { "epla",   12206 },
    { "emin",    1534 },
    { "emul",    6134 },
    { "feel",    1502 },
    { "felt",      94 },
    { "fine",   49086 },
    { "fire",       4 },
    { "fish",    6006 },
    { "fond",   12283 },
    { "fund",    6014 },
    //  XX +funk is not part of +ut, and this hook appears to be unused
    //  remove from here and the +ut hint
    //
    { "funk", 0xbefafa, c3y, 31 },
    { "fuse",    24021 },
    { "gain",      380 },
    { "lose",  0x2fefe },
    { "mile",      382 },
    { "mine",      372 },
    { "mint",    49083 },
    { "moot",  0x2feff },
    { "mull",    24020 },
    { "nest",       92 },
    { "peel",     1526 },
    { "play",     3006 },
    { "peek",     1532 },
    { "repo",       22 },
    { "rest",     6102 },
    { "tack",     6007 },
    { "toss",    24540 },
    { "wrap",     6136 },
    {},
  };


static u3j_hood _136_pen_ho[] = {
  { "ap", 22 },
  { "ut", 86 },
  {},
};


/* layer four
 */
static u3j_harm _136_qua_trip_a[] = {{".2", u3we_trip}, {}};

static u3j_harm _136_qua_slaw_a[] = {{".2", u3we_slaw}, {}};
static u3j_harm _136_qua_scot_a[] = {{".2", u3we_scot}, {}};
static u3j_harm _136_qua_scow_a[] = {{".2", u3we_scow}, {}};

static u3j_harm _136_qua__po_ind_a[] = {{".2", u3wcp_ind}, {}};
static u3j_harm _136_qua__po_ins_a[] = {{".2", u3wcp_ins}, {}};
static u3j_harm _136_qua__po_tod_a[] = {{".2", u3wcp_tod}, {}};
static u3j_harm _136_qua__po_tos_a[] = {{".2", u3wcp_tos}, {}};
static u3j_core _136_qua__po_d[] =
  { { "ind", 7, _136_qua__po_ind_a, 0, no_hashes },
    { "ins", 7, _136_qua__po_ins_a, 0, no_hashes },
    { "tod", 7, _136_qua__po_tod_a, 0, no_hashes },
    { "tos", 7, _136_qua__po_tos_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__bend_fun_a[] = {{".2", u3we_bend_fun}, {}};
static u3j_core _136_qua__bend_d[] =
  { { "fun", 7, _136_qua__bend_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__cold_fun_a[] = {{".2", u3we_cold_fun}, {}};
static u3j_core _136_qua__cold_d[] =
  { { "fun", 7, _136_qua__cold_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__cook_fun_a[] = {{".2", u3we_cook_fun}, {}};
static u3j_core _136_qua__cook_d[] =
  { { "fun", 7, _136_qua__cook_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__comp_fun_a[] = {{".2", u3we_comp_fun}, {}};
static u3j_core _136_qua__comp_d[] =
  { { "fun", 7, _136_qua__comp_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__easy_fun_a[] = {{".2", u3we_easy_fun}, {}};
static u3j_core _136_qua__easy_d[] =
  { { "fun", 7, _136_qua__easy_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__glue_fun_a[] = {{".2", u3we_glue_fun}, {}};
static u3j_core _136_qua__glue_d[] =
  { { "fun", 7, _136_qua__glue_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__here_fun_a[] = {{".2", u3we_here_fun}, {}};
static u3j_core _136_qua__here_d[] =
  { { "fun", 7, _136_qua__here_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__just_fun_a[] = {{".2", u3we_just_fun}, {}};
static u3j_core _136_qua__just_d[] =
  { { "fun", 7, _136_qua__just_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__mask_fun_a[] = {{".2", u3we_mask_fun}, {}};
static u3j_core _136_qua__mask_d[] =
  { { "fun", 7, _136_qua__mask_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__shim_fun_a[] = {{".2", u3we_shim_fun}, {}};
static u3j_core _136_qua__shim_d[] =
  { { "fun", 7, _136_qua__shim_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__stag_fun_a[] = {{".2", u3we_stag_fun}, {}};
static u3j_core _136_qua__stag_d[] =
  { { "fun", 7, _136_qua__stag_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__stew_fun_a[] = {{".2", u3we_stew_fun}, {}};
static u3j_core _136_qua__stew_d[] =
  { { "fun", 31, _136_qua__stew_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua__stir_fun_a[] = {{".2", u3we_stir_fun}, {}};
static u3j_core _136_qua__stir_d[] =
  { { "fun", 7, _136_qua__stir_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_qua_pfix_a[] = {{".2", u3we_pfix}, {}};

static u3j_harm _136_qua_plug_a[] = {{".2", u3we_plug}, {}};
static u3j_harm _136_qua_pose_a[] = {{".2", u3we_pose}, {}};

static u3j_harm _136_qua_sfix_a[] = {{".2", u3we_sfix}, {}};

static u3j_harm _136_qua_mink_a[] = {{".2", u3we_mink}, {}};
static u3j_harm _136_qua_mole_a[] = {{".2", u3we_mole}, {}};
static u3j_harm _136_qua_mule_a[] = {{".2", u3we_mule}, {}};


static u3j_hood _136_qua_ho[] = {
  { "show",    188 },
  {},
};


/* layer three
 */
static u3j_harm _136_tri__cofl__drg_a[] = {{".2", u3wef_drg}, {}};
static u3j_harm _136_tri__cofl__lug_a[] = {{".2", u3wef_lug}, {}};
static u3j_core _136_tri__cofl_d[] =
  { { "drg", 7, _136_tri__cofl__drg_a, 0, no_hashes },
    { "lug", 7, _136_tri__cofl__lug_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_tri__rd_add_a[] = {{".2", u3wer_add}, {}};
static u3j_harm _136_tri__rd_sub_a[] = {{".2", u3wer_sub}, {}};
static u3j_harm _136_tri__rd_mul_a[] = {{".2", u3wer_mul}, {}};
static u3j_harm _136_tri__rd_div_a[] = {{".2", u3wer_div}, {}};
static u3j_harm _136_tri__rd_sqt_a[] = {{".2", u3wer_sqt}, {}};
static u3j_harm _136_tri__rd_fma_a[] = {{".2", u3wer_fma}, {}};
static u3j_harm _136_tri__rd_lth_a[] = {{".2", u3wer_lth}, {}};
static u3j_harm _136_tri__rd_lte_a[] = {{".2", u3wer_lte}, {}};
static u3j_harm _136_tri__rd_equ_a[] = {{".2", u3wer_equ}, {}};
static u3j_harm _136_tri__rd_gte_a[] = {{".2", u3wer_gte}, {}};
static u3j_harm _136_tri__rd_gth_a[] = {{".2", u3wer_gth}, {}};
static u3j_core _136_tri__rd_d[] =
  { { "add", 7, _136_tri__rd_add_a, 0, no_hashes },
    { "sub", 7, _136_tri__rd_sub_a, 0, no_hashes },
    { "mul", 7, _136_tri__rd_mul_a, 0, no_hashes },
    { "div", 7, _136_tri__rd_div_a, 0, no_hashes },
    { "sqt", 7, _136_tri__rd_sqt_a, 0, no_hashes },
    { "fma", 7, _136_tri__rd_fma_a, 0, no_hashes },
    { "lth", 7, _136_tri__rd_lth_a, 0, no_hashes },
    { "lte", 7, _136_tri__rd_lte_a, 0, no_hashes },
    { "equ", 7, _136_tri__rd_equ_a, 0, no_hashes },
    { "gte", 7, _136_tri__rd_gte_a, 0, no_hashes },
    { "gth", 7, _136_tri__rd_gth_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_tri__rs_add_a[] = {{".2", u3wet_add}, {}};
static u3j_harm _136_tri__rs_sub_a[] = {{".2", u3wet_sub}, {}};
static u3j_harm _136_tri__rs_mul_a[] = {{".2", u3wet_mul}, {}};
static u3j_harm _136_tri__rs_div_a[] = {{".2", u3wet_div}, {}};
static u3j_harm _136_tri__rs_sqt_a[] = {{".2", u3wet_sqt}, {}};
static u3j_harm _136_tri__rs_fma_a[] = {{".2", u3wet_fma}, {}};
static u3j_harm _136_tri__rs_lth_a[] = {{".2", u3wet_lth}, {}};
static u3j_harm _136_tri__rs_lte_a[] = {{".2", u3wet_lte}, {}};
static u3j_harm _136_tri__rs_equ_a[] = {{".2", u3wet_equ}, {}};
static u3j_harm _136_tri__rs_gte_a[] = {{".2", u3wet_gte}, {}};
static u3j_harm _136_tri__rs_gth_a[] = {{".2", u3wet_gth}, {}};
static u3j_core _136_tri__rs_d[] =
  { { "add", 7, _136_tri__rs_add_a, 0, no_hashes },
    { "sub", 7, _136_tri__rs_sub_a, 0, no_hashes },
    { "mul", 7, _136_tri__rs_mul_a, 0, no_hashes },
    { "div", 7, _136_tri__rs_div_a, 0, no_hashes },
    { "sqt", 7, _136_tri__rs_sqt_a, 0, no_hashes },
    { "fma", 7, _136_tri__rs_fma_a, 0, no_hashes },
    { "lth", 7, _136_tri__rs_lth_a, 0, no_hashes },
    { "lte", 7, _136_tri__rs_lte_a, 0, no_hashes },
    { "equ", 7, _136_tri__rs_equ_a, 0, no_hashes },
    { "gte", 7, _136_tri__rs_gte_a, 0, no_hashes },
    { "gth", 7, _136_tri__rs_gth_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_tri__rq_add_a[] = {{".2", u3weq_add}, {}};
static u3j_harm _136_tri__rq_sub_a[] = {{".2", u3weq_sub}, {}};
static u3j_harm _136_tri__rq_mul_a[] = {{".2", u3weq_mul}, {}};
static u3j_harm _136_tri__rq_div_a[] = {{".2", u3weq_div}, {}};
static u3j_harm _136_tri__rq_sqt_a[] = {{".2", u3weq_sqt}, {}};
static u3j_harm _136_tri__rq_fma_a[] = {{".2", u3weq_fma}, {}};
static u3j_harm _136_tri__rq_lth_a[] = {{".2", u3weq_lth}, {}};
static u3j_harm _136_tri__rq_lte_a[] = {{".2", u3weq_lte}, {}};
static u3j_harm _136_tri__rq_equ_a[] = {{".2", u3weq_equ}, {}};
static u3j_harm _136_tri__rq_gte_a[] = {{".2", u3weq_gte}, {}};
static u3j_harm _136_tri__rq_gth_a[] = {{".2", u3weq_gth}, {}};
static u3j_core _136_tri__rq_d[] =
  { { "add", 7, _136_tri__rq_add_a, 0, no_hashes },
    { "sub", 7, _136_tri__rq_sub_a, 0, no_hashes },
    { "mul", 7, _136_tri__rq_mul_a, 0, no_hashes },
    { "div", 7, _136_tri__rq_div_a, 0, no_hashes },
    { "sqt", 7, _136_tri__rq_sqt_a, 0, no_hashes },
    { "fma", 7, _136_tri__rq_fma_a, 0, no_hashes },
    { "lth", 7, _136_tri__rq_lth_a, 0, no_hashes },
    { "lte", 7, _136_tri__rq_lte_a, 0, no_hashes },
    { "equ", 7, _136_tri__rq_equ_a, 0, no_hashes },
    { "gte", 7, _136_tri__rq_gte_a, 0, no_hashes },
    { "gth", 7, _136_tri__rq_gth_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_tri__rh_add_a[] = {{".2", u3wes_add}, {}};
static u3j_harm _136_tri__rh_sub_a[] = {{".2", u3wes_sub}, {}};
static u3j_harm _136_tri__rh_mul_a[] = {{".2", u3wes_mul}, {}};
static u3j_harm _136_tri__rh_div_a[] = {{".2", u3wes_div}, {}};
static u3j_harm _136_tri__rh_sqt_a[] = {{".2", u3wes_sqt}, {}};
static u3j_harm _136_tri__rh_fma_a[] = {{".2", u3wes_fma}, {}};
static u3j_harm _136_tri__rh_lth_a[] = {{".2", u3wes_lth}, {}};
static u3j_harm _136_tri__rh_lte_a[] = {{".2", u3wes_lte}, {}};
static u3j_harm _136_tri__rh_equ_a[] = {{".2", u3wes_equ}, {}};
static u3j_harm _136_tri__rh_gte_a[] = {{".2", u3wes_gte}, {}};
static u3j_harm _136_tri__rh_gth_a[] = {{".2", u3wes_gth}, {}};
static u3j_core _136_tri__rh_d[] =
  { { "add", 7, _136_tri__rh_add_a, 0, no_hashes },
    { "sub", 7, _136_tri__rh_sub_a, 0, no_hashes },
    { "mul", 7, _136_tri__rh_mul_a, 0, no_hashes },
    { "div", 7, _136_tri__rh_div_a, 0, no_hashes },
    { "sqt", 7, _136_tri__rh_sqt_a, 0, no_hashes },
    { "fma", 7, _136_tri__rh_fma_a, 0, no_hashes },
    { "lth", 7, _136_tri__rh_lth_a, 0, no_hashes },
    { "lte", 7, _136_tri__rh_lte_a, 0, no_hashes },
    { "equ", 7, _136_tri__rh_equ_a, 0, no_hashes },
    { "gte", 7, _136_tri__rh_gte_a, 0, no_hashes },
    { "gth", 7, _136_tri__rh_gth_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_tri__og_raw_a[] = {{".2", u3weo_raw}, {}};
static u3j_core _136_tri__og_d[] =
  { { "raw", 7, _136_tri__og_raw_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_tri__sha_sha1_a[] = {{".2", u3we_sha1}, {}};
static u3j_core _136_tri__sha_d[] =
  { { "sha1", 7, _136_tri__sha_sha1_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_tri_shax_a[] = {{".2", u3we_shax}, {}};
static u3j_harm _136_tri_shay_a[] = {{".2", u3we_shay}, {}};
static u3j_harm _136_tri_shas_a[] = {{".2", u3we_shas}, {}};
static u3j_harm _136_tri_shal_a[] = {{".2", u3we_shal}, {}};

static u3j_harm _136_ob_fynd_a[] = {{".2", u3we_fynd_ob}, {}};
static u3j_harm _136_ob_fein_a[] = {{".2", u3we_fein_ob}, {}};
static u3j_core _136_ob_d[] = {
  { "fein", 7, _136_ob_fein_a, 0, no_hashes },
  { "fynd", 7, _136_ob_fynd_a, 0, no_hashes },
  {}
};
static u3j_hood _136_ob_ho[] = {
  { "fein",  42 },
  { "fynd",  20 },
  {},
};


static u3j_hood _136_tri_ho[] = {
  { "ob",      20 },
  { "yore",  5462 },
  { "year", 44975 },
  {},
};


/* layer two
 */
static u3j_harm _136_two_find_a[] = {{".2", u3wb_find, c3y}, {}};
static u3j_harm _136_two_flop_a[] = {{".2", u3wb_flop, c3y}, {}};
static u3j_harm _136_two_lent_a[] = {{".2", u3wb_lent, c3y}, {}};
static u3j_harm _136_two_levy_a[] = {{".2", u3wb_levy, c3y}, {}};
static u3j_harm _136_two_lien_a[] = {{".2", u3wb_lien, c3y}, {}};
static u3j_harm _136_two_murn_a[] = {{".2", u3wb_murn, c3y}, {}};
static u3j_harm _136_two_need_a[] = {{".2", u3wb_need, c3y}, {}};
static u3j_harm _136_two_reap_a[] = {{".2", u3wb_reap, c3y}, {}};
static u3j_harm _136_two_reel_a[] = {{".2", u3wb_reel, c3y}, {}};
static u3j_harm _136_two_roll_a[] = {{".2", u3wb_roll, c3y}, {}};
static u3j_harm _136_two_skid_a[] = {{".2", u3wb_skid, c3y}, {}};
static u3j_harm _136_two_skim_a[] = {{".2", u3wb_skim, c3y}, {}};
static u3j_harm _136_two_skip_a[] = {{".2", u3wb_skip, c3y}, {}};
static u3j_harm _136_two_scag_a[] = {{".2", u3wb_scag, c3y}, {}};
static u3j_harm _136_two_slag_a[] = {{".2", u3wb_slag, c3y}, {}};
static u3j_harm _136_two_snag_a[] = {{".2", u3wb_snag, c3y}, {}};
static u3j_harm _136_two_sort_a[] = {{".2", u3wb_sort, c3y}, {}};
static u3j_harm _136_two_turn_a[] = {{".2", u3wb_turn, c3y}, {}};
static u3j_harm _136_two_weld_a[] = {{".2", u3wb_weld, c3y}, {}};
static u3j_harm _136_two_welp_a[] = {{".2", u3wb_welp, c3y}, {}};
static u3j_harm _136_two_zing_a[] = {{".2", u3wb_zing, c3y}, {}};

static u3j_harm _136_two_bex_a[] = {{".2", u3wc_bex, c3y}, {}};
static u3j_harm _136_two_can_a[] = {{".2", u3wc_can, c3y}, {}};
static u3j_harm _136_two_cat_a[] = {{".2", u3wc_cat, c3y}, {}};
static u3j_harm _136_two_con_a[] = {{".2", u3wc_con, c3y}, {}};
static u3j_harm _136_two_cut_a[] = {{".2", u3wc_cut, c3y}, {}};
static u3j_harm _136_two_dis_a[] = {{".2", u3wc_dis, c3y}, {}};
static u3j_harm _136_two_dor_a[] = {{".2", u3wc_dor, c3y}, {}};
static u3j_harm _136_two_end_a[] = {{".2", u3wc_end, c3y}, {}};
static u3j_harm _136_two_gor_a[] = {{".2", u3wc_gor, c3y}, {}};
static u3j_harm _136_two_lsh_a[] = {{".2", u3wc_lsh, c3y}, {}};
static u3j_harm _136_two_met_a[] = {{".2", u3wc_met, c3y}, {}};
static u3j_harm _136_two_mix_a[] = {{".2", u3wc_mix, c3y}, {}};
static u3j_harm _136_two_mor_a[] = {{".2", u3wc_mor, c3y}, {}};
static u3j_harm _136_two_mug_a[] = {{".2", u3wc_mug, c3y}, {}};
static u3j_harm _136_two_muk_a[] = {{".2", u3wc_muk, c3y}, {}};
static u3j_harm _136_two_pow_a[] = {{".2", u3wc_pow, c3y}, {}};
static u3j_harm _136_two_rap_a[] = {{".2", u3wc_rap, c3y}, {}};
static u3j_harm _136_two_rep_a[] = {{".2", u3wc_rep, c3y}, {}};
static u3j_harm _136_two_rev_a[] = {{".2", u3wc_rev, c3y}, {}};
static u3j_harm _136_two_rip_a[] = {{".2", u3wc_rip, c3y}, {}};
static u3j_harm _136_two_rsh_a[] = {{".2", u3wc_rsh, c3y}, {}};
static u3j_harm _136_two_swp_a[] = {{".2", u3wc_swp, c3y}, {}};
static u3j_harm _136_two_sqt_a[] = {{".2", u3wc_sqt, c3y}, {}};
static u3j_harm _136_two_xeb_a[] = {{".2", u3wc_xeb, c3y}, {}};

static u3j_harm _136_two__in_bif_a[] = {{".2", u3wdi_bif}, {}};
static u3j_harm _136_two__in_del_a[] = {{".2", u3wdi_del}, {}};
static u3j_harm _136_two__in_dif_a[] = {{".2", u3wdi_dif}, {}};
static u3j_harm _136_two__in_gas_a[] = {{".2", u3wdi_gas}, {}};
static u3j_harm _136_two__in_has_a[] = {{".2", u3wdi_has}, {}};
static u3j_harm _136_two__in_int_a[] = {{".2", u3wdi_int}, {}};
static u3j_harm _136_two__in_put_a[] = {{".2", u3wdi_put}, {}};
static u3j_harm _136_two__in_rep_a[] = {{".2", u3wdi_rep}, {}};
static u3j_harm _136_two__in_run_a[] = {{".2", u3wdi_run}, {}};
static u3j_harm _136_two__in_tap_a[] = {{".2", u3wdi_tap}, {}};
static u3j_harm _136_two__in_wyt_a[] = {{".2", u3wdi_wyt}, {}};
static u3j_harm _136_two__in_uni_a[] = {{".2", u3wdi_uni}, {}};

static u3j_harm _136_two__by_all_a[] = {{".2", u3wdb_all, c3y}, {}};
static u3j_harm _136_two__by_any_a[] = {{".2", u3wdb_any, c3y}, {}};
static u3j_harm _136_two__by_apt_a[] = {{".2", u3wdb_apt, c3y}, {}};

static u3j_harm _136_two__by_del_a[] = {{".2", u3wdb_del, c3y}, {}};
static u3j_harm _136_two__by_dif_a[] = {{".2", u3wdb_dif, c3y}, {}};
static u3j_harm _136_two__by_gas_a[] = {{".2", u3wdb_gas, c3y}, {}};
static u3j_harm _136_two__by_get_a[] = {{".2", u3wdb_get, c3y}, {}};
static u3j_harm _136_two__by_has_a[] = {{".2", u3wdb_has, c3y}, {}};
static u3j_harm _136_two__by_int_a[] = {{".2", u3wdb_int, c3y}, {}};
static u3j_harm _136_two__by_jab_a[] = {{".2", u3wdb_jab, c3y}, {}};
static u3j_harm _136_two__by_key_a[] = {{".2", u3wdb_key, c3y}, {}};
static u3j_harm _136_two__by_put_a[] = {{".2", u3wdb_put, c3y}, {}};
static u3j_harm _136_two__by_rep_a[] = {{".2", u3wdb_rep, c3y}, {}};
static u3j_harm _136_two__by_run_a[] = {{".2", u3wdb_run, c3y}, {}};
static u3j_harm _136_two__by_tap_a[] = {{".2", u3wdb_tap, c3y}, {}};
static u3j_harm _136_two__by_uni_a[] = {{".2", u3wdb_uni, c3y}, {}};
static u3j_harm _136_two__by_urn_a[] = {{".2", u3wdb_urn, c3y}, {}};
static u3j_harm _136_two__by_wyt_a[] = {{".2", u3wdb_wyt, c3y}, {}};

static u3j_harm _136_two_cue_a[] = {{".2", u3we_cue}, {}};
static u3j_harm _136_two_jam_a[] = {{".2", u3we_jam}, {}};
static u3j_harm _136_two_mat_a[] = {{".2", u3we_mat}, {}};
static u3j_harm _136_two_rub_a[] = {{".2", u3we_rub}, {}};



/* layer one
 */
static u3j_harm _136_one_add_a[] = {{".2", u3wa_add, c3y}, {}};
static u3j_harm _136_one_dec_a[] = {{".2", u3wa_dec, c3y}, {}};
static u3j_harm _136_one_div_a[] = {{".2", u3wa_div, c3y}, {}};
static u3j_harm _136_one_dvr_a[] = {{".2", u3wc_dvr, c3y}, {}};
static u3j_harm _136_one_gte_a[] = {{".2", u3wa_gte, c3y}, {}};
static u3j_harm _136_one_gth_a[] = {{".2", u3wa_gth, c3y}, {}};
static u3j_harm _136_one_lte_a[] = {{".2", u3wa_lte, c3y}, {}};
static u3j_harm _136_one_lth_a[] = {{".2", u3wa_lth, c3y}, {}};
static u3j_harm _136_one_max_a[] = {{".2", u3wa_max, c3y}, {}};
static u3j_harm _136_one_min_a[] = {{".2", u3wa_min, c3y}, {}};
static u3j_harm _136_one_mod_a[] = {{".2", u3wa_mod, c3y}, {}};
static u3j_harm _136_one_mul_a[] = {{".2", u3wa_mul, c3y}, {}};
static u3j_harm _136_one_sub_a[] = {{".2", u3wa_sub, c3y}, {}};

static u3j_harm _136_one_cap_a[] = {{".2", u3wc_cap, c3y}, {}};
static u3j_harm _136_one_peg_a[] = {{".2", u3wc_peg, c3y}, {}};
static u3j_harm _136_one_mas_a[] = {{".2", u3wc_mas, c3y}, {}};

static u3j_harm _136_lull_plot_fax_a[] = {{".2", u3wg_plot_fax, c3y}, {}};
static u3j_harm _136_lull_plot_met_a[] = {{".2", u3wg_plot_met, c3y}, {}};

static u3j_core _136_lull_plot_d[] =
  { { "fax", 7, _136_lull_plot_fax_a, 0, no_hashes },
    { "met", 7, _136_lull_plot_met_a, 0, no_hashes },
    {}
  };

static u3j_core _136_lull_d[] =
  { { "plot", 31, 0, _136_lull_plot_d, no_hashes },
    {}
  };

static u3j_harm _136_hex_blake3_hash_a[] = {{".2", u3we_blake3_hash, c3y}, {}};
static u3j_harm _136_hex_blake3_compress_a[] = {{".2", u3we_blake3_compress, c3y}, {}};
static u3j_harm _136_hex_blake3_chunk_output_a[] = {{".2", u3we_blake3_chunk_output, c3y}, {}};

static u3j_core _136_hex_blake3_d[] =
  { { "hash", 7, _136_hex_blake3_hash_a, 0, no_hashes },
    { "chunk-output", 7, _136_hex_blake3_chunk_output_a, 0, no_hashes },
    {}
  };


static u3j_core _136_hex_blake3_impl_d[] =
  { { "compress", 7, _136_hex_blake3_compress_a, 0, no_hashes },
    { "blake3",   7, 0,          _136_hex_blake3_d, no_hashes },
    {}
  };

static u3j_harm _136_hex_blake2b_a[] = {{".2", u3we_blake2b, c3y}, {}};

static u3j_core _136_hex_blake_d[] =
  { { "blake2b",     7, _136_hex_blake2b_a,     0, no_hashes },
    { "blake3-impl", 7, 0, _136_hex_blake3_impl_d, no_hashes },
    {}
  };


static u3j_harm _136_hex_chacha_crypt_a[] = {{".2", u3we_chacha_crypt, c3y}, {}};
static u3j_harm _136_hex_chacha_xchacha_a[] = {{".2", u3we_chacha_xchacha, c3y}, {}};
static u3j_core _136_hex_chacha_d[] =
  { { "crypt",   7, _136_hex_chacha_crypt_a,   0, no_hashes },
    { "xchacha", 7, _136_hex_chacha_xchacha_a, 0, no_hashes },
    {}
  };


//+|  %utilities
static u3j_harm _136_hex_bytestream_rip_octs_a[] = {{".2", u3we_bytestream_rip_octs, c3y}, {}};
static u3j_harm _136_hex_bytestream_cat_octs_a[] = {{".2", u3we_bytestream_cat_octs, c3y}, {}};
static u3j_harm _136_hex_bytestream_can_octs_a[] = {{".2", u3we_bytestream_can_octs, c3y}, {}};
//+|  %read-byte
static u3j_harm _136_hex_bytestream_read_byte_a[] = {{".2", u3we_bytestream_read_byte, c3y}, {}};
//+|  %read-octs
static u3j_harm _136_hex_bytestream_read_octs_a[] = {{".2", u3we_bytestream_read_octs, c3y}, {}};
//+|  %navigation
static u3j_harm _136_hex_bytestream_skip_line_a[] = {{".2", u3we_bytestream_skip_line, c3y}, {}};
static u3j_harm _136_hex_bytestream_find_byte_a[] = {{".2", u3we_bytestream_find_byte, c3y}, {}};
static u3j_harm _136_hex_bytestream_seek_byte_a[] = {{".2", u3we_bytestream_seek_byte, c3y}, {}};
//+|  %transformation
static u3j_harm _136_hex_bytestream_chunk_a[] = {{".2", u3we_bytestream_chunk}, {}};
static u3j_harm _136_hex_bytestream_extract_a[] = {{".2", u3we_bytestream_extract}, {}};
static u3j_harm _136_hex_bytestream_fuse_extract_a[] = {{".2", u3we_bytestream_fuse_extract}, {}};
//+|  %bitstream
static u3j_harm _136_hex_bytestream_need_bits_a[] = {{".2", u3we_bytestream_need_bits}, {}};
static u3j_harm _136_hex_bytestream_drop_bits_a[] = {{".2", u3we_bytestream_drop_bits}, {}};
// static u3j_harm _136_hex_bytestream_skip_bits_a[] = {{".2", u3we_bytestream_skip_bits}, {}};
static u3j_harm _136_hex_bytestream_peek_bits_a[] = {{".2", u3we_bytestream_peek_bits}, {}};
static u3j_harm _136_hex_bytestream_read_bits_a[] = {{".2", u3we_bytestream_read_bits}, {}};
// static u3j_harm _136_hex_bytestream_read_need_bits_a[] = {{".2", u3we_bytestream_read_need_bits}, {}};
static u3j_harm _136_hex_bytestream_byte_bits_a[] = {{".2", u3we_bytestream_byte_bits}, {}};

static u3j_core _136_hex_bytestream_d[] =
  { //+|  %utilities
    {"rip-octs", 7, _136_hex_bytestream_rip_octs_a, 0, no_hashes },
    {"cat-octs", 7, _136_hex_bytestream_cat_octs_a, 0, no_hashes },
    {"can-octs", 7, _136_hex_bytestream_can_octs_a, 0, no_hashes },
    //+|  %navigation
    {"skip-line", 7, _136_hex_bytestream_skip_line_a, 0, no_hashes },
    {"find-byte", 7, _136_hex_bytestream_find_byte_a, 0, no_hashes },
    {"seek-byte", 7, _136_hex_bytestream_seek_byte_a, 0, no_hashes },
    //+|  %read-byte
    {"read-byte", 7, _136_hex_bytestream_read_byte_a, 0, no_hashes },
    //+|  %read-octs
    {"read-octs", 7, _136_hex_bytestream_read_octs_a, 0, no_hashes },
    //+|  %transformation
    {"chunk", 7, _136_hex_bytestream_chunk_a, 0, no_hashes },
    {"extract", 7, _136_hex_bytestream_extract_a, 0, no_hashes },
    {"fuse-extract", 7, _136_hex_bytestream_fuse_extract_a, 0, no_hashes },
    //+|  %bitstream
    {"need-bits", 7, _136_hex_bytestream_need_bits_a, 0, no_hashes },
    {"drop-bits", 7, _136_hex_bytestream_drop_bits_a, 0, no_hashes },
    // {"skip-bits", 7, _136_hex_bytestream_skip_bits_a, 0, no_hashes },
    {"peek-bits", 7, _136_hex_bytestream_peek_bits_a, 0, no_hashes },
    {"read-bits", 7, _136_hex_bytestream_read_bits_a, 0, no_hashes },
    // {"read-need-bits", 7, _136_hex_bytestream_read_need_bits_a, 0, no_hashes },
    {"byte-bits", 7, _136_hex_bytestream_byte_bits_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_hex_json_de_a[] = {{".2", u3we_json_de}, {}};
static u3j_harm _136_hex_json_en_a[] = {{".2", u3we_json_en}, {}};
static u3j_core _136_hex_json_d[] =
  { { "de", 15, _136_hex_json_de_a, 0, no_hashes },
    { "en", 15, _136_hex_json_en_a, 0, no_hashes },
    {}
  };

/* /lib jets in non core
*/
static u3j_harm _136_non__lagoon_add_a[]  = {{".2", u3wi_la_add}, {}};
static u3j_harm _136_non__lagoon_sub_a[]  = {{".2", u3wi_la_sub}, {}};
static u3j_harm _136_non__lagoon_mul_a[]  = {{".2", u3wi_la_mul}, {}};
static u3j_harm _136_non__lagoon_div_a[]  = {{".2", u3wi_la_div}, {}};
static u3j_harm _136_non__lagoon_mod_a[]  = {{".2", u3wi_la_mod}, {}};
static u3j_harm _136_non__lagoon_adds_a[] = {{".2", u3wi_la_adds}, {}};
static u3j_harm _136_non__lagoon_subs_a[] = {{".2", u3wi_la_subs}, {}};
static u3j_harm _136_non__lagoon_muls_a[] = {{".2", u3wi_la_muls}, {}};
static u3j_harm _136_non__lagoon_divs_a[] = {{".2", u3wi_la_divs}, {}};
static u3j_harm _136_non__lagoon_mods_a[] = {{".2", u3wi_la_mods}, {}};
static u3j_harm _136_non__lagoon_dot_a[]  = {{".2", u3wi_la_dot}, {}};
static u3j_harm _136_non__lagoon_trans_a[] ={{".2", u3wi_la_transpose}, {}};
static u3j_harm _136_non__lagoon_cumsum_a[]={{".2", u3wi_la_cumsum}, {}};
static u3j_harm _136_non__lagoon_argmin_a[]={{".2", u3wi_la_argmin}, {}};
static u3j_harm _136_non__lagoon_argmax_a[]={{".2", u3wi_la_argmax}, {}};
static u3j_harm _136_non__lagoon_ravel_a[]={{".2", u3wi_la_ravel}, {}};
static u3j_harm _136_non__lagoon_min_a[]  = {{".2", u3wi_la_min}, {}};
static u3j_harm _136_non__lagoon_max_a[]  = {{".2", u3wi_la_max}, {}};
static u3j_harm _136_non__lagoon_linspace_a[]={{".2", u3wi_la_linspace}, {}};
static u3j_harm _136_non__lagoon_range_a[]= {{".2", u3wi_la_range}, {}};
static u3j_harm _136_non__lagoon_abs_a[]  = {{".2", u3wi_la_abs}, {}};
static u3j_harm _136_non__lagoon_gth_a[]  = {{".2", u3wi_la_gth}, {}};
static u3j_harm _136_non__lagoon_gte_a[]  = {{".2", u3wi_la_gte}, {}};
static u3j_harm _136_non__lagoon_lth_a[]  = {{".2", u3wi_la_lth}, {}};
static u3j_harm _136_non__lagoon_lte_a[]  = {{".2", u3wi_la_lte}, {}};
static u3j_harm _136_non__lagoon_diag_a[] = {{".2", u3wi_la_diag}, {}};
static u3j_harm _136_non__lagoon_trace_a[]= {{".2", u3wi_la_trace}, {}};
static u3j_harm _136_non__lagoon_mmul_a[] = {{".2", u3wi_la_mmul}, {}};
static u3j_harm _136_non__mice_a[] = {{".2", u3we_mice}, {}};

static u3j_core _136_non__la_core_d[] =
  { { "add-rays", 7, _136_non__lagoon_add_a,  0, no_hashes },
    { "sub-rays", 7, _136_non__lagoon_sub_a,  0, no_hashes },
    { "mul-rays", 7, _136_non__lagoon_mul_a,  0, no_hashes },
    { "div-rays", 7, _136_non__lagoon_div_a,  0, no_hashes },
    { "mod-rays", 7, _136_non__lagoon_mod_a,  0, no_hashes },
    { "add-scal", 7, _136_non__lagoon_adds_a, 0, no_hashes },
    { "sub-scal", 7, _136_non__lagoon_subs_a, 0, no_hashes },
    { "mul-scal", 7, _136_non__lagoon_muls_a, 0, no_hashes },
    { "div-scal", 7, _136_non__lagoon_divs_a, 0, no_hashes },
    { "mod-scal", 7, _136_non__lagoon_mods_a, 0, no_hashes },
    { "dot",      7, _136_non__lagoon_dot_a,  0, no_hashes },
    { "transpose",7, _136_non__lagoon_trans_a, 0, no_hashes },
    { "cumsum",   7, _136_non__lagoon_cumsum_a, 0, no_hashes },
    { "argmin",   7, _136_non__lagoon_argmin_a, 0, no_hashes },
    { "argmax",   7, _136_non__lagoon_argmax_a, 0, no_hashes },
    { "ravel",    7, _136_non__lagoon_ravel_a, 0, no_hashes },
    { "min",      7, _136_non__lagoon_min_a, 0, no_hashes },
    { "max",      7, _136_non__lagoon_max_a, 0, no_hashes },
    { "linspace", 7, _136_non__lagoon_linspace_a, 0, no_hashes },
    { "range",    7, _136_non__lagoon_range_a, 0, no_hashes },
    { "abs",      7, _136_non__lagoon_abs_a, 0, no_hashes },
    { "gth",      7, _136_non__lagoon_gth_a, 0, no_hashes },
    { "gte",      7, _136_non__lagoon_gte_a, 0, no_hashes },
    { "lth",      7, _136_non__lagoon_lth_a, 0, no_hashes },
    { "lte",      7, _136_non__lagoon_lte_a, 0, no_hashes },
    { "diag",     7, _136_non__lagoon_diag_a, 0, no_hashes },
    { "trace",    7, _136_non__lagoon_trace_a,0, no_hashes },
    { "mmul",     7, _136_non__lagoon_mmul_a, 0, no_hashes },
    {}
  };

static u3j_core _136_non_d[] =
  { { "lagoon", 7, 0, _136_non__la_core_d, no_hashes },
    { "mice", 7, _136_non__mice_a, 0, no_hashes },
    {}
  };


static u3j_harm _136_hex_lia_run_v1_a[] = {{".2", u3we_lia_run_v1, c3y}, {}};

static u3j_harm _136_hex_lia_run_once_inner_a[] = {{".2", u3we_lia_run_once, c3y}, {}};

static u3j_core _136_hex_lia_run_once_d[] = {
  { "run-once-inner-v0", 15, _136_hex_lia_run_once_inner_a, 0, no_hashes },
  {}
};

static u3j_core _136_hex_lia_monad_d[] = {
  { "run-v1", 7, _136_hex_lia_run_v1_a, 0, no_hashes },
  { "run-once-v0", 7, 0, _136_hex_lia_run_once_d, no_hashes },
  {}
};

static u3j_core _136_hex_wasm_engine_d[] = {
  { "monad-v0", 3, 0, _136_hex_lia_monad_d, no_hashes },
  {}
};

static u3j_core _136_hex_wasm_op_def_d[] = {
  { "wasm-engine-v0", 3, 0, _136_hex_wasm_engine_d, no_hashes },
  {}
};

static u3j_core _136_hex_wasm_validator_d[] = {
  { "wasm-op-def-v0", 3, 0, _136_hex_wasm_op_def_d, no_hashes },
  {}
};

static u3j_core _136_hex_wasm_parser_d[] = {
  { "validator-v0", 3, 0, _136_hex_wasm_validator_d, no_hashes },
  {}
};

static u3j_core _136_hex_lia_sur_d[] = {
  { "wasm-parser-v0", 3, 0, _136_hex_wasm_parser_d, no_hashes },
  {}
};

static u3j_core _136_hex_wasm_engine_sur_d[] = {
  { "monad-sur-v1", 3, 0, _136_hex_lia_sur_d, no_hashes },
  {}
};

static u3j_core _136_hex_wasm_sur_d[] = {
  { "engine-sur-v0", 3, 0, _136_hex_wasm_engine_sur_d, no_hashes },
  {}
};

static u3j_core _136_hex_d[] =
  { { "non", 7, 0, _136_non_d, no_hashes },

    { "lull",   3, 0, _136_lull_d, no_hashes },

    { "lore",  63, _136_hex_lore_a, 0, no_hashes },

    { "leer",  63, _136_hex_leer_a, 0, no_hashes },
    { "loss",  63, _136_hex_loss_a, 0, no_hashes },
    { "lune", 127, _136_hex_lune_a, 0, no_hashes },

    { "crc", 31, 0, _136_hex__crc_d, no_hashes },

    { "coed", 63, 0, _136_hex_coed_d, no_hashes },
    { "aes",  31, 0, _136_hex_aes_d,  no_hashes },

    { "hmac",   63, 0, _136_hex_hmac_d,   no_hashes },
    { "argon",  31, 0, _136_hex_argon_d,  no_hashes },
    { "blake",  31, 0, _136_hex_blake_d,  no_hashes },
    { "chacha", 31, 0, _136_hex_chacha_d, no_hashes },
    { "kecc",   31, 0, _136_hex_kecc_d,   no_hashes },
    { "ripemd", 31, 0, _136_hex_ripe_d,   no_hashes },
    { "scr",    31, 0, _136_hex_scr_d,    no_hashes },
    { "secp",    6, 0, _136_hex_secp_d,   no_hashes },
    { "mimes",  31, 0, _136_hex_mimes_d,  no_hashes },
    { "json",   31, 0, _136_hex_json_d,   no_hashes },
    { "checksum", 15, 0, _136_hex_checksum_d, no_hashes},
    { "wasm-sur-v0", 3, 0, _136_hex_wasm_sur_d, no_hashes },
    { "bytestream-v0", 31, 0, _136_hex_bytestream_d, no_hashes},
    { "zlib-v0", 31, 0, _136_hex__zlib_d, no_hashes },
    {}
  };

static u3j_core _136_pen_d[] =
  { { "hex", 7, 0, _136_hex_d, no_hashes },

    { "cell", 7, _136_pen_cell_a, 0, no_hashes },
    { "comb", 7, _136_pen_comb_a, 0, no_hashes },
    { "cons", 7, _136_pen_cons_a, 0, no_hashes },
    { "core", 7, _136_pen_core_a, 0, no_hashes },
    { "face", 7, _136_pen_face_a, 0, no_hashes },
    { "fitz", 7, _136_pen_fitz_a, 0, no_hashes },
    { "fork", 7, _136_pen_fork_a, 0, no_hashes },
    { "look", 7, _136_pen_look_a, 0, no_hashes },
    { "loot", 7, _136_pen_loot_a, 0, no_hashes },
    { "ut", 15, 0, _136_pen__ut_d, no_hashes, _136_pen__ut_ho },
    {}
  };

static u3j_core _136_qua__vi_d[] = 
  {
    { "mole", 7, _136_qua_mole_a, 0, no_hashes },
    { "mule", 7, _136_qua_mule_a, 0, no_hashes },
    {}
  };

static u3j_core _136_qua_d[] =
  { { "pen", 3, 0, _136_pen_d, no_hashes, _136_pen_ho },

    { "po", 7, 0, _136_qua__po_d, no_hashes },

    { "trip", 7, _136_qua_trip_a, 0, no_hashes },

    { "bend", 7, 0, _136_qua__bend_d, no_hashes },
    { "cold", 7, 0, _136_qua__cold_d, no_hashes },
    { "comp", 7, 0, _136_qua__comp_d, no_hashes },
    { "cook", 7, 0, _136_qua__cook_d, no_hashes },
    { "easy", 7, 0, _136_qua__easy_d, no_hashes },
    { "glue", 7, 0, _136_qua__glue_d, no_hashes },
    { "here", 7, 0, _136_qua__here_d, no_hashes },
    { "just", 7, 0, _136_qua__just_d, no_hashes },
    { "mask", 7, 0, _136_qua__mask_d, no_hashes },
    { "shim", 7, 0, _136_qua__shim_d, no_hashes },
    { "stag", 7, 0, _136_qua__stag_d, no_hashes },
    { "stew", 7, 0, _136_qua__stew_d, no_hashes },
    { "stir", 7, 0, _136_qua__stir_d, no_hashes },

    { "pfix", 7, _136_qua_pfix_a, 0, no_hashes },
    { "plug", 7, _136_qua_plug_a, 0, no_hashes },
    { "pose", 7, _136_qua_pose_a, 0, no_hashes },
    { "sfix", 7, _136_qua_sfix_a, 0, no_hashes },

    { "mink", 7, _136_qua_mink_a, 0, no_hashes },
    { "vi", 7, 0, _136_qua__vi_d, no_hashes    },

    { "scot", 7, _136_qua_scot_a, 0, no_hashes },
    { "scow", 7, _136_qua_scow_a, 0, no_hashes },
    { "slaw", 7, _136_qua_slaw_a, 0, no_hashes },
    {}
  };

static u3j_core _136_tri_d[] =
  { { "qua", 3, 0, _136_qua_d, no_hashes, _136_qua_ho },

    { "cofl", 7, 0, _136_tri__cofl_d, no_hashes },
    { "rd",   7, 0, _136_tri__rd_d,   no_hashes },
    { "rs",   7, 0, _136_tri__rs_d,   no_hashes },
    { "rq",   7, 0, _136_tri__rq_d,   no_hashes },
    { "rh",   7, 0, _136_tri__rh_d,   no_hashes },
    { "og",   7, 0, _136_tri__og_d,   no_hashes },

    { "sha",  7, 0,               _136_tri__sha_d, no_hashes },
    { "shax", 7, _136_tri_shax_a, 0,               no_hashes },
    { "shay", 7, _136_tri_shay_a, 0,               no_hashes },
    { "shas", 7, _136_tri_shas_a, 0,               no_hashes },
    { "shal", 7, _136_tri_shal_a, 0,               no_hashes },

    { "ob", 3, 0, _136_ob_d, no_hashes, _136_ob_ho },
    {}
  };

static u3j_harm _136_two_clz_a[] = {{".2", u3wc_clz, c3n}, {}};
static u3j_harm _136_two_ctz_a[] = {{".2", u3wc_ctz, c3n}, {}};
static u3j_harm _136_two_ham_a[] = {{".2", u3wc_ham, c3n}, {}};

static u3j_harm _136_two__hew_fun_a[] = {{".2", u3wc_hew, c3n}, {}};
static u3j_core _136_two__hew_d[] =
  { { "fun", 15, _136_two__hew_fun_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_two__by_bif_a[] = {{".2", u3wdb_bif, c3y}, {}};

static u3j_core _136_two__by_d[] =
  { { "all", 7, _136_two__by_all_a, 0, no_hashes },
    { "any", 7, _136_two__by_any_a, 0, no_hashes },
    { "apt", 7, _136_two__by_apt_a, 0, no_hashes },
    { "bif", 7, _136_two__by_bif_a, 0, no_hashes },
    { "del", 7, _136_two__by_del_a, 0, no_hashes },
    { "dif", 7, _136_two__by_dif_a, 0, no_hashes },
    { "gas", 7, _136_two__by_gas_a, 0, no_hashes },
    { "get", 7, _136_two__by_get_a, 0, no_hashes },
    { "has", 7, _136_two__by_has_a, 0, no_hashes },
    { "int", 7, _136_two__by_int_a, 0, no_hashes },
    { "jab", 7, _136_two__by_jab_a, 0, no_hashes },
    { "key", 7, _136_two__by_key_a, 0, no_hashes },
    { "put", 7, _136_two__by_put_a, 0, no_hashes },
    { "rep", 7, _136_two__by_rep_a, 0, no_hashes },
    { "run", 7, _136_two__by_run_a, 0, no_hashes },
    { "tap", 7, _136_two__by_tap_a, 0, no_hashes },
    { "uni", 7, _136_two__by_uni_a, 0, no_hashes },
    { "urn", 7, _136_two__by_urn_a, 0, no_hashes },
    { "wyt", 3, _136_two__by_wyt_a, 0, no_hashes },
    {}
  };


static u3j_harm _136_two__in_apt_a[] = {{".2", u3wdi_apt}, {}};

static u3j_core _136_two__in_d[] =
  { { "apt", 7, _136_two__in_apt_a, 0, no_hashes },
    { "bif", 7, _136_two__in_bif_a, 0, no_hashes },
    { "del", 7, _136_two__in_del_a, 0, no_hashes },
    { "dif", 7, _136_two__in_dif_a, 0, no_hashes },
    { "gas", 7, _136_two__in_gas_a, 0, no_hashes },
    { "has", 7, _136_two__in_has_a, 0, no_hashes },
    { "int", 7, _136_two__in_int_a, 0, no_hashes },
    { "put", 7, _136_two__in_put_a, 0, no_hashes },
    { "rep", 7, _136_two__in_rep_a, 0, no_hashes },
    { "run", 7, _136_two__in_run_a, 0, no_hashes },
    { "tap", 7, _136_two__in_tap_a, 0, no_hashes },
    { "uni", 7, _136_two__in_uni_a, 0, no_hashes },
    { "wyt", 3, _136_two__in_wyt_a, 0, no_hashes },
    {}
  };

static u3j_harm _136_two_rig_a[] = {{".2", u3wc_rig, c3n}, {}};

static u3j_harm _136_two_mate_a[] = {{".2", u3wb_mate, c3y}, {}};
static u3j_harm _136_two_sew_a[]  = {{".2", u3wc_sew, c3y}, {}};

static u3j_core _136_two_d[] =
  { { "tri", 3, 0, _136_tri_d, no_hashes, _136_tri_ho },

    { "find", 7, _136_two_find_a, 0, no_hashes },
    { "flop", 7, _136_two_flop_a, 0, no_hashes },
    { "lent", 7, _136_two_lent_a, 0, no_hashes },
    { "levy", 7, _136_two_levy_a, 0, no_hashes },
    { "lien", 7, _136_two_lien_a, 0, no_hashes },
    { "murn", 7, _136_two_murn_a, 0, no_hashes },
    { "need", 7, _136_two_need_a, 0, no_hashes },
    { "mate", 7, _136_two_mate_a, 0, no_hashes },
    { "reap", 7, _136_two_reap_a, 0, no_hashes },
    { "reel", 7, _136_two_reel_a, 0, no_hashes },
    { "roll", 7, _136_two_roll_a, 0, no_hashes },
    { "skid", 7, _136_two_skid_a, 0, no_hashes },
    { "skim", 7, _136_two_skim_a, 0, no_hashes },
    { "skip", 7, _136_two_skip_a, 0, no_hashes },
    { "scag", 7, _136_two_scag_a, 0, no_hashes },
    { "slag", 7, _136_two_slag_a, 0, no_hashes },
    { "snag", 7, _136_two_snag_a, 0, no_hashes },
    { "sort", 7, _136_two_sort_a, 0, no_hashes },
    { "turn", 7, _136_two_turn_a, 0, no_hashes },
    { "weld", 7, _136_two_weld_a, 0, no_hashes },
    { "welp", 7, _136_two_welp_a, 0, no_hashes },
    { "zing", 7, _136_two_zing_a, 0, no_hashes },

    { "bex",  7, _136_two_bex_a, 0, no_hashes },
    { "cat",  7, _136_two_cat_a, 0, no_hashes },
    { "can",  7, _136_two_can_a, 0, no_hashes },
    { "clz",  7, _136_two_clz_a, 0, no_hashes },
    { "con",  7, _136_two_con_a, 0, no_hashes },
    { "ctz",  7, _136_two_ctz_a, 0, no_hashes },
    { "cue",  7, _136_two_cue_a, 0, no_hashes },
    { "cut",  7, _136_two_cut_a, 0, no_hashes },
    { "dis",  7, _136_two_dis_a, 0, no_hashes },
    { "dor",  7, _136_two_dor_a, 0, no_hashes },
    { "end",  7, _136_two_end_a, 0, no_hashes },
    { "gor",  7, _136_two_gor_a, 0, no_hashes },
    { "ham",  7, _136_two_ham_a, 0, no_hashes },
    { "hew", 7, 0, _136_two__hew_d, no_hashes },
    { "jam",  7, _136_two_jam_a, 0, no_hashes },
    { "lsh",  7, _136_two_lsh_a, 0, no_hashes },
    { "mat",  7, _136_two_mat_a, 0, no_hashes },
    { "met",  7, _136_two_met_a, 0, no_hashes },
    { "mix",  7, _136_two_mix_a, 0, no_hashes },
    { "mor",  7, _136_two_mor_a, 0, no_hashes },
    { "mug",  7, _136_two_mug_a, 0, no_hashes },
    { "muk",  7, _136_two_muk_a, 0, no_hashes },
    { "rap",  7, _136_two_rap_a, 0, no_hashes },
    { "rep",  7, _136_two_rep_a, 0, no_hashes },
    { "rev",  7, _136_two_rev_a, 0, no_hashes },
    { "rig",  7, _136_two_rig_a, 0, no_hashes },
    { "rip",  7, _136_two_rip_a, 0, no_hashes },
    { "rsh",  7, _136_two_rsh_a, 0, no_hashes },
    { "swp",  7, _136_two_swp_a, 0, no_hashes },
    { "rub",  7, _136_two_rub_a, 0, no_hashes },
    { "pow",  7, _136_two_pow_a, 0, no_hashes },
    { "sew",  7, _136_two_sew_a, 0, no_hashes },
    { "sqt",  7, _136_two_sqt_a, 0, no_hashes },
    { "xeb",  7, _136_two_xeb_a, 0, no_hashes },

    { "by", 7, 0, _136_two__by_d, no_hashes },
    { "in", 7, 0, _136_two__in_d, no_hashes },
    {}
  };

static u3j_core _136_one_d[] =
  { { "two", 3, 0, _136_two_d, no_hashes },

    { "add", 7, _136_one_add_a, 0, no_hashes },
    { "dec", 7, _136_one_dec_a, 0, no_hashes },
    { "div", 7, _136_one_div_a, 0, no_hashes },
    { "dvr", 7, _136_one_dvr_a, 0, no_hashes },
    { "gte", 7, _136_one_gte_a, 0, no_hashes },
    { "gth", 7, _136_one_gth_a, 0, no_hashes },
    { "lte", 7, _136_one_lte_a, 0, no_hashes },
    { "lth", 7, _136_one_lth_a, 0, no_hashes },
    { "max", 7, _136_one_max_a, 0, no_hashes },
    { "min", 7, _136_one_min_a, 0, no_hashes },
    { "mod", 7, _136_one_mod_a, 0, no_hashes },
    { "mul", 7, _136_one_mul_a, 0, no_hashes },
    { "sub", 7, _136_one_sub_a, 0, no_hashes },

    { "cap", 7, _136_one_cap_a, 0, no_hashes },
    { "mas", 7, _136_one_mas_a, 0, no_hashes },
    { "peg", 7, _136_one_peg_a, 0, no_hashes },
    {}
  };

u3j_core _k136_d[] =
  { { "one", 3, 0, _136_one_d, no_hashes },
    {}
  };

