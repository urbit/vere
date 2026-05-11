/// @file
///
/// Various noun types.

#ifndef U3_TYPES_H
#define U3_TYPES_H

#include "c3/c3.h"

/// Sentinel value for u3_noun types that aren't actually nouns.
#define u3_none (u3_noun)c3_w_max

/// 0, or `~` in Hoon.
#define u3_nul (c3_w)0

/// 0, or `%$` in Hoon.
#define u3_blip (c3_w)0

/// Pointer offset into the loom.
///
/// Declare variables of this type using u3p() to annotate the type of the
/// pointee. Ensure that variable names of this type end in `_p`.
typedef c3_w      u3_post;
#define u3p(type) u3_post

/// Tagged noun pointer.  Bit numbers are for 32-bit (64-bit) mode.
///
/// If bit 31 (63) is 0, the noun is a direct atom (also called a "cat").
/// If bit 31 (63) is 1 and bit 30 (62) is 0, it's an indirect atom ("pug").
/// If bit 31 (63) is 1 and bit 30 (62) is 1, it's an indirect cell ("pom").
///
/// Bits 0-29 (0-61) are a word offset (i.e. u3_post) against the loom.
typedef c3_w      u3_noun;
typedef c3_h      u3_noun_h;
typedef c3_d      u3_noun_d;

/// Optional noun type.
///
/// u3_weak is either a valid noun or u3_none.
typedef u3_noun u3_weak;

/// Atom.
typedef u3_noun u3_atom;

/// Term (Hoon aura @tas).
typedef u3_noun u3_term;

/// Cell of the form `[a b]`.
typedef u3_noun u3_cell;

/// Cell of the form `[a b c]`.
typedef u3_noun u3_trel;

/// Cell of the form `[a b c d]`.
typedef u3_noun u3_qual;

/// Cell of the form `[a b c d e]`.
typedef u3_noun u3_quin;

/// Unary noun function.
typedef u3_noun (*u3_funk)(u3_noun);

/// Binary noun function.
typedef u3_noun (*u3_funq)(u3_noun, u3_noun);

#endif /* ifndef U3_TYPES_H */
