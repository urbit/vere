#!/bin/bash
function xxd_i() {
    var=$1
    src_file=$2
    dst_h_file=$3
    dst_c_file=$4

    # Generate `.h` file.
    echo "Generating ${dst_h_file}..."
    echo "#ifndef ${var}_H" > $dst_h_file
    echo "#define ${var}_H" >> $dst_h_file
    echo "extern unsigned char $var[];" >> $dst_h_file
    echo "extern unsigned int ${var}_len;" >> $dst_h_file
    echo '#endif' >> $dst_h_file

    # Generate `.c` file.
    echo "Generating ${dst_c_file}..."
    printf '#include "%s"\n' $dst_h_file > $dst_c_file
    echo "unsigned char $var[] = {" >> $dst_c_file
    cnt=0
    while IFS='' read line
    do
        for byte in $line
        do
            echo -n " 0x$byte," >> $dst_c_file
            cnt=$((cnt+1))
        done
    # <() is syntax for Bash process substitution.
    done < <(od -An -v -tx1 $src_file)
    echo "};" >> $dst_c_file
    echo "unsigned int ${var}_len = $cnt;" >> $dst_c_file
}

version=2024-07-02
src_remote=https://curl.se/ca/cacert-${version}.pem
src_file=ca_bundle.pem
dst_h_file=ca_bundle.h
dst_c_file=ca_bundle.c

rm $dst_h_file $dst_c_file

echo "Downloading ca bundle from ${src_remote}"
curl -o $src_file $src_remote

xxd_i include_ca_bundle_crt $src_file $dst_h_file $dst_c_file

rm $src_file
