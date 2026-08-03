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

#![allow(unused)]
#![allow(non_upper_case_globals)]

use std::cell::Ref;
use std::collections::{BTreeSet, HashMap, HashSet};
use std::rc::Rc;
use std::todo;
use imbl::{HashMap as IHashMap, HashSet as IHashSet, Vector as IVec};

use clang_sys::*;
use indexmap::IndexMap;

use crate::ast::{
  binop, decl_ref_name, int_literal_value, is_expr_kind, is_local_lvalue,
  is_noun_type, unary_op, unwrap_expr, Cursor, Name, Ty,
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
#[derive(Clone, Copy)]
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
  contains: IHashMap<ValId, IHashSet<ValId>>  // noun -> (set sub-noun)
}

#[derive(Default, Clone)]
struct Flow {
  local: Option<Env>,                   // local environment, if code is reachable
  goto_envs: IHashMap<Name, IVec<Env>>, // environments from goto, for goto labels
  exit_envs: IVec<Env>,                 // environments from return, for exit
  break_envs: IVec<Env>,
  cont_envs: IVec<Env>,
}

type R<T> = Result<T, Vec<Finding>>;

macro_rules! report {
  ($g:expr, $cur:expr, $cat:expr, $($msg:tt)+) => {
    return Err(vec![report(Some($cur), $cat, format!($($msg)+), $g)])
  };
}

macro_rules! report_global {
  ($g:expr, $cat:expr, $($msg:tt)+) => {
    return Err(vec![report(None, $cat, format!($($msg)+), $g)])
  };
}

impl Flow {
  fn scope_done(mut self, depth: u32, g: &mut Gen) -> R<Flow>
  {
    self.local = self.local.map(|e| end_scope(e, depth, g)).transpose()?;
    Ok(self)
  }

  fn change_local(&self, local: Option<Env>) -> Flow
  {
    return Flow {
      local,
      goto_envs: self.goto_envs.clone(),
      exit_envs: self.exit_envs.clone(),
      break_envs: self.break_envs.clone(),
      cont_envs: self.cont_envs.clone(),
    };
  }

  fn join(self, another: Option<Env>) -> R<Flow>
  {
    return Ok(Flow {
      local: mayb_join(self.local, another)?,
      goto_envs: self.goto_envs,
      exit_envs: self.exit_envs,
      break_envs: self.break_envs,
      cont_envs: self.cont_envs,
    })
  }
}

impl Env {
  fn insert_new(&mut self, name: VarName, rc: RefcountState, g: &mut Gen)
  {
    let id: ValId = g.id_gen; g.id_gen += 1;
    
    assert!(!self.locations.contains_key(&id));
    assert!(!self.locations_rev.contains_key(&name));

    self.values.insert(id, rc);
    self.locations.insert(id, IHashSet::from([name.clone()]));
    self.locations_rev.insert(name, id);
  }

  /// Add `name` as one more location of the existing value `id`.
  /// Declaration sites only: the name must be fresh.
  fn bind_decl(&mut self, name: VarName, id: ValId)
  {
    assert!(!self.locations_rev.contains_key(&name));

    self.locations.entry(id).or_default().insert(name.clone());
    self.locations_rev.insert(name, id);
  }

  /// Variable names currently holding value `id`, for messages.
  fn names(&self, id: ValId) -> String
  {
    let mut ns: Vec<&str> = self.locations.get(&id)
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
      RefcountState::Owned {rc: 0} =>
        unreachable!("linter invariant: Owned rc > 0"),

      RefcountState::Owned {rc: 1} => *state = RefcountState::Poisoned,

      RefcountState::Owned {rc} => *state = RefcountState::Owned {rc: rc - 1},

      RefcountState::Direct => {}

      RefcountState::Borrowed => report!(g, cur, "refcount error",
        "transfer of borrowed reference [{}]: the caller retains \
         ownership, u3k first", self.names(id)),

      RefcountState::Uninit => report!(g, cur, "refcount error",
        "transfer of uninitialized variable [{}]", self.names(id)),

      RefcountState::Poisoned => report!(g, cur, "refcount error",
        "transfer of already-consumed value [{}]", self.names(id)),
    };
    Ok(self)
  }
}

struct Gen<'a> {
  func_cur: &'a Cursor,
  funcname: Name,
  id_gen: u32,
  goto_labels_allowed: bool,
  host: &'a mut dyn Host,
  sem: &'a Sem
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
    sem
  };

  for p in fun.arguments() {
    let pname = p.spelling();
    if pname.is_empty() {
      report_v!(&g, fun, "strange argument", "nameless argument");
    }
    if p.kind() != CXCursor_ParmDecl || !is_noun_type(&p.ty()) {
      continue;
    }
    let mode: Mode = sem.arg_mode(&pname);
    let rc = match mode {
      Mode::Transfer => RefcountState::Owned { rc: 1 },
      Mode::Retain => RefcountState::Borrowed,
    };
    env.insert_new(VarName {name: pname, depth: 0}, rc, &mut g);
  }

  let flo: Flow = Flow {local: Some(env), ..Default::default()};
  match execute_statement(&body, flo, 0, &mut g) {
    Err(finding) => finding,
    Ok(flo) => flo.exit_envs.into_iter().chain(flo.local)
                .map(|env| check_exit(env, sem))
                .flatten().collect()
  }
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
    return cur.children().into_iter()
      .try_fold(flo, |flo, kid| execute_statement(&kid, flo, depth + 1, g))?
      .scope_done(depth + 1, g);
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
                    .scope_done(depth + 1, g),
      }
    };

    let first      = branch(then, &flo,   t_op_env, g)?;
    let mut second = branch(els,  &first, f_op_env, g)?;

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

    match eval_cond(cond, env.clone(), depth, g)? {
      (None, None) => report!(g, cur, "strange control flow",
        "strange conditional"),

      (None, Some(f_env)) => return Ok(flo.change_local(Some(f_env))),

      (Some(t_env), f_op_env) => {
        let loop_flo = flo.change_local(Some(t_env));
        let (done_flo, cont) = execute_loop_body(body, loop_flo, None,
          depth + 1, g)?;

        //  we check if "continue" environment is joinable with the env before
        //  the conditional
        //
        mayb_join(Some(env), cont)?;
        return done_flo.scope_done(depth + 1, g)?
                       .join(f_op_env);
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
                                       .scope_done(depth + 1, g),

      (Some(t_env), f_op_env) => {
        let loop_flo = flo.change_local(Some(t_env));
        let (done_flo, cont) = execute_loop_body(&body,
          loop_flo, inc, depth, g)?;
        mayb_join(cont, Some(env))?;
        return done_flo.scope_done(depth + 1, g)?.join(f_op_env);
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

    let (mut loop1_flo, cont) = execute_loop_body(body, flo.clone(), None,
      depth + 1, g)?;

    mayb_join(cont, flo.local)?;
    loop1_flo = loop1_flo.scope_done(depth + 1, g)?;

    let Some(env) = loop1_flo.local.clone() else {
      return Ok(loop1_flo);
    };

    match eval_cond(cond, env.clone(), depth, g)? {
      (None, None) => report!(g, cur, "strange control flow",
        "strange conditional"),
      
      (None, Some(_)) => return Ok(loop1_flo),
      
      (Some(t_env), f_op_env) => {
        let loop2_flo = loop1_flo.change_local(Some(t_env));
        let (done_flo, cont) = execute_loop_body(body, loop2_flo, None,
          depth + 1, g)?;

        mayb_join(cont, Some(env))?;
        return done_flo.scope_done(depth + 1, g)?
                       .join(f_op_env);
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
  else if k == CXCursor_CaseStmt {
    todo!()
  }
  else if k == CXCursor_DefaultStmt {
    todo!()
  }
  else if k == CXCursor_BreakStmt {
    todo!()
  }
  else if k == CXCursor_ContinueStmt {
    todo!()
  }
  else if k == CXCursor_LabelStmt {
    if !g.goto_labels_allowed {
      report!(g, cur, "illegal goto label position",
        "linter cannot analyze this goto label position. review the function\
         and add assertion annotations");
    }
    let label = cur.spelling();
    let Flow { local, mut goto_envs, exit_envs, break_envs, cont_envs} = flo;
    //  all environments before label: goto sites + natural control flow if
    //  reachable
    //
    let total = join_all(goto_envs.remove(&label).unwrap_or_default()
      .into_iter().chain(local))?;

    let flo = Flow {local: total, goto_envs, exit_envs, break_envs, cont_envs};
    match cur.children().last() {
      Some(sub) => execute_statement(sub, flo, depth, g),
      None => Ok(flo),
    }
  }
  else {
    //  Local ops: Env -> Env mapping
    let Flow { local, mut goto_envs, mut exit_envs,
      break_envs, cont_envs } = flo;

    let Some(env) = local else {
      return Ok(Flow { local: None, goto_envs, exit_envs, break_envs,
        cont_envs });
    };
  
    if k == CXCursor_DeclStmt {
      let local = execute_decl(cur, env, depth, g)?;
      return Ok(Flow { local, goto_envs, exit_envs, break_envs, cont_envs });
    }
  
    if k == CXCursor_ReturnStmt {
      if let Some(env) = execute_return(cur, env, depth, g)? {
        exit_envs.push_back(env);
      }
      return Ok(Flow { local: None, goto_envs, exit_envs, break_envs,
        cont_envs });
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
      return Ok(Flow { local: None, goto_envs, exit_envs, break_envs,
        cont_envs });
    }
    if k == CXCursor_IndirectGotoStmt {
      report!(g, cur, "computed goto", "");
    }
  
    if k == CXCursor_BreakStmt || k == CXCursor_ContinueStmt {
      report!(g, cur, "strange control flow",
        "break/continue outside a loop-body walker");
    }
  
    if k == CXCursor_NullStmt || k == CXCursor_AsmStmt {
      return Ok(Flow { local: Some(env), goto_envs, exit_envs, break_envs,
        cont_envs });
    }
  
    //  expression in statement position: `u3z(a);`, `x = f(y);`, bare `x;`
    if is_expr_kind(k) {
      let local = execute_expr_stmt(cur, env, depth, g)?;
      return Ok(Flow { local, goto_envs, exit_envs, break_envs, cont_envs });
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
      match vid {
        Some(vid) => nxt.bind_decl(var, vid),
        None if is_noun_type(&ty) => {
          //  Noun variable initialized with a non-noun value: gotta be
          //  direct. eval_expr should reflect that
          nxt.insert_new(var, RefcountState::Direct, g);
        }
        None => {}
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
    match vid {
      Some(v) => nxt.bind_decl(VarName {name: path(), depth}, v),
      None if fnoun => {
        nxt.insert_new(VarName {name: path(), depth},
          RefcountState::Direct, g);
      }
      None => {}
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
    if env.locations_rev.keys().any(|v| v.name == n) {
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

// Returns fall out flow (natural flow + break) + continue env, with the scope
// cleared
//
fn execute_loop_body(cur: &Cursor,
  flo: Flow,
  inc: Option<Cursor>,
  depth: u32,
  g: &mut Gen) -> R<(Flow, Option<Env>)>
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
    (Some(i), Some(e)) => eval_expr(&i, e, depth, g)?.1,
    _ => flo_done.local,
  };
    
  let cont = join_all(flo_done.cont_envs)?
    .map(|e| end_scope(e, depth, g)).transpose()?;

  let local = join_all(flo_done.break_envs.into_iter().chain(flo_done.local))?;
  let out = Flow {
    local,
    goto_envs: flo.goto_envs.union_with(flo_done.goto_envs, |a, b| a + b),
    exit_envs: flo.exit_envs + flo_done.exit_envs,
    break_envs: flo.break_envs,
    cont_envs: flo.cont_envs,
  };
  Ok((out, cont))
}

/// `return [expr];` -- evaluate the expression, check it against the
/// function's product protocol, and hand back the env to park in
/// exit_envs. None when the expression itself ends the path
/// (`return u3m_bail(..)` -- common in this codebase).
fn execute_return(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  let (opt_vid, opt_env) = eval_expr(cur, env, depth, g)?;
  let Some(vid) = opt_vid else { return Ok(opt_env) };
  let Some(mut env) = opt_env else { return Ok(None) };
  match g.sem.product {
    Mode::Retain => {},
    Mode::Transfer => env = env.lose(vid, cur, g)?,
  }
  Ok(Some(env))
}

/// An expression evaluated for its effects only
fn execute_expr_stmt(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<Option<Env>>
{
  let (vid, env) = eval_expr(cur, env, depth, g)?;
  let Some(mut env) = env else { return Ok(None); };
  let Some(vid) = vid else { return Ok(Some(env)); };

  if env.locations.get(&vid).is_none_or(|l| l.is_empty()) {
    if holds_owned(&env, Some(vid)) {
      report!(g, cur, "leak",
        "owned product of the expression is discarded without being \
         consumed");
    }
  }
  // not freeing the dropped vid. consequences?
  Ok(Some(env))
}

fn execute_switch_body(cur: &Cursor, flo: Flow, depth: u32, g: &mut Gen)
  -> R<Flow>
{
  todo!()
}

/// gotos, breaks and continues are assumed to not be present in GNU statement-
/// expressions. The linter will try to enforce ot too
/// 
fn eval_expr(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<(Option<ValId>, Option<Env>)>
{
  todo!()
}

fn eval_cond(cur: &Cursor, env: Env, depth: u32, g: &mut Gen)
  -> R<(Option<Env>, Option<Env>)>
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

/// Join any number of envs into one, or None if no envs
fn join_all<I>(envs: I) -> R<Option<Env>>
  where I: IntoIterator<Item = Env>
{
  let mut it = envs.into_iter();
  it.next().map(|first| it.try_fold(first, join)).transpose()
}

fn check_exit(env: Env, sem: &Sem) -> Vec<Finding>
{
  todo!()
}

fn end_scope(env: Env, depth: u32, g: &mut Gen) -> R<Env>
{
  todo!();
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