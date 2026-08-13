//! The abstract interpreter: a path-sensitive walk over one function body
//! that tracks the ownership state of every noun-holding variable.
//!
//! This module is deliberately self-contained. Its entire interface to the
//! rest of the tool is:
//!
//!   in:  a function definition `Cursor` (read through `crate::ast`),
//!        the function's own resolved protocol (`Sem`),
//!        a `Host` that resolves callee protocols and block asserts;
//!   out: a list of `Finding`s.
//!
//! To replace the interpreter, reimplement `check_function` with the same
//! signature; nothing else in the tool needs to change.

#![allow(non_upper_case_globals)]

use std::collections::{BTreeMap, HashMap, HashSet};
use std::rc::Rc;
use imbl::{HashMap as IHashMap, HashSet as IHashSet, Vector as IVec};

use clang_sys::*;

use crate::ast::{
  binop, decl_ref_name, int_literal_value, is_direct_type, is_expr_kind,
  is_local_lvalue, is_noun_ptr_type, is_noun_type, is_weak_type, unary_op,
  unwrap_expr, Cursor, Name, Ty, Loc,
};
use crate::config;
use crate::sem::{AssertMode, Finding, ArgumentMode, FillMode, PointeeMode,
  ProductMode, Sem};

static LI: &str = "linter invariant";

macro_rules! report {
  ($g:expr, $cur:expr, $cat:expr, $($msg:tt)+) => {
    return Err(vec![report(Some($cur), $cat, format!($($msg)+), $g)])
  };
}

#[allow(unused)]
macro_rules! report_global {
  ($g:expr, $cat:expr, $($msg:tt)+) => {
    return Err(vec![report(None, $cat, format!($($msg)+), $g)])
  };
}

//  like report!, but anchored at an explicit Loc (e.g. cur.extent_end()
//  for findings about the point where control flow reconverges)
macro_rules! report_loc {
  ($g:expr, $loc:expr, $cat:expr, $($msg:tt)+) => {
    return Err(vec![report_at($loc, $cat, format!($($msg)+), $g)])
  };
}

/// Services the interpreter needs from the enclosing tool.
pub trait Host {
  /// Resolved refcount protocol of a callee (annotation + defaults).
  fn callee_sem(&mut self, callee: &Cursor) -> Rc<Sem>;
  /// `{ // @Refcount: assert ... }` annotations on a compound statement.
  fn block_asserts(&self, compound: &Cursor)
    -> Vec<(AssertMode, Vec<String>)>;
}

type ValId = u32;
#[derive(Clone, Copy, PartialEq, Eq)]
enum RefcountState {
  Uninit,             // not initialized yet
  Borrowed,           // correctly borrowed
  Owned {extra: u32}, // correctly owned
  Poisoned,           // consumed, may be freed, not valid to use
  Direct,             // direct atom, no refcounting
  Passthrough,        // Argument of a passthrough function: no rc ops allowed
  Slot,               // pointer to a noun slot; target in Env.slots
}

/// What a live slot-pointer value points at. A filled or dead slot has
/// no entry: its value is Poisoned and any use reports.
#[derive(Clone, PartialEq, Eq)]
enum SlotTarget {
  Var(VarName),         // &x: the binding cell of a tracked variable
  Hole {owner: ValId},  // unfilled deferred-cons slot inside [owner]
}

impl SlotTarget {
  /// The same target with the hole owner mapped through `f`; None when
  /// the mapping is ambiguous.
  fn remap(self, f: impl Fn(ValId) -> Option<ValId>) -> Option<SlotTarget> {
    match self {
      SlotTarget::Var(v) => Some(SlotTarget::Var(v)),
      SlotTarget::Hole {owner} =>
        f(owner).map(|o| SlotTarget::Hole {owner: o}),
    }
  }
}

/// "name" is just a name, "depth" is the depth of the scope to account
/// for name shadowing
/// 
#[derive(Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
struct VarName {name: Name, depth: u32}

/// Immutable execution environment. I think of a (sufficiently simple) piece of
/// C AST as a function that transforms Env.
/// 
/// Noun values are numbered, each ValId represents an immutable noun with its
/// current refcount state. "vars" tell us which variables in the scope
/// hold a given value. `vars_rev` is an inverse of that.
/// 
/// "contains" maps countainer nouns to subnouns within them. Used to poison
/// borrowed refs to subnouns of a noun when the latter is poisoned or compared
/// to model unifying equality (UE)
/// 
/// "slots" tracks noun pointers (u3_noun* and similar)
///
/// "views" caches head/tail lookups for a given noun
///
/// "weak" marks values that may be u3_none (u3_weak products/parameters
/// and the u3_none literal); a comparison against u3_none refines it
/// away on the unequal branch
///
#[derive(Default, Clone)]
struct Env {
  values: IHashMap<ValId, RefcountState>,
  vars: IHashMap<ValId, IHashSet<VarName>>,
  vars_rev: IHashMap<VarName, ValId>,
  contains: IHashMap<ValId, IHashSet<ValId>>,
  slots: IHashMap<ValId, SlotTarget>,
  views: IHashMap<(ValId, bool), ValId>,
  weak: IHashSet<ValId>,
}

impl Env {
  /// Make a brand-new value bound to a given name
  /// 
  fn insert_new(&mut self, name: VarName, rc: RefcountState, g: &mut Gen)
    -> ValId
  {
    let id: ValId = g.id_gen; g.id_gen += 1;

    assert!(!self.vars.contains_key(&id));
    assert!(!self.vars_rev.contains_key(&name));

    self.values.insert(id, rc);
    self.vars.insert(id, IHashSet::from([name.clone()]));
    self.vars_rev.insert(name, id);
    id
  }

  /// Add `name` as one more location of the existing value `id`.
  /// Declaration sites only: the name must be fresh.
  fn bind_decl(&mut self, name: VarName, id: ValId)
  {
    assert!(!self.vars_rev.contains_key(&name));

    self.vars.entry(id).or_default().insert(name.clone());
    self.vars_rev.insert(name, id);
  }

  /// Variable names currently holding value `id`, for messages.
  fn names(&self, id: ValId) -> String
  {
    let mut ns: Vec<&str> = self.vars.get(&id)
      .map(|locs| locs.iter().map(|v| v.name.as_ref()).collect())
      .unwrap_or_default();
    if ns.is_empty() {
      return "<temporary>".to_string();
    }
    ns.sort();  //  deterministic messages
    ns.join(", ")
  }

  /// Does `id` still own unfilled deferred-cons slots (u3i_defcons
  /// holes that were never stored to)?
  fn has_holes(&self, id: ValId) -> bool
  {
    self.slots.values()
      .any(|t| matches!(t, SlotTarget::Hole {owner} if *owner == id))
  }

  /// u3a_lose/u3z plus the consequences: borrowed views into it become poisoned
  fn lose(&mut self, id: ValId, cur: &Cursor, g: &mut Gen) -> R<()>
  {
    if self.has_holes(id) {
      report!(g, cur, "refcount error",
        "giving away [{}] with unfilled deferred slots (u3i_defcons \
         holes must be stored to first)", self.names(id));
    }
    let state = self.values.get_mut(&id)
      .expect("linter invariant: every ValId is present in values");

    match *state {
      RefcountState::Owned {extra: 0} => {
        g.note_poison(id, format!("consumed at {}", loc_str(cur)));
        *state = RefcountState::Poisoned;
        let why = format!("view into [{}], which was consumed at {}",
          self.names(id), loc_str(cur));
        poison_borrowed_within(self, id, &why, g);
      }

      RefcountState::Owned {extra}
        => *state = RefcountState::Owned {extra: extra - 1},

      RefcountState::Direct => {}

      RefcountState::Borrowed => report!(g, cur, "refcount error",
        "transfer of borrowed reference [{}]: the caller retains \
         ownership, u3k first", self.names(id)),

      RefcountState::Uninit => report!(g, cur, "refcount error",
        "transfer of uninitialized variable [{}]", self.names(id)),

      RefcountState::Poisoned => report!(g, cur, "refcount error",
        "transfer of already-consumed value [{}]{}", self.names(id),
        g.why_poisoned(id)),

      RefcountState::Passthrough => report!(g, cur, "refcount error",
        "losing passthrough value [{}]", self.names(id)),

      RefcountState::Slot => report!(g, cur, "refcount error",
        "transferring a slot pointer [{}] as a noun", self.names(id)),
    };

    Ok(())
  }
}

/// Control-flow representation
/// "local": environment of the natural control flow, if the code is reachable
/// "goto_envs": environments at the goto sites that are carried towards the
///              labels, with the goto site for printouts
/// "exit_envs": environments at the `return` sites that are joined in the end,
///              with the `return` location and the returned value if it is an
///              integer literal for conditional protocols
/// "break/cont_envs": environments at `break` and `continue` sites to join with
///                    environments after/before loop iterations
/// "switch_env": environment at `switch` side
/// "switch_vid": a noun that we switch on, to check if it is direct
#[derive(Default, Clone)]
struct Flow {
  local: Option<Env>,
  goto_envs: IHashMap<Name, IVec<(String, Env)>>,
  exit_envs: IVec<(Loc, Option<u64>, Env)>,
  break_envs: IVec<(String, Env)>,
  cont_envs: IVec<(String, Env)>,
  switch_env: Option<Env>,
  switch_vid: Option<ValId>,
}

impl Flow {
  fn scope_done(mut self, loc: Loc, depth: u32, g: &mut Gen) -> R<Flow>
  {
    self.local = self.local.map(|e| end_scope(loc, e, depth, g)).transpose()?;
    Ok(self)
  }

  fn change_local(&self, local: Option<Env>) -> Flow
  {
    let mut out = self.clone();
    out.local = local;
    return out;
  }

  fn join(mut self, loc: Loc, another: Option<Env>, g: &mut Gen) -> R<Flow>
  {
    self.local = mayb_join(loc, self.local, another, g)?;
    Ok(self)
  }
}

type R<T> = Result<T, Vec<Finding>>;

struct Gen<'a> {
  func_cur: &'a Cursor,
  funcname: Name,
  id_gen: u32,
  goto_labels_allowed: bool,
  host: &'a mut dyn Host,
  sem: &'a Sem,
  // for "assert transfer" annotations on blocks
  assert_transfer_all: u32,
  //  the original parameter values: we assume that the structural relation
  //  of the inputs is reflected in their factual refcounts even though they are
  //  borrowed
  param_vids: Vec<ValId>,
  //  pointee-annotated pointer parameters: the synthetic "*name"
  //  variable, its original value, and the contract to enforce at exit
  pointee_params: Vec<(VarName, ValId, PointeeMode)>,
  //  why a value became Poisoned, for use-after-free messages.
  poison_why: HashMap<ValId, String>,
  //  where a value gained its counted reference, for leak reports
  //  delayed to a scope end or exit sweep (the value may be nameless
  //  by then).
  owned_at: HashMap<ValId, String>,
  //  why a value may be u3_none, for [u3_none] reports.
  weak_why: HashMap<ValId, String>,
  //  fills deferred by a conditional-fill callee (`fills transferred
  //  `x` on `c3y``): the enclosing condition claims them and applies
  //  the fill on the branch whose product matches. Must be resolved by
  //  a direct c3y/c3n comparison in the same statement.
  pending_cond_fills: Option<Vec<PendCondFill>>,
}

/// One deferred conditional fill: the target variable (re-resolved at
/// claim time) and the loobean product that triggers the fill.
struct PendCondFill {
  var: VarName,
  on: bool, // fill happens when the product is this loobean
  kind: PendKind,
}

enum PendKind {
  //  annotated `fills transferred ... on`: the owned fill is deferred
  //  until the claiming comparison, and MUST be claimed (loud if not)
  Owned,
  //  loobean destructurer (u3r_cell &co) or annotated `fills retained
  //  ... on`: the borrowed fill is applied optimistically at the call;
  //  the claiming comparison UNDOES it on the branch whose product
  //  says it never happened, restoring the old value AND its state
  //  (the fill's own unbind poisoned it). Unclaimed pendings drop
  //  silently
  View { prior: ValId, prior_st: RefcountState },
}

/// Deferred-fill hygiene at a statement/condition boundary: optimistic
/// view fills simply stay (their pendings drop), but a deferred OWNED
/// fill that was never claimed is a loud error.
fn drop_pending(cur: &Cursor, g: &mut Gen) -> R<()> {
  let Some(pends) = g.pending_cond_fills.take() else { return Ok(()); };
  if pends.iter().any(|p| matches!(p.kind, PendKind::Owned)) {
    report!(g, cur, "complicated",
      "a conditional-fill call was not compared against c3y/c3n; \
       compare its product directly (`if (c3n == f(.., &out))`)");
  }
  Ok(())
}

impl Gen<'_> {
  /// Is a store blessed as a transfer by an enclosing nameless
  /// `assert transfer` block? (The named form has no store-site
  /// effect; it subtracts at block end, see named_transfers().)
  fn store_transfers(&self) -> bool {
    self.assert_transfer_all > 0
  }

  /// Remember why `id` became Poisoned (the first cause wins).
  fn note_poison(&mut self, id: ValId, why: String) {
    self.poison_why.entry(id).or_insert(why);
  }

  /// " (<why>)" suffix for messages about a poisoned value.
  fn why_poisoned(&self, id: ValId) -> String {
    self.poison_why.get(&id)
      .map(|w| format!(" ({})", w))
      .unwrap_or_default()
  }

  /// Remember where `id` gained its counted reference (creation wins).
  fn note_owned(&mut self, id: ValId, whence: String) {
    self.owned_at.entry(id).or_insert(whence);
  }

  /// Remember why `id` may be u3_none (the first cause wins).
  fn note_weak(&mut self, id: ValId, why: String) {
    self.weak_why.entry(id).or_insert(why);
  }

  /// " (<why>)" suffix for messages about a possibly-none value.
  fn why_weak(&self, id: ValId) -> String {
    self.weak_why.get(&id)
      .map(|w| format!(" ({})", w))
      .unwrap_or_default()
  }

  /// " (<creation site>)" suffix for delayed leak messages.
  fn where_owned(&self, id: ValId) -> String {
    self.owned_at.get(&id)
      .map(|w| format!(" ({})", w))
      .unwrap_or_default()
  }

  /// The pointee contract of a synthetic "*param" variable, if any.
  fn pointee_contract(&self, var: &VarName) -> Option<PointeeMode> {
    self.pointee_params.iter()
      .find(|(v, _, _)| v == var)
      .map(|(_, _, pm)| *pm)
  }
}

/// "file:line" for poison provenance messages.
fn floc(l: &Loc) -> String {
  match &l.file {
    Some(f) => format!("{}:{}", crate::relpath(f), l.line),
    None => format!("line {}", l.line),
  }
}

fn loc_str(cur: &Cursor) -> String {
  floc(&cur.location())
}

macro_rules! report_v {
  ($g:expr, $cur:expr, $cat:expr, $($msg:tt)+) => {
    return vec![report(Some($cur), $cat, format!($($msg)+), $g)]
  };
}

/// Check one function definition; returns the findings.
pub fn check_function(host: &mut dyn Host, fun: &Cursor, sem: &Sem)
  -> Vec<Finding>
{
  let funcname = fun.spelling();
  let mut body: Option<Cursor> = None;
  for c in fun.children() {
    if c.kind() == CXCursor_CompoundStmt {
      body = Some(c);
    }
  }

  let Some(body) = body else { return vec![]; };
  let mut env = Env::default();
  let mut g = Gen {
    func_cur: fun,
    funcname,
    id_gen: 0,
    goto_labels_allowed: true,
    host,
    sem,
    assert_transfer_all: 0,
    param_vids: Vec::new(),
    pointee_params: Vec::new(),
    poison_why: HashMap::new(),
    owned_at: HashMap::new(),
    weak_why: HashMap::new(),
    pending_cond_fills: None,
  };

  for p in fun.arguments() {
    let pname = p.spelling();
    if pname.is_empty() {
      report_v!(&g, fun, "strange argument", "nameless argument");
    }
    if p.kind() != CXCursor_ParmDecl || !is_noun_type(&p.ty()) {
      //  a pointee-annotated pointer parameter: model the pointed-at
      //  noun as a variable "*name" (initial state per the contract)
      //  and the parameter itself as a slot pointer to it
      if p.kind() == CXCursor_ParmDecl {
        if is_noun_ptr_type(&p.ty()) {
          let Some(pm) = sem.pointees.get(&*pname).copied() else {
            report_v!(&g, fun, "annotation",
              "@Refcount: u3_noun* [{pname}] requires an annotation");
          };
          let pv = VarName {
            name: Name::from(format!("*{}", pname)), depth: 0,
          };
          let rc = if pm.consumes {
            RefcountState::Owned {extra: 0}
          } else if pm.reads {
            RefcountState::Borrowed
          } else {
            RefcountState::Uninit
          };
          let pvid = env.insert_new(pv.clone(), rc, &mut g);
          if matches!(rc, RefcountState::Owned {..}) {
            g.note_owned(pvid, "`consumes` pointee parameter".to_string());
          }
          if is_weak_type(&p.ty().pointee_type()) {
            env.weak.insert(pvid);
            g.note_weak(pvid, format!("u3_weak pointee parameter \
              [{}]", pname));
          }
          g.param_vids.push(pvid);
          let sid = new_val(&mut env, RefcountState::Slot, &mut g);
          env.slots.insert(sid, SlotTarget::Var(pv.clone()));
          env.bind_decl(VarName {name: pname, depth: 0}, sid);
          g.pointee_params.push((pv, pvid, pm));
        }
      }
      continue;
    }
    //  report noun annotations on noun pointer arguments
    //
    if sem.pointees.contains_key(&*pname) {
      report_v!(&g, fun, "annotation",
        "@Refcount: pointee annotation (reads/consumes/fills) on \
         noun-typed parameter [{pname}]: pointee clauses apply to \
         pointer-to-noun parameters only");
    }
    let mode = sem.arg_mode(&pname);
    let rc = match mode {
      ArgumentMode::Conslike
        => report_v!(&g, fun, "not implemented", "checking of conslike"),

      ArgumentMode::Transfer    => RefcountState::Owned { extra: 0 },
      ArgumentMode::Retain      => RefcountState::Borrowed,
      ArgumentMode::Direct      => RefcountState::Direct,
      ArgumentMode::Passthrough => RefcountState::Passthrough
    };
    let pid = env.insert_new(VarName {name: pname.clone(), depth: 0}, rc,
      &mut g);
    if matches!(rc, RefcountState::Owned {..}) {
      g.note_owned(pid, "transfer parameter".to_string());
    }
    if is_weak_type(&p.ty()) {
      env.weak.insert(pid);
      g.note_weak(pid, format!("u3_weak parameter [{}]", pname));
    }
    g.param_vids.push(pid);
  }

  let flo: Flow = Flow {local: Some(env), ..Default::default()};
  match execute_statement(&body, flo, 0, &mut g) {
    Err(finding) => finding,
    Ok(flo) => {
      if !flo.goto_envs.is_empty() {
        let mut labels: Vec<&str> =
          flo.goto_envs.keys().map(|k| k.as_ref()).collect();
        labels.sort();
        return vec![report(None, "complicated",
          format!("backward goto to [{}]: the label was already passed, \
                   won't analyze", labels.join(", ")), &g)];
      }
      if g.sem.noreturn {
        if !flo.exit_envs.is_empty() || flo.local.is_some() {
          return vec![report(None, "annotation",
            "annotated `@Refcount: noreturn`, but a return or \
             fall-through is reachable".to_string(), &g)];
        }
        return vec![];
      }
      flo.exit_envs.into_iter()
        .chain(flo.local.map(|env| (body.extent_end(), None, env)))
        .map(|(loc, ret_lit, env)| check_exit(loc, ret_lit, env, &g))
        .flatten().collect()
    }
  }
}

/// `@Refcount: assert transfer <names>`: the block consumes one counted
/// reference of each listed name, on top of whatever its statements do
/// visibly. Applied to the fall-through path at block end.
fn named_transfers(cur: &Cursor, mut flo: Flow, names: &[Name],
  g: &mut Gen) -> R<Flow>
{
  if names.is_empty() {
    return Ok(flo);
  }
  let Some(mut env) = flo.local else {
    report!(g, cur, "annotation",
      "`assert transfer {}`: the block never falls through, so there is \
       no path to apply the named transfer to", names.join(" "));
  };
  for name in names {
    let Some((_, id)) = read_var(&env, name) else {
      report!(g, cur, "annotation",
        "`assert transfer {name}`: no variable of that name is in scope");
    };
    env.lose(id, cur, g)?;
  }
  flo.local = Some(env);
  Ok(flo)
}

fn execute_statement(cur: &Cursor, flo: Flow, depth: u32, g: &mut Gen)
  -> R<Flow>
{
  let k = cur.kind();
  
  //  Control flow stuff, nested statements, i.e. goto labels and everything
  //  that could contain them. We iterate over them even if we don't have
  //  current Env (end of normal control flow) because the code could be
  //  reachable via goto jumps.
  //  Goto sanity rules, enforced by the linter: no backward jumps, no jumps
  //  into control flow statements that are otherwise unreachable, no computed
  //  gotos.
  //
  //  When gotos are disallowed via a flag and we don't have local env, we can
  //  never obtain local env. This is used to make some code walks simpler.
  //
  //  deferred-fill hygiene: pendings never survive a statement
  //  boundary (view fills stay applied; unclaimed owned fills are loud)
  drop_pending(cur, g)?;

  if k == CXCursor_CompoundStmt {
    //  block-assert blessings for this compound. The nameless form
    //  blesses every store in the block: the eval side consults
    //  g.store_transfers() at store sites (restored on the way out --
    //  recursion is the save/restore stack). The named form has no
    //  store-site effect: the listed names are consumed on top of the
    //  block's own effects, at block end.
    let mut all_here = 0u32;
    let mut names_here: Vec<Name> = Vec::new();
    let mut direct_here: Vec<Name> = Vec::new();
    for (mode, names) in g.host.block_asserts(cur) {
      match mode {
        AssertMode::Transfer => {
          if names.is_empty() {
            all_here += 1;
          } else {
            names_here.extend(names.into_iter().map(Name::from));
          }
        }
        //  `assert direct low`: trusted claim that the named values
        //  are direct atoms (no runtime check exists), applied at
        //  block entry
        AssertMode::Direct => {
          if names.is_empty() {
            report!(g, cur, "annotation",
              "`assert direct` requires variable names");
          }
          direct_here.extend(names.into_iter().map(Name::from));
        }
        _ => {
          report!(g, cur, "annotation",
            "block-level `assert retain/produce` is no longer supported: \
             stores retain by default, annotate transfers only");
        }
      }
    }
    let mut flo = flo;
    if let Some(env) = flo.local.as_mut() {
      for n in &direct_here {
        let Some((_, vid)) = read_var(env, n) else {
          report!(g, cur, "annotation",
            "`assert direct {}`: no variable of that name is in scope", n);
        };
        refine_direct(env, vid);
      }
    }
    g.assert_transfer_all += all_here;

    let mut out = cur.children().into_iter()
      .try_fold(flo, |flo, kid| execute_statement(&kid, flo, depth + 1, g))?;

    //  lose each name from `` @Refcount: assert transfers `x`, `y`, ... ``
    out = named_transfers(cur, out, &names_here, g)?;
    out = out.scope_done(cur.extent_end(), depth + 1, g)?;

    g.assert_transfer_all -= all_here;
    return Ok(out);
  }
  
  else if k == CXCursor_IfStmt {
    let kids = cur.children();
    let then = kids.get(1).copied();
    let els = kids.get(2).copied();
    let Some(env) = flo.local.clone() else {
      //  walk to check that no gotos are present
      if flo.goto_envs.is_empty() {
        return Ok(flo);
      }
      let goto_stashed = g.goto_labels_allowed;
      g.goto_labels_allowed = false;
      let first = match &then {
        None    => flo,
        Some(t) => execute_statement(t, flo, depth + 1, g)?,
      };
      let second = match &els {
        None    => first,
        Some(e) => execute_statement(e, first, depth + 1, g)?,
      };
      g.goto_labels_allowed = goto_stashed;
      return Ok(second);
    };
    let Some(cond) = kids.first() else {
      report!(g, cur, "strange control flow", "if without conditional");
    };
    let (t_op_env, f_op_env) = eval_cond(cond, env, depth, g)?;

    let branch = |s: Option<Cursor>, flo: &Flow, env, g: &mut Gen| -> R<Flow> {
      let flo = flo.change_local(env);
      match s {
        None    => Ok(flo),
        Some(s) => execute_statement(&s, flo, depth + 1, g)?
                    .scope_done(s.extent_end(), depth + 1, g),
      }
    };

    let first      = branch(then.clone(), &flo,   t_op_env, g)?;
    let mut second = branch(els.clone(),  &first, f_op_env, g)?;

    //  label the sides with the branch extents, so join errors can
    //  name the disagreeing branches
    let range_lab = |c: &Option<Cursor>| -> String {
      c.as_ref()
        .map(|c| format!("{}..{}",
          floc(&c.location()), c.extent_end().line))
        .unwrap_or_default()
    };
    second.local = mayb_join_l(cur.extent_end(),
      &range_lab(&then), first.local,
      &range_lab(&els), second.local, g)?;
    return Ok(second);
  }
  //  instead of doing fixpoint analysis or somesuch, we check that the body
  //  of the while loop no-ops when it comes to refcounting:
  //  Body[cond == true] == Id[cond == false]
  //  So we join f_env with body evauated agains t_env
  //  We special-case while (1) by filling flo.local with joins of break_envs,
  //  or None if no breaks: code after while (1) {} without breaks is
  //  unreachable
  else if k == CXCursor_WhileStmt {
    let kids = cur.children();
    let Some(body) = kids.last() else {
      report!(g, cur, "strange control flow", "while without body");
    };
    let Some(env) = flo.local.clone() else {
      if flo.goto_envs.is_empty() {
        return Ok(flo);
      }
      let goto_stashed = g.goto_labels_allowed;
      g.goto_labels_allowed = false;
      let out = execute_statement(body, flo, depth + 1, g)?;
      g.goto_labels_allowed = goto_stashed;
      return Ok(out);
    };

    let Some(cond) = kids.get(0) else {
      report!(g, cur, "strange control flow", "while without condition");
    };

    match eval_cond(cond, env.clone(), depth, g)? {
      (None, None) => report!(g, cur, "strange control flow",
        "strange conditional"),

      (None, Some(f_env)) => return Ok(flo.change_local(Some(f_env))),

      (Some(t_env), f_op_env) => {
        let loop_flo = flo.change_local(Some(t_env));
        let (done_flo, fall, cont) = execute_loop_body(body, loop_flo, None,
          depth + 1, g)?;

        //  "fall" and "cont" go to the loop beginning, so we check the join
        //  the output flow's local env is false env + breaks
        //
        mayb_join(cur.location(), Some(env.clone()), cont, g)?;
        mayb_join(cur.location(), Some(env), fall.clone(), g)?;
        //  the loop also exits AFTER an iteration: the condition's
        //  false branch over the body-end env. Joining it with the
        //  zero-iteration exit lets a variable initialized inside the
        //  body survive the loop (its pre-loop Uninit adopts the
        //  initialized side); the exit refinement is re-applied by the
        //  second cond evaluation, so it survives too
        let f_fall = match fall {
          Some(fe) => eval_cond(cond, fe, depth, g)?.1,
          None => None,
        };
        let f_exit = mayb_join(cur.extent_end(), f_op_env, f_fall, g)?;
        return done_flo.join(cur.extent_end(), f_exit, g);
      }
    }
  }
  //  similarly, the only exits are the cond-false branch and breaks
  //
  else if k == CXCursor_ForStmt {
    let kids = cur.children();
    let Some(body) = kids.last() else {
      report!(g, cur, "strange control flow", "if without body");
    };
    if flo.local.is_none() {
      if flo.goto_envs.is_empty() {
        return Ok(flo);
      }
      let goto_stashed = g.goto_labels_allowed;
      g.goto_labels_allowed = false;
      let out = execute_statement(body, flo, depth + 1, g)?;
      g.goto_labels_allowed = goto_stashed;
      return Ok(out);
    };

    let Some((init, cond, inc, body)) = for_parts(cur) else {
      report!(g, cur, "strange control flow", "strange if");
    };

    let flo = match init {
      None => flo,
      Some(i) => execute_statement(&i, flo, depth + 1, g)?
    };

    let Some(env) = flo.local.clone() else {
      report!(g, cur, "strange control flow", "strange init expression")
    };

    match cond.map(|c| eval_cond(&c, env.clone(), depth, g))
              .transpose()?
              .unwrap_or((Some(env.clone()), None)) {
      (None, None) => report!(g, cur, "strange control flow",
        "strange conditional"),

      (None, Some(f_env)) => return flo.change_local(Some(f_env))
                                       .scope_done(cur.extent_end(), depth + 1, g),

      (Some(t_env), f_op_env) => {
        let loop_flo = flo.change_local(Some(t_env));
        //  depth + 2: the body must nest INSIDE the for-init scope
        //  (depth + 1), or each iteration sweeps the init variables and
        //  the loop join sees diverging alias sets
        let (done_flo, fall, cont) = execute_loop_body(&body,
          loop_flo, inc, depth + 2, g)?;
        mayb_join(cur.location(), cont, Some(env.clone()), g)?;
        mayb_join(cur.location(), fall.clone(), Some(env), g)?;
        //  post-iteration exit through the condition's false branch
        //  (see WhileStmt); a condition-less for has no fall-out exit
        let f_fall = match (&cond, fall) {
          (Some(c), Some(fe)) => eval_cond(c, fe, depth, g)?.1,
          _ => None,
        };
        let f_exit = mayb_join(cur.extent_end(), f_op_env, f_fall, g)?;
        //  the for-init scope ends here, on every exit path
        let done_flo = done_flo.scope_done(cur.extent_end(), depth + 1, g)?;
        let f_exit = f_exit
          .map(|e| end_scope(cur.extent_end(), e, depth + 1, g))
          .transpose()?;
        return done_flo.join(cur.extent_end(), f_exit, g);
      }
    }
  }
  else if k == CXCursor_DoStmt {
    let kids = cur.children();
    let Some(body) = kids.get(0) else {
      report!(g, cur, "strange control flow", "do without body");
    };
    if flo.local.is_none() {
      if flo.goto_envs.is_empty() {
        return Ok(flo);
      }
      let goto_stashed = g.goto_labels_allowed;
      g.goto_labels_allowed = false;
      let out = execute_statement(body, flo, depth + 1, g)?;
      g.goto_labels_allowed = goto_stashed;
      return Ok(out);
    };
    let Some(cond) = kids.last() else {
      report!(g, cur, "strange control flow", "do without condition");
    };

    let (loop1_flo, fall1, cont1) = execute_loop_body(body, flo.clone(),
      None, depth + 1, g)?;

    mayb_join(cur.location(), cont1, flo.local, g)?;

    let Some(env) = fall1 else {
      //  the body never reaches the condition: breaks are the exits
      return Ok(loop1_flo);
    };

    match eval_cond(cond, env.clone(), depth, g)? {
      (None, None) => report!(g, cur, "strange control flow",
        "strange conditional"),

      (None, Some(f_env)) =>
        return loop1_flo.join(cur.extent_end(), Some(f_env), g),

      (Some(t_env), f_op_env) => {
        let loop2_flo = loop1_flo.change_local(Some(t_env));
        let (done_flo, fall2, cont2) = execute_loop_body(body, loop2_flo,
          None, depth + 1, g)?;

        //  looping paths must reproduce the env before the conditional
        mayb_join(cur.location(), cont2, Some(env.clone()), g)?;
        mayb_join(cur.location(), fall2, Some(env), g)?;
        //  exits: breaks from both passes and the cond-false branch
        let done_flo = done_flo.join(cur.extent_end(), loop1_flo.local, g)?;
        return done_flo.join(cur.extent_end(), f_op_env, g);
      }
    }
  }
  else if k == CXCursor_SwitchStmt {
    let kids = cur.children();
    let Some(body) = kids.last() else {
      report!(g, cur, "strange control flow", "switch without body");
    };
    let Some(env) = flo.local.clone() else {
      if flo.goto_envs.is_empty() {
        return Ok(flo);
      }
      let goto_stashed = g.goto_labels_allowed;
      g.goto_labels_allowed = false;
      let out = execute_statement(body, flo, depth + 1, g)?;
      g.goto_labels_allowed = goto_stashed;
      return Ok(out);
    };
    let Some(val) = kids.get(0) else {
      report!(g, cur, "strange control flow",
        "switch without switch expression");
    };
    let mut flo_in = flo.clone();
    let (vid, nxt) = eval_expr(val, env, depth, g)?;
    let Some(env) = nxt else {
      report!(g, cur, "strange control flow",
        "switch crashes immediately in the expression");
    };
    flo_in.local = Some(env.clone());
    flo_in.switch_env = Some(env);
    flo_in.switch_vid = vid;
    flo_in.break_envs = Default::default();
    flo_in.goto_envs = Default::default();

    let mut flo_done = execute_statement(body, flo_in, depth, g)?;

    //  depth + 1: break envs re-enter at the switch's level, only the
    //  case-arm scopes end here
    flo_done.local =
      join_scoped(cur.extent_end(), flo_done.break_envs, flo_done.local,
        depth + 1, g)?;

    flo_done.switch_env = flo.switch_env;
    flo_done.switch_vid = flo.switch_vid;
    flo_done.goto_envs = flo_done.goto_envs
      .union_with(flo.goto_envs, |a, b| a + b);
    
    flo_done.break_envs = flo.break_envs;
    return Ok(flo_done);
  }
  else if matches!(k, CXCursor_CaseStmt | CXCursor_DefaultStmt) {
    let kids = cur.children();
    let min_kids = if k == CXCursor_CaseStmt { 2 } else { 1 };
    let switch = flo.switch_env.clone();
    let mut flo = flo.join(cur.location(), switch, g)?;

    //  a case label in direct range proves the switched noun is a
    //  direct atom in this arm
    if k == CXCursor_CaseStmt && kids.len() >= 2 {
      if let (Some(vid), Some(local)) = (flo.switch_vid, flo.local.as_mut())
      {
        let labels = &kids[..kids.len() - 1];
        if labels.iter().all(|l| int_literal_value(l)
          .is_some_and(|v| v <= config::DIRECT_MAX || v == config::U3_NONE))
        {
          refine_direct(local, vid);
        }
      }
    }

    //  XX this might break on dangling range cases like this:
    //
    //    switch (x) {
    //      case 1 ... 3:  /* no code after the range case */
    //    }
    //    ...
    //
    //  I am too lazy to handle this case
    //
    if let Some(body) = kids.last() && kids.len() >= min_kids {
      return execute_statement(body, flo, depth, g)
    }
    return Ok(flo)
  }
  else if k == CXCursor_BreakStmt {
    let mut flo = flo;
    if let Some(env) = flo.local {
      flo.break_envs.push_back((floc(&cur.location()), env));
      flo.local = None;
    };
    return Ok(flo)
  }
  else if k == CXCursor_ContinueStmt {
    let mut flo = flo;
    if let Some(env) = flo.local {
      flo.cont_envs.push_back((floc(&cur.location()), env));
      flo.local = None;
    };
    return Ok(flo)
  }
  else if k == CXCursor_LabelStmt {
    if !g.goto_labels_allowed {
      report!(g, cur, "illegal goto label position",
        "linter cannot analyze this goto label position. review the function\
         and add assertion annotations");
    }
    let label = cur.spelling();
    let mut flo = flo;
    //  all environments before label: goto sites + natural control flow if
    //  reachable
    //
    //  depth + 1: parked envs re-enter at this statement's level, only
    //  scopes strictly inside the jumped-over region end here
    let parked = flo.goto_envs.remove(&label).unwrap_or_default();
    flo.local = join_scoped(cur.location(), parked, flo.local, depth + 1, g)?;
    match cur.children().last() {
      Some(sub) => execute_statement(sub, flo, depth, g),
      None => Ok(flo),
    }
  }
  else {
    //  Local ops: Env -> Env mapping
    let Some(env) = flo.local.clone() else {
      let mut flo = flo;
      flo.local = None;
      return Ok(flo);
    };
  
    if k == CXCursor_DeclStmt {
      let mut flo = flo;
      flo.local = execute_decl(cur, env, depth, g)?;
      return Ok(flo);
    }
  
    if k == CXCursor_ReturnStmt {
      let mut flo = flo;
      if let Some(env) = execute_return(cur, env, depth, g)? {
        let ret_lit = cur.children().first()
          .and_then(int_literal_value);
        flo.exit_envs.push_back((cur.location(), ret_lit, env));
      }
      flo.local = None;
      return Ok(flo);
    }
  
    if k == CXCursor_GotoStmt {
      let mut flo = flo;
      //  park the env under the target label; joined in at the LabelStmt.
      //  (backward gotos need rejecting here once labels are tracked)
      let target = cur.children().first()
        .filter(|c| c.kind() == CXCursor_LabelRef)
        .map(|c| c.spelling());
      let Some(target) = target else {
        report!(g, cur, "strange goto", "");
      };
      flo.goto_envs.entry(target).or_default()
        .push_back((floc(&cur.location()), env));
      flo.local = None;

      return Ok(flo);
    }
    if k == CXCursor_IndirectGotoStmt {
      report!(g, cur, "computed goto", "");
    }
  
    if k == CXCursor_NullStmt || k == CXCursor_AsmStmt {
      return Ok(flo);
    }
  
    //  expression in statement position: `u3z(a);`, `x = f(y);`, bare `x;`
    //  (StmtExpr: a statement-expression macro used as a statement)
    if is_expr_kind(k) || k == CXCursor_StmtExpr {
      let mut flo = flo;
      flo.local = execute_expr_stmt(cur, env, depth, g)?;
      return Ok(flo);
    }
  
    report!(g, cur, "unhandled statement kind", "[{}] is not handled yet", k);
  }
}

fn report_at(loc: Loc, cat: &'static str, msg: String, g: &Gen) -> Finding
{
  Finding {
    file: loc.file.as_deref().unwrap_or("None").to_string(),
    line: loc.line,
    col:  loc.col,
    func: g.funcname.to_string(),
    cat,
    msg,
  }
}

fn report(cur: Option<&Cursor>, cat: &'static str, msg: String, g: &Gen)
  -> Finding
{
  report_at(cur.unwrap_or(g.func_cur).location(), cat, msg, g)
}

//  --------------------------------------------------------------------------
//  local statement handlers: one env in, one env out (None = path ended)

/// Variable declarations (possibly several per statement), including
/// initializers.
///
fn execute_decl(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  let mut env = env;
  for d in cur.children() {
    if d.kind() != CXCursor_VarDecl { continue; }
    let name = d.spelling();
    let var = VarName {name: name.clone(), depth};

    let kids = d.children();
    let ty = d.ty();
    let tk = ty.canonical().kind();
    let is_array = tk == CXType_ConstantArray || tk == CXType_IncompleteArray
      || tk == CXType_VariableArray;
    let is_record = tk == CXType_Record;

    //  The initializer is the last child, unless it is a typeref or an inline
    //  type definition. If it is an array declaration then we check if the
    //  last child is a braced list or a string literal, otherwise it is
    //  something like a size expression which is not an initializer
    //
    let init = kids.last().copied().filter(|i| {
      let ik = i.kind();
      //  ParmDecl: a function-pointer declarator's parameter list
      //  (`void (*next)(u3_noun);`) shows up as children too
      !matches!(ik, CXCursor_TypeRef | CXCursor_StructDecl | CXCursor_UnionDecl
        | CXCursor_EnumDecl | CXCursor_ParmDecl)
      && (
        !is_array
        || matches!(ik, CXCursor_InitListExpr | CXCursor_StringLiteral)
      )
    });

    //  scalar declaration
    //
    if !is_record && !is_array {
      //  a non-noun local whose name collides with a tracked outer
      //  variable must still SHADOW it (c3_malloc's stmt-expr local
      //  `void* rut` vs a caller's noun `rut`): bind it as an
      //  untracked scalar so reads resolve to this scope's variable,
      //  not to the outer noun
      let shadows = !is_noun_type(&ty) && !is_noun_ptr_type(&ty)
        && read_var(&env, &name).is_some();
      let Some(init) = init else {
        //  slot pointers (u3_noun*) are tracked alongside nouns
        if is_noun_type(&ty) || is_noun_ptr_type(&ty) {
          env.insert_new(var, RefcountState::Uninit, g);
        } else if shadows {
          env.insert_new(var, RefcountState::Direct, g);
        }
        continue;
      };
      if init.kind() == CXCursor_InitListExpr {
        //  braced scalar init (`c3_c* p = {0};`): nouns never do this
        if is_noun_type(&ty) {
          report!(g, &d, "strange definition",
            "braced initializer on a noun variable, won't analyze");
        }
        let Some(nxt) = eval_decl_init_effects(&init, env, depth, g)? else {
          return Ok(None);
        };
        env = nxt;
        continue;
      }
      let (vid, nxt) = eval_expr(&init, env, depth, g)?;
      let Some(mut nxt) = nxt else { return Ok(None); };
      if vid.is_none() && shadows {
        bind_value(&mut nxt, var, None, g);
        env = nxt;
        continue;
      }
      if vid.is_some() || is_noun_type(&ty) {
        //  declared types are contracts: a possibly-u3_none initializer
        //  may only bind to a u3_weak variable
        let weak_init = weak_desc(&nxt, g, vid, &init);
        if is_noun_type(&ty) && !is_weak_type(&ty) {
          if let Some(d) = weak_init {
            report!(g, &init, "u3_none",
              "{} initializes {} variable [{}]: declare it u3_weak, \
               or compare against u3_none first",
              d, ty.spelling(), name);
          }
        }
        //  a None vid on a noun variable means a non-noun initializer:
        //  gotta be direct (bind_value handles both)
        let id = bind_value(&mut nxt, var, vid, g);
        //  a u3_none literal bound to a u3_weak variable: the fresh
        //  value is a known none
        if vid.is_none() && weak_init.is_some() {
          nxt.weak.insert(id);
          g.note_weak(id, format!("u3_none literal at {}",
            loc_str(&init)));
        }
        //  a noun value narrowed into a sub-noun-width integer type
        //  (`c3_l col_l = u3h(blu)`) is necessarily a direct atom
        if let Some(v) = vid {
          if is_direct_type(&ty) {
            refine_direct(&mut nxt, v);
          }
        }
      }
      env = nxt;
      continue;
    }

    //  array decl: elements are unreachable to the tracker (subscripted
    //  reads have no name), so arrays of nouns are refused, size
    //  expressions must not touch nouns, and initializer elements are
    //  evaluated for effect only
    //
    if is_array {
      let sizes = &kids[..kids.len() - (init.is_some() as usize)];
      for c in sizes {
        if !is_expr_kind(c.kind()) { continue; }
        if let Some(why) = touches_nouns(c, &env) {
          report!(g, c, "strange definition",
            "array size expression involves nouns ({}), won't analyze", why);
        }
      }
      if type_has_nouns(&ty) {
        report!(g, &d, "complicated", "array of nouns, won't analyze");
      }
      if let Some(init) = init {
        let Some(nxt) = eval_decl_init_effects(&init, env, depth, g)? else {
          return Ok(None);
        };
        env = nxt;
      }
      continue;
    }

    //  record decl without nouns anywhere: nothing to bind, but the
    //  initializer elements can still involve nouns, so we eval them for
    //  effects
    //
    if !type_has_nouns(&ty) {
      if let Some(init) = init {
        let Some(nxt) = eval_decl_init_effects(&init, env, depth, g)? else {
          return Ok(None);
        };
        env = nxt;
      }
      continue;
    }

    //  noun-bearing record: only simple structs are handled - a flat struct
    //  of named scalar fields. Everything else gets reported
    //
    if ty.canonical().is_union() {
      report!(g, &d, "strange definition",
        "union with noun members, won't analyze");
    }
    let fields = ty.canonical().fields();
    for f in &fields {
      if f.spelling().is_empty() {
        report!(g, &d, "strange definition",
          "anonymous member in a noun-bearing struct, won't analyze");
      }
      let fk = f.ty().canonical().kind();
      let aggregate = fk == CXType_Record || fk == CXType_ConstantArray
        || fk == CXType_IncompleteArray || fk == CXType_VariableArray;
      if aggregate && type_has_nouns(&f.ty()) {
        report!(g, &d, "strange definition",
          "noun-bearing sub-aggregate `{}`, won't analyze", f.spelling());
      }
    }
    match init {
      None => {
        for f in &fields {
          if is_noun_type(&f.ty()) {
            let path: Name = format!("{}.{}", name, f.spelling()).into();
            env.insert_new(VarName {name: path, depth},
              RefcountState::Uninit, g);
          }
        }
      }
      Some(i) if i.kind() == CXCursor_InitListExpr => {
        match fill_flat_record(&i, &name, &fields, env, depth, g)? {
          Some(nxt) => env = nxt,
          None => return Ok(None),
        }
      }
      //  `struct pair p = other;`: member-wise ownership is invisible
      Some(_) => {
        report!(g, &d, "strange definition",
          "noun-bearing struct initialized by aggregate copy, won't analyze");
      }
    }
  }

  return Ok(Some(env));
}

fn type_has_nouns(ty: &Ty) -> bool
{
  if is_noun_type(ty) {
    return true;
  }
  let canon = ty.canonical();
  let k = canon.kind();
  if k == CXType_Record {
    return canon.fields().iter().any(|f| type_has_nouns(&f.ty()));
  }
  if k == CXType_ConstantArray || k == CXType_IncompleteArray
    || k == CXType_VariableArray
  {
    //  read the element type through the sugared type where possible, so
    //  the noun typedef spelling survives (canonical u3_noun[2] is
    //  unsigned int[2])
    let arr = if ty.kind() == k { *ty } else { canon };
    return type_has_nouns(&arr.elem_type());
  }
  false
}

/// Evaluate everything expression-like for refcount sideeffects in declarations
///
fn eval_decl_init_effects(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  let k = cur.kind();
  //  a braced sub-list, or a designated element (an UnexposedExpr of
  //  type void: designator steps, then the value)
  if k == CXCursor_InitListExpr
    || (k == CXCursor_UnexposedExpr && cur.ty().kind() == CXType_Void)
  {
    let mut env = env;
    for c in cur.children() {
      let Some(nxt) = eval_decl_init_effects(&c, env, depth, g)? else {
        return Ok(None);
      };
      env = nxt;
    }
    return Ok(Some(env));
  }
  //  designator steps and character data: no value flow
  if k == CXCursor_MemberRef || k == CXCursor_StringLiteral {
    return Ok(Some(env));
  }
  let (vid, nxt) = eval_expr(cur, env, depth, g)?;
  let Some(nxt) = nxt else { return Ok(None); };
  if vid.is_some_and(|v|
    matches!(nxt.values.get(&v), Some(RefcountState::Slot)))
  {
    report!(g, cur, "complicated",
      "slot pointer in an untracked aggregate initializer, won't \
       analyze");
  }
  Ok(Some(nxt))
}

/// Fill a flat struct with nouns. Unlisted remainder gets filled with zeros,
/// those are Direct
/// 
fn fill_flat_record(list: &Cursor,
  name: &Name,
  fields: &[Cursor],
  env: Env,
  depth: u32,
  g: &mut Gen) -> R<Option<Env>>
{
  let mut env = env;
  let elems = list.children();
  if elems.len() > fields.len() {
    report!(g, list, "strange definition",
      "more initializer elements than fields, won't analyze");
  }
  for (n, f) in fields.iter().enumerate() {
    let fnoun = is_noun_type(&f.ty());
    let path = || -> Name { format!("{}.{}", name, f.spelling()).into() };
    let Some(e) = elems.get(n) else {
      //  zero-filled tail
      if fnoun {
        env.insert_new(VarName {name: path(), depth},
          RefcountState::Direct, g);
      }
      continue;
    };
    let ek = e.kind();
    if ek == CXCursor_InitListExpr
      || (ek == CXCursor_UnexposedExpr && e.ty().kind() == CXType_Void)
    {
      report!(g, e, "strange definition",
        "nested or designated initializer in a noun-bearing struct, \
         won't analyze");
    }
    let (vid, nxt) = eval_expr(e, env, depth, g)?;
    let Some(mut nxt) = nxt else { return Ok(None); };
    if vid.is_some() || fnoun {
      //  field types are contracts, same as scalar declarations
      let weak_init = weak_desc(&nxt, g, vid, e);
      if fnoun && !is_weak_type(&f.ty()) {
        if let Some(d) = weak_init {
          report!(g, e, "u3_none",
            "{} initializes {} field [{}]: compare against u3_none \
             first", d, f.ty().spelling(), path());
        }
      }
      let id = bind_value(&mut nxt, VarName {name: path(), depth}, vid, g);
      if vid.is_none() && weak_init.is_some() {
        nxt.weak.insert(id);
        g.note_weak(id, format!("u3_none literal at {}", loc_str(e)));
      }
    }
    env = nxt;
  }
  Ok(Some(env))
}

/// Does this expression subtree involve nouns in any way?
/// 
fn touches_nouns(cur: &Cursor, env: &Env) -> Option<String>
{
  if cur.kind() == CXCursor_TypeRef {
    return None;
  }
  if is_noun_type(&cur.ty()) {
    return Some("noun-typed subexpression".to_string());
  }
  if cur.kind() == CXCursor_DeclRefExpr {
    let n = cur.spelling();
    if env.vars_rev.keys().any(|v| v.name == n) {
      return Some(format!("read of tracked variable `{}`", n));
    }
  }
  cur.children().iter().find_map(|c| touches_nouns(c, env))
}

/// Returns (
///     flow whose local is the joined BREAK paths,
///     the fall-through env after body + inc,
///     the joined continue env,
///  ), scopes cleared. The fall-through and continue envs loop around: the
///  caller checks them against the pre-condition env and drops them; only
/// breaks (and the caller's cond-false branch) exit the loop.
///
fn execute_loop_body(cur: &Cursor,
  flo: Flow,
  inc: Option<Cursor>,
  depth: u32,
  g: &mut Gen) -> R<(Flow, Option<Env>, Option<Env>)>
{
  //  goto jumps are allowed within a control flow block, can't jump inside.
  //  this is also enforced by the linter
  //
  //  we also stash our breaks and continues of the previous iteration level
  //
  let mut flo_done = execute_statement(cur,
    Flow {
      local: flo.local.clone(),
      ..Default::default()
    }, depth, g)?;

  flo_done.local = match (inc, flo_done.local.clone()) {
    (Some(i), Some(e)) => execute_expr_stmt(&i, e, depth, g)?,
    _ => flo_done.local,
  };

  let cont = join_scoped(cur.location(), flo_done.cont_envs, None, depth, g)?;
  let brks = join_scoped(cur.extent_end(), flo_done.break_envs, None, depth,
    g)?;
  let fall = flo_done.local;

  //  a nameless owned value minted by this iteration (absent from the
  //  pre-iteration env) leaks once per iteration. The looping envs are
  //  verified against the pre-condition env and DROPPED, so no scope
  //  end ever sweeps them -- checked here instead. (A braced body's own
  //  end_scope catches this first; this covers bare-statement bodies.)
  if !g.sem.noreturn {
    if let Some(pre) = &flo.local {
      for e in [&fall, &cont].into_iter().flatten() {
        let mut fresh: Vec<ValId> = e.values.iter()
          .filter(|(id, st)| matches!(st, RefcountState::Owned {..})
            && !e.vars.contains_key(*id)
            && !pre.values.contains_key(*id))
          .map(|(id, _)| *id)
          .collect();
        fresh.sort();
        if let Some(id) = fresh.first() {
          report_loc!(g, cur.extent_end(), "leak",
            "owned value{} is left unconsumed by the loop body: a new \
             reference leaks every iteration", g.where_owned(*id));
        }
      }
    }
  }

  let mut out = flo;
  out.local = brks;
  out.goto_envs = out.goto_envs.union_with(flo_done.goto_envs, |a, b| a + b);
  out.exit_envs = out.exit_envs + flo_done.exit_envs;
  Ok((out, fall, cont))
}

/// `return [expr];` -- evaluate the expression, check it against the
/// function's product protocol, and hand back the env to park in
/// exit_envs.
/// 
fn execute_return(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  let (opt_vid, opt_env) = eval_expr(cur, env, depth, g)?;
  let Some(mut env) = opt_env else { return Ok(None) };
  if let Some(vid) = opt_vid {
    if matches!(env.values.get(&vid), Some(RefcountState::Slot)) {
      report!(g, cur, "complicated",
        "returning a slot pointer, won't analyze");
    }
  }
  //  the signature is the contract: a possibly-u3_none product may
  //  only leave through a u3_weak return type
  let rty = g.func_cur.result_type();
  if is_noun_type(&rty) && !is_weak_type(&rty) {
    if let Some(rexpr) = cur.children().first() {
      if let Some(d) = weak_desc(&env, g, opt_vid, rexpr) {
        report!(g, cur, "u3_none",
          "{} returned from a function declared to return {}: return \
           u3_weak, or compare against u3_none first", d, rty.spelling());
      }
    }
  }
  match g.sem.product {
    ProductMode::Retain => {},
    ProductMode::Transfer => {
      if let Some(vid) = opt_vid {
        env.lose(vid, cur, g)?
      }
    },

    ProductMode::Direct => {
      if let Some(vid) = opt_vid {
        let rc = env.values.get(&vid).expect("linter invariant");
        if !matches!(rc, RefcountState::Direct) {
          report!(g, cur, "refcount error", "product is not direct");
        }
      }
    },

    ProductMode::Passthrough => {
      let Some(vid) = opt_vid else {
        report!(g, cur, "refcount error", "product is not passthrough");
      };
      let rc = env.values.get(&vid).expect("linter invariant");
      if !matches!(rc, RefcountState::Passthrough) {
          report!(g, cur, "refcount error", "product is not passthrough");
        }
    },
    ProductMode::NonNoun => {
      if let Some(vid) = opt_vid {
        let rc = env.values.get(&vid).expect("linter invariant");
        if !matches!(rc, RefcountState::Direct | RefcountState::Borrowed) {
          report!(g, cur, "refcount error", "non-noun-producing function \
          returned an owned noun");
        }
      }
    },
  }
  Ok(Some(env))
}

/// An expression evaluated for its effects only
fn execute_expr_stmt(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  let u = unwrap_expr(*cur);

  //  `u3k(i), u3k(t);` -- a comma chain in statement position is a
  //  sequence of statements: each element gets statement semantics
  //  (notably the bare-u3k in-place upgrade below)
  if u.kind() == CXCursor_BinaryOperator && u.binop_kind() == binop::COMMA {
    let kids = u.children();
    if kids.len() == 2 {
      let Some(env) = execute_expr_stmt(&kids[0], env, depth, g)? else {
        return Ok(None);
      };
      return execute_expr_stmt(&kids[1], env, depth, g);
    }
  }

  //  bare `u3k(x);` in statement position: the new count belongs to [x]
  //  itself, not to a discarded product
  if u.kind() == CXCursor_CallExpr {
    let rs = u.referenced().map(|r| r.spelling());
    if matches!(rs.as_deref(), Some("u3a_gain") | Some("u3a_take")) {
      let mut args = u.arguments();
      if args.is_empty() {
        args = u.children().into_iter().skip(1).collect();
      }
      //  the upgraded location: a plain name, or `*p` where p is a
      //  tracked slot pointer (`u3k(*bot);` upgrades the pointee)
      let target = args.first().and_then(|a0| {
        let au = unwrap_expr(*a0);
        if let Some(nm) = decl_ref_name(&au) {
          return read_var(&env, &nm);
        }
        if au.kind() == CXCursor_UnaryOperator
          && unary_op(&au).as_deref() == Some("*")
        {
          let pn = au.children().first().and_then(decl_ref_name)?;
          let (_, pid) = read_var(&env, &pn)?;
          if let Some(SlotTarget::Var(var)) = env.slots.get(&pid) {
            let vid = *env.vars_rev.get(var)?;
            return Some((var.clone(), vid));
          }
        }
        None
      });
      if let Some((_, vid)) = target {
        {
          let mut env = env;
          //  u3a_gain asserts on u3_none
          if env.weak.contains(&vid) {
            report!(g, cur, "u3_none",
              "u3k of possibly-none value [{}]{}: u3a_gain asserts on \
               u3_none, compare first", env.names(vid), g.why_weak(vid));
          }
          match *env.values.get(&vid).expect(LI) {
            RefcountState::Borrowed => {
              env.values.insert(vid, RefcountState::Owned {extra: 0});
              g.note_owned(vid, format!("counted by u3k at {}",
                loc_str(cur)));
            }
            RefcountState::Owned {extra} => {
              env.values.insert(vid, RefcountState::Owned {extra: extra + 1});
            }
            RefcountState::Direct => {}
            RefcountState::Uninit => report!(g, cur, "refcount error",
              "u3k of uninitialized variable [{}]", env.names(vid)),
            RefcountState::Poisoned => report!(g, cur, "use-after-free",
              "u3k of already-consumed value [{}]", env.names(vid)),
            RefcountState::Slot => report!(g, cur, "refcount error",
              "u3k of slot pointer [{}]", env.names(vid)),
            RefcountState::Passthrough => report!(g, cur, "refcount error",
              "refcount operation on passthrough value [{}]",
              env.names(vid)),
          }
          return Ok(Some(env));
        }
      }
    }
  }

  let (_, env) = eval_expr(cur, env, depth, g)?;
  let Some(env) = env else { return Ok(None); };

  //  a dropped vid stays in `values` forever (no GC); join() carries
  //  location-less ids across branches unchanged. A dropped OWNED
  //  product is reported by the scope-end/exit orphan sweeps, with its
  //  creation site
  Ok(Some(env))
}

//  --------------------------------------------------------------------------
//  expression evaluation helpers

/// Innermost live binding of `name`: scopes shadow, so among the live
/// bindings the one with the greatest depth wins.
fn read_var(env: &Env, name: &str) -> Option<(VarName, ValId)>
{
  env.vars_rev.iter()
    .filter(|(v, _)| &*v.name == name)
    .max_by_key(|(v, _)| v.depth)
    .map(|(v, id)| (v.clone(), *id))
}

/// Reading through a name whose value was already consumed.
fn read_check(cur: &Cursor, env: &Env, vid: ValId, g: &Gen) -> R<()>
{
  if matches!(env.values.get(&vid), Some(RefcountState::Poisoned)) {
    report!(g, cur, "use-after-free",
      "[{}] is (derived from) a value already consumed on this path{}",
      env.names(vid), g.why_poisoned(vid));
  }
  Ok(())
}

/// A value with no variable location yet (call product, sub-noun read).
fn new_val(env: &mut Env, rc: RefcountState, g: &mut Gen) -> ValId
{
  let id: ValId = g.id_gen; g.id_gen += 1;
  env.values.insert(id, rc);
  id
}

/// Poison every Borrowed value transitively contained in `root` (the
/// root itself is untouched): its interior may be freed under us, by
/// unifying equality (u3r_sing) or by consumption of the parent.
fn poison_borrowed_within(env: &mut Env, root: ValId, why: &str,
  g: &mut Gen)
{
  let mut stack = vec![root];
  let mut seen: HashSet<ValId> = HashSet::from([root]);
  while let Some(v) = stack.pop() {
    let Some(kids) = env.contains.get(&v) else { continue; };
    let kids: Vec<ValId> = kids.iter().copied().collect();
    for k in kids {
      if !seen.insert(k) { continue; }
      stack.push(k);
      if let Some(st) = env.values.get_mut(&k) {
        if matches!(*st, RefcountState::Borrowed) {
          g.note_poison(k, why.to_string());
          *st = RefcountState::Poisoned;
        }
      }
    }
  }
}

/// `var` stops being a location of its current value (overwrite or
/// out-param rebinding). An owned value losing its last location here
/// stays Owned and nameless: the scope-end/exit sweeps (and the join
/// and loop orphan guards) report it as a leak, with its creation site
/// from Gen.owned_at.
fn unbind_var(mut env: Env, var: &VarName) -> Env
{
  let Some(id) = env.vars_rev.get(var).copied() else { return env; };
  env.vars_rev.remove(var);
  if let Some(locs) = env.vars.get_mut(&id) {
    locs.remove(var);
    if locs.is_empty() {
      env.vars.remove(&id);
    }
  }
  env
}

/// Bind `var` to the value produced by an initializer or assignment.
/// A value with no vid (non-noun expression) binds as a fresh direct atom.
/// 
fn bind_value(env: &mut Env, var: VarName, rvid: Option<ValId>, g: &mut Gen)
  -> ValId
{
  let id = match rvid {
    None => new_val(env, RefcountState::Direct, g),
    Some(r) => r,
  };
  env.bind_decl(var, id);
  id
}

/// Is this slot pointer a plain word/byte view of a proven-direct
/// atom? Such a pointer carries no refcount obligations: it may be
/// stored or handed around freely (the &direct-var rule, extended to
/// existing slot values like pointee parameters).
fn slot_is_direct_view(env: &Env, sid: ValId) -> bool
{
  match env.slots.get(&sid) {
    Some(SlotTarget::Var(var)) => env.vars_rev.get(var)
      .and_then(|tv| env.values.get(tv))
      .is_some_and(|st| matches!(st, RefcountState::Direct)),
    _ => false,
  }
}

/// Read through a live slot pointer: the noun currently in the slot.
fn read_slot(cur: &Cursor, env: Env, sid: ValId, g: &Gen)
  -> R<(Option<ValId>, Option<Env>)>
{
  match env.slots.get(&sid).cloned().expect(LI) {
    SlotTarget::Var(v) => {
      let Some(tvid) = env.vars_rev.get(&v).copied() else {
        report!(g, cur, "complicated",
          "slot pointer target [{}] is out of scope", v.name);
      };
      read_check(cur, &env, tvid, g)?;
      Ok((Some(tvid), Some(env)))
    }
    SlotTarget::Hole {..} => {
      report!(g, cur, "refcount error",
        "read through [{}], an unfilled deferred slot", env.names(sid));
    }
  }
}

/// `*p = v` through a live slot pointer: rebind the target variable,
/// or fill the deferred-cons hole (the stored value's counted
/// reference moves into the owning structure, conslike).
fn store_slot(cur: &Cursor, env: Env, sid: ValId, rvid: Option<ValId>,
  g: &mut Gen) -> R<(Option<ValId>, Option<Env>)>
{
  let mut env = env;
  match env.slots.get(&sid).cloned().expect(LI) {
    SlotTarget::Var(var) => {
      let Some(old) = env.vars_rev.get(&var).copied() else {
        report!(g, cur, "complicated",
          "slot pointer target [{}] is out of scope", var.name);
      };
      if rvid == Some(old) {
        return Ok((rvid, Some(env)));  //  *p = *p
      }
      //  a pointee parameter not annotated `fills` must not be written
      if let Some(pm) = g.pointee_contract(&var) {
        if pm.fills.is_none() {
          report!(g, cur, "annotation",
            "store through pointer parameter [{}] whose pointee is not \
             annotated `fills retained|transferred`", var.name);
        }
      }
      env = unbind_var(env, &var);
      let id = bind_value(&mut env, var, rvid, g);
      Ok((Some(id), Some(env)))
    }
    SlotTarget::Hole {owner} => {
      if let Some(v) = rvid {
        match *env.values.get(&v).expect(LI) {
          RefcountState::Owned {extra: 0} => {
            env.values.insert(v, RefcountState::Borrowed);
            env.contains.entry(owner).or_default().insert(v);
          }
          RefcountState::Owned {extra} => {
            env.values.insert(v, RefcountState::Owned {extra: extra - 1});
            env.contains.entry(owner).or_default().insert(v);
          }
          RefcountState::Direct => {}
          RefcountState::Borrowed => report!(g, cur, "refcount error",
            "storing an uncounted reference [{}] into a deferred slot: \
             the cell owns its parts, u3k first", env.names(v)),
          RefcountState::Uninit => report!(g, cur, "refcount error",
            "storing uninitialized [{}] into a deferred slot",
            env.names(v)),
          RefcountState::Poisoned => report!(g, cur, "use-after-free",
            "storing already-consumed [{}] into a deferred slot{}",
            env.names(v), g.why_poisoned(v)),
          RefcountState::Slot => report!(g, cur, "refcount error",
            "storing a slot pointer [{}] into a deferred slot",
            env.names(v)),
          RefcountState::Passthrough => report!(g, cur, "refcount error",
            "storing passthrough value [{}] into a deferred slot",
            env.names(v)),
        }
        //  the stored value's own unfilled holes become the owner's:
        //  filling them completes the same structure (this roll-up is
        //  what keeps hole owners naming the ROOT, so the loop join
        //  converges regardless of how long the built chain gets)
        let nested: Vec<ValId> = env.slots.iter()
          .filter(|(_, t)|
            matches!(t, SlotTarget::Hole {owner: o} if *o == v))
          .map(|(k, _)| *k)
          .collect();
        for k in nested {
          env.slots.insert(k, SlotTarget::Hole {owner});
        }
      }
      //  the slot is filled; the pointer that led here is dead
      env.slots.remove(&sid);
      g.note_poison(sid,
        format!("deferred slot filled at {}", loc_str(cur)));
      env.values.insert(sid, RefcountState::Poisoned);
      Ok((rvid, Some(env)))
    }
  }
}

/// Noun used in a non-noun context has to be direct, unless it's in a macro
/// XX add macro whitelist? Currently usage of noun in macros for ints is not
/// flagged, though it may be out of scope for the refcount linter
/// 
fn direct_use_check(cur: &Cursor, env: &Env, vid: Option<ValId>, g: &Gen)
  -> R<()>
{
  let Some(v) = vid else { return Ok(()); };
  //  u3a_to_ptr, u3h_to_slot are fine
  if cur.is_macro_origin() {
    return Ok(());
  }
  if !matches!(env.values.get(&v), Some(RefcountState::Direct)) {
    report!(g, cur, "strange expression",
      "noun value [{}] used as a C integer without being proven direct: \
       guard with u3a_is_cat, or extract with u3r_word/u3r_chub",
      env.names(v));
  }
  Ok(())
}

/// Can a conditional guard refine this value to a direct atom?
fn refinable(env: &Env, vid: ValId) -> bool
{
  matches!(env.values.get(&vid),
    Some(RefcountState::Owned {..} | RefcountState::Borrowed
      | RefcountState::Uninit))
}

/// This branch proves the value is a direct atom: no counted references
/// exist, every alias of the value is off the refcount hook.
fn refine_direct(env: &mut Env, vid: ValId)
{
  if refinable(env, vid) {
    env.values.insert(vid, RefcountState::Direct);
  }
}

/// This branch proves the value is not u3_none (compared unequal to
/// u3_none, or equal to a valid literal).
fn refine_valid(env: &mut Env, vid: ValId)
{
  env.weak.remove(&vid);
}

/// Possibly-u3_none-ness of an evaluated expression, as a subject
/// phrase for [u3_none] reports: a tracked value's weak flag, or the
/// u3_none literal itself (which flows with no ValId). None = proven
/// valid.
fn weak_desc(env: &Env, g: &Gen, vid: Option<ValId>, expr: &Cursor)
  -> Option<String>
{
  match vid {
    Some(v) if env.weak.contains(&v) =>
      Some(format!("possibly-none value [{}]{}", env.names(v),
        g.why_weak(v))),
    None if int_literal_value(expr) == Some(config::U3_NONE) =>
      Some("a u3_none literal".to_string()),
    _ => None,
  }
}

//  --------------------------------------------------------------------------
//  expression evaluation

/// gotos, breaks and continues are assumed to not be present in GNU statement-
/// expressions. The linter will try to enforce that too
///
fn eval_expr(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<(Option<ValId>, Option<Env>)>
{
  let cur = unwrap_expr(*cur);
  let k = cur.kind();
  let mut env = env;

  //  `return expr;` arrives here whole from execute_return
  if k == CXCursor_ReturnStmt {
    return match cur.children().first() {
      Some(e) => eval_expr(e, env, depth, g),
      None => Ok((None, Some(env))),
    };
  }

  //  no ValId for non-noun values: a caller binding one to a noun
  //  variable makes it Direct. CXXBoolLiteralExpr is C23 true/false.
  if matches!(k, CXCursor_IntegerLiteral | CXCursor_CharacterLiteral
    | CXCursor_FloatingLiteral | CXCursor_StringLiteral
    | CXCursor_CXXBoolLiteralExpr)
  {
    return Ok((None, Some(env)));
  }

  //  sizeof/alignof: unevaluated context
  if k == CXCursor_UnaryExpr {
    return Ok((None, Some(env)));
  }

  if k == CXCursor_DeclRefExpr {
    let name = cur.spelling();
    let Some((_, vid)) = read_var(&env, &name) else {
      //  enum constants, globals, function names: opaque, untracked
      return Ok((None, Some(env)));
    };
    read_check(&cur, &env, vid, g)?;
    return Ok((Some(vid), Some(env)));
  }

  if k == CXCursor_MemberRefExpr {
    if let Some(path) = decl_ref_name(&cur) {
      if let Some((_, vid)) = read_var(&env, &path) {
        read_check(&cur, &env, vid, g)?;
        return Ok((Some(vid), Some(env)));
      }
    }
    //  untracked member chain (p->f etc.): base effects only
    for c in cur.children() {
      let (_, nxt) = eval_expr(&c, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      env = nxt;
    }
    return Ok((None, Some(env)));
  }

  if k == CXCursor_CallExpr {
    return eval_call(&cur, env, depth, g);
  }

  if k == CXCursor_ConditionalOperator {
    let kids = cur.children();
    if kids.len() != 3 {
      report!(g, &cur, "strange expression",
        "conditional operator without three operands");
    }
    return eval_ternary(&cur, &kids[0], &kids[1], &kids[2], env, depth, g);
  }

  if k == CXCursor_BinaryOperator {
    let op = cur.binop_kind();
    let kids = cur.children();
    if kids.len() != 2 {
      report!(g, &cur, "strange expression",
        "binary operator without two operands");
    }
    let (lhs, rhs) = (kids[0], kids[1]);
    if op == binop::ASSIGN {
      return eval_assign(&cur, &lhs, &rhs, env, depth, g);
    }
    if op == binop::COMMA {
      let (_, nxt) = eval_expr(&lhs, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      return eval_expr(&rhs, nxt, depth, g);
    }
    if op == binop::LAND || op == binop::LOR {
      //  value position: model the short circuit, join the outcomes
      let (t_env, f_env) = eval_cond(&cur, env, depth, g)?;
      return Ok((None, mayb_join(cur.location(), t_env, f_env, g)?));
    }
    //  arithmetic/comparison: operands are read, the product is not a
    //  counted noun. ==/!= is noun identity and fine on any noun;
    //  everything else treats the word as a raw C integer, which is
    //  meaningless on an indirect atom -- directness must be proven
    let integer_op = op != binop::EQ && op != binop::NE;
    let (lv, nxt) = eval_expr(&lhs, env, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    if integer_op {
      direct_use_check(&cur, &nxt, lv, g)?;
    }
    let (rv, nxt) = eval_expr(&rhs, nxt, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    if integer_op {
      direct_use_check(&cur, &nxt, rv, g)?;
    }
    return Ok((None, Some(nxt)));
  }

  if k == CXCursor_CompoundAssignOperator {
    if let Some(l) = cur.children().first() {
      if let Some(name) = decl_ref_name(l) {
        //  C arithmetic on a counted reference; fine on a proven-direct
        //  atom (counter idioms)
        if let Some((_, vid)) = read_var(&env, &name) {
          if !matches!(env.values.get(&vid), Some(RefcountState::Direct)) {
            report!(g, &cur, "strange expression",
              "compound assignment to tracked noun variable [{}]", name);
          }
        }
      }
    }
    for c in cur.children() {
      let (_, nxt) = eval_expr(&c, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      env = nxt;
    }
    return Ok((None, Some(env)));
  }

  if k == CXCursor_UnaryOperator {
    let op = unary_op(&cur);
    let kids = cur.children();
    let Some(child) = kids.first() else {
      report!(g, &cur, "strange expression", "unary operator without \
        operand");
    };
    if op.as_deref() == Some("&") {
      if let Some(name) = decl_ref_name(child) {
        if let Some((var, vid)) = read_var(&env, &name) {
          //  the address of a proven-direct atom is a plain word/byte
          //  view of the variable (sew/aor-style buffer readers): there
          //  is no refcount to corrupt and nothing to free through it
          if matches!(env.values.get(&vid), Some(RefcountState::Direct)) {
            return Ok((None, Some(env)));
          }
          //  &ptr (a u3_noun**) only makes sense as a u3i_defcons
          //  argument, which intercepts it before evaluation
          if matches!(env.values.get(&vid), Some(RefcountState::Slot)) {
            report!(g, &cur, "complicated",
              "address of slot pointer [{}] taken outside u3i_defcons, \
               won't analyze", name);
          }
          //  the address of a tracked noun variable is a slot pointer:
          //  reads and stores through it hit the variable. Every sink
          //  that would let it escape (unannotated call parameters,
          //  stores to memory, returns) reports instead.
          let mut env = env;
          let sid = new_val(&mut env, RefcountState::Slot, g);
          env.slots.insert(sid, SlotTarget::Var(var));
          return Ok((Some(sid), Some(env)));
        }
      }
      //  address of untracked storage: effects in the operand only
      let (_, nxt) = eval_expr(child, env, depth, g)?;
      return Ok((None, nxt));
    }
    if matches!(op.as_deref(), Some("++") | Some("--")) {
      if let Some(name) = decl_ref_name(child) {
        //  C arithmetic on a counted reference; fine on a proven-direct
        //  atom (counter idioms)
        if let Some((_, vid)) = read_var(&env, &name) {
          if !matches!(env.values.get(&vid), Some(RefcountState::Direct)) {
            report!(g, &cur, "strange expression",
              "increment/decrement of tracked noun variable [{}]", name);
          }
        }
      }
      let (_, nxt) = eval_expr(child, env, depth, g)?;
      return Ok((None, nxt));
    }
    //  *p through a live slot pointer reads the slot's current noun
    if op.as_deref() == Some("*") {
      let (v, nxt) = eval_expr(child, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      if let Some(sid) = v {
        if matches!(nxt.values.get(&sid), Some(RefcountState::Slot)) {
          return read_slot(&cur, nxt, sid, g);
        }
        read_check(&cur, &nxt, sid, g)?;
      }
      return Ok((None, Some(nxt)));
    }

    //  ! ~ - + : the operand is read, the result is untracked
    let (_, nxt) = eval_expr(child, env, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    return Ok((None, Some(nxt)));
  }

  if k == CXCursor_ArraySubscriptExpr {
    //  elements are untracked storage; base and index for effects.
    //  a noun INDEX is a raw offset: directness must be proven
    for (i, c) in cur.children().iter().enumerate() {
      let (v, nxt) = eval_expr(c, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      if i == 1 {
        direct_use_check(&cur, &nxt, v, g)?;
      }
      env = nxt;
    }
    return Ok((None, Some(env)));
  }

  if k == CXCursor_InitListExpr || k == CXCursor_CompoundLiteralExpr {
    //  compound literal in expression position: untracked aggregate
    //  ((float16_t){SB_REAL16_ONE} in the lagoon i754 jets). The
    //  TypeRef child of a compound literal is not an expression.
    for c in cur.children().into_iter()
      .filter(|c| is_expr_kind(c.kind()))
    {
      let (_, nxt) = eval_expr(&c, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      env = nxt;
    }
    return Ok((None, Some(env)));
  }

  if k == CXCursor_CompoundStmt {
    //  GNU statement expression (unwrap_expr strips the StmtExpr node):
    //  run the prefix statements, the last expression is the value
    return eval_stmt_expr(&cur, env, depth, g);
  }

  report!(g, &cur, "unhandled expression kind", "[{}] is not handled yet",
    k);
}

/// GNU statement expression: `({ stmt; stmt; value; })`.
fn eval_stmt_expr(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<(Option<ValId>, Option<Env>)>
{
  let kids = cur.children();
  let inner = depth + 1;
  let n = kids.len();

  let mut flo = Flow { local: Some(env), ..Default::default() };
  for kid in kids.iter().take(n.saturating_sub(1)) {
    flo = execute_statement(kid, flo, inner, g)?;
    if !flo.goto_envs.is_empty() || !flo.break_envs.is_empty()
      || !flo.cont_envs.is_empty() || !flo.exit_envs.is_empty()
    {
      report!(g, cur, "complicated",
        "goto/break/continue/return inside a statement expression, \
         won't analyze");
    }
  }
  let Some(env) = flo.local else { return Ok((None, None)); };
  let Some(last) = kids.last() else {
    return Ok((None, Some(end_scope(cur.extent_end(), env, inner, g)?)));
  };

  if !is_expr_kind(last.kind()) {
    //  no value: execute the tail statement and close the scope
    let flo = Flow { local: Some(env), ..Default::default() };
    let flo = execute_statement(last, flo, inner, g)?;
    if !flo.goto_envs.is_empty() || !flo.break_envs.is_empty()
      || !flo.cont_envs.is_empty() || !flo.exit_envs.is_empty()
    {
      report!(g, cur, "complicated",
        "goto/break/continue/return inside a statement expression, \
         won't analyze");
    }
    let out = flo.local
      .map(|e| end_scope(cur.extent_end(), e, inner, g))
      .transpose()?;
    return Ok((None, out));
  }

  let (vid, nxt) = eval_expr(last, env, inner, g)?;
  let Some(mut nxt) = nxt else { return Ok((None, None)); };

  //  the value must survive the scope teardown (it may live in a local
  //  of the statement expression, e.g. c3_min): hold it with a synthetic
  //  outer-depth location, then drop that location without poisoning
  let hold = vid.map(|v| {
    let hold = VarName {name: Name::from("<stmt-expr>"), depth};
    nxt.bind_decl(hold.clone(), v);
    hold
  });
  let mut out = end_scope(cur.extent_end(), nxt, inner, g)?;
  if let Some(hold) = hold {
    if let Some(v) = out.vars_rev.remove(&hold) {
      if let Some(locs) = out.vars.get_mut(&v) {
        locs.remove(&hold);
        if locs.is_empty() {
          out.vars.remove(&v);
        }
      }
    }
  }
  Ok((vid, Some(out)))
}

/// `cond ? a : b` -- both branches evaluated against the split
/// environments, then rejoined; the two result values are reconciled.
fn eval_ternary(cur: &Cursor, cond: &Cursor, a: &Cursor, b: &Cursor,
  env: Env, depth: u32, g: &mut Gen) -> R<(Option<ValId>, Option<Env>)>
{
  let (t_env, f_env) = eval_cond(cond, env, depth, g)?;
  let (t_vid, t_env) = match t_env {
    Some(e) => eval_expr(a, e, depth, g)?,
    None => (None, None),
  };
  let (f_vid, f_env) = match f_env {
    Some(e) => eval_expr(b, e, depth, g)?,
    None => (None, None),
  };

  let (te, fe) = match (t_env, f_env) {
    (None, None) => return Ok((None, None)),
    //  one branch crashed: the other passes through, ids untouched
    (Some(te), None) => return Ok((t_vid, Some(te))),
    (None, Some(fe)) => return Ok((f_vid, Some(fe))),
    (Some(te), Some(fe)) => (te, fe),
  };
  //  mixed literal/value product: mint a stand-in for the product in
  //  the value branch, so the product's facts stay per-branch instead
  //  of fusing with the variable's own binding across the join --
  //  `(hav == u3_none) ? u3_nul : hav` produces a proven-valid noun
  //  even though [hav] itself is maybe-none after the join
  let none_lit =
    |c: &Cursor| int_literal_value(c) == Some(config::U3_NONE);
  let (mut te, mut fe) = (te, fe);
  let (t_vid, f_vid) = match (t_vid, f_vid) {
    (Some(v), None) =>
      (Some(ternary_standin(cur, &mut te, v, none_lit(b), g)), None),
    (None, Some(v)) =>
      (None, Some(ternary_standin(cur, &mut fe, v, none_lit(a), g))),
    other => other,
  };
  let mut joined = join(cur.location(), te.clone(), fe.clone(), g)?;

  //  branch result ids may be renamed by the join: a value that had
  //  names lands under the id its names resolve to, a location-less one
  //  is carried under its own id
  let resolve = |side: &Env, joined: &Env, v: ValId| -> ValId {
    match side.vars.get(&v).and_then(|l| l.iter().next()) {
      Some(name) => *joined.vars_rev.get(name).expect(LI),
      None => v,
    }
  };
  let vid = match (t_vid, f_vid) {
    (None, None) => None,
    //  one branch has no tracked value (a literal: reading a direct
    //  atom adds no obligations): the stand-in minted above passes
    //  through (location-less, so resolve keeps its identity)
    (Some(v), None) => Some(resolve(&te, &joined, v)),
    (None, Some(v)) => Some(resolve(&fe, &joined, v)),
    (Some(av), Some(bv)) => {
      let a2 = resolve(&te, &joined, av);
      let b2 = resolve(&fe, &joined, bv);
      if a2 == b2 {
        Some(a2)
      } else {
        Some(merge_ternary_vals(cur, &mut joined, a2, b2, g)?)
      }
    }
  };
  Ok((vid, Some(joined)))
}

/// The product of a ternary whose other arm is an untracked literal:
/// a fresh stand-in value in the value-arm env carrying the branch's
/// facts. Validity is per-branch (`(hav == u3_none) ? u3_nul : hav`
/// produces a valid noun while [hav] stays maybe-none after the join);
/// a u3_none arm makes the product possibly-none regardless. An owned
/// variable hands its count to the product and keeps an uncounted
/// view (consuming the product poisons it).
fn ternary_standin(cur: &Cursor, env: &mut Env, v: ValId,
  none_arm: bool, g: &mut Gen) -> ValId
{
  let pro = match *env.values.get(&v).expect(LI) {
    RefcountState::Borrowed => {
      let pro = new_val(env, RefcountState::Borrowed, g);
      env.contains.entry(v).or_default().insert(pro);
      pro
    }
    RefcountState::Direct => new_val(env, RefcountState::Direct, g),
    RefcountState::Owned {extra} => {
      let pro = new_val(env, RefcountState::Owned {extra}, g);
      env.values.insert(v, RefcountState::Borrowed);
      env.contains.entry(pro).or_default().insert(v);
      if let Some(w) = g.owned_at.get(&v).cloned() {
        g.note_owned(pro, w);
      }
      pro
    }
    //  Uninit/Poisoned/Slot/Passthrough: keep the identity, the use
    //  sites report
    _ => return v,
  };
  if env.weak.contains(&v) {
    env.weak.insert(pro);
    if let Some(w) = g.weak_why.get(&v).cloned() {
      g.note_weak(pro, w);
    }
  }
  if none_arm && !env.weak.contains(&pro) {
    env.weak.insert(pro);
    g.note_weak(pro, format!("u3_none arm of the conditional at {}",
      loc_str(cur)));
  }
  pro
}

/// The two branches of a ternary produced different values: unify their
/// ownership into one value.
fn merge_ternary_vals(cur: &Cursor, env: &mut Env, a: ValId, b: ValId,
  g: &Gen) -> R<ValId>
{
  let located = |env: &Env, v: ValId| -> bool {
    env.vars.get(&v).is_some_and(|l| !l.is_empty())
  };
  let sa = *env.values.get(&a).expect(LI);
  let sb = *env.values.get(&b).expect(LI);
  if matches!(sa, RefcountState::Slot) || matches!(sb, RefcountState::Slot)
  {
    report!(g, cur, "complicated",
      "conditional produces slot pointers, won't analyze");
  }
  let a_loc = located(env, a);
  let b_loc = located(env, b);

  let merged = match join_states(sa, sb) {
    //  consuming "one of two live variables" cannot be attributed, so
    //  owned values are only mergeable when at most one side has names
    Some(RefcountState::Owned {..}) if a_loc && b_loc => {
      report!(g, cur, "complicated",
        "conditional over two live owned variables ([{}] vs [{}]), \
         won't analyze", env.names(a), env.names(b));
    }
    Some(m) => m,
    None => {
      report!(g, cur, "refcount error",
        "conditional branches produce conflicting ownership ([{}] vs \
         [{}])", env.names(a), env.names(b));
    }
  };

  //  keep the located side's identity so its names stay attached
  let (keep, lose) = if !a_loc && b_loc { (b, a) } else { (a, b) };
  env.values.insert(keep, merged);
  //  possibly-none if either arm was
  if env.weak.contains(&lose) {
    env.weak.insert(keep);
  }

  //  fold the loser's containment structure into the survivor
  let lose_kids = env.contains.get(&lose).cloned().unwrap_or_default();
  if !lose_kids.is_empty() {
    let mut kids = env.contains.get(&keep).cloned().unwrap_or_default();
    for k in lose_kids {
      kids.insert(k);
    }
    env.contains.insert(keep, kids);
  }
  let parents: Vec<ValId> = env.contains.iter()
    .filter(|(_, kids)| kids.contains(&lose))
    .map(|(p, _)| *p)
    .collect();
  for p in parents {
    env.contains.entry(p).or_default().insert(keep);
  }
  //  a location-less loser would otherwise read as a leaked own at exit
  if !located(env, lose) {
    env.values.insert(lose, RefcountState::Poisoned);
  }
  Ok(keep)
}

/// Rebind a destructurer out-param variable to a borrowed sub-noun of
/// `src_vid`. An owned prior value goes nameless and is reported by
/// the orphan sweeps.
fn fill_out_param(mut env: Env, var: VarName, prior: Option<ValId>,
  src_vid: Option<ValId>, loob: bool, g: &mut Gen) -> Env
{
  let prior_st = prior.map(|p| *env.values.get(&p).expect(LI));
  env = unbind_var(env, &var);
  let id = new_val(&mut env, RefcountState::Borrowed, g);
  env.bind_decl(var.clone(), id);
  if let Some(src) = src_vid {
    env.contains.entry(src).or_default().insert(id);
  }
  //  a loobean destructurer fills only when it returns c3y: the fill
  //  is applied optimistically, and a claiming comparison restores the
  //  old binding on the c3n branch
  if let (true, Some(prior), Some(prior_st)) = (loob, prior, prior_st) {
    g.pending_cond_fills.get_or_insert_with(Vec::new)
      .push(PendCondFill {
        var, on: true, kind: PendKind::View {prior, prior_st},
      });
  }
  env
}

/// The per-argument protocol modes every call shape supports --
/// declared functions and calls through function pointers alike:
/// transfer consumes the argument, retain leaves it with the caller
/// (an unnamed owned product then stays an unconsumed orphan for the
/// scope-end sweep), direct proves it a direct atom. Passthrough and
/// Conslike are declared-call-only refinements, handled by the caller.
fn apply_basic_arg_mode(env: &mut Env, mode: ArgumentMode, v: ValId,
  a: &Cursor, g: &mut Gen) -> R<()>
{
  match mode {
    ArgumentMode::Transfer => env.lose(v, a, g)?,
    ArgumentMode::Retain => {}
    ArgumentMode::Direct => {
      //  the callee bails unless this is a direct atom: on return it
      //  is proven direct, with no counted references
      refine_direct(env, v);
    }
    _ => unreachable!("{}", LI),
  }
  Ok(())
}

/// A call expression: hard-wired noun primitives first, then the
/// callee's resolved protocol.
/// A destructurer call (u3x_cell &co): the argument at `src_i` is the
/// source noun; every other `&var` argument is an out-param whose
/// variable is rebound to a borrowed sub-noun of the source.
fn eval_destructurer(_cur: &Cursor, args: &[Cursor], src_i: usize,
  loob: bool, env: Env, depth: u32, g: &mut Gen)
  -> R<(Option<ValId>, Option<Env>)>
{
  let mut env = env;
  let mut src_vid: Option<ValId> = None;
  for (i, a) in args.iter().enumerate() {
    if i == src_i {
      let (v, nxt) = eval_expr(a, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      env = nxt;
      if let Some(d) = weak_desc(&env, g, v, a) {
        report!(g, a, "u3_none",
          "destructuring {}: compare against u3_none first", d);
      }
      src_vid = v;
      continue;
    }
    let au = unwrap_expr(*a);
    if au.kind() == CXCursor_UnaryOperator
      && unary_op(&au).as_deref() == Some("&")
    {
      if let Some(nm) = au.children().first().and_then(decl_ref_name) {
        if let Some((var, prior)) = read_var(&env, &nm) {
          env = fill_out_param(env, var, Some(prior), src_vid, loob, g);
          continue;
        }
      }
      //  &untracked storage: opaque out-param
      continue;
    }
    let (v, nxt) = eval_expr(a, env, depth, g)?;
    let Some(mut nxt) = nxt else { return Ok((None, None)); };
    if let Some(v) = v.filter(|v|
      matches!(nxt.values.get(v), Some(RefcountState::Slot)))
    {
      //  a slot-pointer value (an out-param handed through, e.g.
      //  `u3r_qual(dat, &typ, bot, mod, use)` with u3_noun* params):
      //  the pointed-at variable is rebound to a borrowed sub-noun,
      //  same as a literal `&var`
      match nxt.slots.get(&v).cloned().expect(LI) {
        SlotTarget::Var(var) => {
          let prior = nxt.vars_rev.get(&var).copied();
          nxt = fill_out_param(nxt, var, prior, src_vid, loob, g);
        }
        SlotTarget::Hole {..} => {
          report!(g, a, "refcount error",
            "a destructurer would fill a deferred slot with an \
             uncounted view; only a `fills transferred` callee may \
             fill one");
        }
      }
    }
    env = nxt;
  }
  Ok((None, Some(env)))
}

fn eval_call(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<(Option<ValId>, Option<Env>)>
{
  //  conditional fills must be claimed by the immediately enclosing
  //  c3y/c3n comparison; reaching another call first loses them
  drop_pending(cur, g)?;
  let callee = cur.referenced();
  let is_fn_decl = callee.as_ref()
    .is_some_and(|c| c.kind() == CXCursor_FunctionDecl);
  let cname: Option<Name> = callee.as_ref().map(|c| c.spelling());
  //  an indirect call may still reference the pointer variable/field:
  //  its name must not collide with the special-cased function names
  let cn = if is_fn_decl { cname.as_deref().unwrap_or("") } else { "" };
  let mut args = cur.arguments();
  if args.is_empty() {
    args = cur.children().into_iter().skip(1).collect();
  }
  let mut env = env;

  if config::NORETURN_FNS.contains(&cn) {
    for a in &args {
      let (_, nxt) = eval_expr(a, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      env = nxt;
    }
    return Ok((None, None));
  }

  //  u3z: give away one counted reference. A possibly-u3_none argument
  //  is a de-facto safe no-op (u3a_north/south_is_normal return c3n
  //  for it), so it is tolerated unless --strict-weak asks otherwise
  if cn == "u3a_lose" {
    let Some(a0) = args.first() else {
      report!(g, cur, "strange expression", "u3z without an argument");
    };
    let (vid, nxt) = eval_expr(a0, env, depth, g)?;
    let Some(mut nxt) = nxt else { return Ok((None, None)); };
    if config::strict_weak() {
      if let Some(d) = weak_desc(&nxt, g, vid, a0) {
        report!(g, cur, "u3_none",
          "u3z of {}: compare against u3_none first (reported under \
           --strict-weak)", d);
      }
    }
    if let Some(vid) = vid {
      nxt.lose(vid, cur, g)?;
    }
    return Ok((None, Some(nxt)));
  }

  //  u3k: a new counted reference to the same noun. The count is
  //  independent of the argument's own reference, so the product is a
  //  separate value (a fresh count on a direct atom is still direct)
  if cn == "u3a_gain" || cn == "u3a_take" {
    let Some(a0) = args.first() else {
      report!(g, cur, "strange expression", "u3k without an argument");
    };
    let (vid, nxt) = eval_expr(a0, env, depth, g)?;
    let Some(mut nxt) = nxt else { return Ok((None, None)); };
    //  u3a_gain asserts on u3_none: gaining a possibly-none value is
    //  always an error
    if let Some(d) = weak_desc(&nxt, g, vid, a0) {
      report!(g, cur, "u3_none",
        "u3k of {}: u3a_gain asserts on u3_none, compare first", d);
    }
    if let Some(v) = vid {
      if nxt.has_holes(v) {
        report!(g, cur, "refcount error",
          "u3k of [{}], an incomplete noun with unfilled deferred slots",
          nxt.names(v));
      }
      //  u3k of an unnamed owned temporary (`u3k(u3k(x))`, `u3k(f(x))`):
      //  the same pointer gains one more count, so keep the value's
      //  identity -- both counts are then consumed through it (the
      //  skid.c double-gain shape). A fresh vid here would orphan the
      //  inner count as a phantom leak and let the first consumption
      //  falsely kill the second reference. Named values still get a
      //  fresh vid: their aliases must not share the new count.
      if let Some(RefcountState::Owned {extra}) = nxt.values.get(&v).copied()
      {
        if nxt.vars.get(&v).is_none_or(|l| l.is_empty()) {
          nxt.values.insert(v, RefcountState::Owned {extra: extra + 1});
          return Ok((Some(v), Some(nxt)));
        }
      }
    }
    let prod = match vid.map(|v| (v, *nxt.values.get(&v).expect(LI))) {
      Some((v, RefcountState::Uninit)) => {
        report!(g, cur, "refcount error",
          "u3k of uninitialized variable [{}]", nxt.names(v));
      }
      Some((v, RefcountState::Slot)) => {
        report!(g, cur, "refcount error",
          "u3k of slot pointer [{}]", nxt.names(v));
      }
      Some((_, RefcountState::Direct)) => RefcountState::Direct,
      _ => RefcountState::Owned {extra: 0},
    };
    let id = new_val(&mut nxt, prod, g);
    if matches!(prod, RefcountState::Owned {..}) {
      g.note_owned(id, format!("created by u3k at {}", loc_str(cur)));
    }
    return Ok((Some(id), Some(nxt)));
  }

  //  u3h/u3t: an uncounted view into the argument's interior
  if cn == "u3a_h" || cn == "u3a_t" {
    let Some(a0) = args.first() else {
      report!(g, cur, "strange expression", "u3h/u3t without an argument");
    };
    let (vid, nxt) = eval_expr(a0, env, depth, g)?;
    let Some(mut nxt) = nxt else { return Ok((None, None)); };
    //  u3_none is not a cell: destructuring a possibly-none value
    //  reads garbage
    if let Some(d) = weak_desc(&nxt, g, vid, a0) {
      report!(g, cur, "u3_none",
        "u3h/u3t of {}: compare against u3_none first", d);
    }
    if let Some(parent) = vid {
      if matches!(nxt.values.get(&parent), Some(RefcountState::Uninit)) {
        report!(g, cur, "refcount error",
          "u3h/u3t of uninitialized variable [{}]", nxt.names(parent));
      }
      if matches!(nxt.values.get(&parent), Some(RefcountState::Slot)) {
        report!(g, cur, "refcount error",
          "u3h/u3t of slot pointer [{}]", nxt.names(parent));
      }
      if nxt.has_holes(parent) {
        report!(g, cur, "refcount error",
          "u3h/u3t of [{}], an incomplete noun with unfilled deferred \
           slots", nxt.names(parent));
      }
      //  nouns are immutable: a repeated u3h/u3t of the same parent
      //  value is the SAME sub-noun -- reuse its value, so refinements
      //  (an is_cat guard) and consumption survive across reads
      let key = (parent, cn == "u3a_t");
      if let Some(&cached) = nxt.views.get(&key) {
        if !matches!(nxt.values.get(&cached),
          Some(RefcountState::Poisoned) | None)
        {
          return Ok((Some(cached), Some(nxt)));
        }
      }
      let id = new_val(&mut nxt, RefcountState::Borrowed, g);
      nxt.contains.entry(parent).or_default().insert(id);
      nxt.views.insert(key, id);
      return Ok((Some(id), Some(nxt)));
    }
    let id = new_val(&mut nxt, RefcountState::Borrowed, g);
    return Ok((Some(id), Some(nxt)));
  }

  //  u3a_is_cat &co: reads only, loobean product. u3_none looks like
  //  an indirect reference to these predicates (u3a_is_dog(u3_none) is
  //  c3y), so a possibly-none argument must be checked first
  if config::guard_kind(cn).is_some() {
    let Some(a0) = args.first() else {
      report!(g, cur, "strange expression", "guard without an argument");
    };
    let (v, nxt) = eval_expr(a0, env, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    if let Some(d) = weak_desc(&nxt, g, v, a0) {
      report!(g, cur, "u3_none",
        "{}() of {}: compare against u3_none first", cn, d);
    }
    return Ok((None, Some(nxt)));
  }

  //  u3x_cell &co: `&var` out-params become borrowed sub-nouns of the
  //  source
  if let Some(src_i) = config::destructurer_src(cn) {
    return eval_destructurer(cur, &args, src_i,
      config::destructurer_loobean(cn), env, depth, g);
  }

  //  u3i_defcons: allocate a cell whose head and tail are filled later
  //  through the returned slot pointers. The product is a fresh owned
  //  cell carrying two unfilled holes; each &ptr argument rebinds that
  //  pointer variable to one of them.
  if cn == "u3i_defcons" {
    let cell = new_val(&mut env, RefcountState::Owned {extra: 0}, g);
    g.note_owned(cell, format!("created by u3i_defcons() at {}",
      loc_str(cur)));
    for a in &args {
      let au = unwrap_expr(*a);
      let pname = (au.kind() == CXCursor_UnaryOperator
        && unary_op(&au).as_deref() == Some("&"))
        .then(|| au.children().first().and_then(decl_ref_name))
        .flatten();
      let Some(pname) = pname else {
        report!(g, a, "complicated",
          "u3i_defcons out-pointer is not `&ptr` of a local pointer \
           variable, won't analyze");
      };
      let Some((var, _)) = read_var(&env, &pname) else {
        report!(g, a, "complicated",
          "u3i_defcons out-pointer [{}] is not a tracked pointer \
           variable, won't analyze", pname);
      };
      env = unbind_var(env, &var);
      let h = new_val(&mut env, RefcountState::Slot, g);
      env.slots.insert(h, SlotTarget::Hole {owner: cell});
      env.bind_decl(var, h);
    }
    return Ok((Some(cell), Some(env)));
  }

  //  no referenced function declaration: a call through a function
  //  pointer (referenced() may still name the pointer variable/field).
  //  Convention: functions called through pointers TRANSFER -- noun
  //  arguments are consumed, a noun product is owned by the caller.
  //  Callback implementations must therefore follow transfer protocol.
  if !is_fn_decl {
    //  a call through a function pointer: the protocol comes from the
    //  pointer declarator's own @Refcount annotation (a struct field,
    //  variable, or parameter -- e.g. a `//  @Refcount: retains `who``
    //  trailing comment on a callback field); the default is TRANSFER
    let dsem: Option<Rc<Sem>> = callee.as_ref()
      .filter(|c| matches!(c.kind(),
        CXCursor_FieldDecl | CXCursor_VarDecl | CXCursor_ParmDecl))
      .map(|c| g.host.callee_sem(c));
    let dname = cname.as_deref().unwrap_or("<fn pointer>");
    if let Some(ds) = &dsem {
      if !ds.pointees.is_empty() {
        report!(g, cur, "annotation",
          "pointee clauses on function-pointer declarator \
           [{}] are not supported yet", dname);
      }
    }
    //  declarator parameter names, for per-argument annotations
    let params: Vec<Cursor> = callee.as_ref()
      .map(|c| c.children().into_iter()
        .filter(|k| k.kind() == CXCursor_ParmDecl).collect())
      .unwrap_or_default();
    if dsem.as_ref().is_some_and(|ds| ds.noreturn) {
      for a in &args {
        let (_, nxt) = eval_expr(a, env, depth, g)?;
        let Some(nxt) = nxt else { return Ok((None, None)); };
        env = nxt;
      }
      return Ok((None, None));
    }
    //  evaluate ALL arguments first, consume after -- same two-phase
    //  shape as a declared call: the callee consumes at the call, not
    //  while later operands still evaluate (`f(x, u3k(x))` gains from
    //  x after x's count is promised away, which is legal C)
    let mut evald: Vec<(Cursor, Option<ValId>)> = Vec::new();
    for a in &args {
      let (v, nxt) = eval_expr(a, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      env = nxt;
      if let Some(v) = v {
        if matches!(env.values.get(&v), Some(RefcountState::Slot)) {
          report!(g, a, "complicated",
            "slot pointer [{}] passed through a function-pointer call, \
             won't analyze", env.names(v));
        }
      }
      evald.push((*a, v));
    }
    let mut noun_args: Vec<ValId> = Vec::new();
    for (i, (a, v)) in evald.iter().enumerate() {
      //  the declarator's parameter type is the contract, same as a
      //  declared call
      if let Some(p) = params.get(i) {
        if is_noun_type(&p.ty()) && !is_weak_type(&p.ty()) {
          if let Some(d) = weak_desc(&env, g, *v, a) {
            report!(g, a, "u3_none",
              "{} passed as a {} parameter of [{}]: compare against \
               u3_none first", d,
              p.ty().spelling(), dname);
          }
        }
      }
      let Some(v) = *v else { continue; };
      noun_args.push(v);
      let mode = dsem.as_ref()
        .map(|ds| params.get(i)
          .map(|p| ds.arg_mode(&p.spelling()))
          .unwrap_or(ds.default_args))
        .unwrap_or(ArgumentMode::Transfer);
      match mode {
        ArgumentMode::Transfer | ArgumentMode::Retain
        | ArgumentMode::Direct => {
          apply_basic_arg_mode(&mut env, mode, v, a, g)?;
        }
        _ => {
          report!(g, a, "annotation",
            "unsupported argument mode on function-pointer declarator \
             [{}] (transfer/retain/direct only)", dname);
        }
      }
    }
    if is_noun_type(&cur.ty()) {
      //  the call expression's type is the declarator's return type:
      //  u3_weak products may be u3_none
      let weak_prod = is_weak_type(&cur.ty());
      let retain_prod = dsem.as_ref()
        .is_some_and(|ds| ds.product == ProductMode::Retain);
      let id = if retain_prod {
        let id = new_val(&mut env, RefcountState::Borrowed, g);
        for v in noun_args {
          env.contains.entry(v).or_default().insert(id);
        }
        id
      } else {
        let id = new_val(&mut env, RefcountState::Owned {extra: 0}, g);
        g.note_owned(id, format!("created by {}() at {}", dname,
          loc_str(cur)));
        id
      };
      if weak_prod {
        env.weak.insert(id);
        g.note_weak(id, format!("u3_weak product of {}() at {}", dname,
          loc_str(cur)));
      }
      return Ok((Some(id), Some(env)));
    }
    return Ok((None, Some(env)));
  }
  let cal = callee.expect(LI);

  let sem = g.host.callee_sem(&cal);

  //  `@Refcount: noreturn`: execution ends at the call site; arguments
  //  are evaluated for effect, no transfer accounting applies
  if sem.noreturn {
    for a in &args {
      let (_, nxt) = eval_expr(a, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      env = nxt;
    }
    return Ok((None, None));
  }

  //  a custom protocol says nothing about how arguments are treated
  if sem.custom {
    let mut tracked = false;
    for a in &args {
      let (v, nxt) = eval_expr(a, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      tracked = tracked || v.is_some();
      env = nxt;
    }
    if tracked {
      report!(g, cur, "complicated",
        "call to `@Refcount: custom` function {}() with tracked noun \
         arguments, won't analyze", cn);
    }
    return Ok((None, Some(env)));
  }

  //  forward decls may have unnamed params: prefer the definition
  let pcur = cal.definition().unwrap_or(cal);
  let params = pcur.arguments();

  //  pointee-annotated names must name actual parameters
  for pn in sem.pointees.keys() {
    if !params.iter().any(|p| &*p.spelling() == pn.as_str()) {
      report!(g, cur, "annotation",
        "@Refcount: pointee annotation names unknown parameter `{}` of \
         {}()", pn, cn);
    }
  }

  //  phase 1: C evaluates every operand before the call
  let mut evald: Vec<(Cursor, Option<Cursor>, Option<ValId>)> = Vec::new();
  //  pointer args to pointee-annotated params; the bool is whether the
  //  parameter's pointee type is u3_weak (its fills may be u3_none)
  enum PointeeArg {
    Var(VarName, ValId), //  variable slot and its current value
    Hole(ValId),         //  sid of an unfilled deferred slot
  }
  let mut pointee_work: Vec<(Cursor, PointeeArg, PointeeMode, bool)> =
    Vec::new();
  for (i, a) in args.iter().enumerate() {
    //  `&var` handed to an annotated pointer-to-noun parameter: the
    //  pointee clauses say what the callee does through the pointer,
    //  so the address does not escape. Read checks happen now;
    //  consumption and refill apply after all operands evaluate --
    //  the call itself acts on the pointee, not the operand
    if let Some(p) = params.get(i) {
      if let Some(pm) = sem.pointees.get(&*p.spelling()).copied() {
        if is_noun_type(&p.ty()) {
          report!(g, a, "annotation",
            "@Refcount: pointee annotation on noun-typed parameter \
             `{}` of {}(): pointee clauses apply to pointer-to-noun \
             parameters only", p.spelling(), cn);
        }
        //  resolve the argument to the variable slot it points at: a
        //  literal `&var`, or a tracked slot-pointer value (a pointer
        //  parameter or local handed along, e.g. recursion on an
        //  accumulator out-param)
        let au = unwrap_expr(*a);
        let mut parg: Option<PointeeArg> = None;
        if au.kind() == CXCursor_UnaryOperator
          && unary_op(&au).as_deref() == Some("&")
        {
          if let Some(nm) = au.children().first().and_then(decl_ref_name) {
            if let Some((var, vid)) = read_var(&env, &nm) {
              if matches!(env.values.get(&vid), Some(RefcountState::Slot)) {
                report!(g, a, "complicated",
                  "address of slot pointer [{}] handed to the `{}` \
                   parameter of {}(), won't analyze", nm,
                  p.spelling(), cn);
              }
              parg = Some(PointeeArg::Var(var, vid));
            }
          }
          //  &untracked storage: opaque out-pointer, plain walk below
        } else {
          let (v, nxt) = eval_expr(a, env, depth, g)?;
          let Some(nxt) = nxt else { return Ok((None, None)); };
          env = nxt;
          let Some(sid) = v else {
            //  untracked pointer expression (NULL, global): opaque
            evald.push((*a, params.get(i).copied(), None));
            continue;
          };
          match env.values.get(&sid).copied() {
            Some(RefcountState::Slot) => {
              match env.slots.get(&sid).cloned().expect(LI) {
                SlotTarget::Var(var) => {
                  let Some(vid) = env.vars_rev.get(&var).copied() else {
                    report!(g, a, "complicated",
                      "slot pointer target [{}] is out of scope",
                      var.name);
                  };
                  parg = Some(PointeeArg::Var(var, vid));
                }
                //  a callee may COMPLETE a deferred slot, but only by
                //  storing an owned value into it: the hole holds
                //  nothing to read or consume, and a retained fill
                //  would put an uncounted reference inside the cell
                SlotTarget::Hole {..} => {
                  if pm.reads || pm.consumes {
                    report!(g, a, "refcount error",
                      "{}() reads through a pointer to an unfilled \
                       deferred slot", cn);
                  }
                  if pm.fills != Some(FillMode::Transferred) {
                    report!(g, a, "refcount error",
                      "only a `fills transferred` callee may fill a \
                       deferred slot ({}() would not store an owned \
                       value)", cn);
                  }
                  parg = Some(PointeeArg::Hole(sid));
                }
              }
            }
            Some(RefcountState::Poisoned) => {
              report!(g, a, "use-after-free",
                "dead slot pointer [{}] passed to {}(){}",
                env.names(sid), cn, g.why_poisoned(sid));
            }
            _ => {
              report!(g, a, "complicated",
                "cannot resolve the pointer handed to the annotated \
                 `{}` parameter of {}(), won't analyze",
                p.spelling(), cn);
            }
          }
        }
        let weak_ptee = is_weak_type(&p.ty().pointee_type());
        match parg {
          Some(PointeeArg::Var(var, vid)) => {
            if pm.reads || pm.consumes {
              if matches!(env.values.get(&vid), Some(RefcountState::Uninit))
              {
                report!(g, a, "refcount error",
                  "{} parameter of {}() reads uninitialized variable \
                   [{}]",
                  if pm.consumes { "consuming" } else { "reading" },
                  cn, env.names(vid));
              }
              read_check(a, &env, vid, g)?;
              //  the pointee type is the contract: the callee reads
              //  the pointed-at noun as valid
              if !weak_ptee && env.weak.contains(&vid) {
                report!(g, a, "u3_none",
                  "pointer to possibly-none value [{}]{} handed to the \
                   `{}` parameter of {}(): compare against u3_none \
                   first", env.names(vid), g.why_weak(vid),
                  p.spelling(), cn);
              }
            }
            pointee_work.push((*a, PointeeArg::Var(var, vid), pm,
              weak_ptee));
            evald.push((*a, params.get(i).copied(), None));
            continue;
          }
          Some(h @ PointeeArg::Hole(_)) => {
            pointee_work.push((*a, h, pm, weak_ptee));
            evald.push((*a, params.get(i).copied(), None));
            continue;
          }
          None => {}  //  opaque &untracked: plain walk below
        }
      }
    }
    let (v, nxt) = eval_expr(a, env, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    env = nxt;
    evald.push((*a, params.get(i).copied(), v));
  }

  //  phase 2: the callee's per-argument protocol
  let mut pass_vid: Option<ValId> = None;
  let mut cons_vids: Vec<ValId> = Vec::new();
  for (a, p, v) in &evald {
    let is_slot = v.is_some_and(|v|
      matches!(env.values.get(&v), Some(RefcountState::Slot)));
    let Some(p) = p else {
      //  varargs: too ambiguous -- but a slot pointer must not slip in
      if is_slot {
        report!(g, a, "complicated",
          "slot pointer passed to variadic {}(), won't analyze", cn);
      }
      //  except u3i_list &co, whose varargs are all consumed
      if config::VARARG_TRANSFER_FNS.contains(&cn) {
        if let Some(v) = *v {
          env.lose(v, a, g)?;
        }
      }
      continue;
    };
    if !is_noun_type(&p.ty()) {
      //  a pointer to a tracked noun may only go to a parameter whose
      //  pointee behavior is declared
      if is_slot {
        report!(g, a, "complicated",
          "address of a tracked noun handed to parameter `{}` of {}() \
           without a pointee annotation: annotate the callee \
           (`@Refcount: reads|consumes|fills retained|fills transferred \
           `{}``; add `on `c3y`` for u3r_cell-style conditional \
           out-params)",
          p.spelling(), cn, p.spelling());
      }
      //  an owned product handed to a declared non-noun parameter
      //  (e.g. u3a_malloc(u3kb_lent(..))) drops its reference: it stays
      //  an unconsumed orphan and the scope-end sweep reports it
      continue;
    }
    //  the parameter type is the contract: a possibly-u3_none argument
    //  may only flow into a u3_weak parameter
    if !is_weak_type(&p.ty()) {
      if let Some(d) = weak_desc(&env, g, *v, a) {
        report!(g, a, "u3_none",
          "{} passed as `{}`, a {} parameter of {}(): compare \
           against u3_none first", d, p.spelling(),
          p.ty().spelling(), cn);
      }
    }
    let Some(v) = *v else {
      //  untracked argument value (global, literal): nothing to account
      continue;
    };
    let pname = p.spelling();
    match sem.arg_mode(&pname) {
      ArgumentMode::Passthrough => {
        pass_vid = Some(v);
      }
      ArgumentMode::Conslike => {
        //  the argument's counted reference moves into the product;
        //  the name keeps an uncounted view of the noun
        match *env.values.get(&v).expect(LI) {
          RefcountState::Owned {extra: 0} => {
            env.values.insert(v, RefcountState::Borrowed);
            cons_vids.push(v);
          }
          RefcountState::Owned {extra} => {
            env.values.insert(v, RefcountState::Owned {extra: extra - 1});
            cons_vids.push(v);
          }
          RefcountState::Direct => {}
          RefcountState::Borrowed => {
            report!(g, a, "refcount error",
              "cons-like parameter of {}() consumes [{}], but the caller \
               retains ownership: u3k first", cn, env.names(v));
          }
          RefcountState::Uninit => {
            report!(g, a, "refcount error",
              "transfer of uninitialized variable [{}]", env.names(v));
          }
          RefcountState::Poisoned => {
            report!(g, a, "refcount error",
              "transfer of already-consumed value [{}]", env.names(v));
          }
          RefcountState::Slot => {
            report!(g, a, "refcount error",
              "slot pointer [{}] passed as a noun to {}()",
              env.names(v), cn);
          }
          RefcountState::Passthrough => {
            report!(g, a, "refcount error",
              "losing passthrough value [{}]", env.names(v));
          }
        }
      }
      mode => {
        apply_basic_arg_mode(&mut env, mode, v, a, g)?;
      }
    }
  }

  //  pointee consumption: one counted reference of the old pointee is
  //  given away inside the call (transferring read, or u3z of the old
  //  value before a refill -- indistinguishable from out here)
  for (a, parg, pm, _) in &pointee_work {
    if pm.consumes {
      if let PointeeArg::Var(_, vid) = parg {
        env.lose(*vid, a, g)?;
      }
    }
  }

  //  a unifying comparison -- u3r_sing itself, or any function that
  //  can run one over its arguments (nock evaluation, memo/hashtable
  //  key lookups; see config::UNIFYING_FNS): equal interior copies may
  //  be freed and repointed, so borrowed views into the arguments die
  if config::UNIFYING_FNS.contains(&cn) {
    let why = format!("possibly freed by unifying call to {}() at {}",
      cn, loc_str(cur));
    for (_, _, v) in &evald {
      if let Some(v) = *v {
        poison_borrowed_within(&mut env, v, &why, g);
      }
    }
  }

  //  slamming a prepared gate runs arbitrary nock, and the gate may
  //  have captured ANY noun the jet can see (PR #865: roll's gate
  //  captured the list, and unifying equality inside the slam freed
  //  the borrowed tail held across iterations). Parameters are safe --
  //  the caller's counted reference protects those roots -- but every
  //  other borrowed view may be a caller-unprotected interior copy
  if cn == "u3j_gate_slam" {
    let stale: Vec<ValId> = env.values.iter()
      .filter(|(id, st)| matches!(st, RefcountState::Borrowed)
        && !g.param_vids.contains(id))
      .map(|(id, _)| *id)
      .collect();
    let why = format!("possibly freed during u3j_gate_slam() at {}: \
      the gate may unify nouns it captured", loc_str(cur));
    for id in stale {
      g.note_poison(id, why.clone());
      env.values.insert(id, RefcountState::Poisoned);
    }
  }

  //  pointee refill: the pointed-to variable holds a fresh value on
  //  return -- owned for `fills transferred`; an uncounted view for
  //  `fills retained`, tied to the call's noun arguments so consuming
  //  them poisons it. Overwriting an unconsumed owned pointee without
  //  a `consumes` clause leaves a nameless owned orphan that the
  //  scope-end/exit sweeps report as a leak
  for (a, parg, pm, weak_ptee) in &pointee_work {
    let Some(fm) = pm.fills else { continue; };
    //  a conditional fill keys on the call's loobean product; the
    //  enclosing c3y/c3n comparison claims it
    if let Some(on) = pm.fill_on {
      if pm.reads || pm.consumes {
        report!(g, a, "annotation",
          "conditional fill on a parameter of {}() cannot be combined \
           with reads/consumes clauses", cn);
      }
      let PointeeArg::Var(var, prior) = parg else {
        report!(g, a, "complicated",
          "conditional fill of a deferred slot (u3i_defcons hole) by \
           {}(), won't analyze", cn);
      };
      match fm {
        //  the owned fill is deferred until the claiming comparison
        //  and lands on the matching branch only (unclaimed = loud)
        FillMode::Transferred => {
          g.pending_cond_fills.get_or_insert_with(Vec::new)
            .push(PendCondFill {
              var: var.clone(), on, kind: PendKind::Owned,
            });
          continue;
        }
        //  the borrowed fill is applied optimistically, u3r_cell
        //  style: the claiming comparison restores the old binding on
        //  the branch whose product says the fill never happened
        //  (unclaimed optimistic fills simply stay)
        FillMode::Retained => {
          let prior_st = *env.values.get(prior).expect(LI);
          g.pending_cond_fills.get_or_insert_with(Vec::new)
            .push(PendCondFill {
              var: var.clone(), on,
              kind: PendKind::View {prior: *prior, prior_st},
            });
          //  fall through to the unconditional fill below
        }
      }
    }
    match parg {
      PointeeArg::Var(var, _) => {
        env = unbind_var(env, var);
        let rc = match fm {
          FillMode::Transferred => RefcountState::Owned { extra: 0 },
          FillMode::Retained => RefcountState::Borrowed,
        };
        let id = new_val(&mut env, rc, g);
        if matches!(fm, FillMode::Transferred) {
          g.note_owned(id, format!("filled by {}() at {}", cn,
            loc_str(cur)));
        }
        //  a u3_weak* out-param may have been filled with u3_none
        if *weak_ptee {
          env.weak.insert(id);
          g.note_weak(id, format!("filled through a u3_weak pointer \
            by {}() at {}", cn, loc_str(cur)));
        }
        env.bind_decl(var.clone(), id);
        if matches!(fm, FillMode::Retained) {
          for (_, p, v) in &evald {
            if let (Some(p), Some(v)) = (p, v) {
              if is_noun_type(&p.ty()) {
                env.contains.entry(*v).or_default().insert(id);
              }
            }
          }
        }
      }
      //  the callee completed a deferred slot: a fresh owned value went
      //  in and the structure owns it now (phase 1 admits only `fills
      //  transferred` here)
      PointeeArg::Hole(sid) => {
        let SlotTarget::Hole {owner} =
          env.slots.get(sid).cloned().expect(LI)
        else {
          unreachable!("{}", LI);
        };
        let v = new_val(&mut env, RefcountState::Borrowed, g);
        env.contains.entry(owner).or_default().insert(v);
        env.slots.remove(sid);
        g.note_poison(*sid,
          format!("deferred slot filled by {}() at {}", cn,
            loc_str(cur)));
        env.values.insert(*sid, RefcountState::Poisoned);
      }
    }
  }

  //  the declared return type carries the none-ness contract: a
  //  u3_weak product may be u3_none; any other noun return type
  //  promises a valid noun (the callee's own body check enforces it),
  //  so a passthrough product is proven valid by the signature --
  //  this is how u3x_good blesses its u3_weak argument
  let weak_prod = is_weak_type(&cal.result_type());
  let mark_weak = |env: &mut Env, g: &mut Gen, id: ValId| {
    if weak_prod {
      env.weak.insert(id);
      g.note_weak(id, format!("u3_weak product of {}() at {}", cn,
        loc_str(cur)));
    }
  };
  if let Some(v) = pass_vid {
    if !weak_prod && is_noun_type(&cal.result_type()) {
      refine_valid(&mut env, v);
    }
    return Ok((Some(v), Some(env)));
  }
  match &sem.product {
    ProductMode::NonNoun => Ok((None, Some(env))),
    ProductMode::Transfer => {
      let id = new_val(&mut env, RefcountState::Owned {extra: 0}, g);
      g.note_owned(id, format!("created by {}() at {}", cn, loc_str(cur)));
      mark_weak(&mut env, g, id);
      for v in cons_vids {
        env.contains.entry(id).or_default().insert(v);
      }
      Ok((Some(id), Some(env)))
    }
    ProductMode::Retain => {
      //  the product is (a sub-noun of) one of the arguments -- except
      //  for container lookups (u3h_git), whose product borrows from
      //  the untracked table, not the key
      let id = new_val(&mut env, RefcountState::Borrowed, g);
      mark_weak(&mut env, g, id);
      if !config::UNTIED_RETAIN_FNS.contains(&cn) {
        for (_, p, v) in &evald {
          if let (Some(p), Some(v)) = (p, v) {
            if is_noun_type(&p.ty()) {
              env.contains.entry(*v).or_default().insert(id);
            }
          }
        }
      }
      for v in cons_vids {
        env.contains.entry(id).or_default().insert(v);
      }
      Ok((Some(id), Some(env)))
    }
    ProductMode::Direct => {
      let id = new_val(&mut env, RefcountState::Direct, g);
      mark_weak(&mut env, g, id);
      Ok((Some(id), Some(env)))
    }
    ProductMode::Passthrough => {
      //  annotated passthrough, but the argument was untracked
      Ok((None, Some(env)))
    }
  }
}

/// `lhs = rhs`. A tracked noun variable is rebound (pure alias update);
/// stores to anything else retain by default and transfer only under an
/// enclosing nameless `@Refcount: assert transfer` blessing.
fn eval_assign(cur: &Cursor, lhs: &Cursor, rhs: &Cursor, env: Env,
  depth: u32, g: &mut Gen) -> R<(Option<ValId>, Option<Env>)>
{
  let (rvid, nxt) = eval_expr(rhs, env, depth, g)?;
  let Some(mut env) = nxt else { return Ok((None, None)); };

  //  `*p = rhs` through a tracked slot pointer: a store to the target
  //  variable, or a deferred-cons fill
  let lu = unwrap_expr(*lhs);
  if lu.kind() == CXCursor_UnaryOperator
    && unary_op(&lu).as_deref() == Some("*")
  {
    if let Some(pname) = lu.children().first().and_then(decl_ref_name) {
      if let Some((_, sid)) = read_var(&env, &pname) {
        match env.values.get(&sid).copied() {
          Some(RefcountState::Slot) => {
            //  the pointee type is the contract: only a u3_weak slot
            //  may receive a possibly-u3_none value
            if is_noun_type(&lu.ty()) && !is_weak_type(&lu.ty()) {
              if let Some(d) = weak_desc(&env, g, rvid, rhs) {
                report!(g, cur, "u3_none",
                  "{} stored through [{}], a pointer to {}: compare \
                   against u3_none first",
                  d, pname, lu.ty().spelling());
              }
            }
            return store_slot(cur, env, sid, rvid, g);
          }
          Some(RefcountState::Poisoned) => {
            report!(g, cur, "use-after-free",
              "store through dead slot pointer [{}]{}", env.names(sid),
              g.why_poisoned(sid));
          }
          _ => {}  //  opaque pointer: the untracked-memory store below
        }
      }
    }
  }

  //  declared types are contracts: a possibly-u3_none value may only
  //  be assigned to a u3_weak lvalue (tracked variable, lazy-tracked
  //  local, or untracked storage alike)
  let lty = lhs.ty();
  let weak_rhs = weak_desc(&env, g, rvid, rhs);
  if is_noun_type(&lty) && !is_weak_type(&lty) {
    if let Some(d) = &weak_rhs {
      report!(g, cur, "u3_none",
        "{} assigned to {} lvalue: declare it u3_weak, or compare \
         against u3_none first", d, lty.spelling());
    }
  }

  let lname = decl_ref_name(lhs);
  if let Some(ln) = &lname {
    if let Some((var, old)) = read_var(&env, ln) {
      if rvid == Some(old) {
        return Ok((rvid, Some(env)));  //  x = x
      }
      env = unbind_var(env, &var);
      let id = bind_value(&mut env, var, rvid, g);
      //  a u3_none literal assigned to a u3_weak variable: the fresh
      //  value is a known none
      if rvid.is_none() && weak_rhs.is_some() {
        env.weak.insert(id);
        g.note_weak(id, format!("u3_none literal at {}", loc_str(cur)));
      }
      //  a noun value narrowed into a sub-noun-width integer type is
      //  necessarily a direct atom
      if rvid.is_some() && is_direct_type(&lhs.ty()) {
        refine_direct(&mut env, id);
      }
      return Ok((Some(id), Some(env)));
    }
    //  a local the declaration pass did not track (noun value kept in a
    //  c3_w etc.): start tracking at the first noun-valued store
    if rvid.is_some() && is_local_lvalue(lhs) {
      let id = bind_value(&mut env, VarName {name: ln.clone(), depth},
        rvid, g);
      if is_direct_type(&lhs.ty()) {
        refine_direct(&mut env, id);
      }
      return Ok((Some(id), Some(env)));
    }
  }

  //  store to untracked storage (global, *p, p->f, arr[i]): evaluate
  //  the lvalue expression for its effects, then apply store semantics
  let (_, nxt) = eval_expr(lhs, env, depth, g)?;
  let Some(mut env) = nxt else { return Ok((None, None)); };

  if let Some(v) = rvid {
    //  a slot pointer stashed in memory could be written through later,
    //  invisibly to the tracker -- except a pointer to a PROVEN-DIRECT
    //  atom, which is a plain word/byte view of the value (sew/aor-
    //  style buffer readers): no refcount to corrupt through it
    if matches!(env.values.get(&v), Some(RefcountState::Slot))
      && !slot_is_direct_view(&env, v)
    {
      report!(g, cur, "complicated",
        "slot pointer stored to untracked memory, won't analyze");
    }
    if g.store_transfers() {
      env.lose(v, cur, g)?;
    } else if env.vars.get(&v).is_none_or(|l| l.is_empty())
      && matches!(env.values.get(&v), Some(RefcountState::Owned {..}))
      && !g.sem.noreturn
    {
      report!(g, cur, "leak",
        "owned value stored to untracked memory; stores retain by \
         default, wrap in `{{ // @Refcount: assert transfer }}` if this \
         store consumes the reference");
    }
  }
  Ok((rvid, Some(env)))
}

//  --------------------------------------------------------------------------
//  conditions

/// Returns (true-branch env, false-branch env); None for a branch that
/// is statically impossible or crashed.
fn eval_cond(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<(Option<Env>, Option<Env>)>
{
  let cur = unwrap_expr(*cur);

  //  constant condition: while(1) never falls out, etc.
  if let Some(lit) = int_literal_value(&cur) {
    return Ok(if lit != 0 { (Some(env), None) } else { (None, Some(env)) });
  }

  let k = cur.kind();

  //  see through __builtin_expect (c3_likely/c3_unlikely)
  if k == CXCursor_CallExpr {
    if let Some(r) = cur.referenced() {
      if &*r.spelling() == "__builtin_expect" {
        if let Some(a0) = cur.arguments().first() {
          return eval_cond(a0, env, depth, g);
        }
      }
    }
  }

  if k == CXCursor_UnaryOperator && unary_op(&cur).as_deref() == Some("!") {
    if let Some(k0) = cur.children().first() {
      let (t, f) = eval_cond(k0, env, depth, g)?;
      return Ok((f, t));
    }
  }

  if k == CXCursor_BinaryOperator {
    let op = cur.binop_kind();
    let kids = cur.children();

    if op == binop::LAND && kids.len() == 2 {
      let (lt, lf) = eval_cond(&kids[0], env, depth, g)?;
      let (tt, tf) = match lt {
        Some(e) => eval_cond(&kids[1], e, depth, g)?,
        None => (None, None),
      };
      return Ok((tt, mayb_join(cur.location(), lf, tf, g)?));
    }
    if op == binop::LOR && kids.len() == 2 {
      let (lt, lf) = eval_cond(&kids[0], env, depth, g)?;
      let (ft, ff) = match lf {
        Some(e) => eval_cond(&kids[1], e, depth, g)?,
        None => (None, None),
      };
      return Ok((mayb_join(cur.location(), lt, ft, g)?, ff));
    }

    if (op == binop::EQ || op == binop::NE) && kids.len() == 2 {
      let (lv, nxt) = eval_expr(&kids[0], env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      let (rv, nxt) = eval_expr(&kids[1], nxt, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      //  a tracked value (owned temporaries included) compared against
      //  a direct literal IS that direct atom on the equal branch --
      //  `if (u3qb_lent(shape) != 2) bail;` leaks nothing on the path
      //  that survives (the refinement dissolves the owned temporary);
      //  a dropped owned temporary on the other branch surfaces in the
      //  scope-end sweep
      let lit_l = int_literal_value(&kids[0]);
      let lit_r = int_literal_value(&kids[1]);
      let direct_lit = |l: Option<u64>| l.is_some_and(|v|
        v <= config::DIRECT_MAX || v == config::U3_NONE);
      let mut eq_vid: Option<ValId> = None;
      for (v, other_lit) in [(lv, lit_r), (rv, lit_l)] {
        if v.is_some() && direct_lit(other_lit) {
          eq_vid = v;
        }
      }
      //  facts resolved on the post-evaluation env, so an assignment
      //  inside the comparison refines the rebound value
      let fact = guard_fact(&kids[0], &kids[1], &nxt);
      let mut te = nxt.clone();
      let mut fe = nxt;
      //  claim a conditional-fill callee's deferred fills: the branch
      //  whose product matches the `on` loobean gets the owned fill,
      //  the other branch leaves the variable untouched
      if let Some(pends) = g.pending_cond_fills.take() {
        let loob = [lit_l, lit_r].iter().flatten()
          .find(|v| **v == config::C3Y || **v == config::C3N)
          .copied();
        //  not a loobean comparison: optimistic view fills simply
        //  stay; an unclaimed OWNED fill is a loud error
        if loob.is_none()
          && pends.iter().any(|p| matches!(p.kind, PendKind::Owned))
        {
          report!(g, &cur, "complicated",
            "a conditional-fill call must be compared against c3y/c3n \
             directly");
        }
        //  product on the TRUE branch: l for ==, the other loobean
        //  for !=
        let l = loob.unwrap_or(config::C3Y);
        let t_is_y = (op == binop::EQ) == (l == config::C3Y);
        for p in pends.into_iter().filter(|_| loob.is_some()) {
          match p.kind {
            //  deferred owned fill: lands on the matching branch only
            PendKind::Owned => {
              let fill_env = if p.on == t_is_y { &mut te } else { &mut fe };
              let mut env2 = std::mem::take(fill_env);
              env2 = unbind_var(env2, &p.var);
              let id = new_val(&mut env2,
                RefcountState::Owned {extra: 0}, g);
              g.note_owned(id, format!("conditional fill claimed at {}",
                loc_str(&cur)));
              env2.bind_decl(p.var.clone(), id);
              *fill_env = env2;
            }
            //  optimistic destructurer fill: already applied; restore
            //  the old binding AND its state on the branch where the
            //  fill never happened
            PendKind::View {prior, prior_st} => {
              let undo_env = if p.on == t_is_y { &mut fe } else { &mut te };
              let mut env2 = std::mem::take(undo_env);
              env2 = unbind_var(env2, &p.var);
              env2.values.insert(prior, prior_st);
              env2.bind_decl(p.var.clone(), prior);
              *undo_env = env2;
            }
          }
        }
      }
      if let Some(vid) = eq_vid {
        let eq_env = if op == binop::EQ { &mut te } else { &mut fe };
        refine_direct(eq_env, vid);
      }
      //  none-ness refinement: unequal to u3_none proves a valid noun;
      //  equal to any other literal (a valid noun) does too. A
      //  comparison against a tracked proven-valid value proves the
      //  equal branch valid as well
      for (v, other_lit) in [(lv, lit_r), (rv, lit_l)] {
        let (Some(v), Some(l)) = (v, other_lit) else { continue; };
        if l == config::U3_NONE {
          let ne_env = if op == binop::EQ { &mut fe } else { &mut te };
          refine_valid(ne_env, v);
        } else {
          let eq_env = if op == binop::EQ { &mut te } else { &mut fe };
          refine_valid(eq_env, v);
        }
      }
      if let (Some(l), Some(r), None, None) = (lv, rv, lit_l, lit_r) {
        let eq_env = if op == binop::EQ { &mut te } else { &mut fe };
        if !eq_env.weak.contains(&l) { refine_valid(eq_env, r); }
        else if !eq_env.weak.contains(&r) { refine_valid(eq_env, l); }
      }
      if let Some((vid, eq_direct, ne_direct)) = fact {
        let (t_dir, f_dir) = if op == binop::EQ {
          (eq_direct, ne_direct)
        } else {
          (ne_direct, eq_direct)
        };
        if t_dir { refine_direct(&mut te, vid); }
        if f_dir { refine_direct(&mut fe, vid); }
      }
      return Ok((Some(te), Some(fe)));
    }

    if matches!(op, binop::LT | binop::GT | binop::LE | binop::GE)
      && kids.len() == 2
    {
      let (_, nxt) = eval_expr(&kids[0], env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      let (_, nxt) = eval_expr(&kids[1], nxt, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      let facts = bound_fact(op, &kids[0], &kids[1], &nxt);
      let mut te = nxt.clone();
      let mut fe = nxt;
      for (vid, on_true) in facts {
        if on_true { refine_direct(&mut te, vid); }
        else { refine_direct(&mut fe, vid); }
      }
      return Ok((Some(te), Some(fe)));
    }
  }

  //  bare truthiness of a tracked variable: the false branch implies
  //  the value is 0, a direct atom
  if k == CXCursor_DeclRefExpr {
    if let Some((_, vid)) = read_var(&env, &cur.spelling()) {
      read_check(&cur, &env, vid, g)?;
      let te = env.clone();
      let mut fe = env;
      refine_direct(&mut fe, vid);
      return Ok((Some(te), Some(fe)));
    }
  }

  //  generic condition: effects only, no refinement
  let (_, nxt) = eval_expr(&cur, env, depth, g)?;
  let Some(nxt) = nxt else { return Ok((None, None)); };
  Ok((Some(nxt.clone()), Some(nxt)))
}

/// Resolve a tracked value from an expression WITHOUT evaluating it: a
/// plain variable name, or a u3h/u3t chain over one -- resolved through
/// the view cache that the condition's own evaluation just populated
/// (facts are computed on the post-evaluation env).
fn resolve_tracked_expr(env: &Env, cur: &Cursor) -> Option<ValId> {
  let u = unwrap_expr(*cur);
  if let Some(n) = decl_ref_name(&u) {
    return read_var(env, &n).map(|(_, v)| v);
  }
  //  *p through a live slot pointer: the target variable's value, so
  //  a guard over a pointee (`u3a_is_cat(*q_octs)`) refines it
  if u.kind() == CXCursor_UnaryOperator
    && unary_op(&u).as_deref() == Some("*")
  {
    let pv = resolve_tracked_expr(env, u.children().first()?)?;
    if matches!(env.values.get(&pv), Some(RefcountState::Slot)) {
      if let Some(SlotTarget::Var(var)) = env.slots.get(&pv) {
        return env.vars_rev.get(var).copied();
      }
    }
    return None;
  }
  if u.kind() == CXCursor_CallExpr {
    if let Some(r) = u.referenced() {
      let tail = match &*r.spelling() {
        "u3a_h" => false,
        "u3a_t" => true,
        _ => return None,
      };
      let mut args = u.arguments();
      if args.is_empty() {
        args = u.children().into_iter().skip(1).collect();
      }
      let parent = resolve_tracked_expr(env, args.first()?)?;
      return env.views.get(&(parent, tail)).copied();
    }
  }
  None
}

/// For a comparison a==b, the (value, direct-when-equal,
/// direct-when-unequal) refinement. Both branches matter:
/// `c3n == u3a_is_cat(x)` proves x direct on the NOT-equal branch.
fn guard_fact(a: &Cursor, b: &Cursor, env: &Env)
  -> Option<(ValId, bool, bool)>
{
  for (x, y) in [(a, b), (b, a)] {
    let lit = int_literal_value(x);
    let mut name = decl_ref_name(y);
    if name.is_none() {
      //  look through an assignment: (name = expr)
      let yu = unwrap_expr(*y);
      if yu.kind() == CXCursor_BinaryOperator
        && yu.binop_kind() == binop::ASSIGN
      {
        name = yu.children().first().and_then(decl_ref_name);
      }
    }
    if let (Some(l), Some(n)) = (lit, &name) {
      if let Some((_, vid)) = read_var(env, n) {
        //  equal to a direct literal (or the u3_none sentinel) means
        //  direct on the equal branch
        if (l <= config::DIRECT_MAX || l == config::U3_NONE)
          && refinable(env, vid)
        {
          return Some((vid, true, false));
        }
        return None;
      }
    }
    //  c3y/c3n == u3a_is_cat/dog(var)
    let yc = unwrap_expr(*y);
    if let Some(l) = lit {
      if yc.kind() == CXCursor_CallExpr {
        if let Some(r) = yc.referenced() {
          if let Some(kind) = config::guard_kind(&r.spelling()) {
            let mut gargs = yc.arguments();
            if gargs.is_empty() {
              gargs = yc.children().into_iter().skip(1).collect();
            }
            let Some(vid) = gargs.first()
              .and_then(|ga| resolve_tracked_expr(env, ga))
              else { return None; };
            if kind != "cat" && kind != "dog" {
              return None;  //  is_atom/is_cell etc. don't imply direct
            }
            if !refinable(env, vid) {
              return None;
            }
            //  the guard reads c3y exactly when the value is a cat
            let direct_on_yes = kind == "cat";
            let (eq_d, ne_d) = if l == config::C3Y {
              (direct_on_yes, !direct_on_yes)
            } else if l == config::C3N {
              (!direct_on_yes, direct_on_yes)
            } else {
              return None;
            };
            return Some((vid, eq_d, ne_d));
          }
        }
      }
    }
  }
  None
}

/// For a relational comparison, (value, branch) refinements: the value
/// is provably a direct atom (bounded below 2^31) on that branch.
fn bound_fact(op: i32, a: &Cursor, b: &Cursor, env: &Env)
  -> Vec<(ValId, bool)>
{
  let resolve = |c: &Cursor| decl_ref_name(c)
    .and_then(|n| read_var(env, &n))
    .map(|(_, id)| id);
  let va = resolve(a);
  let vb = resolve(b);
  let lit_a = int_literal_value(a);
  let lit_b = int_literal_value(b);
  let is_dir = |v: ValId| {
    matches!(env.values.get(&v), Some(RefcountState::Direct))
  };

  if let (Some(va), Some(lb)) = (va, lit_b) {
    //  var < lit (true), var <= lit (true),
    //  var > lit (false), var >= lit (false)
    let on_true = matches!(op, binop::LT | binop::LE);
    let bound_incl = matches!(op, binop::LE | binop::GT);
    let limit = if bound_incl { config::DIRECT_MAX }
                else { config::DIRECT_MAX + 1 };
    if lb <= limit && refinable(env, va) {
      return vec![(va, on_true)];
    }
    return vec![];
  }
  if let (Some(vb), Some(la)) = (vb, lit_a) {
    //  lit > var (true), lit >= var (true),
    //  lit < var (false), lit <= var (false)
    let on_true = matches!(op, binop::GT | binop::GE);
    let bound_incl = matches!(op, binop::GE | binop::LT);
    let limit = if bound_incl { config::DIRECT_MAX }
                else { config::DIRECT_MAX + 1 };
    if la <= limit && refinable(env, vb) {
      return vec![(vb, on_true)];
    }
    return vec![];
  }
  if let (Some(va), Some(vb)) = (va, vb) {
    //  var-vs-var: on the branch where x <= y, a direct y bounds x
    //  below 2^31 (this is what verifies the c3_min pattern)
    let (small_t, big_t) = if matches!(op, binop::LT | binop::LE) {
      (va, vb)  //  true: a bounded by b
    } else {
      (vb, va)  //  a > b: true: b <= a
    };
    let mut facts = Vec::new();
    if is_dir(big_t) && refinable(env, small_t) {
      facts.push((small_t, true));
    }
    if is_dir(small_t) && refinable(env, big_t) {
      facts.push((big_t, false));
    }
    return facts;
  }
  vec![]
}

/// States of one value on two joining paths. Uninit loses to anything:
/// compileable, sane code initializes a variable on every path that
/// later reads it, so the initialized side is the truth. Direct is a
/// branch-local refinement ("we know more here"), it dissolves into the
/// other side. Poisoned absorbs Borrowed so a branch-local consumption
/// is reported at the next USE instead of at every join (conditional
/// u3r_sing depends on this). Diverging counted ownership is
/// irreconcilable here; join() adds one context-dependent rule for
/// split owned groups.
fn join_states(rc1: RefcountState, rc2: RefcountState)
  -> Option<RefcountState>
{
  use RefcountState::*;
  match (rc1, rc2) {
    (a, b) if a == b => Some(a),
    (Uninit, x) | (x, Uninit) => Some(x),
    (Direct, x) | (x, Direct) => match x {
      Borrowed => Some(Borrowed),
      Owned {extra} => Some(Owned {extra}),
      Poisoned => Some(Poisoned),
      _ => None,
    },
    (Poisoned, Borrowed) | (Borrowed, Poisoned) => Some(Poisoned),
    _ => None,
  }
}

/// Join two environments at a control-flow merge. Names are the ground
/// truth: both sides must track the same names, and each name lands in
/// the joined value determined by BOTH sides' placements (the meet of
/// the two alias partitions). Extra rules:
///   - a name Uninit on exactly one side adopts the other side's
///     placement (sane compiled code initializes before use);
///   - names regroup freely across uncounted values;
///   - an owned group that one side splits must keep its count in
///     exactly one fragment (the other side says which); the remaining
///     fragments become uncounted views.
fn join(loc: Loc, env1: Env, env2: Env, g: &mut Gen) -> R<Env>
{
  join_l(loc, "", env1, "", env2, g)
}

/// Human name of a joining path from its label ("one path" fallbacks
/// keep unlabeled call sites readable).
fn side_str(lab: &str, fallback: &str) -> String {
  if lab.is_empty() {
    fallback.to_string()
  } else {
    format!("the path through [{}]", lab)
  }
}

fn join_l(loc: Loc, lab1: &str, env1: Env, lab2: &str, env2: Env,
  g: &mut Gen) -> R<Env>
{
  use RefcountState::*;

  if env1.vars_rev.len() != env2.vars_rev.len()
    || env1.vars_rev.keys().any(|n| !env2.vars_rev.contains_key(n))
  {
    // different sets of tracked names in envs, we are already done
    //
    let only = |a: &Env, b: &Env| -> String {
      let mut ns: Vec<&str> = a.vars_rev.keys()
        .filter(|n| !b.vars_rev.contains_key(*n))
        .map(|n| n.name.as_ref())
        .collect();
      ns.sort();
      ns.dedup();
      ns.join(", ")
    };
    let o1 = only(&env1, &env2);
    let o2 = only(&env2, &env1);
    let mut parts = Vec::new();
    if !o1.is_empty() {
      parts.push(format!("[{}] tracked on {} only", o1,
        side_str(lab1, "one path")));
    }
    if !o2.is_empty() {
      parts.push(format!("[{}] tracked on {} only", o2,
        side_str(lab2, "the other path")));
    }
    report_loc!(g, loc, "refcount error",
      "joining paths disagree about which variables are live: {} -- \
       does one path jump over the declaration (switch fallthrough, \
       missing break, goto past a decl)?", parts.join("; "));
  }

  //  an owned value that one path still names but the other has
  //  already unbound leaked on the unbinding path (an overwrite in a
  //  bare branch: no scope end runs before this join). Checked here
  //  because the fragment pass below would silently absorb the
  //  orphaned count into the named side's fragment
  for (a, b, lab) in [(&env1, &env2, lab1), (&env2, &env1, lab2)] {
    if g.sem.noreturn {
      break;
    }
    let mut leaked: Vec<ValId> = a.values.iter()
      .filter(|(id, st)| matches!(st, Owned {..})
        && !a.vars.contains_key(*id)
        && b.vars.contains_key(*id))
      .map(|(id, _)| *id)
      .collect();
    leaked.sort();
    if let Some(id) = leaked.first() {
      report_loc!(g, loc, "leak",
        "owned reference in [{}] overwritten on {} without being \
         consumed{}", b.names(*id), side_str(lab, "one path"),
        g.where_owned(*id));
    }
  }

  let mut names: Vec<VarName> = env1.vars_rev.keys().cloned().collect();
  names.sort();

  let st1 = |v: ValId| -> RefcountState { *env1.values.get(&v).expect(LI) };
  let st2 = |v: ValId| -> RefcountState { *env2.values.get(&v).expect(LI) };

  //  A join of an Uninit value with any other value produces the latter: it
  //  is assumed that the analyzed code at least compiles.
  //
  //  rep_X helpers are used in cases when a value in env_X is Uninit. It takes
  //  val_Y: ValId from env_Y that is bound to the same name as the Uninit
  //  value in question, and produces a ValId from env_X that is bound to one of
  //  val_Y's names in env_Y, if that value is initialized in env_X. The
  //  choice is arbitrary as the name with Uninit value is not read in env_X.
  //  The sort is for determinism.
  //
  let rep1 = |val2: ValId| -> Option<ValId> {
    let mut ms: Vec<&VarName> = env2.vars.get(&val2)
      .map(|s| s.iter().collect()).unwrap_or_default();
    ms.sort();
    ms.into_iter()
      .map(|m| *env1.vars_rev.get(m).expect(LI))
      .find(|v| !matches!(st1(*v), Uninit))
  };
  let rep2 = |val1: ValId| -> Option<ValId> {
    let mut ms: Vec<&VarName> = env1.vars.get(&val1)
      .map(|s| s.iter().collect()).unwrap_or_default();
    ms.sort();
    ms.into_iter()
      .map(|m| *env2.vars_rev.get(m).expect(LI))
      .find(|v| !matches!(st2(*v), Uninit))
  };

  //  fragments of the meet partition: names keyed by their placement on
  //  both sides
  let mut frags: BTreeMap<(Option<ValId>, Option<ValId>), Vec<VarName>> =
    BTreeMap::new();
  for n in &names {
    let v1 = *env1.vars_rev.get(n).expect(LI);
    let v2 = *env2.vars_rev.get(n).expect(LI);
    let (s1, s2) = (st1(v1), st2(v2));
    let k1 = if matches!(s1, Uninit) && !matches!(s2, Uninit) {
      rep1(v2)
    } else {
      Some(v1)
    };
    let k2 = if matches!(s2, Uninit) && !matches!(s1, Uninit) {
      rep2(v1)
    } else {
      Some(v2)
    };
    frags.entry((k1, k2)).or_default().push(n.clone());
  }

  //  fragments per group: a split group's fragments need fresh ids, and
  //  a split owned group resolves its count by what the other side says
  let mut k1_n: HashMap<ValId, u32> = HashMap::new();
  let mut k2_n: HashMap<ValId, u32> = HashMap::new();
  for (k1, k2) in frags.keys() {
    if let Some(v) = k1 {
      *k1_n.entry(*v).or_insert(0) += 1;
    }
    if let Some(v) = k2 {
      *k2_n.entry(*v).or_insert(0) += 1;
    }
  }

  let mut vars_joined     = <IHashMap<ValId, IHashSet<VarName>>>::new();
  let mut vars_rev_joined = <IHashMap<VarName, ValId>>::new();
  let mut values_joined   = <IHashMap<ValId, RefcountState>>::new();
  let mut slots_joined    = <IHashMap<ValId, SlotTarget>>::new();
  let mut map1: HashMap<ValId, Vec<ValId>> = HashMap::new();
  let mut map2: HashMap<ValId, Vec<ValId>> = HashMap::new();
  let mut kept1: HashMap<ValId, u32> = HashMap::new();
  let mut kept2: HashMap<ValId, u32> = HashMap::new();
  //  every noun fragment with its source groups and their states, for
  //  the count-rescue pass below
  let mut frag_info: Vec<(ValId, Option<ValId>, Option<ValId>,
    RefcountState, RefcountState)> = Vec::new();

  //  slot-pointer fragments join AFTER the noun fragments: hole owners
  //  are noun values, and reconciling two holes needs the owners'
  //  joined images (map1/map2)
  let slot_frag = |k1: &Option<ValId>, k2: &Option<ValId>| -> bool {
    matches!(k1.map(|v| st1(v)), Some(Slot))
      || matches!(k2.map(|v| st2(v)), Some(Slot))
  };

  for ((k1, k2), ns) in frags.iter()
    .filter(|((k1, k2), _)| !slot_frag(k1, k2))
  {
    let s1 = k1.map(|v| st1(v)).unwrap_or(Uninit);
    let s2 = k2.map(|v| st2(v)).unwrap_or(Uninit);
    let split1 = matches!(s1, Owned {..})
      && k1.map(|v| k1_n[&v] > 1).unwrap_or(false);
    let split2 = matches!(s2, Owned {..})
      && k2.map(|v| k2_n[&v] > 1).unwrap_or(false);
    //  a fragment of a split owned group keeps the count only where the
    //  other side also holds it; the rest are uncounted views, or
    //  already consumed on the other path (`u3z(som)` vs `pro = som`).
    //  Conservation below demands exactly one surviving count, so the
    //  unsplit `if (c) u3z(x);` bug stays an error.
    let rc = match (s1, s2) {
      (Owned {..}, Direct) if split1 => Borrowed,
      (Direct, Owned {..}) if split2 => Borrowed,
      (Owned {..}, Borrowed) | (Borrowed, Owned {..}) => Borrowed,
      (Owned {..}, Poisoned) | (Poisoned, Owned {..}) => Poisoned,
      _ => match join_states(s1, s2) {
        Some(rc) => rc,
        None => {
          let who: Vec<&str> = ns.iter().map(|n| n.name.as_ref()).collect();
          report_loc!(g, loc, "refcount error",
            "joining branches have conflicting refcounts for [{}] \
             ({} on {}, {} on {})",
            who.join(", "),
            state_word(s1), side_str(lab1, "one path"),
            state_word(s2), side_str(lab2, "the other path"))
        }
      },
    };
    let id = match k1 {
      Some(v) if k1_n[v] == 1 => *v,
      _ => { let id = g.id_gen; g.id_gen += 1; id }
    };
    values_joined.insert(id, rc);
    //  a fresh fragment id inherits the creation site of its source
    if matches!(rc, Owned {..}) {
      let w = [k1, k2].iter().filter_map(|k| k.as_ref())
        .find_map(|v| g.owned_at.get(v).cloned());
      if let Some(w) = w {
        g.note_owned(id, w);
      }
    }
    vars_joined.insert(id, ns.iter().cloned().collect());
    for n in ns {
      vars_rev_joined.insert(n.clone(), id);
    }
    if let Some(v) = k1 {
      map1.entry(*v).or_default().push(id);
      if matches!(rc, Owned {..}) { *kept1.entry(*v).or_insert(0) += 1; }
    }
    if let Some(v) = k2 {
      map2.entry(*v).or_default().push(id);
      if matches!(rc, Owned {..}) { *kept2.entry(*v).or_insert(0) += 1; }
    }
    frag_info.push((id, *k1, *k2, s1, s2));
  }

  //  count rescue: a split owned group whose count landed in NO
  //  fragment lost it to the other path consuming the value AND
  //  rebinding every shared name -- `if (c) { u3z(x); y = u3_nul; }
  //  else { y = x; }` is legal, the count lives on through [y] on the
  //  else path. The single fragment the other side has NOT consumed
  //  keeps the count; ambiguity (several candidates) stays an error
  for (env, kept, mine) in
    [(&env1, &mut kept1, 1u8), (&env2, &mut kept2, 2u8)]
  {
    let mut groups: Vec<ValId> = env.vars.keys().copied().collect();
    groups.sort();
    for v in groups {
      let Some(Owned {extra}) = env.values.get(&v).copied() else {
        continue;
      };
      if kept.get(&v).copied().unwrap_or(0) != 0 {
        continue;
      }
      let cands: Vec<ValId> = frag_info.iter()
        .filter(|(id, k1, k2, s1, s2)| {
          let (own_k, oth_s) = if mine == 1 { (k1, s2) } else { (k2, s1) };
          *own_k == Some(v)
            && !matches!(oth_s, Poisoned)
            && matches!(values_joined.get(id), Some(Borrowed))
        })
        .map(|(id, ..)| *id)
        .collect();
      if let [id] = cands[..] {
        values_joined.insert(id, Owned {extra});
        if let Some(w) = g.owned_at.get(&v).cloned() {
          g.note_owned(id, w);
        }
        kept.insert(v, 1);
      }
    }
  }

  //  pass 2: slot pointers. A name must point at "the same place" on
  //  both sides: the same variable slot, or holes of the same (joined)
  //  owner -- hole identity itself is iteration-local, only the owner
  //  matters. `&var` on one path vs a hole on the other is the
  //  first-iteration shape (`lit = &pro` before any defcons): the var
  //  must still be Uninit on the &var side and hold the hole's owner on
  //  the other. Anything irreconcilable joins as a Poisoned pointer
  //  (using it reports); the conservation check below still demands
  //  every hole OBLIGATION survives.
  let img = |o: ValId, map: &HashMap<ValId, Vec<ValId>>| -> Option<ValId> {
    match map.get(&o) {
      None => Some(o),  //  location-less owner keeps its identity
      Some(is) if is.len() == 1 => Some(is[0]),
      Some(_) => None,  //  owner split across fragments: ambiguous
    }
  };
  for ((k1, k2), ns) in frags.iter()
    .filter(|((k1, k2), _)| slot_frag(k1, k2))
  {
    let s1 = k1.map(|v| st1(v)).unwrap_or(Uninit);
    let s2 = k2.map(|v| st2(v)).unwrap_or(Uninit);
    let t1 = k1.and_then(|v| env1.slots.get(&v));
    let t2 = k2.and_then(|v| env2.slots.get(&v));
    use SlotTarget::*;
    let target: Option<SlotTarget> = match (s1, s2) {
      (Slot, Slot) => match (t1.expect(LI), t2.expect(LI)) {
        (Var(a), Var(b)) if a == b => Some(Var(a.clone())),
        (Hole {owner: o1}, Hole {owner: o2}) => {
          match (img(*o1, &map1), img(*o2, &map2)) {
            (Some(i1), Some(i2)) if i1 == i2 => Some(Hole {owner: i1}),
            _ => None,
          }
        }
        (Var(v), Hole {owner: o2}) => {
          let uninit1 = env1.vars_rev.get(v)
            .is_some_and(|w| matches!(st1(*w), Uninit));
          if uninit1 && env2.vars_rev.get(v) == Some(o2) {
            img(*o2, &map2).map(|i| Hole {owner: i})
          } else { None }
        }
        (Hole {owner: o1}, Var(v)) => {
          let uninit2 = env2.vars_rev.get(v)
            .is_some_and(|w| matches!(st2(*w), Uninit));
          if uninit2 && env1.vars_rev.get(v) == Some(o1) {
            img(*o1, &map1).map(|i| Hole {owner: i})
          } else { None }
        }
        _ => None,
      },
      //  a name Uninit on one side adopts the other side's slot
      (Slot, Uninit) => t1.expect(LI).clone().remap(|o| img(o, &map1)),
      (Uninit, Slot) => t2.expect(LI).clone().remap(|o| img(o, &map2)),
      //  stale (filled/dead) on one path: reported at the next use
      (Slot, Poisoned) | (Poisoned, Slot) => None,
      _ => {
        let who: Vec<&str> = ns.iter().map(|n| n.name.as_ref()).collect();
        report_loc!(g, loc, "refcount error",
          "joining branches disagree about [{}]: slot pointer on one \
           path, noun on the other", who.join(", "));
      }
    };
    let id = match k1 {
      Some(v) if k1_n[v] == 1 => *v,
      _ => { let id = g.id_gen; g.id_gen += 1; id }
    };
    match target {
      Some(t) => {
        values_joined.insert(id, Slot);
        slots_joined.insert(id, t);
      }
      None => {
        g.note_poison(id,
          format!("slot pointer irreconcilable across the join at {}",
            floc(&loc)));
        values_joined.insert(id, RefcountState::Poisoned);
      }
    }
    vars_joined.insert(id, ns.iter().cloned().collect());
    for n in ns {
      vars_rev_joined.insert(n.clone(), id);
    }
    if let Some(v) = k1 {
      map1.entry(*v).or_default().push(id);
    }
    if let Some(v) = k2 {
      map2.entry(*v).or_default().push(id);
    }
  }

  //  count conservation: every owned group keeps its count in exactly
  //  one fragment of the joined partition
  for (env, other, kept, own_lab, oth_lab) in
    [(&env1, &env2, &kept1, lab1, lab2),
     (&env2, &env1, &kept2, lab2, lab1)]
  {
    let mut groups: Vec<ValId> = env.vars.keys().copied().collect();
    groups.sort();
    for v in groups {
      if !matches!(env.values.get(&v), Some(Owned {..})) {
        continue;
      }
      let k = kept.get(&v).copied().unwrap_or(0);
      if k == 1 {
        continue;
      }
      //  a count kept in NO fragment evaporated on this path -- a
      //  leak, which a noreturn function tolerates
      if k == 0 && g.sem.noreturn {
        continue;
      }
      //  say what the other path holds under the same names
      let mut ns: Vec<&VarName> = env.vars.get(&v)
        .map(|s| s.iter().collect()).unwrap_or_default();
      ns.sort();
      let mut descs: Vec<String> = ns.iter()
        .filter_map(|n| other.vars_rev.get(*n).map(|ov| {
          let st = *other.values.get(ov).expect(LI);
          format!("[{}] is {}", n.name, state_word(st))
        }))
        .collect();
      descs.dedup();
      report_loc!(g, loc, "refcount error",
        "joining branches disagree about ownership of [{}]: {} on {}; \
         on {}, {}",
        env.names(v),
        state_word(*env.values.get(&v).expect(LI)),
        side_str(own_lab, "one path"),
        side_str(oth_lab, "the other path"),
        descs.join("; "));
    }
  }

  //  location-less values (consumed temporaries, poisoned sub-noun
  //  reads) live in `values`/`contains` forever: carry them under their
  //  own ids (ids are never reused, so a location-less id in one branch
  //  is either shared history or unique to that branch); diverged
  //  states on shared junk are unreadable either way: Poisoned.
  for (id, st) in env1.values.iter().chain(env2.values.iter()) {
    if env1.vars.contains_key(id) || env2.vars.contains_key(id) {
      continue;
    }
    match values_joined.get(id) {
      None => { values_joined.insert(*id, *st); }
      Some(prev) if prev == st => {}
      Some(_) => { values_joined.insert(*id, RefcountState::Poisoned); }
    }
  }

  //  live orphaned holes (no pointer name left) keep their obligation:
  //  the entry rides along under the value's own id, owner remapped to
  //  its joined image where one exists
  for (slots, map) in [(&env1.slots, &map1), (&env2.slots, &map2)] {
    for (id, t) in slots.iter() {
      if env1.vars.contains_key(id) || env2.vars.contains_key(id)
        || slots_joined.contains_key(id)
        || !matches!(values_joined.get(id), Some(RefcountState::Slot))
      {
        continue;
      }
      match t.clone().remap(|o| img(o, map)) {
        Some(rt) => { slots_joined.insert(*id, rt); }
        None => { values_joined.insert(*id, RefcountState::Poisoned); }
      }
    }
  }

  //  deferred-slot conservation: every unfilled hole a side tracks must
  //  survive into the join, exactly once per obligation -- an ambiguous
  //  pointer must not silently drop (or duplicate) the duty to fill
  for (env, map) in [(&env1, &map1), (&env2, &map2)] {
    let mut owners: Vec<ValId> = env.slots.values()
      .filter_map(|t| match t {
        SlotTarget::Hole {owner} => Some(*owner),
        SlotTarget::Var(_) => None,
      })
      .collect();
    owners.sort();
    owners.dedup();
    for o in owners {
      let n = env.slots.values()
        .filter(|t| matches!(t, SlotTarget::Hole {owner} if *owner == o))
        .count();
      let imgs: Vec<ValId> = map.get(&o).cloned().unwrap_or_else(|| vec![o]);
      let nj = slots_joined.values()
        .filter(|t| matches!(t, SlotTarget::Hole {owner}
          if imgs.contains(owner)))
        .count();
      if n != nj {
        report_loc!(g, loc, "refcount error",
          "joining branches disagree about the deferred slots of [{}] \
           ({} unfilled on this path, {} after the join)",
          env.names(o), n, nj);
      }
    }
  }

  //  containment follows the fragments: a split value's edges duplicate
  //  onto every fragment (conservative for the poison walks);
  //  location-less ids keep their identity
  let remap = |contains: &IHashMap<ValId, IHashSet<ValId>>,
    map: &HashMap<ValId, Vec<ValId>>| -> IHashMap<ValId, IHashSet<ValId>>
  {
    let mut out = <IHashMap<ValId, IHashSet<ValId>>>::new();
    for (k, kids) in contains.iter() {
      let mut kid_imgs: Vec<ValId> = Vec::new();
      for kid in kids.iter() {
        match map.get(kid) {
          Some(imgs) => kid_imgs.extend(imgs.iter().copied()),
          None => kid_imgs.push(*kid),
        }
      }
      let keys: Vec<ValId> = match map.get(k) {
        Some(imgs) => imgs.clone(),
        None => vec![*k],
      };
      for kk in keys {
        let entry = out.entry(kk).or_default();
        for ki in &kid_imgs {
          entry.insert(*ki);
        }
      }
    }
    out
  };

  let cont_joined = remap(&env1.contains, &map1)
    .union_with(remap(&env2.contains, &map2), |a, b| a + b);

  //  view-cache entries survive the join when both the parent and the
  //  child value kept their identity (unsplit groups keep their id;
  //  location-less temporaries always do) and the child is still a
  //  plain view; a same-key conflict (both sides minted their own view
  //  after the split) drops the entry
  let mut views_joined: IHashMap<(ValId, bool), ValId> = IHashMap::new();
  let mut views_dead: HashSet<(ValId, bool)> = HashSet::new();
  for (k, c) in env1.views.iter().chain(env2.views.iter()) {
    if views_dead.contains(k)
      || !values_joined.contains_key(&k.0)
      || !matches!(values_joined.get(c),
        Some(RefcountState::Borrowed) | Some(RefcountState::Direct))
    {
      continue;
    }
    match views_joined.get(k) {
      None => { views_joined.insert(*k, *c); }
      Some(prev) if prev == c => {}
      Some(_) => {
        views_joined.remove(k);
        views_dead.insert(*k);
      }
    }
  }

  //  possibly-none on either path stays possibly-none: weak marks
  //  follow the fragment images (a fresh fragment id inherits its
  //  source's provenance); location-less ids keep their identity
  let mut weak_joined: IHashSet<ValId> = IHashSet::new();
  for (wset, map) in [(&env1.weak, &map1), (&env2.weak, &map2)] {
    for v in wset.iter() {
      match map.get(v) {
        Some(imgs) => {
          for i in imgs {
            weak_joined.insert(*i);
            if let Some(w) = g.weak_why.get(v).cloned() {
              g.note_weak(*i, w);
            }
          }
        }
        None => {
          if values_joined.contains_key(v) {
            weak_joined.insert(*v);
          }
        }
      }
    }
  }

  Ok(Env {
    values: values_joined,
    vars: vars_joined,
    vars_rev: vars_rev_joined,
    contains: cont_joined,
    slots: slots_joined,
    views: views_joined,
    weak: weak_joined,
  })
}


fn mayb_join(loc: Loc, env1: Option<Env>, env2: Option<Env>, g: &mut Gen)
  -> R<Option<Env>>
{
  mayb_join_l(loc, "", env1, "", env2, g)
}

fn mayb_join_l(loc: Loc, lab1: &str, env1: Option<Env>, lab2: &str,
  env2: Option<Env>, g: &mut Gen) -> R<Option<Env>>
{
  match (env1, env2) {
    (Some(env1), Some(env2)) =>
      join_l(loc, lab1, env1, lab2, env2, g).map(Some),

    (env1, env2) => Ok(env1.or(env2)),
  }
}

/// Merge two path labels when their envs merge.
fn merge_labs(a: String, b: &str) -> String {
  if a.is_empty() {
    b.to_string()
  } else if b.is_empty() {
    a
  } else {
    format!("{}, {}", a, b)
  }
}

/// Join any number of labeled envs into one, or None if no envs
fn join_all<I>(loc: Loc, envs: I, g: &mut Gen) -> R<Option<Env>>
  where I: IntoIterator<Item = (String, Env)>
{
  let mut it = envs.into_iter();
  it.next()
    .map(|first| it.try_fold(first, |(la, a), (lb, b)| {
      let e = join_l(loc.clone(), &la, a, &lb, b, g)?;
      Ok((merge_labs(la, &lb), e))
    }))
    .transpose()
    .map(|o| o.map(|(_, e)| e))
}

// envs were parked, so we need to end their scopes using the depth of the join
// site. `local` is in the correct scope already.
fn join_scoped<I>(loc: Loc, envs: I, local: Option<Env>, depth: u32,
  g: &mut Gen) -> R<Option<Env>>
  where I: IntoIterator<Item = (String, Env)>
{
  let scoped = envs.into_iter()
    .map(|(l, e)| Ok((l, end_scope(loc.clone(), e, depth, g)?)))
    .collect::<R<Vec<_>>>()?;
  join_all(loc, scoped.into_iter()
    .chain(local.map(|e| (String::new(), e))), g)
}

/// Prose name of a refcount state, for join diagnostics.
fn state_word(st: RefcountState) -> String {
  use RefcountState::*;
  match st {
    Uninit => "uninitialized".to_string(),
    Borrowed => "an uncounted view".to_string(),
    Owned {extra: 0} => "owned".to_string(),
    Owned {extra} => format!("owned with {} extra references", extra),
    Poisoned => "already consumed".to_string(),
    Direct => "a direct atom".to_string(),
    Passthrough => "passthrough".to_string(),
    Slot => "a slot pointer".to_string(),
  }
}

/// The loobean word for a `doomed`/`fill_on` condition (c3y = 0).
fn loob_word(b: bool) -> u64 {
  if b { config::C3Y } else { config::C3N }
}

fn check_exit(loc: Loc, ret_lit: Option<u64>, env: Env, g: &Gen)
  -> Vec<Finding>
{
  let mut env = env;
  let mut out: Vec<Finding> = Vec::new();

  //  a doomed exit obliges the caller to die: nothing is checked
  if let Some(d) = g.sem.doomed {
    if ret_lit == Some(loob_word(d)) {
      return out;
    }
  }

  //  pointee-parameter contracts on this exit path
  for (pv, orig, pm) in &g.pointee_params {
    let cur = env.vars_rev.get(pv).copied();
    let state = cur.map(|v| *env.values.get(&v).expect(LI));
    //  a conditional fill (`fills ... on `c3y``) keys on this exit's
    //  literal loobean product
    if let Some(on) = pm.fill_on {
      let Some(l) = ret_lit else {
        out.push(report_at(loc.clone(), "annotation", format!(
          "conditional fill contract on [{}]: this exit does not \
           return a literal c3y/c3n, cannot tell whether the fill \
           happened", pv.name), g));
        continue;
      };
      if l != loob_word(on) {
        match pm.fills {
          //  the no-fill product: an owned value stored here anyway
          //  would be invisible to the caller
          Some(FillMode::Transferred) => {
            if let (Some(v), Some(RefcountState::Owned {..})) =
              (cur, state)
            {
              out.push(report_at(loc.clone(), "leak", format!(
                "pointee contract `fills transferred ... on {}`: [{}] \
                 filled with an owned value on a path that returns the \
                 opposite loobean; the caller will not consume it",
                if on { "c3y" } else { "c3n" }, pv.name), g));
              env.values.insert(v, RefcountState::Poisoned);
            }
          }
          //  the caller keeps its old pointee on this product: a
          //  rewrite here is invisible to its model of the variable
          Some(FillMode::Retained) => {
            if cur != Some(*orig) {
              out.push(report_at(loc.clone(), "refcount error", format!(
                "pointee contract `fills retained ... on {}`: [{}] \
                 rewritten on a path that returns the opposite \
                 loobean; the caller treats it as untouched",
                if on { "c3y" } else { "c3n" }, pv.name), g));
            }
          }
          None => {}
        }
        continue;
      }
      //  fall through to the unconditional enforcement below
    }
    match pm.fills {
      Some(FillMode::Transferred) => match (cur, state) {
        (Some(v), Some(RefcountState::Owned {extra})) => {
          //  transferred to the caller: one count is consumed here
          let st = if extra > 0 { RefcountState::Owned {extra: extra - 1} }
                   else { RefcountState::Poisoned };
          env.values.insert(v, st);
        }
        (_, Some(RefcountState::Direct)) => {}
        (_, Some(RefcountState::Uninit)) => {
          out.push(report_at(loc.clone(), "refcount error", format!(
            "pointee contract `fills transferred`: [{}] never filled \
             on this path", pv.name), g));
        }
        (_, Some(RefcountState::Borrowed)) => {
          out.push(report_at(loc.clone(), "refcount error", format!(
            "pointee contract `fills transferred`: [{}] holds an \
             uncounted reference at exit, u3k first", pv.name), g));
        }
        _ => {
          out.push(report_at(loc.clone(), "refcount error", format!(
            "pointee contract `fills transferred`: [{}] holds no \
             usable value at exit{}", pv.name,
            cur.map(|v| g.why_poisoned(v)).unwrap_or_default()), g));
        }
      },
      Some(FillMode::Retained) => match state {
        Some(RefcountState::Borrowed | RefcountState::Direct) => {}
        Some(RefcountState::Uninit) => {
          out.push(report_at(loc.clone(), "refcount error", format!(
            "pointee contract `fills retained`: [{}] never filled on \
             this path", pv.name), g));
        }
        _ => {
          out.push(report_at(loc.clone(), "refcount error", format!(
            "pointee contract `fills retained`: [{}] must hold an \
             uncounted reference at exit (an owned one would leak)",
            pv.name), g));
        }
      },
      None => {
        //  no fills clause: the pointer is never written, so the name
        //  must still hold the original value (a still-owned original
        //  under `consumes` surfaces in the leak sweep below)
        if cur != Some(*orig) {
          out.push(report_at(loc.clone(), "annotation", format!(
            "[{}] was rewritten, but the pointee is not annotated \
             `fills retained|transferred`", pv.name), g));
        }
      }
    }
  }

  //  unfilled deferred slots surviving to an exit: the built structure
  //  is incomplete
  {
    let mut hs: Vec<(ValId, ValId)> = env.slots.iter()
      .filter_map(|(sid, t)| match t {
        SlotTarget::Hole {owner} => Some((*sid, *owner)),
        SlotTarget::Var(_) => None,
      })
      .collect();
    hs.sort();
    for (sid, owner) in hs {
      out.push(report_at(loc.clone(), "refcount error", format!(
        "deferred slot (pointer [{}]) never filled: cell in [{}] is \
         incomplete", env.names(sid), env.names(owner)), g));
    }
  }

  let mut leaked: Vec<(ValId, u32)> = env.values.iter()
    .filter_map(|(id, v)| match v {
      RefcountState::Owned {extra} => Some((*id, *extra)),
      _ => None,
    })
    .collect();

  leaked.sort();

  out.extend(leaked.into_iter().map(|(id, ex)| {
    let refs = if ex > 0 { format!(" ({} extra references)", ex) }
      else { String::new() };

    let msg = format!("owned reference in [{}] not consumed{}{}",
      env.names(id), refs, g.where_owned(id));

    report_at(loc.clone(), "leak", msg, g)
  }));
  out
}

fn end_scope(loc: Loc, env: Env, depth: u32, g: &mut Gen) -> R<Env>
{
  let mut env = env;

  //  names dying at this depth, plus owned values that already lost
  //  their last name mid-scope (overwrites, discarded call products):
  //  the latter are swept as anonymous orphans, identified by their
  //  creation site
  let mut gone: Vec<(Option<VarName>, ValId)> = env.vars_rev.iter()
    .filter(|(k, _)| k.depth >= depth)
    .map(|(k, id)| (Some(k.clone()), *id))
    .chain(env.values.iter()
      .filter_map(|(k, v)| {
        (matches!(v, RefcountState::Owned {..})
          && !env.vars.contains_key(k))
          .then_some((None, *k))
      })
    )
    .collect();

  gone.sort();

  //  remove names in gone, remove empty sets
  for (name, id) in &gone {
    let Some(name) = name else { continue; };
    env.vars_rev.remove(name);
    let Some(vs) = env.vars.get_mut(id) else { continue; };
    vs.remove(name);
    if vs.is_empty() {
      env.vars.remove(id);
    }
  }

  //  values that just lost their last location won't have an entry in `vars`
  let mut orphans: BTreeMap<ValId, Vec<Name>> = BTreeMap::new();
  for (name, id) in &gone {
    if !env.vars.contains_key(id) {
      let name = match name {
        Some(name) => name.name.clone(),
        None => Rc::from("<temporary>"),
      };
      orphans.entry(*id).or_default().push(name);
    }
  }

  //  owned orphans leak; every orphan is poisoned (ids stay in `values`
  //  forever, now unreachable)
  let mut leaks: Vec<Finding> = Vec::new();
  for (id, names) in orphans {
    g.note_poison(id, format!("[{}] went out of scope at {}",
      names.join(", "), floc(&loc)));
    //  the last pointer to an unfilled deferred slot dying means the
    //  slot can never be filled: report here, where the name is known
    if let Some(SlotTarget::Hole {owner}) = env.slots.get(&id) {
      leaks.push(report_at(loc.clone(), "refcount error",
        format!("pointer [{}] to an unfilled deferred slot of [{}] \
                 goes out of scope: the slot can never be filled",
          names.join(", "), env.names(*owner)),
        g));
      env.slots.remove(&id);
    }
    let state = env.values.get_mut(&id).expect("linter invariant");
    let RefcountState::Owned {extra} = *state else {
      *state = RefcountState::Poisoned;
      continue;
    };
    if !g.sem.noreturn {
      let refs = if extra > 0 { format!(" ({} extra references)", extra) }
                 else { String::new() };

      leaks.push(report_at(loc.clone(), "leak",
        format!("owned reference in [{}] goes out of scope without being \
                  consumed{}{}", names.join(", "), refs,
          g.where_owned(id)),
        g));
    }
    *state = RefcountState::Poisoned;
  }

  if leaks.is_empty() { Ok(env) } else { Err(leaks) }
}

// Splits ForStmt into (init, cond, inc, body). 
fn for_parts(cur: &Cursor)
  -> Option<(Option<Cursor>, Option<Cursor>, Option<Cursor>, Cursor)>
{
  let kids = cur.children();
  let body = *kids.last()?;
  let parts = &kids[..kids.len() - 1];

  //  offsets of the two top-level `;` inside the for(...) parens
  let mut depth = 0i32;
  let mut semis: Vec<u32> = Vec::new();
  for t in cur.tokens() {
    match t.spelling.as_str() {
      "(" => depth += 1,
      ")" => { depth -= 1; if depth == 0 { break } }
      ";" if depth == 1 => semis.push(t.offset),
      _ => {}
    }
  }
  let (&s1, &s2) = match semis.as_slice() {
    [a, b, ..] => (a, b),
    _ => return None
  };

  let (mut init, mut cond, mut inc) = (None, None, None);
  for c in parts {
    let o = c.extent_start().offset;
    if o < s1 { init = Some(*c) }
    else if o < s2 { cond = Some(*c) }
    else { inc = Some(*c) }
  }
  Some((init, cond, inc, body))
}
