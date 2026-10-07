//! Refcount protocol semantics: the `Sem` table, the `@Refcount:`
//! annotation grammar, prefix/position defaults, comment harvesting,
//! declaration/definition sync checking, and block-level asserts.
//!
//! This module (plus `ast`) is the entire vocabulary the abstract
//! interpreter needs; the interpreter itself lives in `interp1`.

use std::collections::{BTreeMap, HashMap};
use std::rc::Rc;
use std::sync::OnceLock;

use regex::Regex;

use crate::ast::{is_noun_type, Cursor};

/// Refcount mode of one argument.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ArgumentMode {
  Transfer,
  Retain,
  Direct,      // the callee bails unless the argument is a direct atom
  Passthrough, // the argument's value IS the product (see ProductMode)
  Conslike,    // consumed by being stored inside the product (u3nc-style)
}

impl ArgumentMode {
  pub fn as_str(self) -> &'static str {
    match self {
      ArgumentMode::Transfer => "transfer",
      ArgumentMode::Retain => "retain",
      ArgumentMode::Direct => "direct",
      ArgumentMode::Passthrough => "passthrough",
      ArgumentMode::Conslike => "conslike",
    }
  }
}

/// What the callee does with the noun behind a pointer-to-noun
/// parameter. The clauses compose: an in-place accumulator update is
/// `@Refcount: consumes `out`, fills transferred `out``.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub struct PointeeMode {
  pub reads: bool,             // reads *a without consuming it
  pub consumes: bool,          // gives away one counted ref of the old *a
  pub fills: Option<FillMode>, // writes a new value into *a
  //  `fills ... `a` on `c3y``: the fill happens exactly when the
  //  function returns this loobean (true = c3y); None = unconditional
  pub fill_on: Option<bool>,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum FillMode {
  Transferred, // the caller owns the new pointee and must consume it
  Retained,    // the new pointee is an uncounted view
}

/// Refcount mode of the product.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum ProductMode {
  Transfer,
  Retain,
  Direct,                    // the product is always a direct atom
  Passthrough, 
  NonNoun,                   // the function does not return a noun
}

impl ProductMode {
  pub fn as_str(&self) -> &'static str {
    match self {
      ProductMode::Transfer => "transfer",
      ProductMode::Retain => "retain",
      ProductMode::Direct => "direct",
      ProductMode::Passthrough { .. } => "passthrough",
      ProductMode::NonNoun => "non-noun",
    }
  }
}

/// One reported problem (or annotation warning).
#[derive(Clone, Debug)]
pub struct Finding {
  pub file: String,
  pub line: u32,
  pub col: u32,
  pub func: String,
  pub cat: &'static str,
  pub msg: String,
}

/// Block-level `{ // @Refcount: assert ... }` annotation modes. Only
/// `transfer` is part of the protocol; `produce` and `retain` still parse
/// so the interpreter can reject them loudly instead of skipping them.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum AssertMode {
  Transfer,
  Direct,
  Unknown,
}

/// The refcount protocol of one function.
#[derive(Clone, Debug)]
pub struct Sem {
  pub default_args: ArgumentMode,
  pub args: BTreeMap<String, ArgumentMode>, // param name -> mode
  pub pointees: BTreeMap<String, PointeeMode>, // pointer param -> pointee mode
  pub product: ProductMode,
  pub noreturn: bool,               // calling this ends execution
  //  `doomed on `c3n``: exits returning this loobean oblige the CALLER
  //  to die; leaks and contracts on those paths are not checked
  pub doomed: Option<bool>,
  pub check: bool,
  pub custom: bool,
  pub why: String,
  pub from_def: bool,               // resolved with the definition visible
  pub warnings: Vec<(u32, String)>, // annotation conflicts, (line, message)
}

impl Sem {
  pub fn new(default_args: ArgumentMode, product: ProductMode, why: &str)
    -> Sem
  {
    Sem {
      default_args,
      args: BTreeMap::new(),
      pointees: BTreeMap::new(),
      product,
      noreturn: false,
      doomed: None,
      check: true,
      custom: false,
      why: why.to_string(),
      from_def: false,
      warnings: Vec::new(),
    }
  }

  pub fn arg_mode(&self, name: &str) -> ArgumentMode {
    self.args.get(name).copied().unwrap_or(self.default_args)
  }

  /// The observable protocol, ignoring bookkeeping fields (for the
  /// decl-vs-def sync check).
  pub fn proto_key(&self) -> String {
    format!(
      "{:?}|{:?}|{:?}|{:?}|{}|{}|{}|{:?}",
      self.default_args, self.args, self.pointees, self.product, self.custom,
      self.check, self.noreturn, self.doomed
    )
  }
}

// ---------------------------------------------------------------------------
// regexes

fn re_refcount() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| Regex::new(r"(?i)@Refcount:[ \t]*([^\n\r]*)").unwrap())
}

fn re_argname() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| Regex::new(r"`(\w+)`").unwrap())
}

fn re_trail_comment() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| Regex::new(r"\*/\s*$").unwrap())
}

//  prose remarks inside a clause line: `-- ...` to end of line, and
//  parenthesized asides. Neither appears in the clause grammar, so
//  both are stripped uniformly before parsing (`noreturn (bail_f
//  never returns)`, `assert transfer -- the kernel owns [roc]`).
fn re_dash_remark() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| Regex::new(r"--.*").unwrap())
}

fn re_paren_remark() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| Regex::new(r"\([^()]*\)").unwrap())
}

// the file-level annotation: every function in the file is custom unless
// its own annotation asserts a protocol. The phrase must fit on one line
// (re_refcount captures a single line).
const FILE_CUSTOM_PHRASE: &str =
  r"all\s+functions\s+are\s+custom\s+unless\s+asserted\s+otherwise\b";

pub fn re_file_custom() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| {
    Regex::new(&format!(r"(?i)@Refcount:\s*{}", FILE_CUSTOM_PHRASE)).unwrap()
  })
}

// same phrase as a lone clause, for a comment that clang attaches to a
// function declaration instead of the file
fn re_file_custom_clause() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| {
    Regex::new(&format!(r"(?i)^{}", FILE_CUSTOM_PHRASE)).unwrap()
  })
}

fn re_jet_dir() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  //  a-f the classic tiers, g/i the newer ones, 135+ versioned trees
  RE.get_or_init(|| Regex::new(r"/jets/[a-z0-9]+/").unwrap())
}

fn re_jet_qw() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| Regex::new(r"^u3[qw][a-z]+_").unwrap())
}

fn re_jet_k() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| Regex::new(r"^u3k[a-z]+_").unwrap())
}

fn re_block_assert() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| {
    Regex::new(r"(?i)@Refcount:\s*assert\s+(transfer|produce|retain|direct)((?:\s+[A-Za-z_]\w*)*)")
      .unwrap()
  })
}

fn re_any_comment() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| Regex::new(r"(//[^\n]*|/\*(?s:.)*?\*/)").unwrap())
}

// words naming the arguments-slot and the product-slot in a clause
const ARG_SLOT_WORDS: &[&str] = &["argument", "arguments", "arg", "args"];
const PROD_SLOT_WORDS: &[&str] = &["product", "result", "return"];

// keywords that begin a clause; a comma-separated fragment that does NOT
// start with one of these continues the previous clause
const CLAUSE_HEADS: &[&str] = &[
  "transfers", "retains", "transfer", "retain", "direct", "passthrough",
  "conslike", "reads", "consumes", "fills",
  "custom", "assert", "noreturn", "doomed",
];

/// `c3y`/`c3n` as a condition name.
fn loob_name(s: &str) -> Option<bool> {
  match s {
    "c3y" => Some(true),
    "c3n" => Some(false),
    _ => None,
  }
}

// ---------------------------------------------------------------------------
// annotation grammar

/// Split a clause on commas into separate clauses, unless a fragment is a
/// bare continuation (e.g. the `y`, `z` in "transfers `x`, `y`, `z`").
fn split_commas(text: &str) -> Vec<String> {
  let mut out: Vec<String> = Vec::new();
  for frag in text.split(',') {
    let frag = frag.trim();
    if frag.is_empty() {
      continue;
    }
    let head = frag
      .split_whitespace()
      .next()
      .unwrap_or("")
      .to_lowercase();
    if CLAUSE_HEADS.contains(&head.as_str()) || out.is_empty() {
      out.push(frag.to_string());
    } else {
      let last = out.last_mut().unwrap();
      *last = format!("{}, {}", last, frag);
    }
  }
  out
}

/// The text of each @Refcount: clause in a comment, in order. Prose
/// remarks -- `-- ...` trailers and parenthesized asides -- are
/// stripped first, so any clause may carry one.
pub fn refcount_clauses(comment: &str) -> Vec<String> {
  let mut out = Vec::new();
  for m in re_refcount().captures_iter(comment) {
    let c = re_trail_comment().replace(&m[1], "");
    let c = re_dash_remark().replace(&c, "");
    let mut c = c.into_owned();
    loop {
      //  innermost-out, so nested parens in a remark strip cleanly
      let next = re_paren_remark().replace_all(&c, " ").into_owned();
      if next == c {
        break;
      }
      c = next;
    }
    let c = c.trim().trim_end_matches('*').trim();
    out.extend(split_commas(c));
  }
  out
}

/// Python-repr-style quoting for the "unrecognized clause" warning.
fn py_repr(s: &str) -> String {
  let esc = s.replace('\\', "\\\\").replace('\'', "\\'");
  format!("'{}'", esc)
}

/// Mutate `sem` according to the @Refcount: clauses in a comment. Each
/// clause updates the protocol (last write wins); a clause that changes a
/// slot an earlier clause already set is recorded in sem.warnings.
/// Returns true if any clause was recognized.
pub fn parse_fn_annotations(comment: &str, sem: &mut Sem, line: u32) -> bool {
  let clauses = refcount_clauses(comment);
  if clauses.is_empty() {
    return false;
  }
  //  slot name -> mode description; all modes share one namespace, so a
  //  later clause of a different kind (`transfers `x`` then `direct `x``)
  //  still conflicts
  let mut explicit: HashMap<String, String> = HashMap::new();

  // records the write and warns on conflict; the caller then applies it
  fn set_slot(
    explicit: &mut HashMap<String, String>,
    warnings: &mut Vec<(u32, String)>,
    line: u32,
    slot: &str,
    mode: &str,
  ) {
    if let Some(prev) = explicit.get(slot) {
      if prev.as_str() != mode {
        warnings.push((
          line,
          format!(
            "conflicting @Refcount: annotations set {} to {} then {}",
            slot, prev, mode
          ),
        ));
      }
    }
    explicit.insert(slot.to_string(), mode.to_string());
  }

  let mut saw_custom = false;
  let mut saw_other = false;
  for clause in &clauses {
    let lowered = clause.to_lowercase();
    let mut toks: Vec<&str> = lowered.split_whitespace().collect();
    if toks.is_empty() {
      continue;
    }
    let mut head = toks[0];

    if re_file_custom_clause().is_match(clause) {
      saw_custom = true;
      sem.custom = true;
      sem.check = false;
      sem.why =
        "@Refcount: all functions are custom unless asserted otherwise"
          .to_string();
      continue;
    }

    if head == "custom" {
      saw_custom = true;
      sem.custom = true;
      sem.check = false;
      sem.why = "@Refcount: custom".to_string();
      continue;
    }

    saw_other = true;
    if head == "assert" {
      // trust the annotated protocol; do not check the body
      sem.check = false;
      sem.why = "@Refcount: assert".to_string();
      toks.remove(0);
      if toks.is_empty() {
        continue;
      }
      head = toks[0];
    }

    let names: Vec<String> = re_argname()
      .captures_iter(clause)
      .map(|c| c[1].to_string())
      .collect();
    let words: Vec<&str> = toks[1..].to_vec();
    let hit_prod = words.iter().any(|w| PROD_SLOT_WORDS.contains(w));
    let hit_args = words.iter().any(|w| ARG_SLOT_WORDS.contains(w));

    if head == "noreturn" {
      //  calling this function ends execution: no argument accounting
      //  applies at the call site (dying with sloppy counts is fine)
      if !toks[1..].is_empty() {
        sem.warnings.push((
          line,
          "@Refcount: noreturn takes no arguments".to_string(),
        ));
      }
      set_slot(&mut explicit, &mut sem.warnings, line, "noreturn",
        "noreturn");
      sem.noreturn = true;
      continue;
    }

    if head == "passthrough" {
      let Some(name) = names.first() else {
        sem.warnings.push((
          line,
          "@Refcount: passthrough requires an argument name".to_string(),
        ));
        continue;
      };
      if names.len() > 1 {
        sem.warnings.push((
          line,
          "@Refcount: passthrough takes exactly one argument name"
            .to_string(),
        ));
      }
      //  the product can be only one argument: a later passthrough
      //  replaces an earlier one
      if let Some(prev) = sem
        .args
        .iter()
        .find(|(k, m)| **m == ArgumentMode::Passthrough && *k != name)
        .map(|(k, _)| k.clone())
      {
        sem.warnings.push((
          line,
          format!(
            "only one @Refcount: passthrough argument is allowed: `{}` \
             replaces `{}`",
            name, prev
          ),
        ));
        sem.args.remove(&prev);
      }
      let slot = format!("argument `{}`", name);
      set_slot(&mut explicit, &mut sem.warnings, line, &slot, "passthrough");
      set_slot(&mut explicit, &mut sem.warnings, line, "product",
        "passthrough");
      sem.args.insert(name.clone(), ArgumentMode::Passthrough);
      sem.product = ProductMode::Passthrough;
      sem.why = "@Refcount: passthrough".to_string();
      continue;
    }

    if head == "direct" {
      if !names.is_empty() {
        for n in &names {
          let slot = format!("argument `{}`", n);
          set_slot(&mut explicit, &mut sem.warnings, line, &slot, "direct");
          sem.args.insert(n.clone(), ArgumentMode::Direct);
        }
      } else if hit_prod && !hit_args {
        set_slot(&mut explicit, &mut sem.warnings, line, "product", "direct");
        sem.product = ProductMode::Direct;
      } else {
        // bare: every argument is proven direct
        set_slot(&mut explicit, &mut sem.warnings, line, "arguments", "direct");
        sem.default_args = ArgumentMode::Direct;
      }
      sem.why = "@Refcount: direct".to_string();
      continue;
    }

    if head == "reads" || head == "consumes" {
      //  pointee protocol of a pointer-to-noun parameter: the callee
      //  reads *a (without consuming), or gives away one counted
      //  reference of the old *a. Meaningless as a default or on the
      //  product, so names are required.
      if names.is_empty() {
        sem.warnings.push((
          line,
          format!("@Refcount: {} requires argument names", head),
        ));
        continue;
      }
      for n in &names {
        let slot = format!("pointee `{}` {}", n, head);
        set_slot(&mut explicit, &mut sem.warnings, line, &slot, head);
        let pm = sem.pointees.entry(n.clone()).or_default();
        if head == "reads" {
          pm.reads = true;
        } else {
          pm.consumes = true;
        }
      }
      sem.why = format!("@Refcount: {}", head);
      continue;
    }

    if head == "fills" {
      //  the callee writes a new value into *a: `fills transferred`
      //  hands the caller a counted reference, `fills retained` an
      //  uncounted view. A trailing `on `c3y|c3n`` makes the fill
      //  conditional on the function's (loobean) product.
      let (fm, mname) = match words.first().copied() {
        Some("transferred") => (FillMode::Transferred, "fills transferred"),
        Some("retained") => (FillMode::Retained, "fills retained"),
        _ => {
          sem.warnings.push((
            line,
            "@Refcount: fills requires `retained` or `transferred`"
              .to_string(),
          ));
          continue;
        }
      };
      let mut names = names.clone();
      let mut fill_on: Option<bool> = None;
      if words.contains(&"on") {
        let cond = names.pop().and_then(|n| loob_name(&n));
        let Some(cond) = cond else {
          sem.warnings.push((
            line,
            "@Refcount: fills ... on requires a final `c3y` or `c3n`"
              .to_string(),
          ));
          continue;
        };
        fill_on = Some(cond);
      }
      if names.is_empty() {
        sem.warnings.push((
          line,
          "@Refcount: fills requires argument names".to_string(),
        ));
        continue;
      }
      for n in &names {
        let slot = format!("pointee `{}` fill", n);
        set_slot(&mut explicit, &mut sem.warnings, line, &slot, mname);
        let pm = sem.pointees.entry(n.clone()).or_default();
        pm.fills = Some(fm);
        pm.fill_on = fill_on;
      }
      sem.why = format!("@Refcount: {}", mname);
      continue;
    }

    if head == "doomed" {
      //  `doomed on `c3n``: an exit returning this loobean obliges the
      //  caller to die; leaks and contracts on such paths are moot
      let cond = words.contains(&"on")
        .then(|| names.first().and_then(|n| loob_name(n)))
        .flatten();
      let Some(cond) = cond else {
        sem.warnings.push((
          line,
          "@Refcount: doomed requires `on `c3y`` or `on `c3n``"
            .to_string(),
        ));
        continue;
      };
      set_slot(&mut explicit, &mut sem.warnings, line, "doomed",
        if cond { "doomed on c3y" } else { "doomed on c3n" });
      sem.doomed = Some(cond);
      continue;
    }

    if head == "conslike" {
      if !names.is_empty() {
        for n in &names {
          let slot = format!("argument `{}`", n);
          set_slot(&mut explicit, &mut sem.warnings, line, &slot, "conslike");
          sem.args.insert(n.clone(), ArgumentMode::Conslike);
        }
      } else if hit_prod {
        sem.warnings.push((
          line,
          "@Refcount: conslike applies to arguments only".to_string(),
        ));
        continue;
      } else {
        set_slot(&mut explicit, &mut sem.warnings, line, "arguments",
          "conslike");
        sem.default_args = ArgumentMode::Conslike;
      }
      sem.why = "@Refcount: conslike".to_string();
      continue;
    }

    if matches!(head, "transfers" | "retains" | "transfer" | "retain") {
      let transfer = head.starts_with("transfer");
      let (amode, mname) = if transfer {
        (ArgumentMode::Transfer, "transfer")
      } else {
        (ArgumentMode::Retain, "retain")
      };
      let pmode = if transfer {
        ProductMode::Transfer
      } else {
        ProductMode::Retain
      };
      if !names.is_empty() {
        for n in &names {
          let slot = format!("argument `{}`", n);
          set_slot(&mut explicit, &mut sem.warnings, line, &slot, mname);
          sem.args.insert(n.clone(), amode);
        }
      } else if hit_prod && !hit_args {
        set_slot(&mut explicit, &mut sem.warnings, line, "product", mname);
        sem.product = pmode;
      } else if hit_args && !hit_prod {
        set_slot(&mut explicit, &mut sem.warnings, line, "arguments", mname);
        sem.default_args = amode;
      } else {
        // bare, or naming both slots: the whole protocol
        set_slot(&mut explicit, &mut sem.warnings, line, "product", mname);
        sem.product = pmode;
        set_slot(&mut explicit, &mut sem.warnings, line, "arguments", mname);
        sem.default_args = amode;
      }
      sem.why = format!("@Refcount: {}", head);
      continue;
    }

    sem.warnings.push((
      line,
      format!("unrecognized @Refcount: clause {}", py_repr(clause)),
    ));
  }

  if saw_custom && saw_other {
    sem.warnings.push((
      line,
      "@Refcount: custom must be the only annotation".to_string(),
    ));
  }
  true
}

// ---------------------------------------------------------------------------
// prefix/position defaults

pub fn prefix_sem(name: &str, file_path: &str, is_static: bool) -> Sem {
  if name.starts_with("u3r_") || name.starts_with("u3x_") {
    return Sem::new(ArgumentMode::Retain, ProductMode::Retain,
      "prefix u3r/u3x");
  }
  if name.starts_with("u3z_") {
    // memo cache: keys retained, products transferred (u3z_save's
    // own per-arg comment overrides this)
    return Sem::new(ArgumentMode::Retain, ProductMode::Transfer,
      "prefix u3z");
  }
  if re_jet_qw().is_match(name) {
    return Sem::new(ArgumentMode::Retain, ProductMode::Transfer,
      "prefix u3q/u3w");
  }
  if re_jet_k().is_match(name) {
    return Sem::new(ArgumentMode::Transfer, ProductMode::Transfer,
      "prefix u3k jets");
  }
  if (is_static || name.starts_with('_'))
    && !file_path.is_empty()
    && re_jet_dir().is_match(file_path)
  {
    // historical convention (u3.md): jet internals retain
    return Sem::new(ArgumentMode::Retain, ProductMode::Transfer,
      "internal fn in jet dir");
  }
  Sem::new(ArgumentMode::Transfer, ProductMode::Transfer, "default transfer")
}

// ---------------------------------------------------------------------------
// comment harvesting

/// Cache of raw file contents, for physically reading comments at
/// specific byte offsets.
#[derive(Default)]
pub struct SrcCache {
  files: HashMap<String, Rc<Vec<u8>>>,
}

impl SrcCache {
  pub fn bytes(&mut self, path: &str) -> Rc<Vec<u8>> {
    if let Some(b) = self.files.get(path) {
      return b.clone();
    }
    let b = Rc::new(std::fs::read(path).unwrap_or_default());
    self.files.insert(path.to_string(), b.clone());
    b
  }
}

/// A comment on the same source line as the end of a declaration, e.g.
/// `void foo(u3_noun a);  // @Refcount: retains`.
fn trailing_line_comment(cur: &Cursor, cache: &mut SrcCache) -> String {
  let end = cur.extent_end();
  let path = match &end.file {
    Some(p) => p.clone(),
    None => return String::new(),
  };
  let size = cache.bytes(&path).len() as u32;
  for tok in cur.tokens_after(size) {
    if tok.line != end.line {
      break;
    }
    if tok.kind == clang_sys::CXToken_Comment {
      return tok.spelling;
    }
  }
  String::new()
}

/// The annotation physically written at this decl/def site, read straight
/// from source via the cursor's byte offsets. Unlike libclang's raw
/// comment (which reports the definition's doc comment for the declaration
/// too), this distinguishes what is actually written at each site.
pub fn site_comment(cur: &Cursor, cache: &mut SrcCache) -> String {
  let loc = cur.extent_start();
  let path = match &loc.file {
    Some(p) => p.clone(),
    None => return String::new(),
  };
  let data = cache.bytes(&path);
  if data.is_empty() {
    return String::new();
  }
  let head_end = (loc.offset as usize).min(data.len());
  let head = String::from_utf8_lossy(&data[..head_end]).into_owned();
  let h = head.trim_end_matches([' ', '\t', '\r', '\n']);
  let mut lead = String::new();
  if h.ends_with("*/") {
    // a block comment immediately above (C comments do not nest)
    if let Some(i) = h.rfind("/*") {
      lead = h[i..].to_string();
    }
  } else {
    // a contiguous run of // line comments immediately above
    let mut out: Vec<&str> = Vec::new();
    for ln in head.lines().rev() {
      let st = ln.trim();
      if st.starts_with("//") {
        out.push(ln);
      } else if st.is_empty() && out.is_empty() {
        continue;
      } else {
        break;
      }
    }
    out.reverse();
    lead = out.join("\n");
  }
  // a trailing comment on the declaration's own line (after the `;`)
  let end = cur.extent_end().offset as usize;
  let tail_end = (end + 200).min(data.len());
  let tail_all = if end <= tail_end {
    String::from_utf8_lossy(&data[end.min(data.len())..tail_end]).into_owned()
  } else {
    String::new()
  };
  let tail = tail_all.split('\n').next().unwrap_or("").to_string();
  let trail = re_any_comment()
    .find(&tail)
    .map(|m| m.as_str().to_string())
    .unwrap_or_default();
  format!("{}\n{}", lead, trail)
}

/// Comments annotating a single cursor: its doc comment, any comments in
/// the signature, and a trailing comment on the declaration's own line.
fn cursor_comment(cur: &Cursor, cache: &mut SrcCache) -> Vec<String> {
  let mut texts = Vec::new();
  if let Some(rc) = cur.raw_comment() {
    texts.push(rc);
  }
  for tok in cur.tokens() {
    if tok.kind == clang_sys::CXToken_Punctuation && tok.spelling == "{" {
      break;
    }
    if tok.kind == clang_sys::CXToken_Comment {
      texts.push(tok.spelling);
    }
  }
  let t = trailing_line_comment(cur, cache);
  if !t.is_empty() {
    texts.push(t);
  }
  texts
}

/// All comments plausibly annotating this function (declaration and
/// definition).
pub fn cursor_comments(cur: &Cursor, cache: &mut SrcCache) -> String {
  let mut texts: Vec<String> = Vec::new();
  let mut seen: Vec<(String, u32)> = Vec::new();
  let defn = cur.definition();
  let mut sites: Vec<Cursor> = vec![*cur, cur.canonical()];
  if let Some(d) = defn {
    sites.push(d);
  }
  for c in &sites {
    let loc = c.location();
    let k = (format!("{:?}", loc.file), loc.line);
    if seen.contains(&k) {
      continue;
    }
    seen.push(k);
    for t in cursor_comment(c, cache) {
      if !texts.contains(&t) {
        texts.push(t);
      }
    }
  }
  texts.join("\n")
}

// ---------------------------------------------------------------------------
// resolution

pub type SemCache = HashMap<(Option<String>, String), Rc<Sem>>;

/// Semantics of the function declared/defined at cursor.
pub fn resolve_sem(cur: &Cursor, sem_cache: &mut SemCache, src: &mut SrcCache) -> Rc<Sem> {
  let name = cur.spelling();
  let is_static = cur.is_static();
  let is_fn = cur.kind() == clang_sys::CXCursor_FunctionDecl;
  let fpath = cur.location().file.unwrap_or_default();
  //  non-function cursors (fn-pointer fields/variables/params) are
  //  keyed by site: bare field names like `kick_f` recur across
  //  unrelated structs
  let key = (
    if is_static || !is_fn {
      Some(format!("{}:{}", fpath, if is_fn { 0 } else { cur.location().line }))
    } else {
      None
    },
    name.to_string(),
  );
  let has_def = cur.definition().is_some();
  if let Some(cached) = sem_cache.get(&key) {
    if cached.from_def || !has_def {
      return cached.clone();
    }
  }
  // annotations may live only on the definition (.c); a sem cached from
  // a TU that saw just the header declaration must not shadow it
  let mut sem = prefix_sem(&name, &fpath, is_static);
  let loc = cur.location();
  let line = if loc.file.is_some() { loc.line } else { 0 };
  parse_fn_annotations(&cursor_comments(cur, src), &mut sem, line);
  //  the return type is ground truth: annotations cannot make a non-noun
  //  product tracked. (Declarator cursors have no result type; the
  //  call site derives the product from the call expression instead.)
  if is_fn && !is_noun_type(&cur.result_type()) {
    sem.product = ProductMode::NonNoun;
  }
  sem.from_def = has_def;
  let rc = Rc::new(sem);
  sem_cache.insert(key, rc.clone());
  rc
}

/// A Sem carrying only what a single comment's @Refcount clauses say
/// (on top of the positional default), for comparing decl vs def.
fn annotation_sem(name: &str, fpath: &str, is_static: bool, comment: &str, line: u32) -> Sem {
  let mut sem = prefix_sem(name, fpath, is_static);
  parse_fn_annotations(comment, &mut sem, line);
  sem
}

/// Report if a definition's @Refcount annotations differ from those on
/// its declaration (.h vs .c must agree). Returns [(line, message)].
pub fn annotation_sync_findings(
  cur: &Cursor,
  fpath: &str,
  src: &mut SrcCache,
) -> Vec<(u32, String)> {
  let decl = cur.canonical();
  let dloc = decl.location();
  let cloc = cur.location();
  if let (Some(df), Some(cf)) = (&dloc.file, &cloc.file) {
    if df == cf && dloc.line == cloc.line {
      return Vec::new(); // definition is its own sole declaration
    }
  }
  let name = cur.spelling();
  let is_static = cur.is_static();
  let def_sem = annotation_sem(&name, fpath, is_static, &site_comment(cur, src), cloc.line);
  let decl_sem = annotation_sem(&name, fpath, is_static, &site_comment(&decl, src), dloc.line);
  if def_sem.proto_key() != decl_sem.proto_key() {
    let dfile = dloc.file.as_deref().unwrap_or("None").to_string();
    let rel = crate::relpath(&dfile);
    return vec![(
      cloc.line,
      format!(
        "@Refcount annotations out of sync between definition and declaration at {}:{}",
        rel, dloc.line
      ),
    )];
  }
  Vec::new()
}

// ---------------------------------------------------------------------------
// block-level ASSERT annotations

/// All comment tokens of one file: (offset, line, text).
pub struct FileComments {
  pub comments: Vec<(u32, u32, String)>,
}

impl FileComments {
  pub fn new(tu: &crate::ast::Tu, path: &str) -> FileComments {
    let size = std::fs::metadata(path).map(|m| m.len()).unwrap_or(0) as u32;
    let comments = tu
      .tokenize_file_range(path, 0, size)
      .into_iter()
      .filter(|t| t.kind == clang_sys::CXToken_Comment)
      .map(|t| (t.offset, t.line, t.spelling))
      .collect();
    FileComments { comments }
  }

  pub fn between(&self, lo: u32, hi: u32) -> Vec<&(u32, u32, String)> {
    self.comments
      .iter()
      .filter(|(o, _, _)| lo < *o && *o < hi)
      .collect()
  }
}

//  the colon is the annotation marker (same boundary as re_refcount):
//  prose like "see the @Refcount annotations" stays legal in comments
fn re_refcount_mention() -> &'static Regex {
  static RE: OnceLock<Regex> = OnceLock::new();
  RE.get_or_init(|| Regex::new(r"(?i)@\s*refcount\s*:").unwrap())
}

/// Loud check for @Refcount comments inside a function body: the only
/// annotation meaningful there is a block assert, and block_asserts()
/// silently skips anything its regex does not match -- a typo like
/// `asswert transfer` would otherwise change semantics without a peep.
/// Every mention that is not the start of a well-formed
/// `@Refcount: assert transfer|produce|retain [names]` is reported.
/// Returns [(line, message)].
pub fn body_annotation_warnings(fun: &Cursor, fcm: &FileComments)
  -> Vec<(u32, String)>
{
  let lo = fun.extent_start().offset;
  let hi = fun.extent_end().offset;
  let mut out = Vec::new();
  for (_, line, text) in fcm.between(lo, hi) {
    let valid: Vec<usize> =
      re_block_assert().find_iter(text).map(|m| m.start()).collect();
    for m in re_refcount_mention().find_iter(text) {
      if valid.contains(&m.start()) {
        continue;
      }
      let rest = &text[m.start()..];
      let snippet = rest.lines().next().unwrap_or(rest).trim_end();
      let at = line + text[..m.start()].matches('\n').count() as u32;
      out.push((
        at,
        format!(
          "unrecognized @Refcount annotation inside a function body \
           (only `assert transfer [names...]` is meaningful here): {}",
          py_repr(snippet)
        ),
      ));
    }
  }
  out
}

/// ASSERT annotations attached to a CompoundStmt: comments between the
/// opening brace and the first child statement.
pub fn block_asserts(compound: &Cursor, fcm: &FileComments) -> Vec<(AssertMode, Vec<String>)> {
  let kids = compound.children();
  let lo = compound.extent_start().offset;
  let hi = match kids.first() {
    Some(k) => k.extent_start().offset,
    None => compound.extent_end().offset,
  };
  let mut out = Vec::new();
  for (_, _, text) in fcm.between(lo, hi) {
    for m in re_block_assert().captures_iter(text) {
      let mode = match m[1].to_uppercase().as_str() {
        "TRANSFER" => AssertMode::Transfer,
        "DIRECT" => AssertMode::Direct,
        _ => AssertMode::Unknown,
      };
      let names: Vec<String> = m[2].split_whitespace().map(|s| s.to_string()).collect();
      out.push((mode, names));
    }
  }
  out
}
