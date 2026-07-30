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

use std::collections::{BTreeSet, HashMap, HashSet};
use std::rc::Rc;

use clang_sys::*;
use indexmap::IndexMap;

use crate::ast::{
  binop, decl_ref_name, int_literal_value, is_expr_kind, is_local_lvalue, is_noun_type,
  unary_op, unwrap_expr, Cursor, Name,
};
use crate::config;
use crate::sem::{AssertMode, Finding, Mode, Sem};

/// Services the interpreter needs from the enclosing tool.
pub trait Host {
  /// Resolved refcount protocol of a callee (annotation + defaults).
  fn callee_sem(&mut self, callee: &Cursor) -> Rc<Sem>;
  /// `{ // @Refcount: assert ... }` annotations on a compound statement.
  fn block_asserts(&self, compound: &Cursor) -> Vec<(AssertMode, Vec<String>)>;
}

/// Check one function definition; returns the findings.
pub fn check_function(host: &mut dyn Host, fun: &Cursor, sem: &Sem) -> Vec<Finding> {
  let mut chk = Chk {
    host,
    fun: *fun,
    name: fun.spelling().to_string(),
    sem,
    findings: Vec::new(),
    open_temps: Vec::new(),
    next_temp: 0,
    frozen: HashSet::new(),
    assert_depth: 0,
    reported: HashSet::new(),
    param_modes: IndexMap::new(),
    noun_params: Vec::new(),
    goto_envs: IndexMap::new(),
    label_pos: HashMap::new(),
  };
  chk.run();
  chk.findings
}

// ---------------------------------------------------------------------------
// value / environment model

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
enum St {
  Uninit,
  Owned,
  Borrowed,
  Consumed,
  Direct,
  Unknown,
  Conflict,
  Poisoned,
}
use St::*;

#[derive(Clone, Debug)]
struct Val {
  state: St,
  origins: BTreeSet<Name>,
  temp_id: Option<u64>,
  // srcs: which variables this expression value was loaded from
  // (survives ternary merges so consumption can be attributed);
  // never stored into the environment
  srcs: BTreeSet<Name>,
}

impl Val {
  fn new(state: St) -> Val {
    Val { state, origins: BTreeSet::new(), temp_id: None, srcs: BTreeSet::new() }
  }

  fn with_origins(state: St, origins: BTreeSet<Name>) -> Val {
    Val { state, origins, temp_id: None, srcs: BTreeSet::new() }
  }

  fn borrowed_from(name: &Name) -> Val {
    Val::with_origins(Borrowed, BTreeSet::from([name.clone()]))
  }

  fn key_eq(&self, other: &Val) -> bool {
    self.state == other.state && self.origins == other.origins
  }
}

fn merge_core(a: &Val, b: &Val) -> Val {
  if a.key_eq(b) {
    return Val::with_origins(a.state, a.origins.clone());
  }
  let (sa, sb) = (a.state, b.state);
  let has = |s: St| sa == s || sb == s;
  let pair = |x: St, y: St| (sa == x && sb == y) || (sa == y && sb == x);
  let union = || a.origins.union(&b.origins).cloned().collect::<BTreeSet<_>>();
  if has(Unknown) {
    return Val::new(Unknown);
  }
  if has(Conflict) && !has(Poisoned) {
    return Val::new(Conflict); // sticky: a path divergence stays reportable
  }
  if pair(Direct, Owned) {
    return Val::new(Owned);
  }
  if pair(Direct, Borrowed) {
    return Val::with_origins(Borrowed, union());
  }
  if pair(Direct, Consumed) {
    return Val::new(Consumed);
  }
  if sa == Borrowed && sb == Borrowed {
    return Val::with_origins(Borrowed, union());
  }
  if pair(Uninit, Owned) {
    return Val::new(Conflict);
  }
  let in_dead = |s: St| matches!(s, Poisoned | Consumed | Conflict);
  if in_dead(sa) && in_dead(sb) {
    return Val::new(if has(Poisoned) { Poisoned } else { Conflict });
  }
  if pair(Owned, Consumed) || pair(Owned, Borrowed) {
    return Val::new(Conflict);
  }
  Val::new(Unknown)
}

fn merge_val(a: &Val, b: &Val) -> Val {
  let mut r = merge_core(a, b);
  r.srcs = a.srcs.union(&b.srcs).cloned().collect();
  r
}

type Env = IndexMap<Name, Val>;

fn merge_env(envs: Vec<Env>) -> Option<Env> {
  let mut it = envs.into_iter();
  let mut out = it.next()?;
  for e in it {
    for (k, v) in e {
      match out.get(&k) {
        Some(o) => {
          let m = merge_val(o, &v);
          out.insert(k, m);
        }
        None => {
          // var declared in one branch only; keep as-is
          out.insert(k, v);
        }
      }
    }
  }
  Some(out)
}

fn env_key(env: &Env) -> Vec<(Name, St, Vec<Name>)> {
  let mut v: Vec<(Name, St, Vec<Name>)> = env
    .iter()
    .map(|(k, val)| (k.clone(), val.state, val.origins.iter().cloned().collect()))
    .collect();
  v.sort();
  v
}

/// Result of executing a statement: environments that fall through,
/// break out, or continue.
#[derive(Default)]
struct Flow {
  falls: Vec<Env>,
  brks: Vec<Env>,
  conts: Vec<Env>,
}

enum Stop {
  PathEnd,
  Skip(String),
}

type R<T> = Result<T, Stop>;

struct AssertCtx {
  asserts: Vec<(AssertMode, Vec<Name>)>,
  frozen_here: Vec<Name>,
  snapshots: HashMap<Name, Option<Val>>,
}

// ---------------------------------------------------------------------------
// the checker

struct Chk<'a> {
  host: &'a mut dyn Host,
  fun: Cursor,
  name: String,
  sem: &'a Sem,
  findings: Vec<Finding>,
  open_temps: Vec<u64>, // owned temporaries pending consumption
  next_temp: u64,
  frozen: HashSet<Name>, // vars under an ASSERT block
  assert_depth: u32,     // nesting depth of ASSERT blocks
  reported: HashSet<(u32, &'static str, String)>, // dedup (line, cat, var)
  param_modes: IndexMap<Name, Mode>,
  noun_params: Vec<Name>,
  goto_envs: IndexMap<Name, Vec<Env>>, // label -> envs parked by forward gotos
  label_pos: HashMap<Name, u32>,       // label name -> source offset
}

impl<'a> Chk<'a> {
  // -- reporting

  fn report(&mut self, cur: Option<&Cursor>, cat: &'static str, msg: String, var: &str) {
    let loc = match cur {
      Some(c) => c.location(),
      None => self.fun.location(),
    };
    let key = (loc.line, cat, var.to_string());
    if !self.reported.insert(key) {
      return;
    }
    self.findings.push(Finding {
      file: loc.file.unwrap_or_else(|| "None".to_string()),
      line: loc.line,
      col: loc.col,
      func: self.name.clone(),
      cat,
      msg,
    });
  }

  // -- entry

  fn run(&mut self) {
    let mut body: Option<Cursor> = None;
    for c in self.fun.children() {
      if c.kind() == CXCursor_CompoundStmt {
        body = Some(c);
      }
    }
    let Some(body) = body else { return };
    let mut env: Env = IndexMap::new();
    for p in self.fun.arguments() {
      if p.kind() != CXCursor_ParmDecl || p.spelling().is_empty() {
        continue;
      }
      if is_noun_type(&p.ty()) {
        let pname = p.spelling();
        let mode = self.sem.arg_mode(&pname);
        self.param_modes.insert(pname.clone(), mode);
        self.noun_params.push(pname.clone());
        env.insert(
          pname.clone(),
          if mode == Mode::Transfer {
            Val::new(Owned)
          } else {
            Val::borrowed_from(&pname)
          },
        );
      }
    }
    self.index_labels(&body);
    match self.exec_stmt(&body, vec![env]) {
      Ok(flow) => {
        for e in &flow.falls {
          self.check_exit(e, &body, None, None);
        }
        if !self.goto_envs.is_empty() {
          // forward gotos whose label the walker never reached
          // (e.g. a label nested in a branch entered another way)
          let mut labels: Vec<Name> = self.goto_envs.keys().cloned().collect();
          labels.sort();
          let fun = self.fun;
          self.report(
            Some(&fun),
            "skipped",
            format!(
              "goto target(s) {} never reached by the walker; those paths are unanalyzed",
              labels.join(", ")
            ),
            "",
          );
        }
      }
      Err(reason) => {
        let fun = self.fun;
        self.report(
          Some(&fun),
          "skipped",
          format!(
            "not analyzed ({}); annotate with @Refcount: custom or @Refcount: assert",
            reason
          ),
          "",
        );
      }
    }
  }

  fn index_labels(&mut self, c: &Cursor) {
    if c.kind() == CXCursor_LabelStmt {
      self.label_pos.insert(c.spelling(), c.extent_start().offset);
    }
    for ch in c.children() {
      self.index_labels(&ch);
    }
  }

  // -- exit checks

  fn check_exit(
    &mut self,
    env: &Env,
    cur: &Cursor,
    returned_root: Option<&str>,
    loc_cur: Option<&Cursor>,
  ) {
    let where_ = *loc_cur.unwrap_or(cur);
    for p in self.noun_params.clone() {
      let mode = self.param_modes[&p];
      let v = env.get(&p).cloned().unwrap_or_else(|| Val::new(Unknown));
      if self.frozen.contains(&p) {
        continue;
      }
      if mode == Mode::Retain {
        // a retained arg variable may be reused as a cursor, but
        // an owned reference parked in it dies with the frame
        if returned_root == Some(&*p) {
          continue;
        }
        if v.state == Owned {
          self.report(
            Some(&where_),
            "leak",
            format!(
              "owned reference left in retained argument variable [{}] on this path",
              p
            ),
            &p,
          );
        } else if v.state == Conflict {
          self.report(
            Some(&where_),
            "conflict",
            format!(
              "retained argument variable [{}] holds an owned reference on some paths",
              p
            ),
            &p,
          );
        }
        continue;
      }
      // mode == Transfer
      if returned_root == Some(&*p) {
        continue;
      }
      if v.state == Owned {
        self.report(
          Some(&where_),
          "leak",
          format!("transferred argument [{}] not consumed on this path", p),
          &p,
        );
      } else if v.state == Conflict {
        self.report(
          Some(&where_),
          "conflict",
          format!("argument [{}] consumed on some paths but not others", p),
          &p,
        );
      }
    }
    for (name, v) in env {
      if self.param_modes.contains_key(name) || self.frozen.contains(name) {
        continue;
      }
      if returned_root == Some(name.as_ref()) {
        continue;
      }
      if v.state == Owned {
        self.report(
          Some(&where_),
          "leak",
          format!("owned local [{}] not consumed on this path", name),
          name,
        );
      } else if v.state == Conflict {
        self.report(
          Some(&where_),
          "conflict",
          format!("local [{}] consumed on some paths but not others", name),
          name,
        );
      }
    }
  }

  // -- consumption / liveness

  fn poison_derived(&mut self, env: &mut Env, root: &str) {
    let keys: Vec<Name> = env
      .iter()
      .filter(|(_, v)| v.state == Borrowed && v.origins.contains(root))
      .map(|(k, _)| k.clone())
      .collect();
    for k in keys {
      env.insert(k, Val::new(Poisoned));
    }
  }

  fn use_check(&mut self, cur: &Cursor, name: &str, v: &Val) {
    if self.frozen.contains(name) {
      return;
    }
    if v.state == Poisoned {
      self.report(
        Some(cur),
        "use-after-free",
        format!("[{}] is derived from a noun already consumed on this path", name),
        name,
      );
    } else if v.state == Consumed {
      self.report(
        Some(cur),
        "use-after-free",
        format!("[{}] used after its reference was consumed", name),
        name,
      );
    }
  }

  fn drop_temp(&mut self, temp_id: Option<u64>) {
    if let Some(t) = temp_id {
      if let Some(i) = self.open_temps.iter().position(|x| *x == t) {
        self.open_temps.remove(i);
      }
    }
  }

  /// A counted reference to `val` is given away here.
  fn consume(&mut self, cur: &Cursor, env: &mut Env, val: &Val, what: Option<Name>) {
    self.drop_temp(val.temp_id);
    let mut name = what;
    if name.is_none() {
      // attribute by value provenance (e.g. the value came out of
      // a ternary over variables)
      let srcs: Vec<Name> = val
        .srcs
        .iter()
        .filter(|s| env.contains_key(s.as_ref()))
        .cloned()
        .collect();
      if srcs.len() == 1 {
        name = Some(srcs[0].clone());
      } else if srcs.len() > 1 {
        // one of several variables was consumed; we cannot tell
        // which, so stop tracking all of them
        for s in srcs {
          if !self.frozen.contains(&s) {
            env.insert(s, Val::new(Unknown));
          }
        }
        return;
      }
    }
    if let Some(n) = &name {
      if self.frozen.contains(n) {
        return;
      }
    }
    let disp = || match &name {
      Some(n) => n.to_string(),
      None => "?".to_string(),
    };
    match val.state {
      Direct | Unknown | Uninit => {}
      Owned => {
        if let Some(n) = &name {
          env.insert(n.clone(), Val::new(Consumed));
          let n = n.clone();
          self.poison_derived(env, &n);
        }
      }
      Borrowed => {
        let origins = if val.origins.is_empty() {
          "?".to_string()
        } else {
          val.origins.iter().cloned().collect::<Vec<_>>().join(", ")
        };
        self.report(
          Some(cur),
          "over-free",
          format!(
            "counted reference to retained/borrowed value [{}] given away (origins: {})",
            disp(),
            origins
          ),
          name.as_deref().unwrap_or(""),
        );
        if let Some(n) = &name {
          env.insert(n.clone(), Val::new(Consumed));
          for root in val.origins.iter().cloned().collect::<Vec<Name>>() {
            self.poison_derived(env, &root);
          }
        }
      }
      Consumed | Poisoned => {
        self.report(
          Some(cur),
          "double-free",
          format!("reference to [{}] consumed twice on this path", disp()),
          name.as_deref().unwrap_or(""),
        );
      }
      Conflict => {
        self.report(
          Some(cur),
          "conflict",
          format!(
            "[{}] consumed here but its ownership differs between paths",
            disp()
          ),
          name.as_deref().unwrap_or(""),
        );
      }
    }
  }

  // -- expression evaluation

  fn new_temp(&mut self) -> u64 {
    self.next_temp += 1;
    self.open_temps.push(self.next_temp);
    self.next_temp
  }

  /// An owned call product dropped in a context that does not count
  /// references (arithmetic, comparison, a non-noun sink) is leaked.
  fn discard_owned(&mut self, cur: &Cursor, val: &Val) {
    if val.temp_id.is_some() && val.state == Owned {
      self.report(
        Some(cur),
        "leak",
        "owned product discarded in a non-noun expression (reference is leaked)"
          .to_string(),
        "",
      );
      self.drop_temp(val.temp_id);
    }
  }

  fn eval_expr(&mut self, cur: &Cursor, env: &mut Env) -> R<Val> {
    let cur = unwrap_expr(*cur);
    let k = cur.kind();

    if k == CXCursor_IntegerLiteral || k == CXCursor_CharacterLiteral {
      return Ok(Val::new(Direct));
    }
    if k == CXCursor_StringLiteral {
      return Ok(Val::new(Unknown));
    }
    if k == CXCursor_DeclRefExpr {
      let name = cur.spelling();
      if let Some(v) = env.get(&name).cloned() {
        self.use_check(&cur, &name, &v);
        let mut out = Val::with_origins(v.state, v.origins);
        out.srcs = BTreeSet::from([name]);
        return Ok(out);
      }
      return Ok(Val::new(Direct)); // enum constants, globals treated as opaque
    }
    if k == CXCursor_CallExpr {
      return self.eval_call(&cur, env);
    }
    if k == CXCursor_ConditionalOperator {
      let kids = cur.children();
      if kids.len() == 3 {
        let e0 = std::mem::take(env);
        let (t_envs, f_envs) = self.eval_cond(&kids[0], e0)?;
        let mut vals: Vec<Val> = Vec::new();
        let mut outs: Vec<Env> = Vec::new();
        for mut e in t_envs {
          vals.push(self.eval_expr(&kids[1], &mut e)?);
          outs.push(e);
        }
        for mut e in f_envs {
          vals.push(self.eval_expr(&kids[2], &mut e)?);
          outs.push(e);
        }
        if let Some(m) = merge_env(outs) {
          *env = m;
        }
        let mut it = vals.into_iter();
        let Some(mut out) = it.next() else {
          return Ok(Val::new(Unknown));
        };
        for v in it {
          out = merge_val(&out, &v);
        }
        return Ok(out);
      }
      return Ok(Val::new(Unknown));
    }
    if k == CXCursor_BinaryOperator {
      let op = cur.binop_kind();
      let kids = cur.children();
      if kids.len() != 2 {
        return Ok(Val::new(Unknown));
      }
      let (lhs, rhs) = (kids[0], kids[1]);
      if op == binop::ASSIGN {
        return self.eval_assign(&cur, &lhs, &rhs, env);
      }
      if op == binop::COMMA {
        self.eval_stmt_expr_result(&lhs, env)?;
        return self.eval_expr(&rhs, env);
      }
      let lv = self.eval_expr(&lhs, env)?;
      let rv = self.eval_expr(&rhs, env)?;
      // an owned call product used purely in arithmetic/comparison is
      // discarded here (its value is read, its reference dropped)
      if op != binop::LAND && op != binop::LOR {
        self.discard_owned(&cur, &lv);
        self.discard_owned(&cur, &rv);
      }
      return Ok(Val::new(Direct)); // arithmetic/comparison: not a counted noun
    }
    if k == CXCursor_CompoundAssignOperator {
      for c in cur.children() {
        self.eval_expr(&c, env)?;
      }
      return Ok(Val::new(Unknown));
    }
    if k == CXCursor_UnaryOperator {
      let op = unary_op(&cur);
      let kids = cur.children();
      let child = kids.first();
      if op.as_deref() == Some("&") {
        if let Some(ch) = child {
          if let Some(name) = decl_ref_name(ch) {
            if env.contains_key(&name) {
              env.insert(name, Val::new(Unknown)); // escapes; e.g. out-param
            }
          }
        }
        return Ok(Val::new(Unknown));
      }
      if let Some(ch) = child {
        self.eval_expr(ch, env)?;
      }
      return Ok(Val::new(if op.as_deref() == Some("*") { Unknown } else { Direct }));
    }
    if k == CXCursor_MemberRefExpr {
      if let Some(name) = decl_ref_name(&cur) {
        if let Some(v) = env.get(&name).cloned() {
          self.use_check(&cur, &name, &v);
          let mut out = Val::with_origins(v.state, v.origins);
          out.srcs = BTreeSet::from([name]);
          return Ok(out);
        }
      }
      for c in cur.children() {
        self.eval_expr(&c, env)?;
      }
      return Ok(Val::new(Unknown));
    }
    if k == CXCursor_ArraySubscriptExpr {
      for c in cur.children() {
        self.eval_expr(&c, env)?;
      }
      return Ok(Val::new(Unknown));
    }
    if k == CXCursor_CompoundStmt {
      // GNU statement-expression: execute the prefix statements,
      // then the value is the last expression's value (this is what
      // sees through c3_min/c3_max-style macros)
      let kids = cur.children();
      let mut flow = Flow { falls: vec![env.clone()], ..Default::default() };
      let n = kids.len();
      for child in kids.iter().take(n.saturating_sub(1)) {
        if flow.falls.is_empty() && self.goto_envs.is_empty() {
          break;
        }
        let nxt = self
          .exec_stmt(child, std::mem::take(&mut flow.falls))
          .map_err(Stop::Skip)?;
        flow.falls = nxt.falls;
        flow.brks.extend(nxt.brks);
        flow.conts.extend(nxt.conts);
      }
      let Some(m) = merge_env(flow.falls) else {
        return Ok(Val::new(Unknown));
      };
      *env = m;
      if let Some(last) = kids.last() {
        if is_expr_kind(last.kind()) {
          return self.eval_expr(last, env);
        }
        let f2 = self
          .exec_stmt(last, vec![env.clone()])
          .map_err(Stop::Skip)?;
        if let Some(m2) = merge_env(f2.falls) {
          *env = m2;
        }
      }
      return Ok(Val::new(Unknown));
    }
    if k == CXCursor_InitListExpr {
      for c in cur.children() {
        let v = self.eval_expr(&c, env)?;
        let nm = decl_ref_name(&c);
        self.consume(&cur, env, &v, nm);
      }
      return Ok(Val::new(Unknown));
    }
    // default: recurse
    for c in cur.children() {
      self.eval_expr(&c, env)?;
    }
    Ok(Val::new(Unknown))
  }

  fn eval_assign(&mut self, cur: &Cursor, lhs: &Cursor, rhs: &Cursor, env: &mut Env) -> R<Val> {
    let rv = self.eval_expr(rhs, env)?;
    let rname = decl_ref_name(rhs);
    let lname = decl_ref_name(lhs);
    if let Some(ln) = &lname {
      if !env.contains_key(ln)
        && is_local_lvalue(lhs)
        && matches!(rv.state, Owned | Borrowed | Consumed | Poisoned)
      {
        // lazily track locals the declaration pass missed: noun values
        // held in c3_w variables or struct members of local structs
        env.insert(ln.clone(), Val::new(Uninit));
      }
    }
    if let Some(ln) = &lname {
      if env.contains_key(ln) {
        let old_state = env[ln].state;
        if old_state == Owned && !self.frozen.contains(ln) {
          self.report(
            Some(cur),
            "leak",
            format!(
              "owned reference in [{}] overwritten without being consumed",
              ln
            ),
            ln,
          );
        }
        self.drop_temp(rv.temp_id);
        env.insert(ln.clone(), Val::with_origins(rv.state, rv.origins.clone()));
        // x = y moves ownership: y becomes an alias borrowed from x
        if let Some(rn) = &rname {
          if rn != ln && env.get(rn).map(|v| v.state) == Some(Owned) {
            env.insert(rn.clone(), Val::borrowed_from(ln));
          }
        }
        return Ok(env[ln].clone());
      }
    }
    // store through pointer / into struct or array
    let lhs_u = unwrap_expr(*lhs);
    if rv.state == Owned {
      if self.is_param_deref(&lhs_u) || self.assert_depth > 0 {
        // *out = product (ownership passes to the caller), or a
        // store blessed by an enclosing ASSERT block
        self.consume(cur, env, &rv, rname);
      } else if rname.as_deref().map(|r| self.frozen.contains(r)) == Some(true) {
        // frozen source: the assert block owns the accounting
      } else {
        self.report(
          Some(cur),
          "escape",
          "owned reference stored to memory; wrap in an \"@Refcount: assert transfer\" block if this store is the intended consumption"
            .to_string(),
          rname.as_deref().unwrap_or(""),
        );
        self.consume(cur, env, &rv, rname);
      }
    } else {
      self.eval_expr(lhs, env)?;
    }
    Ok(Val::with_origins(rv.state, rv.origins))
  }

  /// True for `*p = ...` where p is a pointer parameter.
  fn is_param_deref(&self, lhs: &Cursor) -> bool {
    if lhs.kind() != CXCursor_UnaryOperator {
      return false;
    }
    if unary_op(lhs).as_deref() != Some("*") {
      return false;
    }
    let kids = lhs.children();
    let name = match kids.first().and_then(decl_ref_name) {
      Some(n) => n,
      None => return false,
    };
    self.fun.arguments().iter().any(|p| p.spelling() == name)
  }

  /// Struct initializer: bind each element to a member path of the
  /// declared variable, moving ownership out of source variables.
  fn bind_init_list(&mut self, d: &Cursor, init: &Cursor, env: &mut Env) -> R<()> {
    let elems = init.children();
    let fields = d.ty().canonical().fields();
    if fields.is_empty() || elems.len() > fields.len() {
      // array or unsupported shape: evaluate and consume elements
      self.eval_expr(init, env)?;
      return Ok(());
    }
    for (f, e) in fields.iter().zip(elems.iter()) {
      let v = self.eval_expr(e, env)?;
      if !matches!(v.state, Owned | Borrowed | Consumed | Poisoned | Direct) {
        continue;
      }
      let key: Name = Rc::from(format!("{}.{}", d.spelling(), f.spelling()));
      self.drop_temp(v.temp_id);
      env.insert(key.clone(), Val::with_origins(v.state, v.origins.clone()));
      if let Some(ename) = decl_ref_name(e) {
        if ename != key && env.get(&ename).map(|v| v.state) == Some(Owned) {
          env.insert(ename, Val::borrowed_from(&key));
        }
      }
    }
    Ok(())
  }

  // -- calls

  fn eval_call(&mut self, cur: &Cursor, env: &mut Env) -> R<Val> {
    let callee = cur.referenced();
    let cname: Option<Name> = callee.as_ref().map(|c| c.spelling());
    let cn = cname.as_deref().unwrap_or("");
    let mut args = cur.arguments();
    if args.is_empty() {
      let kids = cur.children();
      args = kids.into_iter().skip(1).collect();
    }

    if config::NORETURN_FNS.contains(&cn) {
      for a in &args {
        self.eval_expr(a, env)?;
      }
      return Err(Stop::PathEnd);
    }

    if cn == "u3a_lose" {
      if let Some(a0) = args.first() {
        let v = self.eval_expr(a0, env)?;
        let nm = decl_ref_name(a0);
        self.consume(cur, env, &v, nm);
      }
      return Ok(Val::new(Direct));
    }
    if cn == "u3a_gain" || cn == "u3a_take" {
      if let Some(a0) = args.first() {
        self.eval_expr(a0, env)?;
      }
      let t = self.new_temp();
      let mut v = Val::new(Owned);
      v.temp_id = Some(t);
      return Ok(v);
    }
    if cn == "u3a_h" || cn == "u3a_t" {
      if let Some(a0) = args.first() {
        let v = self.eval_expr(a0, env)?;
        let name = decl_ref_name(a0);
        if v.state == Borrowed {
          return Ok(Val::with_origins(Borrowed, v.origins));
        }
        if matches!(v.state, Owned | Unknown | Direct) {
          if let Some(n) = &name {
            return Ok(Val::borrowed_from(n));
          }
        }
        let origins = if !v.origins.is_empty() {
          v.origins
        } else if let Some(n) = name {
          BTreeSet::from([n])
        } else {
          BTreeSet::new()
        };
        return Ok(Val::with_origins(Borrowed, origins));
      }
      return Ok(Val::new(Unknown));
    }
    if config::guard_kind(cn).is_some() {
      if let Some(a0) = args.first() {
        self.eval_expr(a0, env)?;
      }
      return Ok(Val::new(Direct));
    }
    if let Some(src_i) = config::destructurer_src(cn) {
      let mut src_name: Option<Name> = None;
      for (i, a) in args.iter().enumerate() {
        if i == src_i {
          self.eval_expr(a, env)?;
          src_name = decl_ref_name(a);
        } else {
          let au = unwrap_expr(*a);
          if au.kind() == CXCursor_UnaryOperator && unary_op(&au).as_deref() == Some("&")
          {
            let kids = au.children();
            if let Some(nm) = kids.first().and_then(decl_ref_name) {
              let origins = match &src_name {
                Some(s) => BTreeSet::from([s.clone()]),
                None => BTreeSet::new(),
              };
              env.insert(nm, Val::with_origins(Borrowed, origins));
              continue;
            }
          }
          self.eval_expr(a, env)?;
        }
      }
      return Ok(Val::new(Direct));
    }

    // generic call
    let mut semo: Option<Rc<Sem>> = None;
    let mut params: Vec<Cursor> = Vec::new();
    if let Some(cal) = &callee {
      if !cn.is_empty() {
        semo = Some(self.host.callee_sem(cal));
        // forward decls may have unnamed params
        let pcur = cal.definition().unwrap_or(*cal);
        params = pcur.arguments();
      }
    }

    // phase 1: evaluate all argument expressions (C evaluates every
    // operand before the call; consumption happens inside the callee)
    let mut evald: Vec<(Cursor, Option<Cursor>, Val)> = Vec::new();
    for (i, a) in args.iter().enumerate() {
      let p = params.get(i).copied();
      let v = self.eval_expr(a, env)?;
      evald.push((*a, p, v));
    }
    // phase 2: apply the callee's per-argument effects
    let mut pass_val: Option<Val> = None;
    for (a, p, v) in &evald {
      let p_noun = p.map(|p| is_noun_type(&p.ty())).unwrap_or(false);
      let aname = decl_ref_name(a);
      if let (Some(sem), Some(p)) = (&semo, p) {
        if sem.passthrough.as_deref() == Some(&*p.spelling()) {
          // identity: this argument's value IS the product;
          // counts are untouched
          pass_val = Some(v.clone());
          continue;
        }
      }
      if !p_noun {
        // an owned call product handed to a declared non-noun
        // parameter (e.g. u3a_malloc(u3kb_lent(..))) is leaked: its
        // value is used, its reference dropped. (&var to a pointer
        // param is handled in eval_expr('&'); varargs (p is None)
        // are too ambiguous to flag.)
        if p.is_some() {
          self.discard_owned(cur, v);
        }
        continue;
      }
      let is_custom = semo.as_ref().map(|s| s.custom).unwrap_or(true);
      if is_custom {
        if let Some(an) = &aname {
          if env.contains_key(an) {
            env.insert(an.clone(), Val::new(Unknown));
          }
        }
        continue;
      }
      let sem = semo.as_ref().unwrap().clone();
      let pname = p.map(|p| p.spelling()).unwrap_or_else(|| Rc::from(""));
      if sem.is_direct(&pname) {
        // the callee bails unless this argument is a direct atom;
        // on return its reference carries no count
        self.drop_temp(v.temp_id);
        if let Some(an) = &aname {
          if env.get(an).map(|x| matches!(x.state, Owned | Borrowed | Unknown))
            == Some(true)
          {
            env.insert(an.clone(), Val::new(Direct));
          }
        }
        continue;
      }
      let mode = sem.arg_mode(&pname);
      if mode == Mode::Transfer {
        self.consume(cur, env, v, aname);
      } else {
        // retained: liveness only; owned temporaries leak
        if v.temp_id.is_some() && v.state == Owned {
          self.report(
            Some(cur),
            "leak",
            format!(
              "owned product passed to retaining parameter of {}(); reference is leaked",
              cn
            ),
            aname.as_deref().unwrap_or(""),
          );
          self.drop_temp(v.temp_id);
        }
      }
    }

    if let Some(pv) = pass_val {
      return Ok(pv);
    }
    if let Some(cal) = &callee {
      let rt = cal.result_type();
      if is_noun_type(&rt) {
        let is_custom = semo.as_ref().map(|s| s.custom).unwrap_or(true);
        if is_custom {
          return Ok(Val::new(Unknown));
        }
        let sem = semo.as_ref().unwrap();
        if sem.product == Mode::Transfer {
          let t = self.new_temp();
          let mut v = Val::new(Owned);
          v.temp_id = Some(t);
          return Ok(v);
        }
        let mut roots: BTreeSet<Name> = BTreeSet::new();
        for a in &args {
          if let Some(nm) = decl_ref_name(a) {
            if let Some(v) = env.get(&nm) {
              if matches!(v.state, Owned | Borrowed) {
                if v.origins.is_empty() {
                  roots.insert(nm);
                } else {
                  roots.extend(v.origins.iter().cloned());
                }
              }
            }
          }
        }
        return Ok(Val::with_origins(Borrowed, roots));
      }
    }
    Ok(Val::new(Unknown))
  }

  // -- conditions

  /// Returns (true_envs, false_envs).
  fn eval_cond(&mut self, cur: &Cursor, env: Env) -> R<(Vec<Env>, Vec<Env>)> {
    let cur = unwrap_expr(*cur);
    if let Some(lit) = int_literal_value(&cur) {
      // constant condition: while(1) never falls out, etc.
      return Ok(if lit != 0 {
        (vec![env], vec![])
      } else {
        (vec![], vec![env])
      });
    }
    let k = cur.kind();
    // see through __builtin_expect (c3_likely/c3_unlikely)
    if k == CXCursor_CallExpr {
      if let Some(r) = cur.referenced() {
        if &*r.spelling() == "__builtin_expect" {
          let args = cur.arguments();
          if let Some(a0) = args.first() {
            return self.eval_cond(a0, env);
          }
        }
      }
    }
    if k == CXCursor_UnaryOperator && unary_op(&cur).as_deref() == Some("!") {
      let kids = cur.children();
      if let Some(k0) = kids.first() {
        let (t, f) = self.eval_cond(k0, env)?;
        return Ok((f, t));
      }
    }
    if k == CXCursor_BinaryOperator {
      let op = cur.binop_kind();
      let kids = cur.children();
      if op == binop::LAND && kids.len() == 2 {
        let (lt, lf) = self.eval_cond(&kids[0], env)?;
        let (mut tt, mut tf) = (Vec::new(), Vec::new());
        for e in lt {
          let (t2, f2) = self.eval_cond(&kids[1], e)?;
          tt.extend(t2);
          tf.extend(f2);
        }
        let mut falses = lf;
        falses.extend(tf);
        return Ok((tt, falses));
      }
      if op == binop::LOR && kids.len() == 2 {
        let (lt, lf) = self.eval_cond(&kids[0], env)?;
        let (mut ft, mut ff) = (Vec::new(), Vec::new());
        for e in lf {
          let (t2, f2) = self.eval_cond(&kids[1], e)?;
          ft.extend(t2);
          ff.extend(f2);
        }
        let mut trues = lt;
        trues.extend(ft);
        return Ok((trues, ff));
      }
      if (op == binop::EQ || op == binop::NE) && kids.len() == 2 {
        let fact = self.guard_fact(&kids[0], &kids[1], &env);
        let mut env = env;
        self.eval_expr(&kids[0], &mut env)?;
        self.eval_expr(&kids[1], &mut env)?;
        let mut te = env.clone();
        let mut fe = env;
        if let Some((name, eq_ref, ne_ref)) = fact {
          // te is the branch where the condition is true; for EQ
          // that is the equal case, for NE the not-equal case
          let (true_ref, false_ref) = if op == binop::EQ {
            (eq_ref, ne_ref)
          } else {
            (ne_ref, eq_ref)
          };
          if let Some(r) = true_ref {
            te.insert(name.clone(), r);
          }
          if let Some(r) = false_ref {
            fe.insert(name, r);
          }
        }
        return Ok((vec![te], vec![fe]));
      }
      if matches!(op, binop::LT | binop::GT | binop::LE | binop::GE) && kids.len() == 2 {
        let facts = self.bound_fact(op, &kids[0], &kids[1], &env);
        let mut env = env;
        self.eval_expr(&kids[0], &mut env)?;
        self.eval_expr(&kids[1], &mut env)?;
        let mut te = env.clone();
        let mut fe = env;
        for (name, on_true) in facts {
          if on_true {
            te.insert(name, Val::new(Direct));
          } else {
            fe.insert(name, Val::new(Direct));
          }
        }
        return Ok((vec![te], vec![fe]));
      }
    }
    // bare truthiness of a noun variable: false branch implies 0,
    // which is a direct atom with no counted references
    if k == CXCursor_DeclRefExpr && env.contains_key(&cur.spelling()) {
      let name = cur.spelling();
      let mut env = env;
      self.eval_expr(&cur, &mut env)?;
      let te = env.clone();
      let mut fe = env;
      if let Some(v) = fe.get(&name) {
        if matches!(v.state, Owned | Borrowed | Unknown | Uninit) {
          fe.insert(name, Val::new(Direct));
        }
      }
      return Ok((vec![te], vec![fe]));
    }
    // generic condition
    let mut env = env;
    self.eval_expr(&cur, &mut env)?;
    Ok((vec![env.clone()], vec![env]))
  }

  /// For a relational comparison, return (var, branch) refinements: the
  /// variable is provably a direct atom (bounded below 2^31) on that
  /// branch (true = comparison-true branch).
  fn bound_fact(&self, op: i32, a: &Cursor, b: &Cursor, env: &Env) -> Vec<(Name, bool)> {
    let name_a = decl_ref_name(a);
    let lit_b = int_literal_value(b);
    let lit_a = int_literal_value(a);
    let name_b = decl_ref_name(b);

    let refinable = |n: &str| {
      env.get(n)
        .map(|v| matches!(v.state, Owned | Borrowed | Unknown | Uninit))
        == Some(true)
    };
    let is_direct = |n: &str| env.get(n).map(|v| v.state == Direct) == Some(true);

    if let (Some(na), Some(lb)) = (&name_a, lit_b) {
      // var < lit (true), var <= lit (true),
      // var > lit (false), var >= lit (false)
      let on_true = matches!(op, binop::LT | binop::LE);
      let bound_incl = matches!(op, binop::LE | binop::GT); // bound is <= lit
      let limit = if bound_incl {
        config::DIRECT_MAX
      } else {
        config::DIRECT_MAX + 1
      };
      if lb <= limit && refinable(na) {
        return vec![(na.clone(), on_true)];
      }
      return vec![];
    }
    if let (Some(nb), Some(la)) = (&name_b, lit_a) {
      // lit > var (true), lit >= var (true),
      // lit < var (false), lit <= var (false)
      let on_true = matches!(op, binop::GT | binop::GE);
      let bound_incl = matches!(op, binop::GE | binop::LT);
      let limit = if bound_incl {
        config::DIRECT_MAX
      } else {
        config::DIRECT_MAX + 1
      };
      if la <= limit && refinable(nb) {
        return vec![(nb.clone(), on_true)];
      }
      return vec![];
    }
    if let (Some(na), Some(nb)) = (&name_a, &name_b) {
      // var-vs-var: on the branch where x <= y, a direct y bounds
      // x below 2^31 (this is what verifies the c3_min pattern:
      // `(_x < _y) ? _x : _y` with one operand proven direct)
      let (small_t, big_t) = if matches!(op, binop::LT | binop::LE) {
        (na, nb) // true: a bounded by b
      } else {
        (nb, na) // a > b: true: b <= a
      };
      let mut facts = Vec::new();
      if is_direct(big_t) && refinable(small_t) {
        facts.push((small_t.clone(), true));
      }
      if is_direct(small_t) && refinable(big_t) {
        facts.push((big_t.clone(), false));
      }
      return facts;
    }
    vec![]
  }

  /// For a comparison a==b, return (var, eq_refine, ne_refine): the Val
  /// to assign var when the operands are equal / not-equal (either may be
  /// None). Both branches matter: `c3n == u3a_is_cat(x)` proves x direct
  /// on the NOT-equal branch (there u3a_is_cat(x) == c3y).
  fn guard_fact(
    &self,
    a: &Cursor,
    b: &Cursor,
    env: &Env,
  ) -> Option<(Name, Option<Val>, Option<Val>)> {
    for (x, y) in [(a, b), (b, a)] {
      let lit = int_literal_value(x);
      let mut name = decl_ref_name(y);
      if name.is_none() {
        // look through an assignment: (name = expr)
        let yu = unwrap_expr(*y);
        if yu.kind() == CXCursor_BinaryOperator && yu.binop_kind() == binop::ASSIGN {
          if let Some(k0) = yu.children().first() {
            name = decl_ref_name(k0);
          }
        }
      }
      if let (Some(l), Some(n)) = (lit, &name) {
        if env.contains_key(n) {
          if l <= config::DIRECT_MAX || l == config::U3_NONE {
            let old = &env[n];
            if matches!(old.state, Owned | Borrowed | Unknown | Uninit) {
              // equal to a direct literal => direct on that branch
              return Some((n.clone(), Some(Val::new(Direct)), None));
            }
          }
          return None;
        }
      }
      // c3y/c3n == u3a_is_cat/dog(var)
      let yc = unwrap_expr(*y);
      if let Some(l) = lit {
        if yc.kind() == CXCursor_CallExpr {
          if let Some(r) = yc.referenced() {
            if let Some(kind) = config::guard_kind(&r.spelling()) {
              let gargs = yc.arguments();
              let gname = gargs.first().and_then(decl_ref_name);
              if let Some(gn) = gname {
                if env.contains_key(&gn) {
                  if kind != "cat" && kind != "dog" {
                    return None; // is_atom/is_cell etc. don't imply direct
                  }
                  let old = &env[&gn];
                  if !matches!(old.state, Owned | Borrowed | Unknown) {
                    return None;
                  }
                  // the guard is direct exactly when it reads c3y (cat)
                  // or c3n (dog); the equal branch is where it reads `lit`
                  let direct_on_true = kind == "cat";
                  let (eq_d, ne_d) = if l == config::C3Y {
                    (direct_on_true, !direct_on_true)
                  } else if l == config::C3N {
                    (!direct_on_true, direct_on_true)
                  } else {
                    return None;
                  };
                  let eq_r = if eq_d { Some(Val::new(Direct)) } else { None };
                  let ne_r = if ne_d { Some(Val::new(Direct)) } else { None };
                  if eq_r.is_none() && ne_r.is_none() {
                    return None;
                  }
                  return Some((gn, eq_r, ne_r));
                }
              }
              return None;
            }
          }
        }
      }
    }
    None
  }

  // -- statement-level result handling

  fn eval_stmt_expr_result(&mut self, cur: &Cursor, env: &mut Env) -> R<()> {
    // bare `u3k(x);` increments x's count in place: x becomes owned
    let u = unwrap_expr(*cur);
    if u.kind() == CXCursor_CallExpr {
      if let Some(r) = u.referenced() {
        let rs = r.spelling();
        if &*rs == "u3a_gain" || &*rs == "u3a_take" {
          let gargs = u.arguments();
          if let Some(gn) = gargs.first().and_then(decl_ref_name) {
            if let Some(v) = env.get(&gn).cloned() {
              self.use_check(&u, &gn, &v);
              if matches!(v.state, Borrowed | Unknown) {
                env.insert(gn, Val::new(Owned));
              }
              return Ok(());
            }
          }
        }
      }
    }
    let v = self.eval_expr(cur, env)?;
    if v.temp_id.is_some() && v.state == Owned {
      self.report(
        Some(cur),
        "leak",
        "owned product of call discarded (product of a transferring function must be consumed)"
          .to_string(),
        "",
      );
      self.drop_temp(v.temp_id);
    }
    Ok(())
  }

  // -- statements

  /// Execute statement over each env; returns Flow.
  fn exec_stmt(&mut self, cur: &Cursor, envs: Vec<Env>) -> Result<Flow, String> {
    if cur.kind() == CXCursor_LabelStmt {
      // a label is a join point: fall-through paths meet the paths
      // parked by forward gotos targeting it (handled here rather
      // than exec_one so it fires even with zero incoming envs)
      let parked = self
        .goto_envs
        .shift_remove(&cur.spelling())
        .unwrap_or_default();
      let mut all = envs;
      all.extend(parked);
      let Some(m) = merge_env(all) else {
        return Ok(Flow::default());
      };
      let kids = cur.children();
      let Some(sub) = kids.last() else {
        return Ok(Flow { falls: vec![m], ..Default::default() });
      };
      return self.exec_stmt(sub, vec![m]);
    }
    let mut out = Flow::default();
    for env in envs {
      match self.exec_one(cur, env) {
        Ok(f) => {
          out.falls.extend(f.falls);
          out.brks.extend(f.brks);
          out.conts.extend(f.conts);
        }
        Err(Stop::PathEnd) => {}
        Err(Stop::Skip(r)) => return Err(r),
      }
    }
    Ok(out)
  }

  /// Begin a block-assert scope for a compound statement: freeze the
  /// named variables (snapshotting their states) and raise the depth
  /// that blesses owned stores.
  fn assert_enter(&mut self, compound: &Cursor, env: &Env) -> AssertCtx {
    let asserts: Vec<(AssertMode, Vec<Name>)> = self
      .host
      .block_asserts(compound)
      .into_iter()
      .map(|(m, ns)| (m, ns.into_iter().map(Rc::from).collect()))
      .collect();
    let mut frozen_here = Vec::new();
    let mut snapshots = HashMap::new();
    for (_, names) in &asserts {
      for n in names {
        if !self.frozen.contains(n) {
          frozen_here.push(n.clone());
          self.frozen.insert(n.clone());
        }
        snapshots.insert(n.clone(), env.get(n).cloned());
      }
    }
    if !asserts.is_empty() {
      self.assert_depth += 1;
    }
    AssertCtx { asserts, frozen_here, snapshots }
  }

  /// End a block-assert scope: unfreeze and apply the declared effects
  /// to every surviving environment (fall-throughs and breaks).
  fn assert_exit(&mut self, ctx: AssertCtx, flow: &mut Flow) {
    if !ctx.asserts.is_empty() {
      self.assert_depth -= 1;
    }
    for n in &ctx.frozen_here {
      self.frozen.remove(n);
    }
    for (mode, names) in &ctx.asserts {
      for n in names {
        for e in flow.falls.iter_mut().chain(flow.brks.iter_mut()) {
          match mode {
            AssertMode::Transfer => {
              e.insert(n.clone(), Val::new(Consumed));
            }
            AssertMode::Produce => {
              e.insert(n.clone(), Val::new(Owned));
            }
            AssertMode::Retain => {
              if let Some(Some(snap)) = ctx.snapshots.get(n) {
                e.insert(n.clone(), snap.clone());
              }
            }
          }
        }
      }
    }
  }

  fn exec_one(&mut self, cur: &Cursor, env: Env) -> R<Flow> {
    let k = cur.kind();

    if k == CXCursor_CompoundStmt {
      let ctx = self.assert_enter(cur, &env);
      let mut flow = Flow { falls: vec![env], ..Default::default() };
      for child in cur.children() {
        if flow.falls.is_empty() && self.goto_envs.is_empty() {
          break; // all paths ended and no goto can resurrect one
        }
        let nxt = self
          .exec_stmt(&child, std::mem::take(&mut flow.falls))
          .map_err(Stop::Skip)?;
        flow.falls = nxt.falls;
        flow.brks.extend(nxt.brks);
        flow.conts.extend(nxt.conts);
      }
      self.assert_exit(ctx, &mut flow);
      return Ok(flow);
    }

    if k == CXCursor_DeclStmt {
      let mut env = env;
      for d in cur.children() {
        if d.kind() != CXCursor_VarDecl {
          continue;
        }
        let init = d.children().into_iter().last();
        match init {
          Some(i) if i.kind() == CXCursor_InitListExpr => {
            self.bind_init_list(&d, &i, &mut env)?;
          }
          Some(i) if i.kind() != CXCursor_TypeRef => {
            let v = self.eval_expr(&i, &mut env)?;
            // noun-spelled decls always track; other locals track
            // when the initializer value is interesting (owned
            // products in c3_w, `typeof(a) _x = a` in c3_min-style
            // macros loading from tracked variables)
            let track = is_noun_type(&d.ty())
              || matches!(v.state, Owned | Borrowed | Consumed | Poisoned)
              || (v.state == Direct && !v.srcs.is_empty());
            if track {
              self.drop_temp(v.temp_id);
              let dname = d.spelling();
              env.insert(
                dname.clone(),
                Val::with_origins(v.state, v.origins.clone()),
              );
              // `u3_noun cur = owned;` declares a borrowing
              // cursor: ownership stays with the source (unlike
              // assignment to an existing var, which moves it)
              if let Some(iname) = decl_ref_name(&i) {
                if iname != dname
                  && env.get(&iname).map(|x| x.state) == Some(Owned)
                {
                  env.insert(dname, Val::borrowed_from(&iname));
                }
              }
            }
          }
          _ => {
            if is_noun_type(&d.ty()) {
              env.insert(d.spelling(), Val::new(Uninit));
            }
          }
        }
      }
      return Ok(Flow { falls: vec![env], ..Default::default() });
    }

    if k == CXCursor_IfStmt {
      let kids = cur.children();
      let then = kids.get(1).copied();
      let els = kids.get(2).copied();
      let (t_envs, f_envs) = match kids.first() {
        Some(cond) => self.eval_cond(cond, env)?,
        None => {
          let c = env.clone();
          (vec![env], vec![c])
        }
      };
      let ft = match &then {
        Some(t) => self.exec_stmt(t, t_envs).map_err(Stop::Skip)?,
        None => Flow { falls: t_envs, ..Default::default() },
      };
      let fe = match &els {
        Some(e) => self.exec_stmt(e, f_envs).map_err(Stop::Skip)?,
        None => Flow { falls: f_envs, ..Default::default() },
      };
      let mut all = ft.falls;
      all.extend(fe.falls);
      let mut flow = Flow::default();
      if let Some(m) = merge_env(all) {
        flow.falls = vec![m];
      }
      flow.brks = ft.brks;
      flow.brks.extend(fe.brks);
      flow.conts = ft.conts;
      flow.conts.extend(fe.conts);
      return Ok(flow);
    }

    if matches!(k, CXCursor_WhileStmt | CXCursor_ForStmt | CXCursor_DoStmt) {
      return self.exec_loop(cur, env);
    }

    if k == CXCursor_SwitchStmt {
      let kids = cur.children();
      if kids.is_empty() {
        return Ok(Flow { falls: vec![env], ..Default::default() });
      }
      let body = *kids.last().unwrap();
      let mut env = env;
      self.eval_expr(&kids[0], &mut env)?;
      // per-arm forking: execution may enter at every case/default
      // label with the switch-entry state, in addition to falling
      // through from the previous arm. Without the per-label seed,
      // an early-terminating arm (default: return u3m_bail(..)
      // listed first) would starve every later arm and kill all
      // code after the switch.
      let entry = env.clone();
      let ctx = self.assert_enter(&body, &env); // asserts on the body brace
      let mut flow = Flow::default();
      let mut has_default = false;
      for child in body.children() {
        if matches!(child.kind(), CXCursor_CaseStmt | CXCursor_DefaultStmt) {
          if child.kind() == CXCursor_DefaultStmt {
            has_default = true;
          }
          flow.falls.push(entry.clone());
        }
        if flow.falls.is_empty() && self.goto_envs.is_empty() {
          continue; // dead zone between arms; keep scanning
        }
        let nxt = self
          .exec_stmt(&child, std::mem::take(&mut flow.falls))
          .map_err(Stop::Skip)?;
        flow.falls = nxt.falls;
        flow.brks.extend(nxt.brks);
        flow.conts.extend(nxt.conts);
      }
      self.assert_exit(ctx, &mut flow);
      // nested default labels (inside stacked cases) still count
      fn find_default(c: &Cursor) -> bool {
        if c.kind() == CXCursor_DefaultStmt {
          return true;
        }
        if c.kind() == CXCursor_SwitchStmt {
          return false; // a nested switch's default is its own
        }
        c.children().iter().any(find_default)
      }
      if !has_default {
        has_default = body.children().iter().any(find_default);
      }
      // the switch may also match nothing -- unless there is a
      // default case
      let mut all = flow.falls;
      all.extend(flow.brks);
      if !has_default {
        all.push(entry);
      }
      let mut out = Flow { conts: flow.conts, ..Default::default() };
      if let Some(m) = merge_env(all) {
        out.falls = vec![m];
      }
      return Ok(out);
    }

    if matches!(k, CXCursor_CaseStmt | CXCursor_DefaultStmt) {
      let kids = cur.children();
      if let Some(sub) = kids.last() {
        if sub.kind() != CXCursor_IntegerLiteral {
          return self.exec_stmt(sub, vec![env]).map_err(Stop::Skip);
        }
      }
      return Ok(Flow { falls: vec![env], ..Default::default() });
    }

    if k == CXCursor_ReturnStmt {
      let kids = cur.children();
      let mut env = env;
      let mut root: Option<Name> = None;
      if let Some(k0) = kids.first() {
        let v = self.eval_expr(k0, &mut env)?;
        root = decl_ref_name(k0);
        if root.is_none() && v.srcs.len() == 1 {
          // the returned value came straight out of a variable
          // (e.g. a passthrough call `return u3z_save(.., pro)`);
          // that variable is what is handed back, not leaked
          let s = v.srcs.iter().next().unwrap();
          if env.contains_key(s) {
            root = Some(s.clone());
          }
        }
        self.check_return_val(cur, &v, root.as_deref());
        self.drop_temp(v.temp_id);
      }
      self.check_exit(&env, cur, root.as_deref(), Some(cur));
      return Err(Stop::PathEnd);
    }

    if k == CXCursor_BreakStmt {
      return Ok(Flow { brks: vec![env], ..Default::default() });
    }
    if k == CXCursor_ContinueStmt {
      return Ok(Flow { conts: vec![env], ..Default::default() });
    }
    if k == CXCursor_GotoStmt {
      let kids = cur.children();
      let target = kids
        .first()
        .filter(|c| c.kind() == CXCursor_LabelRef)
        .map(|c| c.spelling());
      let Some(target) = target else {
        return Err(Stop::Skip("unresolvable goto".to_string()));
      };
      let Some(&tpos) = self.label_pos.get(&target) else {
        return Err(Stop::Skip("unresolvable goto".to_string()));
      };
      if tpos <= cur.extent_start().offset {
        // jumping to an earlier label forms a loop the walker
        // cannot model
        return Err(Stop::Skip("backward goto (loop)".to_string()));
      }
      // forward goto: park this path; it resumes at the label
      let mut bucket = self.goto_envs.shift_remove(&target).unwrap_or_default();
      bucket.push(env);
      let parked: Vec<Env> = merge_env(bucket).into_iter().collect();
      self.goto_envs.insert(target, parked);
      return Ok(Flow::default());
    }
    if k == CXCursor_IndirectGotoStmt {
      return Err(Stop::Skip("computed goto".to_string()));
    }
    if k == CXCursor_LabelStmt {
      return self.exec_stmt(cur, vec![env]).map_err(Stop::Skip); // handled in exec_stmt
    }
    if k == CXCursor_NullStmt || k == CXCursor_AsmStmt {
      return Ok(Flow { falls: vec![env], ..Default::default() });
    }

    // expression statement or anything else expression-like
    if is_expr_kind(k) {
      let mut env = env;
      self.eval_stmt_expr_result(cur, &mut env)?;
      return Ok(Flow { falls: vec![env], ..Default::default() });
    }

    // unknown statement kind: recurse conservatively
    let mut flow = Flow { falls: vec![env], ..Default::default() };
    for child in cur.children() {
      if flow.falls.is_empty() && self.goto_envs.is_empty() {
        break;
      }
      let nxt = self
        .exec_stmt(&child, std::mem::take(&mut flow.falls))
        .map_err(Stop::Skip)?;
      flow.falls = nxt.falls;
      flow.brks.extend(nxt.brks);
      flow.conts.extend(nxt.conts);
    }
    Ok(flow)
  }

  fn exec_loop(&mut self, cur: &Cursor, env: Env) -> R<Flow> {
    let k = cur.kind();
    let kids = cur.children();
    if kids.is_empty() {
      return Ok(Flow { falls: vec![env], ..Default::default() });
    }
    let (mut cond, mut init, mut inc): (Option<Cursor>, Option<Cursor>, Option<Cursor>) =
      (None, None, None);
    let body: Cursor;
    if k == CXCursor_WhileStmt {
      cond = Some(kids[0]);
      body = *kids.last().unwrap();
    } else if k == CXCursor_DoStmt {
      body = kids[0];
      cond = Some(*kids.last().unwrap());
    } else {
      // FOR_STMT: children vary; last is body
      body = *kids.last().unwrap();
      // heuristics: first non-expression child is init (DECL_STMT),
      // condition is a comparison-like expr, inc is the rest
      for c in &kids[..kids.len() - 1] {
        if c.kind() == CXCursor_DeclStmt && init.is_none() {
          init = Some(*c);
        } else if cond.is_none() {
          cond = Some(*c);
        } else {
          inc = Some(*c);
        }
      }
      // note: for-loop part identification is approximate; over-
      // approximating by treating extra parts as plain expressions
    }
    let envs = match &init {
      Some(i) => self.exec_stmt(i, vec![env]).map_err(Stop::Skip)?.falls,
      None => vec![env],
    };
    let Some(mut head) = merge_env(envs) else {
      return Ok(Flow::default());
    };
    let mut exits: Vec<Env> = Vec::new();
    if k == CXCursor_DoStmt {
      // run body once unconditionally first; the condition sees only
      // the post-body state (the entry state never reaches it, so a
      // do/while(0) preserves refinements made inside the body)
      let f = self.exec_stmt(&body, vec![head.clone()]).map_err(Stop::Skip)?;
      exits.extend(f.brks);
      let mut back_envs = f.falls;
      back_envs.extend(f.conts);
      match merge_env(back_envs) {
        None => {
          // body never falls through to the condition
          let mut out = Flow::default();
          if let Some(m) = merge_env(exits) {
            out.falls = vec![m];
          }
          return Ok(out);
        }
        Some(b) => head = b,
      }
    }
    for _ in 0..8 {
      let (t_envs, f_envs) = match &cond {
        Some(c) if c.kind() != CXCursor_CompoundStmt => {
          self.eval_cond(c, head.clone())?
        }
        _ => (vec![head.clone()], vec![]),
      };
      let f = self.exec_stmt(&body, t_envs).map_err(Stop::Skip)?;
      let back_envs = match &inc {
        Some(i) => {
          let mut pre = f.falls;
          pre.extend(f.conts);
          self.exec_stmt(i, pre).map_err(Stop::Skip)?.falls
        }
        None => {
          let mut pre = f.falls;
          pre.extend(f.conts);
          pre
        }
      };
      exits.extend(f.brks);
      let back = merge_env(back_envs);
      let new_head = match back {
        None => head.clone(),
        Some(b) => merge_env(vec![head.clone(), b]).unwrap(),
      };
      let exits_now = f_envs;
      if env_key(&new_head) == env_key(&head) {
        let mut all = exits_now;
        all.extend(std::mem::take(&mut exits));
        let mut out = Flow::default();
        if let Some(m) = merge_env(all) {
          out.falls = vec![m];
        }
        return Ok(out);
      }
      head = new_head;
    }
    // did not stabilize; be conservative
    let mut all = vec![head];
    all.extend(exits);
    let mut out = Flow::default();
    if let Some(m) = merge_env(all) {
      out.falls = vec![m];
    }
    Ok(out)
  }

  fn check_return_val(&mut self, cur: &Cursor, v: &Val, root: Option<&str>) {
    if let Some(pt) = &self.sem.passthrough {
      if root == Some(pt.as_str()) || v.srcs.contains(pt.as_str()) {
        // identity function: the product's ownership is, by definition,
        // whatever the passed-through argument's was; nothing to check
        return;
      }
    }
    if self.sem.product == Mode::Transfer {
      if v.state == Borrowed && root.map(|r| self.frozen.contains(r)) != Some(true) {
        let derived = if v.origins.is_empty() {
          String::new()
        } else {
          format!(
            " derived from [{}]",
            v.origins.iter().cloned().collect::<Vec<_>>().join(", ")
          )
        };
        self.report(
          Some(cur),
          "return-borrowed",
          format!(
            "transfer-product function returns an uncounted (retained) reference{}",
            derived
          ),
          "",
        );
      } else if matches!(v.state, Consumed | Poisoned) {
        self.report(
          Some(cur),
          "use-after-free",
          "returning a value whose reference was already consumed".to_string(),
          "",
        );
      }
    } else if self.sem.product == Mode::Retain && v.state == Owned {
      self.report(
        Some(cur),
        "leak",
        "retain-product function returns a counted reference (caller will not free it)"
          .to_string(),
        "",
      );
    }
  }
}
