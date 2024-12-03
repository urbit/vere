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

version=ac87d8bbb3915d5e7c880b97c102ffe22112335f
src_remote=https://github.com/urbit/urbit/raw/$version/bin/ivory.pill
src_file=ivory.pill
dst_h_file=ivory.h
dst_c_file=ivory.c

rm $dst_h_file $dst_c_file

echo "Downloading ivory pill from ${src_remote}"
curl -LJ -o $src_file $src_remote

xxd_i u3_Ivory_pill $src_file $dst_h_file $dst_c_file

rm $src_file
