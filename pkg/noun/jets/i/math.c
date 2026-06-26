/// @file
///
/// Jets for the numerics `math.hoon` library (userspace, registered under the
/// `non` chapter alongside `lagoon`).  Each body runs the IDENTICAL algorithm
/// as its Hoon arm (same reduction, coefficients, Horner order) in Berkeley
/// SoftFloat, so jet output is BIT-EXACT to the pure-Hoon reference -- not
/// merely faithful.  Transcendentals force round-nearest-even internally (they
/// take no rounding-mode axis), matching the Hoon.
///
/// Marshalling uses chub (64-bit) reads/writes so it is word-size-agnostic
/// across the 32-bit and 64-bit runtimes (the divergence that broke @rq sub).
///
/// MASTER COPY lives in urbit/numerics libmath/vere/; applied by hand to the
/// vere runtime (pkg/noun/jets/i/math.c) -- see libmath/vere/README.md.  The
/// `_rd_*` algorithm cores below are shared with the bit-exact harness
/// (libmath/vere/test/) via -DMATH_JET_HARNESS, so the jet and its test can
/// never drift.

#ifndef MATH_JET_HARNESS
#include "jets/q.h"
#include "jets/w.h"
#include "noun.h"
#endif
#include "softfloat.h"

#ifdef MATH_JET_HARNESS
#include <stdint.h>
typedef uint64_t c3_d;
typedef int64_t  c3_ds;
#endif

  union doub {
    float64_t d;
    c3_d c;
  };

  static const c3_d _RD_QNAN = 0x7ff8000000000000ULL;
  static const c3_d _RD_PINF = 0x7ff0000000000000ULL;
  static const c3_d _RD_NINF = 0xfff0000000000000ULL;

  static inline union doub _rd_bits(c3_d b) { union doub u; u.c = b; return u; }

  //  The math doors carry a rounding mode r=?(%n %u %d %z) whose bunt is %z.
  //  The transcendental KERNELS force %n (correctly-rounded, no axis), but the
  //  composite arms (pow/atan2/tan/pow-n) round their BARE door ops per r.  The
  //  wrapper sets _math_rnd from the door's r (axis 60 of the gate); cores
  //  bracket bare ops with it and restore near-even around kernel calls.
  //  Default = minMag (%z), matching the door bunt for the harness/cold path.
  static int _math_rnd = softfloat_round_minMag;
  static inline int _rnd_of(c3_d r) {            // @tas 'n'/'u'/'d'/'z' -> SoftFloat
    switch ( r ) {
      case 'n': return softfloat_round_near_even;
      case 'u': return softfloat_round_max;
      case 'd': return softfloat_round_min;
      case 'z': return softfloat_round_minMag;
      default:  return softfloat_round_minMag;
    }
  }

/* @rd exp -- math.hoon ++rd ++exp
**   x = k*ln2 + r (Cody-Waite), exp(x) = 2^k * P(r), P a degree-11 minimax
**   polynomial; +scale2 is a correctly-rounded ldexp.
*/
  //  pow2(j) = (j+1023)<<52 as f64 bits = 2^j  (normal range j in [-1022,1023])
  static inline float64_t
  _rd_pow2(c3_ds j)
  {
    union doub u;
    u.c = ((c3_d)(j + 1023)) << 52;
    return u.d;
  }

  //  scale2: ldexp with overflow/subnormal tails (math.hoon:1519)
  static inline float64_t
  _rd_scale2(float64_t p, c3_ds k)
  {
    if ( (k - 1024) >= 0 ) {                         // k>=1024
      return f64_mul(f64_mul(p, _rd_pow2(1023)), _rd_pow2(k - 1023));
    }
    if ( !((k + 1022) >= 0) ) {                      // k<-1022
      return f64_mul(f64_mul(p, _rd_pow2(k + 54)), _rd_pow2(-54));
    }
    return f64_mul(p, _rd_pow2(k));
  }

  static float64_t
  _rd_exp(float64_t x)
  {
    union doub r0;
    //  degree-11 minimax coeffs c0..c11 (math.hoon:1542-1547)
    static const c3_d cs[12] = {
      0x3ff0000000000000ULL, 0x3ff0000000000000ULL, 0x3fe0000000000011ULL,
      0x3fc555555555555aULL, 0x3fa555555554f0cfULL, 0x3f8111111110f225ULL,
      0x3f56c16c187fbe02ULL, 0x3f2a01a01b14378fULL, 0x3efa01991ac8730aULL,
      0x3ec71ddf5749d126ULL, 0x3e928b4057f44145ULL, 0x3e5af631d0059becULL
    };
    union doub log2e, ln2hi, ln2lo, ka, kf, rr, p, c, zero;

    zero.c = 0;
    r0.d = x;
    if ( !f64_eq(x, x) )       { r0.c = _RD_QNAN; return r0.d; }  // NaN
    if ( r0.c == _RD_PINF )    { return x; }                      // +inf
    if ( r0.c == _RD_NINF )    { r0.c = 0; return r0.d; }         // -inf -> 0

    log2e.c = 0x3ff71547652b82feULL;
    ln2hi.c = 0x3fe62e42fee00000ULL;
    ln2lo.c = 0x3dea39ef35793c76ULL;

    c3_ds k = (c3_ds)f64_to_i64(f64_mul(x, log2e.d), softfloat_round_near_even, 0);
    if ( (k - 1025) >= 0 )    { r0.c = _RD_PINF; return r0.d; }   // overflow -> inf
    if ( !((k + 1075) >= 0) ) { r0.c = 0; return r0.d; }          // underflow -> 0

    ka.d = ui64_to_f64( (c3_d)(k < 0 ? -k : k) );
    kf.d = (k >= 0) ? ka.d : f64_sub(zero.d, ka.d);
    rr.d = f64_sub( f64_sub(x, f64_mul(kf.d, ln2hi.d)), f64_mul(kf.d, ln2lo.d) );

    p.c = 0;
    for ( int i = 12; i-- != 0; ) {        // Horner over flop(cs): c11..c0
      c.c = cs[i];
      p.d = f64_add(f64_mul(p.d, rr.d), c.d);
    }
    return _rd_scale2(p.d, k);
  }

/* @rd log/log-2/log-10 -- math.hoon ++rd ++log/++log-2/++log-10/++lr
**   lr: x = 2^e * m, m in [sqrt(1/2),sqrt(2)); returns [e-as-rd, log(1+f)] with
**   f=m-1, s=f/(2+f), log(1+f) = f - s*(f - 2z*P2(z)), z=s*s, P2 the atanh
**   series 1/3 + z/5 + ...  log/log-2/log-10 recombine the integer part.
*/
  //  +lr (math.hoon:2043): for finite positive x, -> *ef = e as @rd, *lm = log(m)
  static void _rd_lr(float64_t x, float64_t* ef, float64_t* lm) {
    static const c3_d cs[10] = {
      0x3fd5555555555555ULL, 0x3fc999999999999aULL, 0x3fc2492492492492ULL,
      0x3fbc71c71c71c71cULL, 0x3fb745d1745d1746ULL, 0x3fb3b13b13b13b14ULL,
      0x3fb1111111111111ULL, 0x3fae1e1e1e1e1e1eULL, 0x3faaf286bca1af28ULL,
      0x3fa8618618618618ULL };
    union doub r0, xx, m, f, s, z, p, c, rr, l1, efd, one, zero;
    zero.c = 0; one.c = 0x3ff0000000000000ULL;
    r0.d = x;
    int sub = ( ((r0.c >> 52) & 0x7ffULL) == 0 );
    xx = r0;
    if ( sub ) xx.d = f64_mul(x, _rd_bits(0x4350000000000000ULL).d);   // x * 2^54
    c3_ds ae = sub ? -54 : 0;
    c3_d  b  = xx.c;
    c3_ds e  = (c3_ds)((b >> 52) & 0x7ffULL) - 1023;
    m.c = (b & 0xfffffffffffffULL) | 0x3ff0000000000000ULL;            // m in [1,2)
    if ( f64_le(_rd_bits(0x3ff6a09e667f3bcdULL).d, m.d) ) {            // m >= sqrt(2)
      m.d = f64_mul(m.d, _rd_bits(0x3fe0000000000000ULL).d); e = e + 1;
    }
    e = e + ae;
    f.d = f64_sub(m.d, one.d);
    s.d = f64_div(f.d, f64_add(m.d, one.d));
    z.d = f64_mul(s.d, s.d);
    p.c = 0;
    for ( int i = 10; i-- != 0; ) { c.c = cs[i]; p.d = f64_add(f64_mul(p.d, z.d), c.d); }
    rr.d = f64_mul(f64_add(z.d, z.d), p.d);
    l1.d = f64_sub(f.d, f64_mul(s.d, f64_sub(f.d, rr.d)));
    efd.d = ui64_to_f64( (c3_d)(e < 0 ? -e : e) );
    if ( e < 0 ) efd.d = f64_sub(zero.d, efd.d);
    *ef = efd.d; *lm = l1.d;
  }
  //  shared guard: 0 on NaN/+inf/+-0/x<0 returned via *out
  static int _rd_log_guard(float64_t x, float64_t* out) {
    union doub r0; r0.d = x;
    if ( !f64_eq(x, x) )                       { r0.c = _RD_QNAN; *out = r0.d; return 1; }
    if ( r0.c == _RD_PINF )                    { *out = x;        return 1; }
    if ( (r0.c == 0)||(r0.c == 0x8000000000000000ULL) ) { r0.c = _RD_NINF; *out = r0.d; return 1; }
    if ( (r0.c >> 63) == 1 )                   { r0.c = _RD_QNAN; *out = r0.d; return 1; }
    return 0;
  }
  static float64_t _rd_log(float64_t x) {
    float64_t g, ef, lm; union doub hi, lo;
    if ( _rd_log_guard(x, &g) ) return g;
    _rd_lr(x, &ef, &lm);
    hi.d = f64_mul(ef, _rd_bits(0x3fe62e42fee00000ULL).d);            // e*ln2hi
    lo.d = f64_mul(ef, _rd_bits(0x3dea39ef35793c76ULL).d);            // e*ln2lo
    return f64_add(hi.d, f64_add(lm, lo.d));
  }
  static float64_t _rd_log2(float64_t x) {
    float64_t g, ef, lm;
    if ( _rd_log_guard(x, &g) ) return g;
    _rd_lr(x, &ef, &lm);                                              // e + lm/ln2
    return f64_add(ef, f64_mul(lm, _rd_bits(0x3ff71547652b82feULL).d));
  }
  static float64_t _rd_log10(float64_t x) {
    float64_t g, ef, lm;
    if ( _rd_log_guard(x, &g) ) return g;
    _rd_lr(x, &ef, &lm);                                              // e*log10(2) + lm/ln10
    return f64_add(f64_mul(ef, _rd_bits(0x3fd34413509f79ffULL).d),
                   f64_mul(lm, _rd_bits(0x3fdbcb7b1526e50eULL).d));
  }

  static inline float64_t _rd_neg(float64_t a) { union doub z; z.c=0; return f64_sub(z.d, a); }

/* @rd sin/cos -- math.hoon ++rd ++sin/++cos/++rd-trig
**   x = q*(pi/2) + (rhi+rlo) (2-part pi/2), fdlibm sin/cos kernels by q&3.
*/
  static const c3_d _RD_SC[8] = {     // sin kernel coeffs (math.hoon:1611)
    0xbfc5555555555555ULL, 0x3f81111111111111ULL, 0xbf2a01a01a01a01aULL,
    0x3ec71de3a556c734ULL, 0xbe5ae64567f544e4ULL, 0x3de6124613a86d09ULL,
    0xbd6ae7f3e733b81fULL, 0x3ce952c77030ad4aULL
  };
  static const c3_d _RD_CC[8] = {     // cos kernel coeffs (math.hoon:1618)
    0x3fa5555555555555ULL, 0xbf56c16c16c16c17ULL, 0x3efa01a01a01a01aULL,
    0xbe927e4fb7789f5cULL, 0x3e21eed8eff8d898ULL, 0xbda93974a8c07c9dULL,
    0x3d2ae7f3e733b81fULL, 0xbca6827863b97d97ULL
  };

  static float64_t _rd_ksin(float64_t xx, float64_t yy) {
    union doub z, r, v, aa, bb, dd, c, half;
    half.c = 0x3fe0000000000000ULL;
    z.d = f64_mul(xx, xx);
    r.c = 0;                            // Horner over flop(tail sc): sc[7..1]
    for ( int i = 8; i-- != 1; ) { c.c = _RD_SC[i]; r.d = f64_add(f64_mul(r.d, z.d), c.d); }
    v.d = f64_mul(z.d, xx);
    aa.d = f64_sub(f64_mul(half.d, yy), f64_mul(v.d, r.d));
    bb.d = f64_sub(f64_mul(z.d, aa.d), yy);
    dd.d = f64_sub(bb.d, f64_mul(v.d, _rd_bits(_RD_SC[0]).d));
    return f64_sub(xx, dd.d);
  }
  static float64_t _rd_kcos(float64_t xx, float64_t yy) {
    union doub z, rc, hz, w2, aa, bb, c, half, one;
    half.c = 0x3fe0000000000000ULL; one.c = 0x3ff0000000000000ULL;
    z.d = f64_mul(xx, xx);
    rc.c = 0;                          // Horner over flop(cc): cc[7..0]
    for ( int i = 8; i-- != 0; ) { c.c = _RD_CC[i]; rc.d = f64_add(f64_mul(rc.d, z.d), c.d); }
    hz.d = f64_mul(half.d, z.d);
    w2.d = f64_sub(one.d, hz.d);
    aa.d = f64_sub(f64_sub(one.d, w2.d), hz.d);
    bb.d = f64_sub(f64_mul(f64_mul(z.d, z.d), rc.d), f64_mul(xx, yy));
    return f64_add(w2.d, f64_add(aa.d, bb.d));
  }
  //  trig-fin: is_sin ? sin(x) : cos(x); ax=|x|, sb=sign bit (math.hoon:1643)
  static float64_t _rd_trigfin(int is_sin, float64_t ax, c3_d sb) {
    union doub qf, t, w, rhi, rlo, ks, kc, v;
    c3_ds q = (c3_ds)f64_to_i64(f64_mul(ax, _rd_bits(0x3fe45f306dc9c883ULL).d),
                                softfloat_round_near_even, 0);          // round(ax*2/pi)
    c3_d  aq = (c3_d)(q < 0 ? -q : q);
    qf.d = ui64_to_f64(aq);
    t.d = f64_sub(ax, f64_mul(qf.d, _rd_bits(0x3ff921fb54400000ULL).d)); // ax - qf*pi/2_hi
    w.d = f64_mul(qf.d, _rd_bits(0x3dd0b4611a626331ULL).d);             // qf*pi/2_lo
    rhi.d = f64_sub(t.d, w.d);
    rlo.d = f64_sub(f64_sub(t.d, rhi.d), w.d);
    int m = (int)(aq & 3);
    ks.d = _rd_ksin(rhi.d, rlo.d);
    kc.d = _rd_kcos(rhi.d, rlo.d);
    if ( is_sin ) {
      v.d = (m==0) ? ks.d : (m==1) ? kc.d : (m==2) ? _rd_neg(ks.d) : _rd_neg(kc.d);
      return (sb == 1) ? _rd_neg(v.d) : v.d;
    }
    return (m==0) ? kc.d : (m==1) ? _rd_neg(ks.d) : (m==2) ? _rd_neg(kc.d) : ks.d;
  }
  static float64_t _rd_sin(float64_t x) {
    union doub r0, ax;
    r0.d = x;
    if ( !f64_eq(x, x) )                   { r0.c = _RD_QNAN; return r0.d; }  // NaN
    if ( (r0.c == _RD_PINF)||(r0.c == _RD_NINF) ) { r0.c = _RD_QNAN; return r0.d; }  // +-inf -> NaN
    if ( (r0.c == 0)||(r0.c == 0x8000000000000000ULL) ) { return x; }         // +-0 -> +-0
    ax.c = r0.c & 0x7fffffffffffffffULL;
    return _rd_trigfin(1, ax.d, r0.c >> 63);
  }
  static float64_t _rd_cos(float64_t x) {
    union doub r0, ax;
    r0.d = x;
    if ( !f64_eq(x, x) )                   { r0.c = _RD_QNAN; return r0.d; }  // NaN
    if ( (r0.c == _RD_PINF)||(r0.c == _RD_NINF) ) { r0.c = _RD_QNAN; return r0.d; }  // +-inf -> NaN
    ax.c = r0.c & 0x7fffffffffffffffULL;
    return _rd_trigfin(0, ax.d, 0);
  }

/* @rd tan -- math.hoon ++rd ++tan/++rd-tan (fdlibm __kernel_tan)
**   q*pi/2 reduction; big |x|~pi/4 reduced; odd q -> -cot path.
*/
  static float64_t _rd_ktan(float64_t x, float64_t y, c3_ds iy) {
    static const c3_d rl[6] = {        // math.hoon:1700
      0x3fc111111110fe7aULL, 0x3f9664f48406d637ULL, 0x3f6d6d22c9560328ULL,
      0x3f4344d8f2f26501ULL, 0x3f147e88a03792a6ULL, 0xbef375cbdb605373ULL };
    static const c3_d vl[6] = {        // math.hoon:1705
      0x3faba1ba1bb341feULL, 0x3f8226e3e96e8493ULL, 0x3f57dbc8fee08315ULL,
      0x3f3026f71a8d1068ULL, 0x3f12b80f32f0a7e9ULL, 0x3efb2a7074bf7ad4ULL };
    union doub xb, ax, xa, ya, xr, yr, z, w, c, rr, vp, vv, s, r, w2;
    union doub one, mone, two, third;
    one.c=0x3ff0000000000000ULL; mone.c=0xbff0000000000000ULL;
    two.c=0x4000000000000000ULL;  third.c=0x3fd5555555555563ULL;
    xb.d = x;
    c3_d hxneg = xb.c >> 63;
    ax.c = xb.c & 0x7fffffffffffffffULL;
    int big = f64_le(_rd_bits(0x3fe5942800000000ULL).d, ax.d);        // |x| >= ~0.674
    xa.d = (hxneg == 1) ? _rd_neg(x) : x;
    ya.d = (hxneg == 1) ? _rd_neg(y) : y;
    if ( big ) {                       // pio4_hi - xa + (pio4_lo - ya)
      xr.d = f64_add(f64_sub(_rd_bits(0x3fe921fb54442d18ULL).d, xa.d),
                     f64_sub(_rd_bits(0x3c81a62633145c07ULL).d, ya.d));
      yr.c = 0;
    } else { xr.d = x; yr.d = y; }
    z.d = f64_mul(xr.d, xr.d);
    w.d = f64_mul(z.d, z.d);
    rr.c = 0;
    for ( int i = 6; i-- != 0; ) { c.c = rl[i]; rr.d = f64_add(f64_mul(rr.d, w.d), c.d); }
    vp.c = 0;
    for ( int i = 6; i-- != 0; ) { c.c = vl[i]; vp.d = f64_add(f64_mul(vp.d, w.d), c.d); }
    vv.d = f64_mul(z.d, vp.d);
    s.d = f64_mul(z.d, xr.d);
    r.d = f64_add(yr.d, f64_mul(z.d, f64_add(f64_mul(s.d, f64_add(rr.d, vv.d)), yr.d)));
    r.d = f64_add(r.d, f64_mul(third.d, s.d));
    w2.d = f64_add(xr.d, r.d);
    if ( big ) {
      union doub fac, v, t1;
      fac.d = (hxneg == 1) ? mone.d : one.d;
      v.d   = (iy == 1) ? one.d : mone.d;
      t1.d = f64_sub(f64_div(f64_mul(w2.d, w2.d), f64_add(w2.d, v.d)), r.d);
      t1.d = f64_mul(two.d, f64_sub(xr.d, t1.d));
      return f64_mul(fac.d, f64_sub(v.d, t1.d));
    }
    if ( iy == 1 ) return w2.d;
    { union doub zz, vv2, a, tt, ss;     // -cot path
      zz.c  = w2.c & 0xffffffff00000000ULL;
      vv2.d = f64_sub(r.d, f64_sub(zz.d, xr.d));
      a.d   = f64_div(mone.d, w2.d);
      tt.c  = a.c & 0xffffffff00000000ULL;
      ss.d  = f64_add(one.d, f64_mul(tt.d, zz.d));
      return f64_add(tt.d, f64_mul(a.d, f64_add(ss.d, f64_mul(tt.d, vv2.d))));
    }
  }
  static float64_t _rd_tan(float64_t x) {
    union doub r0, ax, qf, t, w, rhi, rlo, res;
    r0.d = x;
    if ( !f64_eq(x, x) )                   { r0.c = _RD_QNAN; return r0.d; }
    if ( (r0.c == _RD_PINF)||(r0.c == _RD_NINF) ) { r0.c = _RD_QNAN; return r0.d; }
    if ( (r0.c == 0)||(r0.c == 0x8000000000000000ULL) ) { return x; }
    c3_d neg = r0.c >> 63;
    ax.c = r0.c & 0x7fffffffffffffffULL;
    c3_ds q = (c3_ds)f64_to_i64(f64_mul(ax.d, _rd_bits(0x3fe45f306dc9c883ULL).d),
                                softfloat_round_near_even, 0);
    c3_d aq = (c3_d)(q < 0 ? -q : q);
    qf.d = ui64_to_f64(aq);
    t.d = f64_sub(ax.d, f64_mul(qf.d, _rd_bits(0x3ff921fb54400000ULL).d));
    w.d = f64_mul(qf.d, _rd_bits(0x3dd0b4611a626331ULL).d);
    rhi.d = f64_sub(t.d, w.d);
    rlo.d = f64_sub(f64_sub(t.d, rhi.d), w.d);
    c3_ds iy = ((aq & 1) == 0) ? 1 : -1;
    res.d = _rd_ktan(rhi.d, rlo.d, iy);
    return (neg == 1) ? _rd_neg(res.d) : res.d;
  }

/* @rd atan/atan2 -- math.hoon ++rd ++atan/++rd-atan/++atan2
**   fdlibm breakpoint reduction (7/16,11/16,19/16,39/16) + degree-10 minimax.
*/
  static float64_t _rd_atan(float64_t x) {
    static const c3_d at[11] = {       // fdlibm aT[] (math.hoon:1869)
      0x3fd555555555550dULL, 0xbfc999999998ebc4ULL, 0x3fc24924920083ffULL,
      0xbfbc71c6fe231671ULL, 0x3fb745cdc54c206eULL, 0xbfb3b0f2af749a6dULL,
      0x3fb10d66a0d03d51ULL, 0xbfadde2d52defd9aULL, 0x3fa97b4b24760debULL,
      0xbfa2b4442c6a6c2fULL, 0x3f90ad3ae322da11ULL };
    union doub r0, ax, xr, hi, lo, z, sp, s, c, res, one, two, ohf;
    r0.d = x;
    if ( !f64_eq(x, x) )       { r0.c = _RD_QNAN; return r0.d; }       // NaN
    if ( r0.c == _RD_PINF )    { r0.c = 0x3ff921fb54442d18ULL; return r0.d; }  // +inf -> pi/2
    if ( r0.c == _RD_NINF )    { r0.c = 0xbff921fb54442d18ULL; return r0.d; }  // -inf -> -pi/2
    if ( (r0.c == 0)||(r0.c == 0x8000000000000000ULL) ) { return x; }  // +-0
    c3_d neg = r0.c >> 63;
    ax.c = r0.c & 0x7fffffffffffffffULL;
    one.c=0x3ff0000000000000ULL; two.c=0x4000000000000000ULL; ohf.c=0x3ff8000000000000ULL;
    int dir = 0;
    if ( f64_lt(ax.d, _rd_bits(0x3fdc000000000000ULL).d) ) {            // |x| < 7/16
      xr.d = ax.d; hi.c = 0; lo.c = 0; dir = 1;
    } else if ( f64_lt(ax.d, _rd_bits(0x3fe6000000000000ULL).d) ) {     // < 11/16
      xr.d = f64_div(f64_sub(f64_add(ax.d, ax.d), one.d), f64_add(two.d, ax.d));
      hi.c = 0x3fddac670561bb4fULL; lo.c = 0x3c7a2b7f222f65e2ULL;       // atan(0.5)
    } else if ( f64_lt(ax.d, _rd_bits(0x3ff3000000000000ULL).d) ) {     // < 19/16
      xr.d = f64_div(f64_sub(ax.d, one.d), f64_add(ax.d, one.d));
      hi.c = 0x3fe921fb54442d18ULL; lo.c = 0x3c81a62633145c07ULL;       // atan(1)=pi/4
    } else if ( f64_lt(ax.d, _rd_bits(0x4003800000000000ULL).d) ) {     // < 39/16
      xr.d = f64_div(f64_sub(ax.d, ohf.d), f64_add(one.d, f64_mul(ohf.d, ax.d)));
      hi.c = 0x3fef730bd281f69bULL; lo.c = 0x3c7007887af0cbbdULL;       // atan(1.5)
    } else {                                                            // -1/x
      xr.d = f64_div(_rd_bits(0xbff0000000000000ULL).d, ax.d);
      hi.c = 0x3ff921fb54442d18ULL; lo.c = 0x3c91a62633145c07ULL;       // pi/2
    }
    z.d = f64_mul(xr.d, xr.d);
    sp.c = 0;
    for ( int i = 11; i-- != 0; ) { c.c = at[i]; sp.d = f64_add(f64_mul(sp.d, z.d), c.d); }
    s.d = f64_mul(z.d, sp.d);
    if ( dir ) res.d = f64_sub(xr.d, f64_mul(xr.d, s.d));
    else       res.d = f64_sub(hi.d, f64_sub(f64_sub(f64_mul(xr.d, s.d), lo.d), xr.d));
    return (neg == 1) ? _rd_neg(res.d) : res.d;
  }
  //  bare door ops (div/add/sub/mul) round per _math_rnd; atan kernel is %n.
  static float64_t _rd_atan2(float64_t y, float64_t x) {
    union doub xb, pi, two, mone, zero, q, a, r;
    zero.c = 0; pi.c = 0x400921fb54442d18ULL;
    two.c = 0x4000000000000000ULL; mone.c = 0xbff0000000000000ULL;
    xb.d = x;
    if ( f64_lt(zero.d, x) ) {                                          // x>0: atan(div y x)
      softfloat_roundingMode = _math_rnd; q.d = f64_div(y, x);
      softfloat_roundingMode = softfloat_round_near_even; return _rd_atan(q.d);
    }
    if ( f64_lt(x, zero.d) && f64_le(zero.d, y) ) {                     // x<0,y>=0: add(atan,pi)
      softfloat_roundingMode = _math_rnd; q.d = f64_div(y, x);
      softfloat_roundingMode = softfloat_round_near_even; a.d = _rd_atan(q.d);
      softfloat_roundingMode = _math_rnd; r.d = f64_add(a.d, pi.d);
      softfloat_roundingMode = softfloat_round_near_even; return r.d;
    }
    if ( f64_lt(x, zero.d) && f64_lt(y, zero.d) ) {                     // x<0,y<0: sub(atan,pi)
      softfloat_roundingMode = _math_rnd; q.d = f64_div(y, x);
      softfloat_roundingMode = softfloat_round_near_even; a.d = _rd_atan(q.d);
      softfloat_roundingMode = _math_rnd; r.d = f64_sub(a.d, pi.d);
      softfloat_roundingMode = softfloat_round_near_even; return r.d;
    }
    if ( (xb.c == 0) && f64_lt(zero.d, y) ) {                           // x==+0,y>0: div(pi,2)
      softfloat_roundingMode = _math_rnd; r.d = f64_div(pi.d, two.d);
      softfloat_roundingMode = softfloat_round_near_even; return r.d;
    }
    if ( (xb.c == 0) && f64_lt(y, zero.d) ) {                           // x==+0,y<0: mul(-1,div(pi,2))
      softfloat_roundingMode = _math_rnd;
      r.d = f64_mul(mone.d, f64_div(pi.d, two.d));
      softfloat_roundingMode = softfloat_round_near_even; return r.d;
    }
    return zero.d;
  }

/* @rd asin/acos -- math.hoon ++rd ++asin/++acos/++rd-ainv
**   fdlibm rational P/Q kernel; sqt is correctly-rounded (= f64_sqrt).
*/
  static float64_t _rd_ainv_rr(float64_t t) {   // R(t) = P(t)/Q(t)  (math.hoon:1775)
    static const c3_d ps[6] = {
      0x3fc5555555555555ULL, 0xbfd4d61203eb6f7dULL, 0x3fc9c1550e884455ULL,
      0xbfa48228b5688f3bULL, 0x3f49efe07501b288ULL, 0x3f023de10dfdf709ULL };
    static const c3_d qs[4] = {
      0xc0033a271c8a2d4bULL, 0x40002ae59c598ac8ULL, 0xbfe6066c1b8d0159ULL,
      0x3fb3b8c5b12e9282ULL };
    union doub pp, qq, c, one;
    one.c = 0x3ff0000000000000ULL;
    pp.c = 0;
    for ( int i = 6; i-- != 0; ) { c.c = ps[i]; pp.d = f64_add(f64_mul(pp.d, t), c.d); }
    qq.c = 0;
    for ( int i = 4; i-- != 0; ) { c.c = qs[i]; qq.d = f64_add(f64_mul(qq.d, t), c.d); }
    return f64_div(f64_mul(t, pp.d), f64_add(one.d, f64_mul(t, qq.d)));
  }
  static float64_t _rd_asin(float64_t x) {
    union doub r0, ax, t, w, r, s, res, half, one, two, pio2h, pio2l, pio4;
    half.c=0x3fe0000000000000ULL; one.c=0x3ff0000000000000ULL; two.c=0x4000000000000000ULL;
    pio2h.c=0x3ff921fb54442d18ULL; pio2l.c=0x3c91a62633145c07ULL; pio4.c=0x3fe921fb54442d18ULL;
    r0.d = x;
    if ( !f64_eq(x, x) )            { r0.c = _RD_QNAN; return r0.d; }   // NaN
    c3_d sgn = r0.c >> 63;
    ax.c = r0.c & 0x7fffffffffffffffULL;
    if ( f64_lt(one.d, ax.d) )      { r0.c = _RD_QNAN; return r0.d; }   // |x|>1 -> NaN
    if ( ax.c == one.c )                                               // |x|==1
      return f64_add(f64_mul(x, pio2h.d), f64_mul(x, pio2l.d));
    if ( f64_lt(ax.d, half.d) ) {                                     // |x|<0.5
      if ( f64_lt(ax.d, _rd_bits(0x3e50000000000000ULL).d) ) return x; // tiny
      t.d = f64_mul(x, x);
      return f64_add(x, f64_mul(x, _rd_ainv_rr(t.d)));
    }
    w.d = f64_sub(one.d, ax.d);
    t.d = f64_mul(w.d, half.d);
    r.d = _rd_ainv_rr(t.d);
    s.d = f64_sqrt(t.d);
    if ( f64_le(_rd_bits(0x3fef333300000000ULL).d, ax.d) ) {           // near 1
      res.d = f64_sub(pio2h.d, f64_sub(f64_mul(two.d, f64_add(s.d, f64_mul(s.d, r.d))), pio2l.d));
      return (sgn == 1) ? _rd_neg(res.d) : res.d;
    }
    { union doub df, cc, p2, q2;                                       // head/tail
      df.c = s.c & 0xffffffff00000000ULL;
      cc.d = f64_div(f64_sub(t.d, f64_mul(df.d, df.d)), f64_add(s.d, df.d));
      p2.d = f64_sub(f64_mul(two.d, f64_mul(s.d, r.d)), f64_sub(pio2l.d, f64_mul(two.d, cc.d)));
      q2.d = f64_sub(pio4.d, f64_mul(two.d, df.d));
      res.d = f64_sub(pio4.d, f64_sub(p2.d, q2.d));
      return (sgn == 1) ? _rd_neg(res.d) : res.d;
    }
  }
  static float64_t _rd_acos(float64_t x) {
    union doub r0, ax, z, s, r, w, half, one, two, pi, pio2h, pio2l;
    half.c=0x3fe0000000000000ULL; one.c=0x3ff0000000000000ULL; two.c=0x4000000000000000ULL;
    pi.c=0x400921fb54442d18ULL; pio2h.c=0x3ff921fb54442d18ULL; pio2l.c=0x3c91a62633145c07ULL;
    r0.d = x;
    if ( !f64_eq(x, x) )            { r0.c = _RD_QNAN; return r0.d; }   // NaN
    c3_d neg = r0.c >> 63;
    ax.c = r0.c & 0x7fffffffffffffffULL;
    if ( f64_lt(one.d, ax.d) )      { r0.c = _RD_QNAN; return r0.d; }   // |x|>1 -> NaN
    if ( ax.c == one.c ) {                                             // |x|==1
      if ( neg == 0 ) { union doub z0; z0.c = 0; return z0.d; }         // 1 -> 0
      return f64_add(pi.d, f64_mul(two.d, pio2l.d));                    // -1 -> pi
    }
    if ( f64_lt(ax.d, half.d) ) {                                     // |x|<0.5
      if ( f64_lt(ax.d, _rd_bits(0x3c60000000000000ULL).d) ) return pio2h.d;  // tiny -> pi/2
      z.d = f64_mul(x, x);
      r.d = _rd_ainv_rr(z.d);
      return f64_sub(pio2h.d, f64_sub(x, f64_sub(pio2l.d, f64_mul(x, r.d))));
    }
    if ( neg == 1 ) {                                                 // x <= -0.5
      z.d = f64_mul(f64_add(one.d, x), half.d);
      s.d = f64_sqrt(z.d);
      r.d = _rd_ainv_rr(z.d);
      w.d = f64_sub(f64_mul(r.d, s.d), pio2l.d);
      return f64_sub(pi.d, f64_mul(two.d, f64_add(s.d, w.d)));
    }
    { union doub df, cc;                                              // x >= 0.5
      z.d = f64_mul(f64_sub(one.d, x), half.d);
      s.d = f64_sqrt(z.d);
      df.c = s.c & 0xffffffff00000000ULL;
      cc.d = f64_div(f64_sub(z.d, f64_mul(df.d, df.d)), f64_add(s.d, df.d));
      r.d = _rd_ainv_rr(z.d);
      w.d = f64_add(f64_mul(r.d, s.d), cc.d);
      return f64_mul(two.d, f64_add(df.d, w.d));
    }
  }

/* @rd sqt/cbt -- math.hoon ++rd ++sqt/++cbt
**   sqt: correctly-rounded f64 sqrt (the Markstein-corrected Hoon == f64_sqrt).
**   cbt: sign(x) * exp(log|x| / 3).
*/
  static float64_t _rd_sqt(float64_t x) {
    union doub r0; r0.d = x;
    if ( !f64_eq(x, x) )                       { r0.c = _RD_QNAN; return r0.d; }  // NaN
    if ( r0.c == _RD_PINF )                    { return x; }                      // +inf
    if ( (r0.c == 0)||(r0.c == 0x8000000000000000ULL) ) { return x; }             // +-0
    if ( (r0.c >> 63) == 1 )                   { r0.c = _RD_QNAN; return r0.d; }  // x<0 -> NaN
    return f64_sqrt(x);
  }
  static float64_t _rd_cbt(float64_t x) {
    union doub r0, ax, r;
    r0.d = x;
    if ( !f64_eq(x, x) )                       { return x; }                      // NaN -> NaN
    if ( (r0.c == 0)||(r0.c == 0x8000000000000000ULL) ) { return x; }             // +-0
    ax.c = r0.c & 0x7fffffffffffffffULL;
    r.d = _rd_exp(f64_mul(_rd_log(ax.d), _rd_bits(0x3fd5555555555555ULL).d));     // exp(log|x|/3)
    return ((r0.c >> 63) == 1) ? _rd_neg(r.d) : r.d;
  }

/* @rd pow/pow-n -- math.hoon ++rd ++pow/++pow-n
**   pow-n: x^n by repeated mul (n a positive integer as @rd).
**   pow: positive-integer fast path -> pow-n, else exp(n*log x).
*/
  static float64_t _rd_pow_n(float64_t x, float64_t n) {
    union doub nn, p, one, two;
    one.c = 0x3ff0000000000000ULL; two.c = 0x4000000000000000ULL;
    nn.d = n;
    if ( nn.c == 0 ) return one.d;             // n == +0 -> 1
    softfloat_roundingMode = _math_rnd;        // bare mul/sub round per door r
    p.d = x;
    while ( !f64_lt(n, two.d) ) {              // while n >= 2: p *= x; n -= 1
      p.d = f64_mul(p.d, x);
      n = f64_sub(n, one.d);
    }
    softfloat_roundingMode = softfloat_round_near_even;
    return p.d;
  }
  static float64_t _rd_pow(float64_t x, float64_t n) {
    union doub nn, ni, zero, lg, prod;
    zero.c = 0; nn.d = n;
    //  integer detection is rounding-mode-independent (exact for true integers)
    ni.d = i64_to_f64(f64_to_i64(n, softfloat_round_near_even, 0));   // san (need (toi n))
    if ( (nn.c == ni.c) && f64_lt(zero.d, n) )                        // positive integer
      return _rd_pow_n(x, ni.d);
    lg.d = _rd_log(x);                                                // %n kernel
    softfloat_roundingMode = _math_rnd;                              // bare mul per door r
    prod.d = f64_mul(n, lg.d);
    softfloat_roundingMode = softfloat_round_near_even;
    return _rd_exp(prod.d);                                           // %n kernel: exp(n*log x)
  }

/* ===================================================================
** @rs (single-precision) cores -- math.hoon ++rs.  Single-precision twins
** of the @rd cores: identical reductions/Horner order, single-precision
** constants and minimax coeffs (read from the Hoon arms), SoftFloat f32 ops.
** Marshalling is still chub-based (read low 32 bits, write a 32-bit atom), so
** these are word-size-agnostic exactly like the @rd cores.  The bit pattern is
** a plain uint32_t (NOT c3_w, which is 64-bit on the vere64 build).
** =================================================================== */

  union sing {
    float32_t s;
    uint32_t  c;
  };

  static const uint32_t _RS_QNAN = 0x7fc00000U;
  static const uint32_t _RS_PINF = 0x7f800000U;
  static const uint32_t _RS_NINF = 0xff800000U;

  static inline union sing _rs_bits(uint32_t b) { union sing u; u.c = b; return u; }
  static inline float32_t  _rs_neg(float32_t a) {            // (sub .0 a)
    union sing z; z.c = 0; return f32_sub(z.s, a);
  }

/* @rs exp -- math.hoon ++rs ++exp
**   x = k*ln2 + r (Cody-Waite), exp(x) = 2^k * P(r), P a degree-6 minimax.
*/
  //  pow2(j) = (j+127)<<23 as f32 bits = 2^j  (normal range j in [-126,127])
  static inline float32_t _rs_pow2(c3_ds j) {
    union sing u; u.c = ((uint32_t)(j + 127)) << 23; return u.s;
  }
  //  scale2: ldexp with overflow/subnormal tails (math.hoon ++rs ++exp)
  static inline float32_t _rs_scale2(float32_t p, c3_ds k) {
    if ( (k - 128) >= 0 ) {                            // k>=128
      return f32_mul(f32_mul(p, _rs_pow2(127)), _rs_pow2(k - 127));
    }
    if ( !((k + 126) >= 0) ) {                         // k<-126
      return f32_mul(f32_mul(p, _rs_pow2(k + 24)), _rs_pow2(-24));
    }
    return f32_mul(p, _rs_pow2(k));
  }
  static float32_t _rs_exp(float32_t x) {
    union sing r0;
    //  degree-6 minimax coeffs c0..c6 (math.hoon ++rs ++exp)
    static const uint32_t cs[7] = {
      0x3f800000U, 0x3f800000U, 0x3f000000U, 0x3e2aaa02U,
      0x3d2aaa56U, 0x3c0937d3U, 0x3ab6ba99U
    };
    union sing log2e, ln2hi, ln2lo, ka, kf, rr, p, c, zero;
    zero.c = 0;
    r0.s = x;
    if ( !f32_eq(x, x) )    { r0.c = _RS_QNAN; return r0.s; }  // NaN
    if ( r0.c == _RS_PINF ) { return x; }                      // +inf
    if ( r0.c == _RS_NINF ) { r0.c = 0; return r0.s; }         // -inf -> 0

    log2e.c = 0x3fb8aa3bU;
    ln2hi.c = 0x3f317200U;
    ln2lo.c = 0x35bfbe8eU;

    c3_ds k = (c3_ds)f32_to_i32(f32_mul(x, log2e.s), softfloat_round_near_even, 0);
    if ( (k - 129) >= 0 )    { r0.c = _RS_PINF; return r0.s; }  // overflow -> inf
    if ( !((k + 150) >= 0) ) { r0.c = 0; return r0.s; }         // underflow -> 0

    ka.s = ui32_to_f32( (uint32_t)(k < 0 ? -k : k) );
    kf.s = (k >= 0) ? ka.s : f32_sub(zero.s, ka.s);
    rr.s = f32_sub( f32_sub(x, f32_mul(kf.s, ln2hi.s)), f32_mul(kf.s, ln2lo.s) );

    p.c = 0;
    for ( int i = 7; i-- != 0; ) {        // Horner over flop(cs): c6..c0
      c.c = cs[i];
      p.s = f32_add(f32_mul(p.s, rr.s), c.s);
    }
    return _rs_scale2(p.s, k);
  }

/* @rs sin/cos/tan -- math.hoon ++rs ++sin/++cos/++rs-trig/++tan
**   x = q*(pi/2) + (rhi+rlo) (3-part pi/2, f32 needs the extra word), fdlibm
**   sin/cos kernels by q&3.  tan = sin/cos (no dedicated kernel, unlike @rd).
*/
  static const uint32_t _RS_SC[5] = {     // sin kernel coeffs
    0xbe2aaaabU, 0x3c088889U, 0xb9500d01U, 0x3638ef1dU, 0xb2d7322bU
  };
  static const uint32_t _RS_CC[5] = {     // cos kernel coeffs
    0x3d2aaaabU, 0xbab60b61U, 0x37d00d01U, 0xb493f27eU, 0x310f76c7U
  };
  static float32_t _rs_ksin(float32_t xx, float32_t yy) {
    union sing z, r, v, aa, bb, dd, c, half;
    half.c = 0x3f000000U;
    z.s = f32_mul(xx, xx);
    r.c = 0;                            // Horner over flop(tail sc): sc[4..1]
    for ( int i = 5; i-- != 1; ) { c.c = _RS_SC[i]; r.s = f32_add(f32_mul(r.s, z.s), c.s); }
    v.s = f32_mul(z.s, xx);
    aa.s = f32_sub(f32_mul(half.s, yy), f32_mul(v.s, r.s));
    bb.s = f32_sub(f32_mul(z.s, aa.s), yy);
    dd.s = f32_sub(bb.s, f32_mul(v.s, _rs_bits(_RS_SC[0]).s));
    return f32_sub(xx, dd.s);
  }
  static float32_t _rs_kcos(float32_t xx, float32_t yy) {
    union sing z, rc, hz, w2, aa, bb, c, half, one;
    half.c = 0x3f000000U; one.c = 0x3f800000U;
    z.s = f32_mul(xx, xx);
    rc.c = 0;                          // Horner over flop(cc): cc[4..0]
    for ( int i = 5; i-- != 0; ) { c.c = _RS_CC[i]; rc.s = f32_add(f32_mul(rc.s, z.s), c.s); }
    hz.s = f32_mul(half.s, z.s);
    w2.s = f32_sub(one.s, hz.s);
    aa.s = f32_sub(f32_sub(one.s, w2.s), hz.s);
    bb.s = f32_sub(f32_mul(f32_mul(z.s, z.s), rc.s), f32_mul(xx, yy));
    return f32_add(w2.s, f32_add(aa.s, bb.s));
  }
  //  trig-fin: is_sin ? sin(x) : cos(x); ax=|x|, sb=sign bit
  static float32_t _rs_trigfin(int is_sin, float32_t ax, uint32_t sb) {
    union sing qf, r1, r2, w, rhi, rlo, ks, kc, v;
    c3_ds q = (c3_ds)f32_to_i32(f32_mul(ax, _rs_bits(0x3f22f983U).s),
                                softfloat_round_near_even, 0);          // round(ax*2/pi)
    c3_d  aq = (c3_d)(q < 0 ? -q : q);
    qf.s = ui32_to_f32((uint32_t)aq);
    r1.s = f32_sub(ax, f32_mul(qf.s, _rs_bits(0x3fc90000U).s));         // ax - qf*pio2_1
    r2.s = f32_sub(r1.s, f32_mul(qf.s, _rs_bits(0x39fda000U).s));       // r1 - qf*pio2_2
    w.s = f32_mul(qf.s, _rs_bits(0x33a22169U).s);                      // qf*pio2_3
    rhi.s = f32_sub(r2.s, w.s);
    rlo.s = f32_sub(f32_sub(r2.s, rhi.s), w.s);
    int m = (int)(aq & 3);
    ks.s = _rs_ksin(rhi.s, rlo.s);
    kc.s = _rs_kcos(rhi.s, rlo.s);
    if ( is_sin ) {
      v.s = (m==0) ? ks.s : (m==1) ? kc.s : (m==2) ? _rs_neg(ks.s) : _rs_neg(kc.s);
      return (sb == 1) ? _rs_neg(v.s) : v.s;
    }
    return (m==0) ? kc.s : (m==1) ? _rs_neg(ks.s) : (m==2) ? _rs_neg(kc.s) : ks.s;
  }
  static float32_t _rs_sin(float32_t x) {
    union sing r0, ax;
    r0.s = x;
    if ( !f32_eq(x, x) )                          { r0.c = _RS_QNAN; return r0.s; }  // NaN
    if ( (r0.c == _RS_PINF)||(r0.c == _RS_NINF) ) { r0.c = _RS_QNAN; return r0.s; }  // +-inf -> NaN
    if ( (r0.c == 0)||(r0.c == 0x80000000U) )     { return x; }                      // +-0 -> +-0
    ax.c = r0.c & 0x7fffffffU;
    return _rs_trigfin(1, ax.s, r0.c >> 31);
  }
  static float32_t _rs_cos(float32_t x) {
    union sing r0, ax;
    r0.s = x;
    if ( !f32_eq(x, x) )                          { r0.c = _RS_QNAN; return r0.s; }  // NaN
    if ( (r0.c == _RS_PINF)||(r0.c == _RS_NINF) ) { r0.c = _RS_QNAN; return r0.s; }  // +-inf -> NaN
    ax.c = r0.c & 0x7fffffffU;
    return _rs_trigfin(0, ax.s, 0);
  }
  //  tan = (div (sin x) (cos x)): sin/cos kernels %n, the bare div per door r
  static float32_t _rs_tan(float32_t x) {
    float32_t s = _rs_sin(x), c = _rs_cos(x);
    softfloat_roundingMode = _math_rnd;
    float32_t r = f32_div(s, c);
    softfloat_roundingMode = softfloat_round_near_even;
    return r;
  }

/* @rs sqt -- math.hoon ++rs ++sqt = (sqt:^rs x): correctly-rounded f32 sqrt. */
  static float32_t _rs_sqt(float32_t x) {
    union sing r; r.s = f32_sqrt(x);
    if ( !f32_eq(r.s, r.s) ) r.c = _RS_QNAN;        // _nan_unify
    return r.s;
  }

/* @rs log/log-2/log-10 -- math.hoon ++rs ++log/++lr/++log-2/++log-10
**   x = 2^e * m, m in [sqrt(1/2),sqrt(2)); log(1+f) via atanh series (deg-4).
*/
  //  +lr: finite positive x -> *ef = e as @rs, *l1 = log(mantissa)
  static void _rs_lr(float32_t x, float32_t* ef, float32_t* l1) {
    static const uint32_t cs[5] = {
      0x3eaaaaabU, 0x3e4ccccdU, 0x3e124925U, 0x3de38e39U, 0x3dba2e8cU };
    union sing xb, m, f, s, z, p2, r, ll, efa, c, one, half;
    one.c = 0x3f800000U; half.c = 0x3f000000U;
    xb.s = x;
    int sub = (((xb.c >> 23) & 0xffU) == 0);
    if ( sub ) xb.s = f32_mul(x, _rs_bits(0x4b800000U).s);     // *2^24
    int32_t ae = sub ? -24 : 0;
    int32_t e = (int32_t)((xb.c >> 23) & 0xffU) - 127;
    m.c = (xb.c & 0x7fffffU) | 0x3f800000U;
    if ( !f32_lt(m.s, _rs_bits(0x3fb504f3U).s) ) {            // m >= sqrt(2)
      m.s = f32_mul(m.s, half.s); e += 1;
    }
    e += ae;
    f.s = f32_sub(m.s, one.s);
    s.s = f32_div(f.s, f32_add(m.s, one.s));
    z.s = f32_mul(s.s, s.s);
    p2.c = 0; for ( int i = 5; i-- != 0; ) { c.c = cs[i]; p2.s = f32_add(f32_mul(p2.s, z.s), c.s); }
    r.s = f32_mul(f32_add(z.s, z.s), p2.s);
    ll.s = f32_sub(f.s, f32_mul(s.s, f32_sub(f.s, r.s)));
    efa.s = ui32_to_f32((uint32_t)(e < 0 ? -e : e));
    *ef = (e >= 0) ? efa.s : _rs_neg(efa.s);
    *l1 = ll.s;
  }
  //  shared guards for log/log-2/log-10; returns 1 (and sets *g) on a special case
  static int _rs_log_guard(float32_t x, float32_t* g) {
    union sing r0; r0.s = x;
    if ( !f32_eq(x, x) )    { r0.c = _RS_QNAN; *g = r0.s; return 1; }       // NaN
    if ( r0.c == _RS_PINF ) { *g = x; return 1; }                          // +inf -> inf
    if ( (r0.c == 0)||(r0.c == 0x80000000U) ) { r0.c = _RS_NINF; *g = r0.s; return 1; }  // +-0 -> -inf
    if ( (r0.c >> 31) == 1 ){ r0.c = _RS_QNAN; *g = r0.s; return 1; }       // x<0 -> NaN
    return 0;
  }
  static float32_t _rs_log(float32_t x) {
    union sing g, ef, l1, hi, lo;
    if ( _rs_log_guard(x, &g.s) ) return g.s;
    _rs_lr(x, &ef.s, &l1.s);
    hi.s = f32_mul(ef.s, _rs_bits(0x3f317200U).s);                // e*ln2hi
    lo.s = f32_mul(ef.s, _rs_bits(0x35bfbe8eU).s);                // e*ln2lo
    return f32_add(hi.s, f32_add(l1.s, lo.s));
  }
  static float32_t _rs_log2(float32_t x) {
    union sing g, ef, l1;
    if ( _rs_log_guard(x, &g.s) ) return g.s;
    _rs_lr(x, &ef.s, &l1.s);
    return f32_add(ef.s, f32_mul(l1.s, _rs_bits(0x3fb8aa3bU).s)); // e + lm/ln2
  }
  static float32_t _rs_log10(float32_t x) {
    union sing g, ef, l1;
    if ( _rs_log_guard(x, &g.s) ) return g.s;
    _rs_lr(x, &ef.s, &l1.s);
    return f32_add(f32_mul(ef.s, _rs_bits(0x3e9a209bU).s),        // e*log10(2)
                   f32_mul(l1.s, _rs_bits(0x3ede5bd9U).s));       // + lm/ln10
  }

/* @rs cbt -- math.hoon ++rs ++cbt = sign(x) * exp(log|x| / 3). */
  static float32_t _rs_cbt(float32_t x) {
    union sing r0, ax, r;
    r0.s = x;
    if ( !f32_eq(x, x) )                       { return x; }                      // NaN
    if ( (r0.c == 0)||(r0.c == 0x80000000U) )  { return x; }                      // +-0
    ax.c = r0.c & 0x7fffffffU;
    r.s = _rs_exp(f32_mul(_rs_log(ax.s), _rs_bits(0x3eaaaaabU).s));               // exp(log|x|/3)
    return ((r0.c >> 31) == 1) ? _rs_neg(r.s) : r.s;
  }

/* @rs asin/acos -- math.hoon ++rs ++asin/++acos/++rs-ainv
**   rational kernel R(t) = t*P2(t)/(1 + c*t); sqt = _rs_sqt.
*/
  static float32_t _rs_ainv_rr(float32_t t) {
    static const uint32_t ps[3] = { 0x3e2aaa75U, 0xbd2f13baU, 0xbc0dd36bU };
    union sing pp, c, one;
    one.c = 0x3f800000U;
    pp.c = 0; for ( int i = 3; i-- != 0; ) { c.c = ps[i]; pp.s = f32_add(f32_mul(pp.s, t), c.s); }
    return f32_div(f32_mul(t, pp.s),
                   f32_add(one.s, f32_mul(t, _rs_bits(0xbf34e5aeU).s)));
  }
  static float32_t _rs_asin(float32_t x) {
    union sing r0, ax, t, w, r, s, res, half, one, two, pio2h, pio2l, pio4;
    half.c=0x3f000000U; one.c=0x3f800000U; two.c=0x40000000U;
    pio2h.c=0x3fc90fdbU; pio2l.c=0xb33bbd2eU; pio4.c=0x3f490fdbU;
    r0.s = x;
    if ( !f32_eq(x, x) )       { r0.c = _RS_QNAN; return r0.s; }   // NaN
    uint32_t sgn = r0.c >> 31;
    ax.c = r0.c & 0x7fffffffU;
    if ( f32_lt(one.s, ax.s) ) { r0.c = _RS_QNAN; return r0.s; }   // |x|>1 -> NaN
    if ( ax.c == one.c )                                          // |x|==1
      return f32_add(f32_mul(x, pio2h.s), f32_mul(x, pio2l.s));
    if ( f32_lt(ax.s, half.s) ) {                                // |x|<0.5
      if ( f32_lt(ax.s, _rs_bits(0x39800000U).s) ) return x;      // tiny
      t.s = f32_mul(x, x);
      return f32_add(x, f32_mul(x, _rs_ainv_rr(t.s)));
    }
    w.s = f32_sub(one.s, ax.s);
    t.s = f32_mul(w.s, half.s);
    r.s = _rs_ainv_rr(t.s);
    s.s = _rs_sqt(t.s);
    if ( f32_le(_rs_bits(0x3f79999aU).s, ax.s) ) {                // near 1
      res.s = f32_sub(pio2h.s, f32_sub(f32_mul(two.s, f32_add(s.s, f32_mul(s.s, r.s))), pio2l.s));
      return (sgn == 1) ? _rs_neg(res.s) : res.s;
    }
    { union sing df, cc, p2, q2;
      df.c = s.c & 0xfffff000U;
      cc.s = f32_div(f32_sub(t.s, f32_mul(df.s, df.s)), f32_add(s.s, df.s));
      p2.s = f32_sub(f32_mul(two.s, f32_mul(s.s, r.s)), f32_sub(pio2l.s, f32_mul(two.s, cc.s)));
      q2.s = f32_sub(pio4.s, f32_mul(two.s, df.s));
      res.s = f32_sub(pio4.s, f32_sub(p2.s, q2.s));
      return (sgn == 1) ? _rs_neg(res.s) : res.s;
    }
  }
  static float32_t _rs_acos(float32_t x) {
    union sing r0, ax, z, s, r, w, half, one, two, pi, pio2h, pio2l;
    half.c=0x3f000000U; one.c=0x3f800000U; two.c=0x40000000U;
    pi.c=0x40490fdbU; pio2h.c=0x3fc90fdbU; pio2l.c=0xb33bbd2eU;
    r0.s = x;
    if ( !f32_eq(x, x) )       { r0.c = _RS_QNAN; return r0.s; }   // NaN
    uint32_t neg = r0.c >> 31;
    ax.c = r0.c & 0x7fffffffU;
    if ( f32_lt(one.s, ax.s) ) { r0.c = _RS_QNAN; return r0.s; }   // |x|>1 -> NaN
    if ( ax.c == one.c ) {                                        // |x|==1
      if ( neg == 0 ) { union sing z0; z0.c = 0; return z0.s; }    // 1 -> 0
      return f32_add(pi.s, f32_mul(two.s, pio2l.s));               // -1 -> pi
    }
    if ( f32_lt(ax.s, half.s) ) {                                // |x|<0.5
      if ( f32_lt(ax.s, _rs_bits(0x32800000U).s) ) return pio2h.s; // tiny -> pi/2
      z.s = f32_mul(x, x);
      r.s = _rs_ainv_rr(z.s);
      return f32_sub(pio2h.s, f32_sub(x, f32_sub(pio2l.s, f32_mul(x, r.s))));
    }
    if ( neg == 1 ) {                                            // x <= -0.5
      z.s = f32_mul(f32_add(one.s, x), half.s);
      s.s = _rs_sqt(z.s);
      r.s = _rs_ainv_rr(z.s);
      w.s = f32_sub(f32_mul(r.s, s.s), pio2l.s);
      return f32_sub(pi.s, f32_mul(two.s, f32_add(s.s, w.s)));
    }
    z.s = f32_mul(f32_sub(one.s, x), half.s);                    // x >= 0.5
    s.s = _rs_sqt(z.s);
    r.s = _rs_ainv_rr(z.s);
    return f32_mul(two.s, f32_add(s.s, f32_mul(s.s, r.s)));
  }

/* @rs atan/atan2 -- math.hoon ++rs ++atan/++rs-atan/++atan2
**   fdlibm breakpoint reduction (7/16,11/16,19/16,39/16) + deg-4 minimax.
*/
  static float32_t _rs_atan(float32_t x) {
    static const uint32_t at[5] = {
      0x3eaaaaa9U, 0xbe4cca98U, 0x3e11f50dU, 0xbdda1247U, 0x3d7cac25U };
    union sing r0, ax, xr, hi, lo, z, sp, s, res, one, two, ohf, c;
    r0.s = x;
    if ( !f32_eq(x, x) )    { r0.c = _RS_QNAN; return r0.s; }       // NaN
    if ( r0.c == _RS_PINF ) { r0.c = 0x3fc90fdbU; return r0.s; }    // +inf -> pi/2
    if ( r0.c == _RS_NINF ) { r0.c = 0xbfc90fdbU; return r0.s; }    // -inf -> -pi/2
    if ( (r0.c == 0)||(r0.c == 0x80000000U) ) { return x; }         // +-0
    uint32_t neg = r0.c >> 31;
    ax.c = r0.c & 0x7fffffffU;
    one.c=0x3f800000U; two.c=0x40000000U; ohf.c=0x3fc00000U;
    int dir = 0;
    if ( f32_lt(ax.s, _rs_bits(0x3ee00000U).s) ) {                 // |x| < 7/16
      xr.s = ax.s; hi.c = 0; lo.c = 0; dir = 1;
    } else if ( f32_lt(ax.s, _rs_bits(0x3f300000U).s) ) {          // < 11/16
      xr.s = f32_div(f32_sub(f32_add(ax.s, ax.s), one.s), f32_add(two.s, ax.s));
      hi.c = 0x3eed6338U; lo.c = 0x31ac376aU;                      // atan(0.5)
    } else if ( f32_lt(ax.s, _rs_bits(0x3f980000U).s) ) {          // < 19/16
      xr.s = f32_div(f32_sub(ax.s, one.s), f32_add(ax.s, one.s));
      hi.c = 0x3f490fdbU; lo.c = 0xb2bbbd2eU;                      // pi/4
    } else if ( f32_lt(ax.s, _rs_bits(0x401c0000U).s) ) {          // < 39/16
      xr.s = f32_div(f32_sub(ax.s, ohf.s), f32_add(one.s, f32_mul(ohf.s, ax.s)));
      hi.c = 0x3f7b985fU; lo.c = 0xb2d7e096U;                      // atan(1.5)
    } else {                                                       // -1/x
      xr.s = f32_div(_rs_bits(0xbf800000U).s, ax.s);
      hi.c = 0x3fc90fdbU; lo.c = 0xb33bbd2eU;                      // pi/2
    }
    z.s = f32_mul(xr.s, xr.s);
    sp.c = 0; for ( int i = 5; i-- != 0; ) { c.c = at[i]; sp.s = f32_add(f32_mul(sp.s, z.s), c.s); }
    s.s = f32_mul(z.s, sp.s);
    if ( dir ) res.s = f32_sub(xr.s, f32_mul(xr.s, s.s));
    else       res.s = f32_sub(hi.s, f32_sub(f32_sub(f32_mul(xr.s, s.s), lo.s), xr.s));
    return (neg == 1) ? _rs_neg(res.s) : res.s;
  }
  //  bare door ops (div/add/sub/mul) round per _math_rnd; atan kernel is %n.
  static float32_t _rs_atan2(float32_t y, float32_t x) {
    union sing xb, pi, two, zero, mone, q, a, r;
    zero.c = 0; pi.c = 0x40490fdbU; two.c = 0x40000000U; mone.c = 0xbf800000U;
    xb.s = x;
    if ( f32_lt(zero.s, x) ) {                                     // x>0: atan(div y x)
      softfloat_roundingMode = _math_rnd; q.s = f32_div(y, x);
      softfloat_roundingMode = softfloat_round_near_even; return _rs_atan(q.s);
    }
    if ( f32_lt(x, zero.s) && f32_le(zero.s, y) ) {                // x<0,y>=0: add(atan,pi)
      softfloat_roundingMode = _math_rnd; q.s = f32_div(y, x);
      softfloat_roundingMode = softfloat_round_near_even; a.s = _rs_atan(q.s);
      softfloat_roundingMode = _math_rnd; r.s = f32_add(a.s, pi.s);
      softfloat_roundingMode = softfloat_round_near_even; return r.s;
    }
    if ( f32_lt(x, zero.s) && f32_lt(y, zero.s) ) {                // x<0,y<0: sub(atan,pi)
      softfloat_roundingMode = _math_rnd; q.s = f32_div(y, x);
      softfloat_roundingMode = softfloat_round_near_even; a.s = _rs_atan(q.s);
      softfloat_roundingMode = _math_rnd; r.s = f32_sub(a.s, pi.s);
      softfloat_roundingMode = softfloat_round_near_even; return r.s;
    }
    if ( (xb.c == 0) && f32_lt(zero.s, y) ) {                      // x==+0,y>0: div(pi,2)
      softfloat_roundingMode = _math_rnd; r.s = f32_div(pi.s, two.s);
      softfloat_roundingMode = softfloat_round_near_even; return r.s;
    }
    if ( (xb.c == 0) && f32_lt(y, zero.s) ) {                      // x==+0,y<0: mul(-1,div(pi,2))
      softfloat_roundingMode = _math_rnd;
      r.s = f32_mul(mone.s, f32_div(pi.s, two.s));
      softfloat_roundingMode = softfloat_round_near_even; return r.s;
    }
    return zero.s;
  }

/* @rs pow/pow-n -- math.hoon ++rs ++pow/++pow-n */
  static float32_t _rs_pow_n(float32_t x, float32_t n) {
    union sing nn, p, one, two;
    one.c = 0x3f800000U; two.c = 0x40000000U;
    nn.s = n;
    if ( nn.c == 0 ) return one.s;                 // n == +0 -> 1
    softfloat_roundingMode = _math_rnd;            // bare mul/sub round per door r
    p.s = x;
    while ( !f32_lt(n, two.s) ) { p.s = f32_mul(p.s, x); n = f32_sub(n, one.s); }
    softfloat_roundingMode = softfloat_round_near_even;
    return p.s;
  }
  static float32_t _rs_pow(float32_t x, float32_t n) {
    union sing nn, ni, zero, lg, prod;
    zero.c = 0; nn.s = n;
    ni.s = i32_to_f32(f32_to_i32(n, softfloat_round_near_even, 0));   // san (need (toi n)) (mode-indep)
    if ( (nn.c == ni.c) && f32_lt(zero.s, n) )                        // positive integer
      return _rs_pow_n(x, ni.s);
    lg.s = _rs_log(x);                                               // %n kernel
    softfloat_roundingMode = _math_rnd;                             // bare mul per door r
    prod.s = f32_mul(n, lg.s);
    softfloat_roundingMode = softfloat_round_near_even;
    return _rs_exp(prod.s);                                          // %n kernel: exp(n*log x)
  }

/* ===================================================================
** @rh (half-precision) cores -- math.hoon ++rh.  NATIVE f16: each core
** computes entirely in half-precision (SoftFloat f16 ops), mirroring the
** native Hoon arms op-for-op -- NOT widen-to-f32-and-narrow.  Same reductions
** and Horner order as @rd/@rs but with the f16 minimax coeffs read from the
** Hoon (lower polynomial degrees: sin/cos/atan/ainv are shorter, ainv has no
** denominator, acos has no tiny branch).  Marshalling is chub-based (low 16
** bits), so word-size-agnostic like @rd/@rs.  Kernels round near-even; the
** composites (tan/atan2/pow/pow-n) honor the door's r via _math_rnd.
** =================================================================== */

  union half {
    float16_t h;
    uint16_t  c;
  };

  static const uint16_t _RH_QNAN = 0x7e00U;
  static const uint16_t _RH_PINF = 0x7c00U;
  static const uint16_t _RH_NINF = 0xfc00U;

  static inline union half _rh_bits(uint16_t b) { union half u; u.c = b; return u; }
  static inline float16_t  _rh_neg(float16_t a) {            // (sub .0 a)
    union half z; z.c = 0; return f16_sub(z.h, a);
  }

/* @rh exp -- math.hoon ++rh ++exp
**   x = k*ln2 + r (Cody-Waite), exp(x) = 2^k * P(r), P a degree-4 minimax.
*/
  //  pow2(j) = (|j+15|)<<10 as f16 bits = 2^j  (normal range j in [-14,15])
  static inline float16_t _rh_pow2(c3_ds j) {
    int v = (int)(j + 15); if ( v < 0 ) v = -v;
    union half u; u.c = (uint16_t)(v << 10); return u.h;
  }
  //  scale2: ldexp with overflow/subnormal tails (math.hoon ++rh ++exp)
  static inline float16_t _rh_scale2(float16_t p, c3_ds k) {
    if ( (k - 16) >= 0 ) {                             // k>=16
      return f16_mul(f16_mul(p, _rh_pow2(15)), _rh_pow2(k - 15));
    }
    if ( !((k + 14) >= 0) ) {                          // k<-14
      return f16_mul(f16_mul(p, _rh_pow2(k + 11)), _rh_pow2(-11));
    }
    return f16_mul(p, _rh_pow2(k));
  }
  static float16_t _rh_exp(float16_t x) {
    union half r0;
    static const uint16_t cs[5] = { 0x3c00, 0x3c00, 0x3800, 0x3160, 0x295c };
    union half log2e, ln2hi, ln2lo, ka, kf, rr, p, c, zero;
    zero.c = 0;
    r0.h = x;
    if ( !f16_eq(x, x) )    { r0.c = _RH_QNAN; return r0.h; }  // NaN
    if ( r0.c == _RH_PINF ) { return x; }                      // +inf
    if ( r0.c == _RH_NINF ) { r0.c = 0; return r0.h; }         // -inf -> 0

    log2e.c = 0x3dc5; ln2hi.c = 0x3980; ln2lo.c = 0x1dc8;

    c3_ds k = (c3_ds)f16_to_i32(f16_mul(x, log2e.h), softfloat_round_near_even, 0);
    if ( (k - 17) >= 0 )    { r0.c = _RH_PINF; return r0.h; }   // overflow -> inf
    if ( !((k + 24) >= 0) ) { r0.c = 0; return r0.h; }          // underflow -> 0

    ka.h = ui32_to_f16( (uint32_t)(k < 0 ? -k : k) );
    kf.h = (k >= 0) ? ka.h : f16_sub(zero.h, ka.h);
    rr.h = f16_sub( f16_sub(x, f16_mul(kf.h, ln2hi.h)), f16_mul(kf.h, ln2lo.h) );

    p.c = 0;
    for ( int i = 5; i-- != 0; ) {        // Horner over flop(cs): c4..c0
      c.c = cs[i];
      p.h = f16_add(f16_mul(p.h, rr.h), c.h);
    }
    return _rh_scale2(p.h, k);
  }

/* @rh sin/cos/tan -- math.hoon ++rh ++sin/++cos/++rh-trig/++tan
**   x = q*(pi/2) + (rhi+rlo) (3-part pi/2), fdlibm sin/cos kernels by q&3 (each
**   with just 2 coeffs).  tan = sin/cos (the div honors the door r).
*/
  static const uint16_t _RH_SC[2] = { 0xb155, 0x2044 };   // sin kernel coeffs
  static const uint16_t _RH_CC[2] = { 0x2955, 0x95b0 };   // cos kernel coeffs
  static float16_t _rh_ksin(float16_t xx, float16_t yy) {
    union half z, r, v, aa, bb, dd, c, half;
    half.c = 0x3800;
    z.h = f16_mul(xx, xx);
    r.c = 0;                            // Horner over flop(tail sc): sc[1]
    for ( int i = 2; i-- != 1; ) { c.c = _RH_SC[i]; r.h = f16_add(f16_mul(r.h, z.h), c.h); }
    v.h = f16_mul(z.h, xx);
    aa.h = f16_sub(f16_mul(half.h, yy), f16_mul(v.h, r.h));
    bb.h = f16_sub(f16_mul(z.h, aa.h), yy);
    dd.h = f16_sub(bb.h, f16_mul(v.h, _rh_bits(_RH_SC[0]).h));
    return f16_sub(xx, dd.h);
  }
  static float16_t _rh_kcos(float16_t xx, float16_t yy) {
    union half z, rc, hz, w2, aa, bb, c, half, one;
    half.c = 0x3800; one.c = 0x3c00;
    z.h = f16_mul(xx, xx);
    rc.c = 0;                          // Horner over flop(cc): cc[1..0]
    for ( int i = 2; i-- != 0; ) { c.c = _RH_CC[i]; rc.h = f16_add(f16_mul(rc.h, z.h), c.h); }
    hz.h = f16_mul(half.h, z.h);
    w2.h = f16_sub(one.h, hz.h);
    aa.h = f16_sub(f16_sub(one.h, w2.h), hz.h);
    bb.h = f16_sub(f16_mul(f16_mul(z.h, z.h), rc.h), f16_mul(xx, yy));
    return f16_add(w2.h, f16_add(aa.h, bb.h));
  }
  //  trig-fin: is_sin ? sin(x) : cos(x); ax=|x|, sb=sign bit
  static float16_t _rh_trigfin(int is_sin, float16_t ax, uint16_t sb) {
    union half qf, r1, r2, w, rhi, rlo, ks, kc, v;
    c3_ds q = (c3_ds)f16_to_i32(f16_mul(ax, _rh_bits(0x3918).h),
                                softfloat_round_near_even, 0);          // round(ax*2/pi)
    c3_d  aq = (c3_d)(q < 0 ? -q : q);
    qf.h = ui32_to_f16((uint32_t)aq);
    r1.h = f16_sub(ax, f16_mul(qf.h, _rh_bits(0x3e00).h));             // ax - qf*pio2_1
    r2.h = f16_sub(r1.h, f16_mul(qf.h, _rh_bits(0x2c80).h));           // r1 - qf*pio2_2
    w.h = f16_mul(qf.h, _rh_bits(0x0fed).h);                           // qf*pio2_3
    rhi.h = f16_sub(r2.h, w.h);
    rlo.h = f16_sub(f16_sub(r2.h, rhi.h), w.h);
    int m = (int)(aq & 3);
    ks.h = _rh_ksin(rhi.h, rlo.h);
    kc.h = _rh_kcos(rhi.h, rlo.h);
    if ( is_sin ) {
      v.h = (m==0) ? ks.h : (m==1) ? kc.h : (m==2) ? _rh_neg(ks.h) : _rh_neg(kc.h);
      return (sb == 1) ? _rh_neg(v.h) : v.h;
    }
    return (m==0) ? kc.h : (m==1) ? _rh_neg(ks.h) : (m==2) ? _rh_neg(kc.h) : ks.h;
  }
  static float16_t _rh_sin(float16_t x) {
    union half r0, ax;
    r0.h = x;
    if ( !f16_eq(x, x) )                      { r0.c = _RH_QNAN; return r0.h; }  // NaN
    if ( (r0.c == _RH_PINF)||(r0.c == _RH_NINF) ) { r0.c = _RH_QNAN; return r0.h; }  // +-inf -> NaN
    if ( (r0.c == 0)||(r0.c == 0x8000U) )     { return x; }                      // +-0 -> +-0
    ax.c = r0.c & 0x7fffU;
    return _rh_trigfin(1, ax.h, r0.c >> 15);
  }
  static float16_t _rh_cos(float16_t x) {
    union half r0, ax;
    r0.h = x;
    if ( !f16_eq(x, x) )                      { r0.c = _RH_QNAN; return r0.h; }  // NaN
    if ( (r0.c == _RH_PINF)||(r0.c == _RH_NINF) ) { r0.c = _RH_QNAN; return r0.h; }  // +-inf -> NaN
    ax.c = r0.c & 0x7fffU;
    return _rh_trigfin(0, ax.h, 0);
  }
  //  tan = (div (sin x) (cos x)): sin/cos kernels %n, the bare div per door r
  static float16_t _rh_tan(float16_t x) {
    float16_t s = _rh_sin(x), c = _rh_cos(x);
    softfloat_roundingMode = _math_rnd;
    float16_t r = f16_div(s, c);
    softfloat_roundingMode = softfloat_round_near_even;
    return r;
  }

/* @rh sqt -- math.hoon ++rh ++sqt = (sqt:^rh x): correctly-rounded f16 sqrt. */
  static float16_t _rh_sqt(float16_t x) {
    union half r; r.h = f16_sqrt(x);
    if ( !f16_eq(r.h, r.h) ) r.c = _RH_QNAN;        // _nan_unify
    return r.h;
  }

/* @rh log/log-2/log-10 -- math.hoon ++rh ++log/++lr/++log-2/++log-10
**   x = 2^e * m, m in [sqrt(1/2),sqrt(2)); log(1+f) via atanh series (deg-1).
*/
  //  +lr: finite positive x -> *ef = e as @rh, *l1 = log(mantissa)
  static void _rh_lr(float16_t x, float16_t* ef, float16_t* l1) {
    static const uint16_t cs[2] = { 0x3555, 0x3266 };
    union half xb, m, f, s, z, p2, r, ll, efa, c, one, half;
    one.c = 0x3c00; half.c = 0x3800;
    xb.h = x;
    int sub = (((xb.c >> 10) & 0x1fU) == 0);
    if ( sub ) xb.h = f16_mul(x, _rh_bits(0x6400).h);         // *2^10
    int32_t ae = sub ? -10 : 0;
    int32_t e = (int32_t)((xb.c >> 10) & 0x1fU) - 15;
    m.c = (xb.c & 0x3ffU) | 0x3c00U;
    if ( !f16_lt(m.h, _rh_bits(0x3da8).h) ) {                // m >= sqrt(2)
      m.h = f16_mul(m.h, half.h); e += 1;
    }
    e += ae;
    f.h = f16_sub(m.h, one.h);
    s.h = f16_div(f.h, f16_add(m.h, one.h));
    z.h = f16_mul(s.h, s.h);
    p2.c = 0; for ( int i = 2; i-- != 0; ) { c.c = cs[i]; p2.h = f16_add(f16_mul(p2.h, z.h), c.h); }
    r.h = f16_mul(f16_add(z.h, z.h), p2.h);
    ll.h = f16_sub(f.h, f16_mul(s.h, f16_sub(f.h, r.h)));
    efa.h = ui32_to_f16((uint32_t)(e < 0 ? -e : e));
    *ef = (e >= 0) ? efa.h : _rh_neg(efa.h);
    *l1 = ll.h;
  }
  //  shared guards for log/log-2/log-10; returns 1 (and sets *g) on a special case
  static int _rh_log_guard(float16_t x, float16_t* g) {
    union half r0; r0.h = x;
    if ( !f16_eq(x, x) )    { r0.c = _RH_QNAN; *g = r0.h; return 1; }       // NaN
    if ( r0.c == _RH_PINF ) { *g = x; return 1; }                          // +inf -> inf
    if ( (r0.c == 0)||(r0.c == 0x8000U) ) { r0.c = _RH_NINF; *g = r0.h; return 1; }  // +-0 -> -inf
    if ( (r0.c >> 15) == 1 ){ r0.c = _RH_QNAN; *g = r0.h; return 1; }       // x<0 -> NaN
    return 0;
  }
  static float16_t _rh_log(float16_t x) {
    union half g, ef, l1, hi, lo;
    if ( _rh_log_guard(x, &g.h) ) return g.h;
    _rh_lr(x, &ef.h, &l1.h);
    hi.h = f16_mul(ef.h, _rh_bits(0x3980).h);                // e*ln2hi
    lo.h = f16_mul(ef.h, _rh_bits(0x1dc8).h);                // e*ln2lo
    return f16_add(hi.h, f16_add(l1.h, lo.h));
  }
  static float16_t _rh_log2(float16_t x) {
    union half g, ef, l1;
    if ( _rh_log_guard(x, &g.h) ) return g.h;
    _rh_lr(x, &ef.h, &l1.h);
    return f16_add(ef.h, f16_mul(l1.h, _rh_bits(0x3dc5).h));  // e + lm/ln2
  }
  static float16_t _rh_log10(float16_t x) {
    union half g, ef, l1;
    if ( _rh_log_guard(x, &g.h) ) return g.h;
    _rh_lr(x, &ef.h, &l1.h);
    return f16_add(f16_mul(ef.h, _rh_bits(0x34d1).h),         // e*log10(2)
                   f16_mul(l1.h, _rh_bits(0x36f3).h));        // + lm/ln10
  }

/* @rh cbt -- math.hoon ++rh ++cbt = sign(x) * exp(log|x| / 3). */
  static float16_t _rh_cbt(float16_t x) {
    union half r0, ax, r;
    r0.h = x;
    if ( !f16_eq(x, x) )                   { return x; }                          // NaN
    if ( (r0.c == 0)||(r0.c == 0x8000U) )  { return x; }                          // +-0
    ax.c = r0.c & 0x7fffU;
    r.h = _rh_exp(f16_mul(_rh_log(ax.h), _rh_bits(0x3555).h));                    // exp(log|x|/3)
    return ((r0.c >> 15) == 1) ? _rh_neg(r.h) : r.h;
  }

/* @rh asin/acos -- math.hoon ++rh ++asin/++acos/++rh-ainv
**   poly kernel R(t) = t*P(t) (deg-4, NO denominator); sqt = f16_sqrt.  The
**   near-1 asin branch omits pio2l (f16 script), acos has no tiny branch.
*/
  static float16_t _rh_ainv_rr(float16_t t) {
    static const uint16_t ps[4] = { 0x3155, 0x2cea, 0x2729, 0x2ccc };
    union half pp, c;
    pp.c = 0; for ( int i = 4; i-- != 0; ) { c.c = ps[i]; pp.h = f16_add(f16_mul(pp.h, t), c.h); }
    return f16_mul(t, pp.h);
  }
  static float16_t _rh_asin(float16_t x) {
    union half r0, ax, t, w, r, s, res, half, one, two, pio2h, pio2l, pio4, nr1;
    half.c=0x3800; one.c=0x3c00; two.c=0x4000;
    pio2h.c=0x3e48; pio2l.c=0x0fed; pio4.c=0x3a48; nr1.c=0x3bcd;
    r0.h = x;
    if ( !f16_eq(x, x) )       { r0.c = _RH_QNAN; return r0.h; }   // NaN
    uint16_t sgn = r0.c >> 15;
    ax.c = r0.c & 0x7fffU;
    if ( f16_lt(one.h, ax.h) ) { r0.c = _RH_QNAN; return r0.h; }   // |x|>1 -> NaN
    if ( ax.c == one.c )                                          // |x|==1
      return f16_add(f16_mul(x, pio2h.h), f16_mul(x, pio2l.h));
    if ( f16_lt(ax.h, half.h) ) {                                // |x|<0.5
      if ( f16_lt(ax.h, _rh_bits(0x0c00).h) ) return x;           // |x|<2^-12 -> x
      t.h = f16_mul(x, x);
      return f16_add(x, f16_mul(x, _rh_ainv_rr(t.h)));
    }
    w.h = f16_sub(one.h, ax.h);
    t.h = f16_mul(w.h, half.h);
    r.h = _rh_ainv_rr(t.h);
    s.h = f16_sqrt(t.h);
    if ( f16_le(nr1.h, ax.h) ) {                                // |x|>=0.975
      res.h = f16_sub(pio2h.h, f16_mul(two.h, f16_add(s.h, f16_mul(s.h, r.h))));
      return (sgn == 1) ? _rh_neg(res.h) : res.h;
    }
    { union half df, cc, p2, q2;
      df.c = s.c & 0xfff0U;
      cc.h = f16_div(f16_sub(t.h, f16_mul(df.h, df.h)), f16_add(s.h, df.h));
      p2.h = f16_sub(f16_mul(two.h, f16_mul(s.h, r.h)), f16_sub(pio2l.h, f16_mul(two.h, cc.h)));
      q2.h = f16_sub(pio4.h, f16_mul(two.h, df.h));
      res.h = f16_sub(pio4.h, f16_sub(p2.h, q2.h));
      return (sgn == 1) ? _rh_neg(res.h) : res.h;
    }
  }
  static float16_t _rh_acos(float16_t x) {
    union half r0, ax, z, s, r, w, half, one, two, pih, pio2h, pio2l;
    half.c=0x3800; one.c=0x3c00; two.c=0x4000;
    pih.c=0x4248; pio2h.c=0x3e48; pio2l.c=0x0fed;
    r0.h = x;
    if ( !f16_eq(x, x) )       { r0.c = _RH_QNAN; return r0.h; }   // NaN
    uint16_t neg = r0.c >> 15;
    ax.c = r0.c & 0x7fffU;
    if ( f16_lt(one.h, ax.h) ) { r0.c = _RH_QNAN; return r0.h; }   // |x|>1 -> NaN
    if ( ax.c == one.c ) {                                        // |x|==1
      if ( neg == 0 ) { union half z0; z0.c = 0; return z0.h; }    // 1 -> 0
      return f16_add(pih.h, f16_mul(two.h, pio2l.h));              // -1 -> pi
    }
    if ( f16_lt(ax.h, half.h) ) {                                // |x|<0.5
      z.h = f16_mul(x, x);
      r.h = _rh_ainv_rr(z.h);
      return f16_sub(pio2h.h, f16_sub(x, f16_sub(pio2l.h, f16_mul(x, r.h))));
    }
    if ( neg == 1 ) {                                            // x <= -0.5
      z.h = f16_mul(f16_add(one.h, x), half.h);
      s.h = f16_sqrt(z.h);
      r.h = _rh_ainv_rr(z.h);
      w.h = f16_sub(f16_mul(r.h, s.h), pio2l.h);
      return f16_sub(pih.h, f16_mul(two.h, f16_add(s.h, w.h)));
    }
    z.h = f16_mul(f16_sub(one.h, x), half.h);                    // x >= 0.5
    s.h = f16_sqrt(z.h);
    r.h = _rh_ainv_rr(z.h);
    return f16_mul(two.h, f16_add(s.h, f16_mul(s.h, r.h)));
  }

/* @rh atan/atan2 -- math.hoon ++rh ++atan/++rh-atan/++atan2
**   fdlibm breakpoint reduction (7/16,11/16,19/16,39/16) + deg-2 minimax.
*/
  static float16_t _rh_atan(float16_t x) {
    static const uint16_t at[3] = { 0x3555, 0xb266, 0x3092 };
    union half r0, ax, xr, hi, lo, z, sp, s, res, one, two, ohf, c;
    r0.h = x;
    if ( !f16_eq(x, x) )    { r0.c = _RH_QNAN; return r0.h; }       // NaN
    if ( r0.c == _RH_PINF ) { r0.c = 0x3e48; return r0.h; }         // +inf -> pi/2
    if ( r0.c == _RH_NINF ) { r0.c = 0xbe48; return r0.h; }         // -inf -> -pi/2
    if ( (r0.c == 0)||(r0.c == 0x8000U) ) { return x; }            // +-0
    uint16_t neg = r0.c >> 15;
    ax.c = r0.c & 0x7fffU;
    one.c=0x3c00; two.c=0x4000; ohf.c=0x3e00;
    int dir = 0;
    if ( f16_lt(ax.h, _rh_bits(0x3700).h) ) {                     // |x| < 7/16
      xr.h = ax.h; hi.c = 0; lo.c = 0; dir = 1;
    } else if ( f16_lt(ax.h, _rh_bits(0x3980).h) ) {              // < 11/16
      xr.h = f16_div(f16_sub(f16_add(ax.h, ax.h), one.h), f16_add(two.h, ax.h));
      hi.c = 0x376b; lo.c = 0x019c;                               // atan(0.5)
    } else if ( f16_lt(ax.h, _rh_bits(0x3cc0).h) ) {              // < 19/16
      xr.h = f16_div(f16_sub(ax.h, one.h), f16_add(ax.h, one.h));
      hi.c = 0x3a48; lo.c = 0x0bed;                               // pi/4
    } else if ( f16_lt(ax.h, _rh_bits(0x40e0).h) ) {              // < 39/16
      xr.h = f16_div(f16_sub(ax.h, ohf.h), f16_add(one.h, f16_mul(ohf.h, ax.h)));
      hi.c = 0x3bdd; lo.c = 0x87a1;                               // atan(1.5)
    } else {                                                       // -1/x
      xr.h = f16_div(_rh_bits(0xbc00).h, ax.h);
      hi.c = 0x3e48; lo.c = 0x0fed;                               // pi/2
    }
    z.h = f16_mul(xr.h, xr.h);
    sp.c = 0; for ( int i = 3; i-- != 0; ) { c.c = at[i]; sp.h = f16_add(f16_mul(sp.h, z.h), c.h); }
    s.h = f16_mul(z.h, sp.h);
    if ( dir ) res.h = f16_sub(xr.h, f16_mul(xr.h, s.h));
    else       res.h = f16_sub(hi.h, f16_sub(f16_sub(f16_mul(xr.h, s.h), lo.h), xr.h));
    return (neg == 1) ? _rh_neg(res.h) : res.h;
  }
  //  bare door ops (div/add/sub/mul) round per _math_rnd; atan kernel is %n.
  static float16_t _rh_atan2(float16_t y, float16_t x) {
    union half xb, pi, two, zero, mone, q, a, r;
    zero.c = 0; pi.c = 0x4248; two.c = 0x4000; mone.c = 0xbc00;
    xb.h = x;
    if ( f16_lt(zero.h, x) ) {                                     // x>0: atan(div y x)
      softfloat_roundingMode = _math_rnd; q.h = f16_div(y, x);
      softfloat_roundingMode = softfloat_round_near_even; return _rh_atan(q.h);
    }
    if ( f16_lt(x, zero.h) && f16_le(zero.h, y) ) {                // x<0,y>=0: add(atan,pi)
      softfloat_roundingMode = _math_rnd; q.h = f16_div(y, x);
      softfloat_roundingMode = softfloat_round_near_even; a.h = _rh_atan(q.h);
      softfloat_roundingMode = _math_rnd; r.h = f16_add(a.h, pi.h);
      softfloat_roundingMode = softfloat_round_near_even; return r.h;
    }
    if ( f16_lt(x, zero.h) && f16_lt(y, zero.h) ) {                // x<0,y<0: sub(atan,pi)
      softfloat_roundingMode = _math_rnd; q.h = f16_div(y, x);
      softfloat_roundingMode = softfloat_round_near_even; a.h = _rh_atan(q.h);
      softfloat_roundingMode = _math_rnd; r.h = f16_sub(a.h, pi.h);
      softfloat_roundingMode = softfloat_round_near_even; return r.h;
    }
    if ( (xb.c == 0) && f16_lt(zero.h, y) ) {                      // x==+0,y>0: div(pi,2)
      softfloat_roundingMode = _math_rnd; r.h = f16_div(pi.h, two.h);
      softfloat_roundingMode = softfloat_round_near_even; return r.h;
    }
    if ( (xb.c == 0) && f16_lt(y, zero.h) ) {                      // x==+0,y<0: mul(-1,div(pi,2))
      softfloat_roundingMode = _math_rnd;
      r.h = f16_mul(mone.h, f16_div(pi.h, two.h));
      softfloat_roundingMode = softfloat_round_near_even; return r.h;
    }
    return zero.h;
  }

/* @rh pow/pow-n -- math.hoon ++rh ++pow/++pow-n */
  static float16_t _rh_pow_n(float16_t x, float16_t n) {
    union half nn, p, one, two;
    one.c = 0x3c00; two.c = 0x4000;
    nn.h = n;
    if ( nn.c == 0 ) return one.h;                 // n == +0 -> 1
    softfloat_roundingMode = _math_rnd;            // bare mul/sub round per door r
    p.h = x;
    while ( !f16_lt(n, two.h) ) { p.h = f16_mul(p.h, x); n = f16_sub(n, one.h); }
    softfloat_roundingMode = softfloat_round_near_even;
    return p.h;
  }
  static float16_t _rh_pow(float16_t x, float16_t n) {
    union half nn, ni, zero, lg, prod;
    zero.c = 0; nn.h = n;
    ni.h = i32_to_f16(f16_to_i32(n, softfloat_round_near_even, 0));   // san (need (toi n))
    if ( (nn.c == ni.c) && f16_lt(zero.h, n) )                        // positive integer
      return _rh_pow_n(x, ni.h);
    lg.h = _rh_log(x);                                               // %n kernel
    softfloat_roundingMode = _math_rnd;                             // bare mul per door r
    prod.h = f16_mul(n, lg.h);
    softfloat_roundingMode = softfloat_round_near_even;
    return _rh_exp(prod.h);                                          // %n kernel: exp(n*log x)
  }

/* ===================================================================
** @rq (quad, 128-bit) cores -- math.hoon ++rq.  Native f128 algorithms (the
** widest type, no delegation): same reductions as @rd, higher-degree minimax
** in float128_t.  Marshalling reads TWO chubs into float128_t.v[0..1], so it is
** word-size-agnostic -- the chub ABI sidesteps the c3_w*[n=2|4] divergence of
** the old rq.c.  Composite arms honor the door's r via _math_rnd.
** =================================================================== */

  union quad {
    float128_t q;
    c3_d       w[2];        // w[0] = v[0] = low 64 bits, w[1] = v[1] = high 64
  };
  static inline float128_t _rq_bits(c3_d hi, c3_d lo) {
    union quad u; u.w[0] = lo; u.w[1] = hi; return u.q;
  }
  //  by-value wrappers over the pointer-based f128M_* ops (this SoftFloat build
  //  has no by-value f128_*).  Compiler inlines these; keeps the cores readable.
  static inline float128_t _rqm(float128_t a, float128_t b) { float128_t r; f128M_mul(&a,&b,&r); return r; }
  static inline float128_t _rqa(float128_t a, float128_t b) { float128_t r; f128M_add(&a,&b,&r); return r; }
  static inline float128_t _rqs(float128_t a, float128_t b) { float128_t r; f128M_sub(&a,&b,&r); return r; }
  static inline float128_t _rqd(float128_t a, float128_t b) { float128_t r; f128M_div(&a,&b,&r); return r; }
  static inline float128_t _rqq(float128_t a)               { float128_t r; f128M_sqrt(&a,&r);  return r; }
  static inline int _rqeq(float128_t a, float128_t b) { return f128M_eq(&a,&b); }
  static inline int _rqlt(float128_t a, float128_t b) { return f128M_lt(&a,&b); }
  static inline int _rqle(float128_t a, float128_t b) { return f128M_le(&a,&b); }
  static inline c3_ds _rqtoi(float128_t a, int m) { return (c3_ds)f128M_to_i64(&a, (uint_fast8_t)m, 0); }
  static inline float128_t _rqi64(c3_ds n) { float128_t r; i64_to_f128M(n, &r); return r; }
  static const c3_d _RQ_QNAN_HI = 0x7fff800000000000ULL;
  static const c3_d _RQ_PINF_HI = 0x7fff000000000000ULL;
  static const c3_d _RQ_NINF_HI = 0xffff000000000000ULL;
  static inline float128_t _rq_neg(float128_t a) {        // (sub .0 a)
    return _rqs(_rq_bits(0,0), a);
  }

/* @rq exp -- math.hoon ++rq ++exp
**   Cody-Waite x=k*ln2+r, exp=2^k*P(r), P a degree-24 minimax (f128).
*/
  //  pow2(j) = 2^j as f128 bits: exp field (bits 112-126) = j+16383
  static inline float128_t _rq_pow2(c3_ds j) {
    union quad u; u.w[0] = 0; u.w[1] = ((c3_d)(j + 16383)) << 48; return u.q;
  }
  static inline float128_t _rq_scale2(float128_t p, c3_ds k) {
    if ( (k - 16384) >= 0 )
      return _rqm(_rqm(p, _rq_pow2(16383)), _rq_pow2(k - 16383));
    if ( !((k + 16382) >= 0) )
      return _rqm(_rqm(p, _rq_pow2(k + 112)), _rq_pow2(-112));
    return _rqm(p, _rq_pow2(k));
  }
  static float128_t _rq_exp(float128_t x) {
    //  fdlibm rational reconstruction: exp(r) = 1 - ((lo - r*c/(2-c)) - hi),
    //  c = r - t*P(t), t = r*r.  EXC = even minimax P(t) {lo, hi} (deg-10).
    //  (math.hoon ++rq ++exp; faithful ~0.84 ULP, see tools/rq_check.c)
    static const c3_d EXC[11][2] = {
      {0x5555555555555555ULL,0x3ffc555555555555ULL},{0x6c16c16c16c09e83ULL,0xbff66c16c16c16c1ULL},
      {0x6abc0115453d96ddULL,0x3ff11566abc01156ULL},{0xaac663e4a6d65ccaULL,0xbfebbbd779334ef0ULL},
      {0xda06115986f507fbULL,0x3fe666a8f2bf70ebULL},{0x43eb0e288c2e45a8ULL,0xbfe122805d644267ULL},
      {0x12be0476b628552fULL,0x3fdbd6db2c4e0507ULL},{0xeb838f5da821635aULL,0xbfd67da4e1efb419ULL},
      {0xdc61daecbfc0d781ULL,0x3fd1355867f7df64ULL},{0x54bb7852bc52bd9aULL,0xbfcbf56e4264f8adULL},
      {0x822162270789ca71ULL,0x3fc68fc13579bfe0ULL},
    };
    union quad r0; r0.q = x;
    if ( !_rqeq(x, x) )                       return _rq_bits(_RQ_QNAN_HI, 0);   // NaN
    if ( r0.w[1]==_RQ_PINF_HI && r0.w[0]==0 ) return x;                          // +inf
    if ( r0.w[1]==_RQ_NINF_HI && r0.w[0]==0 ) return _rq_bits(0,0);              // -inf -> 0

    float128_t log2e = _rq_bits(0x3fff71547652b82fULL, 0xe1777d0ffda0d23aULL);
    float128_t ln2hi = _rq_bits(0x3ffe62e42fefa39eULL, 0xf35793c800000000ULL);
    float128_t ln2lo = _rq_bits(0xbfad319ff0342542ULL, 0xfc32f366359d274aULL);

    c3_ds k = _rqtoi(_rqm(x, log2e), softfloat_round_near_even);
    if ( (k - 16385) >= 0 )    return _rq_bits(_RQ_PINF_HI, 0);                  // overflow -> inf
    if ( !((k + 16494) >= 0) ) return _rq_bits(0, 0);                           // underflow -> 0

    float128_t ka = _rqi64((c3_ds)(k < 0 ? -k : k));
    float128_t kf = (k >= 0) ? ka : _rq_neg(ka);
    float128_t hi = _rqs(x, _rqm(kf, ln2hi));     // high part of r
    float128_t lo = _rqm(kf, ln2lo);              // low correction
    float128_t r  = _rqs(hi, lo);                 // reduced argument
    float128_t t  = _rqm(r, r);

    float128_t c = _rq_bits(EXC[10][1], EXC[10][0]);
    for ( int i = 10; i-- != 0; )          // Horner P(t)
      c = _rqa(_rqm(c, t), _rq_bits(EXC[i][1], EXC[i][0]));
    c = _rqs(r, _rqm(t, c));               // c = r - t*P(t)

    float128_t one = _rq_bits(0x3fff000000000000ULL, 0);
    float128_t two = _rq_bits(0x4000000000000000ULL, 0);
    float128_t y = _rqs(one, _rqs(_rqs(lo, _rqd(_rqm(r, c), _rqs(two, c))), hi));
    return _rq_scale2(y, k);
  }

/* @rq log/log-2/log-10 -- math.hoon ++rq ++log/++lr/++log-2/++log-10
**   x = 2^e * m, m in [sqrt(1/2),sqrt(2)); log(1+f) via atanh series (deg-22).
*/
  static void _rq_lr(float128_t x, float128_t* ef, float128_t* lm) {
    static const c3_d cs[23][2] = {
      {0x5555555555555555ULL,0x3ffd555555555555ULL},{0x999999999999999aULL,0x3ffc999999999999ULL},
      {0x2492492492492492ULL,0x3ffc249249249249ULL},{0xc71c71c71c71c71cULL,0x3ffbc71c71c71c71ULL},
      {0x5d1745d1745d1746ULL,0x3ffb745d1745d174ULL},{0x3b13b13b13b13b14ULL,0x3ffb3b13b13b13b1ULL},
      {0x1111111111111111ULL,0x3ffb111111111111ULL},{0xe1e1e1e1e1e1e1e2ULL,0x3ffae1e1e1e1e1e1ULL},
      {0x86bca1af286bca1bULL,0x3ffaaf286bca1af2ULL},{0x8618618618618618ULL,0x3ffa861861861861ULL},
      {0x42c8590b21642c86ULL,0x3ffa642c8590b216ULL},{0xae147ae147ae147bULL,0x3ffa47ae147ae147ULL},
      {0x84bda12f684bda13ULL,0x3ffa2f684bda12f6ULL},{0x611a7b9611a7b961ULL,0x3ffa1a7b9611a7b9ULL},
      {0x4210842108421084ULL,0x3ffa084210842108ULL},{0x7c1f07c1f07c1f08ULL,0x3ff9f07c1f07c1f0ULL},
      {0xd41d41d41d41d41dULL,0x3ff9d41d41d41d41ULL},{0xf914c1bacf914c1cULL,0x3ff9bacf914c1bacULL},
      {0xa41a41a41a41a41aULL,0x3ff9a41a41a41a41ULL},{0x9c18f9c18f9c18faULL,0x3ff98f9c18f9c18fULL},
      {0x417d05f417d05f41ULL,0x3ff97d05f417d05fULL},{0x6c16c16c16c16c17ULL,0x3ff96c16c16c16c1ULL},
      {0x72620ae4c415c988ULL,0x3ff95c9882b93105ULL},
    };
    union quad r0; r0.q = x;
    int sub = ( ((r0.w[1] >> 48) & 0x7fffULL) == 0 );
    union quad xx; xx = r0;
    if ( sub ) xx.q = _rqm(x, _rq_bits(0x4077000000000000ULL, 0));   // x * 2^120
    c3_ds ae = sub ? -120 : 0;
    c3_ds e = (c3_ds)((xx.w[1] >> 48) & 0x7fffULL) - 16383;
    union quad m; m.w[0] = xx.w[0]; m.w[1] = (xx.w[1] & 0xffffffffffffULL) | (16383ULL << 48);
    if ( _rqle(_rq_bits(0x3fff6a09e667f3bcULL, 0xc908b2fb1366ea95ULL), m.q) ) {  // m >= sqrt(2)
      m.q = _rqm(m.q, _rq_bits(0x3ffe000000000000ULL, 0)); e = e + 1;
    }
    e = e + ae;
    float128_t one = _rq_bits(0x3fff000000000000ULL, 0);
    float128_t f = _rqs(m.q, one);
    float128_t s = _rqd(f, _rqa(m.q, one));
    float128_t z = _rqm(s, s);
    float128_t p = _rq_bits(0, 0);
    for ( int i = 23; i-- != 0; ) p = _rqa(_rqm(p, z), _rq_bits(cs[i][1], cs[i][0]));
    float128_t r = _rqm(_rqa(z, z), p);
    float128_t l1 = _rqs(f, _rqm(s, _rqs(f, r)));
    float128_t efv = _rqi64( (c3_ds)(e < 0 ? -e : e) );
    if ( e < 0 ) efv = _rq_neg(efv);
    *ef = efv; *lm = l1;
  }
  static int _rq_log_guard(float128_t x, float128_t* out) {
    union quad r0; r0.q = x;
    if ( !_rqeq(x, x) )                       { *out = _rq_bits(_RQ_QNAN_HI, 0); return 1; }
    if ( r0.w[1]==_RQ_PINF_HI && r0.w[0]==0 ) { *out = x;                        return 1; }
    if ( (r0.w[1]==0 && r0.w[0]==0)||(r0.w[1]==0x8000000000000000ULL && r0.w[0]==0) )
                                              { *out = _rq_bits(_RQ_NINF_HI, 0); return 1; }
    if ( (r0.w[1] >> 63) == 1 )               { *out = _rq_bits(_RQ_QNAN_HI, 0); return 1; }
    return 0;
  }
  static float128_t _rq_log(float128_t x) {
    float128_t g, ef, lm;
    if ( _rq_log_guard(x, &g) ) return g;
    _rq_lr(x, &ef, &lm);
    float128_t hi = _rqm(ef, _rq_bits(0x3ffe62e42fefa39eULL, 0xf35793c800000000ULL));   // e*ln2hi
    float128_t lo = _rqm(ef, _rq_bits(0xbfad319ff0342542ULL, 0xfc32f366359d274aULL));   // e*ln2lo
    return _rqa(hi, _rqa(lm, lo));
  }
  static float128_t _rq_log2(float128_t x) {
    float128_t g, ef, lm;
    if ( _rq_log_guard(x, &g) ) return g;
    _rq_lr(x, &ef, &lm);                                                                 // e + lm/ln2
    return _rqa(ef, _rqm(lm, _rq_bits(0x3fff71547652b82fULL, 0xe1777d0ffda0d23aULL)));
  }
  static float128_t _rq_log10(float128_t x) {
    float128_t g, ef, lm;
    if ( _rq_log_guard(x, &g) ) return g;
    _rq_lr(x, &ef, &lm);                                                                 // e*log10(2) + lm/ln10
    return _rqa(_rqm(ef, _rq_bits(0x3ffd34413509f79fULL, 0xef311f12b35816f9ULL)),
                _rqm(lm, _rq_bits(0x3ffdbcb7b1526e50ULL, 0xe32a6ab7555f5a68ULL)));
  }

/* @rq sin/cos/tan -- math.hoon ++rq ++sin/++cos/++rq-trig/++tan
**   x = q*(pi/2) + (rhi+rlo) (2-part pi/2), fdlibm kernels by q&3.  tan=sin/cos.
*/
  static const c3_d _RQ_SC[16][2] = {     // sin kernel coeffs
    {0x5555555555555555ULL,0xbffc555555555555ULL},{0x1111111111111111ULL,0x3ff8111111111111ULL},
    {0xa01a01a01a01a01aULL,0xbff2a01a01a01a01ULL},{0x38faac1c88e50017ULL,0x3fec71de3a556c73ULL},
    {0x38fe747e4b837dc7ULL,0xbfe5ae64567f544eULL},{0x97ca38331d23af68ULL,0x3fde6124613a86d0ULL},
    {0xf11d8656b0ee8cb0ULL,0xbfd6ae7f3e733b81ULL},{0xa6b2605197771b00ULL,0x3fce952c77030ad4ULL},
    {0x724ca1ec3b7b9675ULL,0xbfc62f49b4681415ULL},{0x18bef146fcee6e45ULL,0x3fbd71b8ef6dcf57ULL},
    {0x9d97b8704dd7f628ULL,0xbfb4761b41316381ULL},{0x8d4e44a419776f11ULL,0x3fab3f3ccdd165faULL},
    {0x320a9a18f15d4277ULL,0xbfa1d1ab1c2dcceaULL},{0xd7abe30e7766f129ULL,0x3f98259f98b4358aULL},
    {0xc42e1ee46fa6bfc4ULL,0xbf8e434d2e783f5bULL},{0x1b5382cdffa97422ULL,0x3f843981254dd0d5ULL},
  };
  static const c3_d _RQ_CC[16][2] = {     // cos kernel coeffs
    {0x5555555555555555ULL,0x3ffa555555555555ULL},{0x6c16c16c16c16c17ULL,0xbff56c16c16c16c1ULL},
    {0xa01a01a01a01a01aULL,0x3fefa01a01a01a01ULL},{0xc72ef016d3ea6679ULL,0xbfe927e4fb7789f5ULL},
    {0x7b544da987acfe85ULL,0x3fe21eed8eff8d89ULL},{0xd20badf145dfa3e5ULL,0xbfda93974a8c07c9ULL},
    {0xf11d8656b0ee8cb0ULL,0x3fd2ae7f3e733b81ULL},{0x77bb004886a2c2abULL,0xbfca6827863b97d9ULL},
    {0x507a9cad2bf8f0bbULL,0x3fc1e542ba402022ULL},{0x29450c90b7f338ecULL,0xbfb90ce396db7f85ULL},
    {0x7cca4b4067ca9d8aULL,0x3faff2cf01972f57ULL},{0x9a38f2050ba6b015ULL,0xbfa688e85fc6a4e5ULL},
    {0xd373c5c51c354a8dULL,0x3f9d0a18a2635085ULL},{0xe60caded4c2989c5ULL,0xbf933932c5047d60ULL},
    {0xc42e1ee46fa6bfc4ULL,0x3f89434d2e783f5bULL},{0xa13f8a2b4af9d6b7ULL,0xbf7f2710231c0fd7ULL},
  };
  static float128_t _rq_ksin(float128_t xx, float128_t yy) {
    float128_t half = _rq_bits(0x3ffe000000000000ULL, 0);
    float128_t z = _rqm(xx, xx);
    float128_t r = _rq_bits(0, 0);          // Horner over flop(tail sc): sc[15..1]
    for ( int i = 16; i-- != 1; ) r = _rqa(_rqm(r, z), _rq_bits(_RQ_SC[i][1], _RQ_SC[i][0]));
    float128_t v = _rqm(z, xx);
    float128_t aa = _rqs(_rqm(half, yy), _rqm(v, r));
    float128_t bb = _rqs(_rqm(z, aa), yy);
    float128_t dd = _rqs(bb, _rqm(v, _rq_bits(_RQ_SC[0][1], _RQ_SC[0][0])));
    return _rqs(xx, dd);
  }
  static float128_t _rq_kcos(float128_t xx, float128_t yy) {
    float128_t half = _rq_bits(0x3ffe000000000000ULL, 0), one = _rq_bits(0x3fff000000000000ULL, 0);
    float128_t z = _rqm(xx, xx);
    float128_t rc = _rq_bits(0, 0);         // Horner over flop(cc): cc[15..0]
    for ( int i = 16; i-- != 0; ) rc = _rqa(_rqm(rc, z), _rq_bits(_RQ_CC[i][1], _RQ_CC[i][0]));
    float128_t hz = _rqm(half, z);
    float128_t w2 = _rqs(one, hz);
    float128_t aa = _rqs(_rqs(one, w2), hz);
    float128_t bb = _rqs(_rqm(_rqm(z, z), rc), _rqm(xx, yy));
    return _rqa(w2, _rqa(aa, bb));
  }
  static float128_t _rq_trigfin(int is_sin, float128_t ax, c3_d sb) {
    c3_ds q = _rqtoi(_rqm(ax, _rq_bits(0x3ffe45f306dc9c88ULL, 0x2a53f84eafa3ea6aULL)),
                     softfloat_round_near_even);                       // round(ax*2/pi)
    c3_d aq = (c3_d)(q < 0 ? -q : q);
    float128_t qf = _rqi64((c3_ds)aq);
    float128_t t = _rqs(ax, _rqm(qf, _rq_bits(0x3fff921fb54442d1ULL, 0x8460000000000000ULL))); // ax - qf*pio2_hi
    float128_t w = _rqm(qf, _rq_bits(0x3fc2313198a2e037ULL, 0x07344a409382229aULL));            // qf*pio2_lo
    float128_t rhi = _rqs(t, w);
    float128_t rlo = _rqs(_rqs(t, rhi), w);
    int mm = (int)(aq & 3);
    float128_t ks = _rq_ksin(rhi, rlo);
    float128_t kc = _rq_kcos(rhi, rlo);
    if ( is_sin ) {
      float128_t v = (mm==0) ? ks : (mm==1) ? kc : (mm==2) ? _rq_neg(ks) : _rq_neg(kc);
      return (sb == 1) ? _rq_neg(v) : v;
    }
    return (mm==0) ? kc : (mm==1) ? _rq_neg(ks) : (mm==2) ? _rq_neg(kc) : ks;
  }
  static float128_t _rq_sin(float128_t x) {
    union quad r0, ax; r0.q = x;
    if ( !_rqeq(x, x) )                       return _rq_bits(_RQ_QNAN_HI, 0);   // NaN
    if ( (r0.w[1]==_RQ_PINF_HI||r0.w[1]==_RQ_NINF_HI) && r0.w[0]==0 ) return _rq_bits(_RQ_QNAN_HI, 0);  // +-inf -> NaN
    if ( (r0.w[1]==0||r0.w[1]==0x8000000000000000ULL) && r0.w[0]==0 ) return x;  // +-0 -> +-0
    ax.w[0] = r0.w[0]; ax.w[1] = r0.w[1] & 0x7fffffffffffffffULL;
    return _rq_trigfin(1, ax.q, r0.w[1] >> 63);
  }
  static float128_t _rq_cos(float128_t x) {
    union quad r0, ax; r0.q = x;
    if ( !_rqeq(x, x) )                       return _rq_bits(_RQ_QNAN_HI, 0);   // NaN
    if ( (r0.w[1]==_RQ_PINF_HI||r0.w[1]==_RQ_NINF_HI) && r0.w[0]==0 ) return _rq_bits(_RQ_QNAN_HI, 0);  // +-inf -> NaN
    ax.w[0] = r0.w[0]; ax.w[1] = r0.w[1] & 0x7fffffffffffffffULL;
    return _rq_trigfin(0, ax.q, 0);
  }
  //  tan = (div (sin x) (cos x)): sin/cos kernels %n, the bare div per door r
  static float128_t _rq_tan(float128_t x) {
    float128_t s = _rq_sin(x), c = _rq_cos(x);
    softfloat_roundingMode = _math_rnd;
    float128_t r = _rqd(s, c);
    softfloat_roundingMode = softfloat_round_near_even;
    return r;
  }

/* @rq atan/atan2 -- math.hoon ++rq ++atan/++rq-atan/++atan2
**   fdlibm breakpoint reduction (7/16,11/16,19/16,39/16) + degree-30 minimax.
*/
  static float128_t _rq_atan(float128_t x) {
    static const c3_d at[31][2] = {
      {0x5555555555555555ULL,0x3ffd555555555555ULL},{0x999999999999999aULL,0xbffc999999999999ULL},
      {0x2492492492492492ULL,0x3ffc249249249249ULL},{0xc71c71c71c71c705ULL,0xbffbc71c71c71c71ULL},
      {0x5d1745d1745cf720ULL,0x3ffb745d1745d174ULL},{0x3b13b13b1395a0f6ULL,0xbffb3b13b13b13b1ULL},
      {0x11111111010e24e1ULL,0x3ffb111111111111ULL},{0xe1e1e1d48fd7bd0fULL,0xbffae1e1e1e1e1e1ULL},
      {0x86bc9d8a12661ce3ULL,0x3ffaaf286bca1af2ULL},{0x861762af171f46fbULL,0xbffa861861861861ULL},
      {0x4297f77f1796654aULL,0x3ffa642c8590b216ULL},{0xa6aeeb974d91c763ULL,0xbffa47ae147ae147ULL},
      {0x982c83840df48c76ULL,0x3ffa2f684bda12f5ULL},{0xf24134848b6f9bc3ULL,0xbffa1a7b9611a7a0ULL},
      {0x46f1272e8718edfeULL,0x3ffa084210841eedULL},{0xdac476af1946ed1aULL,0xbff9f07c1f0773e1ULL},
      {0x771b4773d1fdbc46ULL,0x3ff9d41d41cf56a0ULL},{0xd9a2c9f0ffa28317ULL,0xbff9bacf910ca5eaULL},
      {0x5da3c3e48cd55593ULL,0x3ff9a41a3ed6e709ULL},{0x4d8ac15872fbb51cULL,0xbff98f9bfe02ad67ULL},
      {0x069f17b95f6e54f4ULL,0x3ff97d05170d1702ULL},{0x6ea05add542af078ULL,0xbff96c10bc42d041ULL},
      {0xeab121f20635a41aULL,0x3ff95c74e7f6f412ULL},{0x283b75528258306bULL,0xbff94dac2e21aa0eULL},
      {0xdb9d4b05d70c99cfULL,0x3ff93e573faac561ULL},{0x4f5966908860d11aULL,0xbff92af32cae28f7ULL},
      {0xdec0acdf4b356e20ULL,0x3ff90c8b03c55304ULL},{0xf7ad970276c5cf2bULL,0xbff8b4ed3a3349acULL},
      {0xd5dd688f198ae3cbULL,0x3ff8261c1a9eda3aULL},{0x876f4b57d627e71eULL,0xbff71a7ac449b285ULL},
      {0x138131c15128032aULL,0x3ff51a4ea418ebe8ULL},
    };
    union quad r0, ax, xr, hi, lo; r0.q = x;
    if ( !_rqeq(x, x) )                       return _rq_bits(_RQ_QNAN_HI, 0);   // NaN
    if ( r0.w[1]==_RQ_PINF_HI && r0.w[0]==0 ) return _rq_bits(0x3fff921fb54442d1ULL, 0x8469898cc51701b8ULL);  // +inf -> pi/2
    if ( r0.w[1]==_RQ_NINF_HI && r0.w[0]==0 ) return _rq_bits(0xbfff921fb54442d1ULL, 0x8469898cc51701b8ULL);  // -inf -> -pi/2
    if ( (r0.w[1]==0||r0.w[1]==0x8000000000000000ULL) && r0.w[0]==0 ) return x;  // +-0
    c3_d neg = r0.w[1] >> 63;
    ax.w[0] = r0.w[0]; ax.w[1] = r0.w[1] & 0x7fffffffffffffffULL;
    float128_t one = _rq_bits(0x3fff000000000000ULL, 0), two = _rq_bits(0x4000000000000000ULL, 0);
    float128_t ohf = _rq_bits(0x3fff800000000000ULL, 0);
    int dir = 0;
    if ( _rqlt(ax.q, _rq_bits(0x3ffdc00000000000ULL, 0)) ) {             // |x| < 7/16
      xr.q = ax.q; hi.q = _rq_bits(0,0); lo.q = _rq_bits(0,0); dir = 1;
    } else if ( _rqlt(ax.q, _rq_bits(0x3ffe600000000000ULL, 0)) ) {      // < 11/16
      xr.q = _rqd(_rqs(_rqa(ax.q, ax.q), one), _rqa(two, ax.q));
      hi.q = _rq_bits(0x3ffddac670561bb4ULL, 0xf68adfc88bd97875ULL);     // atan(0.5)
      lo.q = _rq_bits(0x3f89a06dc282b0e4ULL, 0xc39be01c59e2dcddULL);
    } else if ( _rqlt(ax.q, _rq_bits(0x3fff300000000000ULL, 0)) ) {      // < 19/16
      xr.q = _rqd(_rqs(ax.q, one), _rqa(ax.q, one));
      hi.q = _rq_bits(0x3ffe921fb54442d1ULL, 0x8469898cc51701b8ULL);     // pi/4
      lo.q = _rq_bits(0x3f8bcd129024e088ULL, 0xa67cc74020bbea64ULL);
    } else if ( _rqlt(ax.q, _rq_bits(0x4000380000000000ULL, 0)) ) {      // < 39/16
      xr.q = _rqd(_rqs(ax.q, ohf), _rqa(one, _rqm(ohf, ax.q)));
      hi.q = _rq_bits(0x3ffef730bd281f69ULL, 0xb200f10f5e197794ULL);     // atan(1.5)
      lo.q = _rq_bits(0xbf8bebe566c99adaULL, 0x9f231bccae27916cULL);
    } else {                                                            // -1/x
      xr.q = _rqd(_rq_bits(0xbfff000000000000ULL, 0), ax.q);
      hi.q = _rq_bits(0x3fff921fb54442d1ULL, 0x8469898cc51701b8ULL);     // pi/2
      lo.q = _rq_bits(0x3f8ccd129024e088ULL, 0xa67cc74020bbea64ULL);
    }
    float128_t z = _rqm(xr.q, xr.q);
    float128_t sp = _rq_bits(0, 0);
    for ( int i = 31; i-- != 0; ) sp = _rqa(_rqm(sp, z), _rq_bits(at[i][1], at[i][0]));
    float128_t s = _rqm(z, sp);
    float128_t res = dir ? _rqs(xr.q, _rqm(xr.q, s))
                         : _rqs(hi.q, _rqs(_rqs(_rqm(xr.q, s), lo.q), xr.q));
    return (neg == 1) ? _rq_neg(res) : res;
  }
  static float128_t _rq_atan2(float128_t y, float128_t x) {
    union quad xb; xb.q = x;
    float128_t zero = _rq_bits(0,0);
    float128_t pi = _rq_bits(0x4000921fb54442d1ULL, 0x8469898cc51701b8ULL);
    float128_t two = _rq_bits(0x4000000000000000ULL, 0), mone = _rq_bits(0xbfff000000000000ULL, 0);
    if ( _rqlt(zero, x) ) {                                              // x>0: atan(div y x)
      softfloat_roundingMode = _math_rnd; float128_t q = _rqd(y, x);
      softfloat_roundingMode = softfloat_round_near_even; return _rq_atan(q);
    }
    if ( _rqlt(x, zero) && _rqle(zero, y) ) {                            // x<0,y>=0: add(atan,pi)
      softfloat_roundingMode = _math_rnd; float128_t q = _rqd(y, x);
      softfloat_roundingMode = softfloat_round_near_even; float128_t a = _rq_atan(q);
      softfloat_roundingMode = _math_rnd; float128_t r = _rqa(a, pi);
      softfloat_roundingMode = softfloat_round_near_even; return r;
    }
    if ( _rqlt(x, zero) && _rqlt(y, zero) ) {                            // x<0,y<0: sub(atan,pi)
      softfloat_roundingMode = _math_rnd; float128_t q = _rqd(y, x);
      softfloat_roundingMode = softfloat_round_near_even; float128_t a = _rq_atan(q);
      softfloat_roundingMode = _math_rnd; float128_t r = _rqs(a, pi);
      softfloat_roundingMode = softfloat_round_near_even; return r;
    }
    if ( (xb.w[1]==0 && xb.w[0]==0) && _rqlt(zero, y) ) {                // x==+0,y>0: div(pi,2)
      softfloat_roundingMode = _math_rnd; float128_t r = _rqd(pi, two);
      softfloat_roundingMode = softfloat_round_near_even; return r;
    }
    if ( (xb.w[1]==0 && xb.w[0]==0) && _rqlt(y, zero) ) {                // x==+0,y<0: mul(-1,div(pi,2))
      softfloat_roundingMode = _math_rnd; float128_t r = _rqm(mone, _rqd(pi, two));
      softfloat_roundingMode = softfloat_round_near_even; return r;
    }
    return zero;
  }

/* @rq asin/acos -- math.hoon ++rq ++asin/++acos/++rq-ainv
**   poly kernel R(t) (deg-30, NOT P/Q) + sqrt head/tail; sqt = f128 sqrt.
*/
  static const c3_d _RQ_RR[31][2] = {
    {0x83f400d50d55a7e8ULL,0x3f8089912e54d43fULL},{0x5555555555555552ULL,0x3ffc555555555555ULL},
    {0x3333333333335009ULL,0x3ffb333333333333ULL},{0x6db6db6db668951bULL,0x3ffa6db6db6db6dbULL},
    {0x71c71c72bac1ec0eULL,0x3ff9f1c71c71c71cULL},{0x8ba2e81aa31a41d8ULL,0x3ff96e8ba2e8ba2eULL},
    {0xec4f0b6fe680df37ULL,0x3ff91c4ec4ec4ec4ULL},{0x996cf372753e7c99ULL,0x3ff8c99999999999ULL},
    {0x92177cc3275353f7ULL,0x3ff87a8787878787ULL},{0xf7f1afe5cbeac090ULL,0x3ff83fde50d79433ULL},
    {0xf127037c33f0fb5bULL,0x3ff812ef3cf3cf83ULL},{0x5c114e9da47dedfdULL,0x3ff7df3bd37a5edeULL},
    {0x909e07297f5e2958ULL,0x3ff7a6863d723133ULL},{0x52ce6cb856ef901fULL,0x3ff7782dd9f3ff64ULL},
    {0x2fdb62f41c709bd1ULL,0x3ff751ba328f884cULL},{0xb0d4961421efa7ecULL,0x3ff731681fe6e02dULL},
    {0x3408e80b5b3f8641ULL,0x3ff715efe556e52aULL},{0x1812712268a45b42ULL,0x3ff6fc96253ecb71ULL},
    {0xebe9bc6e47ec09afULL,0x3ff6d4a82428408aULL},{0xc0a9facb94511de4ULL,0x3ff6aa377fe913f6ULL},
    {0x84737463fe8656d8ULL,0x3ff6b48ca21a48d1ULL},{0xe39134518885e78fULL,0x3ff57e9aa4b5b4c4ULL},
    {0xa0d03ab9c51426b2ULL,0x3ff819064c5185faULL},{0x1c085f962a89aaccULL,0xbff9300f0da2da1eULL},
    {0x97d25a1be10b6c8aULL,0x3ffb0643398cdbcbULL},{0x528e0d54bf4f5e05ULL,0xbffc27ed3dd5cd82ULL},
    {0xad880c8cd533b68bULL,0x3ffd1d64319be957ULL},{0x81c3902a2c54acc7ULL,0xbffd9731e485678bULL},
    {0xa99aa13d6e9cd204ULL,0x3ffdae10872f69b7ULL},{0x25c164c7f61091faULL,0xbffd228fc6527609ULL},
    {0x2c15b8ad9b2377ceULL,0x3ffb9aa4ca63cbd7ULL},
  };
  static float128_t _rq_ainv_rr(float128_t t) {
    float128_t pp = _rq_bits(0, 0);
    for ( int i = 31; i-- != 0; ) pp = _rqa(_rqm(pp, t), _rq_bits(_RQ_RR[i][1], _RQ_RR[i][0]));
    return pp;
  }
  static float128_t _rq_asin(float128_t x) {
    union quad r0, ax; r0.q = x;
    float128_t half = _rq_bits(0x3ffe000000000000ULL, 0), one = _rq_bits(0x3fff000000000000ULL, 0);
    float128_t two = _rq_bits(0x4000000000000000ULL, 0);
    float128_t pio2h = _rq_bits(0x3fff921fb54442d1ULL, 0x8469898cc51701b8ULL);
    float128_t pio2l = _rq_bits(0x3f8ccd129024e088ULL, 0xa67cc74020bbea64ULL);
    float128_t pio4 = _rq_bits(0x3ffe921fb54442d1ULL, 0x8469898cc51701b8ULL);
    if ( !_rqeq(x, x) )       return _rq_bits(_RQ_QNAN_HI, 0);           // NaN
    c3_d sgn = r0.w[1] >> 63;
    ax.w[0] = r0.w[0]; ax.w[1] = r0.w[1] & 0x7fffffffffffffffULL;
    if ( _rqlt(one, ax.q) )  return _rq_bits(_RQ_QNAN_HI, 0);            // |x|>1 -> NaN
    if ( ax.w[1]==0x3fff000000000000ULL && ax.w[0]==0 )                               // |x|==1
      return _rqa(_rqm(x, pio2h), _rqm(x, pio2l));
    if ( _rqlt(ax.q, half) ) {                                          // |x|<0.5
      if ( _rqlt(ax.q, _rq_bits(0x3fc6000000000000ULL, 0)) ) return x;   // tiny
      float128_t t = _rqm(x, x);
      return _rqa(x, _rqm(x, _rq_ainv_rr(t)));
    }
    float128_t w = _rqs(one, ax.q);
    float128_t t = _rqm(w, half);
    float128_t r = _rq_ainv_rr(t);
    float128_t s = _rqq(t);
    if ( _rqle(_rq_bits(0x3ffef33333333333ULL, 0x3333333333333333ULL), ax.q) ) {  // near 1
      float128_t res = _rqs(pio2h, _rqs(_rqm(two, _rqa(s, _rqm(s, r))), pio2l));
      return (sgn == 1) ? _rq_neg(res) : res;
    }
    union quad sq; sq.q = s;
    float128_t df = _rq_bits(sq.w[1], sq.w[0] & 0xff00000000000000ULL);
    float128_t cc = _rqd(_rqs(t, _rqm(df, df)), _rqa(s, df));
    float128_t p2 = _rqs(_rqm(two, _rqm(s, r)), _rqs(pio2l, _rqm(two, cc)));
    float128_t q2 = _rqs(pio4, _rqm(two, df));
    float128_t res = _rqs(pio4, _rqs(p2, q2));
    return (sgn == 1) ? _rq_neg(res) : res;
  }
  static float128_t _rq_acos(float128_t x) {
    union quad r0, ax; r0.q = x;
    float128_t half = _rq_bits(0x3ffe000000000000ULL, 0), one = _rq_bits(0x3fff000000000000ULL, 0);
    float128_t two = _rq_bits(0x4000000000000000ULL, 0);
    float128_t pi = _rq_bits(0x4000921fb54442d1ULL, 0x8469898cc51701b8ULL);
    float128_t pio2h = _rq_bits(0x3fff921fb54442d1ULL, 0x8469898cc51701b8ULL);
    float128_t pio2l = _rq_bits(0x3f8ccd129024e088ULL, 0xa67cc74020bbea64ULL);
    if ( !_rqeq(x, x) )       return _rq_bits(_RQ_QNAN_HI, 0);           // NaN
    c3_d neg = r0.w[1] >> 63;
    ax.w[0] = r0.w[0]; ax.w[1] = r0.w[1] & 0x7fffffffffffffffULL;
    if ( _rqlt(one, ax.q) )  return _rq_bits(_RQ_QNAN_HI, 0);            // |x|>1 -> NaN
    if ( ax.w[1]==0x3fff000000000000ULL && ax.w[0]==0 ) {                             // |x|==1
      if ( neg == 0 ) return _rq_bits(0, 0);                             // 1 -> 0
      return _rqa(pi, _rqm(two, pio2l));                                 // -1 -> pi
    }
    if ( _rqlt(ax.q, half) ) {                                          // |x|<0.5
      if ( _rqlt(ax.q, _rq_bits(0x3f87000000000000ULL, 0)) ) return pio2h;  // tiny -> pi/2
      float128_t z = _rqm(x, x);
      float128_t r = _rq_ainv_rr(z);
      return _rqs(pio2h, _rqs(x, _rqs(pio2l, _rqm(x, r))));
    }
    if ( neg == 1 ) {                                                   // x <= -0.5
      float128_t z = _rqm(_rqa(one, x), half);
      float128_t s = _rqq(z);
      float128_t r = _rq_ainv_rr(z);
      float128_t w = _rqs(_rqm(r, s), pio2l);
      return _rqs(pi, _rqm(two, _rqa(s, w)));
    }
    float128_t z = _rqm(_rqs(one, x), half);                            // x >= 0.5
    float128_t s = _rqq(z);
    union quad sq; sq.q = s;
    float128_t df = _rq_bits(sq.w[1], sq.w[0] & 0xff00000000000000ULL);
    float128_t cc = _rqd(_rqs(z, _rqm(df, df)), _rqa(s, df));
    float128_t r = _rq_ainv_rr(z);
    float128_t w = _rqa(_rqm(r, s), cc);
    return _rqm(two, _rqa(df, w));
  }

/* @rq sqt/cbt -- math.hoon ++rq ++sqt/++cbt */
  static float128_t _rq_sqt(float128_t x) {
    union quad r0; r0.q = x;
    if ( !_rqeq(x, x) )                       return _rq_bits(_RQ_QNAN_HI, 0);   // NaN
    if ( r0.w[1]==_RQ_PINF_HI && r0.w[0]==0 ) return x;                          // +inf
    if ( (r0.w[1]==0||r0.w[1]==0x8000000000000000ULL) && r0.w[0]==0 ) return x;  // +-0
    if ( (r0.w[1] >> 63) == 1 )               return _rq_bits(_RQ_QNAN_HI, 0);   // x<0 -> NaN
    return _rqq(x);                                                              // correctly-rounded
  }
  static float128_t _rq_cbt(float128_t x) {
    union quad r0, ax; r0.q = x;
    if ( !_rqeq(x, x) )                       return x;                          // NaN -> NaN
    if ( (r0.w[1]==0||r0.w[1]==0x8000000000000000ULL) && r0.w[0]==0 ) return x;  // +-0
    ax.w[0] = r0.w[0]; ax.w[1] = r0.w[1] & 0x7fffffffffffffffULL;
    float128_t r = _rq_exp(_rqm(_rq_log(ax.q), _rq_bits(0x3ffd555555555555ULL, 0x5555555555555555ULL)));  // exp(log|x|/3)
    return ((r0.w[1] >> 63) == 1) ? _rq_neg(r) : r;
  }

/* @rq pow/pow-n -- math.hoon ++rq ++pow/++pow-n */
  static float128_t _rq_pow_n(float128_t x, float128_t n) {
    union quad nn; nn.q = n;
    float128_t one = _rq_bits(0x3fff000000000000ULL, 0), two = _rq_bits(0x4000000000000000ULL, 0);
    if ( nn.w[1]==0 && nn.w[0]==0 ) return one;    // n == +0 -> 1
    softfloat_roundingMode = _math_rnd;            // bare mul/sub round per door r
    float128_t p = x;
    while ( !_rqlt(n, two) ) { p = _rqm(p, x); n = _rqs(n, one); }
    softfloat_roundingMode = softfloat_round_near_even;
    return p;
  }
  static float128_t _rq_pow(float128_t x, float128_t n) {
    union quad nn, ni; nn.q = n;
    float128_t zero = _rq_bits(0,0);
    ni.q = _rqi64(_rqtoi(n, softfloat_round_near_even));               // san (need (toi n))
    if ( (nn.w[1]==ni.w[1] && nn.w[0]==ni.w[0]) && _rqlt(zero, n) )    // positive integer
      return _rq_pow_n(x, ni.q);
    float128_t lg = _rq_log(x);                                       // %n kernel
    softfloat_roundingMode = _math_rnd;                              // bare mul per door r
    float128_t prod = _rqm(n, lg);
    softfloat_roundingMode = softfloat_round_near_even;
    return _rq_exp(prod);                                             // %n kernel: exp(n*log x)
  }

#ifndef MATH_JET_HARNESS

/* u3 ABI wrappers.  Each transcendental forces round-near-even; the @rd door's
** rounding axis is ignored (the Hoon does the same).  Sample is the single @rd
** at u3x_sam.
*/
  static u3_noun
  _rd_jet(u3_noun cor, float64_t (*fun)(float64_t))
  {
    u3_noun x = u3r_at(u3x_sam, cor);
    if ( u3_none == x || c3n == u3ud(x) ) {
      return u3m_bail(c3__exit);
    }
    {
      union doub c, e;
      softfloat_roundingMode = softfloat_round_near_even;
      _math_rnd = _rnd_of(u3r_at(60, cor));      // door rounding r (for composite arms)
      c.c = u3r_chub(0, x);
      e.d = fun(c.d);
      return u3i_chubs(1, &e.c);
    }
  }

  u3_noun u3qi_rd_exp(u3_atom a)
  {
    union doub c, e;
    softfloat_roundingMode = softfloat_round_near_even;
    c.c = u3r_chub(0, a);
    e.d = _rd_exp(c.d);
    return u3i_chubs(1, &e.c);
  }
  u3_noun u3wi_rd_exp(u3_noun cor) { return _rd_jet(cor, _rd_exp); }

  u3_noun u3qi_rd_log(u3_atom a)
  {
    union doub c, e;
    softfloat_roundingMode = softfloat_round_near_even;
    c.c = u3r_chub(0, a);
    e.d = _rd_log(c.d);
    return u3i_chubs(1, &e.c);
  }
  u3_noun u3wi_rd_log(u3_noun cor) { return _rd_jet(cor, _rd_log); }

  u3_noun u3qi_rd_sin(u3_atom a)
  {
    union doub c, e;
    softfloat_roundingMode = softfloat_round_near_even;
    c.c = u3r_chub(0, a);
    e.d = _rd_sin(c.d);
    return u3i_chubs(1, &e.c);
  }
  u3_noun u3wi_rd_sin(u3_noun cor) { return _rd_jet(cor, _rd_sin); }

  u3_noun u3qi_rd_cos(u3_atom a)
  {
    union doub c, e;
    softfloat_roundingMode = softfloat_round_near_even;
    c.c = u3r_chub(0, a);
    e.d = _rd_cos(c.d);
    return u3i_chubs(1, &e.c);
  }
  u3_noun u3wi_rd_cos(u3_noun cor) { return _rd_jet(cor, _rd_cos); }

  u3_noun u3qi_rd_tan(u3_atom a)
  {
    union doub c, e;
    softfloat_roundingMode = softfloat_round_near_even;
    c.c = u3r_chub(0, a);
    e.d = _rd_tan(c.d);
    return u3i_chubs(1, &e.c);
  }
  u3_noun u3wi_rd_tan(u3_noun cor) { return _rd_jet(cor, _rd_tan); }

  u3_noun u3qi_rd_atan(u3_atom a)
  {
    union doub c, e;
    softfloat_roundingMode = softfloat_round_near_even;
    c.c = u3r_chub(0, a);
    e.d = _rd_atan(c.d);
    return u3i_chubs(1, &e.c);
  }
  u3_noun u3wi_rd_atan(u3_noun cor) { return _rd_jet(cor, _rd_atan); }

  u3_noun u3qi_rd_atan2(u3_atom y, u3_atom x)
  {
    union doub yy, xx, e;
    softfloat_roundingMode = softfloat_round_near_even;
    yy.c = u3r_chub(0, y);
    xx.c = u3r_chub(0, x);
    e.d = _rd_atan2(yy.d, xx.d);
    return u3i_chubs(1, &e.c);
  }
  u3_noun u3wi_rd_atan2(u3_noun cor)
  {
    u3_noun y, x;
    if ( c3n == u3r_mean(cor, {u3x_sam_2, &y}, {u3x_sam_3, &x}) ||
         c3n == u3ud(y) || c3n == u3ud(x) ) {
      return u3m_bail(c3__exit);
    }
    _math_rnd = _rnd_of(u3r_at(60, cor));         // door rounding r
    return u3qi_rd_atan2(y, x);
  }

  u3_noun u3qi_rd_asin(u3_atom a)
  {
    union doub c, e;
    softfloat_roundingMode = softfloat_round_near_even;
    c.c = u3r_chub(0, a);
    e.d = _rd_asin(c.d);
    return u3i_chubs(1, &e.c);
  }
  u3_noun u3wi_rd_asin(u3_noun cor) { return _rd_jet(cor, _rd_asin); }

  u3_noun u3qi_rd_acos(u3_atom a)
  {
    union doub c, e;
    softfloat_roundingMode = softfloat_round_near_even;
    c.c = u3r_chub(0, a);
    e.d = _rd_acos(c.d);
    return u3i_chubs(1, &e.c);
  }
  u3_noun u3wi_rd_acos(u3_noun cor) { return _rd_jet(cor, _rd_acos); }

  u3_noun u3qi_rd_log2(u3_atom a)
  { union doub c,e; softfloat_roundingMode=softfloat_round_near_even;
    c.c=u3r_chub(0,a); e.d=_rd_log2(c.d); return u3i_chubs(1,&e.c); }
  u3_noun u3wi_rd_log2(u3_noun cor) { return _rd_jet(cor, _rd_log2); }

  u3_noun u3qi_rd_log10(u3_atom a)
  { union doub c,e; softfloat_roundingMode=softfloat_round_near_even;
    c.c=u3r_chub(0,a); e.d=_rd_log10(c.d); return u3i_chubs(1,&e.c); }
  u3_noun u3wi_rd_log10(u3_noun cor) { return _rd_jet(cor, _rd_log10); }

  u3_noun u3qi_rd_sqt(u3_atom a)
  { union doub c,e; softfloat_roundingMode=softfloat_round_near_even;
    c.c=u3r_chub(0,a); e.d=_rd_sqt(c.d); return u3i_chubs(1,&e.c); }
  u3_noun u3wi_rd_sqt(u3_noun cor) { return _rd_jet(cor, _rd_sqt); }

  u3_noun u3qi_rd_cbt(u3_atom a)
  { union doub c,e; softfloat_roundingMode=softfloat_round_near_even;
    c.c=u3r_chub(0,a); e.d=_rd_cbt(c.d); return u3i_chubs(1,&e.c); }
  u3_noun u3wi_rd_cbt(u3_noun cor) { return _rd_jet(cor, _rd_cbt); }

  //  pow / pow-n: sample is [x=@rd n=@rd]
  static u3_noun _rd_jet2(u3_noun cor, float64_t (*fun)(float64_t, float64_t))
  {
    u3_noun x, n;
    if ( c3n == u3r_mean(cor, {u3x_sam_2, &x}, {u3x_sam_3, &n}) ||
         c3n == u3ud(x) || c3n == u3ud(n) ) {
      return u3m_bail(c3__exit);
    }
    { union doub xx, nn, e;
      softfloat_roundingMode = softfloat_round_near_even;
      _math_rnd = _rnd_of(u3r_at(60, cor));      // door rounding r
      xx.c = u3r_chub(0, x); nn.c = u3r_chub(0, n);
      e.d = fun(xx.d, nn.d);
      return u3i_chubs(1, &e.c);
    }
  }
  u3_noun u3qi_rd_pow(u3_atom x, u3_atom n)
  { union doub xx,nn,e; softfloat_roundingMode=softfloat_round_near_even;
    xx.c=u3r_chub(0,x); nn.c=u3r_chub(0,n); e.d=_rd_pow(xx.d,nn.d); return u3i_chubs(1,&e.c); }
  u3_noun u3wi_rd_pow(u3_noun cor) { return _rd_jet2(cor, _rd_pow); }

  u3_noun u3qi_rd_pow_n(u3_atom x, u3_atom n)
  { union doub xx,nn,e; softfloat_roundingMode=softfloat_round_near_even;
    xx.c=u3r_chub(0,x); nn.c=u3r_chub(0,n); e.d=_rd_pow_n(xx.d,nn.d); return u3i_chubs(1,&e.c); }
  u3_noun u3wi_rd_pow_n(u3_noun cor) { return _rd_jet2(cor, _rd_pow_n); }

/* @rs ABI wrappers.  @rs is a 32-bit atom: read the low 32 bits of the chub,
** write the 32-bit result as a chub (high bits zero -> normalizes to a 32-bit
** atom).  Chub I/O keeps this word-size-agnostic, like the @rd wrappers.
*/
  static inline float32_t _rs_in(u3_atom a) {
    union sing s; s.c = (uint32_t)u3r_chub(0, a); return s.s;
  }
  static inline u3_noun _rs_out(float32_t v) {
    union sing s; s.s = v; { c3_d out = (c3_d)s.c; return u3i_chubs(1, &out); }
  }
  static u3_noun _rs_jet(u3_noun cor, float32_t (*fun)(float32_t)) {
    u3_noun x = u3r_at(u3x_sam, cor);
    if ( u3_none == x || c3n == u3ud(x) ) return u3m_bail(c3__exit);
    softfloat_roundingMode = softfloat_round_near_even;
    _math_rnd = _rnd_of(u3r_at(60, cor));        // door rounding r (for @rs tan)
    return _rs_out(fun(_rs_in(x)));
  }
  static u3_noun _rs_jet2(u3_noun cor, float32_t (*fun)(float32_t, float32_t)) {
    u3_noun x, n;
    if ( c3n == u3r_mean(cor, {u3x_sam_2, &x}, {u3x_sam_3, &n}) ||
         c3n == u3ud(x) || c3n == u3ud(n) ) {
      return u3m_bail(c3__exit);
    }
    softfloat_roundingMode = softfloat_round_near_even;
    _math_rnd = _rnd_of(u3r_at(60, cor));        // door rounding r
    return _rs_out(fun(_rs_in(x), _rs_in(n)));
  }

  u3_noun u3qi_rs_exp(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_exp(_rs_in(a))); }
  u3_noun u3wi_rs_exp(u3_noun cor) { return _rs_jet(cor, _rs_exp); }
  u3_noun u3qi_rs_log(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_log(_rs_in(a))); }
  u3_noun u3wi_rs_log(u3_noun cor) { return _rs_jet(cor, _rs_log); }
  u3_noun u3qi_rs_sin(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_sin(_rs_in(a))); }
  u3_noun u3wi_rs_sin(u3_noun cor) { return _rs_jet(cor, _rs_sin); }
  u3_noun u3qi_rs_cos(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_cos(_rs_in(a))); }
  u3_noun u3wi_rs_cos(u3_noun cor) { return _rs_jet(cor, _rs_cos); }
  u3_noun u3qi_rs_tan(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_tan(_rs_in(a))); }
  u3_noun u3wi_rs_tan(u3_noun cor) { return _rs_jet(cor, _rs_tan); }
  u3_noun u3qi_rs_atan(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_atan(_rs_in(a))); }
  u3_noun u3wi_rs_atan(u3_noun cor){ return _rs_jet(cor, _rs_atan); }
  u3_noun u3qi_rs_asin(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_asin(_rs_in(a))); }
  u3_noun u3wi_rs_asin(u3_noun cor){ return _rs_jet(cor, _rs_asin); }
  u3_noun u3qi_rs_acos(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_acos(_rs_in(a))); }
  u3_noun u3wi_rs_acos(u3_noun cor){ return _rs_jet(cor, _rs_acos); }
  u3_noun u3qi_rs_sqt(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_sqt(_rs_in(a))); }
  u3_noun u3wi_rs_sqt(u3_noun cor) { return _rs_jet(cor, _rs_sqt); }
  u3_noun u3qi_rs_cbt(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_cbt(_rs_in(a))); }
  u3_noun u3wi_rs_cbt(u3_noun cor) { return _rs_jet(cor, _rs_cbt); }
  u3_noun u3qi_rs_log2(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_log2(_rs_in(a))); }
  u3_noun u3wi_rs_log2(u3_noun cor){ return _rs_jet(cor, _rs_log2); }
  u3_noun u3qi_rs_log10(u3_atom a) { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_log10(_rs_in(a))); }
  u3_noun u3wi_rs_log10(u3_noun cor){ return _rs_jet(cor, _rs_log10); }

  u3_noun u3qi_rs_atan2(u3_atom y, u3_atom x) { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_atan2(_rs_in(y), _rs_in(x))); }
  u3_noun u3wi_rs_atan2(u3_noun cor){ return _rs_jet2(cor, _rs_atan2); }
  u3_noun u3qi_rs_pow(u3_atom x, u3_atom n)   { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_pow(_rs_in(x), _rs_in(n))); }
  u3_noun u3wi_rs_pow(u3_noun cor) { return _rs_jet2(cor, _rs_pow); }
  u3_noun u3qi_rs_pow_n(u3_atom x, u3_atom n) { softfloat_roundingMode=softfloat_round_near_even; return _rs_out(_rs_pow_n(_rs_in(x), _rs_in(n))); }
  u3_noun u3wi_rs_pow_n(u3_noun cor){ return _rs_jet2(cor, _rs_pow_n); }

/* @rh ABI wrappers.  @rh is a 16-bit atom: read the low 16 bits of the chub,
** write the 16-bit result via chub (high bits zero -> normalizes).  Same
** word-agnostic chub I/O as @rd/@rs.  Wrappers set _math_rnd from the door's r
** (axis 60); the cores apply it to the rs composite ops and the f16 narrow.
*/
  static inline float16_t _rh_in(u3_atom a) {
    union half s; s.c = (uint16_t)u3r_chub(0, a); return s.h;
  }
  static inline u3_noun _rh_out(float16_t v) {
    union half s; s.h = v; { c3_d out = (c3_d)s.c; return u3i_chubs(1, &out); }
  }
  static u3_noun _rh_jet(u3_noun cor, float16_t (*fun)(float16_t)) {
    u3_noun x = u3r_at(u3x_sam, cor);
    if ( u3_none == x || c3n == u3ud(x) ) return u3m_bail(c3__exit);
    softfloat_roundingMode = softfloat_round_near_even;   // kernels run near-even
    _math_rnd = _rnd_of(u3r_at(60, cor));        // door rounding r (for @rh tan/cbt/log-2/log-10)
    return _rh_out(fun(_rh_in(x)));
  }
  static u3_noun _rh_jet2(u3_noun cor, float16_t (*fun)(float16_t, float16_t)) {
    u3_noun x, n;
    if ( c3n == u3r_mean(cor, {u3x_sam_2, &x}, {u3x_sam_3, &n}) ||
         c3n == u3ud(x) || c3n == u3ud(n) ) {
      return u3m_bail(c3__exit);
    }
    softfloat_roundingMode = softfloat_round_near_even;   // kernels run near-even
    _math_rnd = _rnd_of(u3r_at(60, cor));        // door rounding r (composites)
    return _rh_out(fun(_rh_in(x), _rh_in(n)));
  }

  u3_noun u3qi_rh_exp(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_exp(_rh_in(a))); }
  u3_noun u3wi_rh_exp(u3_noun cor) { return _rh_jet(cor, _rh_exp); }
  u3_noun u3qi_rh_log(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_log(_rh_in(a))); }
  u3_noun u3wi_rh_log(u3_noun cor) { return _rh_jet(cor, _rh_log); }
  u3_noun u3qi_rh_sin(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_sin(_rh_in(a))); }
  u3_noun u3wi_rh_sin(u3_noun cor) { return _rh_jet(cor, _rh_sin); }
  u3_noun u3qi_rh_cos(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_cos(_rh_in(a))); }
  u3_noun u3wi_rh_cos(u3_noun cor) { return _rh_jet(cor, _rh_cos); }
  u3_noun u3qi_rh_tan(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_tan(_rh_in(a))); }
  u3_noun u3wi_rh_tan(u3_noun cor) { return _rh_jet(cor, _rh_tan); }
  u3_noun u3qi_rh_atan(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_atan(_rh_in(a))); }
  u3_noun u3wi_rh_atan(u3_noun cor){ return _rh_jet(cor, _rh_atan); }
  u3_noun u3qi_rh_asin(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_asin(_rh_in(a))); }
  u3_noun u3wi_rh_asin(u3_noun cor){ return _rh_jet(cor, _rh_asin); }
  u3_noun u3qi_rh_acos(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_acos(_rh_in(a))); }
  u3_noun u3wi_rh_acos(u3_noun cor){ return _rh_jet(cor, _rh_acos); }
  u3_noun u3qi_rh_sqt(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_sqt(_rh_in(a))); }
  u3_noun u3wi_rh_sqt(u3_noun cor) { return _rh_jet(cor, _rh_sqt); }
  u3_noun u3qi_rh_cbt(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_cbt(_rh_in(a))); }
  u3_noun u3wi_rh_cbt(u3_noun cor) { return _rh_jet(cor, _rh_cbt); }
  u3_noun u3qi_rh_log2(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_log2(_rh_in(a))); }
  u3_noun u3wi_rh_log2(u3_noun cor){ return _rh_jet(cor, _rh_log2); }
  u3_noun u3qi_rh_log10(u3_atom a) { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_log10(_rh_in(a))); }
  u3_noun u3wi_rh_log10(u3_noun cor){ return _rh_jet(cor, _rh_log10); }

  u3_noun u3qi_rh_atan2(u3_atom y, u3_atom x) { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_atan2(_rh_in(y), _rh_in(x))); }
  u3_noun u3wi_rh_atan2(u3_noun cor){ return _rh_jet2(cor, _rh_atan2); }
  u3_noun u3qi_rh_pow(u3_atom x, u3_atom n)   { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_pow(_rh_in(x), _rh_in(n))); }
  u3_noun u3wi_rh_pow(u3_noun cor) { return _rh_jet2(cor, _rh_pow); }
  u3_noun u3qi_rh_pow_n(u3_atom x, u3_atom n) { softfloat_roundingMode=softfloat_round_near_even; return _rh_out(_rh_pow_n(_rh_in(x), _rh_in(n))); }
  u3_noun u3wi_rh_pow_n(u3_noun cor){ return _rh_jet2(cor, _rh_pow_n); }

/* @rq ABI wrappers.  @rq is a 128-bit atom: read/write TWO chubs (v[0]=low 64,
** v[1]=high 64).  Wrappers set softfloat_roundingMode=near-even (kernels) and
** _math_rnd from the door's r (axis 60) for the composites (tan/atan2/pow/
** pow-n).  Word-agnostic chub I/O, same as @rd/@rs/@rh.
*/
  static inline float128_t _rq_in(u3_atom a) {
    union quad s; s.w[0] = u3r_chub(0, a); s.w[1] = u3r_chub(1, a); return s.q;
  }
  static inline u3_noun _rq_out(float128_t v) {
    union quad s; s.q = v; return u3i_chubs(2, &s.w[0]);
  }
  static u3_noun _rq_jet(u3_noun cor, float128_t (*fun)(float128_t)) {
    u3_noun x = u3r_at(u3x_sam, cor);
    if ( u3_none == x || c3n == u3ud(x) ) return u3m_bail(c3__exit);
    softfloat_roundingMode = softfloat_round_near_even;   // kernels run near-even
    _math_rnd = _rnd_of(u3r_at(60, cor));        // door rounding r (for tan/cbt/log-2/log-10)
    return _rq_out(fun(_rq_in(x)));
  }
  static u3_noun _rq_jet2(u3_noun cor, float128_t (*fun)(float128_t, float128_t)) {
    u3_noun x, n;
    if ( c3n == u3r_mean(cor, {u3x_sam_2, &x}, {u3x_sam_3, &n}) ||
         c3n == u3ud(x) || c3n == u3ud(n) ) {
      return u3m_bail(c3__exit);
    }
    softfloat_roundingMode = softfloat_round_near_even;   // kernels run near-even
    _math_rnd = _rnd_of(u3r_at(60, cor));        // door rounding r (composites)
    return _rq_out(fun(_rq_in(x), _rq_in(n)));
  }

  u3_noun u3qi_rq_exp(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_exp(_rq_in(a))); }
  u3_noun u3wi_rq_exp(u3_noun cor) { return _rq_jet(cor, _rq_exp); }
  u3_noun u3qi_rq_log(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_log(_rq_in(a))); }
  u3_noun u3wi_rq_log(u3_noun cor) { return _rq_jet(cor, _rq_log); }
  u3_noun u3qi_rq_sin(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_sin(_rq_in(a))); }
  u3_noun u3wi_rq_sin(u3_noun cor) { return _rq_jet(cor, _rq_sin); }
  u3_noun u3qi_rq_cos(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_cos(_rq_in(a))); }
  u3_noun u3wi_rq_cos(u3_noun cor) { return _rq_jet(cor, _rq_cos); }
  u3_noun u3qi_rq_tan(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_tan(_rq_in(a))); }
  u3_noun u3wi_rq_tan(u3_noun cor) { return _rq_jet(cor, _rq_tan); }
  u3_noun u3qi_rq_atan(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_atan(_rq_in(a))); }
  u3_noun u3wi_rq_atan(u3_noun cor){ return _rq_jet(cor, _rq_atan); }
  u3_noun u3qi_rq_asin(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_asin(_rq_in(a))); }
  u3_noun u3wi_rq_asin(u3_noun cor){ return _rq_jet(cor, _rq_asin); }
  u3_noun u3qi_rq_acos(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_acos(_rq_in(a))); }
  u3_noun u3wi_rq_acos(u3_noun cor){ return _rq_jet(cor, _rq_acos); }
  u3_noun u3qi_rq_sqt(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_sqt(_rq_in(a))); }
  u3_noun u3wi_rq_sqt(u3_noun cor) { return _rq_jet(cor, _rq_sqt); }
  u3_noun u3qi_rq_cbt(u3_atom a)   { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_cbt(_rq_in(a))); }
  u3_noun u3wi_rq_cbt(u3_noun cor) { return _rq_jet(cor, _rq_cbt); }
  u3_noun u3qi_rq_log2(u3_atom a)  { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_log2(_rq_in(a))); }
  u3_noun u3wi_rq_log2(u3_noun cor){ return _rq_jet(cor, _rq_log2); }
  u3_noun u3qi_rq_log10(u3_atom a) { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_log10(_rq_in(a))); }
  u3_noun u3wi_rq_log10(u3_noun cor){ return _rq_jet(cor, _rq_log10); }

  u3_noun u3qi_rq_atan2(u3_atom y, u3_atom x) { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_atan2(_rq_in(y), _rq_in(x))); }
  u3_noun u3wi_rq_atan2(u3_noun cor){ return _rq_jet2(cor, _rq_atan2); }
  u3_noun u3qi_rq_pow(u3_atom x, u3_atom n)   { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_pow(_rq_in(x), _rq_in(n))); }
  u3_noun u3wi_rq_pow(u3_noun cor) { return _rq_jet2(cor, _rq_pow); }
  u3_noun u3qi_rq_pow_n(u3_atom x, u3_atom n) { softfloat_roundingMode=softfloat_round_near_even; return _rq_out(_rq_pow_n(_rq_in(x), _rq_in(n))); }
  u3_noun u3wi_rq_pow_n(u3_noun cor){ return _rq_jet2(cor, _rq_pow_n); }

#endif
