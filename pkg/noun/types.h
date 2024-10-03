/// @file
///
/// Various noun types.

#ifndef U3_TYPES_H
#define U3_TYPES_H

#include "c3.h"

/// Sentinel value for u3_noun types that aren't actually nouns.
#define u3_none (u3_noun)0xffffffff

/// 0, or `~` in Hoon.
#define u3_nul 0

/// 0, or `%$` in Hoon.
#define u3_blip 0

/// Pointer offset into the loom.
///
/// Declare variables of this type using u3p() to annotate the type of the
/// pointee. Ensure that variable names of this type end in `_p`.
typedef c3_w      u3_post;
#define u3p(type) u3_post

/// Tagged noun pointer.
///
/// If bit 31 is 0, the noun is a direct 31-bit atom (also called a "cat").
/// If bit 31 is 1 and bit 30 is 0, an indirect atom (also called a "pug").
/// If bit 31 is 1 and bit 30 is 1, an indirect cell (also called a "pom").
///
/// Bits 0-29 are a word offset (i.e. u3_post) against the loom.
typedef c3_w u3_noun;

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
