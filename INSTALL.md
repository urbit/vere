# Building Vere

We use [`zig`][zig] to build Vere, which is packaged as a single binary,
`urbit`.

## Supported Targets

Main (`-Dall`) targets:
- `aarch64-linux-musl`
- `x86_64-linux-musl`
- `aarch64-macos`
- `x86_64-macos`

Additional targets:
- `aarch64-linux-gnu`
- `x86_64-linux-gnu`

## Prerequisites

Install version 0.14.0 of `zig` with the package manager of your choosing, e.g., `brew install zig`, or download the binary from [here][zig-download].

### macOS debugger

macOS is curious operating system because the kernel is derived from from two codebases, the Mach kernel and the BSD kernel. It inherits two different hardware exception handling facilities, Mach exceptions and POSIX signals. We use `libsigsegv` and utilize the POSIX signals which is usually fine except when it comes to debugging with `lldb`.

`lldb` hijacks the Mach exception ports for the task when it attaches to the process. Mach exceptions get handled before POSIX signals which means that as soon as vere faults (this happens often) `lldb` stop with a `EXC_BAD_ACCESS`. It is impossible to continue debugging from this state without the workaround we implemented in https://github.com/urbit/vere/pull/611.

There are more annoying warts with `lldb` currently. First, if you attach the debugger when booting the ship with `lldb -- your-ship/.run` you have to specify `-t`, otherwise the ship is unable to boot for mysterious reasons. The other option is to start the ship and attach afterwards with `lldb -p PID`. Afterwards you should do this dance:

```
p (void)darwin_register_mach_exception_handler()
pro hand -p true -s false -n false SIGBUS
pro hand -p true -s false -n false SIGSEGV
```

### Sanitizers

Sanitizers are supported for native builds only and you need to have `llvm-19` (and `clang-19` on linux) installed on your machine.

For native linux builds this is the only situation where we actually build against the native abi. Normally we build agains musl even on native gnu machines.

macOS:
```terminal
brew install llvm@19
```

Debian/Ubuntu:
```terminal
apt-get install llvm-19 clang-19
```

## Build

Once you install `zig`, you're ready to build:
```console
zig build
```
This builds a native debug binary.

### Build options

A quick overview of the more useful build options.
See `zig build --help` for more.

#### `-Dtarget=[string]`
Cross-compilation target. See [supported targets](#supported-targets).

#### `-Doptimize=[enum]`
Optimization mode.

Supported values:
- Debug (default) => enable debugging information and runtime safety checks
- ReleaseSafe => optimize for memory safety
- ReleaseSmall => optimize for binary size
- ReleaseFast => optimize for speed

#### `-Dall`
Build for all [supported targets](#supported-targets).

#### `-Drelease`
Release flag. Builds with `-Doptimize=ReleaseFast` and `-Dpace=live`, and omits
git rev from binary version.

#### `-Dpace=[enum]`
Release train.

Supported values:
- once (default)
- live
- soon
- edge

#### `-Dcopt=[list]`
Provide additional compiler flags. These propagate to all build artifacts.

Example: `zig build -Dcopt="-g" -Dcopt="-fno-sanitize=all"`

#### `-Dasan`
Enable address sanitizer. Only supported natively. Requires `llvm@19`, see [prerequisites](#sanitizers).

#### `-Dubsan`
Enable undefined behavior sanitizer. Only supported natively. Requires `llvm@19`, see [prerequisites](#sanitizers).

<!-- ## LSP Integration -->

<!-- ```console -->
<!-- bazel run //bazel:refresh_compile_commands -->
<!-- ``` -->

<!-- Running this command will generate a `compile_commands.json` file in the root -->
<!-- of the repository, which `clangd` (or other language server processors) will -->
<!-- use automatically to provide modern editor features like syntax highlighting, -->
<!-- go-to definitions, call hierarchies, symbol manipulation, etc. -->

## Test

Run tests by giving `zig bulid` the test names as arguments, e.g.:

```console
zig build nock-test ames-test --summary all
```

See `zig build --help` for all available tests.

[zig]: https://ziglang.org
[zig-download]: https://ziglang.org/download/
