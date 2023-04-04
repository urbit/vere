## Background

Prior to 2022, the only way to interact with a running ship from Earth was via
HTTP requests sent to the `%lens` agent. In addition to `%lens`, there was a
Python script helper named `herb` which would automatically format HTTP requests
for `%lens` based on user inputs. However, there were several pain points for
using `%lens` and `herb`. `%lens`, `herb`, and the difficulties of using them
are documented more fully [here](https://github.com/urbit/urbit/issues/6418).

Unrelated to the above, inconveniences around writing boilerplate code to
interact with the `%spider` agent was hampering adoption of threads outside of
the Arvo kernel.

## Present

Starting in 2022, tools for solving the above issues began to appear (though
work on them began in 2021). In order, they are:
- `conn.c`
- The Khan vane
- The `urbit eval` utility
- `-eval` and `-khan-eval`
- The click thin client

Together, these tools are the building blocks for performing any action on a
running ship (poke, scry, or command) from Earth, and receiving back
programmatically usable output.

### `conn.c`

[`conn.c`](https://github.com/urbit/vere/blob/develop/pkg/vere/io/conn.c) is a
driver in Vere. It is a part of the "King" (a.k.a. "Urth") process. It exposes
a [Unix domain socket](https://en.wikipedia.org/wiki/Unix_domain_socket) at
`/path/to/pier/.urb/conn.sock` for sending/receiving data from external
processes.

Input to `conn.c` must be a newt-encoded jammed noun that fits mold
`[request-id command arguments]`, where:
- `request-id` is a client-supplied atomic identifier with type `@`. It exists
  entirely for the benefit of the client, allowing responses to be matched to
  requests.
- `command` is one of:
    - `%peek`
    - `%peel`
    - `%ovum`
    - `%fyrd`
    - `%urth`

These commands cover all possible cases for the following 2x2 matrix:
|          | poke             | scry    |
|----------|------------------|---------|
| **vere** | `%urth`          | `%peel` |
| **arvo** | `%ovum`, `%fyrd` | `%peek` |

For a valid command, the output from `conn.c` is a newt-encoded jammed noun with
type `[request-id output]`, where:
- `request-id` matches the input `request-id`
- `output` depends on the `command`

For an invalid command, the output from `conn.c` is a newt-encoded jammed noun
with type `[0 %bail error-code error-string]`. However, `conn.c` makes no
guarantees that:
- It will be able to sufficiently recover from the error to guarantee this
  output
- It will produce a meaningful error code and message

#### `%ovum`

The argument to an `%ovum` command is a raw kernel move which is injected
directly into the Arvo event loop. This is a very powerful - and potentially
dangerous - tool. For example, if a ship somehow got into a state where Clay was
no longer working properly (meaning new files could not be compiled to fix the
state of the kernel), the source code for a new, working Clay could be directly
injected into the ship using an `%ovum`.

The output of an `%ovum` command is:
- `[%news %done]` if the move completed successfully
- `[%news %drop]` if the move was dropped
- `[%bail goof]` if an error occurred

#### `%fyrd`

`%fyrd` is a direct shortcut to the Khan vane. The Khan vane coordinates and
manages threads, and is described in further detail below. The arguments to a
`%fyrd` command are (in order):
1. The name of the desk in which the thread lives (e.g. `%base`) or `beak` for
   the thread (e.g. `[%zod %base %10]`)
2. The name of the thread (e.g. `%hi`)
3. Mark to which the output should be cast (e.g. `%tape`)
4. Mark for how to interpret the input argument to thread (e.g. `%ship`)
5. Input argument to thread (e.g. `~zod`)

The output of a `%fyrd` command is `[%avow (each page goof)]`, the value of
`each` depending on whether the thread succeeded or not.

#### `%urth`

The argument to the `%urth` command is a subcommand for the action to perform.
Currently, the only valid commands are `%pack` and `%meld`.

`%urth` will return `%&` if given a valid command as input, otherwise it will
return `[0 %bail 0xfffffff9 %urth-bad]`. No other output is emitted.

#### `%peek`

The `%peek` command is used to perform a namespace read request (a.k.a. scry)
using Arvo's external peek interface
([arm +22 in arvo.hoon](https://github.com/urbit/urbit/blob/develop/pkg/arvo/sys/arvo.hoon#L1774)).
The argument to `%peek` is the `nom` input to `+peek` in `arvo.hoon` (`lyc` is
auto-filled as `[~ ~]`, i.e. "request from self"). That is to say that the
argument to `%peek` must have type:
```
$+  each  path
$%  [%once vis=view syd=desk tyl=spur]
    [%beam vis=view bem=beam]
==
```
Practically speaking, this means that the input will look like one of these
three examples:
```
[%& p=path]
[%| p=[%once vis=view syd=desk tyl=spur]]
[%| p=[%beam vis=view bem=beam]]
```
Where:
- `path` is a `[view beam]`, with the `view` passed in as a `coin`
- `view` is the vane code for the scry, as well as an optional care, possibly
  appended to the vane (e.g. `%j`, `%gx`, etc.)
- `beam` is a `[beak spur]`
- `desk` is used to auto-generate a `beak`: `[our desk now]`
- `spur` is the scry endpoint for the agent or vane

The output of a `%peek` command is `[%peek (unit (unit scry-output))]`, where
`~` means that the scry endpoint is invalid, and `[~ ~]` means that the scry
resolved to nothing.

See [here](https://developers.urbit.org/guides/core/app-school/10-scry) for more
information on scrying.

#### `%peel`

`%peel` attempts to emulate a scry-like namespace, like the one used by Arvo and
accessed by `%peek`. The argument to `%peel` should be a path. Valid paths
result in a non-null `unit` containing the result of the scry. Invalid paths
result in null (i.e. `~`). The valid paths and the data they return are:
```
/help   (unit (list path))  Supported %peel paths
/live   (unit ~)            Pier health check; succeeds if pier is running
/khan   (unit ~)            Khan health check; succeeds if Khan vane is running
/info   (unit mass)         Pier info as a mass
/v      (unit @t)           Returns version of the Vere binary as a cord
/who    (unit @)            Returns the Azimuth identity of the ship as an atom
```

Note that the pier info above is returned as a `mass` report, i.e. type
`(pair cord (each * (list mass)))`. This is not the same as the `|mass` memory
report. `/mass` is meant to be a valid `%peel` path which returns the `|mass`
memory report, but it is currently unimplemented.

### Khan

The Khan vane is a command / response interface for running threads. Khan was
introduced to make running threads a kernel-level feature, as simple as poking
an agent or setting a timer. Threads allow users to run arbitrarily complex code
on their ships in the same way that bash allows them to do so on Linux.

Khan's API exposes three thread requests:
- `%fard`: Kernel thread requests
- `%fyrd`: External thread requests
- `%lard`: "Inline" thread requests

"Kernel" above doesn't mean that this interface is hidden or protected from
userspace agents; thread requests by userspace agents should almost certainly
use `%fard`. It just means that `%fard` thread requests are expected to
originate from within the kernel or a userspace agent.  Specifically:
- `%fard` commands take the thread input argument as a `cage`
- The data in the `vase` of the `cage` is a `unit` (as expected by `%spider`)
- The output is also a `cage` (see below for more information)

`%fyrd` thread requests, on the other hand, perform some extra services that are
useful when running threads from the dojo or via `conn.c`. Specifically:
- `%fyrd` commands take the thread input as a raw `noun`
- Khan performs mark conversion on both the input and output for `%fyrd` requests
- Khan automatically lifts the converted input into a `unit`

"Inline" threads are a particularly specialized Khan thread request where the
thread has already been compiled and is passed as a part of the input.

Khan requests expect the following input:
- `%fard`: `p=[=bear name=term args=cage]`
- `%fyrd`: `p=[=bear name=term args=(pair mark page)]`
- `%lard`: `[=bear =shed]`

Where:
- `bear` is a `desk` or a `beak`; if `bear` is a `desk`, then the it will be
  converted to a `beak` using `our` and `now` as default values
- `shed` is a pre-computed chain of strands that produce a `vase` (the
  canonical thread)

All three produce the same output if an error occured while running the thread:
`[vow %| goof]`, where `vow` is `%arow` for `%fard` and `%lard`, and `%avow` for
`%fyrd`.

If the thread succeeded, `%fard` and `%lard` produce `[%arow %& %noun vase]`.
`%fyrd` produces `[%avow %& mark noun]`, where `mark` is the output mark and
`noun` is the output as a raw noun after mark conversion.

See [here](https://developers.urbit.org/guides/additional/threads/fundamentals)
for more information about threads.

### `urbit eval`

`eval` is a utility command in the Urbit binary. Originally, it was introduced
to evaluate snippets of Hoon code using the binary to emulate Arvo from the
associated ivory pill. This allowed it to run any Hoon code fragments that used
kernel and STL functions (e.g. anything in `hoon.hoon`, `arvo.hoon`, `lull.hoon`,
and `zuse.hoon`). Notably, this did not (and does not) evaluate any Hoon
fragments that require pier state (e.g. scries, `our`, `now`, etc.).

Example:
```
$ echo '(add 2 2)' | ./urbit eval
loom: mapped 2048MB
lite: arvo formula 2a2274c9
lite: core 4bb376f0
lite: final state 4bb376f0
eval (run):
4
```
The result (i.e. `4`) is printed to `stdout`. If the command had failed to
compile, the stack trace would have been printed to `stdout` instead. All other
messages are printed to `stderr`.

#### Options

`eval` was extended with several options that make it useful for processing Hoon
nouns as input to or output from `conn.c`:
- `-j`, `--jam`: output result as a jammed noun
- `-c`, `--cue`: read input as a jammed noun
- `-n`, `--newt`: write output / read input as a newt-encoded jammed noun, when
  paired with `-j` or `-c` respectively
- `-k`: treat the input as the jammed noun input of a `%fyrd` request to
  `conn.c`; if the result is a `goof`, pretty-print it to `stderr` instead of
  returning it

### `-eval` and `-khan-eval`

Two threads that evaluate arbitrary Hoon were added to the suite of threads
included with Arvo:
[`ted/eval.hoon`](https://github.com/urbit/urbit/blob/develop/pkg/arvo/ted/eval.hoon)
and
[`ted/khan-eval.hoon`](https://github.com/urbit/urbit/blob/develop/pkg/arvo/ted/khan-eval.hoon).

Both threads take the same input: Hoon code as a `cord` and an optional
`(list path)`. The optional `(list path)` is a list of Clay file dependencies
which need to be included for the Hoon to be evaluated (i.e. if the Hoon code
includes libraries or types defined outside of the kernel). Each `path` can be a
`beam` (i.e. `[beak spur]`) or just a `spur`, in which case the default `beak`
(i.e. `[our %base now]`) will be prepended.

`ted/eval.hoon` expects the input to be a Hoon expression. It's very similar to
`urbit eval`, except that it has access to ship state: `now`, `our`, vane &
agent state, etc.

`ted/khan-eval.hoon` expects the input to be a thread. It attempts to compile
the thread using the dependencies (if any) and then sends it to Khan as a `%lard`
thread request.

Both threads return regular thread output, i.e. a `vase`.

Examples:
- `-eval '(add 2 2)'`
- `-eval '(my-add 2 2)' [/lib/my-add/hoon ~]`
    - Where `my-add` is defined in `lib/my-add.hoon` in `%base`
- `-eval '(my-add 2 2)' [/(scot %p our)/my-desk/(scot %da now)/lib/my-add/hoon ~]`
    - Where `my-add` is defined in `lib/my-add.hoon` in `%my-desk`
- `-khan-eval '=/  m  (strand ,vase)  ;<  ~  bind:m  (poke [~zod %hood] %helm-hi !>(\'\'))  (pure:m !>(\'success\'))'`

### click

[click](https://github.com/urbit/vere/blob/develop/bin/click) is a `bash` thin
client which auto-formats `-eval` and `-khan-eval` thread calls via `%fyrd`
requests to `conn.c` and coordinates chaining together the appropriate commands
to execute those requests on a running ship.

Using click, a call like:
```
echo $'[0 %fyrd %base %khan-eval %noun %ted-eval \'=/  m  (strand ,vase)  ;<  ~  bind:m  (poke [~zod %hood] %helm-hi !>(\\\'\\\'))  (pure:m !>(\\\'success\\\'))\']' |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
instead looks like:
```
/path/to/click -k /path/to/zod $'=/  m  (strand ,vase)  ;<  ~  bind:m  (poke [~zod %hood] %helm-hi !>(\\\'\\\'))  (pure:m !>(\\\'success\\\'))'
```
or even more conveniently:
```
/path/to/click -k -i threads/poke.hoon /path/to/zod
```

```
Usage:
    click [options] <path-to-pier> <hoon> [<dependencies> ...]
    click [options] -i <path-to-file> <path-to-pier> [<dependencies> ...]
    click [-o|-p] -e -i <path-to-file> <path-to-pier>

    Thin client for interacting with running Urbit ship via conn.c

    options:
        -e                  Execute jammed Hoon
        -h                  Show usage info
        -i <path-to-file>   Read input from file
        -j                  Jam only
        -k                  Execute command using "khan-eval" thread
        -o <path-to-file>   Output to file
        -p                  Filter failure stack traces from result and pretty-print them to stderr
        -x                  Jam to hex
```

## Using these tools

Below are examples of how to execute common commands on a running ship from
Earth.

### `|mass`

Blocked by issues; not currently doable in a way that returns the results as
data.

### `|pack`

```
echo "[0 %urth %pack]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
```
echo "[0 %ovum %d /test %pack ~]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
```
/path/to/click -kp /path/to/pier/zod \
$'=/  m  (strand ,vase)  ;<  ~  bind:m  (flog [%pack ~])  (pure:m !>(\\\'success\\\'))'
```

### `|meld`

```
echo "[0 %urth %meld]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
```
echo "[0 %ovum %d /test %meld ~]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
```
/path/to/click -kp /path/to/pier/zod \
$'=/  m  (strand ,vase)  ;<  ~  bind:m  (flog [%meld ~])  (pure:m !>(\\\'success\\\'))'
```

### `|ota`

#### `|ota ~bus`

```
echo "[0 %ovum [%g /test [%deal [~zod ~zod] %hood %raw-poke %kiln-install %base ~bus %kids]]]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
```
/path/to/click -kp /path/to/pier/zod \
$'=/  m  (strand ,vase)  ;<  our=@p  bind:m  get-our  ;<  ~  bind:m  (poke [our %hood] %kiln-install !>([%base ~bus %kids]))  (pure:m !>(\\\'success\\\'))'
```

#### `|ota %disable`

```
echo "[0 %ovum [%g /test [%deal [~zod ~zod] %hood %raw-poke %kiln-install %base ~zod %base]]]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
```
/path/to/click -kp /path/to/pier/zod \
$'=/  m  (strand ,vase)  ;<  our=@p  bind:m  get-our  ;<  ~  bind:m  (poke [our %hood] %kiln-install !>([%base our %base]))  (pure:m !>(\\\'success\\\'))'
```

#### `|ota ~bus %desk`

```
echo "[0 %ovum [%g /test [%deal [~zod ~zod] %hood %raw-poke %kiln-install %base ~zod %desk]]]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
```
/path/to/click -kp /path/to/pier/zod \
$'=/  m  (strand ,vase)  ;<  our=@p  bind:m  get-our  ;<  ~  bind:m  (poke [our %hood] %kiln-install !>([%base ~bus %desk]))  (pure:m !>(\\\'success\\\'))'
```

### `|install`

#### `|install ~sampel-palnet %desk`

```
echo "[0 %ovum [%g /test [%deal [~zod ~zod] %hood %raw-poke %kiln-install %desk ~sampel-palnet %desk]]]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
```
/path/to/click -kp /path/to/pier/zod \
$'=/  m  (strand ,vase)  ;<  our=@p  bind:m  get-our  ;<  ~  bind:m  (poke [our %hood] %kiln-install !>([%desk ~sampel-palnet %desk]))  (pure:m !>(\\\'success\\\'))'
```

#### `|install ~sampel-palnet %desk, =local %my-desk`

```
echo "[0 %ovum [%g /test [%deal [~zod ~zod] %hood %raw-poke %kiln-install %my-desk ~sampel-palnet %desk]]]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```
```
/path/to/click -kp /path/to/pier/zod \
$'=/  m  (strand ,vase)  ;<  our=@p  bind:m  get-our  ;<  ~  bind:m  (poke [our %hood] %kiln-install !>([%my-desk ~sampel-palnet %desk]))  (pure:m !>(\\\'success\\\'))'
```

### `+code`

```
/path/to/click -kp /path/to/pier/zod \
$'=/  m  (strand ,vase)  ;<  our=@p  bind:m  get-our  ;<  code=@p  bind:m  (scry @p /j/code/(scot %p our))  (pure:m !>((crip (slag 1 (scow %p code)))))'
```

### `+vats`

```
/path/to/click -kp /path/to/pier/zod \
$'=/  m  (strand ,vase)  ;<  our=@p  bind:m  get-our  ;<  now=@da  bind:m  get-time  (pure:m !>((crip ~(ram re [%rose [~ ~ ~] (report-vats our now)]))))' \
'/sur/hood/hoon'
```

### Additional Notes

#### Alternative click calls

Any example above that uses click has two additional options that have been
omitted for brevity, since the actual code for the call would be identical in
each example:

1. Custom `-thread` in `%desk`:
```
echo "[0 %fyrd %desk %thread %noun %noun ~]" |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -cn
```

2. Pass inline thread to click from file:
```
/path/to/bin/click -k -i path/to/thread.hoon /path/to/pier/zod
```

#### Undocked ships

click assumes that the ship at the given pier is docked (i.e. that
`/path/to/pier/.run` exists). If for whatever reason the running ship is
undocked, it's still possible to work around this assumption using the
click-format helper script. For example, the call for `+vats` becomes:
```
/path/to/click-format -k \
$'=/  m  (strand ,vase)  ;<  our=@p  bind:m  get-our  ;<  now=@da  bind:m  get-time  (pure:m !>((crip ~(ram re [%rose [~ ~ ~] (report-vats our now)]))))' \
'/sur/hood/hoon' |
/path/to/urbit eval -jn |
nc -U -W 1 /path/to/pier/zod/.urb/conn.sock |
/path/to/urbit eval -ckn
```

## Issues and Future Work

Currently, there are a number of minor issues and one major issue impacting
interactions between Earth and Mars.

The minor issues are:
- `conn.c`'s simulated namespace for `%peel`
    - Unprincipled namespace simulation for no reason other than consistency
      with Arvo scry
- `/mass` path for `conn.c` `%peel` not implemented
- No `mass` mark in Arvo, so attempting to scry for `|mass` with `%peek`
  crashes the ship

The major issue is the lack of "thick" clients which are able to consume the
newt-encoded jammed nouns emitted by `conn.c` as input. Though not officially
codified yet, it makes sense for newt-encoded jammed nouns to be the
[narrow waist](https://www.oilshell.org/blog/2022/02/diagrams.html) of Urbit,
and [recent design decisions appear to be heading in this direction](https://github.com/urbit/urbit/pull/6396).
Unfortunately, the narrow waist of `bash` is text, and it's not always easy or
useful to convert nouns to text (particularly stack traces).

There exist already two external noun libraries, in
[Rust](https://github.com/urbit/noun) and Haskell (link to Haskell lib coming
soon). Adding more, while not trivial, is not difficult. The proliferation of
noun representation libraries in other languages would open many doors with
regards to the support, hosting, and application opportunities available (the
ever-fabled "Quake over Urbit").
