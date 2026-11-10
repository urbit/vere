/// @file

#include "jets/k.h"
#include "jets/q.h"
#include "jets/u.h"
#include "jets/w.h"

#include "noun.h"

/* _cqe_haj: the hash of an hmac: [wid dat] to the digest.  TRANSFERS.
*/
typedef u3_atom (*_cqe_haj)(void* haj_v, u3_atom wid, u3_atom dat);

/* _cqe_hmac(): +hmac:hmac with the hash haj_f, of block size boq and
**              digest size out, in bytes, on the key [wik key] and the
**              message [wid dat]; boq, out, wik and wid are cats.  RETAINS.
*/
static u3_atom
_cqe_hmac(_cqe_haj haj_f,
          void*    haj_v,
          u3_atom  boq,
          u3_atom  out,
          u3_atom  wik,
          u3_atom  key,
          u3_atom  wid,
          u3_atom  dat)
{
  u3_assert(_(u3a_is_cat(boq)) && _(u3a_is_cat(out)) &&
            _(u3a_is_cat(wik)) && _(u3a_is_cat(wid)));

  // ensure key and message fit signaled lengths
  key = u3qc_end(3, wik, key);
  dat = u3qc_end(3, wid, dat);

  // keys longer than block size are shortened by hashing
  if (wik > boq) {
    key = haj_f(haj_v, wik, key);
    wik = out;
  }

  // keys shorter than block size are right-padded
  if (wik < boq) {
    key = u3kc_lsh(3, (boq - wik), key);
  }

  // pad key, inner and outer
  c3_y trail = (boq % 4);
  c3_y padwords = (boq / 4) + (trail == 0 ? 0 : 1);
  c3_h innpad[padwords], outpad[padwords];
  memset(innpad, 0x36, padwords * 4);
  memset(outpad, 0x5c, padwords * 4);
  if ( trail > 0 ) {
    innpad[padwords-1] = 0x36363636 >> (8 * (4 - trail));
    outpad[padwords-1] = 0x5c5c5c5c >> (8 * (4 - trail));
  }
  u3_atom innkey = u3kc_mix(u3k(key), u3i_halfs(padwords, innpad));
  u3_atom outkey = u3kc_mix(    key , u3i_halfs(padwords, outpad));

  // append inner padding to message, then hash
  u3_atom innmsg = u3ka_add(u3kc_lsh(3, wid, innkey), dat);
  u3_atom innhaj = haj_f(haj_v, u3i_word(wid + boq), innmsg);

  // prepend outer padding to result, hash again
  u3_atom outmsg = u3ka_add(u3kc_lsh(3, out, outkey), innhaj);
  return haj_f(haj_v, u3i_word(out + boq), outmsg);
}

/* _cqe_hmac_gate(): a hash that is a gate, haj_v its call site.
*/
static u3_atom
_cqe_hmac_gate(void* haj_v, u3_atom wid, u3_atom dat)
{
  return u3j_gate_slam((u3j_site*)haj_v, u3nc(wid, dat));
}

/* _cqe_hmac_sha1(): +sha-1l:sha.
*/
static u3_atom
_cqe_hmac_sha1(void* haj_v, u3_atom wid, u3_atom dat)
{
  u3_atom pro = u3qe_sha1(wid, dat);

  u3z(wid); u3z(dat);
  return pro;
}

/* _cqe_hmac_flim(): :(cork flim haj_f (flip out)), as +sha-256l:sha and
**                   +sha-512l:sha are: the bytes of the message reversed,
**                   hashed, and the bytes of the digest reversed.
*/
static u3_atom
_cqe_hmac_flim(u3_atom (*haj_f)(u3_atom, u3_atom),
               u3_atom   out,
               u3_atom   wid,
               u3_atom   dat)
{
  u3_atom tad = u3qc_rev(3, wid, dat);
  u3_atom dig = haj_f(wid, tad);
  u3_atom pro = u3qc_rev(3, out, dig);

  u3z(tad); u3z(dig); u3z(wid); u3z(dat);
  return pro;
}

/* _cqe_hmac_sha256(): +sha-256l:sha.
*/
static u3_atom
_cqe_hmac_sha256(void* haj_v, u3_atom wid, u3_atom dat)
{
  return _cqe_hmac_flim(u3qe_shay, 32, wid, dat);
}

/* _cqe_hmac_sha512(): +sha-512l:sha.
*/
static u3_atom
_cqe_hmac_sha512(void* haj_v, u3_atom wid, u3_atom dat)
{
  return _cqe_hmac_flim(u3qe_shal, 64, wid, dat);
}

  u3_noun
  u3qe_hmac(u3_noun haj,
            u3_atom boq,
            u3_atom out,
            u3_atom wik,
            u3_atom key,
            u3_atom wid,
            u3_atom dat)
  {
    u3j_site sit_u;
    u3_atom  pro;

    u3j_gate_prep(&sit_u, u3k(haj));
    pro = _cqe_hmac(_cqe_hmac_gate, &sit_u, boq, out, wik, key, wid, dat);
    u3j_gate_lose(&sit_u);
    return pro;
  }

/* u3qe_hmac_sha{1,256,512}l(): +hmac-sha{1,256,512}l:hmac, on byts.
**                              wik and wid are cats.  RETAINS.
*/
u3_atom
u3qe_hmac_sha1l(u3_atom wik, u3_atom key, u3_atom wid, u3_atom dat)
{
  return _cqe_hmac(_cqe_hmac_sha1, 0, 64, 20, wik, key, wid, dat);
}

u3_atom
u3qe_hmac_sha256l(u3_atom wik, u3_atom key, u3_atom wid, u3_atom dat)
{
  return _cqe_hmac(_cqe_hmac_sha256, 0, 64, 32, wik, key, wid, dat);
}

u3_atom
u3qe_hmac_sha512l(u3_atom wik, u3_atom key, u3_atom wid, u3_atom dat)
{
  return _cqe_hmac(_cqe_hmac_sha512, 0, 128, 64, wik, key, wid, dat);
}

/* _cqe_hmac_meet(): an hmac of atoms through its variant on byts, as
**                   +meet:hmac measures them.  RETAINS.
*/
static u3_atom
_cqe_hmac_meet(u3_atom (*byt_f)(u3_atom, u3_atom, u3_atom, u3_atom),
               u3_atom   k,
               u3_atom   m)
{
  u3_atom wik = u3qc_met(3, k);
  u3_atom wid = u3qc_met(3, m);

  if ( (c3n == u3a_is_cat(wik)) || (c3n == u3a_is_cat(wid)) ) {
    u3z(wik); u3z(wid);
    return u3m_bail(c3__exit);
  }

  return byt_f(wik, k, wid, m);
}

/* u3qe_hmac_sha{1,256,512}(): +hmac-sha{1,256,512}:hmac, on atoms.  RETAINS.
*/
u3_atom
u3qe_hmac_sha1(u3_atom k, u3_atom m)
{
  return _cqe_hmac_meet(u3qe_hmac_sha1l, k, m);
}

u3_atom
u3qe_hmac_sha256(u3_atom k, u3_atom m)
{
  return _cqe_hmac_meet(u3qe_hmac_sha256l, k, m);
}

u3_atom
u3qe_hmac_sha512(u3_atom k, u3_atom m)
{
  return _cqe_hmac_meet(u3qe_hmac_sha512l, k, m);
}

/* _cqe_hmac_flip(): an hmac of cords through its variant on atoms, their
**                   bytes reversed as +flip:hmac does.  RETAINS.
*/
static u3_atom
_cqe_hmac_flip(u3_atom (*atm_f)(u3_atom, u3_atom), u3_atom k, u3_atom m)
{
  u3_atom kew = u3qc_swp(3, k);
  u3_atom mew = u3qc_swp(3, m);
  u3_atom pro = atm_f(kew, mew);

  u3z(kew); u3z(mew);
  return pro;
}

/* u3qe_hmac_sha{1,256,512}t(): +hmac-sha{1,256,512}t:hmac, on cords.
**                              RETAINS.
*/
u3_atom
u3qe_hmac_sha1t(u3_atom k, u3_atom m)
{
  return _cqe_hmac_flip(u3qe_hmac_sha1, k, m);
}

u3_atom
u3qe_hmac_sha256t(u3_atom k, u3_atom m)
{
  return _cqe_hmac_flip(u3qe_hmac_sha256, k, m);
}

u3_atom
u3qe_hmac_sha512t(u3_atom k, u3_atom m)
{
  return _cqe_hmac_flip(u3qe_hmac_sha512, k, m);
}

  u3_noun
  u3we_hmac(u3_noun cor)
  {
    u3_noun haj, boq, out, wik, key, wid, dat;

    // sample is [[haj boq out] [wik key] [wid dat]]
    haj = u3h(u3h(u3h(u3t(cor))));
    boq = u3h(u3t(u3h(u3h(u3t(cor)))));
    out = u3t(u3t(u3h(u3h(u3t(cor)))));
    wik = u3h(u3h(u3t(u3h(u3t(cor)))));
    key = u3t(u3h(u3t(u3h(u3t(cor)))));
    wid = u3h(u3t(u3t(u3h(u3t(cor)))));
    dat = u3t(u3t(u3t(u3h(u3t(cor)))));

    if ( (c3n == u3ud(boq)) ||
         (c3n == u3a_is_cat(boq)) ||
         (c3n == u3ud(out)) ||
         (c3n == u3a_is_cat(out)) ||
         (c3n == u3ud(wik)) ||
         (c3n == u3a_is_cat(wik)) ||
         (c3n == u3ud(key)) ||
         (c3n == u3ud(wid)) ||
         (c3n == u3a_is_cat(wid)) ||
         (c3n == u3ud(dat)) )
    {
      return u3m_bail(c3__exit);
    }
    else {
      return u3qe_hmac(haj, boq, out, wik, key, wid, dat);
    }
  }

/* u3ue_hmac(): array driver of u3we_hmac:
** [haj boq out wik key wid dat] in arg[0..6].
*/
u3_noun
u3ue_hmac(u3_noun* arg)
{
  if (  (c3n == u3ud(arg[1]))
     || (c3n == u3a_is_cat(arg[1]))
     || (c3n == u3ud(arg[2]))
     || (c3n == u3a_is_cat(arg[2]))
     || (c3n == u3ud(arg[3]))
     || (c3n == u3a_is_cat(arg[3]))
     || (c3n == u3ud(arg[4]))
     || (c3n == u3ud(arg[5]))
     || (c3n == u3a_is_cat(arg[5]))
     || (c3n == u3ud(arg[6])) )
  {
    return u3m_bail(c3__exit);
  }
  return u3qe_hmac(arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6]);
}

/* _cue_hmac_byts(): a variant on byts, on [wik key wid dat] in arg[0..3],
**                   checked as u3we_hmac checks them.
*/
static u3_noun
_cue_hmac_byts(u3_atom (*byt_f)(u3_atom, u3_atom, u3_atom, u3_atom),
               u3_noun*  arg)
{
  if (  (c3n == u3ud(arg[0]))
     || (c3n == u3a_is_cat(arg[0]))
     || (c3n == u3ud(arg[1]))
     || (c3n == u3ud(arg[2]))
     || (c3n == u3a_is_cat(arg[2]))
     || (c3n == u3ud(arg[3])) )
  {
    return u3m_bail(c3__exit);
  }
  return byt_f(arg[0], arg[1], arg[2], arg[3]);
}

/* _cwe_hmac_byts(): a variant on byts, of its gate: [key=byts msg=byts].
*/
static u3_noun
_cwe_hmac_byts(u3_atom (*byt_f)(u3_atom, u3_atom, u3_atom, u3_atom),
               u3_noun   cor)
{
  u3_noun arg[4];

  if ( c3n == u3r_mean(cor, {u3x_sam_4, &arg[0]}, {u3x_sam_5, &arg[1]},
                            {u3x_sam_6, &arg[2]}, {u3x_sam_7, &arg[3]}) )
  {
    return u3m_bail(c3__exit);
  }
  return _cue_hmac_byts(byt_f, arg);
}

/* _cue_hmac_atom(): a variant on atoms or cords, on [k m] in arg[0..1].
*/
static u3_noun
_cue_hmac_atom(u3_atom (*atm_f)(u3_atom, u3_atom), u3_noun* arg)
{
  if ( (c3n == u3ud(arg[0])) || (c3n == u3ud(arg[1])) ) {
    return u3m_bail(c3__exit);
  }
  return atm_f(arg[0], arg[1]);
}

/* _cwe_hmac_atom(): a variant on atoms or cords, of its gate: [k=@ m=@].
*/
static u3_noun
_cwe_hmac_atom(u3_atom (*atm_f)(u3_atom, u3_atom), u3_noun cor)
{
  u3_noun arg[2];

  if ( c3n == u3r_mean(cor, {u3x_sam_2, &arg[0]}, {u3x_sam_3, &arg[1]}) ) {
    return u3m_bail(c3__exit);
  }
  return _cue_hmac_atom(atm_f, arg);
}

u3_noun u3we_hmac_sha1(u3_noun cor)    { return _cwe_hmac_atom(u3qe_hmac_sha1, cor); }
u3_noun u3we_hmac_sha256(u3_noun cor)  { return _cwe_hmac_atom(u3qe_hmac_sha256, cor); }
u3_noun u3we_hmac_sha512(u3_noun cor)  { return _cwe_hmac_atom(u3qe_hmac_sha512, cor); }
u3_noun u3we_hmac_sha1t(u3_noun cor)   { return _cwe_hmac_atom(u3qe_hmac_sha1t, cor); }
u3_noun u3we_hmac_sha256t(u3_noun cor) { return _cwe_hmac_atom(u3qe_hmac_sha256t, cor); }
u3_noun u3we_hmac_sha512t(u3_noun cor) { return _cwe_hmac_atom(u3qe_hmac_sha512t, cor); }
u3_noun u3we_hmac_sha1l(u3_noun cor)   { return _cwe_hmac_byts(u3qe_hmac_sha1l, cor); }
u3_noun u3we_hmac_sha256l(u3_noun cor) { return _cwe_hmac_byts(u3qe_hmac_sha256l, cor); }
u3_noun u3we_hmac_sha512l(u3_noun cor) { return _cwe_hmac_byts(u3qe_hmac_sha512l, cor); }

/* u3ue_hmac_*(): array drivers of u3we_hmac_*: [k m] in arg[0..1], or for
** the variants on byts [wik key wid dat] in arg[0..3].
*/
u3_noun u3ue_hmac_sha1(u3_noun* arg)    { return _cue_hmac_atom(u3qe_hmac_sha1, arg); }
u3_noun u3ue_hmac_sha256(u3_noun* arg)  { return _cue_hmac_atom(u3qe_hmac_sha256, arg); }
u3_noun u3ue_hmac_sha512(u3_noun* arg)  { return _cue_hmac_atom(u3qe_hmac_sha512, arg); }
u3_noun u3ue_hmac_sha1t(u3_noun* arg)   { return _cue_hmac_atom(u3qe_hmac_sha1t, arg); }
u3_noun u3ue_hmac_sha256t(u3_noun* arg) { return _cue_hmac_atom(u3qe_hmac_sha256t, arg); }
u3_noun u3ue_hmac_sha512t(u3_noun* arg) { return _cue_hmac_atom(u3qe_hmac_sha512t, arg); }
u3_noun u3ue_hmac_sha1l(u3_noun* arg)   { return _cue_hmac_byts(u3qe_hmac_sha1l, arg); }
u3_noun u3ue_hmac_sha256l(u3_noun* arg) { return _cue_hmac_byts(u3qe_hmac_sha256l, arg); }
u3_noun u3ue_hmac_sha512l(u3_noun* arg) { return _cue_hmac_byts(u3qe_hmac_sha512l, arg); }
