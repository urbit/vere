//! Thin safe(ish) wrapper over libclang (via clang-sys, runtime-loaded),
//! plus small AST utilities shared by the driver, the annotation layer,
//! and the abstract interpreter.
//!
//! Lifetimes are intentionally not modeled: `Cursor`, `Ty`, and `Tok` are
//! only valid while the `Tu` they came from is alive. This is an internal
//! analysis tool; keep translation units alive for the duration of a pass.

#![allow(non_upper_case_globals)]

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint, c_ulong, c_void};
use std::ptr;
use std::rc::Rc;

/// An AST-derived name (cursor spelling, variable path). `Rc<str>` so the
/// interpreter can hold it in many environments; cloning is a refcount
/// bump, never a byte copy.
pub type Name = Rc<str>;

pub fn string_to_name(s: String) -> Name
{
  Rc::from(s)
}

use clang_sys::*;

use crate::config;

/// Load libclang. If `libclang` is Some, that exact .so is used.
pub fn init(libclang: Option<&str>) -> Result<(), String> {
  if let Some(p) = libclang {
    // SAFETY: called once at startup before any threads exist
    unsafe { std::env::set_var("LIBCLANG_PATH", p) };
  }
  clang_sys::load()
}

unsafe fn cx(s: CXString) -> String {
  unsafe {
    if s.data.is_null() {
      return String::new();
    }
    let c = clang_getCString(s);
    let out = if c.is_null() {
      String::new()
    } else {
      CStr::from_ptr(c).to_string_lossy().into_owned()
    };
    clang_disposeString(s);
    out
  }
}

// ---------------------------------------------------------------------------
// locations

#[derive(Clone, Debug, Default)]
pub struct Loc {
  pub file: Option<Name>,
  pub line: u32,
  pub col: u32,
  pub offset: u32,
}

fn to_loc(l: CXSourceLocation) -> Loc {
  let mut f: CXFile = ptr::null_mut();
  let (mut line, mut col, mut off): (c_uint, c_uint, c_uint) = (0, 0, 0);
  unsafe { clang_getExpansionLocation(l, &mut f, &mut line, &mut col, &mut off) };
  let file = if f.is_null() {
    None
  } else {
    Some(unsafe { cx(clang_getFileName(f)) })
  };
  Loc { file: file.map(string_to_name), line, col, offset: off }
}

// ---------------------------------------------------------------------------
// index / translation unit

pub struct Index {
  raw: CXIndex,
}

impl Index {
  pub fn new() -> Index {
    Index { raw: unsafe { clang_createIndex(0, 0) } }
  }

  pub fn parse(&self, file: &str, args: &[String]) -> Result<Tu, String> {
    self.parse_opts(file, args, 0)
  }

  pub fn parse_opts(&self, file: &str, args: &[String],
    opts: CXTranslationUnit_Flags) -> Result<Tu, String>
  {
    let cfile = CString::new(file).map_err(|e| e.to_string())?;
    let cargs: Vec<CString> = args
      .iter()
      .map(|a| CString::new(a.as_str()).unwrap_or_default())
      .collect();
    let ptrs: Vec<*const c_char> = cargs.iter().map(|c| c.as_ptr()).collect();
    let mut tu: CXTranslationUnit = ptr::null_mut();
    let code = unsafe {
      clang_parseTranslationUnit2(
        self.raw,
        cfile.as_ptr(),
        ptrs.as_ptr(),
        ptrs.len() as c_int,
        ptr::null_mut(),
        0,
        opts,
        &mut tu,
      )
    };
    if code != CXError_Success || tu.is_null() {
      return Err(format!("libclang error code {}", code));
    }
    Ok(Tu { raw: tu })
  }
}

impl Drop for Index {
  fn drop(&mut self) {
    unsafe { clang_disposeIndex(self.raw) };
  }
}

/// Cursors of the declarations parsed in this TU itself, in source
/// order, via the indexing API. Unlike walking the root cursor's
/// children, this never touches declarations that entered the TU from a
/// precompiled header -- visiting those forces deserialization of the
/// whole PCH, which costs about as much as parsing the headers. Falls
/// back to the full root walk if the indexer reports an error.
pub fn local_decls(idx: &Index, tu: &Tu) -> Vec<Cursor> {
  extern "C" fn decl_cb(client: CXClientData, info: *const CXIdxDeclInfo) {
    unsafe {
      let out = &mut *(client as *mut Vec<Cursor>);
      out.push(Cursor { raw: (*info).cursor });
    }
  }
  let mut out: Vec<Cursor> = Vec::new();
  let mut cbs = IndexerCallbacks::default();
  cbs.indexDeclaration = Some(decl_cb);
  let failed = unsafe {
    let action = clang_IndexAction_create(idx.raw);
    let rc = clang_indexTranslationUnit(
      action,
      &mut out as *mut Vec<Cursor> as CXClientData,
      &mut cbs,
      std::mem::size_of::<IndexerCallbacks>() as c_uint,
      CXIndexOptSuppressWarnings,
      tu.raw,
    );
    clang_IndexAction_dispose(action);
    rc != 0
  };
  if failed {
    return tu.cursor().children();
  }
  out
}

pub struct Tu {
  raw: CXTranslationUnit,
}

impl Tu {
  /// Serialize to an AST file usable via `-include-pch`. NB a save can
  /// succeed for a TU with error diagnostics; check those first.
  pub fn save(&self, path: &str) -> Result<(), String> {
    let cpath = CString::new(path).map_err(|e| e.to_string())?;
    let code = unsafe {
      clang_saveTranslationUnit(self.raw, cpath.as_ptr(),
        CXSaveTranslationUnit_None)
    };
    if code != CXSaveError_None {
      return Err(format!("CXSaveError {}", code));
    }
    Ok(())
  }

  pub fn cursor(&self) -> Cursor {
    Cursor { raw: unsafe { clang_getTranslationUnitCursor(self.raw) } }
  }

  /// Formatted diagnostics with severity >= Error.
  pub fn error_diagnostics(&self) -> Vec<String> {
    let mut out = Vec::new();
    unsafe {
      let n = clang_getNumDiagnostics(self.raw);
      for i in 0..n {
        let d = clang_getDiagnostic(self.raw, i);
        if clang_getDiagnosticSeverity(d) >= CXDiagnostic_Error {
          out.push(cx(clang_formatDiagnostic(
            d,
            clang_defaultDiagnosticDisplayOptions(),
          )));
        }
        clang_disposeDiagnostic(d);
      }
    }
    out
  }

  /// Tokenize the byte range [lo, hi) of `path` in this TU.
  pub fn tokenize_file_range(&self, path: &str, lo: u32, hi: u32) -> Vec<Tok> {
    let cpath = match CString::new(path) {
      Ok(c) => c,
      Err(_) => return Vec::new(),
    };
    unsafe {
      let f = clang_getFile(self.raw, cpath.as_ptr());
      if f.is_null() {
        return Vec::new();
      }
      let a = clang_getLocationForOffset(self.raw, f, lo);
      let b = clang_getLocationForOffset(self.raw, f, hi);
      tokenize(self.raw, clang_getRange(a, b))
    }
  }
}

impl Drop for Tu {
  fn drop(&mut self) {
    unsafe { clang_disposeTranslationUnit(self.raw) };
  }
}

// ---------------------------------------------------------------------------
// tokens

#[derive(Clone, Debug)]
pub struct Tok {
  pub kind: CXTokenKind,
  pub spelling: String,
  pub line: u32,
  pub offset: u32, // extent start offset
}

unsafe fn tokenize(tu: CXTranslationUnit, range: CXSourceRange) -> Vec<Tok> {
  unsafe {
    let mut toks: *mut CXToken = ptr::null_mut();
    let mut n: c_uint = 0;
    clang_tokenize(tu, range, &mut toks, &mut n);
    let mut out = Vec::with_capacity(n as usize);
    for i in 0..n as isize {
      let t = *toks.offset(i);
      let loc = to_loc(clang_getTokenLocation(tu, t));
      let ext = to_loc(clang_getRangeStart(clang_getTokenExtent(tu, t)));
      out.push(Tok {
        kind: clang_getTokenKind(t),
        spelling: cx(clang_getTokenSpelling(tu, t)),
        line: loc.line,
        offset: ext.offset,
      });
    }
    if !toks.is_null() {
      clang_disposeTokens(tu, toks, n);
    }
    out
  }
}

// ---------------------------------------------------------------------------
// cursors

#[derive(Copy, Clone)]
pub struct Cursor {
  pub raw: CXCursor,
}

extern "C" fn child_visitor(
  cursor: CXCursor,
  _parent: CXCursor,
  data: CXClientData,
) -> CXChildVisitResult {
  let v = unsafe { &mut *(data as *mut Vec<Cursor>) };
  v.push(Cursor { raw: cursor });
  CXChildVisit_Continue
}

extern "C" fn field_visitor(cursor: CXCursor, data: CXClientData) -> CXVisitorResult {
  let v = unsafe { &mut *(data as *mut Vec<Cursor>) };
  v.push(Cursor { raw: cursor });
  CXVisit_Continue
}

impl Cursor {
  pub fn kind(&self) -> CXCursorKind {
    unsafe { clang_getCursorKind(self.raw) }
  }

  pub fn is_null(&self) -> bool {
    unsafe { clang_Cursor_isNull(self.raw) != 0 }
  }

  pub fn spelling(&self) -> Name {
    Rc::from(unsafe { cx(clang_getCursorSpelling(self.raw)) })
  }

  pub fn children(&self) -> Vec<Cursor> {
    let mut out: Vec<Cursor> = Vec::new();
    unsafe {
      clang_visitChildren(
        self.raw,
        child_visitor,
        &mut out as *mut Vec<Cursor> as CXClientData,
      );
    }
    out
  }

  pub fn referenced(&self) -> Option<Cursor> {
    let c = Cursor { raw: unsafe { clang_getCursorReferenced(self.raw) } };
    if c.is_null() { None } else { Some(c) }
  }

  pub fn definition(&self) -> Option<Cursor> {
    let c = Cursor { raw: unsafe { clang_getCursorDefinition(self.raw) } };
    if c.is_null() { None } else { Some(c) }
  }

  pub fn canonical(&self) -> Cursor {
    Cursor { raw: unsafe { clang_getCanonicalCursor(self.raw) } }
  }

  pub fn is_definition(&self) -> bool {
    unsafe { clang_isCursorDefinition(self.raw) != 0 }
  }

  pub fn location(&self) -> Loc {
    to_loc(unsafe { clang_getCursorLocation(self.raw) })
  }

  /// True if this cursor's token text physically lives somewhere other
  /// than its expansion site -- i.e. it comes from a macro body.
  pub fn is_macro_origin(&self) -> bool {
    let l = unsafe { clang_getCursorLocation(self.raw) };
    let mut f: CXFile = ptr::null_mut();
    let (mut line, mut col, mut off): (c_uint, c_uint, c_uint) = (0, 0, 0);
    unsafe { clang_getSpellingLocation(l, &mut f, &mut line, &mut col, &mut off) };
    let spell = if f.is_null() {
      None
    } else {
      Some(unsafe { cx(clang_getFileName(f)) })
    };
    let exp = self.location();
    spell.as_deref() != exp.file.as_deref().map(|s| s as &str)
      || off != exp.offset
  }

  pub fn extent_start(&self) -> Loc {
    to_loc(unsafe { clang_getRangeStart(clang_getCursorExtent(self.raw)) })
  }

  pub fn extent_end(&self) -> Loc {
    to_loc(unsafe { clang_getRangeEnd(clang_getCursorExtent(self.raw)) })
  }

  pub fn ty(&self) -> Ty {
    Ty { raw: unsafe { clang_getCursorType(self.raw) } }
  }

  pub fn result_type(&self) -> Ty {
    Ty { raw: unsafe { clang_getCursorResultType(self.raw) } }
  }

  pub fn is_static(&self) -> bool {
    unsafe { clang_Cursor_getStorageClass(self.raw) == CX_SC_Static }
  }

  pub fn has_no_linkage(&self) -> bool {
    unsafe { clang_getCursorLinkage(self.raw) == CXLinkage_NoLinkage }
  }

  /// Arguments of a function decl or call expression (empty if N/A).
  pub fn arguments(&self) -> Vec<Cursor> {
    let n = unsafe { clang_Cursor_getNumArguments(self.raw) };
    if n <= 0 {
      return Vec::new();
    }
    (0..n as c_uint)
      .map(|i| Cursor { raw: unsafe { clang_Cursor_getArgument(self.raw, i) } })
      .collect()
  }

  pub fn raw_comment(&self) -> Option<String> {
    let s = unsafe { cx(clang_Cursor_getRawCommentText(self.raw)) };
    if s.is_empty() { None } else { Some(s) }
  }

  fn tu(&self) -> CXTranslationUnit {
    unsafe { clang_Cursor_getTranslationUnit(self.raw) }
  }

  /// Tokens of this cursor's extent. Cursors inside macro expansions
  /// have no tokens.
  pub fn tokens(&self) -> Vec<Tok> {
    unsafe { tokenize(self.tu(), clang_getCursorExtent(self.raw)) }
  }

  /// Tokens on the source line right after this cursor's extent
  /// (up to 200 bytes past the end, clamped to `file_size`).
  pub fn tokens_after(&self, file_size: u32) -> Vec<Tok> {
    let end = self.extent_end();
    if end.file.is_none() {
      return Vec::new();
    }
    unsafe {
      let mut cf: CXFile = ptr::null_mut();
      let (mut l, mut c, mut o): (c_uint, c_uint, c_uint) = (0, 0, 0);
      let raw_end = clang_getRangeEnd(clang_getCursorExtent(self.raw));
      clang_getExpansionLocation(raw_end, &mut cf, &mut l, &mut c, &mut o);
      if cf.is_null() {
        return Vec::new();
      }
      let hi = (end.offset + 200).min(file_size);
      if hi <= end.offset {
        return Vec::new();
      }
      let a = clang_getLocationForOffset(self.tu(), cf, end.offset);
      let b = clang_getLocationForOffset(self.tu(), cf, hi);
      tokenize(self.tu(), clang_getRange(a, b))
    }
  }

  /// Constant-evaluate this cursor; Some(value) for integer results.
  pub fn evaluate_int(&self) -> Option<i64> {
    unsafe {
      let res = clang_Cursor_Evaluate(self.raw);
      if res.is_null() {
        return None;
      }
      let out = if clang_EvalResult_getKind(res) == CXEval_Int {
        Some(clang_EvalResult_getAsLongLong(res) as i64)
      } else {
        None
      };
      clang_EvalResult_dispose(res);
      out
    }
  }

  pub fn binop_kind(&self) -> CXBinaryOperatorKind {
    unsafe { clang_getCursorBinaryOperatorKind(self.raw) }
  }

  pub fn unop_kind(&self) -> CXUnaryOperatorKind {
    unsafe { clang_getCursorUnaryOperatorKind(self.raw) }
  }
}

// ---------------------------------------------------------------------------
// types

#[derive(Copy, Clone)]
pub struct Ty {
  pub raw: CXType,
}

impl Ty {
  pub fn kind(&self) -> CXTypeKind {
    self.raw.kind
  }

  pub fn spelling(&self) -> String {
    unsafe { cx(clang_getTypeSpelling(self.raw)) }
  }

  pub fn canonical(&self) -> Ty {
    Ty { raw: unsafe { clang_getCanonicalType(self.raw) } }
  }

  pub fn fields(&self) -> Vec<Cursor> {
    let mut out: Vec<Cursor> = Vec::new();
    unsafe {
      clang_Type_visitFields(
        self.raw,
        field_visitor,
        &mut out as *mut Vec<Cursor> as CXClientData,
      );
    }
    out
  }

  /// True for union types (structs and unions both canonicalize to Record).
  pub fn is_union(&self) -> bool {
    unsafe {
      clang_getCursorKind(clang_getTypeDeclaration(self.raw)) == CXCursor_UnionDecl
    }
  }

  /// Element type of an array type.
  pub fn elem_type(&self) -> Ty {
    Ty { raw: unsafe { clang_getArrayElementType(self.raw) } }
  }

  /// Pointee type of a pointer type (Invalid kind for non-pointers).
  pub fn pointee_type(&self) -> Ty {
    Ty { raw: unsafe { clang_getPointeeType(self.raw) } }
  }
}

// suppress unused warnings for imports used only through macros
const _: Option<(c_ulong, *const c_void)> = None;

// ---------------------------------------------------------------------------
// AST utilities (ports of the Python helpers)

/// Cursor kinds that are expressions when they appear in statement position
/// (also: valid value-producing tails of GNU statement-expressions).
pub fn is_expr_kind(k: CXCursorKind) -> bool {
  matches!(
    k,
    CXCursor_CallExpr
      | CXCursor_BinaryOperator
      | CXCursor_UnaryOperator
      | CXCursor_ConditionalOperator
      | CXCursor_CompoundAssignOperator
      | CXCursor_CStyleCastExpr
      | CXCursor_ParenExpr
      | CXCursor_UnexposedExpr
      | CXCursor_DeclRefExpr
      | CXCursor_MemberRefExpr
      | CXCursor_ArraySubscriptExpr
      | CXCursor_IntegerLiteral
      | CXCursor_CharacterLiteral
  )
}

/// Strip parens, casts, and statement-expression wrappers.
pub fn unwrap_expr(mut cur: Cursor) -> Cursor {
  loop {
    match cur.kind() {
      CXCursor_ParenExpr | CXCursor_CStyleCastExpr | CXCursor_UnexposedExpr
      | CXCursor_StmtExpr => {
        let kids = cur.children();
        match kids.last() {
          Some(last) => cur = *last,
          None => return cur,
        }
      }
      _ => return cur,
    }
  }
}

pub fn is_noun_type(t: &Ty) -> bool {
  let s = t.spelling().replace("const ", "");
  let s = s.trim();
  config::NOUN_TYPES.contains(&s)
}

/// A single pointer to a noun (`u3_noun*` and friends): the slot-pointer
/// type the interpreter tracks. Reads through the sugared type so the
/// noun typedef spelling survives (canonical u3_noun* is unsigned int*).
pub fn is_noun_ptr_type(t: &Ty) -> bool {
  if t.canonical().kind() != CXType_Pointer {
    return false;
  }
  is_noun_type(&t.pointee_type())
}

/// The u3_weak typedef: a noun reference that may be u3_none. Every
/// other noun typedef promises a valid noun.
pub fn is_weak_type(t: &Ty) -> bool {
  let s = t.spelling().replace("const ", "");
  s.trim() == "u3_weak"
}

/// A type too narrow to hold an indirect noun reference: a noun value
/// bound to a variable of this type is necessarily a direct atom.
pub fn is_direct_type(t: &Ty) -> bool {
  let s = t.spelling().replace("const ", "");
  let s = s.trim();
  config::DIRECT_TYPES.contains(&s)
}

/// Value of an integer-constant expression (sees through macros, casts,
/// and enum constants), masked to 32 bits, or None.
pub fn int_literal_value(cur: &Cursor) -> Option<u64> {
  let u = unwrap_expr(*cur);
  match u.kind() {
    CXCursor_IntegerLiteral | CXCursor_CharacterLiteral | CXCursor_BinaryOperator
    | CXCursor_UnaryOperator | CXCursor_DeclRefExpr => {}
    _ => return None,
  }
  if u.kind() == CXCursor_DeclRefExpr {
    if let Some(r) = u.referenced() {
      if r.kind() != CXCursor_EnumConstantDecl {
        return None;
      }
    }
  }
  cur.evaluate_int().map(|v| (v as u64) & 0xffff_ffff)
}

/// Name of a simple lvalue: a variable, or a dot-member chain rooted at a
/// variable ("match.call_bat"). Arrow access returns None.
pub fn decl_ref_name(cur: &Cursor) -> Option<Name> {
  let cur = unwrap_expr(*cur);
  match cur.kind() {
    CXCursor_DeclRefExpr => Some(cur.spelling()),
    CXCursor_MemberRefExpr => {
      let kids = cur.children();
      let base = kids.first()?;
      if base.ty().canonical().kind() == CXType_Pointer {
        return None; // p->field: not a tracked path
      }
      let bname = decl_ref_name(base)?;
      Some(Rc::from(format!("{}.{}", bname, cur.spelling())))
    }
    _ => None,
  }
}

/// True if the expression is (a member chain rooted at) a local variable
/// or parameter.
pub fn is_local_lvalue(cur: &Cursor) -> bool {
  let mut cur = unwrap_expr(*cur);
  while cur.kind() == CXCursor_MemberRefExpr {
    let kids = cur.children();
    match kids.first() {
      Some(k) => cur = unwrap_expr(*k),
      None => return false,
    }
  }
  if cur.kind() != CXCursor_DeclRefExpr {
    return false;
  }
  match cur.referenced() {
    Some(r) => {
      matches!(r.kind(), CXCursor_VarDecl | CXCursor_ParmDecl) && r.has_no_linkage()
    }
    None => false,
  }
}

/// Opcode spelling of a unary operator (works inside macro expansions).
pub fn unary_op(cur: &Cursor) -> Option<String> {
  let k = cur.unop_kind();
  let m = match k {
    CXUnaryOperator_PostInc | CXUnaryOperator_PreInc => Some("++"),
    CXUnaryOperator_PostDec | CXUnaryOperator_PreDec => Some("--"),
    CXUnaryOperator_AddrOf => Some("&"),
    CXUnaryOperator_Deref => Some("*"),
    CXUnaryOperator_Plus => Some("+"),
    CXUnaryOperator_Minus => Some("-"),
    CXUnaryOperator_Not => Some("~"),
    CXUnaryOperator_LNot => Some("!"),
    _ => None,
  };
  if let Some(s) = m {
    return Some(s.to_string());
  }
  let toks = cur.tokens();
  match toks.first() {
    Some(t) if t.kind == CXToken_Punctuation => Some(t.spelling.clone()),
    _ => None,
  }
}

/// Binary operator opcodes we distinguish (CXBinaryOperatorKind values).
pub mod binop {
  pub const LT: i32 = 11;
  pub const GT: i32 = 12;
  pub const LE: i32 = 13;
  pub const GE: i32 = 14;
  pub const EQ: i32 = 15;
  pub const NE: i32 = 16;
  pub const LAND: i32 = 20;
  pub const LOR: i32 = 21;
  pub const ASSIGN: i32 = 22;
  pub const COMMA: i32 = 33;
}
