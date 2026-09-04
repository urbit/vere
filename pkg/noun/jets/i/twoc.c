/// @file
///
/// Jets for the numerics `/lib/twoc` library: two's-complement signed integer
/// arithmetic, MODULAR (operations wrap mod 2^width rather than crashing on
/// overflow).  Userspace, registered under the `non` chapter alongside `math`,
/// `lagoon`, and `unum`.  Jet output is bit-identical to the unjetted Hoon.
///
/// `/lib/twoc` defines all its logic in ONE width-keyed door `++twid`
/// (`|_ wid=@`); the bloq-keyed `++twoc` facade just re-exports `twid` gates at
/// width `(bex bloq)`.  So we register a SINGLE `%twid` core and every caller --
/// `twid`-direct (`/lib/fixed`) and via the `twoc` facade (`/lib/unum`, Lagoon
/// `%int2`) -- dispatches here.  We read `wid` from the door sample (gate axis
/// 7 = door, door axis 6 = `wid`, so wid at gate axis 30; same shape `unum`
/// uses for `bloq`).
///
/// DISPATCH on `wid`:
///   - wid <= 64   : native c3_d (uint64), masked to wid bits.
///   - wid <= 128  : native unsigned __int128, masked to wid bits.
///   - wid  > 128  : GNU MP (mpz), masked to wid bits.
/// The power-of-two bloq widths (8/16/32/64/128 for bloq 3..7) all land in the
/// native paths; GMP is the exact backstop for genuinely arbitrary precision.
///
/// We DECLINE (return u3_none, so the pure-Hoon arm runs) when:
///   - any operand does not fit in wid bits (u3r_met(0,x) > wid) -- the Hoon
///     determines sign from the raw bit-width (xeb), which diverges from naive
///     masking only off the contract; declining keeps us bit-exact for free.
///   - a divisor is zero in div/rem -- the Hoon `div`/`rem` crash; let it.
///
/// MASTER COPY lives in urbit/numerics libmath/vere/noun/jets/i/twoc.c; applied
/// by hand to the vere runtime.  GMP is already vendored (ext/gmp) and linked
/// into pkg/noun.

#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"
#include "jets/i/twoc.h"   // shared native + GMP two's-complement kernels

//  wid from the twid door sample: gate axis 7 = door, door axis 6 = wid.
#define _TWOC_WID_AXIS 30

//  Read wid (the door sample) from a gate core; c3n if absent/not-atom.
static inline c3_t
_twoc_wid(u3_noun cor, c3_d* out)
{
  u3_noun w = u3r_at(_TWOC_WID_AXIS, cor);
  if ( u3_none == w || c3n == u3ud(w) ) {
    return c3n;
  }
  *out = u3r_chub(0, w);
  return c3y;
}

//  Does atom [a] fit in [wid] bits?  (Operands wider than wid are out of
//  contract; the Hoon's xeb-based sign detection would diverge from masking,
//  so we decline and let the Hoon run.)
static inline c3_t
_twoc_fits(u3_atom a, c3_d wid)
{
  return ( (c3_d)u3r_met(0, a) <= wid ) ? c3y : c3n;
}

//  loobean negation (c3y == 0, c3n == 1; C's ! would invert the convention).
static inline c3_t
_twoc_not(c3_t b)
{
  return ( c3y == b ) ? c3n : c3y;
}

//  read a wid<=128 operand (one or two chubs) into a __int128.
static inline _twoc_u128
_tn_rd128(u3_atom a)
{
  return ( ((_twoc_u128)u3r_chub(1, a)) << 64 ) | (_twoc_u128)u3r_chub(0, a);
}

//  emit a wid<=128 result (two chubs; u3i_chubs strips high zeros).
static inline u3_noun
_tn_em128(_twoc_u128 r)
{
  c3_d buf[2];
  buf[0] = (c3_d)r;
  buf[1] = (c3_d)(r >> 64);
  return u3i_chubs(2, buf);
}

/* ----------------------------------------------------------------------------
** Per-arm cores and wrappers.  Each wrapper extracts the sample, validates
** atoms, reads wid, applies the fits-guard, and dispatches to the core.
** -------------------------------------------------------------------------- */

//  binary op: (wid, a, b) -> @.  [g64]/[g128] are native; [gmp] block runs for
//  wid>128 and must set the result noun [res].
#define _TWOC_BINOP(nam, e64, e128, GMPBLOCK)                                  \
  u3_noun u3qi_twid_##nam(c3_d wid, u3_atom a, u3_atom b) {                     \
    if ( c3n == _twoc_fits(a, wid) || c3n == _twoc_fits(b, wid) ) {            \
      return u3_none;                                                          \
    }                                                                          \
    if ( wid <= 64 ) {                                                         \
      c3_d msk = _tn_msk64(wid), ua = u3r_chub(0, a), ub = u3r_chub(0, b);     \
      c3_d r = (e64);  return u3i_chubs(1, &r);                                \
    }                                                                          \
    if ( wid <= 128 ) {                                                        \
      _twoc_u128 msk = _tn_msk128(wid), ua = _tn_rd128(a), ub = _tn_rd128(b);  \
      _twoc_u128 r = (e128);  return _tn_em128(r);                             \
    }                                                                          \
    { u3_noun res;  mpz_t am, bm;                                              \
      u3r_mp(am, a);  u3r_mp(bm, b);                                           \
      GMPBLOCK                                                                 \
      return res;                                                              \
    }                                                                          \
  }                                                                            \
  u3_noun u3wi_twid_##nam(u3_noun cor) {                                       \
    u3_noun a, b;  c3_d wid;                                                   \
    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ||              \
         c3n == u3ud(a) || c3n == u3ud(b) ) return u3m_bail(c3__exit);         \
    if ( c3n == _twoc_wid(cor, &wid) ) return u3_none;                         \
    return u3qi_twid_##nam(wid, a, b);                                         \
  }

//  binary comparison: (wid, a, b) -> ?.  [c64]/[c128] native loobeans; GMP
//  block sets c3_t [v] from signed mpz compare.
#define _TWOC_CMP(nam, c64, c128, GMPCMP)                                      \
  u3_noun u3qi_twid_##nam(c3_d wid, u3_atom a, u3_atom b) {                     \
    if ( c3n == _twoc_fits(a, wid) || c3n == _twoc_fits(b, wid) ) {            \
      return u3_none;                                                          \
    }                                                                          \
    if ( wid <= 64 ) {                                                         \
      c3_d sb = wid - 1, ua = u3r_chub(0, a), ub = u3r_chub(0, b);             \
      return (c64);                                                            \
    }                                                                          \
    if ( wid <= 128 ) {                                                        \
      c3_d sb = wid - 1;  _twoc_u128 ua = _tn_rd128(a), ub = _tn_rd128(b);     \
      return (c128);                                                           \
    }                                                                          \
    { c3_t v;  mpz_t am, bm, as, bs;                                           \
      u3r_mp(am, a);  u3r_mp(bm, b);                                           \
      mpz_init(as);  mpz_init(bs);                                             \
      _tg_signed(as, am, wid);  _tg_signed(bs, bm, wid);                       \
      v = (GMPCMP) ? c3y : c3n;                                                \
      mpz_clear(am); mpz_clear(bm); mpz_clear(as); mpz_clear(bs);              \
      return v;                                                                \
    }                                                                          \
  }                                                                            \
  u3_noun u3wi_twid_##nam(u3_noun cor) {                                       \
    u3_noun a, b;  c3_d wid;                                                   \
    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ||              \
         c3n == u3ud(a) || c3n == u3ud(b) ) return u3m_bail(c3__exit);         \
    if ( c3n == _twoc_wid(cor, &wid) ) return u3_none;                         \
    return u3qi_twid_##nam(wid, a, b);                                         \
  }

//  unary op: (wid, a) -> @.
#define _TWOC_UNOP(nam, e64, e128, GMPBLOCK)                                   \
  u3_noun u3qi_twid_##nam(c3_d wid, u3_atom a) {                                \
    if ( c3n == _twoc_fits(a, wid) ) return u3_none;                           \
    if ( wid <= 64 ) {                                                         \
      c3_d msk = _tn_msk64(wid), sb = wid - 1, ua = u3r_chub(0, a);            \
      c3_d r = (e64);  (void)sb;  return u3i_chubs(1, &r);                     \
    }                                                                          \
    if ( wid <= 128 ) {                                                        \
      _twoc_u128 msk = _tn_msk128(wid);  c3_d sb = wid - 1;                    \
      _twoc_u128 ua = _tn_rd128(a);                                            \
      _twoc_u128 r = (e128);  (void)sb;  return _tn_em128(r);                  \
    }                                                                          \
    { u3_noun res;  mpz_t am;                                                  \
      u3r_mp(am, a);                                                           \
      GMPBLOCK                                                                 \
      return res;                                                              \
    }                                                                          \
  }                                                                            \
  u3_noun u3wi_twid_##nam(u3_noun cor) {                                       \
    u3_noun a = u3r_at(u3x_sam, cor);  c3_d wid;                               \
    if ( u3_none == a || c3n == u3ud(a) ) return u3m_bail(c3__exit);           \
    if ( c3n == _twoc_wid(cor, &wid) ) return u3_none;                         \
    return u3qi_twid_##nam(wid, a);                                            \
  }

//  add / sub / mul -- pure modular, GMP via add/sub/mul then mask.
_TWOC_BINOP(add, _tn_add64(ua, ub, msk), _tn_add128(ua, ub, msk),
  { mpz_add(am, am, bm);  _tg_mask(am, am, wid);
    mpz_clear(bm);  res = u3i_mp(am); })
_TWOC_BINOP(sub, _tn_sub64(ua, ub, msk), _tn_sub128(ua, ub, msk),
  { mpz_sub(am, am, bm);  _tg_mask(am, am, wid);
    mpz_clear(bm);  res = u3i_mp(am); })
_TWOC_BINOP(mul, _tn_mul64(ua, ub, msk), _tn_mul128(ua, ub, msk),
  { mpz_mul(am, am, bm);  _tg_mask(am, am, wid);
    mpz_clear(bm);  res = u3i_mp(am); })

/* ++div / ++rem:pp -- signed, truncating toward zero (C-style); the remainder's
** sign follows the DIVIDEND.  A zero divisor crashes the Hoon (`div`/`rem`),
** so we DECLINE (u3_none) and let the Hoon bail -- in every path, including
** native, where `qa / qb` with qb==0 would otherwise SIGFPE the serf.
*/
#define _TWOC_DIVREM(nam, e64, e128, GMPDIVR)                                  \
  u3_noun u3qi_twid_##nam(c3_d wid, u3_atom a, u3_atom b) {                     \
    if ( c3n == _twoc_fits(a, wid) || c3n == _twoc_fits(b, wid) ) {            \
      return u3_none;                                                          \
    }                                                                          \
    if ( wid <= 64 ) {                                                         \
      c3_d msk = _tn_msk64(wid), ua = u3r_chub(0, a), ub = u3r_chub(0, b);     \
      if ( 0 == (ub & msk) ) return u3_none;          /* zero divisor */       \
      c3_d r = (e64);  return u3i_chubs(1, &r);                                \
    }                                                                          \
    if ( wid <= 128 ) {                                                        \
      _twoc_u128 msk = _tn_msk128(wid), ua = _tn_rd128(a), ub = _tn_rd128(b);  \
      if ( 0 == (ub & msk) ) return u3_none;                                   \
      _twoc_u128 r = (e128);  return _tn_em128(r);                             \
    }                                                                          \
    { mpz_t am, bm, as, bs;                                                    \
      u3r_mp(am, a);  u3r_mp(bm, b);                                           \
      _tg_mask(am, am, wid);  _tg_mask(bm, bm, wid);                           \
      if ( 0 == mpz_sgn(bm) ) {                       /* zero divisor */       \
        mpz_clear(am);  mpz_clear(bm);  return u3_none;                        \
      }                                                                        \
      mpz_init(as);  mpz_init(bs);                                             \
      _tg_signed(as, am, wid);  _tg_signed(bs, bm, wid);                       \
      GMPDIVR                                                                  \
      _tg_mask(as, as, wid);                                                   \
      mpz_clear(am);  mpz_clear(bm);  mpz_clear(bs);                           \
      return u3i_mp(as);                                                       \
    }                                                                          \
  }                                                                            \
  u3_noun u3wi_twid_##nam(u3_noun cor) {                                       \
    u3_noun a, b;  c3_d wid;                                                   \
    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &b, 0) ||              \
         c3n == u3ud(a) || c3n == u3ud(b) ) return u3m_bail(c3__exit);         \
    if ( c3n == _twoc_wid(cor, &wid) ) return u3_none;                         \
    return u3qi_twid_##nam(wid, a, b);                                         \
  }

_TWOC_DIVREM(div, _tn_div64(ua, ub, msk, wid - 1), _tn_div128(ua, ub, msk, wid - 1),
             mpz_tdiv_q(as, as, bs);)
_TWOC_DIVREM(rem, _tn_rem64(ua, ub, msk, wid - 1), _tn_rem128(ua, ub, msk, wid - 1),
             mpz_tdiv_r(as, as, bs);)

//  comparisons -- two's-complement order.
_TWOC_CMP(gth, _tn_gth64(ua, ub, sb), _tn_gth128(ua, ub, sb), mpz_cmp(as, bs) > 0)
_TWOC_CMP(lth, _tn_gth64(ub, ua, sb), _tn_gth128(ub, ua, sb), mpz_cmp(as, bs) < 0)
_TWOC_CMP(lte, _twoc_not(_tn_gth64(ua, ub, sb)), _twoc_not(_tn_gth128(ua, ub, sb)),
          mpz_cmp(as, bs) <= 0)
_TWOC_CMP(gte, _twoc_not(_tn_gth64(ub, ua, sb)), _twoc_not(_tn_gth128(ub, ua, sb)),
          mpz_cmp(as, bs) >= 0)

//  neg / abs -- unary.
_TWOC_UNOP(neg, _tn_neg64(ua, msk), _tn_neg128(ua, msk),
  { mpz_neg(am, am);  _tg_mask(am, am, wid);  res = u3i_mp(am); })
_TWOC_UNOP(abs, _tn_abs64(ua, msk, sb), _tn_abs128(ua, msk, sb),
  { mpz_t m;  mpz_init(m);  _tg_mask(m, am, wid);
    if ( _tg_sign(m, wid) == c3y ) { mpz_neg(m, m);  _tg_mask(m, m, wid); }
    mpz_clear(am);  res = u3i_mp(m); })

/* ++pow:pp -- a ^ n, n a NON-NEGATIVE raw integer (read as a plain chub for the
** native paths; full mpz for GMP).  We decline a >64-bit exponent on the native
** paths (absurd loop count) and let the Hoon run.
*/
  u3_noun
  u3qi_twid_pow(c3_d wid, u3_atom a, u3_atom n)
  {
    if ( c3n == _twoc_fits(a, wid) ) return u3_none;
    if ( wid <= 128 && (c3_d)u3r_met(0, n) > 64 ) return u3_none;
    if ( wid <= 64 ) {
      c3_d msk = _tn_msk64(wid), ua = u3r_chub(0, a), en = u3r_chub(0, n);
      c3_d r = _tn_pow64(ua, en, msk);
      return u3i_chubs(1, &r);
    }
    if ( wid <= 128 ) {
      _twoc_u128 msk = _tn_msk128(wid), ua = _tn_rd128(a);
      c3_d en = u3r_chub(0, n);
      _twoc_u128 r = _tn_pow128(ua, en, msk);
      return _tn_em128(r);
    }
    { u3_noun res;  mpz_t am, nm, mod;
      u3r_mp(am, a);  u3r_mp(nm, n);
      mpz_init(mod);  mpz_ui_pow_ui(mod, 2, wid);
      _tg_mask(am, am, wid);
      mpz_powm(am, am, nm, mod);                     /* a^n mod 2^wid */
      mpz_clear(nm);  mpz_clear(mod);
      res = u3i_mp(am);
      return res;
    }
  }
  u3_noun
  u3wi_twid_pow(u3_noun cor)
  {
    u3_noun a, n;  c3_d wid;
    if ( c3n == u3r_mean(cor, u3x_sam_2, &a, u3x_sam_3, &n, 0) ||
         c3n == u3ud(a) || c3n == u3ud(n) ) return u3m_bail(c3__exit);
    if ( c3n == _twoc_wid(cor, &wid) ) return u3_none;
    return u3qi_twid_pow(wid, a, n);
  }
