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

/// Dual-bitness struct vocabulary.
///
/// `pkg/noun/{allocate,hashtable,vortex}.h` declare paired structs ending
/// in `_h` (32-bit) and `_d` (64-bit), with a native typedef alias chosen
/// at compile time.  These macros let each pair be written once: a body
/// macro takes a suffix token (`h` or `d`) and uses `U3_W(S)` / `U3_N(S)`
/// / `U3_WS(S)` where the field type varies with the address width, and
/// `U3_PASTE(NAME, S)` to refer to other dual-bitness types.  Fields that
/// are fixed width in both bitnesses (e.g. `c3_d eve_d`) stay bare.
/// Token-paste two names with an underscore between them.  `U3_PASTE_`
/// joins its arguments literally (use when both sides are intended as
/// raw name tokens, even if one of them happens to also be defined as
/// a macro — e.g. `u3a_crag_no`).  `U3_PASTE` first macro-expands its
/// arguments (use when one side is a macro like `U3_NATIVE_SFX` that
/// must resolve to `h`/`d` before the paste).
#define U3_PASTE_(a, b)  a##_##b
#define U3_PASTE(a, b)   U3_PASTE_(a, b)

#ifdef VERE64
#  define U3_NATIVE_SFX  d
#else
#  define U3_NATIVE_SFX  h
#endif

#define U3_NATIVE(name)  U3_PASTE(name, U3_NATIVE_SFX)

#define U3_W_h           c3_h
#define U3_W_d           c3_d
#define U3_W(S)          U3_PASTE(U3_W, S)

#define U3_N_h           u3_noun_h
#define U3_N_d           u3_noun_d
#define U3_N(S)          U3_PASTE(U3_N, S)

#define U3_WS_h          c3_hs
#define U3_WS_d          c3_ds
#define U3_WS(S)         U3_PASTE(U3_WS, S)

#define U3_DEFINE_PAIR(NAME, BODY)                              \
  typedef struct _##NAME##_h { BODY(h) } NAME##_h;              \
  typedef struct _##NAME##_d { BODY(d) } NAME##_d;              \
  typedef U3_NATIVE(NAME) NAME

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
