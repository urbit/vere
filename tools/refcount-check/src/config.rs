//! Tables of functions and types with hard-wired meaning for the checker.

use std::sync::atomic::{AtomicBool, Ordering};

/// Loom mode: -DVERE64 in the compile commands makes the noun word
/// 64-bit -- direct atoms are 63-bit and u3_none is 2^64-1. Set once at
/// startup from the compile db (which must be uniform), read by the
/// width-dependent accessors below.
static VERE64: AtomicBool = AtomicBool::new(false);

pub fn set_vere64(on: bool) {
  VERE64.store(on, Ordering::Relaxed);
}

pub fn vere64() -> bool {
  VERE64.load(Ordering::Relaxed)
}

/// Typedefs that denote reference-counted nouns.
pub const NOUN_TYPES: &[&str] = &[
  "u3_noun", "u3_atom", "u3_cell", "u3_weak", "u3_term",
  "u3_trel", "u3_qual", "u3_quin",
];

/// Typedefs too narrow to hold an indirect noun reference in either
/// loom mode: a noun value bound to a variable of one of these is
/// necessarily a direct atom. Signed variants are excluded (sign
/// extension of a negative value could produce an indirect bit
/// pattern), and so is c3_l: it is the noun width by design (31/63-bit
/// "little", types.h), its narrowness a convention the type does not
/// enforce.
pub const DIRECT_TYPES: &[&str] = &[
  "c3_b", "c3_y", "c3_s", "c3_t", "c3_o", "c3_g",
];

/// 32-bit typedefs (c3_m is "also c3_l" only in 32-bit mode): narrower
/// than the noun word under VERE64 alone, where truncation cannot
/// preserve an indirect reference's high bit.
pub const DIRECT_TYPES_64: &[&str] = &["c3_h", "c3_m"];

pub fn is_direct_type_name(s: &str) -> bool {
  DIRECT_TYPES.contains(&s) || (vere64() && DIRECT_TYPES_64.contains(&s))
}

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

/// What the consumer half of an array-building macro does to the
/// elements of the array. `u3r_mean(a, {axe, &out}, ..)`, `u3x_mean`,
/// `u3i_list(x, ..)` (`u3nl`), `u3m_grab(x, ..)` and `u3i_molt(a,
/// {axe, som}, ..)` expand to a statement expression that declares a
/// local array and hands it to one of the functions below; the
/// interpreter records the array at its declaration and applies the
/// consumer's protocol at the call.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ArrayProtocol {
  /// a `u3r_mean_pair` array: each `&out` is filled with an uncounted
  /// view into the source noun -- only when the call returns c3y if
  /// `loob` (u3r_vmean), unconditionally when it bails instead
  /// (u3x_vmean)
  MeanFill {loob: bool},
  /// every noun element is consumed (u3i_vlist, the u3i_vmolt pairs)
  Transfer,
  /// every noun element stays with the caller (u3m_vgrab: GC roots)
  Retain,
}

/// The consumer's protocol and the position of its array argument.
pub fn array_consumer(name: &str) -> Option<(ArrayProtocol, usize)> {
  Some(match name {
    "u3r_vmean" => (ArrayProtocol::MeanFill {loob: true}, 1),
    "u3x_vmean" => (ArrayProtocol::MeanFill {loob: false}, 1),
    "u3i_vlist" => (ArrayProtocol::Transfer, 0),
    "u3i_vmolt" => (ArrayProtocol::Transfer, 1),
    "u3m_vgrab" => (ArrayProtocol::Retain, 0),
    _ => return None,
  })
}

/// Retain-product functions whose product borrows from an UNTRACKED
/// container (a u3p hashtable), not from their noun arguments: the
/// product must not be tied under the argument values (u3h_git's
/// product is the stored value; freeing the lookup key does not
/// invalidate it).
pub const UNTIED_RETAIN_FNS: &[&str] = &["u3h_git"];

/// Functions that may run unifying equality (u3r_sing) over their noun
/// arguments during execution: equal interior copies can be freed and
/// repointed, so borrowed views into the arguments die at the call.
/// Nock evaluation entry points are here because evaluated code can
/// compare the subject -- or anything reachable from it -- at will.
pub const UNIFYING_FNS: &[&str] = &[
  //  unifying equality itself
  "u3r_sing", "u3r_sing_imp", "u3r_sing_c", "u3r_sing_cell",
  "u3r_sing_mixt", "u3r_sing_trel", "u3r_sing_qual",
  //  nock evaluation entry points
  "u3n_nock_on", "u3n_nock_in", "u3n_nock_it", "u3n_nock_an",
  "u3n_nock_et", "u3n_slam_on", "u3n_slam_in", "u3n_slam_it",
  "u3n_slam_et", "u3n_kick_on",
  //  jet dispatch: evaluates the core (prep looks the battery up in
  //  the warm state, comparing it against registered batteries)
  "u3j_kick", "u3j_gate_slam", "u3j_gate_prep", "u3j_soft", "u3j_cook",
  //  virtualization wrappers around nock evaluation
  "u3m_soft", "u3m_soft_slam", "u3m_soft_nock", "u3m_soft_run",
  "u3m_soft_sure",
  //  vortex conveniences (u3do/u3dc/u3dt expand to u3v_do)
  "u3v_do", "u3v_wish", "u3v_wish_n",
  //  memo cache: lookups compare the key against stored keys
  "u3z_find", "u3z_find_m", "u3z_find_up", "u3z_save", "u3z_save_m",
  "u3z_uniq",
  //  hashtable: key equality on lookup/insert/delete/union
  "u3h_get", "u3h_git", "u3h_put", "u3h_put_get", "u3h_del", "u3h_uni",
];

pub const C3Y: u64 = 0;
pub const C3N: u64 = 1;

/// Largest direct atom: 2^31-1 (32-bit loom) or 2^63-1 (VERE64).
pub fn direct_max() -> u64 {
  if vere64() { 0x7fff_ffff_ffff_ffff } else { 0x7fff_ffff }
}

/// The u3_none sentinel: (u3_noun)c3_w_max, all-ones at the noun word
/// width.
pub fn u3_none() -> u64 {
  if vere64() { 0xffff_ffff_ffff_ffff } else { 0xffff_ffff }
}

/// Wraparound mask for integer-constant evaluation: integer literals
/// in noun context truncate to the noun word width.
pub fn noun_mask() -> u64 {
  if vere64() { u64::MAX } else { 0xffff_ffff }
}

/// --strict-weak: also require a proven-valid noun for u3z/u3a_lose.
/// Off by default: u3z of u3_none is a de-facto safe no-op
/// (u3a_north/south_is_normal return c3n for it), unlike u3k, which
/// asserts.
pub static STRICT_WEAK: std::sync::atomic::AtomicBool =
  std::sync::atomic::AtomicBool::new(false);

pub fn strict_weak() -> bool {
  STRICT_WEAK.load(std::sync::atomic::Ordering::Relaxed)
}
