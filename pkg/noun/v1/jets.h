/// @file

#ifndef U3_JETS_V1_H
#define U3_JETS_V1_H

#include "allocate.h"
#include "c3.h"
#include "types.h"

  /** Noun semantics.
  **/
#if 0
+=  location    $:  pattern=(each static dynamic)
                    name=term
                    hooks=(map term axis)
                ==
+=  static      (each payload=* parent=location)
+=  dynamic     [where=axis parent=location]
::
+=  registry    [roots=(map * location) parents=(list parent)]
+=  parent      (pair axis (map location location))
::
+=  activation  $:  hot-index=@ud
                    drivers=(map axis @ud)
                    label=path
                    jit=* :: FIXME: should probably be (map battery *)
                          :: since there can be multiple batteries per location
                ==
+=  hot-info    $:  reg=registry
                    hot-index=@ud
                    drivers=(map axis @ud)
                    label=path
                ==
+=  bash        @  :: battery hash (sha-256 based)
::
+=  hot         (map bash hot-info)
+=  cold        (map battery=^ (pair bash registry))
+=  warm        (map location activation)
#endif

  /** Data structures.
  ***
  *** All of these are transient structures allocated with malloc.
  **/
    /* u3j_v1_harm: jet arm.
    */
      typedef struct _u3j_v1_harm {
        c3_c*             fcs_c;               //  `.axe` or name
        u3_noun           (*fun_f)(u3_noun);   //  compute or 0 / semitransfer
        // c3_o           (*val_f)(u3_noun);   //  validate or 0 / retain
        c3_o              ice;                 //  perfect (don't test)
        c3_o              tot;                 //  total (never punts)
        c3_o              liv;                 //  live (enabled)
        c3_l              axe_l;               //  computed/discovered axis
        struct _u3j_v1_core* cop_u;               //  containing core
      } u3j_v1_harm;

    /* u3j_v1_hood: hook description.
     */
      typedef struct _u3j_v1_hood {
        c3_c*             nam_c;               //  hook name
        c3_l              axe_l;               //  hook axis (XX: direct)
        c3_o              kic_o;               //  hook is kick (vs. fragment)
        c3_l              sax_l;               //  hook subject axis (XX: direct)
      } u3j_v1_hood;

    /* u3j_v1_core: driver definition.
    */
      typedef struct _u3j_v1_core {
        c3_c*             cos_c;        //  control string
        c3_l              axe_l;        //  axis to parent
        struct _u3j_v1_harm* arm_u;        //  blank-terminated static list
        struct _u3j_v1_core* dev_u;        //  blank-terminated static list
        c3_c**            bas_u;        //  blank-terminated static list
        struct _u3j_v1_hood* huc_u;        //  blank-terminated static list
        struct _u3j_v1_core* par_u;        //  dynamic parent pointer
        c3_l              jax_l;        //  index in global dashboard
      } u3j_v1_core;

    /* u3j_v1_dash, u3_Dash, u3D: jet dashboard singleton
    */
      typedef struct _u3j_v1_dash {
        u3j_v1_core*     dev_u;            //  null-terminated static list
        c3_l          len_l;            //  dynamic array length
        c3_l          all_l;            //  allocated length
        u3j_v1_core*     ray_u;            //  dynamic array by axis
      } u3j_v1_dash;

    /* u3j_v1_fist: a single step in a fine check.
    */
      typedef struct {
        u3_noun bat;                  //  battery
        u3_noun pax;                  //  parent axis
      } u3j_v1_fist;

    /* u3j_v1_fink: (fine check) enough data to verify a located core.
    */
      typedef struct {
        c3_w    len_w;                //  number of fists
        u3_noun sat;                  //  static noun at end of check
        u3j_v1_fist fis_u[0];            //  fists
      } u3j_v1_fink;

    /* u3j_v1_rite: site of a %fast, used to skip re-mining.
    */
      typedef struct {
        c3_o          own_o;          //  rite owns fink?
        u3_weak       clu;            //  cached product of clue formula
        u3p(u3j_v1_fink) fin_p;          //  fine check
      } u3j_v1_rite;

    /* u3j_v1_site: site of a kick (nock 9), used to cache call target.
    */
      struct _u3n_prog;
      typedef struct {
        u3p(struct _u3n_prog) pog_p;  //  program for formula
        u3_noun       axe;            //  axis
        u3_weak       bat;            //  battery (for verification)
        u3_weak       bas;            //  hash of battery (for hot find)
        u3_weak       loc;            //  location (for reaming)
        c3_o          jet_o;          //  have jet driver?
        c3_o          fon_o;          //  site owns fink?
        u3_weak       lab;            //  label (for tracing)
        u3j_v1_core*     cop_u;          //  jet core
        u3j_v1_harm*     ham_u;          //  jet arm
        u3p(u3j_v1_fink) fin_p;          //  fine check
      } u3j_v1_site;

  /** Globals.
  **/
    /* u3_Dash: jet dashboard.
    */
      extern u3j_v1_dash u3j_v1_Dash;
#     define u3D u3j_v1_Dash

    /**  Functions.
    **/
      /* u3j_v1_boot(): initialize jet system.
      */
        c3_w
        u3j_v1_boot(c3_o nuu_o);

      /* u3j_v1_clear(): clear jet table to re-register.
      */
        void
        u3j_v1_clear(void);

      /* u3j_v1_cook():
      **
      **   Execute hook from core, call site cached by arbitrary c string
      */
        u3_noun
        u3j_v1_cook(const c3_c* key_c,
                 u3_noun     cor,
                 const c3_c* tam_c);

      /* u3j_v1_hook():
      **
      **   Execute hook from core.
      */
        u3_noun
        u3j_v1_hook(u3_noun     cor,
                 const c3_c* tam_c);

      /* u3j_v1_soft():
      **
      **   Execute hook from core, without jet.
      */
        u3_noun
        u3j_v1_soft(u3_noun     cor,
                 const c3_c* tam_c);

      /* u3j_v1_kick(): try to kick by jet.  If no kick, produce u3_none.
      **
      ** `axe` is RETAINED by the caller; `cor` is RETAINED iff there
      ** is no kick, TRANSFERRED if one.
      */
        u3_weak
        u3j_v1_kick(u3_noun cor, u3_noun axe);

      /* u3j_v1_kink(): kick either by jet or by nock.
      */
        u3_noun
        u3j_v1_kink(u3_noun cor,
                   u3_noun axe);

      /* u3j_v1_mine(): register core for jets.
      */
        void
        u3j_v1_mine(u3_noun clu,
                   u3_noun cor);

      /* u3j_v1_ream(): refresh after restoring from checkpoint.
      */
        void
        u3j_v1_ream(void);

      /* u3j_v1_stay(): extract cold state
      */
        u3_noun
        u3j_v1_stay(void);

      /* u3j_v1_load(): inject cold state
      */
        void
        u3j_v1_load(u3_noun rel);

      /* u3j_v1_reap(): promote jet state.
      */
        void
        u3j_v1_reap(u3a_jets jed_u);

      /* u3j_v1_take(): copy junior jet state.
      */
        u3a_jets
        u3j_v1_take(u3a_jets jed_u);

      /* u3j_v1_rite_mine(): mine cor with clu, using u3j_v1_rite for caching
      */
        void
        u3j_v1_rite_mine(u3j_v1_rite* rit_u, u3_noun clu, u3_noun cor);

      /* u3j_v1_rite_take(): copy junior rite references from src_u to dst_u.
      */
        void
        u3j_v1_rite_take(u3j_v1_rite* dst_u, u3j_v1_rite* src_u);

      /* u3j_v1_rite_merge(): copy rite references from src_u to dst_u,
      **                   losing old references
      */
        void
        u3j_v1_rite_merge(u3j_v1_rite* dst_u, u3j_v1_rite* src_u);

      /* u3j_v1_site_take(): copy junior site references.
      */
        void
        u3j_v1_site_take(u3j_v1_site* dst_u, u3j_v1_site* src_u);

      /* u3j_v1_site_merge(): copy site references from src_u to dst_u,
      **                   losing old references
      */
        void
        u3j_v1_site_merge(u3j_v1_site* dst_u, u3j_v1_site* src_u);

      /* u3j_v1_site_ream(): refresh u3j_v1_site after restoring from checkpoint
      */
        void
        u3j_v1_site_ream(u3j_v1_site* sit_u);

      /* u3j_v1_site_kick(): kick a core with a u3j_v1_site cache.
       */
        u3_weak
        u3j_v1_site_kick(u3_noun cor, u3j_v1_site* sit_u);

      /* u3j_v1_gate_prep(): prepare a locally cached gate to call repeatedly.
       */
        void
        u3j_v1_gate_prep(u3j_v1_site* sit_u, u3_noun cor);

      /* u3j_v1_gate_slam(): slam a site prepared by u3j_v1_gate_find() with sample.
       */
        u3_noun
        u3j_v1_gate_slam(u3j_v1_site* sit_u, u3_noun sam);

      /* u3j_v1_gate_lose(): clean up site prepared by u3j_v1_gate_find().
       */
        void
        u3j_v1_gate_lose(u3j_v1_site* sit_u);

      /* u3j_v1_rite_mark(): mark u3j_v1_rite for gc.
      */
        c3_w
        u3j_v1_rite_mark(u3j_v1_rite* rit_u);

      /* u3j_v1_rite_lose(): lose references of u3j_v1_rite (but do not free).
      */
        void
        u3j_v1_rite_lose(u3j_v1_rite* rit_u);

      /* u3j_v1_site_lose(): lose references of u3j_v1_site (but do not free).
      */
        void
        u3j_v1_site_lose(u3j_v1_site* sit_u);

      /* u3j_v1_site_mark(): mark u3j_v1_site for gc.
      */
        c3_w
        u3j_v1_site_mark(u3j_v1_site* sit_u);

      /* u3j_v1_mark(): mark jet state for gc.
      */
        c3_w
        u3j_v1_mark(FILE* fil_u);

      /* u3j_v1_free(): free jet state.
      */
        void
        u3j_v1_free(void);

      /* u3j_v1_reclaim(): clear ad-hoc persistent caches to reclaim memory.
      */
        void
        u3j_v1_reclaim(void);

      /* u3j_v1_rewrite_compact(): rewrite jet state for compaction.
      */
        void
        u3j_v1_rewrite_compact();

#endif /* ifndef U3_JETS_H */
