#!/usr/bin/env python3
"""Summarize a jb/measure log (<pier>/.urb/measure-<time>.log).

Usage: measure-report.py [--intervals] [--keep-first] LOG|DIR ...

Accepts any number of logs, or directories (a pier, its .urb, or any
directory holding measure-*.log files). Logs are aggregated. Each log
starts at a restart, so by default the first snapshot interval of each
log is dropped (it includes startup); --keep-first keeps it. Tasks
recorded after a log's last snapshot are ignored.

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
  X <copied> <gained> <gained-clean> <net-zero> <net-zero-pages> <free-clean> <alloc-clean> <alloc-alt> <alloc-reuse>
                                            state swap and allocation (jb/measure only)
  T <war> <cod> <han> <bas> <byc> <per> <for> <dup> <memo>
                                            home-road table entries at the snapshot
  C <reclaims> <byc-cleared> <han-cleared> <dup-before-prune> <dup-pruned>
                                            u3m_reclaim() calls since the last snapshot
"""

import glob
import os
import sys
from collections import defaultdict


def pct(a, b):
    return f"{100.0 * a / b:5.1f}%" if b else "    -"


def new_interval():
    return {
        "events": None,
        "patch": 0, "skip": 0, "tasks": 0, "eve": 0,
        "F": defaultdict(int),
        "U": 0,
        "R": {},
        "K": None,
        "B": {},
        "L": {},
        "T": None,
        "X": None,
        "C": None,
    }


def parse(path):
    """Return (header, intervals, trailing-task-count) for one log."""
    intervals = []
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
            elif t == "R" and cur is not None:
                p = line.split()
                cur["R"][p[1]] = tuple(int(x) for x in p[2:])
            elif t == "K" and cur is not None:
                cur["K"] = tuple(int(x) for x in line.split()[1:])
            elif t == "B" and cur is not None:
                p = line.split()
                cur["B"][(p[1], p[2])] = tuple(int(x) for x in p[3:])
            elif t == "X" and cur is not None:
                cur["X"] = tuple(int(x) for x in line.split()[1:])
            elif t == "T" and cur is not None:
                cur["T"] = tuple(int(x) for x in line.split()[1:])
            elif t == "C" and cur is not None:
                cur["C"] = tuple(int(x) for x in line.split()[1:])
            elif t == "L" and cur is not None:
                p = line.split()
                cur["L"][p[1]] = tuple(int(x) for x in p[2:])
    return header, intervals, len(cur_events)


def find_logs(args):
    logs = []
    for a in args:
        if os.path.isdir(a):
            found = (glob.glob(os.path.join(a, "measure-*.log")) or
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

    intervals = []
    headers = set()
    dropped = trailing = 0
    elapsed = 0
    logs = find_logs(args)
    for path in logs:
        hed, ivs, tail = parse(path)
        if hed:
            headers.add(" ".join(w for w in hed.split()
                                 if w.split("=")[0] in ("word_bits", "page_bytes")))
        trailing += tail
        if ivs and not keep_first:
            #  measure elapsed time from the dropped startup snapshot
            elapsed += ivs[-1]["time"] - ivs[0]["time"]
            ivs = ivs[1:]
            dropped += 1
        intervals.extend(ivs)

    header = (f"# {len(logs)} log(s); {' / '.join(sorted(headers))}; "
              f"dropped {dropped} startup interval(s); "
              f"{trailing} task(s) after a log's last snapshot ignored")
    print(header)
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


    # ---- home tables and reclaims ------------------------------------------
    tis = [i for i in intervals if i["T"]]
    if tis:
        print("\n== home-road table entries at each snapshot ==")
        print(f"  {'table':<6} {'min':>9} {'median':>9} {'max':>9} {'first':>9} {'last':>9}")
        names = ["war", "cod", "han", "bas", "byc", "per", "for", "dup", "memo"]
        for j, name in enumerate(names):
            v = [i["T"][j] for i in tis]
            sv = sorted(v)
            print(f"  {name:<6} {sv[0]:>9} {sv[len(sv) // 2]:>9} {sv[-1]:>9} {v[0]:>9} {v[-1]:>9}")
    cis = [i for i in intervals if i["C"]]
    if cis:
        rec = sum(i["C"][0] for i in cis)
        tasks = sum(i["tasks"] for i in cis)
        byc = sum(i["C"][1] for i in cis)
        han = sum(i["C"][2] for i in cis)
        pre = sum(i["C"][3] for i in cis)
        pru = sum(i["C"][4] for i in cis)
        print("\n== u3m_reclaim() ==")
        print(f"  reclaims: {rec}   tasks per reclaim: {tasks / rec if rec else 0:.0f}")
        if rec:
            print(f"  per reclaim: bytecode entries cleared {byc / rec:.0f}, "
                  f"hank entries cleared {han / rec:.0f}")
            print(f"  dedup prune: {pre / rec:.0f} entries before, "
                  f"{pru / rec:.0f} removed ({pct(pru, pre).strip()})")

    # ---- state swap and allocation (jb/measure only) ------------------------
    xis = [i for i in intervals if i["X"]]
    if xis:
        x = [sum(i["X"][j] for i in xis) for j in range(9)]
        pat = sum(i["patch"] for i in xis)
        print("\n== state swap (fused take/lose) ==")
        print(f"  product take: {x[0]} nouns copied, {x[1]} home nouns gained "
              f"({pct(x[2], x[1]).strip()} on pages clean before the gain)")
        print(f"  gains that netted to zero after roc-lose: {x[3]} ({pct(x[3], x[1]).strip()})")
        print(f"  clean pages whose gains all netted to zero: {x[4]} "
              f"({pct(x[4], pat).strip()} of patch pages; upper bound on A5's saving)")
        print("\n== page records and allocation (home road) ==")
        print(f"  frees that dirtied a clean page via its page record: {x[5]} "
              f"({pct(x[5], pat).strip()} of patch pages)")
        print(f"    of those pages, later allocated into: {x[8]} ({pct(x[8], x[5]).strip()})")
        print(f"  chunk allocations that dirtied a clean page: {x[6]}")
        print(f"    while a dirty page in the same class had room: {x[7]} ({pct(x[7], x[6]).strip()})")

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
