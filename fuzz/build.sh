#!/usr/bin/env bash
# Build fuzz harnesses with afl-clang-fast.
#
# Hybrid build (Option 3 in doc/FUZZING.md §4):
#   - Third-party dep libraries come from `zig build -Dfuzz`. They are
#     uninstrumented. Run that once before calling this script.
#   - Vere's own pkg/* sources are recompiled here with afl-clang-fast
#     so they get AFL++ coverage instrumentation plus ASan+UBSan.
#
# Usage:
#   ./fuzz/build.sh              # build every harness
#   ./fuzz/build.sh fuzz_ur_cue  # build one harness
#
# Output:
#   fuzz/out/<name>.afl       instrumented binary for afl-fuzz
#   fuzz/out/<name>.cmplog    CMPLOG variant for -c

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FUZZ_DIR="$ROOT/fuzz"
OUT_DIR="$FUZZ_DIR/out"
OBJ_DIR="$FUZZ_DIR/out/obj"
ZIG_LIB="$ROOT/zig-out/lib"

mkdir -p "$OUT_DIR" "$OBJ_DIR"

CC="${CC:-afl-clang-fast}"
if ! command -v "$CC" >/dev/null 2>&1; then
  echo "error: $CC not found. Install afl++ (sudo apt install afl++)." >&2
  exit 1
fi

# -----------------------------------------------------------------------
# Common flags
# -----------------------------------------------------------------------

# -DU3_FUZZ is observed by pkg/noun/manage.c to skip installing the
# libsigsegv/Windows exception handlers; see doc/FUZZING.md §3.1.
#
# -fsanitize=address,undefined catches memory safety and UB bugs.
# -fno-sanitize-recover=all makes any UBSan report fatal so AFL sees it.
# -fno-omit-frame-pointer is required for good ASan stack traces.
# -O1 + -g is the AFL++ recommended default for fuzz builds.

COMMON_CFLAGS=(
  -O1 -g
  -fno-omit-frame-pointer
  -fsanitize=address,undefined
  -fno-sanitize=pointer-overflow
  -fno-sanitize-recover=all
  -DU3_FUZZ
  -DASAN_ENABLED
  -Wall
  -Wno-unused-variable
  -Wno-unused-but-set-variable
  -Wno-unused-function
  -Wno-unknown-warning-option
  -Wno-deprecated-non-prototype
  -Wno-macro-redefined
  -Wno-gnu-binary-literal
  -Wno-gnu-empty-initializer
  -Wno-gnu-statement-expression
  -Wno-gnu
  -fms-extensions
  -DU3_OS_linux=1
  -DU3_OS_ENDIAN_little=1
  -DU3_OS_PROF=1
  -DENT_GETENTROPY_UNISTD
  -DU3_GUARD_PAGE
)

COMMON_LDFLAGS=(
  -fsanitize=address,undefined
  -fno-sanitize=pointer-overflow
  -fno-sanitize-recover=all
)

# -----------------------------------------------------------------------
# Dep library resolution
# -----------------------------------------------------------------------

require_zig_lib() {
  local name="$1"
  if [[ ! -f "$ZIG_LIB/lib${name}.a" ]]; then
    echo "error: $ZIG_LIB/lib${name}.a missing — run 'zig build -Dfuzz' first" >&2
    exit 1
  fi
}

# find_cached_lib <name>: print the path to the newest .zig-cache copy
# of lib<name>.a. Used for transitive crypto deps of urcrypt that we
# don't install into zig-out/lib.
find_cached_lib() {
  local name="$1"
  local cache="$ROOT/.zig-cache/o"
  if [[ ! -d "$cache" ]]; then
    echo "error: $cache missing — run 'zig build -Dfuzz' first" >&2
    exit 1
  fi
  local newest
  newest=$(find "$cache" -name "lib${name}.a" -printf '%T@ %p\n' 2>/dev/null |
           sort -rn | head -1 | awk '{print $2}')
  if [[ -z "$newest" ]]; then
    echo "error: lib${name}.a not found in $cache — run 'zig build -Dfuzz' first" >&2
    exit 1
  fi
  echo "$newest"
}

# -----------------------------------------------------------------------
# Shared bundles
# -----------------------------------------------------------------------
#
# Harnesses come in two flavours:
#
#   1. ur-only  — H1..H3. pkg/ur has no transitive deps beyond ext/murmur3,
#                 so we compile everything from source with afl-clang-fast.
#
#   2. noun     — H4+. pkg/noun pulls in gmp, openssl, urcrypt, softfloat,
#                 etc. Recompiling all of that under afl-clang-fast takes
#                 several minutes AND instruments way more code than we
#                 want the fuzzer to explore. Instead we:
#                   * link against zig-out/lib/libnoun.a (built with
#                     -DU3_FUZZ so signal handlers are gated, but
#                     WITHOUT AFL instrumentation),
#                   * compile our own afl-instrumented pkg/ur sources
#                     (so ur symbols override libnoun.a's unresolved
#                     references),
#                   * link all the zig-built dep archives uninstrumented.
#
#                 Tradeoff: the fuzzer gets coverage feedback in pkg/ur
#                 but NOT in pkg/noun. ASan still catches heap / stack
#                 memory bugs in noun via shadow memory, so real
#                 memory-safety issues surface as crashes regardless.
#                 If noun-level coverage becomes the bottleneck we
#                 switch H4+ to full in-source recompilation.

UR_SRC=(
  "$ROOT/pkg/ur/bitstream.c"
  "$ROOT/pkg/ur/hashcons.c"
  "$ROOT/pkg/ur/serial.c"
  "$ROOT/ext/murmur3/vendor/murmur3.c"
)

UR_INC=(
  -I "$ROOT/pkg/ur"
  -I "$ROOT/ext/murmur3/vendor"
)

# For noun harnesses: same ur sources (we want coverage here), plus
# include paths for the noun/c3 headers.
NOUN_INC=(
  # Source tree comes FIRST so pkg/noun/version.h (defines u3v_version)
  # wins over zig-out/include/version.h (binary version string, same
  # name but different header guard — if the zig-out one is picked up
  # first, vortex.h fails to compile).
  -I "$ROOT/pkg"
  -I "$ROOT/pkg/ur"
  -I "$ROOT/ext/murmur3/vendor"
  -I "$ROOT/pkg/noun"
  -I "$ROOT/pkg/noun/platform/linux"
  -I "$ROOT/pkg/c3"
  -I "$ROOT/pkg/ent"
  # zig-out/include is a fallback for generated headers that don't
  # exist in the source tree — gmp.h, softfloat.h, ent/ent.h.
  -I "$ROOT/zig-out/include"
)

# Libraries resolved later by build_noun_harness — some come from
# zig-out/lib (installed by -Dfuzz), others come from .zig-cache
# because they're transitive deps of urcrypt/noun that we don't
# install explicitly (secp256k1, scrypt, ed25519, etc.). The whole
# set is wrapped in --start-group/--end-group so link order doesn't
# matter.
#
# (populated in build_noun_harness below)

# -----------------------------------------------------------------------
# Compile+link helper
# -----------------------------------------------------------------------
#
# build_harness <name> <src_var_name> <inc_var_name>
#
# Expects two bash array variables to exist by name: one with the list
# of extra source files to compile with the harness, one with include
# dirs. Produces $OUT_DIR/<name>.{afl,cmplog}.
build_harness() {
  local name="$1"
  local -n src_arr="$2"
  local -n inc_arr="$3"
  local harness="$FUZZ_DIR/harnesses/${name}.c"

  if [[ ! -f "$harness" ]]; then
    echo "error: harness source $harness not found" >&2
    exit 1
  fi

  local all_src=("$harness" "${src_arr[@]}")

  echo "=> building $name (plain)"
  $CC "${COMMON_CFLAGS[@]}" "${inc_arr[@]}" \
      "${all_src[@]}" \
      "${COMMON_LDFLAGS[@]}" \
      -o "$OUT_DIR/${name}.afl"

  echo "=> building $name (cmplog)"
  AFL_LLVM_CMPLOG=1 \
  $CC "${COMMON_CFLAGS[@]}" "${inc_arr[@]}" \
      "${all_src[@]}" \
      "${COMMON_LDFLAGS[@]}" \
      -o "$OUT_DIR/${name}.cmplog"
}

# build_noun_harness <name>
#
# Noun-flavour build: compiles the harness + afl-instrumented pkg/ur
# sources, links against zig-out/lib static archives (libnoun.a et al)
# plus transitive crypto deps discovered from .zig-cache.
#
# All libraries are wrapped in --start-group/--end-group so the
# single-pass linker can resolve cyclic refs without manual ordering.
build_noun_harness() {
  local name="$1"
  local harness="$FUZZ_DIR/harnesses/${name}.c"

  if [[ ! -f "$harness" ]]; then
    echo "error: harness source $harness not found" >&2
    exit 1
  fi

  # Ensure zig-built deps are in place.
  require_zig_lib noun
  require_zig_lib gmp

  local all_src=("$harness" "${UR_SRC[@]}")

  # Installed zig-out libs (known stable paths).
  local installed_libs=(
    "$ZIG_LIB/libnoun.a"
    "$ZIG_LIB/libpast.a"
    "$ZIG_LIB/libc3.a"
    "$ZIG_LIB/libent.a"
    "$ZIG_LIB/libgmp.a"
    "$ZIG_LIB/libmurmur3.a"
    "$ZIG_LIB/libssl.a"
    "$ZIG_LIB/libcrypto.a"
    "$ZIG_LIB/liburcrypt.a"
    "$ZIG_LIB/libwasm3.a"
    "$ZIG_LIB/libwhereami.a"
    "$ZIG_LIB/libpdjson.a"
    "$ZIG_LIB/libsoftblas.a"
    "$ZIG_LIB/libsoftfloat.a"
    "$ZIG_LIB/libbacktrace.a"
    "$ZIG_LIB/libunwind.a"
    "$ZIG_LIB/libz.a"
    "$ZIG_LIB/libsigsegv.a"
  )

  # Transitive deps of urcrypt that we pull directly from .zig-cache
  # rather than installing. These are all tiny crypto primitives.
  local cached_lib_names=(
    secp256k1
    argon2
    blake3
    ed25519
    ge_additions
    keccak_tiny
    monocypher
    scrypt
    aes_siv
    cifra
    micro_ecc
  )
  local cached_libs=()
  for n in "${cached_lib_names[@]}"; do
    cached_libs+=("$(find_cached_lib "$n")")
  done

  local sys_libs=(-lpthread -ldl -lm -lrt)

  echo "=> building $name (plain, noun)"
  $CC "${COMMON_CFLAGS[@]}" "${NOUN_INC[@]}" \
      "${all_src[@]}" \
      -Wl,--start-group \
      "${installed_libs[@]}" \
      "${cached_libs[@]}" \
      -Wl,--end-group \
      "${sys_libs[@]}" \
      "${COMMON_LDFLAGS[@]}" \
      -o "$OUT_DIR/${name}.afl"

  echo "=> building $name (cmplog, noun)"
  AFL_LLVM_CMPLOG=1 \
  $CC "${COMMON_CFLAGS[@]}" "${NOUN_INC[@]}" \
      "${all_src[@]}" \
      -Wl,--start-group \
      "${installed_libs[@]}" \
      "${cached_libs[@]}" \
      -Wl,--end-group \
      "${sys_libs[@]}" \
      "${COMMON_LDFLAGS[@]}" \
      -o "$OUT_DIR/${name}.cmplog"
}

# -----------------------------------------------------------------------
# Per-harness recipes
# -----------------------------------------------------------------------

# --- H1: fuzz_ur_cue --------------------------------------------------
# Targets ur_cue_with — off-loom cue parser with hashcons root + dict.
build_fuzz_ur_cue() {
  build_harness "fuzz_ur_cue" UR_SRC UR_INC
}

# --- H2: fuzz_ur_cue_test ---------------------------------------------
# Targets ur_cue_test_with — parse-only, no allocation, ~5x throughput
# of H1. Catches bitstream bugs that allocation masks.
build_fuzz_ur_cue_test() {
  build_harness "fuzz_ur_cue_test" UR_SRC UR_INC
}

# --- H3: fuzz_ur_jam_cue_diff -----------------------------------------
# Differential: cue(input) -> jam(result) -> cue(jammed) -> compare.
# Fails if the round-trip doesn't preserve the noun. Catches
# encoder/decoder drift and hashcons bugs.
build_fuzz_ur_jam_cue_diff() {
  build_harness "fuzz_ur_jam_cue_diff" UR_SRC UR_INC
}

# --- H4: fuzz_u3_cue_bytes --------------------------------------------
# Targets u3s_cue_bytes — u3's noun-level cue. First hybrid-build
# harness: needs libnoun.a from zig-out/lib plus its transitive deps.
build_fuzz_u3_cue_bytes() {
  build_noun_harness "fuzz_u3_cue_bytes"
}

# --- H5: fuzz_u3_cue_xeno ---------------------------------------------
# Targets u3s_cue_xeno_with — off-loom cue used by ames/mesa/conn.
build_fuzz_u3_cue_xeno() {
  build_noun_harness "fuzz_u3_cue_xeno"
}

# build_vere_harness <name> [extra_src ...]
#
# Vere-flavour build: noun harness + libuv/lmdb headers/libs +
# additional pkg/vere source files that the harness compiles directly.
# Uses --gc-sections to drop unused functions from those files (so we
# don't have to satisfy every transitive symbol reference in pkg/vere).
#
# Environment:
#   VERE_HARNESS_NO_LIBVERE=1  — skip linking libvere.a, e.g. when
#     the harness #includes a pkg/vere/io/*.c file directly (ames)
#     and linking libvere.a would duplicate symbols.
build_vere_harness() {
  local name="$1"
  shift
  local extra_src=("$@")
  local harness="$FUZZ_DIR/harnesses/${name}.c"

  if [[ ! -f "$harness" ]]; then
    echo "error: harness source $harness not found" >&2
    exit 1
  fi

  require_zig_lib noun
  require_zig_lib libuv
  require_zig_lib lmdb

  # pkg/vere has its own vere.h that conflicts with zig-out/include/vere.h
  # (the latter is a generated copy with stale guards). Put pkg/vere
  # paths FIRST so source-tree headers win over the installed copies,
  # same logic as NOUN_INC's source-first ordering.
  local vere_inc=(
    -I "$ROOT/pkg/vere"
    -I "$ROOT/pkg/vere/db"
    -I "$ROOT/pkg/vere/platform/linux"
    "${NOUN_INC[@]}"
  )

  # Same dep stack as the noun build, plus libuv, lmdb, and libvere
  # itself (so the harness can use ivory pill, mesa pact_test
  # helpers, etc. from prebuilt vere objects).
  local installed_libs=()
  if [[ "${VERE_HARNESS_NO_LIBVERE:-}" != "1" ]]; then
    installed_libs+=("$ZIG_LIB/libvere.a")
  fi
  installed_libs+=(
    "$ZIG_LIB/libnoun.a"
    "$ZIG_LIB/libpast.a"
    "$ZIG_LIB/libc3.a"
    "$ZIG_LIB/libent.a"
    "$ZIG_LIB/libgmp.a"
    "$ZIG_LIB/libmurmur3.a"
    "$ZIG_LIB/libssl.a"
    "$ZIG_LIB/libcrypto.a"
    "$ZIG_LIB/liburcrypt.a"
    "$ZIG_LIB/libwasm3.a"
    "$ZIG_LIB/libwhereami.a"
    "$ZIG_LIB/libpdjson.a"
    "$ZIG_LIB/libsoftblas.a"
    "$ZIG_LIB/libsoftfloat.a"
    "$ZIG_LIB/libbacktrace.a"
    "$ZIG_LIB/libunwind.a"
    "$ZIG_LIB/libz.a"
    "$ZIG_LIB/libsigsegv.a"
    "$ZIG_LIB/liblibuv.a"
    "$ZIG_LIB/liblmdb.a"
    "$ZIG_LIB/libnatpmp.a"
  )
  # libvere additionally references h2o / curl / dns-sd / picohttp /
  # picotls / libgkc / dbus / expat (transitive through h2o). These
  # are not in our zig-out/lib install list but live in .zig-cache.
  local cached_lib_names=(
    secp256k1 argon2 blake3 ed25519 ge_additions
    keccak_tiny monocypher scrypt aes_siv cifra micro_ecc
    h2o curl picohttpparser picotls libgkc dns-sd dbus-1 expat
  )
  local cached_libs=()
  for n in "${cached_lib_names[@]}"; do
    cached_libs+=("$(find_cached_lib "$n")")
  done

  # compat_strlcpy.c provides strlcpy/strlcat stubs for avahi-common,
  # which is pulled in transitively by libvere's mdns code but expects
  # glibc ≥2.38. The fuzz fleet target is ≥2.35 (Ubuntu 22.04).
  local all_src=("$harness" "$FUZZ_DIR/harnesses/compat_strlcpy.c" "${UR_SRC[@]}" "${extra_src[@]}")
  local sys_libs=(-lpthread -ldl -lm -lrt)

  # -ffunction-sections + -Wl,--gc-sections drops unused functions
  # from extra_src. newt.c et al. include status/info functions that
  # would otherwise pull in pkg/vere pier symbols we don't link.
  #
  # -Wl,-z,muldefs lets harnesses that #include a pkg/vere/io/*.c
  # file in-tree still link libvere.a for the many OTHER symbols
  # that file references. Without -z muldefs, the harness's copy of
  # the #included functions collides with libvere.a's copy.
  local extra_cflags=(-ffunction-sections -fdata-sections)
  local extra_ldflags=(-Wl,--gc-sections -Wl,-z,muldefs)

  echo "=> building $name (plain, vere)"
  $CC "${COMMON_CFLAGS[@]}" "${extra_cflags[@]}" "${vere_inc[@]}" \
      "${all_src[@]}" \
      -Wl,--start-group \
      "${installed_libs[@]}" \
      "${cached_libs[@]}" \
      -Wl,--end-group \
      "${sys_libs[@]}" \
      "${extra_ldflags[@]}" \
      "${COMMON_LDFLAGS[@]}" \
      -o "$OUT_DIR/${name}.afl"

  echo "=> building $name (cmplog, vere)"
  AFL_LLVM_CMPLOG=1 \
  $CC "${COMMON_CFLAGS[@]}" "${extra_cflags[@]}" "${vere_inc[@]}" \
      "${all_src[@]}" \
      -Wl,--start-group \
      "${installed_libs[@]}" \
      "${cached_libs[@]}" \
      -Wl,--end-group \
      "${sys_libs[@]}" \
      "${extra_ldflags[@]}" \
      "${COMMON_LDFLAGS[@]}" \
      -o "$OUT_DIR/${name}.cmplog"
}

# --- H6: fuzz_newt_decode ---------------------------------------------
# Targets u3_newt_decode — IPC framing parser. Compiles newt.c into
# the harness so we can call the static helpers; --gc-sections drops
# the rest of newt.c that we don't reach.
build_fuzz_newt_decode() {
  build_vere_harness "fuzz_newt_decode" "$ROOT/pkg/vere/newt.c"
}

# --- H7: fuzz_mesa_sift_pact ------------------------------------------
# Targets mesa_sift_pact_from_buf — mesa packet parser. Compiles
# pact.c directly into the harness so local edits (e.g. PR 998)
# apply without needing to rebuild libvere.a. libvere's ivory pill
# data and other transitive symbols still come from zig-out/lib.
# -z muldefs lets the in-harness pact.o win over libvere.a's copy.
build_fuzz_mesa_sift_pact() {
  build_vere_harness "fuzz_mesa_sift_pact" \
    "$ROOT/pkg/vere/io/mesa/pact.c"
}

# --- H8: fuzz_ames_sift_packet ----------------------------------------
# Targets the ames packet sifters. Uses #include "./io/ames.c" to
# expose static helpers, same as ames_tests.c. Links libvere for the
# transitive symbols ames.c references (u3_Host, u3_ovum_*, mdns_*);
# -z muldefs in build_vere_harness lets the #include'd copies win.
build_fuzz_ames_sift_packet() {
  build_vere_harness "fuzz_ames_sift_packet"
}

# --- H9: fuzz_stun_response -------------------------------------------
# Targets u3_stun_is_request, u3_stun_is_our_response,
# u3_stun_find_xor_mapped_address — all public symbols already in
# libvere.a.
build_fuzz_stun_response() {
  build_vere_harness "fuzz_stun_response"
}

# --- H10: fuzz_lss_ingest ---------------------------------------------
# Targets lss_builder_ingest — content-integrity proofs used by mesa.
build_fuzz_lss_ingest() {
  build_vere_harness "fuzz_lss_ingest"
}

# --- H11: fuzz_disk_sift ----------------------------------------------
# Targets u3_disk_sift — event log replay parser. Each event is
# [4-byte mug][jam bytes]. u3_disk_sift is already in libvere.a.
build_fuzz_disk_sift() {
  build_vere_harness "fuzz_disk_sift"
}

# --- H12: fuzz_ce_patch_control ---------------------------------------
# Targets _ce_patch_read_control + _ce_patch_verify (snapshot patch
# verifier). Uses #include "events.c" so the static helpers are
# callable directly; -z muldefs in build_vere_harness lets the
# in-harness copies override libnoun.a's.
build_fuzz_ce_patch_control() {
  build_vere_harness "fuzz_ce_patch_control"
}

# --- H13: fuzz_past_v4_load -------------------------------------------
# Targets u3_v4_load + u3a_v4_ream (past v4 snapshot loader). The
# harness mmap's a v4 loom region MAP_FIXED at the expected address.
# Links libpast.a from zig-out/lib (already installed by -Dfuzz).
build_fuzz_past_v4_load() {
  build_vere_harness "fuzz_past_v4_load"
}

# --- H15: fuzz_conn_poke ----------------------------------------------
# Mirrors the cue + structural validation in
# pkg/vere/io/conn.c:_conn_moor_poke. Doesn't call into pier code.
build_fuzz_conn_poke() {
  build_noun_harness "fuzz_conn_poke"
}

# --- H16: fuzz_scot_slaw ----------------------------------------------
# Targets Hoon's atom-text parsers in jets/e/slaw.c. Uses the
# #include trick to expose the static sub-parsers; muldefs handles
# the libnoun.a collision.
build_fuzz_scot_slaw() {
  build_vere_harness "fuzz_scot_slaw"
}

# --- H19: fuzz_zlib_de ------------------------------------------------
# Targets u3qe_decompress_gzip / u3qe_decompress_zlib — public entries
# in libnoun.a. Compiles zlib.c into the harness for instrumentation.
build_fuzz_zlib_de() {
  build_vere_harness "fuzz_zlib_de" \
    "$ROOT/pkg/noun/jets/e/zlib.c"
}

# --- H20: fuzz_base_decode --------------------------------------------
# Targets u3qe_de_base16 — base-16 (hex) decoder. Compiles base.c
# into the harness so the decoder branches are instrumented.
build_fuzz_base_decode() {
  build_vere_harness "fuzz_base_decode" \
    "$ROOT/pkg/noun/jets/e/base.c"
}

# --- H21: fuzz_mesa_bitset --------------------------------------------
# Targets pkg/vere/io/mesa/bitset.c — small fragment bitmap used by
# mesa. Compiled in-harness for instrumentation.
build_fuzz_mesa_bitset() {
  build_vere_harness "fuzz_mesa_bitset" \
    "$ROOT/pkg/vere/io/mesa/bitset.c"
}

# --- H26: fuzz_mesa_page_flow -----------------------------------------
# End-to-end reachability test for the #011/#012 findings. Parses two
# page packets through mesa_sift_pact_from_buf, seeds a bitset from
# the first packet's tob_d, then applies the mesa.c:1132 guard using
# the second packet's tob_d and calls bitset_has/put. If a two-packet
# sequence with mismatched tob_d can trip bitset_has's off-by-one
# assertion, #012 is a real (reachable) DoS vector; otherwise it's
# harness-only API hygiene.
build_fuzz_mesa_page_flow() {
  build_vere_harness "fuzz_mesa_page_flow" \
    "$ROOT/pkg/vere/io/mesa/pact.c" \
    "$ROOT/pkg/vere/io/mesa/bitset.c"
}

# --- H22: fuzz_u3r_sing -----------------------------------------------
# Targets u3r_sing — structural noun equality. Two cues + compare.
# u3r_sing is in libnoun.a so no extra source; just the harness.
build_fuzz_u3r_sing() {
  build_noun_harness "fuzz_u3r_sing"
}

# --- H23: fuzz_fore_inject --------------------------------------------
# Mirrors _fore_inject's cue + shape validation from
# pkg/vere/io/fore.c. Skips the file read and pier-side work.
build_fuzz_fore_inject() {
  build_noun_harness "fuzz_fore_inject"
}

# --- H24: fuzz_http_request -------------------------------------------
# Duplicates the three _http_vec_to_* helpers + _http_heds_to_noun
# from pkg/vere/io/http.c inline. Avoids dragging in h2o + openssl
# transitively. Any refactor of those helpers must be mirrored here.
build_fuzz_http_request() {
  build_noun_harness "fuzz_http_request"
}

# --- H25: fuzz_lick_ipc -----------------------------------------------
# Mirrors _lick_moor_poke shape validation — tiny compared to conn.
build_fuzz_lick_ipc() {
  build_noun_harness "fuzz_lick_ipc"
}

# ======================================================================
# Phase 3: Data-entry sub-parser harnesses (H27+)
# ======================================================================

# --- G1: ames / mesa / stun sub-parsers (all vere-flavor, muldefs) ----
build_fuzz_ames_head()    { build_vere_harness "fuzz_ames_head";    }
build_fuzz_ames_prel()    { build_vere_harness "fuzz_ames_prel";    }
build_fuzz_lane_decode()  { build_vere_harness "fuzz_lane_decode";  }
build_fuzz_fine_wail()    { build_vere_harness "fuzz_fine_wail";    }
build_fuzz_fine_meow()    { build_vere_harness "fuzz_fine_meow";    }
build_fuzz_stun_xor()     { build_vere_harness "fuzz_stun_xor";     }

# --- G2: cttp HTTP-client response parsers (inlined helpers) ----------
build_fuzz_cttp_head()    { build_noun_harness "fuzz_cttp_head";    }
build_fuzz_cttp_body()    { build_noun_harness "fuzz_cttp_body";    }

# --- G3: HTTP server sub-parsers --------------------------------------
build_fuzz_http_range()   { build_noun_harness "fuzz_http_range";   }
build_fuzz_http_cookie()  { build_noun_harness "fuzz_http_cookie";  }
build_fuzz_tls_pem()      { build_noun_harness "fuzz_tls_pem";      }

# --- G6: crypto verifier jets -----------------------------------------
build_fuzz_ed_veri()      { build_noun_harness "fuzz_ed_veri";      }
build_fuzz_secp_reco()    { build_noun_harness "fuzz_secp_reco";    }
build_fuzz_secp_schnorr() { build_noun_harness "fuzz_secp_schnorr"; }
# argon2 #includes jets/e/argon2.c → needs -z muldefs (vere-harness).
build_fuzz_argon2()       { build_vere_harness "fuzz_argon2";       }
build_fuzz_blake2b()      { build_noun_harness "fuzz_blake2b";      }
build_fuzz_aes_siv()      { build_noun_harness "fuzz_aes_siv";      }

# --- G7: text-decoder jets --------------------------------------------
build_fuzz_jet_trip()     { build_noun_harness "fuzz_jet_trip";     }
build_fuzz_jet_leer()     { build_noun_harness "fuzz_jet_leer";     }
build_fuzz_jet_lore()     { build_noun_harness "fuzz_jet_lore";     }

# --- G8: bit/byte manipulation jets -----------------------------------
build_fuzz_jet_cut()      { build_noun_harness "fuzz_jet_cut";      }
build_fuzz_jet_rip_rep()  { build_noun_harness "fuzz_jet_rip_rep";  }
build_fuzz_jet_lsh()      { build_noun_harness "fuzz_jet_lsh";      }
build_fuzz_jet_hew_sew()  { build_noun_harness "fuzz_jet_hew_sew";  }

# --- G9: parser combinators / mink / bytestream -----------------------
build_fuzz_nock_mink()    { build_noun_harness "fuzz_nock_mink";    }
build_fuzz_parse_combi()  { build_noun_harness "fuzz_parse_combi";  }
build_fuzz_bytestream()   { build_noun_harness "fuzz_bytestream";   }

# --- G10: serial.c aura parsers / etchers ------------------------------
build_fuzz_serial_sift_ud()         { build_noun_harness "fuzz_serial_sift_ud";         }
build_fuzz_serial_rtrip()  { build_noun_harness "fuzz_serial_rtrip";  }
build_fuzz_serial_etch_all()        { build_noun_harness "fuzz_serial_etch_all";        }

# --- G11: differential jet fuzzing via ice oracle ---------------------
# Layer 1 (math): forces _cj_kick_z's differential path on add/dec/sub/
# mul/div/mod/gte/gth/lte/lth/max/min/cap/mas/peg/dvr via u3j_fuzz_arm.
# Needs libvere for u3v_wish + the ivory pill boot symbols.
build_fuzz_jet_l1_math()            { build_vere_harness "fuzz_jet_l1_math";            }
build_fuzz_jet_l2_bits()            { build_vere_harness "fuzz_jet_l2_bits";            }
build_fuzz_jet_l3_list()            { build_vere_harness "fuzz_jet_l3_list";            }
build_fuzz_jet_l4a_crypto()         { build_vere_harness "fuzz_jet_l4a_crypto";         }
build_fuzz_jet_l4b_encode()         { build_vere_harness "fuzz_jet_l4b_encode";         }
build_fuzz_jet_l5_float()           { build_vere_harness "fuzz_jet_l5_float";           }
build_fuzz_jet_l6_map()             { build_vere_harness "fuzz_jet_l6_map";             }
build_fuzz_jet_l7_parse()           { build_vere_harness "fuzz_jet_l7_parse";           }
build_fuzz_jet_l8_nock()            { build_vere_harness "fuzz_jet_l8_nock";            }
build_fuzz_jet_l9_mapops()          { build_vere_harness "fuzz_jet_l9_mapops";          }
build_fuzz_jet_l10_crypto()         { build_noun_harness "fuzz_jet_l10_crypto";         }
build_pill_test()                   { build_vere_harness "pill_test";                   }
build_fuzz_jet_l11_base()           { build_vere_harness "fuzz_jet_l11_base";           }
build_fuzz_jet_probe()              { build_vere_harness "fuzz_jet_probe";              }
build_rub_roundtrip()               { build_vere_harness "rub_roundtrip";               }
build_blake2b_probe()               { build_vere_harness "blake2b_probe";               }

# --- H14: fuzz_json_de ------------------------------------------------
# Targets u3qe_json_de — the JSON decoder jet. Highest-yield single
# target per the §2.13 survey. Compiles json_de.c directly into the
# harness so the parser's branches are afl-instrumented; libnoun.a's
# copy is overridden via -z muldefs in build_vere_harness. Also
# compiles pdjson.c (the underlying JSON tokenizer) so its branches
# are instrumented too — without this we miss most of the parser's
# coverage feedback.
build_fuzz_json_de() {
  local pdjson_c
  pdjson_c=$(find "$HOME/.cache/zig" -name 'pdjson.c' 2>/dev/null | head -1)
  if [[ -z "$pdjson_c" ]]; then
    echo "error: pdjson.c not found in ~/.cache/zig — run 'zig build -Dfuzz' first" >&2
    exit 1
  fi
  build_vere_harness "fuzz_json_de" \
    "$ROOT/pkg/noun/jets/e/json_de.c" \
    "$pdjson_c"
}

# -----------------------------------------------------------------------
# Dispatch
# -----------------------------------------------------------------------

HARNESSES=(
  fuzz_ur_cue
  fuzz_ur_cue_test
  fuzz_ur_jam_cue_diff
  fuzz_u3_cue_bytes
  fuzz_u3_cue_xeno
  fuzz_newt_decode
  fuzz_mesa_sift_pact
  fuzz_ames_sift_packet
  fuzz_stun_response
  fuzz_lss_ingest
  fuzz_disk_sift
  fuzz_ce_patch_control
  fuzz_past_v4_load
  fuzz_json_de
  fuzz_conn_poke
  fuzz_scot_slaw
  fuzz_zlib_de
  fuzz_base_decode
  fuzz_mesa_bitset
  fuzz_mesa_page_flow
  fuzz_u3r_sing
  fuzz_fore_inject
  fuzz_http_request
  fuzz_lick_ipc
  # --- Phase 3 data-entry sub-parsers (H27+) ---
  fuzz_ames_head
  fuzz_ames_prel
  fuzz_lane_decode
  fuzz_fine_wail
  fuzz_fine_meow
  fuzz_stun_xor
  fuzz_cttp_head
  fuzz_cttp_body
  fuzz_http_range
  fuzz_http_cookie
  fuzz_tls_pem
  fuzz_ed_veri
  fuzz_secp_reco
  fuzz_secp_schnorr
  fuzz_argon2
  fuzz_blake2b
  fuzz_aes_siv
  fuzz_jet_trip
  fuzz_jet_leer
  fuzz_jet_lore
  fuzz_jet_cut
  fuzz_jet_rip_rep
  fuzz_jet_lsh
  fuzz_jet_hew_sew
  fuzz_nock_mink
  fuzz_parse_combi
  fuzz_bytestream
  # --- Phase 4 serial.c aura parsers / etchers ---
  fuzz_serial_sift_ud
  fuzz_serial_rtrip
  fuzz_serial_etch_all
  # --- Phase 5 differential jet fuzzing ---
  fuzz_jet_l1_math
  fuzz_jet_l2_bits
  fuzz_jet_l3_list
  fuzz_jet_l4a_crypto
  fuzz_jet_l4b_encode
)

build_one() {
  local name="$1"
  if ! declare -f "build_${name}" >/dev/null; then
    echo "error: no recipe for harness '$name'" >&2
    echo "known harnesses: ${HARNESSES[*]}" >&2
    exit 1
  fi
  "build_${name}"
}

if [[ $# -eq 0 ]]; then
  for h in "${HARNESSES[@]}"; do
    build_one "$h"
  done
else
  for h in "$@"; do
    build_one "$h"
  done
fi

echo
echo "built: $(ls "$OUT_DIR"/*.afl 2>/dev/null | wc -l) harness(es) in $OUT_DIR/"
