/// @file

#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"

/* _u_sam: axes of a gate sample, and of its head and tail.
*/
static const c3_l _u_sam[]   = { u3x_sam };
static const c3_l _u_sam_2[] = { u3x_sam_2, u3x_sam_3 };
static const c3_l _u_sam_3[] = { u3x_sam_2, u3x_sam_6, u3x_sam_7 };
static const c3_l _u_cut[]   = { u3x_sam_2, u3x_sam_12, u3x_sam_13, u3x_sam_7 };
static const c3_l _u_sew[]   = { u3x_sam_2, u3x_sam_12, u3x_sam_26, u3x_sam_27,
                                 u3x_sam_7 };

/* _u_con: the sample of the door an arm is in, after the arm's own; a
**         trap on the door itself (+wyt) has it at _u_con_2.
*/
static const c3_l _u_con[]       = { u3x_con_sam };
static const c3_l _u_con_2[]     = { u3x_con_2 };
static const c3_l _u_sam_con[]   = { u3x_sam, u3x_con_sam };
static const c3_l _u_sam_2_con[] = { u3x_sam_2, u3x_sam_3, u3x_con_sam };
static const c3_l _u_sam_3_con[] = { u3x_sam_2, u3x_sam_6, u3x_sam_7,
                                     u3x_con_sam };
static const c3_l _u_sam_con_2[] = { u3x_sam, u3x_con_sam_2, u3x_con_sam_3 };

/* _u_sam_4: a four-part sample, and the shapes of some crypto arms.
*/
static const c3_l _u_sam_4[]  = { u3x_sam_2, u3x_sam_6, u3x_sam_14,
                                  u3x_sam_15 };
static const c3_l _u_octs_2[] = { u3x_sam_4, u3x_sam_5, u3x_sam_3 };
static const c3_l _u_hmac[]   = { u3x_sam_4, 50, 51, u3x_sam_12, u3x_sam_13,
                                  u3x_sam_14, u3x_sam_15 };
static const c3_l _u_pbl[]    = { u3x_sam_2, u3x_sam_6, u3x_sam_14, u3x_sam_30,
                                  u3x_sam_62, u3x_sam_63 };
static const c3_l _u_octs_4[] = { u3x_sam_4, u3x_sam_5, u3x_sam_6, u3x_sam_7 };
static const c3_l _u_hew[]    = { u3x_sam, 28, 29, 125 };
static const c3_l _u_blake3[] = { u3x_sam_2, u3x_sam_6, u3x_sam_7,
                                  u3x_con_sam_2, u3x_con_sam_3 };
static const c3_l _u_blake3_compress[] = { u3x_sam_2, u3x_sam_6, u3x_sam_14,
                                           u3x_sam_30, u3x_sam_31 };

#define U3U_UNARY(fun)   { u3w##fun, u3w##fun, u3u##fun, 1, _u_sam,      false }
#define U3U_BINARY(fun)  { u3w##fun, u3w##fun, u3u##fun, 2, _u_sam_2,    false }
#define U3U_TERNARY(fun) { u3w##fun, u3w##fun, u3u##fun, 3, _u_sam_3,    false }
#define U3U_DOOR(fun)    { u3w##fun, u3w##fun, u3u##fun, 2, _u_sam_con,  false }
#define U3U_ARM(fun, len, axe) { u3w##fun, u3w##fun, u3u##fun, len, axe, false }
#define U3U_PUNT(fun)    { u3w##fun, u3w##fun, 0, 1, (c3_l[]){1}, true  }
#define U3U_MONO(fun)    { u3w##fun, u3u##fun, 0, 1, (c3_l[]){1}, true  }

/* U3U_OWN: an arm with a transferring driver too, u3u<fun>_own.
*/
#define U3U_OWN(fun, len, axe)                                        \
  { u3w##fun, u3w##fun, u3u##fun, len, axe, false, u3u##fun##_own }

/* U3U_REAL: the arms of a floating point door (+rd, +rs, +rq, +rh); the
**           comparisons don't read the rounding mode.
*/
#define U3U_REAL(pre)                        \
  U3U_ARM(pre##_add, 3, _u_sam_2_con),       \
  U3U_ARM(pre##_sub, 3, _u_sam_2_con),       \
  U3U_ARM(pre##_mul, 3, _u_sam_2_con),       \
  U3U_ARM(pre##_div, 3, _u_sam_2_con),       \
  U3U_DOOR(pre##_sqt),                       \
  U3U_ARM(pre##_fma, 4, _u_sam_3_con),       \
  U3U_BINARY(pre##_lth),                     \
  U3U_BINARY(pre##_lte),                     \
  U3U_BINARY(pre##_equ),                     \
  U3U_BINARY(pre##_gte),                     \
  U3U_BINARY(pre##_gth)

/* u3u_Harm: array drivers, keyed by their u3w counterparts.  +welp,
**           +tap:by and +wyt:by share the drivers of +weld, +tap:in and
**           +wyt:in.
*/
const u3u_harm u3u_Harm[] = {
  U3U_BINARY(a_add),
  U3U_UNARY(a_dec),
  U3U_BINARY(a_div),
  U3U_BINARY(a_gte),
  U3U_BINARY(a_gth),
  U3U_BINARY(a_lte),
  U3U_BINARY(a_lth),
  U3U_BINARY(a_max),
  U3U_BINARY(a_min),
  U3U_BINARY(a_mod),
  U3U_BINARY(a_mul),
  U3U_BINARY(a_sub),
  U3U_BINARY(b_find),
  U3U_UNARY(b_flop),
  U3U_UNARY(b_lent),
  U3U_BINARY(b_mate),
  U3U_UNARY(b_need),
  U3U_BINARY(b_reap),
  U3U_BINARY(b_scag),
  U3U_BINARY(b_slag),
  U3U_BINARY(b_snag),
  U3U_BINARY(b_weld),
  U3U_UNARY(b_zing),
  U3U_UNARY(c_bex),
  U3U_BINARY(c_can),
  U3U_UNARY(c_cap),
  U3U_TERNARY(c_cat),
  U3U_BINARY(c_clz),
  U3U_BINARY(c_con),
  U3U_UNARY(c_ctz),
  U3U_ARM(c_cut, 4, _u_cut),
  U3U_BINARY(c_dis),
  U3U_BINARY(c_dor),
  U3U_BINARY(c_dvr),
  U3U_BINARY(c_end),
  U3U_BINARY(c_gor),
  U3U_UNARY(c_ham),
  U3U_ARM(c_hew, 4, _u_hew),
  U3U_BINARY(c_lsh),
  U3U_UNARY(c_mas),
  U3U_BINARY(c_met),
  U3U_BINARY(c_mix),
  U3U_BINARY(c_mor),
  U3U_UNARY(c_mug),
  U3U_TERNARY(c_muk),
  U3U_BINARY(c_peg),
  U3U_BINARY(c_pow),
  U3U_BINARY(c_rap),
  U3U_BINARY(c_rep),
  U3U_TERNARY(c_rev),
  U3U_BINARY(c_rig),
  U3U_BINARY(c_rip),
  U3U_BINARY(c_rsh),
  U3U_OWN(c_sew, 5, _u_sew),
  U3U_UNARY(c_sqt),
  U3U_BINARY(c_swp),
  U3U_UNARY(c_xeb),
  U3U_UNARY(cp_ind),
  U3U_UNARY(cp_ins),
  U3U_UNARY(cp_tod),
  U3U_UNARY(cp_tos),
  U3U_ARM(db_apt, 1, _u_con),
  U3U_DOOR(db_bif),
  U3U_DOOR(db_del),
  U3U_DOOR(db_dif),
  U3U_DOOR(db_gas),
  U3U_DOOR(db_get),
  U3U_DOOR(db_has),
  U3U_DOOR(db_int),
  U3U_ARM(db_key, 1, _u_con),
  U3U_ARM(db_put, 3, _u_sam_2_con),
  U3U_DOOR(db_uni),
  U3U_ARM(di_apt, 1, _u_con),
  U3U_ARM(di_apt_140, 1, _u_con),
  U3U_DOOR(di_bif),
  U3U_DOOR(di_del),
  U3U_DOOR(di_dif),
  U3U_DOOR(di_gas),
  U3U_DOOR(di_has),
  U3U_DOOR(di_int),
  U3U_DOOR(di_put),
  U3U_ARM(di_tap, 1, _u_con),
  U3U_DOOR(di_uni),
  U3U_ARM(di_wyt, 1, _u_con_2),
  U3U_PUNT(e_adler32),
  U3U_PUNT(e_argon2),
  U3U_PUNT(e_blake2b),
  U3U_ARM(e_blake3_chunk_output, 5, _u_blake3),
  U3U_ARM(e_blake3_compress, 5, _u_blake3_compress),
  U3U_ARM(e_blake3_hash, 5, _u_blake3),
  U3U_PUNT(e_bytestream_byte_bits),
  U3U_PUNT(e_bytestream_can_octs),
  U3U_PUNT(e_bytestream_cat_octs),
  U3U_PUNT(e_bytestream_chunk),
  U3U_PUNT(e_bytestream_drop_bits),
  U3U_PUNT(e_bytestream_extract),
  U3U_PUNT(e_bytestream_find_byte),
  U3U_PUNT(e_bytestream_fuse_extract),
  U3U_PUNT(e_bytestream_need_bits),
  U3U_PUNT(e_bytestream_peek_bits),
  U3U_PUNT(e_bytestream_read_bits),
  U3U_PUNT(e_bytestream_read_byte),
  U3U_PUNT(e_bytestream_read_octs),
  U3U_PUNT(e_bytestream_rip_octs),
  U3U_PUNT(e_bytestream_seek_byte),
  U3U_PUNT(e_bytestream_skip_line),
  U3U_UNARY(e_chacha_crypt),
  U3U_UNARY(e_chacha_xchacha),
  U3U_UNARY(e_crc32),
  U3U_UNARY(e_cue),
  U3U_UNARY(e_de_base16),
  U3U_PUNT(e_decompress_gzip),
  U3U_PUNT(e_decompress_zlib),
  U3U_DOOR(e_easy_fun),
  U3U_BINARY(e_en_base16),
  U3U_UNARY(e_fein_ob),
  U3U_UNARY(e_fynd_ob),
  U3U_ARM(e_hmac, 7, _u_hmac),
  U3U_BINARY(e_hmac_sha1),
  U3U_BINARY(e_hmac_sha256),
  U3U_BINARY(e_hmac_sha512),
  U3U_BINARY(e_hmac_sha1t),
  U3U_BINARY(e_hmac_sha256t),
  U3U_BINARY(e_hmac_sha512t),
  U3U_ARM(e_hmac_sha1l, 4, _u_octs_4),
  U3U_ARM(e_hmac_sha256l, 4, _u_octs_4),
  U3U_ARM(e_hmac_sha512l, 4, _u_octs_4),
  U3U_UNARY(e_jam),
  U3U_UNARY(e_json_de),
  U3U_UNARY(e_json_en),
  U3U_DOOR(e_just_fun),
  U3U_BINARY(e_kecc224),
  U3U_BINARY(e_kecc256),
  U3U_BINARY(e_kecc384),
  U3U_BINARY(e_kecc512),
  U3U_UNARY(e_leer),
  U3U_PUNT(e_lia_run_once),
  U3U_PUNT(e_lia_run_v1),
  U3U_UNARY(e_lore),
  U3U_BINARY(e_loss),
  U3U_UNARY(e_lune),
  U3U_PUNT(e_make),
  U3U_DOOR(e_mask_fun),
  U3U_UNARY(e_mat),
  U3U_MONO(e_mice),
  U3U_PUNT(e_mink),
  U3U_MONO(e_mock),
  U3U_MONO(e_mole),
  U3U_MONO(e_monk),
  U3U_MONO(e_mule),
  U3U_PUNT(e_reco),
  U3U_PUNT(e_ripe),
  U3U_BINARY(e_rub),
  U3U_PUNT(e_scot),
  U3U_PUNT(e_scow),
  U3U_BINARY(e_sha1),
  U3U_BINARY(e_shal),
  U3U_BINARY(e_shas),
  U3U_UNARY(e_shax),
  U3U_BINARY(e_shay),
  U3U_DOOR(e_shim_fun),
  U3U_PUNT(e_sign),
  U3U_PUNT(e_slaw),
  U3U_PUNT(e_sosi),
  U3U_TERNARY(e_sove),
  U3U_UNARY(e_trip),
  U3U_ARM(ea_cbca_de, 3, _u_sam_con_2),
  U3U_ARM(ea_cbca_en, 3, _u_sam_con_2),
  U3U_ARM(ea_cbcb_de, 3, _u_sam_con_2),
  U3U_ARM(ea_cbcb_en, 3, _u_sam_con_2),
  U3U_ARM(ea_cbcc_de, 3, _u_sam_con_2),
  U3U_ARM(ea_cbcc_en, 3, _u_sam_con_2),
  U3U_PUNT(ea_ecba_de),
  U3U_PUNT(ea_ecba_en),
  U3U_PUNT(ea_ecbb_de),
  U3U_PUNT(ea_ecbb_en),
  U3U_PUNT(ea_ecbc_de),
  U3U_PUNT(ea_ecbc_en),
  U3U_PUNT(ea_siva_de),
  U3U_PUNT(ea_siva_en),
  U3U_PUNT(ea_sivb_de),
  U3U_PUNT(ea_sivb_en),
  U3U_PUNT(ea_sivc_de),
  U3U_PUNT(ea_sivc_en),
  U3U_ARM(ee_add_double_scalarmult, 4, _u_sam_4),
  U3U_TERNARY(ee_add_scalarmult_scalarmult_base),
  U3U_UNARY(ee_luck),
  U3U_BINARY(ee_point_add),
  U3U_UNARY(ee_point_neg),
  U3U_UNARY(ee_puck),
  U3U_UNARY(ee_recs),
  U3U_TERNARY(ee_scad),
  U3U_BINARY(ee_scalarmult),
  U3U_UNARY(ee_scalarmult_base),
  U3U_BINARY(ee_scap),
  U3U_BINARY(ee_scas),
  U3U_BINARY(ee_shar),
  U3U_BINARY(ee_sign),
  U3U_ARM(ee_sign_octs, 3, _u_octs_2),
  U3U_ARM(ee_sign_octs_raw, 4, _u_octs_4),
  U3U_TERNARY(ee_sign_raw),
  U3U_BINARY(ee_slar),
  U3U_TERNARY(ee_smac),
  U3U_TERNARY(ee_veri),
  U3U_ARM(ee_veri_octs, 4, _u_cut),
  U3U_DOOR(ef_drg),
  U3U_ARM(ef_lug, 4, _u_sam_3_con),
  U3U_DOOR(eo_raw),
  U3U_PUNT(es_hsh),
  U3U_PUNT(es_hsl),
  U3U_ARM(es_pbk, 4, _u_sam_4),
  U3U_ARM(es_pbl, 6, _u_pbl),
  U3U_REAL(er),
  U3U_REAL(et),
  U3U_REAL(eq),
  U3U_REAL(es),
  U3U_BINARY(f_cell),
  U3U_BINARY(f_comb),
  U3U_BINARY(f_cons),
  U3U_BINARY(f_core),
  U3U_BINARY(f_face),
  U3U_BINARY(f_fitz),
  U3U_BINARY(f_flan_139),
  U3U_UNARY(f_flip_139),
  U3U_BINARY(f_flor_139),
  U3U_UNARY(f_fork),
  U3U_BINARY(f_look),
  U3U_BINARY(f_loot),
  U3U_MONO(fu_crop),
  U3U_MONO(fu_fish),
  U3U_MONO(fu_fuse),
  U3U_MONO(fu_mint),
  U3U_MONO(fu_mull),
  U3U_MONO(fu_nest_dext),
  U3U_MONO(fu_redo),
  U3U_MONO(fu_rest),
  U3U_BINARY(g_plot_fax),
  U3U_BINARY(g_plot_met),
  U3U_PUNT(i_la_abs),
  U3U_PUNT(i_la_add),
  U3U_PUNT(i_la_adds),
  U3U_PUNT(i_la_argmax),
  U3U_PUNT(i_la_argmin),
  U3U_PUNT(i_la_cumsum),
  U3U_PUNT(i_la_diag),
  U3U_PUNT(i_la_div),
  U3U_PUNT(i_la_divs),
  U3U_PUNT(i_la_dot),
  U3U_PUNT(i_la_gte),
  U3U_PUNT(i_la_gth),
  U3U_PUNT(i_la_linspace),
  U3U_PUNT(i_la_lte),
  U3U_PUNT(i_la_lth),
  U3U_PUNT(i_la_max),
  U3U_PUNT(i_la_min),
  U3U_PUNT(i_la_mmul),
  U3U_PUNT(i_la_mod),
  U3U_PUNT(i_la_mods),
  U3U_PUNT(i_la_mul),
  U3U_PUNT(i_la_muls),
  U3U_PUNT(i_la_range),
  U3U_PUNT(i_la_ravel),
  U3U_PUNT(i_la_sub),
  U3U_PUNT(i_la_subs),
  U3U_PUNT(i_la_trace),
  U3U_PUNT(i_la_transpose),
  {}
};
