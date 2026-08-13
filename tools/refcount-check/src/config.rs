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
  "c3_b", "c3_y", "c3_s", "c3_t", "c3_o", "c3_g",
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

/// Destructurers returning a loobean (u3r_*): the out-params are
/// filled only when the product is c3y. The u3x_* variants bail
/// instead of returning c3n, so their fills are unconditional.
pub fn destructurer_loobean(name: &str) -> bool {
  name.starts_with("u3r_")
}

/// Hard-wired destructurers: varargs, so the input args cant be named. So these
/// are special cased
pub fn destructurer_src(name: &str) -> Option<usize> {
  match name {
    "u3x_mean" | "u3r_mean" => Some(0),
    _ => None,
  }
}

/// Retain-product functions whose product borrows from an UNTRACKED
/// container (a u3p hashtable), not from their noun arguments: the
/// product must not be tied under the argument values (u3h_git's
/// product is the stored value; freeing the lookup key does not
/// invalidate it).
pub const UNTIED_RETAIN_FNS: &[&str] = &["u3h_git"];

/// Variadic noun-core functions that consume every noun vararg
/// (`u3_none`-terminated lists). Varargs of any other function stay
/// unaccounted ("too ambiguous").
pub const VARARG_TRANSFER_FNS: &[&str] = &["u3i_list"];

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
pub const DIRECT_MAX: u64 = 0x7fff_ffff;
pub const U3_NONE: u64 = 0xffff_ffff;

/// --strict-weak: also require a proven-valid noun for u3z/u3a_lose.
/// Off by default: u3z of u3_none is a de-facto safe no-op
/// (u3a_north/south_is_normal return c3n for it), unlike u3k, which
/// asserts.
pub static STRICT_WEAK: std::sync::atomic::AtomicBool =
  std::sync::atomic::AtomicBool::new(false);

pub fn strict_weak() -> bool {
  STRICT_WEAK.load(std::sync::atomic::Ordering::Relaxed)
}
