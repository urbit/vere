/// @file
///
/// Body of the u3nc interpreter loop, _nc_burn().  Included by
/// nock-compile.c once per road direction, with _nc_mov_ws defined as -1
/// (north road: the stack grows down) or 1 (south road: it grows up) and
/// _nc_burn defined as the name of the function, so that the stack
/// direction and the reference-counting fast paths are compile-time
/// constants.  Not a standalone header.

/* _nc_burn(): run a program on its arguments.  TRANSFERS the arguments.
**
**   An activation is the callee's slots, then a frame holding the
**   caller's state, pushed only once the call is known to run: a call
**   site copies its arguments straight into the slots of the callee's
**   new activation, retained, and tries the array jet on them there;
**   a hit pops the slots and leaves no other trace.  A tail call copies
**   its arguments to a temporary above its own activation, moves the
**   caller's references into it, drops what the caller had left, and
**   moves the temporary down in its place, under the same frame.
*/
static u3_noun
_nc_burn(u3nc_prog* pog_u, u3_noun* arg, c3_w len_w)
{
  const c3_ws mov_ws = _nc_mov_ws;

#define X(op) &&do_##op,
  static const void* lab[] = { OPCODES };
#undef X

  u3a_road* const rod_u = u3R;
  c3_y*      ip;          //  next byte of the bytecode
  u3_noun*   reg;
  u3_noun*   nex;
  nc_frame*  fam_u;
  nc_frame   fam;
  u3nc_prog* gop_u;
  u3nc_dire* dir_u;
  c3_w*      sot_w;
  c3_w       a_w, b_w, c_w, d_w, i_w;
  u3_noun    x, o, pro, hin;
  u3_noun    out;         //  out-parameter of hint/hilt calls, address-taken
  u3_noun    jar[2];      //  argument array of a jet op, address-taken
  c3_w       kni_w;       //  argument cursor of _nc_knit(), address-taken
  u3_post    emp_p = rod_u->cap_p;

#define RB()  (*ip++)
#define RS()  ({ c3_w _v = ip[0] | (ip[1] << 8); ip += 2; _v; })
#define RV()  ({                                                         \
    c3_y _n = *ip++;                                                     \
    c3_w _v = 0;                                                         \
    for ( c3_y _i = 0; _i < _n; _i++ ) {                                 \
      _v |= ((c3_w)*ip++) << (8 * _i);                                   \
    }                                                                    \
    _v;                                                                  \
  })
#define BURN()  goto *lab[*ip++]
#define JUMP(t)  (ip = pog_u->byc_u.ops_y + (t))
#define PUT(d, v)  do { u3_noun _o = reg[d]; reg[d] = (v); LOSE(_o); } while ( 0 )
#define PUSH(n)  _nc_push(rod_u, mov_ws, n)
#define POP(n)   _nc_pop(rod_u, mov_ws, n)
#define TOP(n)   _nc_top(rod_u, mov_ws, n)

#define ARG1(op, a)                                                      \
  do_##op##_B: a = RB(); goto op##_in;                                   \
  do_##op##_S: a = RS(); goto op##_in;                                   \
  do_##op##_V: a = RV();                                                 \
  op##_in:
#define ARG2(op, a, b)                                                   \
  do_##op##_B: a = RB(); b = RB(); goto op##_in;                         \
  do_##op##_S: a = RS(); b = RS(); goto op##_in;                         \
  do_##op##_V: a = RV(); b = RV();                                       \
  op##_in:
#define ARG3(op, a, b, c)                                                \
  do_##op##_B: a = RB(); b = RB(); c = RB(); goto op##_in;               \
  do_##op##_S: a = RS(); b = RS(); c = RS(); goto op##_in;               \
  do_##op##_V: a = RV(); b = RV(); c = RV();                             \
  op##_in:

//  FRAME(): push a frame recording this activation, its product to d_w
//
#define FRAME()  do {                                                    \
    fam_u = PUSH(_nc_frame_w);                                           \
    fam_u->pog_u = pog_u;                                                \
    fam_u->reg   = reg;                                                  \
    fam_u->ip_w  = ip - pog_u->byc_u.ops_y;                              \
    fam_u->des_w = d_w;                                                  \
    fam_u->key   = u3_none;                                              \
    fam_u->cid_w = 0;                                                    \
  } while ( 0 )

//  SITE(): the call site a_w, its callee gop_u, its argument slots
//          sot_w and their number len_w
//
#define SITE()  do {                                                     \
    dir_u = &(pog_u->dir_u.dat_u[a_w]);                                  \
    if ( dir_u->pog_p ) {                                                \
      gop_u = u3to(u3nc_prog, dir_u->pog_p);                             \
    }                                                                    \
    else {                                                               \
      u3t_off(noc_o);                                                    \
      gop_u = _nc_callee(dir_u);                                         \
      u3t_on(noc_o);                                                     \
    }                                                                    \
    sot_w = pog_u->sot_u.sot_w + dir_u->sot_w;                           \
    len_w = dir_u->len_w;                                                \
  } while ( 0 )

//  GATHER(): push num words and copy the site's arguments into them
//
#define GATHER(num)  do {                                                \
    nex = PUSH(num);                                                     \
    if ( 1 == len_w ) {                                                  \
      nex[0] = reg[sot_w[0]];                                            \
    }                                                                    \
    else if ( 2 == len_w ) {                                             \
      nex[0] = reg[sot_w[0]];                                            \
      nex[1] = reg[sot_w[1]];                                            \
    }                                                                    \
    else {                                                               \
      for ( i_w = 0; i_w < len_w; i_w++ ) {                              \
        nex[i_w] = reg[sot_w[i_w]];                                      \
      }                                                                  \
    }                                                                    \
  } while ( 0 )

  reg = PUSH(pog_u->tot_w);

  for ( i_w = 0; i_w < len_w; i_w++ ) {
    reg[i_w] = arg[i_w];
  }

  fam_u = PUSH(_nc_frame_w);
  fam_u->pog_u = 0;

  enter:
    //  pog_u: the callee, its len_w arguments in its slots at reg
    //
    for ( i_w = len_w; i_w < pog_u->tot_w; i_w++ ) {
      reg[i_w] = 0;
    }

    ip = pog_u->byc_u.ops_y;
    BURN();

  call:
    //  gop_u: the callee; x: its one argument, transferred;
    //  d_w: the slot for its product
    //
    nex    = PUSH(gop_u->tot_w);
    nex[0] = x;
    FRAME();
    pog_u  = gop_u;
    reg    = nex;
    len_w  = 1;
    goto enter;

  tail:
    //  gop_u: the callee; x: its one argument, transferred
    //
    for ( i_w = 0; i_w < pog_u->tot_w; i_w++ ) {
      LOSE(reg[i_w]);
    }
    fam = *(nc_frame*)TOP(_nc_frame_w);
    POP(_nc_frame_w + pog_u->tot_w);

    pog_u  = gop_u;
    reg    = PUSH(pog_u->tot_w);
    reg[0] = x;
    *(nc_frame*)PUSH(_nc_frame_w) = fam;
    len_w  = 1;
    goto enter;

  done:
    //  pro: the product, transferred
    //
    for ( i_w = 0; i_w < pog_u->tot_w; i_w++ ) {
      LOSE(reg[i_w]);
    }

    fam_u = TOP(_nc_frame_w);

    if ( !fam_u->pog_u ) {
      POP(_nc_frame_w + pog_u->tot_w);
      u3_assert( emp_p == rod_u->cap_p );
      return pro;
    }

    if ( u3_none != fam_u->key ) {
      _nc_save(fam_u->cid_w, fam_u->key, pro);
      LOSE(fam_u->key);
    }

    POP(_nc_frame_w + pog_u->tot_w);
    pog_u = fam_u->pog_u;
    reg   = fam_u->reg;
    ip    = pog_u->byc_u.ops_y + fam_u->ip_w;
    d_w   = fam_u->des_w;
    PUT(d_w, pro);
    BURN();

  {
    do_IMM_0:
      d_w = RB();
      PUT(d_w, 0);
      BURN();

    do_IMM_1:
      d_w = RB();
      PUT(d_w, 1);
      BURN();

    do_IMM_B:
      a_w = RB();
      d_w = RB();
      PUT(d_w, a_w);
      BURN();

    do_IMM_S:
      a_w = RS();
      d_w = RB();
      PUT(d_w, a_w);
      BURN();

    ARG2(IML, a_w, d_w)
      PUT(d_w, GAIN(pog_u->lit_u.non[a_w]));
      BURN();

    ARG2(MOV, a_w, d_w)
      PUT(d_w, GAIN(reg[a_w]));
      BURN();

    ARG2(INC, a_w, d_w)
      PUT(d_w, u3i_vint(GAIN(reg[a_w])));
      BURN();

    //  jetted calls the interpreter knows: the jets retain their arguments
    //
    ARG2(DEC, a_w, d_w)
      _nc_stat(jet_d);
      PUT(d_w, u3ua_dec(&(reg[a_w])));
      BURN();

    ARG3(ADD, a_w, b_w, d_w)
      _nc_stat(jet_d);
      jar[0] = reg[a_w];
      jar[1] = reg[b_w];
      PUT(d_w, u3ua_add(jar));
      BURN();

    ARG3(CON, a_w, b_w, d_w)
      PUT(d_w, u3nc(GAIN(reg[a_w]), GAIN(reg[b_w])));
      BURN();

    ARG2(HED, a_w, d_w)
      x = reg[a_w];
      PUT(d_w, ( c3y == u3du(x) ) ? GAIN(u3h(x)) : 0);
      BURN();

    ARG2(TAL, a_w, d_w)
      x = reg[a_w];
      PUT(d_w, ( c3y == u3du(x) ) ? GAIN(u3t(x)) : 0);
      BURN();

    ARG1(CEL, a_w)
      if ( c3n == u3du(reg[a_w]) ) {
        u3m_bail(c3__exit);
      }
      BURN();

    ARG1(LOB, a_w)
      if ( reg[a_w] > 1 ) {
        u3m_bail(c3__exit);
      }
      BURN();

    ARG2(EQU, a_w, b_w)
      u3r_sing(reg[a_w], reg[b_w]);
      BURN();

    ARG1(HSP, a_w)
      u3n_hilt_fore(GAIN(pog_u->lit_u.non[a_w]), 0, &out);
      *(u3_noun*)PUSH(1) = out;
      BURN();

    ARG1(HSE, a_w)
      x = *(u3_noun*)TOP(1);
      POP(1);
      u3n_hilt_hind(x, 0);
      BURN();

    ARG2(HDP, a_w, b_w)
      hin = pog_u->lit_u.non[a_w];
      o   = GAIN(reg[b_w]);

      switch ( u3h(hin) ) {
        case c3__hunk:
        case c3__lose:
        case c3__mean:
        case c3__spot: {
          u3t_push(u3nc(GAIN(u3h(hin)), o));
          x = u3_nul;
        } break;

        case c3__live: {
          if ( c3y == u3ud(o) ) {
            u3t_heck(o);
          }
          else {
            LOSE(o);
          }
          x = u3_nul;
        } break;

        case c3__slog: {
          if ( !(u3C.wag_h & u3o_quiet) ) {
            u3t_slog(o);
          }
          else {
            LOSE(o);
          }
          x = u3_nul;
        } break;

        //  %loop needs the subject, which isn't at hand; %hand is unknown
        //
        case c3__loop:
        case c3__hand: {
          LOSE(o);
          x = u3_nul;
        } break;

        default: {
          out = o;
          u3n_hint_fore(GAIN(hin), 0, &out);
          x = out;
        } break;
      }

      *(u3_noun*)PUSH(1) = x;
      BURN();

    ARG2(HDE, a_w, b_w)
      hin = pog_u->lit_u.non[a_w];
      x   = *(u3_noun*)TOP(1);
      POP(1);

      switch ( u3h(hin) ) {
        case c3__hunk:
        case c3__lose:
        case c3__mean:
        case c3__spot: {
          u3t_drop();
        } break;

        case c3__live:
        case c3__slog:
        case c3__loop:
        case c3__hand: {
        } break;

        default: {
          u3n_hint_hind(x, 0);
        } break;
      }
      BURN();

    ARG3(SPY, a_w, b_w, d_w)
      x = u3m_soft_esc(GAIN(reg[a_w]), GAIN(reg[b_w]));

      if ( c3n == u3du(x) ) {
        u3m_bail(u3nc(1, GAIN(reg[b_w])));
      }
      else if ( c3n == u3du(u3t(x)) ) {
        u3t_push(u3nt(c3__hunk, GAIN(reg[a_w]), GAIN(reg[b_w])));
        u3m_bail(c3__exit);
      }

      PUT(d_w, GAIN(u3t(u3t(x))));
      LOSE(x);
      BURN();

    ARG3(NOK, a_w, b_w, d_w)
      x     = reg[a_w];
      u3t_off(noc_o);
      gop_u = _nc_entry(x, reg[b_w]);
      u3t_on(noc_o);
      GAIN(x);
      goto call;

    ARG2(CAL, a_w, d_w)
      SITE();
      GATHER(gop_u->tot_w);

      if ( dir_u->arm_u && (u3_none != (pro = dir_u->arm_u->arg_f(nex))) ) {
        _nc_stat(jet_d);
        POP(gop_u->tot_w);
        PUT(d_w, pro);
        BURN();
      }
      goto cal_go;

    ARG2(CAM, a_w, d_w)
      SITE();
      GATHER(gop_u->tot_w);

      kni_w = 0;
      x     = u3nc(_nc_knit(gop_u->ned, nex, &kni_w), GAIN(u3t(dir_u->bell)));
      o   = u3z_find_m(dir_u->cid_w, 144 + c3__nock, x);

      if ( u3_none != o ) {
        POP(gop_u->tot_w);
        LOSE(x);
        PUT(d_w, o);
        BURN();
      }

      for ( i_w = 0; i_w < len_w; i_w++ ) {
        GAIN(nex[i_w]);
      }
      FRAME();
      fam_u->key   = x;
      fam_u->cid_w = dir_u->cid_w;
      goto cal_in;

    cal_go:
      for ( i_w = 0; i_w < len_w; i_w++ ) {
        GAIN(nex[i_w]);
      }
      FRAME();

    cal_in:
      _nc_stat(dir_d);
      pog_u = gop_u;
      reg   = nex;
      goto enter;

    ARG3(CSL, a_w, b_w, d_w)
      dir_u = &(pog_u->dir_u.dat_u[a_w]);
      x     = reg[b_w];

      if ( dir_u->ham_u ) {
        pro = u3j_kick_arm(GAIN(x), dir_u->ham_u, u3t(dir_u->ring));

        if ( u3_none != pro ) {
          _nc_stat(jet_d);
          PUT(d_w, pro);
          BURN();
        }
        LOSE(x);
      }
      goto csl_go;

    ARG3(CSM, a_w, b_w, d_w)
      dir_u = &(pog_u->dir_u.dat_u[a_w]);
      x     = reg[b_w];
      o     = u3nc(GAIN(x), GAIN(u3t(dir_u->bell)));
      pro   = u3z_find_m(dir_u->cid_w, 144 + c3__nock, o);

      if ( u3_none != pro ) {
        LOSE(o);
        PUT(d_w, pro);
        BURN();
      }

      _nc_stat(sub_d);
      u3t_off(noc_o);
      gop_u = _nc_entry(x, u3t(dir_u->bell));
      u3t_on(noc_o);
      nex    = PUSH(gop_u->tot_w);
      nex[0] = GAIN(x);
      FRAME();
      fam_u->key   = o;
      fam_u->cid_w = dir_u->cid_w;
      pog_u  = gop_u;
      reg    = nex;
      len_w  = 1;
      goto enter;

    csl_go:
      _nc_stat(sub_d);
      u3t_off(noc_o);
      gop_u = _nc_entry(x, u3t(dir_u->bell));
      u3t_on(noc_o);
      GAIN(x);
      goto call;

    ARG2(CLQ, a_w, b_w)
      if ( c3n == u3du(reg[a_w]) ) {
        JUMP(b_w);
      }
      BURN();

    ARG3(EQQ, a_w, b_w, c_w)
      if ( c3n == u3r_sing(reg[a_w], reg[b_w]) ) {
        JUMP(c_w);
      }
      BURN();

    ARG3(EQI, a_w, b_w, c_w)
      if ( reg[b_w] != a_w ) {
        JUMP(c_w);
      }
      BURN();

    ARG3(EQL, a_w, b_w, c_w)
      if ( c3n == u3r_sing(pog_u->lit_u.non[a_w], reg[b_w]) ) {
        JUMP(c_w);
      }
      BURN();

    ARG2(BRN, a_w, b_w)
      x = reg[a_w];
      if ( 1 == x ) {
        JUMP(b_w);
      }
      else if ( 0 != x ) {
        u3m_bail(c3__exit);
      }
      BURN();

    ARG2(BRZ, a_w, b_w)
      if ( 0 != reg[a_w] ) {
        JUMP(b_w);
      }
      BURN();

    ARG1(HOP, a_w)
      JUMP(a_w);
      BURN();

    ARG1(JMP, a_w)
      SITE();

      //  without a jet to try first, distinct argument slots move to the
      //  callee in one pass
      //
      if ( !dir_u->arm_u && (c3y == dir_u->uni_o) ) {
        nex = PUSH(len_w);
        for ( i_w = 0; i_w < len_w; i_w++ ) {
          u3_noun* sot = &(reg[sot_w[i_w]]);
          nex[i_w] = *sot;
          *sot     = 0;
        }
        _nc_stat(dir_d);
        goto jmp_go;
      }

      GATHER(len_w);

      if ( dir_u->arm_u && (u3_none != (pro = dir_u->arm_u->arg_f(nex))) ) {
        _nc_stat(jet_d);
        POP(len_w);
        goto done;
      }

      //  the caller's references move to the callee: the first argument
      //  from a slot takes its reference, any other from the same slot
      //  finds it gone and gains one
      //
      _nc_stat(dir_d);
      for ( i_w = 0; i_w < len_w; i_w++ ) {
        u3_noun* sot = &(reg[sot_w[i_w]]);

        if ( *sot ) {
          *sot = 0;
        }
        else {
          GAIN(nex[i_w]);
        }
      }

    jmp_go:
      for ( i_w = 0; i_w < pog_u->tot_w; i_w++ ) {
        LOSE(reg[i_w]);
      }
      POP(len_w);
      fam = *(nc_frame*)TOP(_nc_frame_w);
      POP(_nc_frame_w + pog_u->tot_w);

      pog_u = gop_u;
      reg   = PUSH(pog_u->tot_w);
      memmove(reg, nex, len_w * sizeof(u3_noun));
      *(nc_frame*)PUSH(_nc_frame_w) = fam;
      goto enter;

    ARG2(JSP, a_w, b_w)
      dir_u = &(pog_u->dir_u.dat_u[a_w]);
      x     = reg[b_w];

      if ( dir_u->ham_u ) {
        pro = u3j_kick_arm(GAIN(x), dir_u->ham_u, u3t(dir_u->ring));

        if ( u3_none != pro ) {
          _nc_stat(jet_d);
          goto done;
        }
        LOSE(x);
      }

      _nc_stat(sub_d);
      u3t_off(noc_o);
      gop_u = _nc_entry(x, u3t(dir_u->bell));
      u3t_on(noc_o);
      reg[b_w] = 0;
      goto tail;

    ARG1(DON, a_w)
      pro      = reg[a_w];
      reg[a_w] = 0;
      goto done;

    do_BOM:
      u3m_bail(c3__exit);
  }

#undef RB
#undef RS
#undef RV
#undef BURN
#undef JUMP
#undef PUT
#undef PUSH
#undef POP
#undef TOP
#undef ARG1
#undef ARG2
#undef ARG3
#undef FRAME
#undef SITE
#undef GATHER
}

