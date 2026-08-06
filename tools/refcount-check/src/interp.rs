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
  is_local_lvalue, is_noun_type, unary_op, unwrap_expr, Cursor, Name, Ty, Loc,
};
use crate::config;
use crate::sem::{AssertMode, Finding, ArgumentMode, ProductMode, Sem};

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
  Owned {extra: u32},    // correctly owned
  Poisoned,           // consumed, not valid to use
  Direct,             // direct atom, no refcounting
  Passthrough,             // do not touch refcounts
}

#[derive(Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
struct VarName {name: Name, depth: u32}

/// Immutable execution environment. I think of a (sufficiently simple) piece of
/// C AST as a function that transforms Env.
/// Noun values are numbered, each ValId represents an immutable noun with its
/// current refcount state. "Locations" tell us which variables in the scope
/// hold a given value. `locations_rev` is an inverse of that.
/// `goto_envs` holds environments that existed at goto site, restoring them
/// when reaching the goto label, by either merging with the current env or by
/// using it directly if the code is otherwise unreachable
#[derive(Default, Clone)]
struct Env {
  values: IHashMap<ValId, RefcountState>,
  vars: IHashMap<ValId, IHashSet<VarName>>,
  vars_rev: IHashMap<VarName, ValId>,
  contains: IHashMap<ValId, IHashSet<ValId>>  // noun -> (set sub-noun)
}

impl Env {
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

  /// One counted reference to `id` is given away (transferred to a
  /// callee, stored, returned). Owned decrements; losing the last
  /// reference poisons the value, so later reads through any alias
  /// report use-after-transfer. Direct atoms are free to give away.
  fn lose(mut self, id: ValId, cur: &Cursor, g: &Gen) -> R<Env>
  {
    let state = self.values.get_mut(&id)
      .expect("linter invariant: every ValId is present in values");

    match *state {
      RefcountState::Owned {extra: 0} => *state = RefcountState::Poisoned,

      RefcountState::Owned {extra}
        => *state = RefcountState::Owned {extra: extra - 1},

      RefcountState::Direct => {}

      RefcountState::Borrowed => report!(g, cur, "refcount error",
        "transfer of borrowed reference [{}]: the caller retains \
         ownership, u3k first", self.names(id)),

      RefcountState::Uninit => report!(g, cur, "refcount error",
        "transfer of uninitialized variable [{}]", self.names(id)),

      RefcountState::Poisoned => report!(g, cur, "refcount error",
        "transfer of already-consumed value [{}]", self.names(id)),

      RefcountState::Passthrough => report!(g, cur, "refcount error",
        "losing passthrough value [{}]", self.names(id)),
    };
    Ok(self)
  }
}

#[derive(Default, Clone)]
struct Flow {
  local: Option<Env>,                   // local environment, if code is reachable
  goto_envs: IHashMap<Name, IVec<Env>>, // environments from goto, for goto labels
  exit_envs: IVec<Env>,                 // environments from return, for exit
  break_envs: IVec<Env>,
  cont_envs: IVec<Env>,
  switch_env: Option<Env>,
  switch_vid: Option<ValId>,            // the switched-on noun, for case refinement
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
  assert_transfer_all: u32,
  //  the original parameter values: roots the CALLER holds a counted
  //  reference to, immune to the u3j_gate_slam borrowed-view sweep
  param_vids: Vec<ValId>,
}

impl Gen<'_> {
  /// Is a store blessed as a transfer by an enclosing nameless
  /// `assert transfer` block? (The named form has no store-site
  /// effect; it subtracts at block end, see named_transfers().)
  fn store_transfers(&self) -> bool {
    self.assert_transfer_all > 0
  }
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
  };

  for p in fun.arguments() {
    let pname = p.spelling();
    if pname.is_empty() {
      report_v!(&g, fun, "strange argument", "nameless argument");
    }
    if p.kind() != CXCursor_ParmDecl || !is_noun_type(&p.ty()) {
      continue;
    }
    let mode = sem.arg_mode(&pname);
    let rc = match mode {
      ArgumentMode::Conslike
        => report_v!(&g, fun, "not implmented","checking of conslike"),

      //  read-only marks pointer-to-noun parameters, which are not
      //  noun-typed; on a noun parameter it is an annotation mistake
      ArgumentMode::ReadOnly
        => report_v!(&g, fun, "annotation",
             "@Refcount: read-only on noun-typed parameter [{pname}]: \
              read-only applies to pointer-to-noun parameters only"),

      ArgumentMode::Transfer    => RefcountState::Owned { extra: 0 },
      ArgumentMode::Retain      => RefcountState::Borrowed,
      ArgumentMode::Direct      => RefcountState::Direct,
      ArgumentMode::Passthrough => RefcountState::Passthrough

    };
    let pid = env.insert_new(VarName {name: pname, depth: 0}, rc, &mut g);
    g.param_vids.push(pid);
  }

  let flo: Flow = Flow {local: Some(env), ..Default::default()};
  match execute_statement(&body, flo, 0, &mut g) {
    Err(finding) => finding,
    Ok(flo) => {
      //  a parked goto env whose label never followed is a backward
      //  jump (a loop the walker cannot model): report, don't silently
      //  drop the path
      if !flo.goto_envs.is_empty() {
        let mut labels: Vec<&str> =
          flo.goto_envs.keys().map(|k| k.as_ref()).collect();
        labels.sort();
        return vec![report(None, "complicated",
          format!("backward goto to [{}]: the label was already passed, \
                   won't analyze", labels.join(", ")), &g)];
      }
      flo.exit_envs.into_iter().chain(flo.local)
        .map(|env| check_exit(body.extent_end(), env, &g))
        .flatten().collect()
    }
  }
}

/// `@Refcount: assert transfer <names>`: the block consumes one counted
/// reference of each listed name, on top of whatever its statements do
/// visibly -- for transfers the walker cannot recognize at the store
/// site (slot-laundered values, encoded pointers). Applied to the
/// fall-through path at block end.
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
    env = lose_cascade(env, id, cur, g)?;
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
  if k == CXCursor_CompoundStmt {
    //  block-assert blessings for this compound. The nameless form
    //  blesses every store in the block: the eval side consults
    //  g.store_transfers() at store sites (restored on the way out --
    //  recursion is the save/restore stack). The named form has no
    //  store-site effect: the listed names are consumed on top of the
    //  block's own effects, at block end.
    let mut all_here = 0u32;
    let mut names_here: Vec<Name> = Vec::new();
    for (mode, names) in g.host.block_asserts(cur) {
      if !matches!(mode, AssertMode::Transfer) {
        report!(g, cur, "annotation",
          "block-level `assert retain/produce` is no longer supported: \
           stores retain by default, annotate transfers only");
      }
      if names.is_empty() {
        all_here += 1;
      } else {
        names_here.extend(names.into_iter().map(Name::from));
      }
    }
    g.assert_transfer_all += all_here;

    let walked = cur.children().into_iter()
      .try_fold(flo, |flo, kid| execute_statement(&kid, flo, depth + 1, g))
      .and_then(|flo| named_transfers(cur, flo, &names_here, g));
    let out = walked.and_then(|f| f.scope_done(cur.extent_end(), depth + 1, g));

    g.assert_transfer_all -= all_here;
    return out;
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

    let first      = branch(then, &flo,   t_op_env, g)?;
    let mut second = branch(els,  &first, f_op_env, g)?;

    second.local = mayb_join(cur.extent_end(), first.local, second.local, g)?;
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

        //  looping paths (fall-through and continue) must reproduce the
        //  env before the conditional; they do NOT flow out. The only
        //  exits are the cond-false branch and breaks, so a break-less
        //  `while (1)` leaves no fall-out path.
        //
        mayb_join(cur.location(), Some(env.clone()), cont, g)?;
        mayb_join(cur.location(), Some(env), fall, g)?;
        return done_flo.join(cur.extent_end(), f_op_env, g);
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
        mayb_join(cur.location(), fall, Some(env), g)?;
        //  the for-init scope ends here, on every exit path
        let done_flo = done_flo.scope_done(cur.extent_end(), depth + 1, g)?;
        let f_op_env = f_op_env
          .map(|e| end_scope(cur.extent_end(), e, depth + 1, g))
          .transpose()?;
        return done_flo.join(cur.extent_end(), f_op_env, g);
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
    //  no discard check on the switch value: direct-range case labels
    //  refine it per arm (switching on an indirect noun is expected to
    //  hit a bailing default), and an unconsumed owned value still
    //  surfaces as a leak later
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
      flo.break_envs.push_back(env);
      flo.local = None;
    };
    return Ok(flo)
  }
  else if k == CXCursor_ContinueStmt {
    let mut flo = flo;
    if let Some(env) = flo.local {
      flo.cont_envs.push_back(env);
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
        flo.exit_envs.push_back(env);
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
      flo.goto_envs.entry(target).or_default().push_back(env);
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
    if is_expr_kind(k) {
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
      !matches!(ik, CXCursor_TypeRef | CXCursor_StructDecl | CXCursor_UnionDecl
        | CXCursor_EnumDecl)
      && (
        !is_array
        || matches!(ik, CXCursor_InitListExpr | CXCursor_StringLiteral)
      )
    });

    //  scalar declaration
    //
    if !is_record && !is_array {
      let Some(init) = init else {
        if is_noun_type(&ty) {
          env.insert_new(var, RefcountState::Uninit, g);
        }
        continue;
      };
      if init.kind() == CXCursor_InitListExpr {
        //  braced scalar init (`c3_c* p = {0};`): nouns never do this
        if is_noun_type(&ty) {
          report!(g, &d, "strange definition",
            "braced initializer on a noun variable, won't analyze");
        }
        let Some(nxt) = eval_init_effects(&init, env, depth, g)? else {
          return Ok(None);
        };
        env = nxt;
        continue;
      }
      let (vid, nxt) = eval_expr(&init, env, depth, g)?;
      let Some(mut nxt) = nxt else { return Ok(None); };
      if vid.is_some() || is_noun_type(&ty) {
        //  a None vid on a noun variable means a non-noun initializer:
        //  gotta be direct (bind_value handles both)
        bind_value(&mut nxt, var, vid, g);
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
        let Some(nxt) = eval_init_effects(&init, env, depth, g)? else {
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
        let Some(nxt) = eval_init_effects(&init, env, depth, g)? else {
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

//  --------------------------------------------------------------------------
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

/// Evaluate everything expression-like for refcount sideeffects
///
fn eval_init_effects(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
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
      let Some(nxt) = eval_init_effects(&c, env, depth, g)? else {
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
  if holds_owned(&nxt, vid) {
    report!(g, cur, "leak",
      "owned value discarded into an untracked aggregate initializer");
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
      bind_value(&mut nxt, VarName {name: path(), depth}, vid, g);
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

fn holds_owned(env: &Env, vid: Option<ValId>) -> bool
{
  vid.map_or(false, |v|
    matches!(env.values.get(&v), Some(RefcountState::Owned {..})))
}

// Returns (flow whose local is the joined BREAK paths, the fall-through
// env after body + inc, the joined continue env), scopes cleared. The
// fall-through and continue envs loop around: the caller checks them
// against the pre-condition env and drops them; only breaks (and the
// caller's cond-false branch) exit the loop.
//
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

  let mut out = flo;
  out.local = brks;
  out.goto_envs = out.goto_envs.union_with(flo_done.goto_envs, |a, b| a + b);
  out.exit_envs = out.exit_envs + flo_done.exit_envs;
  Ok((out, fall, cont))
}

/// `return [expr];` -- evaluate the expression, check it against the
/// function's product protocol, and hand back the env to park in
/// exit_envs. None when the expression itself ends the path
/// (`return u3m_bail(..)` -- common in this codebase).
fn execute_return(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  let (opt_vid, opt_env) = eval_expr(cur, env, depth, g)?;
  // let Some(vid) = opt_vid else { return Ok(opt_env) };
  let Some(mut env) = opt_env else { return Ok(None) };
  match g.sem.product {
    ProductMode::Retain => {},
    ProductMode::Transfer => {
      if let Some(vid) = opt_vid {
        // if None: always direct?
        env = env.lose(vid, cur, g)?
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
  //  bare `u3k(x);` in statement position: the new count belongs to [x]
  //  itself, not to a discarded product
  let u = unwrap_expr(*cur);
  if u.kind() == CXCursor_CallExpr {
    let rs = u.referenced().map(|r| r.spelling());
    if matches!(rs.as_deref(), Some("u3a_gain") | Some("u3a_take")) {
      let mut args = u.arguments();
      if args.is_empty() {
        args = u.children().into_iter().skip(1).collect();
      }
      if let Some(nm) = args.first().and_then(decl_ref_name) {
        if let Some((_, vid)) = read_var(&env, &nm) {
          let mut env = env;
          match *env.values.get(&vid).expect(LI) {
            RefcountState::Borrowed => {
              env.values.insert(vid, RefcountState::Owned {extra: 0});
            }
            RefcountState::Owned {extra} => {
              env.values.insert(vid, RefcountState::Owned {extra: extra + 1});
            }
            RefcountState::Direct => {}
            RefcountState::Uninit => report!(g, cur, "refcount error",
              "u3k of uninitialized variable [{}]", env.names(vid)),
            RefcountState::Poisoned => report!(g, cur, "use-after-free",
              "u3k of already-consumed value [{}]", env.names(vid)),
            RefcountState::Passthrough => report!(g, cur, "refcount error",
              "refcount operation on passthrough value [{}]",
              env.names(vid)),
          }
          return Ok(Some(env));
        }
      }
    }
  }

  let (vid, env) = eval_expr(cur, env, depth, g)?;
  let Some(env) = env else { return Ok(None); };
  let Some(vid) = vid else { return Ok(Some(env)); };

  if env.vars.get(&vid).is_none_or(|l| l.is_empty()) {
    if holds_owned(&env, Some(vid)) {
      report!(g, cur, "leak",
        "owned product of the expression is discarded without being \
         captured by a variable");
    }
  }
  //  a dropped non-owned vid stays in `values` forever (no GC); join()
  //  carries location-less ids across branches unchanged
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
      "[{}] is (derived from) a value already consumed on this path",
      env.names(vid));
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

/// An owned value that no variable holds, dropped in a context that does
/// not take its reference, is leaked.
fn discard_check(cur: &Cursor, env: &Env, vid: Option<ValId>, g: &Gen)
  -> R<()>
{
  let Some(v) = vid else { return Ok(()); };
  if env.vars.get(&v).is_none_or(|l| l.is_empty())
    && matches!(env.values.get(&v), Some(RefcountState::Owned {..}))
  {
    report!(g, cur, "leak",
      "owned product discarded in a non-noun context (reference is \
       leaked)");
  }
  Ok(())
}

/// Poison every Borrowed value transitively contained in `root` (the
/// root itself is untouched): its interior may be freed under us, by
/// unifying equality (u3r_sing) or by consumption of the parent.
fn poison_borrowed_within(env: &mut Env, root: ValId)
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
          *st = RefcountState::Poisoned;
        }
      }
    }
  }
}

/// lose() plus the fallout: giving away the last counted reference means
/// the new owner may free the noun, so our borrowed views into it die.
fn lose_cascade(env: Env, id: ValId, cur: &Cursor, g: &Gen) -> R<Env>
{
  let mut env = env.lose(id, cur, g)?;
  //  lose() of an already-poisoned value errors out, so Poisoned here
  //  means the transition happened just now
  if matches!(env.values.get(&id), Some(RefcountState::Poisoned)) {
    poison_borrowed_within(&mut env, id);
  }
  Ok(env)
}

/// `var` stops being a location of its current value (overwrite or
/// out-param rebinding). An owned value losing its last location leaks.
fn unbind_var(mut env: Env, var: &VarName, cur: &Cursor, g: &Gen) -> R<Env>
{
  let Some(id) = env.vars_rev.get(var).copied() else { return Ok(env); };
  env.vars_rev.remove(var);
  if let Some(locs) = env.vars.get_mut(&id) {
    locs.remove(var);
    if locs.is_empty() {
      env.vars.remove(&id);
    }
  }
  if !env.vars.contains_key(&id) {
    let st = env.values.get_mut(&id).expect(LI);
    if let RefcountState::Owned {extra} = *st {
      let refs = if extra > 0 { format!(" ({} extra references)", extra) }
                 else { String::new() };
      report!(g, cur, "leak",
        "owned reference in [{}] overwritten without being consumed{}",
        var.name, refs);
    }
    *st = RefcountState::Poisoned;
  }
  Ok(env)
}

/// Bind `var` to the value produced by an initializer or assignment
/// RHS: plain aliasing, the name becomes one more location of the
/// value. A value with no vid (non-noun expression) binds as a fresh
/// direct atom.
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
  //  variable makes it Direct
  if matches!(k, CXCursor_IntegerLiteral | CXCursor_CharacterLiteral
    | CXCursor_FloatingLiteral | CXCursor_StringLiteral)
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
      let (lv, nxt) = eval_expr(&lhs, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      discard_check(&lhs, &nxt, lv, g)?;
      return eval_expr(&rhs, nxt, depth, g);
    }
    if op == binop::LAND || op == binop::LOR {
      //  value position: model the short circuit, join the outcomes
      let (t_env, f_env) = eval_cond(&cur, env, depth, g)?;
      return Ok((None, mayb_join(cur.location(), t_env, f_env, g)?));
    }
    //  arithmetic/comparison: operands are read, the product is not a
    //  counted noun
    let (lv, nxt) = eval_expr(&lhs, env, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    discard_check(&cur, &nxt, lv, g)?;
    let (rv, nxt) = eval_expr(&rhs, nxt, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    discard_check(&cur, &nxt, rv, g)?;
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
      let (v, nxt) = eval_expr(&c, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      discard_check(&cur, &nxt, v, g)?;
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
        if let Some((_, vid)) = read_var(&env, &name) {
          //  the address of a proven-direct atom is a plain word/byte
          //  view of the variable (sew/aor-style buffer readers): there
          //  is no refcount to corrupt and nothing to free through it
          if matches!(env.values.get(&vid), Some(RefcountState::Direct)) {
            return Ok((None, Some(env)));
          }
          report!(g, &cur, "complicated",
            "address of tracked noun variable [{}] escapes, won't analyze \
             (annotate the callee `@Refcount: destructures `src`` if this \
             is an out-param of a destructurer; `@Refcount: read-only` on \
             the parameter if it only reads through the pointer; or prove \
             the value direct with a u3a_is_cat guard if this is a \
             word/byte view)", name);
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
    //  * ! ~ - + : the operand is read, the result is untracked
    let (v, nxt) = eval_expr(child, env, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    discard_check(&cur, &nxt, v, g)?;
    return Ok((None, Some(nxt)));
  }

  if k == CXCursor_ArraySubscriptExpr {
    //  elements are untracked storage; base and index for effects
    for c in cur.children() {
      let (v, nxt) = eval_expr(&c, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      discard_check(&cur, &nxt, v, g)?;
      env = nxt;
    }
    return Ok((None, Some(env)));
  }

  if k == CXCursor_InitListExpr {
    //  compound literal in expression position: untracked aggregate
    for c in cur.children() {
      let (v, nxt) = eval_expr(&c, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      discard_check(&c, &nxt, v, g)?;
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
    //  atom adds no obligations): the other side's value passes through
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

/// A call expression: hard-wired noun primitives first, then the
/// callee's resolved protocol.
/// A destructurer call (u3x_cell &co, or `@Refcount: destructures`):
/// the argument at `src_i` is the source noun; every other `&var`
/// argument is an out-param whose variable is rebound to a borrowed
/// sub-noun of the source.
fn eval_destructurer(cur: &Cursor, args: &[Cursor], src_i: usize, env: Env,
  depth: u32, g: &mut Gen) -> R<(Option<ValId>, Option<Env>)>
{
  let mut env = env;
  let mut src_vid: Option<ValId> = None;
  for (i, a) in args.iter().enumerate() {
    if i == src_i {
      let (v, nxt) = eval_expr(a, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      env = nxt;
      src_vid = v;
      continue;
    }
    let au = unwrap_expr(*a);
    if au.kind() == CXCursor_UnaryOperator
      && unary_op(&au).as_deref() == Some("&")
    {
      if let Some(nm) = au.children().first().and_then(decl_ref_name) {
        if let Some((var, _)) = read_var(&env, &nm) {
          env = unbind_var(env, &var, cur, g)?;
          let id = new_val(&mut env, RefcountState::Borrowed, g);
          env.bind_decl(var, id);
          if let Some(src) = src_vid {
            env.contains.entry(src).or_default().insert(id);
          }
          continue;
        }
      }
      //  &untracked storage: opaque out-param
      continue;
    }
    let (v, nxt) = eval_expr(a, env, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    discard_check(a, &nxt, v, g)?;
    env = nxt;
  }
  Ok((None, Some(env)))
}

fn eval_call(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<(Option<ValId>, Option<Env>)>
{
  let callee = cur.referenced();
  let cname: Option<Name> = callee.as_ref().map(|c| c.spelling());
  let cn = cname.as_deref().unwrap_or("");
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

  //  u3z: give away one counted reference
  if cn == "u3a_lose" {
    let Some(a0) = args.first() else {
      report!(g, cur, "strange expression", "u3z without an argument");
    };
    let (vid, nxt) = eval_expr(a0, env, depth, g)?;
    let Some(mut nxt) = nxt else { return Ok((None, None)); };
    if let Some(vid) = vid {
      nxt = lose_cascade(nxt, vid, cur, g)?;
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
    let prod = match vid.map(|v| (v, *nxt.values.get(&v).expect(LI))) {
      Some((v, RefcountState::Uninit)) => {
        report!(g, cur, "refcount error",
          "u3k of uninitialized variable [{}]", nxt.names(v));
      }
      Some((_, RefcountState::Direct)) => RefcountState::Direct,
      _ => RefcountState::Owned {extra: 0},
    };
    let id = new_val(&mut nxt, prod, g);
    return Ok((Some(id), Some(nxt)));
  }

  //  u3h/u3t: an uncounted view into the argument's interior
  if cn == "u3a_h" || cn == "u3a_t" {
    let Some(a0) = args.first() else {
      report!(g, cur, "strange expression", "u3h/u3t without an argument");
    };
    let (vid, nxt) = eval_expr(a0, env, depth, g)?;
    let Some(mut nxt) = nxt else { return Ok((None, None)); };
    if let Some(parent) = vid {
      if matches!(nxt.values.get(&parent), Some(RefcountState::Uninit)) {
        report!(g, cur, "refcount error",
          "u3h/u3t of uninitialized variable [{}]", nxt.names(parent));
      }
    }
    let id = new_val(&mut nxt, RefcountState::Borrowed, g);
    if let Some(parent) = vid {
      nxt.contains.entry(parent).or_default().insert(id);
    }
    return Ok((Some(id), Some(nxt)));
  }

  //  u3a_is_cat &co: reads only, loobean product
  if config::guard_kind(cn).is_some() {
    let Some(a0) = args.first() else {
      report!(g, cur, "strange expression", "guard without an argument");
    };
    let (_, nxt) = eval_expr(a0, env, depth, g)?;
    let Some(nxt) = nxt else { return Ok((None, None)); };
    return Ok((None, Some(nxt)));
  }

  //  u3x_cell &co: `&var` out-params become borrowed sub-nouns of the
  //  source
  if let Some(src_i) = config::destructurer_src(cn) {
    return eval_destructurer(cur, &args, src_i, env, depth, g);
  }

  //  no referenced declaration: function pointer or similar
  let Some(cal) = callee else {
    let mut tracked = false;
    for a in &args {
      let (v, nxt) = eval_expr(a, env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      tracked = tracked || v.is_some();
      env = nxt;
    }
    if tracked {
      report!(g, cur, "complicated",
        "call through a function pointer with tracked noun arguments, \
         won't analyze");
    }
    return Ok((None, Some(env)));
  };

  let sem = g.host.callee_sem(&cal);

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

  //  `@Refcount: destructures `src``: an annotated destructurer
  if let Some(srcn) = &sem.destructures {
    let Some(src_i) = params.iter()
      .position(|p| &*p.spelling() == srcn.as_str())
    else {
      report!(g, cur, "annotation",
        "@Refcount: destructures names unknown parameter `{}` of {}()",
        srcn, cn);
    };
    return eval_destructurer(cur, &args, src_i, env, depth, g);
  }

  //  phase 1: C evaluates every operand before the call
  let mut evald: Vec<(Cursor, Option<Cursor>, Option<ValId>)> = Vec::new();
  for (i, a) in args.iter().enumerate() {
    //  `&var` handed to a `@Refcount: read-only` pointer-to-noun
    //  parameter: the callee reads the noun through the pointer but
    //  never writes it, so the address does not escape -- model it as
    //  a plain read of the variable
    if let Some(p) = params.get(i) {
      if sem.arg_mode(&p.spelling()) == ArgumentMode::ReadOnly
        && !is_noun_type(&p.ty())
      {
        let au = unwrap_expr(*a);
        if au.kind() == CXCursor_UnaryOperator
          && unary_op(&au).as_deref() == Some("&")
        {
          if let Some(nm) = au.children().first().and_then(decl_ref_name) {
            if let Some((_, vid)) = read_var(&env, &nm) {
              if matches!(env.values.get(&vid), Some(RefcountState::Uninit)) {
                report!(g, a, "refcount error",
                  "read-only parameter of {}() reads uninitialized \
                   variable [{}]", cn, env.names(vid));
              }
              read_check(a, &env, vid, g)?;
              evald.push((*a, params.get(i).copied(), None));
              continue;
            }
          }
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
    let Some(p) = p else { continue; };  // varargs: too ambiguous
    if !is_noun_type(&p.ty()) {
      //  an owned product handed to a declared non-noun parameter
      //  (e.g. u3a_malloc(u3kb_lent(..))) drops its reference
      discard_check(a, &env, *v, g)?;
      continue;
    }
    let Some(v) = *v else {
      //  untracked argument value (global, literal): nothing to account
      continue;
    };
    let pname = p.spelling();
    match sem.arg_mode(&pname) {
      ArgumentMode::Retain => {
        if env.vars.get(&v).is_none_or(|l| l.is_empty())
          && matches!(env.values.get(&v), Some(RefcountState::Owned {..}))
        {
          report!(g, a, "leak",
            "owned product passed to retaining parameter of {}(); \
             reference is leaked", cn);
        }
      }
      ArgumentMode::Transfer => {
        env = lose_cascade(env, v, a, g)?;
      }
      ArgumentMode::Direct => {
        //  the callee bails unless this is a direct atom: on return it
        //  is proven direct, with no counted references
        refine_direct(&mut env, v);
      }
      ArgumentMode::Passthrough => {
        pass_vid = Some(v);
      }
      ArgumentMode::ReadOnly => {
        report!(g, a, "annotation",
          "@Refcount: read-only on noun-typed parameter of {}(): \
           read-only applies to pointer-to-noun parameters only", cn);
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
          RefcountState::Passthrough => {
            report!(g, a, "refcount error",
              "losing passthrough value [{}]", env.names(v));
          }
        }
      }
    }
  }

  //  a unifying comparison -- u3r_sing itself, or any function that
  //  can run one over its arguments (nock evaluation, memo/hashtable
  //  key lookups; see config::UNIFYING_FNS): equal interior copies may
  //  be freed and repointed, so borrowed views into the arguments die
  if config::UNIFYING_FNS.contains(&cn) {
    for (_, _, v) in &evald {
      if let Some(v) = *v {
        poison_borrowed_within(&mut env, v);
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
    for id in stale {
      env.values.insert(id, RefcountState::Poisoned);
    }
  }

  if let Some(v) = pass_vid {
    return Ok((Some(v), Some(env)));
  }
  match &sem.product {
    ProductMode::NonNoun => Ok((None, Some(env))),
    ProductMode::Transfer => {
      let id = new_val(&mut env, RefcountState::Owned {extra: 0}, g);
      for v in cons_vids {
        env.contains.entry(id).or_default().insert(v);
      }
      Ok((Some(id), Some(env)))
    }
    ProductMode::Retain => {
      //  the product is (a sub-noun of) one of the arguments
      let id = new_val(&mut env, RefcountState::Borrowed, g);
      for (_, p, v) in &evald {
        if let (Some(p), Some(v)) = (p, v) {
          if is_noun_type(&p.ty()) {
            env.contains.entry(*v).or_default().insert(id);
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

  let lname = decl_ref_name(lhs);
  if let Some(ln) = &lname {
    if let Some((var, old)) = read_var(&env, ln) {
      if rvid == Some(old) {
        return Ok((rvid, Some(env)));  //  x = x
      }
      env = unbind_var(env, &var, cur, g)?;
      let id = bind_value(&mut env, var, rvid, g);
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
    if g.store_transfers() {
      env = lose_cascade(env, v, cur, g)?;
    } else if env.vars.get(&v).is_none_or(|l| l.is_empty())
      && matches!(env.values.get(&v), Some(RefcountState::Owned {..}))
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
      discard_check(&kids[0], &nxt, lv, g)?;
      let (rv, nxt) = eval_expr(&kids[1], nxt, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      discard_check(&kids[1], &nxt, rv, g)?;
      //  facts resolved on the post-evaluation env, so an assignment
      //  inside the comparison refines the rebound value
      let fact = guard_fact(&kids[0], &kids[1], &nxt);
      let mut te = nxt.clone();
      let mut fe = nxt;
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
      let (lv, nxt) = eval_expr(&kids[0], env, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      discard_check(&kids[0], &nxt, lv, g)?;
      let (rv, nxt) = eval_expr(&kids[1], nxt, depth, g)?;
      let Some(nxt) = nxt else { return Ok((None, None)); };
      discard_check(&kids[1], &nxt, rv, g)?;
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
  let (v, nxt) = eval_expr(&cur, env, depth, g)?;
  let Some(nxt) = nxt else { return Ok((None, None)); };
  discard_check(&cur, &nxt, v, g)?;
  Ok((Some(nxt.clone()), Some(nxt)))
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
            let Some(gn) = yc.arguments().first().and_then(decl_ref_name)
              else { return None; };
            let Some((_, vid)) = read_var(env, &gn) else { return None; };
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
  use RefcountState::*;

  if env1.vars_rev.len() != env2.vars_rev.len()
    || env1.vars_rev.keys().any(|n| !env2.vars_rev.contains_key(n))
  {
    report_loc!(g, loc, "refcount error",
      "joining branches have conflicting alias sets");
  }

  let mut names: Vec<VarName> = env1.vars_rev.keys().cloned().collect();
  names.sort();

  let st1 = |v: ValId| -> RefcountState { *env1.values.get(&v).expect(LI) };
  let st2 = |v: ValId| -> RefcountState { *env2.values.get(&v).expect(LI) };

  //  an adopted (Uninit-on-this-side) name keys by a rigid
  //  representative of its partner group, or None if the whole partner
  //  group is Uninit on this side
  let rep1 = |group2: ValId| -> Option<ValId> {
    let mut ms: Vec<&VarName> = env2.vars.get(&group2)
      .map(|s| s.iter().collect()).unwrap_or_default();
    ms.sort();
    ms.into_iter()
      .map(|m| *env1.vars_rev.get(m).expect(LI))
      .find(|v| !matches!(st1(*v), Uninit))
  };
  let rep2 = |group1: ValId| -> Option<ValId> {
    let mut ms: Vec<&VarName> = env1.vars.get(&group1)
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
  let mut map1: HashMap<ValId, Vec<ValId>> = HashMap::new();
  let mut map2: HashMap<ValId, Vec<ValId>> = HashMap::new();
  let mut kept1: HashMap<ValId, u32> = HashMap::new();
  let mut kept2: HashMap<ValId, u32> = HashMap::new();

  for ((k1, k2), ns) in &frags {
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
            "joining branches have conflicting refcounts for [{}]",
            who.join(", "))
        }
      },
    };
    let id = match k1 {
      Some(v) if k1_n[v] == 1 => *v,
      _ => { let id = g.id_gen; g.id_gen += 1; id }
    };
    values_joined.insert(id, rc);
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
  }

  //  count conservation: every owned group keeps its count in exactly
  //  one fragment of the joined partition
  for (env, kept) in [(&env1, &kept1), (&env2, &kept2)] {
    let mut groups: Vec<ValId> = env.vars.keys().copied().collect();
    groups.sort();
    for v in groups {
      if matches!(env.values.get(&v), Some(Owned {..}))
        && kept.get(&v).copied().unwrap_or(0) != 1
      {
        report_loc!(g, loc, "refcount error",
          "joining branches disagree about ownership of [{}]",
          env.names(v));
      }
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

  Ok(Env {
    values: values_joined,
    vars: vars_joined,
    vars_rev: vars_rev_joined,
    contains: cont_joined,
  })
}


fn mayb_join(loc: Loc, env1: Option<Env>, env2: Option<Env>, g: &mut Gen)
  -> R<Option<Env>>
{
  match (env1, env2) {
    (Some(env1), Some(env2)) => join(loc, env1, env2, g).map(Some),
    (env1, env2) => Ok(env1.or(env2)),
  }
}

/// Join any number of envs into one, or None if no envs
fn join_all<I>(loc: Loc, envs: I, g: &mut Gen) -> R<Option<Env>>
  where I: IntoIterator<Item = Env>
{
  let mut it = envs.into_iter();
  it.next()
    .map(|first| it.try_fold(first, |a, b| join(loc.clone(), a, b, g)))
    .transpose()
}

// envs were parked, so we need to end their scopes using the depth of the join
// site. `local` is in the correct scope already.
fn join_scoped<I>(loc: Loc, envs: I, local: Option<Env>, depth: u32,
  g: &mut Gen) -> R<Option<Env>>
  where I: IntoIterator<Item = Env>
{
  let scoped = envs.into_iter()
    .map(|e| end_scope(loc.clone(), e, depth, g))
    .collect::<R<Vec<_>>>()?;
  join_all(loc, scoped.into_iter().chain(local), g)
}

fn check_exit(loc: Loc, env: Env, g: &Gen) -> Vec<Finding>
{
  let mut leaked: Vec<(ValId, u32)> = env.values.iter()
    .filter_map(|(id, v)| match v {
      RefcountState::Owned {extra} => Some((*id, *extra)),
      _ => None,
    })
    .collect();

  leaked.sort();

  leaked.into_iter().map(|(id, ex)| {
    let refs = if ex > 0 { format!(" ({} extra references)", ex) }
      else { String::new() };
    
    let msg = format!("owned reference in [{}] not consumed{}",
      env.names(id), refs);
  
    report_at(loc.clone(), "leak", msg, g)
    }).collect()
}

fn end_scope(loc: Loc, env: Env, depth: u32, g: &mut Gen) -> R<Env>
{
  let mut env = env;

  let mut gone: Vec<(VarName, ValId)> = env.vars_rev.iter()
    .filter(|(k, _)| k.depth >= depth)
    .map(|(k, id)| (k.clone(), *id))
    .collect();
  gone.sort();

  //  remove names in gone, remove empty sets
  for (name, id) in &gone {
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
      orphans.entry(*id).or_default().push(name.name.clone());
    }
  }

  //  owned orphans leak; every orphan is poisoned (ids stay in `values`
  //  forever, now unreachable)
  let mut leaks: Vec<Finding> = Vec::new();
  for (id, names) in orphans {
    let state = env.values.get_mut(&id).expect("linter invariant");
    let RefcountState::Owned {extra} = *state else {
      *state = RefcountState::Poisoned;
      continue;
    };
    let refs = if extra > 0 { format!(" ({} extra references)", extra) }
               else { String::new() };

    leaks.push(report_at(loc.clone(), "leak",
      format!("owned reference in [{}] goes out of scope without being \
                consumed{}", names.join(", "), refs),
      g));
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