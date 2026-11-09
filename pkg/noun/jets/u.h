/// @file

#ifndef U3_JETS_U_H
#define U3_JETS_U_H

#include "types.h"

  /** u3u: jets with C-array arguments, for compiled nock.
  ***
  *** An array driver takes the arguments of an arm as an array of
  *** nouns, in depth-first order (see +flatten-need in nock-compilation.hoon)
  *** Arguments are RETAINED, the product is transferred.  Array drivers
  *** are total: they produce or bail, and never punt.  An arm with no
  *** array driver may be listed as punting instead: compiled nock then
  *** calls a driver on the whole core, its u3w driver or one of its own,
  *** and the nock of the arm if the driver punts.
  **/
    typedef u3_noun (*u3u_fun)(u3_noun*);

  /* u3u_harm: array-driver arm.
  **    key_f: a key to map this entry to the hot state, u3w* driver in practice
  **    mon_f: a retaining driver that takes the entire core, can punt
  **    arg_f: array driver, NULL if punting
  **    len_w: number of arguments, 1 if punting
  **    axe_l: argument axes in the core, {1} if punting
  **    pun_t: punting driver, u3w* driver from the hot state will apply
  */
    typedef struct _u3u_harm {
      u3_noun     (*key_f)(u3_noun);
      u3_noun     (*mon_f)(u3_noun);
      u3u_fun       arg_f;
      c3_w          len_w;
      const c3_l*   axe_l;
      c3_t          pun_t;
    } u3u_harm;

  /* u3u_Harm: blank-terminated table of array drivers.
  */
    extern const u3u_harm u3u_Harm[];

  /** Tier 1.
  **/
    u3_noun u3ua_add(u3_noun*);
    u3_noun u3ua_dec(u3_noun*);
    u3_noun u3ua_div(u3_noun*);
    u3_noun u3ua_gte(u3_noun*);
    u3_noun u3ua_gth(u3_noun*);
    u3_noun u3ua_lte(u3_noun*);
    u3_noun u3ua_lth(u3_noun*);
    u3_noun u3ua_max(u3_noun*);
    u3_noun u3ua_min(u3_noun*);
    u3_noun u3ua_mod(u3_noun*);
    u3_noun u3ua_mul(u3_noun*);
    u3_noun u3ua_sub(u3_noun*);

  /** Tier 2.
  **/
    u3_noun u3ub_find(u3_noun*);
    u3_noun u3ub_flop(u3_noun*);
    u3_noun u3ub_lent(u3_noun*);
    u3_noun u3ub_mate(u3_noun*);
    u3_noun u3ub_need(u3_noun*);
    u3_noun u3ub_reap(u3_noun*);
    u3_noun u3ub_scag(u3_noun*);
    u3_noun u3ub_slag(u3_noun*);
    u3_noun u3ub_snag(u3_noun*);
    u3_noun u3ub_weld(u3_noun*);
    u3_noun u3ub_zing(u3_noun*);

  /** Tier 3.
  **/
    u3_noun u3uc_bex(u3_noun*);
    u3_noun u3uc_can(u3_noun*);
    u3_noun u3uc_cap(u3_noun*);
    u3_noun u3uc_cat(u3_noun*);
    u3_noun u3uc_clz(u3_noun*);
    u3_noun u3uc_con(u3_noun*);
    u3_noun u3uc_ctz(u3_noun*);
    u3_noun u3uc_cut(u3_noun*);
    u3_noun u3uc_dis(u3_noun*);
    u3_noun u3uc_dor(u3_noun*);
    u3_noun u3uc_dvr(u3_noun*);
    u3_noun u3uc_end(u3_noun*);
    u3_noun u3uc_gor(u3_noun*);
    u3_noun u3uc_ham(u3_noun*);
    u3_noun u3uc_hew(u3_noun*);
    u3_noun u3uc_lsh(u3_noun*);
    u3_noun u3uc_mas(u3_noun*);
    u3_noun u3uc_met(u3_noun*);
    u3_noun u3uc_mix(u3_noun*);
    u3_noun u3uc_mor(u3_noun*);
    u3_noun u3uc_mug(u3_noun*);
    u3_noun u3uc_muk(u3_noun*);
    u3_noun u3uc_peg(u3_noun*);
    u3_noun u3uc_pow(u3_noun*);
    u3_noun u3uc_rap(u3_noun*);
    u3_noun u3uc_rep(u3_noun*);
    u3_noun u3uc_rev(u3_noun*);
    u3_noun u3uc_rig(u3_noun*);
    u3_noun u3uc_rip(u3_noun*);
    u3_noun u3uc_rsh(u3_noun*);
    u3_noun u3uc_sqt(u3_noun*);
    u3_noun u3uc_swp(u3_noun*);
    u3_noun u3uc_xeb(u3_noun*);
    u3_noun u3ucp_ind(u3_noun*);
    u3_noun u3ucp_ins(u3_noun*);
    u3_noun u3ucp_tod(u3_noun*);
    u3_noun u3ucp_tos(u3_noun*);

  /** Tier 4.
  **/
    u3_noun u3udb_apt(u3_noun*);
    u3_noun u3udb_bif(u3_noun*);
    u3_noun u3udb_del(u3_noun*);
    u3_noun u3udb_dif(u3_noun*);
    u3_noun u3udb_gas(u3_noun*);
    u3_noun u3udb_get(u3_noun*);
    u3_noun u3udb_has(u3_noun*);
    u3_noun u3udb_int(u3_noun*);
    u3_noun u3udb_key(u3_noun*);
    u3_noun u3udb_put(u3_noun*);
    u3_noun u3udb_uni(u3_noun*);
    u3_noun u3udi_apt(u3_noun*);
    u3_noun u3udi_apt_140(u3_noun*);
    u3_noun u3udi_bif(u3_noun*);
    u3_noun u3udi_del(u3_noun*);
    u3_noun u3udi_dif(u3_noun*);
    u3_noun u3udi_gas(u3_noun*);
    u3_noun u3udi_has(u3_noun*);
    u3_noun u3udi_int(u3_noun*);
    u3_noun u3udi_put(u3_noun*);
    u3_noun u3udi_tap(u3_noun*);
    u3_noun u3udi_uni(u3_noun*);
    u3_noun u3udi_wyt(u3_noun*);

  /** Tier 5.
  **/
    u3_noun u3ue_blake3_chunk_output(u3_noun*);
    u3_noun u3ue_blake3_compress(u3_noun*);
    u3_noun u3ue_blake3_hash(u3_noun*);
    u3_noun u3ue_chacha_crypt(u3_noun*);
    u3_noun u3ue_chacha_xchacha(u3_noun*);
    u3_noun u3ue_crc32(u3_noun*);
    u3_noun u3ue_cue(u3_noun*);
    u3_noun u3ue_de_base16(u3_noun*);
    u3_noun u3ue_easy_fun(u3_noun*);
    u3_noun u3ue_en_base16(u3_noun*);
    u3_noun u3ue_fein_ob(u3_noun*);
    u3_noun u3ue_fynd_ob(u3_noun*);
    u3_noun u3ue_hmac(u3_noun*);
    u3_noun u3ue_jam(u3_noun*);
    u3_noun u3ue_json_de(u3_noun*);
    u3_noun u3ue_json_en(u3_noun*);
    u3_noun u3ue_just_fun(u3_noun*);
    u3_noun u3ue_kecc224(u3_noun*);
    u3_noun u3ue_kecc256(u3_noun*);
    u3_noun u3ue_kecc384(u3_noun*);
    u3_noun u3ue_kecc512(u3_noun*);
    u3_noun u3ue_leer(u3_noun*);
    u3_noun u3ue_lore(u3_noun*);
    u3_noun u3ue_loss(u3_noun*);
    u3_noun u3ue_lune(u3_noun*);
    u3_noun u3ue_mask_fun(u3_noun*);
    u3_noun u3ue_mat(u3_noun*);
    u3_noun u3ue_mice(u3_noun);
    u3_noun u3ue_mock(u3_noun);
    u3_noun u3ue_mole(u3_noun);
    u3_noun u3ue_monk(u3_noun);
    u3_noun u3ue_mule(u3_noun);
    u3_noun u3ue_rub(u3_noun*);
    u3_noun u3ue_sha1(u3_noun*);
    u3_noun u3ue_shal(u3_noun*);
    u3_noun u3ue_shas(u3_noun*);
    u3_noun u3ue_shax(u3_noun*);
    u3_noun u3ue_shay(u3_noun*);
    u3_noun u3ue_shim_fun(u3_noun*);
    u3_noun u3ue_sove(u3_noun*);
    u3_noun u3ue_trip(u3_noun*);
    u3_noun u3uea_cbca_de(u3_noun*);
    u3_noun u3uea_cbca_en(u3_noun*);
    u3_noun u3uea_cbcb_de(u3_noun*);
    u3_noun u3uea_cbcb_en(u3_noun*);
    u3_noun u3uea_cbcc_de(u3_noun*);
    u3_noun u3uea_cbcc_en(u3_noun*);
    u3_noun u3uee_add_double_scalarmult(u3_noun*);
    u3_noun u3uee_add_scalarmult_scalarmult_base(u3_noun*);
    u3_noun u3uee_luck(u3_noun*);
    u3_noun u3uee_point_add(u3_noun*);
    u3_noun u3uee_point_neg(u3_noun*);
    u3_noun u3uee_puck(u3_noun*);
    u3_noun u3uee_recs(u3_noun*);
    u3_noun u3uee_scad(u3_noun*);
    u3_noun u3uee_scalarmult(u3_noun*);
    u3_noun u3uee_scalarmult_base(u3_noun*);
    u3_noun u3uee_scap(u3_noun*);
    u3_noun u3uee_scas(u3_noun*);
    u3_noun u3uee_shar(u3_noun*);
    u3_noun u3uee_sign(u3_noun*);
    u3_noun u3uee_sign_octs(u3_noun*);
    u3_noun u3uee_sign_octs_raw(u3_noun*);
    u3_noun u3uee_sign_raw(u3_noun*);
    u3_noun u3uee_slar(u3_noun*);
    u3_noun u3uee_smac(u3_noun*);
    u3_noun u3uee_veri(u3_noun*);
    u3_noun u3uee_veri_octs(u3_noun*);
    u3_noun u3uef_drg(u3_noun*);
    u3_noun u3uef_lug(u3_noun*);
    u3_noun u3ueo_raw(u3_noun*);
    u3_noun u3ues_pbk(u3_noun*);
    u3_noun u3ues_pbl(u3_noun*);

  /* floating point: +rd, +rs, +rq, +rh
  */
#define U3U_REAL(pre)                   \
    u3_noun u3u##pre##_add(u3_noun*);   \
    u3_noun u3u##pre##_sub(u3_noun*);   \
    u3_noun u3u##pre##_mul(u3_noun*);   \
    u3_noun u3u##pre##_div(u3_noun*);   \
    u3_noun u3u##pre##_sqt(u3_noun*);   \
    u3_noun u3u##pre##_fma(u3_noun*);   \
    u3_noun u3u##pre##_lth(u3_noun*);   \
    u3_noun u3u##pre##_lte(u3_noun*);   \
    u3_noun u3u##pre##_equ(u3_noun*);   \
    u3_noun u3u##pre##_gte(u3_noun*);   \
    u3_noun u3u##pre##_gth(u3_noun*);
    U3U_REAL(er)
    U3U_REAL(et)
    U3U_REAL(eq)
    U3U_REAL(es)
#undef U3U_REAL

  /** Tier 6.
  **/
    u3_noun u3uf_cell(u3_noun*);
    u3_noun u3uf_comb(u3_noun*);
    u3_noun u3uf_cons(u3_noun*);
    u3_noun u3uf_core(u3_noun*);
    u3_noun u3uf_face(u3_noun*);
    u3_noun u3uf_fitz(u3_noun*);
    u3_noun u3uf_flan_139(u3_noun*);
    u3_noun u3uf_flip_139(u3_noun*);
    u3_noun u3uf_flor_139(u3_noun*);
    u3_noun u3uf_fork(u3_noun*);
    u3_noun u3uf_look(u3_noun*);
    u3_noun u3uf_loot(u3_noun*);
    u3_noun u3ufu_crop(u3_noun);
    u3_noun u3ufu_fish(u3_noun);
    u3_noun u3ufu_fuse(u3_noun);
    u3_noun u3ufu_mint(u3_noun);
    u3_noun u3ufu_mull(u3_noun);
    u3_noun u3ufu_nest_dext(u3_noun);
    u3_noun u3ufu_redo(u3_noun);
    u3_noun u3ufu_rest(u3_noun);

  /** Tier 7.
  **/
    u3_noun u3ug_plot_fax(u3_noun*);
    u3_noun u3ug_plot_met(u3_noun*);

#endif /* ifndef U3_JETS_U_H */
