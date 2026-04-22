# Finding 013 — `_http_req_is_auth` cookie key overread

**Status:** real bug in production. Not yet patched.
**Severity:** bounded OOB read (1 byte past `fig_u->key_c`), harness-confirmed
**Reachability:** remote, HTTP client → cookie header, **IF** the h2o
layer accepts NUL bytes in cookie values.

## The bug

`pkg/vere/io/http.c:337` `_http_req_is_auth` scans the `Cookie:`
header looking for `fig_u->key_c` followed by `'='`. The scan loop
increments `j_i` (position within the key) every time
`coo_u.base[i_i] == key_c[j_i]` but never bounds-checks `j_i`
against the key's length.

```c
size_t j_i = 0;
while (i_i < coo_u.len) {
  if (key_c[j_i] == '\0' && coo_u.base[i_i] == '=') {
    // key found; read value
  }
  else if (coo_u.base[i_i] == key_c[j_i]) {
    j_i++;                 // no bound on j_i
  }
  else {
    j_i = 0;
  }
  i_i++;
}
```

Path to the bug: cookie value starts with the full key (e.g. `"sess"`
including the NUL terminator of `key_c`). After the key chars match,
`j_i` reaches `strlen(key_c)`. Next iteration `key_c[j_i] == '\0'`.
If the next cookie byte is also `'\0'` (not `'='`), the `else if`
branch fires (`'\0' == '\0'`), advancing `j_i` past the terminator.
Subsequent iterations read `key_c[j_i+1]`, `key_c[j_i+2]`, …, which
is OOB.

ASan-confirmed in H29 (`fuzz_http_cookie`):
```
ERROR: AddressSanitizer: global-buffer-overflow on address 0x000000e46f45
READ of size 1 at 0x000000e46f45
    #0 _cookie_soft fuzz/harnesses/fuzz_http_cookie.c:90:34
0x000000e46f45 is located 0 bytes to the right of global variable
  '<string literal>' ... (0xe46f40) of size 5
  '<string literal>' is ascii string 'sess'
```

Only 1 byte past the NUL, so the blast radius is small — the OOB
read probes adjacent rodata. In production `fig_u->key_c` is a
heap-allocated string, so the next bytes are heap metadata; reading
them may or may not crash but definitely leaks info about adjacent
allocations.

## Reachability

Depends entirely on whether h2o forwards NUL bytes in cookie headers
to our handler without filtering. RFC 6265 says cookie octets are
`%x21-%x7E`, excluding NUL, but many HTTP stacks are lenient. Need
to confirm h2o behavior. If h2o rejects NUL early, this becomes
theoretical / harness-only.

## Fix

Add `strlen(key_c)` guard to the scan loop:

```c
size_t key_len = strlen(key_c);
while (i_i < coo_u.len) {
  if (j_i == key_len && coo_u.base[i_i] == '=') {
    // key found
  }
  else if (j_i < key_len && coo_u.base[i_i] == key_c[j_i]) {
    j_i++;
  }
  else {
    j_i = 0;
  }
  i_i++;
}
```

or equivalently switch to a `strstr`-style search on `"key="` with
cookie delimiters.

## Reproducing

```bash
./fuzz/build.sh fuzz_http_cookie
./fuzz/out/fuzz_http_cookie.afl < fuzz/findings/013-http_cookie_key_overread/repro.bin
```

Expect: ASan global-buffer-overflow in `_cookie_soft`.
