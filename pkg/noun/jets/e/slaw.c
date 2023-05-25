/// @fil

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include <ctype.h>

static inline u3_noun
_parse_da(u3_noun a)
{
  u3_weak pro;

  if ( u3_none == (pro = u3s_sift_da(u3x_atom(a))) ) {
    return u3_nul;
  }

  return u3nc(u3_nul, pro);
}

static inline u3_noun
_parse_ud(u3_noun a)
{
  u3_weak pro;

  if ( u3_none == (pro = u3s_sift_ud(u3x_atom(a))) ) {
    return u3_nul;
  }

  return u3nc(u3_nul, pro);
}

static inline u3_noun
_parse_ux(u3_noun a)
{
  u3_weak pro;

  if ( u3_none == (pro = u3s_sift_ux(u3x_atom(a))) ) {
    return u3_nul;
  }

  return u3nc(u3_nul, pro);
}

static
u3_noun get_syllable(c3_c** cur_ptr, c3_c* one, c3_c* two, c3_c* three) {
  if (islower((*cur_ptr)[0]) && islower((*cur_ptr)[1]) &&
      islower((*cur_ptr)[2])) {
    *one = (*cur_ptr)[0];
    *two = (*cur_ptr)[1];
    *three = (*cur_ptr)[2];
    (*cur_ptr) += 3;
    return c3y;
  } else {
    return c3n;
  }
}

static u3_noun
combine(u3_noun p, u3_noun q)
{
  if ( (c3y == u3a_is_atom(p)) || (c3y == u3a_is_atom(q)) ) {
    return 0;
  }

  u3_noun lef = u3qa_mul(256, u3t(q));
  u3_noun ret = u3nc(0, u3qa_add(u3t(p), lef));
  u3z(lef);
  u3z(p); u3z(q);

  return ret;
}

#define ENSURE_NOT_END()  do {                  \
    if (*cur == 0) {                            \
      u3a_free(c);                              \
      return u3_none;                           \
    }                                           \
  } while (0)

#define CONSUME(x)  do {                        \
    if (*cur != x) {                            \
      u3a_free(c);                              \
      return u3_none;                           \
    }                                           \
    cur++;                                      \
  } while (0)

#define TRY_GET_SYLLABLE(prefix)                                        \
  c3_c prefix##_one, prefix##_two, prefix##_three;                      \
  if (c3n == get_syllable(&cur, & prefix##_one, & prefix##_two, & prefix##_three)) { \
    u3a_free(c);                                                        \
    return u3_none;                                                     \
  }

u3_noun
_parse_p(u3_noun cor, u3_noun txt) {
  c3_c* c = u3a_string(txt);

  c3_c* cur = c;
  CONSUME('~');

  // We at least have a sig prefix. We're now going to parse tuples of three
  // lowercase letters. Our naming pattern for the pieces we read is [a b c d
  // ...] as we read them.
  TRY_GET_SYLLABLE(a);

  // There was only one syllable. If it's a valid suffix syllable, then
  // it's a galaxy. We don't even have to run this through the scrambler or
  // check for validity since its already a (unit @).
  if (*cur == 0) {
    u3a_free(c);
    return u3_po_find_suffix(a_one, a_two, a_three);
  }

  TRY_GET_SYLLABLE(b);

  // There were only two syllables. If they are a valid prefix and suffix, then
  // it's a star.
  if (*cur == 0) {
    u3_noun a_part = u3_po_find_prefix(a_one, a_two, a_three);
    u3_noun b_part = u3_po_find_suffix(b_one, b_two, b_three);
    u3_atom combined = combine(b_part, a_part);
    u3a_free(c);
    return combined;
  }

  // There must now be a - or it is invalid
  CONSUME('-');

  TRY_GET_SYLLABLE(c);

  ENSURE_NOT_END();

  TRY_GET_SYLLABLE(d);

  if (*cur == 0) {
    u3_noun a_part = u3_po_find_prefix(a_one, a_two, a_three);
    u3_noun b_part = u3_po_find_suffix(b_one, b_two, b_three);
    u3_noun c_part = u3_po_find_prefix(c_one, c_two, c_three);
    u3_noun d_part = u3_po_find_suffix(d_one, d_two, d_three);

    u3_noun m = combine(d_part, combine(c_part, combine(b_part, a_part)));
    u3a_free(c);

    if (_(u3a_is_atom(m))) {
      return 0;
    }

    u3_atom raw = u3k(u3t(m));
    u3z(m);

    u3_noun ob = u3j_cook("u3we_slaw_ob_p", u3k(cor), "ob");
    u3_noun hok = u3j_cook("u3we_slaw_fynd_p", ob, "fynd");
    return u3nc(0, u3n_slam_on(hok, raw));
  }

  // There must now be a - or it is invalid.
  CONSUME('-');

  // The next possible case is a "short" moon. (~ab-cd-ef)
  TRY_GET_SYLLABLE(e);

  ENSURE_NOT_END();

  TRY_GET_SYLLABLE(f);

  if (*cur == 0) {
    u3_noun a_part = u3_po_find_prefix(a_one, a_two, a_three);
    u3_noun b_part = u3_po_find_suffix(b_one, b_two, b_three);
    u3_noun c_part = u3_po_find_prefix(c_one, c_two, c_three);
    u3_noun d_part = u3_po_find_suffix(d_one, d_two, d_three);
    u3_noun e_part = u3_po_find_prefix(e_one, e_two, e_three);
    u3_noun f_part = u3_po_find_suffix(f_one, f_two, f_three);

    u3_noun m = combine(f_part, combine(e_part, combine(d_part,
                combine(c_part, combine(b_part, a_part)))));
    u3a_free(c);

    if (_(u3a_is_atom(m))) {
      return 0;
    }

    u3_atom raw = u3k(u3t(m));
    u3z(m);
    u3_noun ob = u3j_cook("u3we_slaw_ob_p", u3k(cor), "ob");
    u3_noun hok = u3j_cook("u3we_slaw_fynd_p", ob, "fynd");
    return u3nc(0, u3n_slam_on(hok, raw));
  }

  // There must now be a - or it is invalid.
  CONSUME('-');

  // The next possible case is a "long" moon. (~ab-cd-ef-gh)
  TRY_GET_SYLLABLE(g);

  ENSURE_NOT_END();

  TRY_GET_SYLLABLE(h);

  if (*cur == 0) {
    u3_noun a_part = u3_po_find_prefix(a_one, a_two, a_three);
    u3_noun b_part = u3_po_find_suffix(b_one, b_two, b_three);
    u3_noun c_part = u3_po_find_prefix(c_one, c_two, c_three);
    u3_noun d_part = u3_po_find_suffix(d_one, d_two, d_three);
    u3_noun e_part = u3_po_find_prefix(e_one, e_two, e_three);
    u3_noun f_part = u3_po_find_suffix(f_one, f_two, f_three);
    u3_noun g_part = u3_po_find_prefix(g_one, g_two, g_three);
    u3_noun h_part = u3_po_find_suffix(h_one, h_two, h_three);

    u3_noun m = combine(h_part, combine(g_part, combine(f_part,
                combine(e_part, combine(d_part, combine(c_part,
                combine(b_part, a_part)))))));
    u3a_free(c);

    if (_(u3a_is_atom(m))) {
      return 0;
    }

    u3_atom raw = u3k(u3t(m));
    u3z(m);
    u3_noun ob = u3j_cook("u3we_slaw_ob_p", u3k(cor), "ob");
    u3_noun hok = u3j_cook("u3we_slaw_fynd_p", ob, "fynd");
    return u3nc(0, u3n_slam_on(hok, raw));
  }

  // At this point, the only thing it could be is a long comet, of the form
  // ~ab-cd-ef-gh--ij-kl-mn-op

  CONSUME('-');
  CONSUME('-');

  TRY_GET_SYLLABLE(i);
  ENSURE_NOT_END();
  TRY_GET_SYLLABLE(j);
  CONSUME('-');
  TRY_GET_SYLLABLE(k);
  ENSURE_NOT_END();
  TRY_GET_SYLLABLE(l);
  CONSUME('-');
  TRY_GET_SYLLABLE(m);
  ENSURE_NOT_END();
  TRY_GET_SYLLABLE(n);
  CONSUME('-');
  TRY_GET_SYLLABLE(o);
  ENSURE_NOT_END();
  TRY_GET_SYLLABLE(p);

  if (*cur != 0) {
    // We've parsed all of a comet shape, and there's still more in the
    // string. Bail back to the interpreter.
    u3a_free(c);
    return u3_none;
  }

  // We have a long comet. Time to jam it all together. We rely on combine()
  // for the error checking and we don't have to scramble comet names.
  u3_noun a_part = u3_po_find_prefix(a_one, a_two, a_three);
  u3_noun b_part = u3_po_find_suffix(b_one, b_two, b_three);
  u3_noun c_part = u3_po_find_prefix(c_one, c_two, c_three);
  u3_noun d_part = u3_po_find_suffix(d_one, d_two, d_three);
  u3_noun e_part = u3_po_find_prefix(e_one, e_two, e_three);
  u3_noun f_part = u3_po_find_suffix(f_one, f_two, f_three);
  u3_noun g_part = u3_po_find_prefix(g_one, g_two, g_three);
  u3_noun h_part = u3_po_find_suffix(h_one, h_two, h_three);
  u3_noun i_part = u3_po_find_prefix(i_one, i_two, i_three);
  u3_noun j_part = u3_po_find_suffix(j_one, j_two, j_three);
  u3_noun k_part = u3_po_find_prefix(k_one, k_two, k_three);
  u3_noun l_part = u3_po_find_suffix(l_one, l_two, l_three);
  u3_noun m_part = u3_po_find_prefix(m_one, m_two, m_three);
  u3_noun n_part = u3_po_find_suffix(n_one, n_two, n_three);
  u3_noun o_part = u3_po_find_prefix(o_one, o_two, o_three);
  u3_noun p_part = u3_po_find_suffix(p_one, p_two, p_three);

  u3a_free(c);

  return combine(p_part, combine(o_part, combine(n_part, combine(m_part,
         combine(l_part, combine(k_part, combine(j_part, combine(i_part,
         combine(h_part, combine(g_part, combine(f_part, combine(e_part,
         combine(d_part, combine(c_part, combine(b_part, a_part)))))))))))))));
}


#undef ENSURE_NOT_END
#undef CONSUME
#undef TRY_GET_SYLLABLE

u3_noun
_parse_tas(u3_noun txt) {
  // For any symbol which matches, txt will return itself as a
  // value. Therefore, this is mostly checking validity.
  c3_c* c = u3a_string(txt);

  // First character must represent a lowercase letter
  c3_c* cur = c;
  if (!islower(cur[0])) {
    u3a_free(c);
    return 0;
  }
  cur++;

  while (cur[0] != 0) {
    if (!(islower(cur[0]) || isdigit(cur[0]) || cur[0] == '-')) {
      u3a_free(c);
      return 0;
    }

    cur++;
  }

  u3a_free(c);
  return u3nc(0, u3k(txt));
}

u3_noun
u3we_slaw(u3_noun cor)
{
  u3_noun mod;
  u3_noun txt;

  if (c3n == u3r_mean(cor, u3x_sam_2, &mod,
                      u3x_sam_3, &txt, 0)) {
    return u3m_bail(c3__exit);
  }

  switch (mod) {
    case c3__da:
      return _parse_da(txt);

    case 'p':
      return _parse_p(cor, txt);

    case c3__ud:
      return _parse_ud(txt);

    case c3__ux:
      return _parse_ux(txt);

      // %ta is used once in link.hoon. don't bother.

    case c3__tas:
      return _parse_tas(txt);

    default:
      return u3_none;
  }
}
