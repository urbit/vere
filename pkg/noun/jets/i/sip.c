/// @file
///
/// +sip: lazy cursor over a jammed noun.  Jets the single hot arm +grab
/// (atom at axis), which subsumes +dive, +gaze, +fetch, and the +hop skips
/// on the path.  See the +sip design doc and lib/sip.hoon (the normative spec).
///
/// The cursor is a bit-offset into the jammed atom.  We classify nodes by
/// their leading tag bits (0 = atom, 10 = cell, 11 = backref), skip subtrees
/// with an explicit pending-node counter (no C recursion), follow backrefs
/// backward, and finally +rub the target atom out.  Every read is
/// bounds-checked against u3r_met(0, a); malformed input, overrun, a forward
/// or self backref, an out-of-range axis, or a type mismatch all
/// u3m_bail(c3__exit) -- crash parity with the Hoon, which is ground truth.
///
/// Cursor arithmetic is 64-bit (c3_d).  u3r_met returns a c3_w, so any
/// addressable buffer is < 2^32 bits and every in-range offset fits a c3_w;
/// we bail on any decode whose unary length would exceed what a 64-bit cursor
/// can represent (c > 63), which no jam-derived buffer ever contains.

#include "jets/k.h"
#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

//  _sip_bit(): bit [off] of [a], 0 past the end (matches +cut).  RETAIN [a]
//
static inline c3_b
_sip_bit(c3_d off_d, u3_atom a, c3_d met_d)
{
  return ( off_d >= met_d ) ? 0 : u3r_bit((c3_w)off_d, a);
}

//  _sip_rub(): decode the +mat at [off], as +rub.  RETAIN [a]
//
//    Writes the bit-span (bits consumed) to [*pan_d].  If [val] is nonzero,
//    writes the decoded atom value (TRANSFER) to [*val]; skips (+hop, +dive)
//    pass 0 and never materialize the payload.  Bails on unary overrun,
//    matching +rub's `?<  (gth c m)`.
//
static void
_sip_rub(c3_d off_d, u3_atom a, c3_d met_d, c3_d* pan_d, u3_noun* val)
{
  //  c: length of the unary length-of-length prefix (count of leading zeros)
  //
  c3_d c_d = 0;
  while ( 1 ) {
    if ( c_d > met_d ) {
      u3m_bail(c3__exit);
    }
    if ( 0 != _sip_bit(off_d + c_d, a, met_d) ) {
      break;
    }
    c_d++;
  }

  //  c == 0: the value is 0, encoded in a single bit
  //
  if ( 0 == c_d ) {
    *pan_d = 1;
    if ( val ) {
      *val = 0;
    }
    return;
  }

  //  addressability limit: a real +mat never has more than ~32 leading zeros
  //  (that would be a value of >2^31 bits); refuse to overflow the 64-bit
  //  cursor on a hostile run of zeros.
  //
  if ( c_d > 63 ) {
    u3m_bail(c3__exit);
  }

  //  e: the decoded bit-width of the value.  The top bit is implicit (bex),
  //  the low (c-1) bits follow the unary terminator at [off + c + 1].
  //
  c3_d dof_d = off_d + c_d + 1;
  c3_d wid_d = c_d - 1;
  c3_d e_d   = (c3_d)1 << wid_d;
  {
    c3_d i_d;
    for ( i_d = 0; i_d < wid_d; i_d++ ) {
      if ( 0 != _sip_bit(dof_d + i_d, a, met_d) ) {
        e_d |= ((c3_d)1 << i_d);
      }
    }
  }

  //  span = 2c + e; the value occupies [off + 2c, off + 2c + e)
  //
  *pan_d = (c_d + c_d) + e_d;

  if ( val ) {
    c3_d vof_d = off_d + c_d + c_d;

    //  clamp the read to the buffer: bits above met are zero, so the atom is
    //  identical, and we avoid allocating e (which may be hostilely large)
    //
    c3_d act_d;
    if ( vof_d >= met_d ) {
      act_d = 0;
    }
    else {
      c3_d ava_d = met_d - vof_d;
      act_d = ( e_d < ava_d ) ? e_d : ava_d;
    }

    if ( 0 == act_d ) {
      *val = 0;
    }
    else {
      u3_atom vof = u3i_chubs(1, &vof_d);
      u3_atom act = u3i_chubs(1, &act_d);
      *val = u3qc_cut(0, vof, act, a);
      u3z(vof);
      u3z(act);
    }
  }
}

//  _sip_kind_cell(): is the node at [off] a cell (tag 10)?  RETAIN [a]
//
static inline c3_o
_sip_kind_cell(c3_d off_d, u3_atom a, c3_d met_d)
{
  return __( (0 != _sip_bit(off_d, a, met_d)) &&
             (0 == _sip_bit(off_d + 1, a, met_d)) );
}

//  _sip_skip(): bit-offset past the subtree at [off], as +hop.  RETAIN [a]
//
//    Explicit pending-node counter -- O(1) space, no C stack.  A cell consumes
//    one pending node and adds two (head, tail); an atom or backref consumes
//    one, its span read (never followed) by a single +rub.
//
static c3_d
_sip_skip(c3_d off_d, u3_atom a, c3_d met_d)
{
  c3_d pen_d = 1;
  while ( pen_d > 0 ) {
    if ( off_d >= met_d ) {
      u3m_bail(c3__exit);
    }
    if ( 0 == _sip_bit(off_d, a, met_d) ) {         //  atom
      c3_d pan_d;
      _sip_rub(off_d + 1, a, met_d, &pan_d, 0);
      off_d += 1 + pan_d;
      pen_d -= 1;
    }
    else if ( 0 != _sip_bit(off_d + 1, a, met_d) ) {  //  backref
      c3_d pan_d;
      _sip_rub(off_d + 2, a, met_d, &pan_d, 0);
      off_d += 2 + pan_d;
      pen_d -= 1;
    }
    else {                                            //  cell
      off_d += 2;
      pen_d += 1;
    }
  }
  return off_d;
}

//  _sip_gaze(): resolve a backref chain, as +gaze.  RETAIN [a]
//
//    Terminates by construction: each hop asserts a strictly decreasing
//    offset, so no fuel counter is needed.
//
static c3_d
_sip_gaze(c3_d off_d, u3_atom a, c3_d met_d)
{
  while ( 1 ) {
    if ( off_d >= met_d ) {
      u3m_bail(c3__exit);
    }
    if ( 0 == _sip_bit(off_d, a, met_d) ) {           //  atom
      return off_d;
    }
    if ( 0 == _sip_bit(off_d + 1, a, met_d) ) {       //  cell
      return off_d;
    }

    //  backref: follow to a strictly earlier offset, or crash
    //
    {
      c3_d    pan_d;
      u3_noun tgt;
      _sip_rub(off_d + 2, a, met_d, &pan_d, &tgt);

      //  a valid target is < off < met, hence fits a 64-bit cursor
      //
      if ( c3n == u3a_is_cat(tgt) && (u3r_met(0, tgt) > 63) ) {
        u3z(tgt);
        return u3m_bail(c3__exit);
      }
      {
        c3_d nof_d = u3r_chub(0, tgt);
        u3z(tgt);
        if ( nof_d >= off_d ) {
          return u3m_bail(c3__exit);
        }
        off_d = nof_d;
      }
    }
  }
}

//  _sip_head(): bit-offset of the head of the cell at [off].  RETAIN [a]
//
static inline c3_d
_sip_head(c3_d off_d, u3_atom a, c3_d met_d)
{
  off_d = _sip_gaze(off_d, a, met_d);
  if ( c3n == _sip_kind_cell(off_d, a, met_d) ) {
    u3m_bail(c3__exit);
  }
  return off_d + 2;
}

//  _sip_tail(): bit-offset of the tail of the cell at [off].  RETAIN [a]
//
//    The tail begins where the head's subtree ends.
//
static inline c3_d
_sip_tail(c3_d off_d, u3_atom a, c3_d met_d)
{
  off_d = _sip_gaze(off_d, a, met_d);
  if ( c3n == _sip_kind_cell(off_d, a, met_d) ) {
    u3m_bail(c3__exit);
  }
  return _sip_skip(off_d + 2, a, met_d);
}

//  _sip_to_cursor(): a slot-offset atom as a 64-bit cursor; bail if too wide
//
static c3_d
_sip_to_cursor(u3_atom a)
{
  if ( (c3n == u3a_is_cat(a)) && (u3r_met(0, a) > 63) ) {
    u3m_bail(c3__exit);
  }
  return u3r_chub(0, a);
}

//  u3qi_sip_grab(): atom at axis [b] under cursor [off] in jammed buffer [a].
//
//    RETAIN [a], [off], [b].  Mirrors +grab = (fetch (dive [a off] b)).
//
u3_noun
u3qi_sip_grab(u3_atom a, u3_atom off, u3_atom b)
{
  c3_d met_d = u3r_met(0, a);
  c3_d off_d = _sip_to_cursor(off);

  //  axis 0 is out of range
  //
  if ( c3y == u3r_sing(0, b) ) {
    return u3m_bail(c3__exit);
  }

  //  +dive: walk the axis bits below the leading 1, MSB-first; 0 = head, 1 =
  //  tail.  This is the +cap/+mas peel without allocating.
  //
  {
    c3_w bit_w = u3r_met(0, b);
    c3_w j_w   = bit_w - 1;
    while ( j_w-- > 0 ) {
      if ( 0 == u3r_bit(j_w, b) ) {
        off_d = _sip_head(off_d, a, met_d);
      }
      else {
        off_d = _sip_tail(off_d, a, met_d);
      }
    }
  }

  //  +fetch: resolve refs, require an atom, decode it
  //
  off_d = _sip_gaze(off_d, a, met_d);
  if ( 0 != _sip_bit(off_d, a, met_d) ) {
    return u3m_bail(c3__exit);
  }
  {
    c3_d    pan_d;
    u3_noun val;
    _sip_rub(off_d + 1, a, met_d, &pan_d, &val);
    return val;
  }
}

//  u3wi_sip_grab(): wrapper.  Sample is [[a=@ off=@] b=@]; a at axis 24,
//  off at 25, b at 13.
//
u3_noun
u3wi_sip_grab(u3_noun cor)
{
  u3_noun a, off, b;

  if ( (c3n == u3r_mean(cor, 24, &a, 25, &off, 13, &b, 0)) ||
       (c3n == u3ud(a)) ||
       (c3n == u3ud(off)) ||
       (c3n == u3ud(b)) )
  {
    return u3m_bail(c3__fail);
  }

  return u3qi_sip_grab(a, off, b);
}
