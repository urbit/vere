//! u3 noun refcount protocol checker (Rust port of tools/refcount_check.py).
//!
//! Statically verifies that functions taking/returning nouns follow the
//! reference-counting conventions documented in doc/spec/u3.md. See the
//! Python script's docstring / tools/refcount-check/README.md for the
//! annotation grammar and prefix conventions.
//!
//! Layout:
//!   config.rs  -- tables of functions/types with hard-wired meaning
//!   ast.rs     -- thin libclang wrapper + AST utilities
//!   sem1.rs    -- Sem protocol table, @Refcount: grammar, comment harvesting
//!   interp1.rs -- THE ABSTRACT INTERPRETER (swappable; see its module doc)
//!   main.rs    -- driver: cdb handling, TU iteration, output, selftest
//!
//! (sem.rs / interp.rs are the retired first port, kept on disk for
//! reference but no longer compiled.)

mod ast;
mod config;
mod interp;
mod sem;

use std::collections::HashSet;
use std::path::Path;
use std::process::exit;
use std::rc::Rc;

use ast::{is_noun_type, Cursor, Index, Tu};
use sem::{
  annotation_sync_findings, block_asserts, body_annotation_warnings, cursor_comments,
  prefix_sem, re_file_custom, resolve_sem, ArgumentMode, AssertMode,
  FileComments, FillMode, Finding,
  ProductMode, Sem, SemCache, SrcCache,
};

// ---------------------------------------------------------------------------
// misc

/// Path relative to the current directory (os.path.relpath equivalent).
pub fn relpath(p: &str) -> String {
  let path = Path::new(p);
  if !path.is_absolute() {
    return p.to_string();
  }
  let cwd = match std::env::current_dir() {
    Ok(c) => c,
    Err(_) => return p.to_string(),
  };
  let a: Vec<_> = cwd.components().collect();
  let b: Vec<_> = path.components().collect();
  let mut i = 0;
  while i < a.len() && i < b.len() && a[i] == b[i] {
    i += 1;
  }
  let mut out: Vec<String> = Vec::new();
  for _ in i..a.len() {
    out.push("..".to_string());
  }
  for c in &b[i..] {
    out.push(c.as_os_str().to_string_lossy().into_owned());
  }
  if out.is_empty() {
    ".".to_string()
  } else {
    out.join("/")
  }
}

fn read_head(path: &str, n: usize) -> String {
  let data = std::fs::read(path).unwrap_or_default();
  let end = n.min(data.len());
  String::from_utf8_lossy(&data[..end]).into_owned()
}

// ---------------------------------------------------------------------------
// compile_commands.json

#[derive(Clone)]
struct Entry {
  file: String,
  arguments: Vec<String>,
}

fn load_cdb(path: &str) -> Result<Vec<Entry>, String> {
  let text = std::fs::read_to_string(path).map_err(|e| format!("{}: {}", path, e))?;
  let val: serde_json::Value =
    serde_json::from_str(&text).map_err(|e| format!("{}: {}", path, e))?;
  let arr = val.as_array().ok_or_else(|| format!("{}: not a JSON array", path))?;
  let mut out = Vec::new();
  for e in arr {
    let file = e["file"].as_str().unwrap_or_default().to_string();
    let arguments: Vec<String> = e["arguments"]
      .as_array()
      .map(|a| {
        a.iter()
          .map(|x| x.as_str().unwrap_or_default().to_string())
          .collect()
      })
      .unwrap_or_default();
    out.push(Entry { file, arguments });
  }
  Ok(out)
}

fn lint_args(e: &Entry, resource_dir: Option<&str>) -> Vec<String> {
  let args = &e.arguments[1..];
  let zig_root = Path::new(&e.arguments[0])
    .parent()
    .map(|p| p.to_string_lossy().into_owned())
    .unwrap_or_default();
  let skip2 = [
    "-o",
    "--serialize-diagnostics",
    "-gen-cdb-fragment-path",
    "--param",
    "-MF",
    "-MD",
  ];
  let mut out: Vec<String> = Vec::new();
  let mut i = 0;
  while i < args.len() {
    let a = &args[i];
    if skip2.contains(&a.as_str()) {
      i += 2;
      continue;
    }
    //  -Werror: the lint parse (U3_REFCOUNT_LINT macro swaps) can raise
    //  warnings the real build does not; they must not block parsing
    if *a == e.file || a == "-xc" || a == "-c" || a == "-Werror" {
      i += 1;
      continue;
    }
    //  pkg/vere entries resolve the noun headers through a .zig-cache
    //  SNAPSHOT dir; the live pkg/noun sources must win (in the same
    //  search position, so the generated version.h still shadows
    //  pkg/noun/version.h), or annotations edited in pkg/noun stay
    //  invisible until the next cache rebuild
    if a == "-I" && i + 1 < args.len() {
      let path = &args[i + 1];
      if path.contains(".zig-cache")
        && Path::new(path).join("noun.h").is_file()
      {
        out.push("-I".to_string());
        out.push("pkg/noun".to_string());
        //  the snapshot flattens platform/<os>/rsignal.h into its root
        let os_dir = if cfg!(target_os = "macos") {
          "pkg/noun/platform/darwin"
        } else if cfg!(target_os = "windows") {
          "pkg/noun/platform/windows"
        } else {
          "pkg/noun/platform/linux"
        };
        out.push("-I".to_string());
        out.push(os_dir.to_string());
        i += 2;
        continue;
      }
    }
    if a == "-isystem" && i + 1 < args.len() {
      let path = &args[i + 1];
      if !zig_root.is_empty()
        && path.starts_with(&zig_root)
        && path.ends_with("/lib/include")
      {
        if let Some(rd) = resource_dir {
          out.push("-isystem".to_string());
          out.push(rd.to_string());
          i += 2;
          continue;
        }
      }
    }
    out.push(a.clone());
    i += 1;
  }
  let mut res: Vec<String> = vec![
    "-xc".to_string(),
    "-DU3_REFCOUNT_LINT".to_string(),
    "-fparse-all-comments".to_string(),
    "-ferror-limit=0".to_string(),
  ];
  res.extend(out);
  res
}

// ---------------------------------------------------------------------------
// precompiled headers

//  candidate prefix headers, richest first: each args group takes the
//  first candidate that compiles cleanly under its flags. pkg/vere
//  groups resolve vere.h (uv.h and friends on top of noun.h); pkg/noun
//  groups lack pkg/vere on their include path, fail fast on the missing
//  include, and fall back to the noun-only header (whose closure covers
//  every pkg/noun TU -- noun.h pulls in the whole tree except jets/w.h).
//  The paths resolve through the entries' own -I flags, which are
//  relative to the repo root like everything else in the compile db.
const PCH_HEADERS: [&str; 2] = [
  "#include \"vere.h\"\n#include \"noun.h\"\n#include \"jets/w.h\"\n",
  "#include \"noun.h\"\n#include \"jets/w.h\"\n",
];

/// One precompiled header per distinct lint_args vector (in practice
/// two: every pkg/noun entry normalizes to one flags vector and every
/// pkg/vere entry to another).
/// Re-parsing the shared headers per TU is ~90% of the tool's CPU time;
/// a PCH parses them once. The PCH build args must match the consumers'
/// exactly -- clang validates some mismatches loudly (macros, -std,
/// mtime of any recorded file) but include-path drift is accepted
/// silently, so both sides use the same vector. Disabled by --no-pch.
struct PchSet {
  dir: Option<std::path::PathBuf>,
  by_args: std::collections::HashMap<Vec<String>, String>,
  skip: HashSet<String>,
}

/// A file with a preprocessor directive before its first #include must
/// not be parsed against the PCH: the PCH replays the headers' macro
/// state, turning the file's own includes into no-ops, so a macro meant
/// to configure them (say a hypothetical U3_MEMORY_DEBUG above the
/// includes) would be ignored with no diagnostic at all.
fn pre_include_directives(path: &str) -> bool {
  let text = std::fs::read_to_string(path).unwrap_or_default();
  for line in text.lines() {
    let t = line.trim_start();
    if !t.starts_with('#') {
      continue;
    }
    let d = t[1..].trim_start();
    if d.starts_with("include") {
      return false;
    }
    if d.starts_with("define") || d.starts_with("undef")
      || d.starts_with("if")
    {
      return true;
    }
  }
  false
}

impl PchSet {
  fn none() -> PchSet {
    PchSet {
      dir: None,
      by_args: std::collections::HashMap::new(),
      skip: HashSet::new(),
    }
  }

  fn build(entries: &[Entry], resource_dir: Option<&str>, min_group: u32)
    -> PchSet
  {
    //  reclaim dirs left behind by interrupted runs: Drop never fires
    //  on a signal, and pid-keyed names are never reused
    if let Ok(rd) = std::fs::read_dir(std::env::temp_dir()) {
      for ent in rd.flatten() {
        let name = ent.file_name();
        if let Some(pid) = name.to_string_lossy()
          .strip_prefix("refcount-check-pch-")
        {
          if pid.parse::<u32>().is_ok()
            && !Path::new(&format!("/proc/{}", pid)).exists()
          {
            let _ = std::fs::remove_dir_all(ent.path());
          }
        }
      }
    }
    let mut set = PchSet::none();
    let mut groups: std::collections::HashMap<Vec<String>, u32> =
      std::collections::HashMap::new();
    for e in entries {
      if pre_include_directives(&e.file) {
        set.skip.insert(e.file.clone());
        continue;
      }
      *groups.entry(lint_args(e, resource_dir)).or_insert(0) += 1;
    }
    let dir = std::env::temp_dir()
      .join(format!("refcount-check-pch-{}", std::process::id()));
    let idx = Index::new();
    let mut n = 0u32;
    let mut keys: Vec<&Vec<String>> = groups.keys().collect();
    keys.sort(); // deterministic pch numbering
    for args in keys {
      if groups[args] < min_group.max(1) {
        continue; // a PCH costs about one TU parse; no gain for one TU
      }
      if set.dir.is_none() {
        if std::fs::create_dir_all(&dir).is_err() {
          return set;
        }
        set.dir = Some(dir.clone());
      }
      let mut hargs = args.clone();
      if hargs.first().map(|a| a == "-xc").unwrap_or(false) {
        hargs[0] = "-xc-header".to_string();
      }
      let opts = clang_sys::CXTranslationUnit_Incomplete
        | clang_sys::CXTranslationUnit_ForSerialization;
      for (i, text) in PCH_HEADERS.iter().enumerate() {
        //  never rewrite a header another group already built against:
        //  a fresh mtime would invalidate that group's saved PCH
        let hdr = dir.join(format!("prefix-{}.h", i));
        if !hdr.exists() && std::fs::write(&hdr, text).is_err() {
          continue;
        }
        let Ok(tu) = idx.parse_opts(&hdr.to_string_lossy(), &hargs, opts)
        else {
          continue;
        };
        //  a save can succeed for a TU that failed to compile; a broken
        //  PCH must not replace working per-TU parses (and an
        //  unresolvable candidate include lands here, trying the next)
        if !tu.error_diagnostics().is_empty() {
          continue;
        }
        let pch = dir.join(format!("prefix-{}.pch", n));
        n += 1;
        if tu.save(&pch.to_string_lossy()).is_ok() {
          set
            .by_args
            .insert(args.clone(), pch.to_string_lossy().into_owned());
          break;
        }
      }
    }
    set
  }
}

impl Drop for PchSet {
  fn drop(&mut self) {
    if let Some(d) = &self.dir {
      let _ = std::fs::remove_dir_all(d);
    }
  }
}

/// Parse with the entry's group PCH when there is one, plain otherwise.
/// A PCH-rejecting parse (CXError_ASTReadError after e.g. a header was
/// edited mid-run) falls back to a plain parse instead of failing.
fn parse_entry(idx: &Index, file: &str, args: &[String], pchs: &PchSet)
  -> Result<Tu, String>
{
  if !pchs.skip.contains(file) {
    if let Some(p) = pchs.by_args.get(args) {
      let mut a = args.to_vec();
      a.push("-include-pch".to_string());
      a.push(p.clone());
      if let Ok(tu) = idx.parse(file, &a) {
        return Ok(tu);
      }
    }
  }
  idx.parse(file, args)
}

fn glob_sorted(pattern: &str) -> Vec<String> {
  let mut v: Vec<String> = glob::glob(pattern)
    .map(|it| {
      it.flatten()
        .map(|p| p.to_string_lossy().into_owned())
        .collect()
    })
    .unwrap_or_default();
  v.sort();
  v
}

fn find_resource_dir(libclang_path: Option<&str>) -> Option<String> {
  if let Some(lib) = libclang_path {
    if let Some(root) = Path::new(lib).parent().and_then(|p| p.parent()) {
      let cands = glob_sorted(&format!("{}/lib/clang/*/include", root.display()));
      if let Some(last) = cands.last() {
        return Some(last.clone());
      }
    }
  }
  let cands = glob_sorted("/usr/lib/llvm-*/lib/clang/*/include");
  cands.into_iter().next_back()
}

fn find_libclang() -> Option<String> {
  let cands = glob_sorted("/usr/lib/llvm-*/lib/libclang.so*");
  cands.into_iter().next_back()
}

// ---------------------------------------------------------------------------
// interpreter host

struct DriverHost<'a> {
  sem_cache: &'a mut SemCache,
  src: &'a mut SrcCache,
  fcm: &'a FileComments,
}

impl interp::Host for DriverHost<'_> {
  fn callee_sem(&mut self, callee: &Cursor) -> Rc<Sem> {
    resolve_sem(callee, self.sem_cache, self.src)
  }

  fn block_asserts(&self, compound: &Cursor) -> Vec<(AssertMode, Vec<String>)> {
    block_asserts(compound, self.fcm)
  }
}

// ---------------------------------------------------------------------------
// --explain

fn explain(
  entries: &[Entry],
  resource_dir: Option<&str>,
  pchs: &PchSet,
  target: &str,
) -> i32 {
  let (fpart, fname) = match target.rfind(':') {
    Some(i) => (Some(&target[..i]), &target[i + 1..]),
    None => (None, target),
  };
  let idx = Index::new();
  let mut ordered: Vec<&Entry> = entries.iter().collect();
  ordered.sort_by(|a, b| a.file.cmp(&b.file));
  if let Some(fp) = fpart {
    // entries matching the file part first (headers match via the
    // cursor's own location below)
    ordered.sort_by_key(|e| !e.file.contains(fp));
  }
  let mut tus: Vec<Tu> = Vec::new(); // keep found cursors alive
  let mut found: Option<Cursor> = None;
  'outer: for e in &ordered {
    let tu = match parse_entry(&idx, &e.file, &lint_args(e, resource_dir), pchs)
    {
      Ok(t) => t,
      Err(_) => continue,
    };
    if !tu.error_diagnostics().is_empty() {
      continue;
    }
    let root = tu.cursor();
    tus.push(tu);
    for cur in root.children() {
      if cur.kind() != clang_sys::CXCursor_FunctionDecl || &*cur.spelling() != fname {
        continue;
      }
      if let Some(fp) = fpart {
        let cfile = cur.location().file.unwrap_or_default();
        if !cfile.contains(fp) && !e.file.contains(fp) {
          continue;
        }
      }
      if found.is_none() || cur.is_definition() {
        found = Some(cur);
      }
      if found.map(|f| f.is_definition()) == Some(true) {
        break 'outer;
      }
    }
  }
  let Some(cur) = found else {
    let where_ = fpart
      .map(|f| format!(" in files matching \"{}\"", f))
      .unwrap_or_default();
    eprintln!("--explain: function '{}' not found{}", fname, where_);
    return 2;
  };

  let defn = if cur.is_definition() {
    Some(cur)
  } else {
    cur.definition()
  };
  let show = defn.unwrap_or(cur);
  let loc = show.location();
  let loc_file = loc.file.clone().unwrap_or_default();
  let mut sem_cache = SemCache::new();
  let mut src = SrcCache::default();
  let sem = resolve_sem(&cur, &mut sem_cache, &mut src);

  println!(
    "{}  ({}:{}{})",
    fname,
    relpath(&loc_file),
    loc.line,
    if defn.is_some() { "" } else { ", declaration only" }
  );
  println!("  resolved by: {}", sem.why);
  if sem.noreturn {
    println!(
      "  noreturn:    execution ends at every call site; arguments are \
       not accounted"
    );
  }
  if sem.custom {
    println!(
      "  protocol:    CUSTOM -- not checked; call sites treat arguments and product as unknown"
    );
  } else if !sem.check {
    println!(
      "  protocol:    trusted ({} args, {} product); body NOT checked",
      sem.default_args.as_str(),
      sem.product.as_str()
    );
  }
  println!("  arguments:");
  for p in show.arguments() {
    let pname = {
      let s = p.spelling();
      if s.is_empty() { "<unnamed>".to_string() } else { s.to_string() }
    };
    let tspell = p.ty().spelling();
    if !is_noun_type(&p.ty()) {
      if let Some(pm) = sem.pointees.get(&pname) {
        let mut parts: Vec<&str> = Vec::new();
        if pm.reads {
          parts.push("reads the pointee");
        }
        if pm.consumes {
          parts.push("consumes the old pointee");
        }
        match pm.fills {
          Some(FillMode::Transferred) => {
            parts.push("fills it with an owned value (caller must u3z)")
          }
          Some(FillMode::Retained) => {
            parts.push("fills it with an uncounted view")
          }
          None => {}
        }
        match pm.fill_on {
          Some(true) => parts.push("only when returning c3y"),
          Some(false) => parts.push("only when returning c3n"),
          None => {}
        }
        println!("    {:<12} {:<12} POINTEE: {}", pname, tspell,
          parts.join("; "));
      } else {
        println!("    {:<12} {:<12} (not a noun: untracked)", pname, tspell);
      }
      continue;
    }
    let mode = match sem.arg_mode(&pname) {
      ArgumentMode::Passthrough => {
        "PASSTHROUGH (the product is this argument itself)".to_string()
      }
      ArgumentMode::Direct => {
        "DIRECT (proven direct atom if the call returns)".to_string()
      }
      ArgumentMode::Conslike => {
        "CONSLIKE (consumed, but stays alive inside the product)".to_string()
      }
      m => {
        let src_why = if sem.args.contains_key(&pname) {
          "per-arg annotation"
        } else {
          sem.why.as_str()
        };
        format!("{:<12} ({})", m.as_str().to_uppercase(), src_why)
      }
    };
    println!("    {:<12} {:<12} {}", pname, tspell, mode);
  }
  let rt = show.result_type();
  if !is_noun_type(&rt) {
    println!("  product:     {} (not a noun: untracked)", rt.spelling());
  } else {
    let note = match &sem.product {
      ProductMode::Transfer => " (caller owns it and must u3z)",
      ProductMode::Retain => " (uncounted; caller must NOT free)",
      ProductMode::Direct => " (direct atom: no counted references)",
      ProductMode::Passthrough => " (same value and ownership as the argument)",
      ProductMode::NonNoun => "",
    };
    println!(
      "  product:     {}{}",
      sem.product.as_str().to_uppercase(),
      note
    );
  }
  let mut file_custom = false;
  let mut dfile = String::new();
  if let Some(d) = &defn {
    if let Some(f) = d.location().file {
      dfile = f.to_string();
      file_custom = re_file_custom().is_match(&read_head(&dfile, 4096));
    }
  }
  let checked = sem.check && !sem.custom && !file_custom;
  println!(
    "  body checked: {}{}",
    if checked { "yes" } else { "no" },
    if file_custom {
      format!(
        " ({}: all functions are custom unless asserted otherwise)",
        relpath(&dfile)
      )
    } else {
      String::new()
    }
  );

  let comments = cursor_comments(&cur, &mut src);
  let comments = comments.trim();
  if !comments.is_empty() {
    println!("  annotation comments considered:");
    for line in comments.lines() {
      println!("    | {}", line.trim());
    }
  } else {
    println!("  annotation comments considered: none (prefix/position defaults apply)");
  }

  let base = prefix_sem(fname, &loc_file, cur.is_static());
  println!(
    "  positional default (before comments): {} args, {} product [{}]",
    base.default_args.as_str(),
    base.product.as_str(),
    base.why
  );
  0
}

// ---------------------------------------------------------------------------
// CLI

#[derive(Default)]
struct Args {
  cdb: String,
  filters: Vec<String>,
  only: Option<String>,
  function: Option<String>,
  libclang: Option<String>,
  verbose: bool,
  selftest: bool,
  no_pch: bool,
  asserted: bool,
  strict_weak: bool,
  explain: Option<String>,
}

fn parse_args() -> Args {
  let mut a = Args {
    cdb: "compile_commands.json".to_string(),
    ..Default::default()
  };
  let argv: Vec<String> = std::env::args().skip(1).collect();
  fn value(i: &mut usize, argv: &[String], flag: &str, inline: Option<String>) -> String {
    if let Some(v) = inline {
      return v;
    }
    *i += 1;
    match argv.get(*i) {
      Some(v) => v.clone(),
      None => {
        eprintln!("missing value for {}", flag);
        exit(2);
      }
    }
  }
  let mut i = 0;
  while i < argv.len() {
    let arg = argv[i].clone();
    let (flag, inline) = match arg.split_once('=') {
      Some((f, v)) => (f.to_string(), Some(v.to_string())),
      None => (arg.clone(), None),
    };
    match flag.as_str() {
      "--cdb" => a.cdb = value(&mut i, &argv, "--cdb", inline),
      "--filter" => {
        //  repeatable, and each value may be a comma-separated list
        let v = value(&mut i, &argv, "--filter", inline);
        a.filters
          .extend(v.split(',').filter(|s| !s.is_empty()).map(str::to_string));
      }
      "--only" => a.only = Some(value(&mut i, &argv, "--only", inline)),
      "--function" => a.function = Some(value(&mut i, &argv, "--function", inline)),
      "--libclang" => a.libclang = Some(value(&mut i, &argv, "--libclang", inline)),
      "--explain" => a.explain = Some(value(&mut i, &argv, "--explain", inline)),
      "--verbose" => a.verbose = true,
      "--selftest" => a.selftest = true,
      "--no-pch" => a.no_pch = true,
      "--asserted" => a.asserted = true,
      "--strict-weak" => a.strict_weak = true,
      "-h" | "--help" => {
        println!(
          "usage: refcount-check [--cdb F] [--filter S] [--only S] [--function F] \
          [--libclang PATH] [--verbose] [--selftest] [--no-pch] [--asserted] \
          [--strict-weak] [--explain [FILE:]FUNCTION]"
        );
        exit(0);
      }
      other => {
        eprintln!("unknown argument: {}", other);
        exit(2);
      }
    }
    i += 1;
  }
  a
}

// ---------------------------------------------------------------------------
// per-entry work (runs on a worker thread)

/// Everything one translation unit produced, buffered so the main thread
/// can emit it in deterministic (sorted-entry) order.
#[derive(Default)]
struct EntryOut {
  stdout: Vec<String>,
  stderr: Vec<String>,
  findings: Vec<Finding>,
  n_checked: u32,
}

//  temporary profiling counters, reported when REFCOUNT_TIMING is set
static PARSE_US: std::sync::atomic::AtomicU64 =
  std::sync::atomic::AtomicU64::new(0);
static CHECK_US: std::sync::atomic::AtomicU64 =
  std::sync::atomic::AtomicU64::new(0);
static PCH_US: std::sync::atomic::AtomicU64 =
  std::sync::atomic::AtomicU64::new(0);
static WALK_US: std::sync::atomic::AtomicU64 =
  std::sync::atomic::AtomicU64::new(0);

struct CheckTimer(std::time::Instant);
impl Drop for CheckTimer {
  fn drop(&mut self) {
    CHECK_US.fetch_add(self.0.elapsed().as_micros() as u64,
      std::sync::atomic::Ordering::Relaxed);
  }
}

/// One-line protocol summary for --asserted listings.
fn sem_brief(sem: &Sem) -> String {
  let mut parts = vec![
    format!("args={}", sem.default_args.as_str()),
    format!("product={}", sem.product.as_str()),
  ];
  for (n, m) in &sem.args {
    parts.push(format!("{}={}", n, m.as_str()));
  }
  for (n, pm) in &sem.pointees {
    let mut bits: Vec<&str> = Vec::new();
    if pm.reads {
      bits.push("reads");
    }
    if pm.consumes {
      bits.push("consumes");
    }
    match pm.fills {
      Some(FillMode::Transferred) => bits.push("fills-transferred"),
      Some(FillMode::Retained) => bits.push("fills-retained"),
      None => {}
    }
    if let Some(on) = pm.fill_on {
      bits.push(if on { "on-c3y" } else { "on-c3n" });
    }
    parts.push(format!("*{}={}", n, bits.join("+")));
  }
  parts.join(", ")
}

/// Fast textual sweep over a definition's source extent: does it
/// mention any noun typedef as a whole word? Decides whether a
/// function with no nouns in its signature is worth checking anyway.
fn mentions_noun_types(cur: &Cursor, src: &mut SrcCache) -> bool {
  let s = cur.extent_start();
  let e = cur.extent_end();
  let Some(f) = &s.file else { return false; };
  let bytes = src.bytes(f);
  let lo = s.offset as usize;
  let hi = (e.offset as usize).min(bytes.len());
  if lo >= hi {
    return false;
  }
  let text = &bytes[lo..hi];
  let word = |b: u8| b == b'_' || b.is_ascii_alphanumeric();
  config::NOUN_TYPES.iter().any(|t| {
    let t = t.as_bytes();
    text.windows(t.len()).enumerate().any(|(i, w)| {
      w == t
        && (i == 0 || !word(text[i - 1]))
        && text.get(i + t.len()).is_none_or(|b| !word(*b))
    })
  })
}

fn process_entry(
  idx: &Index,
  e: &Entry,
  resource_dir: Option<&str>,
  args: &Args,
  pchs: &PchSet,
  sem_cache: &mut SemCache,
  src: &mut SrcCache,
) -> EntryOut {
  let mut out = EntryOut::default();
  let fpath = &e.file;
  let t0 = std::time::Instant::now();
  let tu = match parse_entry(idx, fpath, &lint_args(e, resource_dir), pchs) {
    Ok(t) => t,
    Err(ex) => {
      out.stderr.push(format!("{}: PARSE FAILED: {}", fpath, ex));
      return out;
    }
  };
  PARSE_US.fetch_add(t0.elapsed().as_micros() as u64,
    std::sync::atomic::Ordering::Relaxed);
  let _check_timer = CheckTimer(std::time::Instant::now());
  let hard = tu.error_diagnostics();
  if !hard.is_empty() {
    out.stderr
      .push(format!("{}: {} parse errors (first: {})", fpath, hard.len(), hard[0]));
    return out;
  }
  //  a file-custom file skips BODY checking only: annotation hygiene
  //  (warnings, decl/def sync) still applies to its definitions
  let file_custom = re_file_custom().is_match(&read_head(fpath, 4096));
  if file_custom && args.verbose {
    out.stdout.push(format!(
      "-- {}: all functions are custom unless asserted otherwise, \
       bodies skipped",
      fpath
    ));
  }
  let fcm = FileComments::new(&tu, fpath);
  let tw = std::time::Instant::now();
  //  enumerate via the indexing API, not tu.cursor().children(): the
  //  root walk would materialize every declaration the PCH brought in
  let decls = ast::local_decls(idx, &tu);
  WALK_US.fetch_add(tw.elapsed().as_micros() as u64,
    std::sync::atomic::Ordering::Relaxed);
  for cur in decls {
    if cur.kind() != clang_sys::CXCursor_FunctionDecl || !cur.is_definition() {
      continue;
    }
    match cur.location().file {
      Some(f) if &*f == fpath.as_str() => {}
      _ => continue,
    }
    if let Some(f) = &args.function {
      if &*cur.spelling() != f.as_str() {
        continue;
      }
    }
    // only functions that take or return nouns -- or, for a nounless
    // signature (driver callbacks and friends), whose body mentions a
    // noun typedef: those still hold nouns in locals and can leak
    let takes = cur
      .arguments()
      .iter()
      .any(|p| p.kind() == clang_sys::CXCursor_ParmDecl && is_noun_type(&p.ty()));
    let rets = is_noun_type(&cur.result_type());
    if !takes && !rets && !mentions_noun_types(&cur, src) {
      continue;
    }
    let sem = resolve_sem(&cur, sem_cache, src);
    let loc = cur.location();
    for (wl, wmsg) in &sem.warnings {
      out.findings.push(Finding {
        file: fpath.clone(),
        line: if *wl != 0 { *wl } else { loc.line },
        col: loc.col,
        func: cur.spelling().to_string(),
        cat: "annotation",
        msg: wmsg.clone(),
      });
    }
    for (sl, smsg) in annotation_sync_findings(&cur, fpath, src) {
      out.findings.push(Finding {
        file: fpath.clone(),
        line: sl,
        col: loc.col,
        func: cur.spelling().to_string(),
        cat: "annotation",
        msg: smsg,
      });
    }
    if file_custom {
      continue;
    }
    if !sem.check {
      //  `assert`ed protocols in non-custom files are the annotation
      //  debt --asserted lists: trusted claims a future analysis
      //  (builder/slot tracking) should verify instead
      if args.asserted && !sem.custom {
        out.stdout.push(format!(
          "{}:{}: {} [{}]",
          relpath(fpath),
          loc.line,
          cur.spelling(),
          sem_brief(&sem)
        ));
      }
      if args.verbose {
        out.stdout
          .push(format!("-- {}: trusted ({})", cur.spelling(), sem.why));
      }
      continue;
    }
    out.n_checked += 1;
    if args.verbose {
      out.stdout.push(format!(
        "-- {}: args={} product={} ({})",
        cur.spelling(),
        sem.default_args.as_str(),
        sem.product.as_str(),
        sem.why
      ));
    }
    for (wl, wmsg) in body_annotation_warnings(&cur, &fcm) {
      out.findings.push(Finding {
        file: fpath.clone(),
        line: wl,
        col: 1,
        func: cur.spelling().to_string(),
        cat: "annotation",
        msg: wmsg,
      });
    }
    let mut host = DriverHost { sem_cache, src, fcm: &fcm };
    out.findings
      .extend(interp::check_function(&mut host, &cur, &sem));
  }
  out
}

// ---------------------------------------------------------------------------
// main

fn main() {
  exit(run());
}

fn run() -> i32 {
  let args = parse_args();
  config::STRICT_WEAK.store(args.strict_weak,
    std::sync::atomic::Ordering::Relaxed);

  let lib = args.libclang.clone().or_else(find_libclang);
  if let Err(e) = ast::init(lib.as_deref()) {
    eprintln!("failed to load libclang: {}", e);
    return 2;
  }
  let resource_dir = find_resource_dir(lib.as_deref());

  let cdb = match load_cdb(&args.cdb) {
    Ok(c) => c,
    Err(e) => {
      eprintln!("cannot load compile db: {}", e);
      return 2;
    }
  };
  let filters: Vec<String> = if args.filters.is_empty() {
    vec!["pkg/noun".to_string(), "pkg/vere".to_string()]
  } else {
    args.filters.clone()
  };
  let mut seen_files: HashSet<String> = HashSet::new();
  let mut entries: Vec<Entry> = Vec::new();
  for e in cdb {
    if !e.file.ends_with(".c")
      || !filters.iter().any(|f| e.file.contains(f.as_str()))
    {
      continue;
    }
    //  test harnesses and benchmarks are outside the checked runtime;
    //  ivory.c and ca_bundle.c are generated megabyte array literals
    //  with no functions -- parsing them costs seconds for nothing
    let base = e.file.rsplit('/').next().unwrap_or(&e.file);
    if base.ends_with("_test.c") || base.ends_with("_tests.c")
      || base == "benchmarks.c" || base == "ivory.c"
      || base == "ca_bundle.c"
    {
      continue;
    }
    if let Some(only) = &args.only {
      if !e.file.contains(only) {
        continue;
      }
    }
    if !seen_files.insert(e.file.clone()) {
      continue;
    }
    entries.push(e);
  }

  if args.selftest {
    // borrow compile flags from any pkg/noun entry (the fixture
    // includes noun headers; vere flags would also work, but pin it)
    entries.retain(|e| e.file.contains("pkg/noun"));
    if entries.is_empty() {
      eprintln!("selftest: no pkg/noun entries in cdb");
      return 2;
    }
    let test_c = Path::new(env!("CARGO_MANIFEST_DIR"))
      .parent()
      .map(|p| p.join("refcount_selftest.c"))
      .unwrap_or_default()
      .to_string_lossy()
      .into_owned();
    let old = entries[0].clone();
    let mut e = old.clone();
    e.arguments = old
      .arguments
      .iter()
      .map(|a| if *a == old.file { test_c.clone() } else { a.clone() })
      .collect();
    e.file = test_c;
    entries = vec![e];
  }

  let t0 = std::time::Instant::now();
  let pchs = if args.no_pch {
    PchSet::none()
  } else {
    //  a PCH only pays off for 2+ TUs, except under --selftest, which
    //  must exercise the same PCH parse path CI relies on
    PchSet::build(&entries, resource_dir.as_deref(),
      if args.selftest { 1 } else { 2 })
  };
  PCH_US.fetch_add(t0.elapsed().as_micros() as u64,
    std::sync::atomic::Ordering::Relaxed);

  if let Some(target) = &args.explain {
    return explain(&entries, resource_dir.as_deref(), &pchs, target);
  }

  entries.sort_by(|a, b| a.file.cmp(&b.file));

  // Thread pool over translation units: libclang parsing dominates the
  // runtime and every entry is independent. Each worker owns its own
  // CXIndex and sem/source caches; results are buffered per entry and
  // emitted in sorted-entry order, so the output is deterministic and
  // identical to a sequential run.
  let n_threads = std::thread::available_parallelism()
    .map(|n| n.get())
    .unwrap_or(1)
    .min(entries.len())
    .max(1);
  // clang-sys keeps the runtime-loaded libclang in a thread-local; each
  // worker thread must install the main thread's handle before use.
  let shared_lib = clang_sys::get_library();
  let next = std::sync::atomic::AtomicUsize::new(0);
  let mut results: Vec<Option<EntryOut>> = Vec::new();
  results.resize_with(entries.len(), || None);
  let slots: Vec<std::sync::Mutex<Option<EntryOut>>> =
    results.into_iter().map(std::sync::Mutex::new).collect();
  let entries_ref = &entries;
  let args_ref = &args;
  let rd = resource_dir.as_deref();
  let next_ref = &next;
  let slots_ref = &slots;
  let pchs_ref = &pchs;
  std::thread::scope(|s| {
    for _ in 0..n_threads {
      let shared_lib = shared_lib.clone();
      // the abstract interpreter recurses on AST depth; urwasm.c
      // overflows the 2 MiB default
      let builder = std::thread::Builder::new().stack_size(64 << 20);
      builder.spawn_scoped(s, move || {
        clang_sys::set_library(shared_lib);
        let idx = Index::new();
        let mut sem_cache = SemCache::new();
        let mut src = SrcCache::default();
        loop {
          let i = next_ref.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
          if i >= entries_ref.len() {
            break;
          }
          let out = process_entry(
            &idx,
            &entries_ref[i],
            rd,
            args_ref,
            pchs_ref,
            &mut sem_cache,
            &mut src,
          );
          *slots_ref[i].lock().unwrap() = Some(out);
        }
      }).expect("spawn worker");
    }
  });

  let mut findings: Vec<Finding> = Vec::new();
  let mut n_checked = 0u32;
  for slot in slots {
    let Some(out) = slot.into_inner().unwrap() else { continue };
    for line in &out.stdout {
      println!("{}", line);
    }
    for line in &out.stderr {
      eprintln!("{}", line);
    }
    findings.extend(out.findings);
    n_checked += out.n_checked;
  }

  if std::env::var_os("REFCOUNT_TIMING").is_some() {
    eprintln!(
      "timing: pch {:.2}s, parse {:.2}s cpu, check {:.2}s cpu \
       (root walk {:.2}s)",
      PCH_US.load(std::sync::atomic::Ordering::Relaxed) as f64 / 1e6,
      PARSE_US.load(std::sync::atomic::Ordering::Relaxed) as f64 / 1e6,
      CHECK_US.load(std::sync::atomic::Ordering::Relaxed) as f64 / 1e6,
      WALK_US.load(std::sync::atomic::Ordering::Relaxed) as f64 / 1e6
    );
  }

  for f in &findings {
    println!(
      "{}:{}:{}: [{}] {}(): {}",
      relpath(&f.file),
      f.line,
      f.col,
      f.cat,
      f.func,
      f.msg
    );
  }
  eprintln!("\n{} functions checked, {} findings", n_checked, findings.len());

  if args.selftest {
    let expected: HashSet<(&str, &str)> = HashSet::from([
      ("bug_leak", "leak"),
      //  double-free / over-free arrive as use-after-free / refcount
      //  error under the value-numbered interpreter
      ("bug_double", "use-after-free"),
      ("bug_overfree", "refcount error"),
      ("bug_uaf", "use-after-free"),
      ("bug_borrow", "use-after-free"),
      ("bug_smuggle", "leak"),
      ("warn_conflict", "annotation"),
      ("warn_typo_assert", "annotation"),
      ("bug_switch_tail", "leak"),
      ("bug_slam_stale", "use-after-free"),
      ("bug_indirect_int", "strange expression"),
      ("bug_pointee_overwrite", "leak"),
      ("bug_pointee_view_uaf", "use-after-free"),
      ("bug_defcons_unfilled", "refcount error"),
      ("bug_defcons_double", "use-after-free"),
      ("bug_fnptr_borrowed", "refcount error"),
      ("bug_vararg_borrowed", "refcount error"),
      ("warn_noreturn_return", "annotation"),
      ("bug_cond_fill_wrongpath", "leak"),
      ("bug_cond_view_wrongpath", "refcount error"),
      ("skip_back_goto", "complicated"),
      ("bug_weak_use", "u3_none"),
      ("bug_weak_return", "u3_none"),
      ("bug_weak_lit", "u3_none"),
      ("bug_weak_gain", "u3_none"),
      ("bug_weak_bind", "u3_none"),
      ("bug_weak_param", "u3_none"),
      ("bug_weak_ternary", "u3_none"),
      //  KNOWN LIMITATION: one env per branch cannot carry the
      //  disjunction "a direct OR b direct" past the || join, so the
      //  min-shape reports instead of verifying (u3qa_min/u3qa_max
      //  need `assert` annotations for now)
      ("ok_min_shape", "refcount error"),
    ]);
    let clean_fns: HashSet<&str> = HashSet::from([
      "bug_ok",
      "ok_retain_prod",
      "ok_passthrough",
      "custom_unchecked",
      "assert_unchecked",
      "needs_direct",
      "ok_direct_caller",
      "ok_block",
      "ok_fwd_goto",
      "ok_pointee_reads",
      "ok_pointee_fill",
      "ok_pointee_update",
      "ok_pointee_view",
      "_peek_pointee",
      "_fill_pointee",
      "_bump_pointee",
      "_view_pointee",
      "ok_defcons_build",
      "ok_double_gain",
      "selftest_die",
      "ok_noreturn_caller",
      "ok_noreturn_leak",
      "_cond_fill",
      "ok_cond_fill",
      "ok_doomed",
      "ok_slot_destructure",
      "ok_gain_deref",
      "ok_fnptr_field",
      "ok_cond_view_gain",
      "_cond_view",
      "ok_cond_view_annot",
      "ok_assert_direct",
      "ok_fnptr_transfer",
      "ok_vararg_list",
      "ok_git_untied",
      "ok_fnptr_decl",
      "weak_find",
      "ok_weak_flow",
      "ok_weak_param",
      "ok_weak_eqlit",
      "ok_weak_good",
      "weak_lose_unchecked",
      "ok_loop_init",
      "ok_consume_or_alias",
    ]);
    let got: HashSet<(&str, &str)> = findings
      .iter()
      .map(|f| (f.func.as_str(), f.cat))
      .collect();
    let found_fns: HashSet<&str> = findings.iter().map(|f| f.func.as_str()).collect();
    let mut missing: Vec<_> = expected.difference(&got).collect();
    missing.sort();
    let mut dirty: Vec<_> = clean_fns.intersection(&found_fns).collect();
    dirty.sort();

    //  second pass under --strict-weak: u3z of a possibly-none value
    //  is only tolerated by default
    config::STRICT_WEAK.store(true, std::sync::atomic::Ordering::Relaxed);
    let idx = Index::new();
    let mut sem_cache = SemCache::new();
    let mut src = SrcCache::default();
    let strict_out = process_entry(&idx, &entries[0],
      resource_dir.as_deref(), &args, &pchs, &mut sem_cache, &mut src);
    config::STRICT_WEAK.store(args.strict_weak,
      std::sync::atomic::Ordering::Relaxed);
    let strict_got: HashSet<(&str, &str)> = strict_out.findings
      .iter()
      .map(|f| (f.func.as_str(), f.cat))
      .collect();
    let mut strict_bad: Vec<String> = Vec::new();
    if !strict_got.contains(&("weak_lose_unchecked", "u3_none")) {
      strict_bad.push(
        "missing (\"weak_lose_unchecked\", \"u3_none\")".to_string());
    }
    for f in &strict_out.findings {
      if f.func == "ok_weak_flow" {
        strict_bad.push(format!("unexpected finding on ok_weak_flow: \
          [{}] {}", f.cat, f.msg));
      }
    }

    let ok = missing.is_empty() && dirty.is_empty() && strict_bad.is_empty();
    if !missing.is_empty() {
      eprintln!("selftest: missing findings: {:?}", missing);
    }
    if !dirty.is_empty() {
      eprintln!("selftest: unexpected findings on: {:?}", dirty);
    }
    if !strict_bad.is_empty() {
      eprintln!("selftest (--strict-weak pass): {}", strict_bad.join("; "));
    }
    eprintln!("selftest: {}", if ok { "PASS" } else { "FAIL" });
    return if ok { 0 } else { 1 };
  }

  if findings.is_empty() { 0 } else { 1 }
}
