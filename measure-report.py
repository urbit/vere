#!/usr/bin/env python3
"""Summarize a jb/measure log (<pier>/.urb/measure/measure-<time>.log).

Usage: measure-report.py [--intervals] [--keep-first] [--keep-ops] [--keep-long] LOG|DIR ...

Accepts any number of logs, or directories (a pier, its .urb, its
.urb/measure, or any directory holding measure-*.log files). Logs are aggregated. Each log
starts at a restart, so by default the first snapshot interval of each
log is dropped (it includes startup); --keep-first keeps it. Intervals
in which a pack, meld or melt ran are dropped too (--keep-ops keeps
them), as are a live ship's intervals more than 2.5 times its snapshot
interval, from host sleep or an idle ship (--keep-long keeps them).
Elapsed time counts only the intervals kept. Tasks recorded after a
log's last snapshot are ignored.

Log lines (one snapshot interval ends at each S line):
  E <eve> <poke|task> <changed> <first-faults> <dirty> <hash-us>: <pages...>
  S <eve> <time> <patch-pages> <skipped-free> <tasks>
  F <phase> <kind> <chunk-log2> <pages>     first write that dirtied a patch page
  U <pages>                                 patch pages with no fault (fresh pages)
  R <table> <exits-home> <empty-home> <entries-home> <exits-all> <empty-all> <entries-all>
  K <jet> <declined> <no-jet> <unregistered>      (jb/measure only)
  B <home|inner> <nock2|site> <cur-ptr> <cur-cur> <cur-sen> <anc-ptr> <anc-cur> <anc-sen> <miss>
                                            bytecode lookups by formula pointer (jb/measure only)
  L <home|inner> <ptr> <copy-cur> <copy-sen> <diff> <none>
                                            call-site battery checks (jb/measure only)
  T <war> <cod> <han> <bas> <byc> <per> <for> <dup> <memo>
                                            home-road table entries at the snapshot
  C <reclaims> <byc-cleared> <han-cleared> [<dup-before-prune> <dup-pruned>]
                                            u3m_reclaim() calls since the last snapshot
                                            (the dedup-prune fields: older logs only)
  A <record-frees> <clean-allocs> <with-dirty-room> <frees-then-allocated>
                                            home-road page records and allocation
  M <resident> <anonymous> <file-backed> <swapped>
                                            process memory in bytes at the snapshot
  O <time> <eve> <pack|meld|melt> <why> <options> <ms> <freed-words>
                                            a pack, meld or melt, logged when it ran.
                                            why: live (|pack, |meld, conn), memory
                                            (memory threshold), loom (replay out of
                                            loom), offline (urbit pack/meld/melt).
                                            options: 1 memo, 2 ford, 4 cold

Lines after an S line report on that interval, except O lines, which
carry their own time. The header gives the ship, the runtime version,
OS and architecture, the mode (live, replay, or offline for a log
holding only an offline operation) and the snapshot interval
(snap_secs for live, snap_events for replay). To integrate logs across ships, see measure-aggregate.py.
"""

import glob
import os
import sys
from collections import defaultdict


def pct(a, b):
    return f"{100.0 * a / b:5.1f}%" if b else "    -"


def parse_header(line):
    """Header fields, e.g. {"ship": "~zod", "mode": "live"}."""
    out = {}
    for w in line[1:].split():
        k, sep, v = w.partition("=")
        if sep:
            out[k] = v
    return out


OPTS = ((1, "memo"), (2, "ford"), (4, "cold"))


def new_interval():
    return {
        "events": None,
        "patch": 0, "skip": 0, "tasks": 0, "eve": 0,
        "F": defaultdict(int),
        "U": 0,
        "A": None,
        "M": None,
        "R": {},
        "K": None,
        "B": {},
        "L": {},
        "T": None,
        "C": None,
        "H": None,
        "D": None,
        "P": None,
        "G": None,
        "Y": None,
        "ops": [],
    }


def parse(path):
    """Return (header, intervals, trailing-task-count, ops) for one log.

    Each interval gets "secs", its length (from the header's time for
    the first), and "ops", the operations that ran in it.
    """
    intervals = []
    ops = []
    cur_events = []
    cur = None
    header = ""
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            t = line[0]
            if t == "#":
                header = line
            elif t == "E":
                #  NB: the log is buffered, so the last line of a live or
                #  crashed run may be cut off; skip anything malformed
                head, sep, pages = line.partition(":")
                p = head.split()
                if not sep or len(p) < 7:
                    continue
                try:
                    cur_events.append({
                        "eve": int(p[1]), "tag": p[2], "n": int(p[3]),
                        "fal": int(p[4]), "dirty": int(p[5]), "us": int(p[6]),
                        "pages": set(int(x) for x in pages.split()),
                    })
                except ValueError:
                    continue
            elif t == "O":
                p = line.split()
                try:
                    ops.append({"time": int(p[1]), "eve": int(p[2]), "op": p[3],
                                "why": p[4], "opt": int(p[5]), "ms": int(p[6]),
                                "words": int(p[7])})
                except (IndexError, ValueError):
                    continue
            elif t == "S":
                p = line.split()
                cur = new_interval()
                cur["eve"], cur["time"], cur["patch"], cur["skip"], cur["tasks"] = (
                    int(p[1]), int(p[2]), int(p[3]), int(p[4]), int(p[5]))
                cur["events"] = cur_events
                cur_events = []
                intervals.append(cur)
            elif t == "F" and cur is not None:
                _, pha, kin, log2, n = line.split()
                cur["F"][(pha, kin, int(log2))] += int(n)
            elif t == "U" and cur is not None:
                cur["U"] = int(line.split()[1])
            elif t == "A" and cur is not None:
                cur["A"] = tuple(int(x) for x in line.split()[1:])
            elif t == "M" and cur is not None:
                cur["M"] = tuple(int(x) for x in line.split()[1:])
            elif t == "R" and cur is not None:
                p = line.split()
                cur["R"][p[1]] = tuple(int(x) for x in p[2:])
            elif t == "K" and cur is not None:
                cur["K"] = tuple(int(x) for x in line.split()[1:])
            elif t == "B" and cur is not None:
                p = line.split()
                cur["B"][(p[1], p[2])] = tuple(int(x) for x in p[3:])
            elif t == "T" and cur is not None:
                cur["T"] = tuple(int(x) for x in line.split()[1:])
            elif t == "P" and cur is not None:
                cur["P"] = tuple(int(x) for x in line.split()[1:])
            elif t == "C" and cur is not None:
                cur["C"] = tuple(int(x) for x in line.split()[1:])
            elif t == "H" and cur is not None:
                cur["H"] = tuple(int(x) for x in line.split()[1:])
            elif t == "D" and cur is not None:
                cur["D"] = tuple(int(x) for x in line.split()[1:])
            elif t == "G" and cur is not None:
                cur["G"] = tuple(int(x) for x in line.split()[1:])
            elif t == "Y" and cur is not None:
                cur["Y"] = tuple(int(x) for x in line.split()[1:])
            elif t == "L" and cur is not None:
                p = line.split()
                cur["L"][p[1]] = tuple(int(x) for x in p[2:])
    hdr = parse_header(header) if header else {}
    prev = int(hdr.get("time", 0)) or None
    for i in intervals:
        i["secs"] = (i["time"] - prev) if prev else 0
        prev = i["time"]
    #  an operation belongs to the interval whose snapshot came next
    for o in ops:
        for i in intervals:
            if i["time"] >= o["time"]:
                i["ops"].append(o)
                break
    return header, intervals, len(cur_events), ops


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


def find_logs(args):
    logs = []
    for a in args:
        if os.path.isdir(a):
            found = (glob.glob(os.path.join(a, "measure-*.log")) or
                     glob.glob(os.path.join(a, "measure", "measure-*.log")) or
                     glob.glob(os.path.join(a, ".urb", "measure", "measure-*.log")) or
                     glob.glob(os.path.join(a, ".urb", "measure-*.log")))
            if not found:
                print(f"no measure-*.log in {a}", file=sys.stderr)
            logs.extend(found)
        else:
            logs.append(a)
    return sorted(set(logs))


def main():
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        sys.exit(1)
    show_intervals = "--intervals" in flags
    keep_first = "--keep-first" in flags
    keep_ops = "--keep-ops" in flags
    keep_long = "--keep-long" in flags

    intervals = []
    headers = set()
    dropped = trailing = 0
    op_dropped = long_dropped = 0
    all_ops = []
    rebuilds = []
    logs = find_logs(args)
    for path in logs:
        hed, ivs, tail, ops = parse(path)
        hdr = parse_header(hed) if hed else {}
        all_ops.extend(dict(o, ship=hdr.get("ship", "?")) for o in ops)
        #  hashcons rebuilds, including at startup
        rebuilds.extend((i["H"], i["D"]) for i in ivs if i["H"] and i["H"][5])
        if hed and ivs:
            headers.add(" ".join(w for w in hed.split()
                                 if w.split("=")[0] in ("word_bits", "page_bytes")))
        trailing += tail
        lim = long_limit(hdr, ivs)
        if ivs and not keep_first:
            ivs = ivs[1:]
            dropped += 1
        if not keep_ops:
            op_dropped += sum(1 for i in ivs if i["ops"])
            ivs = [i for i in ivs if not i["ops"]]
        if lim and not keep_long:
            long_dropped += sum(1 for i in ivs if i["secs"] > lim)
            ivs = [i for i in ivs if i["secs"] <= lim]
        intervals.extend(ivs)
    elapsed = sum(i["secs"] for i in intervals)

    header = (f"# {len(logs)} log(s); {' / '.join(sorted(headers))}; "
              f"dropped {dropped} startup, {op_dropped} pack/meld/melt and "
              f"{long_dropped} long interval(s); "
              f"{trailing} task(s) after a log's last snapshot ignored")
    print(header)
    if all_ops:
        import datetime
        print("\n== packs, melds and melts ==")
        for o in sorted(all_ops, key=lambda o: o["time"]):
            when = datetime.datetime.fromtimestamp(o["time"]).strftime("%Y-%m-%d %H:%M")
            opts = ",".join(n for b, n in OPTS if o["opt"] & b) or "-"
            print(f"  {when}  {o['ship']:<16} event {o['eve']:<12} {o['op']:<5} "
                  f"{o['why']:<8} options {opts:<15} {o['ms'] / 1000:7.1f} s   "
                  f"freed {o['words']} words")
        print()
    if not intervals:
        print("no complete snapshot intervals yet (or only startup ones; "
              "try --keep-first)")
        return

    tot_patch = sum(i["patch"] for i in intervals)
    tot_tasks = sum(i["tasks"] for i in intervals)
    print(f"snapshots: {len(intervals)}   tasks: {tot_tasks}   "
          f"patch pages: {tot_patch} ({tot_patch * 16 / 1024:.1f} MiB)   "
          f"mean/snapshot: {tot_patch / len(intervals):.1f}")
    if elapsed > 0:
        print(f"elapsed: {elapsed / 3600:.1f} h   patch pages/hour: {tot_patch * 3600 / elapsed:.0f} "
              f"({tot_patch * 16 / 1024 * 3600 / elapsed / 1024:.1f} GiB/hour)")

    # ---- attribution ------------------------------------------------------
    by_pha = defaultdict(int)
    by_kin = defaultdict(int)
    by_pk = defaultdict(int)
    uns = 0
    for i in intervals:
        uns += i["U"]
        for (pha, kin, log2), n in i["F"].items():
            by_pha[pha] += n
            by_kin[kin] += n
            key = f"{kin}/{1 << log2}w" if kin in ("w0", "w1", "body", "crag") else kin
            by_pk[(pha, key)] += n

    print("\n== what first dirtied each patch page ==")
    print(f"  {'fresh (no fault: new heap pages)':<34}{uns:>9}  {pct(uns, tot_patch)}")
    print("  by phase:")
    for pha, n in sorted(by_pha.items(), key=lambda x: -x[1]):
        print(f"    {pha:<30}{n:>9}  {pct(n, tot_patch)}")
    print("  by store location:")
    for kin, n in sorted(by_kin.items(), key=lambda x: -x[1]):
        print(f"    {kin:<30}{n:>9}  {pct(n, tot_patch)}")
    print("  top phase x location (w0 = refcount, w1 = mug for nouns; Nw = chunk words):")
    for (pha, key), n in sorted(by_pk.items(), key=lambda x: -x[1])[:25]:
        print(f"    {pha + ' ' + key:<30}{n:>9}  {pct(n, tot_patch)}")

    # ---- per-event overlap -------------------------------------------------
    print("\n== per-task pages changed, per snapshot interval ==")
    print("  union = pages changed by any task; sum = total over tasks;")
    print("  if union ~ mean, tasks touch the same pages and batching can't help")
    tot_u = tot_s = tot_n = 0
    us = []
    rows = []
    for i in intervals:
        evs = [e for e in i["events"] if e["tag"] == "poke"]
        if not evs:
            continue
        union = set()
        for e in evs:
            union |= e["pages"]
        s = sum(e["n"] for e in evs)
        mean = s / len(evs)
        mx = max(e["n"] for e in evs)
        tot_u += len(union)
        tot_s += s
        tot_n += len(evs)
        us.extend(e["us"] for e in i["events"])
        rows.append((i["eve"], len(evs), len(union), s, mean, mx, i["patch"]))
    if tot_n:
        ratios = [r[2] / r[4] for r in rows if r[4]]
        print(f"  pokes: {tot_n}   mean changed/poke: {tot_s / tot_n:.1f}   "
              f"sum/union: {tot_s / max(tot_u, 1):.2f}   "
              f"union/mean, averaged over intervals: {sum(ratios) / len(ratios):.2f}")
        if us:
            us.sort()
            print(f"  hashing overhead per task: median {us[len(us) // 2] / 1000:.1f} ms, "
                  f"max {us[-1] / 1000:.1f} ms")
        if show_intervals:
            print(f"  {'eve':>10} {'pokes':>6} {'union':>7} {'sum':>8} {'mean':>7} {'max':>6} {'patch':>7}")
            for r in rows:
                print(f"  {r[0]:>10} {r[1]:>6} {r[2]:>7} {r[3]:>8} {r[4]:>7.1f} {r[5]:>6} {r[6]:>7}")
    else:
        print("  no pokes recorded (per-task hashing is off unless U3_MEASURE_HASH=1)")

    # ---- road exits --------------------------------------------------------
    print("\n== road exits, per promoted table ==")
    print(f"  {'table':<6} {'exits->home':>12} {'empty':>7} {'avg entries':>12}"
          f"   {'exits(all)':>11} {'empty':>7} {'avg entries':>12}")
    tabs = ["war", "cod", "han", "bas", "byc", "per", "for", "dup"]
    for t in tabs:
        eh = nh = ch = ea = na = ca = 0
        for i in intervals:
            r = i["R"].get(t)
            if r:
                eh += r[0]; nh += r[1]; ch += r[2]
                ea += r[3]; na += r[4]; ca += r[5]
        print(f"  {t:<6} {eh:>12} {pct(nh, eh):>7} {ch / eh if eh else 0:>12.1f}"
              f"   {ea:>11} {pct(na, ea):>7} {ca / ea if ea else 0:>12.1f}")

    # ---- kicks (jb/measure only) -------------------------------------------
    kis = [i for i in intervals if i["K"]]
    if kis:
        k = [sum(i["K"][j] for i in kis) for j in range(4)]
        tk = sum(k)
        print("\n== nock 9 kicks through call sites ==")
        print(f"  total {tk}")
        for name, n in zip(["jet fired", "jet declined", "registered core, no jet at axis",
                            "unregistered core"], k):
            print(f"    {name:<34}{n:>12}  {pct(n, tk)}")


    mis = [i for i in intervals if i["M"]]
    if mis:
        mib = lambda b: f"{b / (1 << 20):.0f} MB"
        print("\n== process memory at each snapshot ==")
        print("  (anonymous memory can only be swapped; file-backed loom pages")
        print("  can be dropped and read back. on macOS, swap is compressed memory)")
        print("                  min      median         max        last")
        for j, name in enumerate(["resident", "anonymous", "file-backed", "swapped"]):
            v = [i["M"][j] for i in mis]
            sv = sorted(v)
            print(f"  {name:<12}{mib(sv[0]):>9}{mib(sv[len(sv) // 2]):>12}"
                  f"{mib(sv[-1]):>12}{mib(v[-1]):>12}")

    ais = [i for i in intervals if i["A"]]
    if ais:
        a = [sum(i["A"][j] for i in ais) for j in range(4)]
        pat = sum(i["patch"] for i in ais)
        print("\n== page records and allocation (home road) ==")
        print(f"  frees that dirtied a clean page via its page record: {a[0]} "
              f"({pct(a[0], pat).strip()} of patch pages)")
        print(f"    of those pages, later allocated into: {a[3]} ({pct(a[3], a[0]).strip()})")
        print(f"  clean pages first written by a chunk allocation: {a[1]} "
              f"({pct(a[1], pat).strip()} of patch pages)")
        print(f"    while a dirty page in the same class had room: {a[2]} "
              f"({pct(a[2], a[1]).strip()}; {pct(a[2], pat).strip()} of patch pages)")

    # ---- home tables and reclaims ------------------------------------------
    tis = [i for i in intervals if i["T"]]
    if tis:
        print("\n== home-road table entries at each snapshot ==")
        print(f"  {'table':<6} {'min':>9} {'median':>9} {'max':>9} {'first':>9} {'last':>9}")
        names = ["war", "cod", "han", "bas", "byc", "per", "for", "dup", "memo", "wish"]
        for j, name in enumerate(names):
            if any(len(i["T"]) <= j for i in tis):
                continue  # older logs lack later columns
            v = [i["T"][j] for i in tis]
            sv = sorted(v)
            print(f"  {name:<6} {sv[0]:>9} {sv[len(sv) // 2]:>9} {sv[-1]:>9} {v[0]:>9} {v[-1]:>9}")
    cis = [i for i in intervals if i["C"]]
    if cis:
        rec = sum(i["C"][0] for i in cis)
        tasks = sum(i["tasks"] for i in cis)
        byc = sum(i["C"][1] for i in cis)
        han = sum(i["C"][2] for i in cis)
        #  older logs, from the on-loom set's periodic prune
        pre = sum(i["C"][3] for i in cis if len(i["C"]) > 4)
        pru = sum(i["C"][4] for i in cis if len(i["C"]) > 4)
        print("\n== u3m_reclaim() ==")
        print(f"  reclaims: {rec}   tasks per reclaim: {tasks / rec if rec else 0:.0f}")
        if rec:
            print(f"  per reclaim: bytecode entries cleared {byc / rec:.0f}, "
                  f"hank entries cleared {han / rec:.0f}")
        if rec and pre:
            print(f"  dedup prune: {pre / rec:.0f} entries before, "
                  f"{pru / rec:.0f} removed ({pct(pru, pre).strip()})")

    pis = [i for i in intervals if i["P"]]
    if pis:
        tasks = sum(i["tasks"] for i in pis)
        cmp = sum(i["P"][4] for i in pis)
        add = sum(i["P"][5] for i in pis)
        print("\n== bytecode programs (home road) ==")
        print("  per snapshot:     programs       bytes   dead keys   looked up")
        for lab, f in (("first", lambda v: v[0]), ("median", lambda v: sorted(v)[len(v) // 2]),
                       ("max", max), ("last", lambda v: v[-1])):
            row = [f([i["P"][k] for i in pis]) for k in range(4)]
            print(f"  {lab:<14}{row[0]:>12}{row[1]:>12}{row[2]:>12}{row[3]:>12}")
        last = pis[-1]["P"]
        print(f"  last: {last[1] / last[0] if last[0] else 0:.0f} bytes per program, "
              f"{pct(last[2], last[0]).strip()} with a dead key, "
              f"{pct(last[3], last[0]).strip()} looked up in the last interval")
        print(f"  compiled: {cmp} ({cmp / tasks if tasks else 0:.2f} per task); "
              f"added to the home table: {add} ({add / tasks if tasks else 0:.2f} per task)")
    his = [i for i in intervals if i["H"]]
    if his:
        cel = [i["H"][1] for i in his]
        scel = sorted(cel)
        mib = lambda b: f"{b / (1 << 20):.0f} MB"
        add = sum(i["H"][3] for i in his)
        dele = sum(i["H"][4] for i in his)
        tasks = sum(i["tasks"] for i in his)
        print("\n== home-road hashcons set ==")
        print(f"  cells: min {scel[0]}, median {scel[len(scel) // 2]}, max {scel[-1]}, "
              f"first {cel[0]}, last {cel[-1]}")
        print(f"  atoms: last {his[-1]['H'][0]}   "
              f"memory: first {mib(his[0]['H'][2])}, last {mib(his[-1]['H'][2])}")
        print(f"  added {add}, removed {dele} "
              f"({add / tasks if tasks else 0:.1f} and {dele / tasks if tasks else 0:.1f} per task)")
    dis = [i for i in intervals if i["D"]]
    if dis:
        D = [sum(i["D"][k] for i in dis) for k in range(7)]
        last = dis[-1]["D"]
        tasks = sum(i["tasks"] for i in dis)
        pro = sum(D[0:3])
        hit = sum(D[3:7])
        per = lambda x: f"{x / tasks if tasks else 0:.1f}"
        print(f"  probes: {pro} ({per(pro)} per task): road-exit take {D[0]}, "
              f"older home nouns in it {D[1]}, u3m_dedup {D[2]}")
        print(f"  hits: {hit} ({pct(hit, pro).strip()} of probes), by the member's origin:")
        for name, c in zip(["added since the last rebuild", "caches (per for byc)",
                            "warm jet state (war bas hot)", "cold state alone (cod)"],
                           D[3:7]):
            print(f"    {name:<30}{c:>12}  {pct(c, hit)}")
        cel = dis[-1]["H"][1] if dis[-1]["H"] else 0
        if last[7]:
            print(f"  cell buckets: {last[7]} ({pct(cel, last[7]).strip()} full)")
        pgs = [i["D"][8] for i in dis]
        if any(pgs):
            sp = sorted(pgs)
            print(f"  pages an on-loom cell table would write per interval "
                  f"(by home bucket): median {sp[len(sp) // 2]}, max {sp[-1]}")
    if rebuilds:
        n = sum(h[5] for h, _ in rebuilds)
        ms = sum(h[6] for h, _ in rebuilds)
        print(f"  rebuilds: {n}, mean {ms / n:.0f} ms")
        h, d = rebuilds[-1]
        if d:
            print(f"  last rebuild, page faults: {d[9]} major, {d[10]} minor")
        last = h[7:]
        tot = sum(last)
        names = ["per", "for", "byc", "war", "bas", "hot", "cod"]
        print("  last rebuild, cells added by root (in walk order; shared cells"
              " count once, so cod counts only what it alone reaches):")
        for name, c in zip(names, last):
            print(f"    {name:<6}{c:>12}  {pct(c, tot)}")
    gis = [i for i in intervals if i["G"]]
    if gis and any(i["G"][0] for i in gis):
        win, gan, nou, _, can, spl, rit, ent, shr = (
            sum(i["G"][j] for i in gis) for j in range(9))
        mx = max(i["G"][3] for i in gis)
        print("\n== deferred gains across the state swap ==")
        print(f"  windows: {win}   gains deferred: {gan} "
              f"({gan / win if win else 0:.0f} per window, on {nou / win if win else 0:.0f} nouns)")
        print(f"  cancelled by a lose: {can}  {pct(can, gan)}")
        print(f"  written at close: {rit}  {pct(rit, gan)}  on {ent} nouns")
        print(f"  written directly, table at its cap: {spl}  {pct(spl, gan + spl)}")
        print(f"  most nouns pending in one window: {mx}   table shrinks: {shr}")
    yis = [i for i in intervals if i["Y"]]
    if yis and any(i["Y"][0] for i in yis):
        y = [sum(i["Y"][j] for i in yis) for j in range(3)]
        print("\n== new arvo state's alignment with the old ==")
        print(f"  home nouns referenced from the new state's new cells: {y[0]}")
        print(f"    same noun at the same position in the old state: {y[1]}  {pct(y[1], y[0])}")
        print(f"      and below old cells with a refcount of 1: {y[2]}  {pct(y[2], y[0])}")

    # ---- formula / battery identity ----------------------------------------
    have_b = any(i["B"] for i in intervals)
    if not have_b:
        return
    print("\n== bytecode lookups (_n_find), by formula pointer ==")
    print("  ptr = table key is this formula; copy = table key is an equal formula")
    print("  elsewhere (cur: formula on the current road, sen: formula senior to it)")
    names = ["cur-ptr", "cur-copy/cur", "cur-copy/sen",
             "anc-ptr", "anc-copy/cur", "anc-copy/sen", "miss"]
    for road in ("home", "inner"):
        for src in ("nock2", "site"):
            v = [sum(i["B"].get((road, src), (0,) * 7)[j] for i in intervals)
                 for j in range(7)]
            t = sum(v)
            if not t:
                continue
            print(f"  {road}/{src}: total {t}")
            for name, n in zip(names, v):
                print(f"    {name:<16}{n:>14}  {pct(n, t)}")
    print("\n== call-site battery checks (_cj_site_lock) ==")
    names = ["same pointer", "copy, on road", "copy, senior", "different", "empty site"]
    for road in ("home", "inner"):
        v = [sum(i["L"].get(road, (0,) * 5)[j] for i in intervals) for j in range(5)]
        t = sum(v)
        if not t:
            continue
        print(f"  {road}: total {t}")
        for name, n in zip(names, v):
            print(f"    {name:<16}{n:>14}  {pct(n, t)}")


if __name__ == "__main__":
    main()
