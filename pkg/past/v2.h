#ifndef U3_V2_H
#define U3_V2_H

#include "allocate.h"
#include "hashtable.h"
#include "jets.h"
#include "options.h"
#include "vortex.h"

  /***  allocate.h
  ***/
#     define u3a_v2_botox        u3a_botox
#     define u3a_v2_box          u3a_box
#     define u3a_v2_cell         u3a_cell
#     define u3a_v2_fbox         u3a_fbox
#     define u3a_v2_fbox_no      u3a_fbox_no
#     define u3a_v2_heap         u3a_heap
#     define u3a_v2_into         u3a_into
#     define u3a_v2_is_cat       u3a_is_cat
#     define u3a_v2_is_cell      u3a_is_cell
#     define u3a_v2_is_north     u3a_is_north
#     define u3a_v2_is_pom       u3a_is_pom
#     define u3a_v2_is_pug       u3a_is_pug
#     define u3a_v2_minimum      u3a_minimum
#     define u3a_v2_outa         u3a_outa
#     define u3a_v2_rewrite      u3a_rewrite
#     define u3a_v2_rewrite_ptr  u3a_rewrite_ptr
#     define u3a_v2_rewritten    u3a_rewritten
#     define u3a_v2_to_pug       u3a_to_pug
#     define u3a_v2_to_pom       u3a_to_pom
#     define u3a_v2_wfree        u3a_wfree

#     define u3v2to              u3to
#     define u3v2of              u3of

    /* u3a_v2_road: contiguous allocation and execution context.
    */
      typedef struct _u3a_v2_road {
        u3p(struct _u3a_v2_road) par_p;          //  parent road
        u3p(struct _u3a_v2_road) kid_p;          //  child road list
        u3p(struct _u3a_v2_road) nex_p;          //  sibling road

        u3p(c3_w) cap_p;                      //  top of transient region
        u3p(c3_w) hat_p;                      //  top of durable region
        u3p(c3_w) mat_p;                      //  bottom of transient region
        u3p(c3_w) rut_p;                      //  bottom of durable region
        u3p(c3_w) ear_p;                      //  original cap if kid is live

        c3_w fut_w[32];                       //  futureproof buffer

        struct {                              //  escape buffer
          union {
            jmp_buf buf;
            c3_w buf_w[256];                  //  futureproofing
          };
        } esc;

        struct {                              //  miscellaneous config
          c3_w fag_w;                         //  flag bits
        } how;                                //

        struct {                                   //  allocation pools
          u3p(u3a_v2_fbox) fre_p[u3a_v2_fbox_no];  //  heap by node size log
          u3p(u3a_fbox) cel_p;                //  custom cell allocator
          c3_w fre_w;                         //  number of free words
          c3_w max_w;                         //  maximum allocated
        } all;

        u3a_jets jed;                         //  jet dashboard

        struct {                              // bytecode state
          u3p(u3h_root) har_p;                // formula->post of bytecode
        } byc;

        struct {                              //  namespace
          u3_noun gul;                        //  (list $+(* (unit (unit)))) now
        } ski;

        struct {                              //  trace stack
          u3_noun tax;                        //  (list ,*)
          u3_noun mer;                        //  emergency buffer to release
        } bug;

        struct {                              //  profile stack
          c3_d    nox_d;                      //  nock steps
          c3_d    cel_d;                      //  cell allocations
          u3_noun don;                        //  (list batt)
          u3_noun trace;                      //  (list trace)
          u3_noun day;                        //  doss, only in u3H (moveme)
        } pro;

        struct {                              //  memoization
          u3p(u3h_root) har_p;                //  (map (pair term noun) noun)
        } cax;
      } u3a_v2_road;

  /**  Globals.
  **/
      /// Current road (thread-local).
      extern u3a_v2_road* u3a_v2_Road;
#       define u3R_v2  u3a_v2_Road


  /***  jets.h
  ***/
#     define  u3j_v2_fink       u3j_fink
#     define  u3j_v2_fist       u3j_fist
#     define  u3j_v2_hank       u3j_hank
#     define  u3j_v2_rite       u3j_rite
#     define  u3j_v2_site       u3j_site


  /***  hashtable.h
  ***/
#     define u3h_v2_buck          u3h_buck
#     define u3h_v2_node          u3h_node
#     define u3h_v2_root          u3h_root
#     define u3h_v2_slot_is_node  u3h_slot_is_node
#     define u3h_v2_slot_is_noun  u3h_slot_is_noun
#     define u3h_v2_noun_to_slot  u3h_noun_to_slot
#     define u3h_v2_slot_to_noun  u3h_slot_to_noun


  /***  nock.h
  ***/
    /* u3n_memo: %memo hint space
     */
    typedef struct {
      c3_l    sip_l;
      u3_noun key;
    } u3n_v2_memo;

  /* u3n_v2_prog: program compiled from nock
   */
  typedef struct _u3n_v2_prog {
    struct {
      c3_o      own_o;                // program owns ops_y?
      c3_w      len_w;                // length of bytecode (bytes)
      c3_y*     ops_y;                // actual array of bytes
    } byc_u;                          // bytecode
    struct {
      c3_w      len_w;                // number of literals
      u3_noun*  non;                  // array of literals
    } lit_u;                          // literals
    struct {
      c3_w      len_w;                // number of memo slots
      u3n_v2_memo* sot_u;             // array of memo slots
    } mem_u;                          // memo slot data
    struct {
      c3_w      len_w;                // number of calls sites
      u3j_v2_site* sit_u;             // array of sites
    } cal_u;                          // call site data
    struct {
      c3_w      len_w;                // number of registration sites
      u3j_v2_rite* rit_u;             // array of sites
    } reg_u;                          // registration site data
  } u3n_v2_prog;


  /***  options.h
  ***/
#     define u3C_v2  u3C


  /***  vortex.h
  ***/
#     define     u3v_v2_arvo  u3v_arvo

    /* u3v_v2_home: all internal (within image) state.
    **       NB: version must be last for discriminability in north road
    */
      typedef struct _u3v_v2_home {
        u3a_v2_road    rod_u;                //  storage state
        u3v_v2_arvo    arv_u;                //  arvo state
        u3v_version    ver_w;                //  version number
      } u3v_v2_home;

  /**  Globals.
  **/
      /// Arvo internal state.
      extern u3v_v2_home* u3v_v2_Home;
#       define u3H_v2  u3v_v2_Home
#       define u3A_v2  (&(u3v_v2_Home->arv_u))

#endif /* U3_V2_H */
