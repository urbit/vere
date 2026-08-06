//! Tables of functions and types with hard-wired meaning for the checker.

/// Typedefs that denote reference-counted nouns.
pub const NOUN_TYPES: &[&str] = &[
  "u3_noun", "u3_atom", "u3_cell", "u3_weak", "u3_term",
  "u3_trel", "u3_qual", "u3_quin",
];

/// Typedefs too narrow to hold an indirect noun reference: a noun
/// value bound to a variable of one of these is necessarily a direct
/// atom. c3_l is 31-bit by convention (types.h), c3_m is "also c3_l";
/// signed variants are excluded (sign extension of a negative value
/// could produce an indirect bit pattern).
pub const DIRECT_TYPES: &[&str] = &[
  "c3_b", "c3_y", "c3_s", "c3_t", "c3_o", "c3_g", "c3_l", "c3_m",
];

/// Calls that never return; execution ends at the call site.
pub const NORETURN_FNS: &[&str] = &[
  "u3m_bail", "u3m_signal", "abort", "exit", "_exit",
  "longjmp", "siglongjmp", "__assert_fail",
];

/// u3a_is_* predicates and what they test.
pub fn guard_kind(name: &str) -> Option<&'static str> {
  Some(match name {
    "u3a_is_cat" => "cat",
    "u3a_is_dog" => "dog",
    "u3a_is_pug" => "pug",
    "u3a_is_pom" => "pom",
    "u3a_is_atom" => "atom",
    "u3a_is_cell" => "cell",
    _ => return None,
  })
}

/// Destructurers: source argument index; other `&var` args become
/// retained out-params borrowed from the source.
pub fn destructurer_src(name: &str) -> Option<usize> {
  match name {
    "u3x_cell" | "u3x_trel" | "u3x_qual" | "u3x_quil"
    | "u3r_cell" | "u3r_trel" | "u3r_qual" | "u3r_quil"
    | "u3x_mean" | "u3r_mean" | "u3r_bite" => Some(0),
    _ => None,
  }
}

pub const C3Y: u64 = 0;
pub const C3N: u64 = 1;
pub const DIRECT_MAX: u64 = 0x7fff_ffff;
pub const U3_NONE: u64 = 0xffff_ffff;
