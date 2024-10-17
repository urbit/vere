# Generated architecture specific `.c`, `.s`, and `.h` files

To generate these, first run the `./configure` script under the unpacked GMP dependency directory with the following options:

macOS:
```terminal
./configure --with-pic --disable-shared
```

linux-x86_64:
```terminal
./configure --with-pic --disable-shared --host=x86_64-linux-musl
```

linux-aarch64:
```terminal
./configure --with-pic --disable-shared --host=aarch64-linux-musl
```

Next, navigate under `mpn/` and run the following to generate the assembly files:

```bash
for file in $(find . -maxdepth 1 -print | grep "\.asm$"); do
    filename_no_path="${file#"./"}";
    filename="${filename_no_path%.*}";
    echo "m4 -DHAVE_CONFIG_H -D__GMP_WITHIN_GMP -DOPERATION_${filename} -DPIC $file > ${filename}.s";
    m4 -DHAVE_CONFIG_H -D__GMP_WITHIN_GMP -DOPERATION_${filename} -DPIC $file > ${filename}.s;
done
```

Next, copy the generated `mpn/*.s` files under the appropriate path, e.g.,
`aarch64-macos/mpn/.`

Now, under the GMP root dir run `make` and copy these files as well:

- `mp_bases.h`
- `fac_table.h`
- `fib_table.h`
- `trialdivtab.h`
- `sieve_table.h`
- `mpn/fib_table.c`
- `mpn/jacobitab.h`
- `mpn/mp_bases.c`
- `mpn/perfsqr.h`

e.g.
```bash
cp {mp_bases.h,fac_table.h,fib_table.h,trialdivtab.h} aarch64-macos/.
cp {mpn/fib_table.c,mpn/jacobitab.h,mpn/mp_bases.c,mpn/perfsqr.h} aarch64-macos/mpn/.
```

## Some additional snippets

### Write the generated assembly file paths into a file `ssources`

```bash
for file in $(find ./mpn -maxdepth 1 -print | grep "\.s" | sort); do
    echo $(realpath $file) >> ssources;
done
```

### Write the relevant C source file paths into a file `csources`

```bash
for file in $(find ./mpn -maxdepth 1 -print | grep "\.c" | sort); do
    echo $(realpath $file) >> csources;
done
```
