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
use imbl::{HashMap as IHashMap, HashSet as IHashSet};

use clang_sys::*;
use indexmap::IndexMap;

use crate::ast::{
    binop, decl_ref_name, int_literal_value, is_expr_kind, is_local_lvalue,
    is_noun_type, unary_op, unwrap_expr, Cursor,
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
    Uninit,             // not initialized yer
    Borrowed,           // correctly borrowed
    Owned {rc: u32},    // correctly owned, rc > 0
    Conflict,           // inconsistent values across branches
    Poisoned,           // consumed, not valid to use
    Direct,             // direct atom, no refcounting
}

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
    locations: IHashMap<ValId, IHashSet<String>>,
    locations_rev: IHashMap<String, ValId>,
    goto_envs: IHashMap<String, Env>,
}

impl Env {
    fn insert_new(&mut self, name: String, rc: RefcountState, id_gen: &mut ValId) {
        let id: ValId = *id_gen;
        *id_gen += 1;
        self.values.update(id, rc);
        let locs: IHashSet<String> = IHashSet::from([name.clone()]);
        self.locations.update(id, locs);
        self.locations_rev.update(name, id);
    }
}

/// Check one function definition; returns the findings.
pub fn check_function(host: &mut dyn Host, fun: &Cursor, sem: &Sem)
    -> Vec<Finding> {
    let mut body: Option<Cursor> = None;
    for c in fun.children() {
        if c.kind() == CXCursor_CompoundStmt {
            body = Some(c);
        }
    }

    let Some(body) = body else { return Vec::new(); };
    let mut env = Env::default();
    let mut id_gen: ValId = 0;
    for p in fun.arguments() {
        if p.kind() != CXCursor_ParmDecl || p.spelling().is_empty() {
            continue;
        }
        if is_noun_type(&p.ty()) {
            let pname = p.spelling();
            let mode = sem.arg_mode(&pname);
            let rc = match mode {
                Mode::Transfer => RefcountState::Owned { rc: 1 },
                Mode::Retain => RefcountState::Borrowed,
            };
            env.insert_new(pname, rc, &mut id_gen);
        }
    }

    match execute_statement(&body, env) {
        Err(finding) => vec![finding],
        Ok(env) => check_exit(env, sem).into_iter().collect(),
    }
}

fn execute_statement(cur: &Cursor, env: Env) -> Result<Env, Finding> {
    let k = cur.kind();

    

    todo!();
}

fn check_exit(env: Env, sem: &Sem) -> Option<Finding> {
    todo!();
}