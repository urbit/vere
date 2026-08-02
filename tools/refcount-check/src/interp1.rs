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
#![allow(unused)]

use std::cell::Ref;
use std::collections::{BTreeSet, HashMap, HashSet};
use std::rc::Rc;
use std::todo;
use imbl::{HashMap as IHashMap, HashSet as IHashSet, Vector as IVec};

use clang_sys::*;
use indexmap::IndexMap;

use crate::ast::{
  binop, decl_ref_name, int_literal_value, is_expr_kind, is_local_lvalue,
  is_noun_type, unary_op, unwrap_expr, Cursor, Name,
};
use crate::config;
use crate::sem::AssertMode::{Retain, Transfer};
use crate::sem::{AssertMode, Finding, Mode, Sem};

/// Services the interpreter needs from the enclosing tool.
pub trait Host {
  /// Resolved refcount protocol of a callee (annotation + defaults).
  fn callee_sem(&mut self, callee: &Cursor) -> Rc<Sem>;
  /// `{ // @Refcount: assert ... }` annotations on a compound statement.
  fn block_asserts(&self, compound: &Cursor)
    -> Vec<(AssertMode, Vec<String>)>;
}

type ValId = u32;
#[derive(Clone)]
enum RefcountState {
  Uninit,             // not initialized yet
  Borrowed,           // correctly borrowed
  Owned {rc: u32},    // correctly owned, rc > 0
  // Conflict,           // inconsistent values across branches
  Poisoned,           // consumed, not valid to use
  Direct,             // direct atom, no refcounting
}

#[derive(Clone, Hash, PartialEq, Eq)]
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
  locations: IHashMap<ValId, IHashSet<VarName>>,
  locations_rev: IHashMap<VarName, ValId>,
}

#[derive(Default, Clone)]
struct Flow {
  local: Option<Env>,                   // local environment, if code is reachable
  goto_envs: IHashMap<Name, IVec<Env>>, // environments from goto, for goto labels
  exit_envs: IVec<Env>,                 // environments from return, for exit
}

type R<T> = Result<T, Vec<Finding>>;

impl Flow {
  fn scope_done(mut self, depth: u32, g: &mut Gen)
    -> R<Flow>
  {
    self.local = self.local.map(|e| end_scope(e, depth, g)).transpose()?;
    Ok(self)
  }

  fn change_local(&self, local: Option<Env>) -> Flow
  {
    return Flow {
      local,
      goto_envs: self.goto_envs.clone(),
      exit_envs: self.exit_envs.clone()
    };
  }

  fn join(self, another: Option<Env>) -> R<Flow>
  {
    return Ok(Flow {
      local: mayb_join(self.local, another)?,
      goto_envs: self.goto_envs,
      exit_envs: self.exit_envs,
    })
  }
}


#[derive(Default, Clone)]
struct LoopFlow {
  local: Option<Env>,
  goto_envs: IHashMap<Name, IVec<Env>>,
  exit_envs: IVec<Env>,
  break_envs: IVec<Env>,
  cont_envs: IVec<Env>,
}

struct SwitchFlow {
  local: Option<Env>,
  goto_envs: IHashMap<Name, IVec<Env>>,
  exit_envs: IVec<Env>,
  break_envs: IVec<Env>,
}

impl Env {
  fn insert_new(&mut self, name: VarName, rc: RefcountState, id_gen: &mut ValId) {
    let id: ValId = *id_gen; *id_gen += 1;

    self.values.update(id, rc);
    let locs: IHashSet<VarName> = IHashSet::from([name.clone()]);
    self.locations.update(id, locs);
    self.locations_rev.update(name, id);
  }
}

struct Gen<'a> {
  func_cur: &'a Cursor,
  funcname: Name,
  id_gen: u32,
  goto_labels_allowed: bool,
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
  let mut id_gen: ValId = 0;
  for p in fun.arguments() {
    if p.kind() != CXCursor_ParmDecl || p.spelling().is_empty() {
      continue;
    }
    if is_noun_type(&p.ty()) {
      let pname = p.spelling();
      let mode: Mode = sem.arg_mode(&pname);
      let rc = match mode {
        Mode::Transfer => RefcountState::Owned { rc: 1 },
        Mode::Retain => RefcountState::Borrowed,
      };
      env.insert_new(VarName{name: pname, depth: 0}, rc, &mut id_gen);
    }
  }

  let flo: Flow = Flow {local: Some(env), ..Default::default()};
  let mut g = Gen {func_cur: fun, funcname, id_gen, goto_labels_allowed: true};
  match execute_statement(&body, flo, 0, &mut g) {
    Err(finding) => finding,
    Ok(flo) => flo.exit_envs.into_iter().chain(flo.local)
                .map(|env| check_exit(env, sem))
                .flatten().collect()
  }
}

macro_rules! report {
  ($g:expr, $cur:expr, $cat:expr, $($msg:tt)+) => {
    return Err(vec![report(Some($cur), $cat, format!($($msg)+), $g)])
  };
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
    let deeper = depth + 1;
    let mut kid = flo;
    for child in cur.children() {
      kid = execute_statement(&child, kid, deeper, g)?;
    }
    kid = kid.scope_done(deeper, g)?;
    return Ok(kid);
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
    let first = match (&then) {
      None => flo.change_local(t_op_env),
      Some(t) => {
        execute_statement(t, flo.change_local(t_op_env), depth + 1, g)?
          .scope_done(depth + 1, g)?
      },
    };
    let mut second = match &els {
      None => first.change_local(f_op_env),
      Some(e) => {
        execute_statement(e, first.change_local(f_op_env), depth + 1, g)?
          .scope_done(depth + 1, g)?
      },
    };

    second.local = mayb_join(first.local, second.local)?;
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

    match eval_cond(cond, env, depth, g)? {
      (None, None) => report!(g, cur, "strange control flow",
        "strange conditional"),

      (None, Some(f_env)) => return Ok(flo.change_local(Some(f_env))),

      (Some(t_env), None) => {
        let loop_flo = flo.change_local(Some(t_env));
        return execute_inf_loop(body, loop_flo, depth + 1, g)?
          .scope_done(depth + 1, g);
      },

      (Some(t_env), Some(f_env)) => {
        let loop_flo = flo.change_local(Some(t_env));
        return execute_loop(body, loop_flo, depth + 1, g)?
          .scope_done(depth + 1, g)?
          .join(Some(f_env));
      }
    }
  }
  //  similarly, we join f_env with (body + inc)[t_env]
  //
  else if k == CXCursor_ForStmt {
    let kids = cur.children();
    let Some(body) = kids.last() else {
      report!(g, cur, "strange control flow", "if without body");
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

    let Some((init, cond, inc, body)) = for_parts(cur) else {
      report!(g, cur, "strange control flow", "strange if");
    };

    let flo = match init {
      None => flo,
      Some(i) => execute_statement(&i, flo, depth + 1, g)?
    };

    match cond {
      None => return execute_for_ever(&body, flo, inc, depth + 1, g),
      Some(c) => {
        match eval_cond(&c, env, depth, g)? {
          (None, None) => report!(g, cur, "strange control flow",
            "strange conditional"),
          
          (None, Some(f_env)) => return flo.change_local(Some(f_env))
            .scope_done(depth + 1, g),
          
          (Some(t_env), None) => {
            return execute_for_ever(&body,
              flo.change_local(Some(t_env)),
              inc,
              depth + 1,
              g)?.scope_done(depth + 1, g);
          },

          (Some(t_env), Some(f_env)) => {
            let loop_flo = flo.change_local(Some(t_env));
            return execute_for_loop(cur, loop_flo, inc, depth, g)?
              .scope_done(depth + 1, g)?.join(Some(f_env));
          }
        }
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

    let loop1_flo = execute_loop(body, flo, depth + 1, g)?
      .scope_done(depth + 1, g)?;

    let Some(env) = loop1_flo.local.clone() else {
      return Ok(loop1_flo);
    };

    match eval_cond(cond, env, depth, g)? {
      (None, None) => report!(g, cur, "strange control flow",
        "strange conditional"),
      
      (None, Some(f_env)) => return Ok(loop1_flo),
      (Some(t_env), None) => {
        let loop2_flo = loop1_flo.change_local(Some(t_env));
        return execute_inf_loop(body, loop2_flo, depth + 1, g)?
          .scope_done(depth + 1, g)},
      
      (Some(t_env), Some(f_env)) => {
        let loop2_flo = loop1_flo.change_local(Some(t_env));
        return execute_loop(body, loop2_flo, depth + 1, g)?
          .scope_done(depth + 1, g)?
          .join(Some(f_env));
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
    let (_, Some(env)) = eval_expr(val, env, depth, g)? else {
      report!(g, cur, "strange control flow",
        "switch crashes immediately in the expression");
    };
    return execute_switch_body(body, flo.change_local(Some(env)), depth + 1, g);
  }
  // move here ^
  // else if k == CXCursor_CaseStmt {
  //   todo!()
  // }
  // else if k == CXCursor_DefaultStmt {
  //   todo!()
  // }
  else if k == CXCursor_LabelStmt {
    if !g.goto_labels_allowed {
      report!(g, cur, "illegal goto label position",
        "linter cannot analyze this goto label position. review the function\
         and add assertion annotations");
    }
    let label = cur.spelling();
    let Flow { local, mut goto_envs, exit_envs } = flo;
    //  all environments before label: goto sites + natural control flow if
    //  reachable
    //
    let mut envs = goto_envs.remove(&label).unwrap_or_default()
      .into_iter().chain(local);

    let total = match envs.next() {
      None => None,
      Some(first) => Some(envs.try_fold(first, join)?),
    };
    let flo = Flow {local: total, goto_envs, exit_envs};
    match cur.children().last() {
      Some(sub) => execute_statement(sub, flo, depth, g),
      None => Ok(flo),
    }
  }
  else {
    //  Local ops: Env -> Env mapping
    let Flow { local, mut goto_envs, mut exit_envs } = flo;
    let Some(env) = local else {
      return Ok(Flow { local: None, goto_envs, exit_envs });
    };
  
    if k == CXCursor_DeclStmt {
      let local = execute_decl(cur, env, depth, g)?;
      return Ok(Flow { local, goto_envs, exit_envs });
    }
  
    if k == CXCursor_ReturnStmt {
      if let Some(env) = execute_return(cur, env, depth, g)? {
        exit_envs.push_back(env);
      }
      return Ok(Flow { local: None, goto_envs, exit_envs });
    }
  
    if k == CXCursor_GotoStmt {
      //  park the env under the target label; joined in at the LabelStmt.
      //  (backward gotos need rejecting here once labels are tracked)
      let target = cur.children().first()
        .filter(|c| c.kind() == CXCursor_LabelRef)
        .map(|c| c.spelling());
      let Some(target) = target else {
        report!(g, cur, "strange goto", "");
      };
      goto_envs.entry(target).or_default().push_back(env);
      return Ok(Flow { local: None, goto_envs, exit_envs });
    }
    if k == CXCursor_IndirectGotoStmt {
      report!(g, cur, "computed goto", "");
    }
  
    if k == CXCursor_BreakStmt || k == CXCursor_ContinueStmt {
      report!(g, cur, "strange control flow",
        "break/continue outside a loop-body walker");
    }
  
    if k == CXCursor_NullStmt || k == CXCursor_AsmStmt {
      return Ok(Flow { local: Some(env), goto_envs, exit_envs });
    }
  
    //  expression in statement position: `u3z(a);`, `x = f(y);`, bare `x;`
    if is_expr_kind(k) {
      let local = execute_expr_stmt(cur, env, depth, g)?;
      return Ok(Flow { local, goto_envs, exit_envs });
    }
  
    report!(g, cur, "unhandled statement kind", "[{}] is not handled yet", k);
  }
}

fn report(cur: Option<&Cursor>, cat: &'static str, msg: String, g: &Gen)
  -> Finding
{
  let loc = cur.unwrap_or(g.func_cur).location();
  Finding {
    file: loc.file.unwrap_or_else(|| "None".to_string()),
    line: loc.line,
    col: loc.col,
    func: g.funcname.to_string(),
    cat,
    msg,
  }
}

//  --------------------------------------------------------------------------
//  local statement handlers: one env in, one env out (None = path ended)

/// Variable declarations (possibly several per statement), including
/// initializers.
fn execute_decl(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  todo!()
}

fn execute_inf_loop(cur: &Cursor, flo: Flow, depth: u32, g: &mut Gen)
  -> R<Flow>
{
  todo!()
}

fn execute_loop(cur: &Cursor, flo: Flow, depth: u32, g: &mut Gen)
  -> R<Flow>
{
  todo!()
}

fn execute_for_ever(cur: &Cursor,
  flo: Flow,
  inc: Option<Cursor>,
  depth: u32,
  g: &mut Gen) -> R<Flow>
{
  todo!()
}

fn execute_for_loop(cur: &Cursor,
  flo: Flow,
  inc: Option<Cursor>,
  depth: u32,
  g: &mut Gen) -> R<Flow>
{
  todo!()
}

/// `return [expr];` -- evaluate the expression, check it against the
/// function's product protocol, and hand back the env to park in
/// exit_envs. None when the expression itself ends the path
/// (`return u3m_bail(..)` -- common in this codebase).
fn execute_return(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  todo!()
}

/// An expression evaluated for its effects only
fn execute_expr_stmt(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  todo!()
}

fn execute_switch_body(cur: &Cursor, flo: Flow, depth: u32, g: &mut Gen)
  -> R<Flow>
{
  todo!()
}

/// Evaluate an expression: the id of the value it produces (None for
/// non-noun values) plus the post-evaluation env (None when the path ends
/// inside, e.g. a u3m_bail() operand).
///
/// GNU statement-expressions (`({ stmt; ...; expr })`) are handled HERE,
/// not in execute_statement: CXCursor_StmtExpr is an *expression* kind --
/// it shows up inside initializers, conditions, and call arguments, so
/// only the expression walker ever encounters it. Under this file's
/// assumption (no labels/gotos inside stmt-exprs) its inner CompoundStmt
/// is a closed region: run the prefix children through execute_statement
/// with a fresh Flow, then evaluate the last child as the value when
/// is_expr_kind(last.kind()). Assert that the resulting goto_envs and
/// exit_envs come back empty -- that's where the assumption is enforced
/// instead of silently miscounting.
fn eval_expr(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<(Option<ValId>, Option<Env>)>
{
  todo!()
}

fn eval_cond(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> Result<(Option<Env>, Option<Env>), Vec<Finding>>
{
  todo!()
}

fn join(env1: Env, env2: Env) -> R<Env>
{
  todo!();
}

fn mayb_join(env1: Option<Env>, env2: Option<Env>) -> R<Option<Env>>
{
  match (env1, env2) {
    (Some(env1), Some(env2)) => join(env1, env2).map(Some),
    (env1, env2) => Ok(env1.or(env2)),
  }
}

fn check_exit(env: Env, sem: &Sem) -> Vec<Finding>
{
  todo!()
}

fn end_scope(env: Env, depth: u32, g: &mut Gen) -> R<Env>
{
  todo!();
}

fn test(res1: R<Env>, res2: R<Env>)
  -> Vec<Finding>
{
  match (res1, res2) {
    (Ok(env1), Ok(env2)) => todo!(),
    (res1, res2) => res1.err().into_iter().chain(res2.err()).flatten().collect()
  }
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