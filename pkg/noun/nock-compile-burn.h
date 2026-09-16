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
_nc_burn(u3nc_prog* pog_u, u3_noun* arg, c3_h len_h)
{
  const c3_ws mov_ws = _nc_mov_ws;

#define X(op) &&do_##op,
  static const void* lab[] = { OPCODES };
#undef X

  u3a_road*  rod_u = u3R;
  c3_y*      ip;          //  next byte of the bytecode
  u3_noun*   reg;
  u3_noun*   nex;
  nc_frame*  fam_u;
  nc_frame   fam;
  u3nc_prog* gop_u;
  u3nc_dire* dir_u;
  c3_h*      sot_h;
  c3_h       a_h, b_h, c_h, d_h, i_h;
  u3_noun    x, o, pro, hin;
  u3_noun    out;         //  out-parameter of hint/hilt calls, address-taken
  c3_h       kni_h;       //  argument cursor of _nc_knit(), address-taken
  u3_post    emp_p = rod_u->cap_p;

#define RB()  (*ip++)
#define RS()  ({ c3_h _v = ip[0] | (ip[1] << 8); ip += 2; _v; })
#define RV()  ({                                                         \
    c3_y _n = *ip++;                                                     \
    c3_h _v = 0;                                                         \
    for ( c3_y _i = 0; _i < _n; _i++ ) {                                 \
      _v |= ((c3_h)*ip++) << (8 * _i);                                   \
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

//  FRAME(): push a frame recording this activation, its product to d_h
//
#define FRAME()  do {                                                    \
    fam_u = PUSH(_nc_frame_w);                                           \
    fam_u->pog_u = pog_u;                                                \
    fam_u->reg   = reg;                                                  \
    fam_u->ip_h  = ip - pog_u->byc_u.ops_y;                              \
    fam_u->des_h = d_h;                                                  \
    fam_u->key   = u3_none;                                              \
    fam_u->cid_h = 0;                                                    \
  } while ( 0 )

//  SITE(): the call site a_h, its callee gop_u, its argument slots
//          sot_h and their number len_h
//
#define SITE()  do {                                                     \
    dir_u = &(pog_u->dir_u.dat_u[a_h]);                                  \
    if ( dir_u->pog_p ) {                                                \
      gop_u = u3to(u3nc_prog, dir_u->pog_p);                             \
    }                                                                    \
    else {                                                               \
      u3t_off(noc_o);                                                    \
      gop_u = _nc_callee(dir_u);                                         \
      u3t_on(noc_o);                                                     \
    }                                                                    \
    sot_h = pog_u->sot_u.sot_h + dir_u->sot_h;                           \
    len_h = dir_u->len_h;                                                \
  } while ( 0 )

//  GATHER(): push num words and copy the site's arguments into them
//
#define GATHER(num)  do {                                                \
    nex = PUSH(num);                                                     \
    if ( 1 == len_h ) {                                                  \
      nex[0] = reg[sot_h[0]];                                            \
    }                                                                    \
    else if ( 2 == len_h ) {                                             \
      nex[0] = reg[sot_h[0]];                                            \
      nex[1] = reg[sot_h[1]];                                            \
    }                                                                    \
    else {                                                               \
      for ( i_h = 0; i_h < len_h; i_h++ ) {                              \
        nex[i_h] = reg[sot_h[i_h]];                                      \
      }                                                                  \
    }                                                                    \
  } while ( 0 )

  reg = PUSH(pog_u->tot_h);

  for ( i_h = 0; i_h < len_h; i_h++ ) {
    reg[i_h] = arg[i_h];
  }

  fam_u = PUSH(_nc_frame_w);
  fam_u->pog_u = NULL;

  enter:
    //  pog_u: the callee, its len_h arguments in its slots at reg
    //
    for ( i_h = len_h; i_h < pog_u->tot_h; i_h++ ) {
      reg[i_h] = 0;
    }

    ip = pog_u->byc_u.ops_y;
    BURN();

  call:
    //  gop_u: the callee; x: its one argument, transferred;
    //  d_h: the slot for its product
    //
    nex    = PUSH(gop_u->tot_h);
    nex[0] = x;
    FRAME();
    pog_u  = gop_u;
    reg    = nex;
    len_h  = 1;
    goto enter;

  tail:
    //  gop_u: the callee; x: its one argument, transferred
    //
    for ( i_h = 0; i_h < pog_u->tot_h; i_h++ ) {
      LOSE(reg[i_h]);
    }
    fam = *(nc_frame*)TOP(_nc_frame_w);
    POP(_nc_frame_w + pog_u->tot_h);

    pog_u  = gop_u;
    reg    = PUSH(pog_u->tot_h);
    reg[0] = x;
    *(nc_frame*)PUSH(_nc_frame_w) = fam;
    len_h  = 1;
    goto enter;

  done:
    //  pro: the product, transferred
    //
    for ( i_h = 0; i_h < pog_u->tot_h; i_h++ ) {
      LOSE(reg[i_h]);
    }

    fam_u = TOP(_nc_frame_w);

    if ( !fam_u->pog_u ) {
      POP(_nc_frame_w + pog_u->tot_h);
      u3_assert( emp_p == rod_u->cap_p );
      return pro;
    }

    if ( u3_none != fam_u->key ) {
      _nc_save(fam_u->cid_h, fam_u->key, pro);
      LOSE(fam_u->key);
    }

    POP(_nc_frame_w + pog_u->tot_h);
    pog_u = fam_u->pog_u;
    reg   = fam_u->reg;
    ip    = pog_u->byc_u.ops_y + fam_u->ip_h;
    d_h   = fam_u->des_h;
    PUT(d_h, pro);
    BURN();

  {
    do_IMM_0:
      d_h = RB();
      PUT(d_h, 0);
      BURN();

    do_IMM_1:
      d_h = RB();
      PUT(d_h, 1);
      BURN();

    do_IMM_B:
      a_h = RB();
      d_h = RB();
      PUT(d_h, a_h);
      BURN();

    do_IMM_S:
      a_h = RS();
      d_h = RB();
      PUT(d_h, a_h);
      BURN();

    ARG2(IML, a_h, d_h)
      PUT(d_h, GAIN(pog_u->lit_u.non[a_h]));
      BURN();

    ARG2(MOV, a_h, d_h)
      PUT(d_h, GAIN(reg[a_h]));
      BURN();

    ARG2(INC, a_h, d_h)
      PUT(d_h, u3i_vint(GAIN(reg[a_h])));
      BURN();

    ARG3(CON, a_h, b_h, d_h)
      PUT(d_h, u3nc(GAIN(reg[a_h]), GAIN(reg[b_h])));
      BURN();

    ARG2(HED, a_h, d_h)
      x = reg[a_h];
      PUT(d_h, ( c3y == u3du(x) ) ? GAIN(u3h(x)) : 0);
      BURN();

    ARG2(TAL, a_h, d_h)
      x = reg[a_h];
      PUT(d_h, ( c3y == u3du(x) ) ? GAIN(u3t(x)) : 0);
      BURN();

    ARG1(CEL, a_h)
      if ( c3n == u3du(reg[a_h]) ) {
        u3m_bail(c3__exit);
      }
      BURN();

    ARG1(LOB, a_h)
      if ( reg[a_h] > 1 ) {
        u3m_bail(c3__exit);
      }
      BURN();

    ARG2(EQU, a_h, b_h)
      u3r_sing(reg[a_h], reg[b_h]);
      BURN();

    ARG1(HSP, a_h)
      u3n_hilt_fore(GAIN(pog_u->lit_u.non[a_h]), 0, &out);
      *(u3_noun*)PUSH(1) = out;
      BURN();

    ARG1(HSE, a_h)
      x = *(u3_noun*)TOP(1);
      POP(1);
      u3n_hilt_hind(x, 0);
      BURN();

    ARG2(HDP, a_h, b_h)
      hin = pog_u->lit_u.non[a_h];
      o   = GAIN(reg[b_h]);

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

    ARG2(HDE, a_h, b_h)
      hin = pog_u->lit_u.non[a_h];
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

    ARG3(SPY, a_h, b_h, d_h)
      x = u3m_soft_esc(GAIN(reg[a_h]), GAIN(reg[b_h]));

      if ( c3n == u3du(x) ) {
        u3m_bail(u3nc(1, GAIN(reg[b_h])));
      }
      else if ( c3n == u3du(u3t(x)) ) {
        u3t_push(u3nt(c3__hunk, GAIN(reg[a_h]), GAIN(reg[b_h])));
        u3m_bail(c3__exit);
      }

      PUT(d_h, GAIN(u3t(u3t(x))));
      LOSE(x);
      BURN();

    ARG3(NOK, a_h, b_h, d_h)
      x     = reg[a_h];
      u3t_off(noc_o);
      gop_u = _nc_entry(x, reg[b_h]);
      u3t_on(noc_o);
      GAIN(x);
      goto call;

    ARG2(CAL, a_h, d_h)
      SITE();
      GATHER(gop_u->tot_h);

      if ( dir_u->arm_u && (u3_none != (pro = dir_u->arm_u->arg_f(nex))) ) {
        _nc_stat(jet_d);
        POP(gop_u->tot_h);
        PUT(d_h, pro);
        BURN();
      }
      goto cal_go;

    ARG2(CAM, a_h, d_h)
      SITE();
      GATHER(gop_u->tot_h);

      kni_h = 0;
      x     = u3nc(_nc_knit(gop_u->ned, nex, &kni_h), GAIN(u3t(dir_u->bell)));
      o   = u3z_find_m(dir_u->cid_h, 144 + c3__nock, x);

      if ( u3_none != o ) {
        POP(gop_u->tot_h);
        LOSE(x);
        PUT(d_h, o);
        BURN();
      }

      for ( i_h = 0; i_h < len_h; i_h++ ) {
        GAIN(nex[i_h]);
      }
      FRAME();
      fam_u->key   = x;
      fam_u->cid_h = dir_u->cid_h;
      goto cal_in;

    cal_go:
      for ( i_h = 0; i_h < len_h; i_h++ ) {
        GAIN(nex[i_h]);
      }
      FRAME();

    cal_in:
      _nc_stat(dir_d);
      pog_u = gop_u;
      reg   = nex;
      goto enter;

    ARG3(CSL, a_h, b_h, d_h)
      dir_u = &(pog_u->dir_u.dat_u[a_h]);
      x     = reg[b_h];

      if ( dir_u->ham_u ) {
        pro = u3j_kick_arm(GAIN(x), dir_u->ham_u, u3t(dir_u->ring));

        if ( u3_none != pro ) {
          _nc_stat(jet_d);
          PUT(d_h, pro);
          BURN();
        }
        LOSE(x);
      }
      goto csl_go;

    ARG3(CSM, a_h, b_h, d_h)
      dir_u = &(pog_u->dir_u.dat_u[a_h]);
      x     = reg[b_h];
      o     = u3nc(GAIN(x), GAIN(u3t(dir_u->bell)));
      pro   = u3z_find_m(dir_u->cid_h, 144 + c3__nock, o);

      if ( u3_none != pro ) {
        LOSE(o);
        PUT(d_h, pro);
        BURN();
      }

      _nc_stat(sub_d);
      u3t_off(noc_o);
      gop_u = _nc_entry(x, u3t(dir_u->bell));
      u3t_on(noc_o);
      nex    = PUSH(gop_u->tot_h);
      nex[0] = GAIN(x);
      FRAME();
      fam_u->key   = o;
      fam_u->cid_h = dir_u->cid_h;
      pog_u  = gop_u;
      reg    = nex;
      len_h  = 1;
      goto enter;

    csl_go:
      _nc_stat(sub_d);
      u3t_off(noc_o);
      gop_u = _nc_entry(x, u3t(dir_u->bell));
      u3t_on(noc_o);
      GAIN(x);
      goto call;

    ARG2(CLQ, a_h, b_h)
      if ( c3n == u3du(reg[a_h]) ) {
        JUMP(b_h);
      }
      BURN();

    ARG3(EQQ, a_h, b_h, c_h)
      if ( c3n == u3r_sing(reg[a_h], reg[b_h]) ) {
        JUMP(c_h);
      }
      BURN();

    ARG3(EQI, a_h, b_h, c_h)
      if ( reg[b_h] != a_h ) {
        JUMP(c_h);
      }
      BURN();

    ARG3(EQL, a_h, b_h, c_h)
      if ( c3n == u3r_sing(pog_u->lit_u.non[a_h], reg[b_h]) ) {
        JUMP(c_h);
      }
      BURN();

    ARG2(BRN, a_h, b_h)
      x = reg[a_h];
      if ( 1 == x ) {
        JUMP(b_h);
      }
      else if ( 0 != x ) {
        u3m_bail(c3__exit);
      }
      BURN();

    ARG2(BRZ, a_h, b_h)
      if ( 0 != reg[a_h] ) {
        JUMP(b_h);
      }
      BURN();

    ARG1(HOP, a_h)
      JUMP(a_h);
      BURN();

    ARG1(JMP, a_h)
      SITE();

      //  without a jet to try first, distinct argument slots move to the
      //  callee in one pass
      //
      if ( !dir_u->arm_u && (c3y == dir_u->uni_o) ) {
        nex = PUSH(len_h);
        for ( i_h = 0; i_h < len_h; i_h++ ) {
          u3_noun* sot = &(reg[sot_h[i_h]]);
          nex[i_h] = *sot;
          *sot     = 0;
        }
        _nc_stat(dir_d);
        goto jmp_go;
      }

      GATHER(len_h);

      if ( dir_u->arm_u && (u3_none != (pro = dir_u->arm_u->arg_f(nex))) ) {
        _nc_stat(jet_d);
        POP(len_h);
        goto done;
      }

      //  the caller's references move to the callee: the first argument
      //  from a slot takes its reference, any other from the same slot
      //  finds it gone and gains one
      //
      _nc_stat(dir_d);
      for ( i_h = 0; i_h < len_h; i_h++ ) {
        u3_noun* sot = &(reg[sot_h[i_h]]);

        if ( *sot ) {
          *sot = 0;
        }
        else {
          GAIN(nex[i_h]);
        }
      }

    jmp_go:
      for ( i_h = 0; i_h < pog_u->tot_h; i_h++ ) {
        LOSE(reg[i_h]);
      }
      POP(len_h);
      fam = *(nc_frame*)TOP(_nc_frame_w);
      POP(_nc_frame_w + pog_u->tot_h);

      pog_u = gop_u;
      reg   = PUSH(pog_u->tot_h);
      memmove(reg, nex, len_h * sizeof(u3_noun));
      *(nc_frame*)PUSH(_nc_frame_w) = fam;
      goto enter;

    ARG2(JSP, a_h, b_h)
      dir_u = &(pog_u->dir_u.dat_u[a_h]);
      x     = reg[b_h];

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
      reg[b_h] = 0;
      goto tail;

    ARG1(DON, a_h)
      pro      = reg[a_h];
      reg[a_h] = 0;
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

