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
  prefix_sem, re_file_custom, resolve_sem, ArgumentMode, AssertMode, FileComments, Finding,
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
    if *a == e.file || a == "-xc" || a == "-c" {
      i += 1;
      continue;
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

fn explain(entries: &[Entry], resource_dir: Option<&str>, target: &str) -> i32 {
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
    let tu = match idx.parse(&e.file, &lint_args(e, resource_dir)) {
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
      println!("    {:<12} {:<12} (not a noun: untracked)", pname, tspell);
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
      format!(" ({} is @Refcount: custom file)", relpath(&dfile))
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
  filter: String,
  only: Option<String>,
  function: Option<String>,
  libclang: Option<String>,
  verbose: bool,
  selftest: bool,
  explain: Option<String>,
}

fn parse_args() -> Args {
  let mut a = Args {
    cdb: "compile_commands.json".to_string(),
    filter: "pkg/noun".to_string(),
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
      "--filter" => a.filter = value(&mut i, &argv, "--filter", inline),
      "--only" => a.only = Some(value(&mut i, &argv, "--only", inline)),
      "--function" => a.function = Some(value(&mut i, &argv, "--function", inline)),
      "--libclang" => a.libclang = Some(value(&mut i, &argv, "--libclang", inline)),
      "--explain" => a.explain = Some(value(&mut i, &argv, "--explain", inline)),
      "--verbose" => a.verbose = true,
      "--selftest" => a.selftest = true,
      "-h" | "--help" => {
        println!(
          "usage: refcount-check [--cdb F] [--filter S] [--only S] [--function F] \
          [--libclang PATH] [--verbose] [--selftest] [--explain [FILE:]FUNCTION]"
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

fn process_entry(
  idx: &Index,
  e: &Entry,
  resource_dir: Option<&str>,
  args: &Args,
  sem_cache: &mut SemCache,
  src: &mut SrcCache,
) -> EntryOut {
  let mut out = EntryOut::default();
  let fpath = &e.file;
  let tu = match idx.parse(fpath, &lint_args(e, resource_dir)) {
    Ok(t) => t,
    Err(ex) => {
      out.stderr.push(format!("{}: PARSE FAILED: {}", fpath, ex));
      return out;
    }
  };
  let hard = tu.error_diagnostics();
  if !hard.is_empty() {
    out.stderr
      .push(format!("{}: {} parse errors (first: {})", fpath, hard.len(), hard[0]));
    return out;
  }
  //  a custom file skips BODY checking only: annotation hygiene
  //  (warnings, decl/def sync) still applies to its definitions
  let file_custom = re_file_custom().is_match(&read_head(fpath, 4096));
  if file_custom && args.verbose {
    out.stdout
      .push(format!("-- {}: @Refcount: custom file, bodies skipped", fpath));
  }
  let fcm = FileComments::new(&tu, fpath);
  for cur in tu.cursor().children() {
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
    // only functions that take or return nouns
    let takes = cur
      .arguments()
      .iter()
      .any(|p| p.kind() == clang_sys::CXCursor_ParmDecl && is_noun_type(&p.ty()));
    let rets = is_noun_type(&cur.result_type());
    if !takes && !rets {
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
  let mut seen_files: HashSet<String> = HashSet::new();
  let mut entries: Vec<Entry> = Vec::new();
  for e in cdb {
    if !e.file.contains(&args.filter) || !e.file.ends_with(".c") {
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
    // borrow compile flags from any pkg/noun entry
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

  if let Some(target) = &args.explain {
    return explain(&entries, resource_dir.as_deref(), target);
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
      ("skip_back_goto", "complicated"),
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
    let ok = missing.is_empty() && dirty.is_empty();
    if !missing.is_empty() {
      eprintln!("selftest: missing findings: {:?}", missing);
    }
    if !dirty.is_empty() {
      eprintln!("selftest: unexpected findings on: {:?}", dirty);
    }
    eprintln!("selftest: {}", if ok { "PASS" } else { "FAIL" });
    return if ok { 0 } else { 1 };
  }

  if findings.is_empty() { 0 } else { 1 }
}
