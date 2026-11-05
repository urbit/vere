#!/usr/bin/env python3
"""Integrate measurement logs across ships.

Usage: measure-aggregate.py [--keep-first] [--keep-ops] [--keep-long] [--by-input] [--csv FILE] INPUT ...

Each INPUT is a log, a directory (searched recursively for
measure-*.log), or a tarball (.tar, .tgz, .tar.gz) of logs, read in
place without extracting. Logs are grouped into ships by the ship named
in their header; logs without one (from before it was added) are
grouped by INPUT. --by-input groups by INPUT always.

For each ship, the first snapshot interval of each log is dropped (it
includes startup) unless --keep-first. So are intervals in which a
pack, meld or melt ran (--keep-ops keeps them), and a live ship's
intervals more than 2.5 times its snapshot interval, from host sleep or
an idle ship (--keep-long keeps them). The remaining intervals are
pooled into one value per metric. The report gives, per metric, how
many ships have it and the distribution across ships (each ship counts
once), and the value pooled over all ships' intervals. --csv writes one
row per ship, for further analysis.

Metrics follow the plan's "Tuning the merged heuristics and policies"
and "Prioritizing follow-up work": lines from older builds that lack
some (P, H, D, G, Y) are used when present.
"""

import csv
import io
import os
import sys
import tarfile
from collections import defaultdict


# ---- parsing -----------------------------------------------------------------

def parse_header(line):
    """Header fields, e.g. {"ship": "~zod", "vere": "...", "word_bits": "32"}."""
    out = {}
    for w in line[1:].split():
        k, sep, v = w.partition("=")
        if sep:
            out[k] = v
    return out


def ints(p):
    try:
        return tuple(int(x) for x in p)
    except ValueError:
        return None


def parse(lines):
    """Return (header dict, intervals, ops) for one log's lines.

    Each S line starts an interval, and the lines after it, up to the
    next S line, report on it. F lines are summed by (phase, kind). O
    lines (a pack, meld or melt) carry their own time, and are given to
    the interval whose snapshot came next.
    """
    header = {}
    intervals = []
    ops = []
    cur = None
    start = None
    for line in lines:
        line = line.rstrip("\n")
        if not line:
            continue
        t, p = line[0], line.split()
        if t == "#":
            header = parse_header(line)
            start = int(header.get("time", 0)) or None
        elif t == "O" and len(p) >= 8:
            v = ints([p[1], p[2], p[5], p[6], p[7]])
            if v is not None:
                ops.append({"time": v[0], "eve": v[1], "op": p[3], "why": p[4],
                            "opt": v[2], "ms": v[3], "words": v[4]})
        elif t == "S":
            v = ints(p[1:6])
            if v is None:
                continue
            cur = {"F": defaultdict(int), "ops": []}
            cur["eve"], cur["time"], cur["patch"], cur["skip"], cur["tasks"] = v
            cur["secs"] = (v[1] - start) if start else 0
            start = v[1]
            intervals.append(cur)
        elif cur is None:
            continue
        elif t == "F" and len(p) == 5:
            n = ints(p[4:5])
            if n:
                cur["F"][(p[1], p[2])] += n[0]
        elif t in "ACDGHMPTY":
            v = ints(p[1:])
            if v is not None:
                cur[t] = v
    for o in ops:
        for i in intervals:
            if i["time"] >= o["time"]:
                i["ops"].append(o)
                break
    return header, intervals, ops


def long_limit(hdr, ivs):
    """Seconds past which a live interval counts as long, or None.

    Uses the header's snap_secs; for older logs without it, the log's
    median interval, if it looks live (a replay's intervals are short).
    """
    if hdr.get("mode") in ("replay", "offline"):
        return None
    sap = int(hdr.get("snap_secs", 0) or 0)
    if not sap:
        secs = sorted(i["secs"] for i in ivs if i["secs"] > 0)
        if not secs or secs[len(secs) // 2] < 60:
            return None
        sap = secs[len(secs) // 2]
    return 2.5 * sap


def read_input(path):
    """Yield (name, lines) for each log in [path]."""
    if not os.path.exists(path):
        print(f"{path}: not found", file=sys.stderr)
    elif os.path.isdir(path):
        #  NB: not glob's "**", which skips hidden directories like .urb
        found = []
        for root, _, files in os.walk(path):
            found.extend(os.path.join(root, f) for f in files
                         if f.startswith("measure-") and f.endswith(".log"))
        for f in sorted(found):
            with open(f, errors="replace") as fh:
                yield f, fh.readlines()
    elif tarfile.is_tarfile(path):
        with tarfile.open(path, "r:*") as tar:
            for m in tar:
                base = os.path.basename(m.name)
                if m.isfile() and base.startswith("measure-") and base.endswith(".log"):
                    fh = tar.extractfile(m)
                    if fh:
                        txt = io.TextIOWrapper(fh, errors="replace")
                        yield f"{path}:{m.name}", txt.readlines()
    else:
        with open(path, errors="replace") as fh:
            yield path, fh.readlines()


# ---- per-ship metrics --------------------------------------------------------

def ratio(a, b):
    return a / b if b else None


def total(ivs, line, j):
    return sum(i[line][j] for i in ivs if line in i and len(i[line]) > j)


def values(ivs, line, j):
    return [i[line][j] for i in ivs if line in i and len(i[line]) > j]


def median(v):
    v = sorted(x for x in v if x is not None)
    return v[len(v) // 2] if v else None


def sums(ivs):
    """Sums over [ivs] that the metrics are ratios of."""
    s = defaultdict(float)
    for i in ivs:
        s["intervals"] += 1
        s["secs"] += i.get("secs", 0)
        s["patch"] += i["patch"]
        s["tasks"] += i["tasks"]
        for (pha, kin), n in i["F"].items():
            s["F"] += n
            if kin == "crag":
                s["F_crag"] += n
            if kin == "w0":
                s["F_w0"] += n
            if pha == "roc-lose":
                s["F_roclose"] += n
            if pha.startswith("take-byc") or pha.startswith("reap-byc"):
                s["F_byc"] += n
            if pha.startswith("rec-"):
                s["F_rec"] += n
            if pha.endswith("-dup"):
                s["F_dup"] += n
            if pha == "take-pro":
                s["F_pro"] += n
        if "A" in i:
            a = i["A"]
            s["A_fre"] += a[0]; s["A_cln"] += a[1]; s["A_alt"] += a[2]; s["A_reu"] += a[3]
            s["A_patch"] += i["patch"]
        if "C" in i:
            s["C_rec"] += i["C"][0]; s["C_tasks"] += i["tasks"]
        if "P" in i:
            p = i["P"]
            s["P_cmp"] += p[4]; s["P_tasks"] += i["tasks"]
        if "H" in i:
            h = i["H"]
            s["H_add"] += h[3]; s["H_del"] += h[4]; s["H_tasks"] += i["tasks"]
            s["H_reb"] += h[5]; s["H_ms"] += h[6]
        if "D" in i:
            d = i["D"]
            s["D_pro"] += d[0] + d[1] + d[2]
            s["D_hit"] += d[3] + d[4] + d[5] + d[6]
            s["D_hit_cold"] += d[6]
            s["D_pag"] += d[8]; s["D_patch"] += i["patch"]
        if "G" in i:
            g = i["G"]
            s["G_win"] += g[0]; s["G_gan"] += g[1]; s["G_can"] += g[4]
            s["G_spl"] += g[5]; s["G_shr"] += g[8]
        if "Y" in i:
            y = i["Y"]
            s["Y_all"] += y[0]; s["Y_pos"] += y[1]; s["Y_exc"] += y[2]
    return s


#  name, decision it informs, function of (sums, intervals). a name
#  in PERCENT is a fraction, printed as a percentage; ratios are pooled
#  across ships from the same sums
#
METRICS = [
    ("hours", "coverage",
     lambda s, v: s["secs"] / 3600 if s["secs"] else None),
    ("tasks", "coverage", lambda s, v: s["tasks"]),
    ("patch pages / hour", "overall churn",
     lambda s, v: ratio(s["patch"], s["secs"] / 3600)),
    ("patch pages / task", "overall churn", lambda s, v: ratio(s["patch"], s["tasks"])),
    ("first writes: page records", "C1 tier 2", lambda s, v: ratio(s["F_crag"], s["F"])),
    ("first writes: refcounts (w0)", "C3", lambda s, v: ratio(s["F_w0"], s["F"])),
    ("first writes: roc-lose", "C1 tier 2 / C3", lambda s, v: ratio(s["F_roclose"], s["F"])),
    ("first writes: bytecode take/reap", "(h)", lambda s, v: ratio(s["F_byc"], s["F"])),
    ("first writes: reclaims (rec-*)", "(b)", lambda s, v: ratio(s["F_rec"], s["F"])),
    ("first writes: dedup set (*-dup)", "(b), (c)", lambda s, v: ratio(s["F_dup"], s["F"])),
    ("first writes: product take (take-pro)", "(i)", lambda s, v: ratio(s["F_pro"], s["F"])),
    ("A: alloc onto clean page, dirty had room / patch", "C1 tier 1",
     lambda s, v: ratio(s["A_alt"], s["A_patch"])),
    ("A: page-record frees / patch", "C1 tier 2",
     lambda s, v: ratio(s["A_fre"], s["A_patch"])),
    ("A: of those, later allocated into", "C1 tier 1 vs 2 overlap",
     lambda s, v: ratio(s["A_reu"], s["A_fre"])),
    ("M: resident MB (median)", "memory",
     lambda s, v: mb(median(values(v, "M", 0)))),
    ("M: resident MB (max)", "memory", lambda s, v: mb(max(values(v, "M", 0), default=None))),
    ("M: anonymous MB (median)", "memory / (c)",
     lambda s, v: mb(median(values(v, "M", 1)))),
    ("M: swapped MB (max)", "memory / (c)",
     lambda s, v: mb(max(values(v, "M", 3), default=None))),
    ("T: bytecode programs (max)", "(b)", lambda s, v: max(values(v, "T", 4), default=None)),
    ("T: wish cache entries (max)", "(b)", lambda s, v: max(values(v, "T", 9), default=None)),
    ("C: reclaims / 1000 tasks", "(b)",
     lambda s, v: ratio(1000 * s["C_rec"], s["C_tasks"]) if s["C_tasks"] else None),
    ("P: program KB (max)", "(b) bound, A8",
     lambda s, v: kb(max(values(v, "P", 1), default=None))),
    ("P: dead keys / programs (median)", "A8",
     lambda s, v: median(ratio(i["P"][2], i["P"][0]) for i in v if "P" in i)),
    ("P: looked up / programs (median)", "A8 eviction cap",
     lambda s, v: median(ratio(i["P"][3], i["P"][0]) for i in v if "P" in i)),
    ("P: compiles / task", "(b), A8", lambda s, v: ratio(s["P_cmp"], s["P_tasks"])),
    ("H: set cells (median)", "(c)", lambda s, v: median(values(v, "H", 1))),
    ("H: set MB (median)", "(c)", lambda s, v: mb(median(values(v, "H", 2)))),
    ("H: inserts / task", "(c) delta growth", lambda s, v: ratio(s["H_add"], s["H_tasks"])),
    ("H: removals / task", "(c)", lambda s, v: ratio(s["H_del"], s["H_tasks"])),
    ("H: rebuild ms (mean)", "(c)", lambda s, v: ratio(s["H_ms"], s["H_reb"])),
    ("D: hits / probes", "(c) scope", lambda s, v: ratio(s["D_hit"], s["D_pro"])),
    ("D: hits on cold state alone", "(c) scope",
     lambda s, v: ratio(s["D_hit_cold"], s["D_hit"])),
    ("D: table fill (median)", "(c) sizing",
     lambda s, v: median(ratio(i["H"][1], i["D"][7]) for i in v if "D" in i and "H" in i)),
    ("D: on-loom table pages / patch", "(c) placement",
     lambda s, v: ratio(s["D_pag"], s["D_patch"])),
    ("D: on-loom table pages / patch (max interval)", "(c) placement",
     lambda s, v: max((ratio(i["D"][8], i["patch"]) for i in v
                       if "D" in i and i["patch"]), default=None)),
    ("G: cancelled / deferred", "(i)", lambda s, v: ratio(s["G_can"], s["G_gan"])),
    ("G: spilled at cap / gains", "(i) cap",
     lambda s, v: ratio(s["G_spl"], s["G_gan"] + s["G_spl"])),
    ("G: peak pending", "(i) cap", lambda s, v: max(values(v, "G", 3), default=None)),
    ("G: shrinks / 1000 windows", "(i) hysteresis",
     lambda s, v: ratio(1000 * s["G_shr"], s["G_win"]) if s["G_win"] else None),
    ("Y: aligned", "(i) adaptive bound", lambda s, v: ratio(s["Y_pos"], s["Y_all"])),
    ("Y: aligned and exclusive", "(i) fused path", lambda s, v: ratio(s["Y_exc"], s["Y_all"])),
]


PERCENT = {name for name, _, _ in METRICS
           if name.startswith(("first writes", "A:", "D: hits", "D: table",
                               "D: on-loom", "G: cancelled", "G: spilled", "Y:"))
           or "keys /" in name or "looked up /" in name}


def mb(b):
    return None if b is None else b / (1 << 20)


def kb(b):
    return None if b is None else b / 1024


# ---- report ------------------------------------------------------------------

def label(key):
    """A ship's name, or for logs without one, its pier's directory."""
    if key.startswith("~"):
        return key
    parts = [p for p in os.path.normpath(key).split(os.sep) if p]
    while parts and parts[-1] in (".urb", "measure"):
        parts.pop()
    return parts[-1] if parts else key


def quantile(v, q):
    v = sorted(v)
    return v[min(len(v) - 1, int(q * (len(v) - 1) + 0.5))]


def fmt(x, pct=False):
    if x is None:
        return "-"
    if pct:
        return f"{100 * x:.1f}%"
    if isinstance(x, float):
        return f"{x:,.2f}" if abs(x) < 10 else f"{x:,.0f}"
    return f"{x:,}"


def main():
    argv = sys.argv[1:]
    keep_first = "--keep-first" in argv
    keep_ops = "--keep-ops" in argv
    keep_long = "--keep-long" in argv
    by_input = "--by-input" in argv
    csv_path = None
    if "--csv" in argv:
        k = argv.index("--csv")
        csv_path = argv[k + 1] if k + 1 < len(argv) else None
        argv = argv[:k] + argv[k + 2:]
    inputs = [a for a in argv if not a.startswith("--")]
    if not inputs:
        print(__doc__)
        sys.exit(1)

    ships = defaultdict(lambda: {"ivs": [], "logs": 0, "dropped": 0, "hdr": {},
                                 "ops": defaultdict(int), "op_ivs": 0, "long_ivs": 0})
    for inp in inputs:
        for name, lines in read_input(inp):
            hdr, ivs, ops = parse(lines)
            key = inp if by_input or "ship" not in hdr else hdr["ship"]
            sh = ships[key]
            sh["logs"] += 1
            #  a log holding only an offline operation has a sparser header
            if hdr and (hdr.get("mode") != "offline" or not sh["hdr"]):
                sh["hdr"] = hdr
            for o in ops:
                sh["ops"][o["op"]] += 1
            lim = long_limit(hdr, ivs)
            if ivs and not keep_first:
                ivs = ivs[1:]
                sh["dropped"] += 1
            if not keep_ops:
                sh["op_ivs"] += sum(1 for i in ivs if i["ops"])
                ivs = [i for i in ivs if not i["ops"]]
            if lim and not keep_long:
                sh["long_ivs"] += sum(1 for i in ivs if i["secs"] > lim)
                ivs = [i for i in ivs if i["secs"] <= lim]
            sh["ivs"].extend(i for i in ivs if i["tasks"] or i["patch"])

    rows = []
    pooled = []
    for key, sh in sorted(ships.items()):
        if not sh["ivs"]:
            continue
        s = sums(sh["ivs"])
        row = {"ship": key, "vere": sh["hdr"].get("vere", "?"),
               "os": sh["hdr"].get("os", "?"), "word_bits": sh["hdr"].get("word_bits", "?"),
               "snap": (f"{sh['hdr']['snap_events']} events"
                        if sh["hdr"].get("mode") == "replay"
                        else f"{sh['hdr']['snap_secs']} s"
                        if sh["hdr"].get("snap_secs") else "?"),
               "logs": sh["logs"], "intervals": len(sh["ivs"]),
               "long_dropped": sh["long_ivs"], "op_dropped": sh["op_ivs"],
               "packs": sh["ops"]["pack"], "melds": sh["ops"]["meld"],
               "melts": sh["ops"]["melt"]}
        for name, _, f in METRICS:
            try:
                row[name] = f(s, sh["ivs"])
            except (IndexError, KeyError, ZeroDivisionError):
                row[name] = None
        rows.append(row)
        pooled.extend(sh["ivs"])

    if not rows:
        print("no complete snapshot intervals (try --keep-first)")
        return

    print(f"# {len(rows)} ship(s), {sum(r['logs'] for r in rows)} log(s), "
          f"{len(pooled)} interval(s)")
    for r in rows:
        ops = ", ".join(f"{r[k]} {k}" for k in ("packs", "melds", "melts") if r[k])
        print(f"  {label(r['ship']):<20} {r['vere']:<16} {r['os']:<8} {r['word_bits']}-bit  "
              f"snap {r['snap']}  {r['logs']} log(s), {r['intervals']} interval(s), "
              f"{fmt(r['hours'])} h; dropped {r['long_dropped']} long, "
              f"{r['op_dropped']} with an operation" + (f"; {ops}" if ops else ""))

    ps = sums(pooled)
    print(f"\n{'metric':<50}{'ships':>6}{'p10':>11}{'median':>11}"
          f"{'p90':>11}{'max':>11}{'pooled':>11}   informs")
    for name, informs, f in METRICS:
        v = [r[name] for r in rows if r[name] is not None]
        if not v:
            continue
        try:
            pv = f(ps, pooled) if name not in ("hours", "tasks") else sum(v)
        except (IndexError, KeyError, ZeroDivisionError):
            pv = None
        c = name in PERCENT
        print(f"{name:<50}{len(v):>6}{fmt(quantile(v, 0.1), c):>11}"
              f"{fmt(quantile(v, 0.5), c):>11}{fmt(quantile(v, 0.9), c):>11}"
              f"{fmt(max(v), c):>11}{fmt(pv, c):>11}   {informs}")

    if csv_path:
        with open(csv_path, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print(f"\nwrote {csv_path}")


if __name__ == "__main__":
    main()
