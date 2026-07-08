#!/usr/bin/env python3
"""u3 noun refcount protocol checker.

Statically verifies that functions taking/returning nouns follow the
reference-counting conventions documented in doc/spec/u3.md:

  - transfer semantics: the function consumes one counted reference to
    each noun argument and returns a product the caller owns.
  - retain semantics: the function does not consume argument references
    and returns a product without a counted reference.
  - mixed semantics must be annotated per argument / for the product.

Annotations use the tag `@Refcount:` followed by one clause; the tag may
appear more than once. Keywords are case-insensitive and argument names are
written in `backticks`. A function starts with the default protocol implied
by its name and location (see "Prefix conventions" below); each clause then
updates that protocol, last write winning. A later clause that sets a slot
(the arguments, the product, or a named argument) to a value different from
an earlier clause is reported as a conflict.

Function-level annotations (in the header comment of a declaration or
definition, or in the trailing comment on a declaration's own line):

  @Refcount: custom               -- protocol too complex; not checked, and
                                     call sites treat arguments and product
                                     as unknown. Must be the only annotation.
  @Refcount: custom file          -- in a comment near the top of a file: no
                                     function in the file is checked (for the
                                     runtime core that implements the
                                     refcount machinery itself).
  @Refcount: assert               -- do not check the body; assume whatever
                                     protocol the annotations describe. May
                                     prefix any of the clauses below.
  @Refcount: transfers product    -- the product is owned by the caller, who
                                     must free or consume it.
  @Refcount: transfers arguments  -- the function takes ownership of every
                                     u3_noun argument.
  @Refcount: transfers            -- both (arguments and product).
  @Refcount: transfers `x`, `y`   -- takes ownership of the named arguments.
  @Refcount: retains ...          -- as `transfers`, mutatis mutandis: the
                                     product carries no count, arguments are
                                     borrowed.
  @Refcount: passthrough `x`      -- identity: the product IS argument `x`,
                                     with unchanged ownership (owned stays
                                     owned, borrowed stays borrowed); counts
                                     are untouched.
  @Refcount: direct `x`, `y`      -- if the function returns, the named
                                     arguments were direct atoms (it bails
                                     otherwise); at call sites their
                                     references carry no count, so owned
                                     values passed here cannot leak and
                                     variables are refined to direct.
  @Refcount: direct               -- as above, for every argument.

Block-level annotations (comment immediately after an opening brace):

  {  // @Refcount: assert transfer [x y ...]  -- the block consumes one
                                     reference to each listed variable; with
                                     no names, owned values stored to memory
                                     inside the block are the consumption.
  {  // @Refcount: assert produce x  -- after the block, x holds a counted
                                     reference owned by this function.
  {  // @Refcount: assert retain x   -- the block does not affect x's counts;
                                     suppresses checks on x inside.
  {  // @Refcount: assert construct  -- the block builds a noun by storing
                                     owned references into it (deferred
                                     construction); such stores are intended.

Prefix conventions (defaults, per u3.md):

  u3r_ u3x_        retain args, uncounted (borrowed) product
  u3q* u3w*        retain args, transfer product
  u3k*             transfer args, transfer product
  u3z_             retain args, transfer product
  static or _-named fns in jets/[a-f]/ (u3.md: "a through f, not g"):
                   retain args, transfer product
  everything else  transfer args, transfer product

Requires: pip install clang==19.x, libclang.so 19, and a fresh
compile_commands.json (zig build -Dgenerate-commands after removing
.zig-cache).
"""

import argparse
import glob
import json
import os
import re
import sys

import clang.cindex as ci
from clang.cindex import CursorKind as CK
from clang.cindex import TokenKind

# ---------------------------------------------------------------------------
# configuration

NOUN_TYPES = {
    'u3_noun', 'u3_atom', 'u3_cell', 'u3_weak', 'u3_term',
    'u3_trel', 'u3_qual', 'u3_quin',
}

NORETURN_FNS = {
    'u3m_bail', 'u3m_signal', 'abort', 'exit', '_exit',
    'longjmp', 'siglongjmp', '__assert_fail',
}

GUARD_FNS = {
    'u3a_is_cat': 'cat', 'u3a_is_dog': 'dog', 'u3a_is_pug': 'pug',
    'u3a_is_pom': 'pom', 'u3a_is_atom': 'atom', 'u3a_is_cell': 'cell',
}

# destructurers: (source arg index, retained out-params via &var)
DESTRUCTURERS = {
    'u3x_cell': 0, 'u3x_trel': 0, 'u3x_qual': 0, 'u3x_quil': 0,
    'u3r_cell': 0, 'u3r_trel': 0, 'u3r_qual': 0, 'u3r_quil': 0,
    'u3x_mean': 0, 'u3r_mean': 0,
}

C3Y = 0
C3N = 1
DIRECT_MAX = 0x7fffffff
U3_NONE = 0xffffffff

# ---------------------------------------------------------------------------
# value / environment model

UNINIT, OWNED, BORROWED, CONSUMED, DIRECT, UNKNOWN, CONFLICT, POISONED = (
    'uninit', 'owned', 'borrowed', 'consumed', 'direct', 'unknown',
    'conflict', 'poisoned')


class Val:
    __slots__ = ('state', 'origins', 'temp_id', 'srcs')

    def __init__(self, state, origins=frozenset(), temp_id=None,
                 srcs=frozenset()):
        self.state = state
        self.origins = origins
        self.temp_id = temp_id
        # srcs: which variables this expression value was loaded from
        # (survives ternary merges so consumption can be attributed);
        # never stored into the environment
        self.srcs = srcs

    def key(self):
        return (self.state, self.origins)

    def __repr__(self):
        o = (',' + '/'.join(sorted(self.origins))) if self.origins else ''
        return f'<{self.state}{o}>'


def merge_val(a, b):
    r = _merge_val(a, b)
    r.srcs = a.srcs | b.srcs
    return r


def _merge_val(a, b):
    if a.key() == b.key():
        return Val(a.state, a.origins)
    sa, sb = a.state, b.state
    pair = {sa, sb}
    if UNKNOWN in pair:
        return Val(UNKNOWN)
    if CONFLICT in pair and POISONED not in pair:
        return Val(CONFLICT)  # sticky: a path divergence stays reportable
    if pair == {DIRECT, OWNED}:
        return Val(OWNED)
    if pair == {DIRECT, BORROWED}:
        return Val(BORROWED, a.origins | b.origins)
    if pair == {DIRECT, CONSUMED}:
        return Val(CONSUMED)
    if sa == BORROWED and sb == BORROWED:
        return Val(BORROWED, a.origins | b.origins)
    if pair == {UNINIT, OWNED}:
        return Val(CONFLICT)
    if pair <= {POISONED, CONSUMED, CONFLICT}:
        return Val(POISONED if POISONED in pair else CONFLICT)
    if pair == {OWNED, CONSUMED} or pair == {OWNED, BORROWED}:
        return Val(CONFLICT)
    return Val(UNKNOWN)


def merge_env(envs):
    envs = [e for e in envs if e is not None]
    if not envs:
        return None
    out = dict(envs[0])
    for e in envs[1:]:
        for k, v in e.items():
            if k in out:
                out[k] = merge_val(out[k], v)
            else:
                out[k] = v
        for k in list(out):
            if k not in e:
                pass  # var declared in one branch only; keep as-is
    return out


def env_key(env):
    return tuple(sorted((k, v.key()) for k, v in env.items()))


class Flow:
    """Result of executing a statement: environments that fall through,
    break out, or continue."""

    def __init__(self, falls=None, brks=None, conts=None):
        self.falls = falls if falls is not None else []
        self.brks = brks if brks is not None else []
        self.conts = conts if conts is not None else []


class SkipFunction(Exception):
    def __init__(self, reason):
        self.reason = reason


# ---------------------------------------------------------------------------
# semantics table

TRANSFER, RETAIN = 'transfer', 'retain'


class Sem:
    __slots__ = ('default_args', 'args', 'product', 'check', 'custom',
                 'why', 'direct_args', 'direct_all', 'passthrough',
                 'from_def', 'warnings')

    def __init__(self, default_args=TRANSFER, product=TRANSFER,
                 check=True, custom=False, why='default'):
        self.default_args = default_args
        self.args = {}          # param name -> TRANSFER|RETAIN
        self.product = product  # TRANSFER|RETAIN|None
        self.check = check
        self.custom = custom
        self.why = why
        self.direct_args = set()  # params proven direct if the fn returns
        self.direct_all = False   # every argument proven direct if it returns
        self.passthrough = None   # param whose value IS the product
        self.from_def = False     # resolved with the definition visible
        self.warnings = []        # annotation conflicts, (line, message)

    def arg_mode(self, name):
        return self.args.get(name, self.default_args)

    def is_direct(self, name):
        return self.direct_all or name in self.direct_args


# one clause per @Refcount: tag, running to the end of the comment line
RE_REFCOUNT = re.compile(r'@Refcount:[ \t]*([^\n\r]*)', re.IGNORECASE)
RE_ARGNAME = re.compile(r'`(\w+)`')
RE_TRAIL_COMMENT = re.compile(r'\*/\s*$')
RE_FILE_CUSTOM = re.compile(r'@Refcount:\s*custom\s+file\b', re.IGNORECASE)
RE_JET_DIR = re.compile(r'/jets/[a-f]/')

# words naming the arguments-slot and the product-slot in a clause
ARG_SLOT_WORDS = {'argument', 'arguments', 'arg', 'args'}
PROD_SLOT_WORDS = {'product', 'result', 'return'}


def refcount_clauses(comment):
    """The text of each @Refcount: clause in a comment, in order."""
    out = []
    for m in RE_REFCOUNT.finditer(comment or ''):
        c = RE_TRAIL_COMMENT.sub('', m.group(1)).strip().rstrip('*').strip()
        out.append(c)
    return out


def parse_fn_annotations(comment, sem, line=0):
    """Mutate sem according to the @Refcount: clauses in a comment. Each
    clause updates the protocol (last write wins); a clause that changes a
    slot an earlier clause already set is recorded in sem.warnings. Returns
    True if any clause was recognized."""
    clauses = refcount_clauses(comment)
    if not clauses:
        return False
    explicit = {}  # slot -> mode, for conflict detection

    def warn(msg):
        sem.warnings.append((line, msg))

    def set_slot(slot, mode, apply):
        prev = explicit.get(slot)
        if prev is not None and prev != mode:
            warn(f'conflicting @Refcount: annotations set {slot} to '
                 f'{prev} then {mode}')
        explicit[slot] = mode
        apply()

    saw_custom = False
    saw_other = False
    for clause in clauses:
        toks = clause.lower().split()
        if not toks:
            continue
        head = toks[0]

        if head == 'custom':
            saw_custom = True
            sem.custom = True
            sem.check = False
            if len(toks) >= 2 and toks[1] == 'file':
                sem.why = '@Refcount: custom file'
            else:
                sem.why = '@Refcount: custom'
            continue

        saw_other = True
        if head == 'assert':
            # trust the annotated protocol; do not check the body
            sem.check = False
            sem.why = '@Refcount: assert'
            toks = toks[1:]
            if not toks:
                continue
            head = toks[0]

        if head == 'passthrough':
            names = RE_ARGNAME.findall(clause)
            if not names:
                warn('@Refcount: passthrough requires an argument name')
            else:
                sem.passthrough = names[0]
                sem.why = '@Refcount: passthrough'
            continue

        if head == 'direct':
            names = RE_ARGNAME.findall(clause)
            if names:
                sem.direct_args.update(names)
            else:
                sem.direct_all = True
            sem.why = '@Refcount: direct'
            continue

        if head in ('transfers', 'retains', 'transfer', 'retain'):
            mode = TRANSFER if head.startswith('transfer') else RETAIN
            names = RE_ARGNAME.findall(clause)
            words = set(toks[1:])
            hit_prod = bool(words & PROD_SLOT_WORDS)
            hit_args = bool(words & ARG_SLOT_WORDS)
            if names:
                for n in names:
                    set_slot(f'argument `{n}`', mode,
                             lambda n=n: sem.args.__setitem__(n, mode))
            elif hit_prod and not hit_args:
                set_slot('product', mode,
                         lambda: setattr(sem, 'product', mode))
            elif hit_args and not hit_prod:
                set_slot('arguments', mode,
                         lambda: setattr(sem, 'default_args', mode))
            else:
                # bare, or naming both slots: the whole protocol
                set_slot('product', mode,
                         lambda: setattr(sem, 'product', mode))
                set_slot('arguments', mode,
                         lambda: setattr(sem, 'default_args', mode))
            sem.why = f'@Refcount: {head}'
            continue

        warn(f'unrecognized @Refcount: clause {clause!r}')

    if saw_custom and saw_other:
        warn('@Refcount: custom must be the only annotation')
    return True


RE_JET_QW = re.compile(r'^u3[qw][a-z]+_')
RE_JET_K = re.compile(r'^u3k[a-z]+_')


def prefix_sem(name, file_path, is_static):
    if name.startswith(('u3r_', 'u3x_')):
        return Sem(RETAIN, RETAIN, why='prefix u3r/u3x')
    if name.startswith('u3z_'):
        # memo cache: keys retained, products transferred (u3z_save's
        # own per-arg comment overrides this)
        return Sem(RETAIN, TRANSFER, why='prefix u3z')
    if RE_JET_QW.match(name):
        return Sem(RETAIN, TRANSFER, why='prefix u3q/u3w')
    if RE_JET_K.match(name):
        return Sem(TRANSFER, TRANSFER, why='prefix u3k jets')
    if ((is_static or name.startswith('_'))
            and file_path and RE_JET_DIR.search(file_path)):
        # historical convention (u3.md): jet internals retain
        return Sem(RETAIN, TRANSFER, why='internal fn in jet dir')
    return Sem(TRANSFER, TRANSFER, why='default transfer')


def cursor_comments(cur):
    """All comments plausibly annotating this function cursor."""
    texts = []
    seen = set()
    try:
        defn = cur.get_definition()
    except Exception:
        defn = None
    for c in (cur, cur.canonical, defn):
        if c is None:
            continue
        k = (str(c.location.file), c.location.line)
        if k in seen:
            continue
        seen.add(k)
        rc = c.raw_comment
        if rc and rc not in texts:
            texts.append(rc)
        # trailing comments in the signature (e.g. `u3_noun a)  //  RETAIN`)
        try:
            for tok in c.get_tokens():
                if tok.kind == TokenKind.PUNCTUATION and tok.spelling == '{':
                    break
                if tok.kind == TokenKind.COMMENT:
                    texts.append(tok.spelling)
        except Exception:
            pass
    return '\n'.join(texts)


def resolve_sem(cur, sem_cache):
    """Semantics of the function declared/defined at cursor."""
    name = cur.spelling
    is_static = cur.storage_class == ci.StorageClass.STATIC
    fpath = str(cur.location.file) if cur.location.file else ''
    key = (fpath, name) if is_static else name
    try:
        has_def = cur.get_definition() is not None
    except Exception:
        has_def = False
    cached = sem_cache.get(key)
    if cached is not None and (cached.from_def or not has_def):
        return cached
    # annotations may live only on the definition (.c); a sem cached from
    # a TU that saw just the header declaration must not shadow it
    sem = prefix_sem(name, fpath, is_static)
    line = cur.location.line if cur.location.file else 0
    parse_fn_annotations(cursor_comments(cur), sem, line)
    sem.from_def = has_def
    sem_cache[key] = sem
    return sem


# ---------------------------------------------------------------------------
# helpers over cursors

def unwrap(cur):
    """Strip parens and casts."""
    while cur is not None and cur.kind in (
            CK.PAREN_EXPR, CK.CSTYLE_CAST_EXPR, CK.UNEXPOSED_EXPR):
        kids = list(cur.get_children())
        if not kids:
            return cur
        cur = kids[-1]
    return cur


def is_noun_type(t):
    s = t.spelling.replace('const ', '').strip()
    return s in NOUN_TYPES


_EVAL_READY = False


def _init_eval():
    """Register clang_Cursor_Evaluate, which the python bindings omit."""
    global _EVAL_READY
    import ctypes
    lib = ci.conf.lib
    lib.clang_Cursor_Evaluate.restype = ctypes.c_void_p
    lib.clang_Cursor_Evaluate.argtypes = [ci.Cursor]
    lib.clang_EvalResult_getKind.restype = ctypes.c_int
    lib.clang_EvalResult_getKind.argtypes = [ctypes.c_void_p]
    lib.clang_EvalResult_getAsLongLong.restype = ctypes.c_longlong
    lib.clang_EvalResult_getAsLongLong.argtypes = [ctypes.c_void_p]
    lib.clang_EvalResult_dispose.restype = None
    lib.clang_EvalResult_dispose.argtypes = [ctypes.c_void_p]
    _EVAL_READY = True


def int_literal_value(cur):
    """Value of an integer-constant expression (sees through macros,
    casts, and enum constants), or None."""
    u = unwrap(cur)
    if u is None:
        return None
    if u.kind not in (CK.INTEGER_LITERAL, CK.CHARACTER_LITERAL,
                      CK.BINARY_OPERATOR, CK.UNARY_OPERATOR,
                      CK.DECL_REF_EXPR):
        return None
    if u.kind == CK.DECL_REF_EXPR and u.referenced is not None \
            and u.referenced.kind != CK.ENUM_CONSTANT_DECL:
        return None
    if not _EVAL_READY:
        _init_eval()
    lib = ci.conf.lib
    res = lib.clang_Cursor_Evaluate(cur)
    if not res:
        return None
    try:
        if lib.clang_EvalResult_getKind(res) != 1:  # CXEval_Int
            return None
        return lib.clang_EvalResult_getAsLongLong(res) & 0xffffffff
    finally:
        lib.clang_EvalResult_dispose(res)


def decl_ref_name(cur):
    """Name of a simple lvalue: a variable, or a dot-member chain rooted
    at a variable ('match.call_bat'). Arrow access returns None."""
    cur = unwrap(cur)
    if cur is None:
        return None
    if cur.kind == CK.DECL_REF_EXPR:
        return cur.spelling
    if cur.kind == CK.MEMBER_REF_EXPR:
        kids = list(cur.get_children())
        if not kids:
            return None
        base = kids[0]
        try:
            if base.type.get_canonical().kind == ci.TypeKind.POINTER:
                return None  # p->field: not a tracked path
        except Exception:
            return None
        bname = decl_ref_name(base)
        return f'{bname}.{cur.spelling}' if bname else None
    return None


def is_local_lvalue(cur):
    """True if the expression is (a member chain rooted at) a local
    variable or parameter."""
    cur = unwrap(cur)
    if cur is None:
        return False
    while cur is not None and cur.kind == CK.MEMBER_REF_EXPR:
        kids = list(cur.get_children())
        cur = unwrap(kids[0]) if kids else None
    if cur is None or cur.kind != CK.DECL_REF_EXPR:
        return False
    ref = cur.referenced
    return (ref is not None
            and ref.kind in (CK.VAR_DECL, CK.PARM_DECL)
            and ref.linkage == ci.LinkageKind.NO_LINKAGE)


_UNOP_READY = False
# CXUnaryOperatorKind -> spelling (subset we care about)
_UNOP_MAP = {1: '++', 2: '--', 3: '++', 4: '--', 5: '&', 6: '*',
             7: '+', 8: '-', 9: '~', 10: '!'}


def unary_op(cur):
    """Opcode of a unary operator (works inside macro expansions)."""
    global _UNOP_READY
    import ctypes
    lib = ci.conf.lib
    if not _UNOP_READY:
        lib.clang_getCursorUnaryOperatorKind.restype = ctypes.c_int
        lib.clang_getCursorUnaryOperatorKind.argtypes = [ci.Cursor]
        _UNOP_READY = True
    kind = lib.clang_getCursorUnaryOperatorKind(cur)
    if kind in _UNOP_MAP:
        return _UNOP_MAP[kind]
    toks = list(cur.get_tokens())
    if toks and toks[0].kind == TokenKind.PUNCTUATION:
        return toks[0].spelling
    return None


def binop(cur):
    try:
        op = cur.binary_operator
        return op.name  # 'Assign', 'EQ', 'NE', 'LAnd', 'LOr', 'Comma', ...
    except AttributeError:
        return None


# ---------------------------------------------------------------------------
# block-level ASSERT annotations

RE_BLOCK_ASSERT = re.compile(
    r'@Refcount:\s*assert\s+'
    r'(?:(transfer|produce|retain)((?:\s+[A-Za-z_]\w*)*)'
    r'|(construct)\b)', re.IGNORECASE)


class FileComments:
    def __init__(self, tu, path):
        self.comments = []  # (offset, line, text)
        fh = tu.get_file(path)
        if fh is None:
            return
        try:
            size = os.path.getsize(path)
            extent = tu.get_extent(path, (0, size))
            for tok in tu.get_tokens(extent=extent):
                if tok.kind == TokenKind.COMMENT:
                    self.comments.append(
                        (tok.extent.start.offset, tok.location.line,
                         tok.spelling))
        except Exception:
            pass

    def between(self, lo, hi):
        return [(o, l, t) for (o, l, t) in self.comments if lo < o < hi]


def block_asserts(compound, fcm):
    """ASSERT annotations attached to a CompoundStmt: comments between the
    opening brace and the first child statement."""
    kids = list(compound.get_children())
    lo = compound.extent.start.offset
    hi = kids[0].extent.start.offset if kids else compound.extent.end.offset
    out = []
    for (_, _, text) in fcm.between(lo, hi):
        for m in RE_BLOCK_ASSERT.finditer(text):
            if m.group(3):
                out.append(('CONSTRUCT', []))
            else:
                out.append((m.group(1).upper(), m.group(2).split()))
    return out


# ---------------------------------------------------------------------------
# the abstract interpreter

class FnChecker:
    def __init__(self, tool, fn_cursor, sem, fcm):
        self.tool = tool
        self.fn = fn_cursor
        self.name = fn_cursor.spelling
        self.sem = sem
        self.fcm = fcm
        self.findings = []
        self.exit_envs = []      # (env, loc, returned_root)
        self.open_temps = []     # owned temporaries pending consumption
        self.frozen = set()      # vars under an ASSERT block
        self.assert_depth = 0    # nesting depth of ASSERT blocks
        self.reported = set()    # dedup (line, cat, var)
        self.param_modes = {}    # name -> TRANSFER|RETAIN
        self.noun_params = []

    # -- reporting

    def report(self, cur, cat, msg, var=''):
        loc = cur.location if cur is not None else self.fn.location
        key = (loc.line, cat, var)
        if key in self.reported:
            return
        self.reported.add(key)
        self.findings.append((str(loc.file), loc.line, loc.column,
                              self.name, cat, msg))

    # -- entry

    def run(self):
        body = None
        for c in self.fn.get_children():
            if c.kind == CK.COMPOUND_STMT:
                body = c
        if body is None:
            return []
        env = {}
        for p in self.fn.get_arguments():
            if p.kind != CK.PARM_DECL or not p.spelling:
                continue
            if is_noun_type(p.type):
                mode = self.sem.arg_mode(p.spelling)
                self.param_modes[p.spelling] = mode
                self.noun_params.append(p.spelling)
                env[p.spelling] = (Val(OWNED) if mode == TRANSFER
                                   else Val(BORROWED,
                                            frozenset([p.spelling])))
        try:
            flow = self.exec_stmt(body, [env])
            for e in flow.falls:
                self.check_exit(e, body, returned_root=None,
                                loc_cur=None)
        except SkipFunction as sk:
            self.report(self.fn, 'skipped',
                        f'not analyzed ({sk.reason}); '
                        f'annotate with @Refcount: custom or @Refcount: assert')
        return self.findings

    # -- exit checks

    def check_exit(self, env, cur, returned_root, loc_cur):
        where = loc_cur if loc_cur is not None else cur
        for p in self.noun_params:
            mode = self.param_modes[p]
            v = env.get(p, Val(UNKNOWN))
            if p in self.frozen:
                continue
            if mode == RETAIN:
                # a retained arg variable may be reused as a cursor, but
                # an owned reference parked in it dies with the frame
                if p == returned_root:
                    continue
                if v.state == OWNED:
                    self.report(where, 'leak',
                                f'owned reference left in retained '
                                f'argument variable [{p}] on this path', p)
                elif v.state == CONFLICT:
                    self.report(where, 'conflict',
                                f'retained argument variable [{p}] holds '
                                f'an owned reference on some paths', p)
                continue
            if mode == TRANSFER:
                if p == returned_root:
                    continue
                if v.state == OWNED:
                    self.report(where, 'leak',
                                f'transferred argument [{p}] not consumed '
                                f'on this path', p)
                elif v.state == CONFLICT:
                    self.report(where, 'conflict',
                                f'argument [{p}] consumed on some paths '
                                f'but not others', p)
        for name, v in env.items():
            if name in self.param_modes or name in self.frozen:
                continue
            if name == returned_root:
                continue
            if v.state == OWNED:
                self.report(where, 'leak',
                            f'owned local [{name}] not consumed on this '
                            f'path', name)
            elif v.state == CONFLICT:
                self.report(where, 'conflict',
                            f'local [{name}] consumed on some paths but '
                            f'not others', name)

    # -- consumption / liveness

    def poison_derived(self, env, root):
        for k, v in env.items():
            if v.state == BORROWED and root in v.origins:
                env[k] = Val(POISONED)

    def use_check(self, cur, name, v):
        if name in self.frozen:
            return
        if v.state == POISONED:
            self.report(cur, 'use-after-free',
                        f'[{name}] is derived from a noun already '
                        f'consumed on this path', name)
        elif v.state == CONSUMED:
            self.report(cur, 'use-after-free',
                        f'[{name}] used after its reference was '
                        f'consumed', name)

    def consume(self, cur, env, val, what):
        """A counted reference to `val` is given away here."""
        if val.temp_id is not None:
            try:
                self.open_temps.remove(val.temp_id)
            except ValueError:
                pass
        name = what if isinstance(what, str) else None
        if name is None:
            # attribute by value provenance (e.g. the value came out of
            # a ternary over variables)
            srcs = [s for s in val.srcs if s in env]
            if len(srcs) == 1:
                name = srcs[0]
            elif len(srcs) > 1:
                # one of several variables was consumed; we cannot tell
                # which, so stop tracking all of them
                for s in srcs:
                    if s not in self.frozen:
                        env[s] = Val(UNKNOWN)
                return
        if name and name in self.frozen:
            return
        st = val.state
        if st in (DIRECT, UNKNOWN, UNINIT):
            pass
        elif st == OWNED:
            if name:
                env[name] = Val(CONSUMED)
                self.poison_derived(env, name)
        elif st == BORROWED:
            self.report(cur, 'over-free',
                        f'counted reference to retained/borrowed value '
                        f'[{name or "?"}] given away '
                        f'(origins: {", ".join(sorted(val.origins)) or "?"})',
                        name or '')
            if name:
                env[name] = Val(CONSUMED)
                for root in val.origins:
                    self.poison_derived(env, root)
        elif st == CONSUMED or st == POISONED:
            self.report(cur, 'double-free',
                        f'reference to [{name or "?"}] consumed twice on '
                        f'this path', name or '')
        elif st == CONFLICT:
            self.report(cur, 'conflict',
                        f'[{name or "?"}] consumed here but its ownership '
                        f'differs between paths', name or '')

    # -- expression evaluation

    def new_temp(self):
        tid = object()
        self.open_temps.append(tid)
        return tid

    def eval_expr(self, cur, env):
        if cur is None:
            return Val(UNKNOWN)
        cur0 = cur
        cur = unwrap(cur)
        if cur is None:
            return Val(UNKNOWN)
        k = cur.kind

        if k == CK.INTEGER_LITERAL or k == CK.CHARACTER_LITERAL:
            return Val(DIRECT)
        if k == CK.STRING_LITERAL:
            return Val(UNKNOWN)
        if k == CK.DECL_REF_EXPR:
            name = cur.spelling
            if name in env:
                v = env[name]
                self.use_check(cur, name, v)
                return Val(v.state, v.origins, srcs=frozenset([name]))
            return Val(DIRECT)  # enum constants, globals treated as opaque
        if k == CK.CALL_EXPR:
            return self.eval_call(cur, env)
        if k == CK.CONDITIONAL_OPERATOR:
            kids = list(cur.get_children())
            if len(kids) == 3:
                t_envs, f_envs = self.eval_cond(kids[0], env)
                vals = []
                for e in t_envs:
                    vals.append(self.eval_expr(kids[1], e))
                for e in f_envs:
                    vals.append(self.eval_expr(kids[2], e))
                merged = merge_env(t_envs + f_envs)
                env.clear()
                env.update(merged)
                out = vals[0]
                for v in vals[1:]:
                    out = merge_val(out, v)
                return out
            return Val(UNKNOWN)
        if k == CK.BINARY_OPERATOR:
            op = binop(cur)
            kids = list(cur.get_children())
            if len(kids) != 2:
                return Val(UNKNOWN)
            lhs, rhs = kids
            if op == 'Assign':
                return self.eval_assign(cur, lhs, rhs, env)
            if op == 'Comma':
                self.eval_stmt_expr_result(lhs, env)
                return self.eval_expr(rhs, env)
            self.eval_expr(lhs, env)
            self.eval_expr(rhs, env)
            return Val(DIRECT)  # arithmetic/comparison: not a counted noun
        if k == CK.COMPOUND_ASSIGNMENT_OPERATOR:
            for c in cur.get_children():
                self.eval_expr(c, env)
            return Val(UNKNOWN)
        if k == CK.UNARY_OPERATOR:
            op = unary_op(cur)
            kids = list(cur.get_children())
            child = kids[0] if kids else None
            if op == '&':
                name = decl_ref_name(child)
                if name and name in env:
                    env[name] = Val(UNKNOWN)  # escapes; e.g. out-param
                return Val(UNKNOWN)
            if child is not None:
                self.eval_expr(child, env)
            return Val(UNKNOWN) if op == '*' else Val(DIRECT)
        if k == CK.MEMBER_REF_EXPR:
            name = decl_ref_name(cur)
            if name is not None and name in env:
                v = env[name]
                self.use_check(cur, name, v)
                return Val(v.state, v.origins, srcs=frozenset([name]))
            for c in cur.get_children():
                self.eval_expr(c, env)
            return Val(UNKNOWN)
        if k == CK.ARRAY_SUBSCRIPT_EXPR:
            for c in cur.get_children():
                self.eval_expr(c, env)
            return Val(UNKNOWN)
        if k == CK.COMPOUND_STMT:
            flow = self.exec_stmt(cur, [env])
            merged = merge_env(flow.falls) or env
            env.clear()
            env.update(merged)
            return Val(UNKNOWN)
        if k == CK.INIT_LIST_EXPR:
            for c in cur.get_children():
                v = self.eval_expr(c, env)
                self.consume(cur, env, v, decl_ref_name(c))
            return Val(UNKNOWN)
        # default: recurse
        for c in cur.get_children():
            self.eval_expr(c, env)
        return Val(UNKNOWN)

    def eval_assign(self, cur, lhs, rhs, env):
        rv = self.eval_expr(rhs, env)
        rname = decl_ref_name(rhs)
        lname = decl_ref_name(lhs)
        if (lname is not None and lname not in env
                and is_local_lvalue(lhs)
                and rv.state in (OWNED, BORROWED, CONSUMED, POISONED)):
            # lazily track locals the declaration pass missed: noun values
            # held in c3_w variables or struct members of local structs
            env[lname] = Val(UNINIT)
        if lname is not None and lname in env:
            old = env[lname]
            if old.state == OWNED and lname not in self.frozen:
                self.report(cur, 'leak',
                            f'owned reference in [{lname}] overwritten '
                            f'without being consumed', lname)
            if rv.temp_id is not None:
                try:
                    self.open_temps.remove(rv.temp_id)
                except ValueError:
                    pass
            env[lname] = Val(rv.state, rv.origins)
            # x = y moves ownership: y becomes an alias borrowed from x
            if (rname and rname != lname and rname in env
                    and env[rname].state == OWNED):
                env[rname] = Val(BORROWED, frozenset([lname]))
            return env[lname]
        # store through pointer / into struct or array
        lhs_u = unwrap(lhs)
        if rv.state == OWNED:
            if self.is_param_deref(lhs_u) or self.assert_depth > 0:
                # *out = product (ownership passes to the caller), or a
                # store blessed by an enclosing ASSERT block
                self.consume(cur, env, rv, rname)
            elif (rname or '') in self.frozen:
                pass
            else:
                self.report(cur, 'escape',
                            f'owned reference stored to memory; wrap in an '
                            f'"@Refcount: assert transfer" block if this store '
                            f'is the intended consumption', rname or '')
                self.consume(cur, env, rv, rname)
        else:
            self.eval_expr(lhs, env)
        return Val(rv.state, rv.origins)

    def is_param_deref(self, lhs):
        """True for `*p = ...` where p is a pointer parameter."""
        if lhs is None or lhs.kind != CK.UNARY_OPERATOR:
            return False
        if unary_op(lhs) != '*':
            return False
        kids = list(lhs.get_children())
        name = decl_ref_name(kids[0]) if kids else None
        if name is None:
            return False
        for p in self.fn.get_arguments():
            if p.spelling == name:
                return True
        return False

    def bind_init_list(self, d, init, env):
        """Struct initializer: bind each element to a member path of the
        declared variable, moving ownership out of source variables
        (`match_data_struct match = { call_bat, ... };` -- the struct
        takes over; the fields are freed as match.call_bat later)."""
        elems = list(init.get_children())
        try:
            fields = list(d.type.get_canonical().get_fields())
        except Exception:
            fields = []
        if not fields or len(elems) > len(fields):
            # array or unsupported shape: evaluate and consume elements
            self.eval_expr(init, env)
            return
        for f, e in zip(fields, elems):
            v = self.eval_expr(e, env)
            if v.state not in (OWNED, BORROWED, CONSUMED, POISONED,
                               DIRECT):
                continue
            key = f'{d.spelling}.{f.spelling}'
            if v.temp_id is not None:
                try:
                    self.open_temps.remove(v.temp_id)
                except ValueError:
                    pass
            env[key] = Val(v.state, v.origins)
            ename = decl_ref_name(e)
            if (ename and ename != key and ename in env
                    and env[ename].state == OWNED):
                env[ename] = Val(BORROWED, frozenset([key]))

    # -- calls

    def eval_call(self, cur, env):
        callee = cur.referenced
        cname = callee.spelling if callee is not None else None
        args = list(cur.get_arguments())
        if not args:
            kids = list(cur.get_children())
            args = kids[1:]

        if cname in NORETURN_FNS:
            for a in args:
                self.eval_expr(a, env)
            raise PathEnd()

        if cname == 'u3a_lose':
            if args:
                v = self.eval_expr(args[0], env)
                self.consume(cur, env, v, decl_ref_name(args[0]))
            return Val(DIRECT)
        if cname in ('u3a_gain', 'u3a_take'):
            if args:
                self.eval_expr(args[0], env)
            return Val(OWNED, temp_id=self.new_temp())
        if cname in ('u3a_h', 'u3a_t'):
            if args:
                v = self.eval_expr(args[0], env)
                name = decl_ref_name(args[0])
                if v.state == BORROWED:
                    return Val(BORROWED, v.origins)
                if v.state in (OWNED, UNKNOWN, DIRECT) and name:
                    return Val(BORROWED, frozenset([name]))
                return Val(BORROWED, v.origins or
                           (frozenset([name]) if name else frozenset()))
            return Val(UNKNOWN)
        if cname in GUARD_FNS:
            if args:
                self.eval_expr(args[0], env)
            return Val(DIRECT)
        if cname in DESTRUCTURERS:
            src_i = DESTRUCTURERS[cname]
            src_name = None
            for i, a in enumerate(args):
                if i == src_i:
                    self.eval_expr(a, env)
                    src_name = decl_ref_name(a)
                else:
                    au = unwrap(a)
                    if (au is not None and au.kind == CK.UNARY_OPERATOR
                            and unary_op(au) == '&'):
                        kids = list(au.get_children())
                        nm = decl_ref_name(kids[0]) if kids else None
                        if nm is not None:
                            env[nm] = Val(BORROWED,
                                          frozenset([src_name])
                                          if src_name else frozenset())
                            continue
                    self.eval_expr(a, env)
            return Val(DIRECT)

        # generic call
        sem = None
        params = []
        if callee is not None and cname:
            sem = resolve_sem(callee, self.tool.sem_cache)
            pcur = callee
            try:
                defn = callee.get_definition()
                if defn is not None:
                    pcur = defn  # forward decls may have unnamed params
            except Exception:
                pass
            params = list(pcur.get_arguments())

        # phase 1: evaluate all argument expressions (C evaluates every
        # operand before the call; consumption happens inside the callee)
        evald = []
        for i, a in enumerate(args):
            p = params[i] if i < len(params) else None
            v = self.eval_expr(a, env)
            evald.append((a, p, v))
        # phase 2: apply the callee's per-argument effects
        pass_val = None
        for a, p, v in evald:
            p_noun = p is not None and is_noun_type(p.type)
            aname = decl_ref_name(a)
            if (sem is not None and p is not None
                    and sem.passthrough == p.spelling):
                # identity: this argument's value IS the product;
                # counts are untouched
                pass_val = v
                continue
            if not p_noun:
                # passing &var to unknown pointer param already handled in
                # eval_expr('&'); nothing else to do
                continue
            if sem is None or sem.custom:
                if aname and aname in env:
                    env[aname] = Val(UNKNOWN)
                continue
            pname = p.spelling if p is not None else ''
            if sem.is_direct(pname):
                # the callee bails unless this argument is a direct atom;
                # on return its reference carries no count
                if v.temp_id is not None:
                    try:
                        self.open_temps.remove(v.temp_id)
                    except ValueError:
                        pass
                if aname and aname in env \
                        and env[aname].state in (OWNED, BORROWED, UNKNOWN):
                    env[aname] = Val(DIRECT)
                continue
            mode = sem.arg_mode(pname)
            if mode == TRANSFER:
                self.consume(cur, env, v, aname)
            else:
                # retained: liveness only; owned temporaries leak
                if v.temp_id is not None and v.state == OWNED:
                    self.report(cur, 'leak',
                                f'owned product passed to retaining '
                                f'parameter of {cname}(); reference is '
                                f'leaked', aname or '')
                    try:
                        self.open_temps.remove(v.temp_id)
                    except ValueError:
                        pass

        if pass_val is not None:
            return pass_val
        rt = callee.result_type if callee is not None else None
        if rt is not None and is_noun_type(rt):
            if sem is None or sem.custom:
                return Val(UNKNOWN)
            if sem.product == TRANSFER:
                return Val(OWNED, temp_id=self.new_temp())
            roots = frozenset()
            for i, a in enumerate(args):
                nm = decl_ref_name(a)
                if nm and nm in env and env[nm].state in (OWNED, BORROWED):
                    roots |= env[nm].origins or frozenset([nm])
            return Val(BORROWED, roots)
        return Val(UNKNOWN)

    # -- conditions

    def eval_cond(self, cur, env):
        """Returns (true_envs, false_envs)."""
        cur = unwrap(cur)
        if cur is None:
            return ([env], [dict(env)])
        lit = int_literal_value(cur)
        if lit is not None:
            # constant condition: while(1) never falls out, etc.
            return ([env], []) if lit else ([], [env])
        k = cur.kind
        # see through __builtin_expect (c3_likely/c3_unlikely)
        if (k == CK.CALL_EXPR and cur.referenced is not None
                and cur.referenced.spelling == '__builtin_expect'):
            args = list(cur.get_arguments())
            if args:
                return self.eval_cond(args[0], env)
        if k == CK.UNARY_OPERATOR and unary_op(cur) == '!':
            kids = list(cur.get_children())
            t, f = self.eval_cond(kids[0], env)
            return (f, t)
        if k == CK.BINARY_OPERATOR:
            op = binop(cur)
            kids = list(cur.get_children())
            if op == 'LAnd' and len(kids) == 2:
                lt, lf = self.eval_cond(kids[0], env)
                tt, tf = [], []
                for e in lt:
                    t2, f2 = self.eval_cond(kids[1], e)
                    tt += t2
                    tf += f2
                return (tt, lf + tf)
            if op == 'LOr' and len(kids) == 2:
                lt, lf = self.eval_cond(kids[0], env)
                ft, ff = [], []
                for e in lf:
                    t2, f2 = self.eval_cond(kids[1], e)
                    ft += t2
                    ff += f2
                return (lt + ft, ff)
            if op in ('EQ', 'NE') and len(kids) == 2:
                fact = self.guard_fact(kids[0], kids[1], env)
                self.eval_expr(kids[0], env)
                self.eval_expr(kids[1], env)
                te, fe = dict(env), dict(env)
                if fact is not None:
                    name, refine = fact
                    if op == 'EQ':
                        if refine is not None:
                            te[name] = refine
                    else:
                        if refine is not None:
                            fe[name] = refine
                return ([te], [fe])
            if op in ('LT', 'GT', 'LE', 'GE') and len(kids) == 2:
                fact = self.bound_fact(op, kids[0], kids[1], env)
                self.eval_expr(kids[0], env)
                self.eval_expr(kids[1], env)
                te, fe = dict(env), dict(env)
                if fact is not None:
                    name, on_true = fact
                    (te if on_true else fe)[name] = Val(DIRECT)
                return ([te], [fe])
        # bare truthiness of a noun variable: false branch implies 0,
        # which is a direct atom with no counted references
        if k == CK.DECL_REF_EXPR and cur.spelling in env:
            name = cur.spelling
            self.eval_expr(cur, env)
            te, fe = dict(env), dict(env)
            if fe[name].state in (OWNED, BORROWED, UNKNOWN, UNINIT):
                fe[name] = Val(DIRECT)
            return ([te], [fe])
        # generic condition
        self.eval_expr(cur, env)
        return ([dict(env)], [dict(env)])

    def bound_fact(self, op, a, b, env):
        """For a relational comparison, return (var, branch) where the
        variable is provably a direct atom (bounded below 2^31) on that
        branch (True = comparison-true branch), or None."""
        name_a, lit_b = decl_ref_name(a), int_literal_value(b)
        lit_a, name_b = int_literal_value(a), decl_ref_name(b)
        if name_a and lit_b is not None:
            name, lit = name_a, lit_b
            # var < lit (true), var <= lit (true),
            # var > lit (false), var >= lit (false)
            on_true = op in ('LT', 'LE')
            bound_incl = op in ('LE', 'GT')       # bound is <= lit
        elif name_b and lit_a is not None:
            name, lit = name_b, lit_a
            # lit > var (true), lit >= var (true),
            # lit < var (false), lit <= var (false)
            on_true = op in ('GT', 'GE')
            bound_incl = op in ('GE', 'LT')
        else:
            return None
        limit = DIRECT_MAX if bound_incl else DIRECT_MAX + 1
        if lit > limit:
            return None
        if name not in env or env[name].state not in (OWNED, BORROWED,
                                                      UNKNOWN, UNINIT):
            return None
        return (name, on_true)

    def guard_fact(self, a, b, env):
        """For a comparison a==b, return (var, refined Val for the equal
        branch) or None."""
        for x, y in ((a, b), (b, a)):
            lit = int_literal_value(x)
            name = decl_ref_name(y)
            if name is None:
                # look through an assignment: (name = expr)
                yu = unwrap(y)
                if (yu is not None and yu.kind == CK.BINARY_OPERATOR
                        and binop(yu) == 'Assign'):
                    name = decl_ref_name(list(yu.get_children())[0])
            if lit is not None and name and name in env:
                if lit <= DIRECT_MAX or lit == U3_NONE:
                    old = env[name]
                    if old.state in (OWNED, BORROWED, UNKNOWN, UNINIT):
                        return (name, Val(DIRECT))
                return None
            # c3y/c3n == u3a_is_*(var)
            yc = unwrap(y)
            if (lit is not None and yc is not None
                    and yc.kind == CK.CALL_EXPR
                    and yc.referenced is not None
                    and yc.referenced.spelling in GUARD_FNS):
                gargs = list(yc.get_arguments())
                gname = decl_ref_name(gargs[0]) if gargs else None
                if gname and gname in env:
                    kind = GUARD_FNS[yc.referenced.spelling]
                    truth = (lit == C3Y)
                    old = env[gname]
                    if old.state not in (OWNED, BORROWED, UNKNOWN):
                        return None
                    # equal-branch refinement
                    if kind == 'cat' and truth:
                        return (gname, Val(DIRECT))
                    if kind == 'dog' and not truth:
                        return (gname, Val(DIRECT))
                return None
        return None

    # -- statement-level result handling

    def eval_stmt_expr_result(self, cur, env):
        # bare `u3k(x);` increments x's count in place: x becomes owned
        u = unwrap(cur)
        if (u is not None and u.kind == CK.CALL_EXPR
                and u.referenced is not None
                and u.referenced.spelling in ('u3a_gain', 'u3a_take')):
            gargs = list(u.get_arguments())
            gname = decl_ref_name(gargs[0]) if gargs else None
            if gname and gname in env:
                v = env[gname]
                self.use_check(u, gname, v)
                if v.state in (BORROWED, UNKNOWN):
                    env[gname] = Val(OWNED)
                return
        v = self.eval_expr(cur, env)
        if v.temp_id is not None and v.state == OWNED:
            self.report(cur, 'leak',
                        'owned product of call discarded '
                        '(product of a transferring function must be '
                        'consumed)')
            try:
                self.open_temps.remove(v.temp_id)
            except ValueError:
                pass

    # -- statements

    def exec_stmt(self, cur, envs):
        """Execute statement over each env; returns Flow."""
        out = Flow()
        for env in envs:
            try:
                f = self.exec_one(cur, env)
                out.falls += f.falls
                out.brks += f.brks
                out.conts += f.conts
            except PathEnd:
                pass
        return out

    def exec_one(self, cur, env):
        k = cur.kind

        if k == CK.COMPOUND_STMT:
            asserts = block_asserts(cur, self.fcm)
            frozen_here = []
            snapshots = {}
            for mode, names in asserts:
                for n in names:
                    if n not in self.frozen:
                        frozen_here.append(n)
                        self.frozen.add(n)
                    snapshots[n] = env.get(n)
            if asserts:
                self.assert_depth += 1
            flow = Flow(falls=[env])
            for child in cur.get_children():
                if not flow.falls:
                    break
                nxt = self.exec_stmt(child, flow.falls)
                flow = Flow(nxt.falls, flow.brks + nxt.brks,
                            flow.conts + nxt.conts)
            if asserts:
                self.assert_depth -= 1
            for n in frozen_here:
                self.frozen.discard(n)
            for mode, names in asserts:
                for n in names:
                    for e in flow.falls:
                        if mode == 'TRANSFER':
                            e[n] = Val(CONSUMED)
                        elif mode == 'PRODUCE':
                            e[n] = Val(OWNED)
                        elif mode == 'RETAIN':
                            if snapshots.get(n) is not None:
                                e[n] = snapshots[n]
            return flow

        if k == CK.DECL_STMT:
            for d in cur.get_children():
                if d.kind != CK.VAR_DECL:
                    continue
                init = None
                for c in d.get_children():
                    init = c  # last child is the initializer if present
                if init is not None and init.kind == CK.INIT_LIST_EXPR:
                    self.bind_init_list(d, init, env)
                elif init is not None and init.kind != CK.TYPE_REF:
                    v = self.eval_expr(init, env)
                    if is_noun_type(d.type):
                        if v.temp_id is not None:
                            try:
                                self.open_temps.remove(v.temp_id)
                            except ValueError:
                                pass
                        env[d.spelling] = Val(v.state, v.origins)
                        # `u3_noun cur = owned;` declares a borrowing
                        # cursor: ownership stays with the source (unlike
                        # assignment to an existing var, which moves it)
                        iname = decl_ref_name(init)
                        if (iname and iname != d.spelling and iname in env
                                and env[iname].state == OWNED):
                            env[d.spelling] = Val(BORROWED,
                                                  frozenset([iname]))
                elif is_noun_type(d.type):
                    env[d.spelling] = Val(UNINIT)
            return Flow(falls=[env])

        if k == CK.IF_STMT:
            kids = list(cur.get_children())
            cond = kids[0] if kids else None
            then = kids[1] if len(kids) > 1 else None
            els = kids[2] if len(kids) > 2 else None
            t_envs, f_envs = self.eval_cond(cond, env)
            flow = Flow()
            if then is not None:
                ft = self.exec_stmt(then, t_envs)
            else:
                ft = Flow(falls=t_envs)
            if els is not None:
                fe = self.exec_stmt(els, f_envs)
            else:
                fe = Flow(falls=f_envs)
            m = merge_env(ft.falls + fe.falls)
            flow.falls = [m] if m is not None else []
            flow.brks = ft.brks + fe.brks
            flow.conts = ft.conts + fe.conts
            return flow

        if k in (CK.WHILE_STMT, CK.FOR_STMT, CK.DO_STMT):
            return self.exec_loop(cur, env)

        if k == CK.SWITCH_STMT:
            kids = list(cur.get_children())
            body = kids[-1] if kids else None
            if kids:
                self.eval_expr(kids[0], env)
            if body is None:
                return Flow(falls=[env])
            f = self.exec_stmt(body, [dict(env)])
            # cases without break fall out; the switch may also match
            # nothing -- unless it has a default case
            def has_default(c):
                if c.kind == CK.DEFAULT_STMT:
                    return True
                if c.kind == CK.SWITCH_STMT:
                    return False  # a nested switch's default is its own
                return any(has_default(ch) for ch in c.get_children())
            skip = [] if any(has_default(ch) for ch in body.get_children()) \
                else [env]
            m = merge_env(f.falls + f.brks + skip)
            return Flow(falls=[m] if m is not None else [],
                        conts=f.conts)

        if k in (CK.CASE_STMT, CK.DEFAULT_STMT):
            kids = list(cur.get_children())
            sub = kids[-1] if kids else None
            if sub is not None and sub.kind not in (CK.INTEGER_LITERAL,):
                return self.exec_stmt(sub, [env])
            return Flow(falls=[env])

        if k == CK.RETURN_STMT:
            kids = list(cur.get_children())
            root = None
            if kids:
                v = self.eval_expr(kids[0], env)
                root = decl_ref_name(kids[0])
                self.check_return_val(cur, v, root, env)
                if v.temp_id is not None:
                    try:
                        self.open_temps.remove(v.temp_id)
                    except ValueError:
                        pass
            self.check_exit(env, cur, returned_root=root, loc_cur=cur)
            raise PathEnd()

        if k == CK.BREAK_STMT:
            return Flow(brks=[env])
        if k == CK.CONTINUE_STMT:
            return Flow(conts=[env])
        if k in (CK.GOTO_STMT, CK.INDIRECT_GOTO_STMT, CK.LABEL_STMT):
            raise SkipFunction('uses goto/labels')
        if k == CK.NULL_STMT:
            return Flow(falls=[env])
        if k == CK.ASM_STMT:
            return Flow(falls=[env])

        # expression statement or anything else expression-like
        if k in (CK.CALL_EXPR, CK.BINARY_OPERATOR, CK.UNARY_OPERATOR,
                 CK.CONDITIONAL_OPERATOR, CK.COMPOUND_ASSIGNMENT_OPERATOR,
                 CK.CSTYLE_CAST_EXPR, CK.PAREN_EXPR, CK.UNEXPOSED_EXPR,
                 CK.DECL_REF_EXPR, CK.MEMBER_REF_EXPR,
                 CK.ARRAY_SUBSCRIPT_EXPR, CK.INTEGER_LITERAL):
            self.eval_stmt_expr_result(cur, env)
            return Flow(falls=[env])

        # unknown statement kind: recurse conservatively
        flow = Flow(falls=[env])
        for child in cur.get_children():
            if not flow.falls:
                break
            nxt = self.exec_stmt(child, flow.falls)
            flow = Flow(nxt.falls, flow.brks + nxt.brks,
                        flow.conts + nxt.conts)
        return flow

    def exec_loop(self, cur, env):
        k = cur.kind
        kids = list(cur.get_children())
        cond = body = init = inc = None
        if k == CK.WHILE_STMT:
            cond, body = kids[0], kids[-1]
        elif k == CK.DO_STMT:
            body, cond = kids[0], kids[-1]
        else:  # FOR_STMT: children vary; last is body
            body = kids[-1]
            rest = kids[:-1]
            # heuristics: first non-expression child is init (DECL_STMT),
            # condition is a comparison-like expr, inc is the rest
            for c in rest:
                if c.kind == CK.DECL_STMT and init is None:
                    init = c
                elif cond is None:
                    cond = c
                else:
                    inc = c
            # note: for-loop part identification is approximate; over-
            # approximating by treating extra parts as plain expressions
        if init is not None:
            f = self.exec_stmt(init, [env])
            envs = f.falls
        else:
            envs = [env]
        head = merge_env(envs)
        if head is None:
            return Flow()
        exits = []
        if k == CK.DO_STMT:
            # run body once unconditionally first; the condition sees only
            # the post-body state (the entry state never reaches it, so a
            # do/while(0) preserves refinements made inside the body)
            f = self.exec_stmt(body, [dict(head)])
            exits += f.brks
            back = merge_env(f.falls + f.conts)
            if back is None:
                # body never falls through to the condition
                m = merge_env(exits)
                return Flow(falls=[m] if m is not None else [])
            head = back
        for _ in range(8):
            if cond is not None and cond.kind not in (CK.COMPOUND_STMT,):
                t_envs, f_envs = self.eval_cond(cond, dict(head))
            else:
                t_envs, f_envs = [dict(head)], []
            f = self.exec_stmt(body, t_envs)
            if inc is not None:
                after = self.exec_stmt(inc, f.falls + f.conts)
                back_envs = after.falls
            else:
                back_envs = f.falls + f.conts
            exits += f.brks
            back = merge_env(back_envs)
            new_head = head if back is None else merge_env([head, back])
            exits_now = f_envs
            if new_head is not None and env_key(new_head) == env_key(head):
                return Flow(falls=[m for m in
                                   [merge_env(exits_now + exits)]
                                   if m is not None])
            head = new_head if new_head is not None else head
        # did not stabilize; be conservative
        m = merge_env([head] + exits)
        return Flow(falls=[m] if m is not None else [])

    def check_return_val(self, cur, v, root, env):
        if self.sem.product == TRANSFER:
            if v.state == BORROWED and root not in self.frozen:
                self.report(cur, 'return-borrowed',
                            f'transfer-product function returns an '
                            f'uncounted (retained) reference'
                            + (f' derived from '
                               f'[{", ".join(sorted(v.origins))}]'
                               if v.origins else ''))
            elif v.state in (CONSUMED, POISONED):
                self.report(cur, 'use-after-free',
                            'returning a value whose reference was '
                            'already consumed')
        elif self.sem.product == RETAIN:
            if v.state == OWNED:
                self.report(cur, 'leak',
                            'retain-product function returns a counted '
                            'reference (caller will not free it)')


class PathEnd(Exception):
    pass


# ---------------------------------------------------------------------------
# driver

def load_cdb(path):
    with open(path) as f:
        return json.load(f)


def lint_args(entry, resource_dir):
    args = entry['arguments'][1:]
    zig_root = os.path.dirname(entry['arguments'][0])
    out, i = [], 0
    skip2 = {'-o', '--serialize-diagnostics', '-gen-cdb-fragment-path',
             '--param', '-MF', '-MD'}
    while i < len(args):
        a = args[i]
        if a in skip2:
            i += 2
            continue
        if a == entry['file'] or a in ('-xc', '-c'):
            i += 1
            continue
        if a == '-isystem' and i + 1 < len(args):
            path = args[i + 1]
            if path.startswith(zig_root) and path.endswith('/lib/include'):
                out += ['-isystem', resource_dir]
                i += 2
                continue
        out.append(a)
        i += 1
    return ['-xc', '-DU3_REFCOUNT_LINT', '-fparse-all-comments',
            '-ferror-limit=0'] + out


def find_resource_dir(libclang_path):
    if libclang_path:
        root = os.path.dirname(os.path.dirname(libclang_path))
        cands = sorted(glob.glob(os.path.join(root, 'lib/clang/*/include')))
        if cands:
            return cands[-1]
    cands = sorted(glob.glob('/usr/lib/llvm-*/lib/clang/*/include'),
                   reverse=True)
    return cands[0] if cands else None


def find_libclang():
    cands = sorted(glob.glob('/usr/lib/llvm-*/lib/libclang.so*'),
                   reverse=True)
    for c in cands:
        return c
    return None


class Tool:
    def __init__(self):
        self.sem_cache = {}
        self.findings = []
        self.skipped = []


def explain(entries, resource_dir, target):
    """Print the resolved refcount protocol for one function and why."""
    fpart = None
    fname = target
    if ':' in target:
        fpart, fname = target.rsplit(':', 1)
    idx = ci.Index.create()
    found = None  # (cursor, file) -- definition preferred
    ordered = sorted(entries, key=lambda x: x['file'])
    if fpart:
        # entries matching the file part first (headers match via the
        # cursor's own location below)
        ordered.sort(key=lambda x: fpart not in x['file'])
    for e in ordered:
        try:
            tu = idx.parse(e['file'], args=lint_args(e, resource_dir))
        except ci.TranslationUnitLoadError:
            continue
        if any(d.severity >= ci.Diagnostic.Error for d in tu.diagnostics):
            continue
        for cur in tu.cursor.get_children():
            if cur.kind != CK.FUNCTION_DECL or cur.spelling != fname:
                continue
            if fpart and cur.location.file is not None \
                    and fpart not in str(cur.location.file) \
                    and fpart not in e['file']:
                continue
            if found is None or cur.is_definition():
                found = (cur, e['file'])
            if found[0].is_definition():
                break
        if found is not None and found[0].is_definition():
            break
    if found is None:
        where = f' in files matching "{fpart}"' if fpart else ''
        print(f'--explain: function {fname!r} not found{where}',
              file=sys.stderr)
        return 2

    cur, tu_file = found
    defn = cur if cur.is_definition() else None
    try:
        defn = defn or cur.get_definition()
    except Exception:
        pass
    show = defn or cur
    loc = show.location
    rel = os.path.relpath
    sem = resolve_sem(cur, {})

    print(f'{fname}  ({rel(str(loc.file))}:{loc.line}'
          f'{"" if defn else ", declaration only"})')
    print(f'  resolved by: {sem.why}')
    if sem.custom:
        print('  protocol:    CUSTOM -- not checked; call sites treat '
              'arguments and product as unknown')
    elif not sem.check:
        print(f'  protocol:    trusted ({sem.default_args} args, '
              f'{sem.product} product); body NOT checked')
    print('  arguments:')
    for p in show.get_arguments():
        pname = p.spelling or '<unnamed>'
        tspell = p.type.spelling
        if not is_noun_type(p.type):
            print(f'    {pname:<12} {tspell:<12} (not a noun: untracked)')
            continue
        if sem.passthrough == pname:
            mode = 'PASSTHROUGH (the product is this argument itself)'
        elif sem.is_direct(pname):
            mode = 'DIRECT (proven direct atom if the call returns)'
        else:
            mode = sem.arg_mode(pname).upper()
            src = ('per-arg annotation' if pname in sem.args
                   else sem.why)
            mode = f'{mode:<12} ({src})'
        print(f'    {pname:<12} {tspell:<12} {mode}')
    rt = show.result_type
    if sem.passthrough is not None:
        print(f'  product:     same value and ownership as '
              f'[{sem.passthrough}]')
    elif is_noun_type(rt):
        print(f'  product:     {sem.product.upper()}'
              + (' (caller owns it and must u3z)'
                 if sem.product == TRANSFER
                 else ' (uncounted; caller must NOT free)'))
    else:
        print(f'  product:     {rt.spelling} (not a noun: untracked)')
    file_custom = False
    if defn is not None and defn.location.file is not None:
        dfile = str(defn.location.file)
        try:
            with open(dfile, 'r', errors='replace') as fh:
                file_custom = bool(RE_FILE_CUSTOM.search(fh.read(4096)))
        except OSError:
            pass
    checked = sem.check and not sem.custom and not file_custom
    print(f'  body checked: {"yes" if checked else "no"}'
          + (f' ({rel(dfile)} is @Refcount: custom file)'
             if file_custom else ''))

    comments = cursor_comments(cur).strip()
    if comments:
        print('  annotation comments considered:')
        for line in comments.splitlines():
            print(f'    | {line.strip()}')
    else:
        print('  annotation comments considered: none '
              '(prefix/position defaults apply)')

    is_static = cur.storage_class == ci.StorageClass.STATIC
    base = prefix_sem(fname, str(loc.file), is_static)
    print(f'  positional default (before comments): '
          f'{base.default_args} args, {base.product} product '
          f'[{base.why}]')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--cdb', default='compile_commands.json')
    ap.add_argument('--filter', default='pkg/noun',
                    help='only check files whose path contains this')
    ap.add_argument('--only', default=None,
                    help='only check files whose path contains this '
                         '(applied after --filter)')
    ap.add_argument('--function', default=None,
                    help='only check the named function')
    ap.add_argument('--libclang', default=None)
    ap.add_argument('--verbose', action='store_true')
    ap.add_argument('--selftest', action='store_true',
                    help='check tools/refcount_selftest.c (expects exactly '
                         'the seeded findings) instead of the codebase')
    ap.add_argument('--explain', metavar='[FILE:]FUNCTION', default=None,
                    help='print the resolved refcount protocol for one '
                         'function and the reason behind it, then exit '
                         '(e.g. --explain hashtable.c:u3h_put)')
    args = ap.parse_args()

    lib = args.libclang or find_libclang()
    if lib:
        ci.Config.set_library_file(lib)
    resource_dir = find_resource_dir(lib)

    cdb = load_cdb(args.cdb)
    seen_files = set()
    entries = []
    for e in cdb:
        f = e['file']
        if args.filter not in f or not f.endswith('.c'):
            continue
        if args.only and args.only not in f:
            continue
        if f in seen_files:
            continue
        seen_files.add(f)
        entries.append(e)

    if args.selftest:
        # borrow compile flags from any pkg/noun entry
        if not entries:
            print('selftest: no pkg/noun entries in cdb', file=sys.stderr)
            return 2
        import copy
        test_c = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              'refcount_selftest.c')
        e = copy.deepcopy(entries[0])
        e['arguments'] = [test_c if a == e['file'] else a
                          for a in e['arguments']]
        e['file'] = test_c
        entries = [e]

    if args.explain:
        return explain(entries, resource_dir, args.explain)

    tool = Tool()
    idx = ci.Index.create()
    n_checked = 0
    for e in sorted(entries, key=lambda x: x['file']):
        fpath = e['file']
        try:
            tu = idx.parse(fpath, args=lint_args(e, resource_dir))
        except ci.TranslationUnitLoadError as ex:
            print(f'{fpath}: PARSE FAILED: {ex}', file=sys.stderr)
            continue
        hard = [d for d in tu.diagnostics
                if d.severity >= ci.Diagnostic.Error]
        if hard:
            print(f'{fpath}: {len(hard)} parse errors '
                  f'(first: {hard[0]})', file=sys.stderr)
            continue
        try:
            with open(fpath, 'r', errors='replace') as fh:
                head = fh.read(4096)
        except OSError:
            head = ''
        if RE_FILE_CUSTOM.search(head):
            if args.verbose:
                print(f'-- {fpath}: @Refcount: custom file, skipped')
            continue
        fcm = FileComments(tu, fpath)
        for cur in tu.cursor.get_children():
            if cur.kind != CK.FUNCTION_DECL or not cur.is_definition():
                continue
            if (cur.location.file is None
                    or str(cur.location.file) != fpath):
                continue
            if args.function and cur.spelling != args.function:
                continue
            # only functions that take or return nouns
            takes = any(is_noun_type(p.type) for p in cur.get_arguments()
                        if p.kind == CK.PARM_DECL)
            rets = is_noun_type(cur.result_type)
            if not takes and not rets:
                continue
            sem = resolve_sem(cur, tool.sem_cache)
            for (wl, wmsg) in sem.warnings:
                tool.findings.append((fpath, wl or cur.location.line,
                                      cur.location.column, cur.spelling,
                                      'annotation', wmsg))
            if not sem.check:
                if args.verbose:
                    print(f'-- {cur.spelling}: trusted ({sem.why})')
                continue
            n_checked += 1
            if args.verbose:
                print(f'-- {cur.spelling}: args={sem.default_args} '
                      f'product={sem.product} ({sem.why})')
            chk = FnChecker(tool, cur, sem, fcm)
            tool.findings += chk.run()

    rel = os.path.relpath
    for (f, l, c, fn, cat, msg) in tool.findings:
        try:
            f = rel(f)
        except ValueError:
            pass
        print(f'{f}:{l}:{c}: [{cat}] {fn}(): {msg}')
    print(f'\n{n_checked} functions checked, '
          f'{len(tool.findings)} findings', file=sys.stderr)

    if args.selftest:
        expected = {
            ('bug_leak', 'leak'),
            ('bug_double', 'double-free'),
            ('bug_double', 'use-after-free'),
            ('bug_overfree', 'over-free'),
            ('bug_uaf', 'use-after-free'),
            ('bug_borrow', 'use-after-free'),
            ('bug_smuggle', 'leak'),
            ('warn_conflict', 'annotation'),
        }
        clean_fns = {
            'bug_ok', 'ok_retain_prod', 'ok_passthrough', 'custom_unchecked',
            'assert_unchecked', 'needs_direct', 'ok_direct_caller', 'ok_block',
        }
        got = {(fn, cat) for (_, _, _, fn, cat, _) in tool.findings}
        found_fns = {fn for (_, _, _, fn, _, _) in tool.findings}
        missing = expected - got
        dirty = clean_fns & found_fns
        ok = not missing and not dirty
        if missing:
            print(f'selftest: missing findings: {sorted(missing)}',
                  file=sys.stderr)
        if dirty:
            print(f'selftest: unexpected findings on: {sorted(dirty)}',
                  file=sys.stderr)
        print('selftest:', 'PASS' if ok else 'FAIL', file=sys.stderr)
        return 0 if ok else 1

    return 1 if tool.findings else 0


if __name__ == '__main__':
    sys.exit(main())
