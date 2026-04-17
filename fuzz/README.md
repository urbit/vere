# Vere fuzz harnesses (AFL++)

Everything here drives AFL++ against the Vere runtime. The plan this
implements is in [`doc/FUZZING.md`](../doc/FUZZING.md).

## Layout

```
fuzz/
  harnesses/        C source for each fuzz harness
  corpus/           seed inputs, one subdir per harness
  dicts/            AFL++ dictionaries per input format
  scripts/          run.sh, replay.sh, triage.sh, cmin.sh
  findings/         crash drops during triage (gitignored)
  regression/       minimised reproducers kept forever
  build.sh          builds every harness via afl-clang-fast
```

## Prerequisites

- `afl++` (Ubuntu: `sudo apt install afl++`) — provides `afl-clang-fast`,
  `afl-fuzz`, `afl-cmin`, `afl-tmin`.
- `clang` (pulled in by afl++).
- A populated `zig-out/lib/` — run `zig build -Dfuzz` from the repo root
  once before building harnesses. That flag installs the uninstrumented
  third-party dependency archives (gmp, libuv, openssl, ...) that
  harnesses link against.

## Building

From the repo root:

```
zig build -Dfuzz              # populate zig-out/lib/ with dep archives
./fuzz/build.sh               # build every harness
./fuzz/build.sh fuzz_ur_cue   # build one harness
```

Each harness produces two binaries under `fuzz/out/`:
- `<name>.afl` — the primary fuzz target, ASan+UBSan, afl-clang-fast
  instrumentation.
- `<name>.cmplog` — same target with `AFL_LLVM_CMPLOG=1` for redqueen.

## Running

```
./fuzz/scripts/run.sh fuzz_ur_cue 8   # 1 main + 7 secondaries
```

See [`doc/FUZZING.md`](../doc/FUZZING.md) §8 for the full workflow.

## Host notes

Fuzzing targets this box: 16 cores, 93 GiB RAM, x86_64 Linux. Before
first run, AFL++ wants:

```
sudo sh -c 'echo core > /proc/sys/kernel/core_pattern'
sudo cpupower frequency-set -g performance   # optional, stabler results
```

`scripts/run.sh` checks these and prints a warning if they're not set.
