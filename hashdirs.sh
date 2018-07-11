#!/bin/bash
set -e

srcdir="$1"
dstdir="$2"
DJB2BIN=out/bin/djb2

[[ ! -f ${DJB2BIN} ]] && echo "error ** please build ${DJB2BIN} at first." && exit -1;
[[ "x$srcdir" = "x" ]] && echo "usage $0 srcdir dstdir" && exit -2;
[[ "x$dstdir" = "x" ]] && echo "usage $0 srcdir dstdir" && exit -2;

mkdir -p $dstdir

function djb2all() {
    for f in $(find "$srcdir" -type f -printf "%P\n"); do
        dst=$(${DJB2BIN} "$f" |awk '{print $srcdir}')
        echo $dst
    done
}

# check if any hash collisions:
# djb2all $srcdir |wc -l
# djb2all $srcdir |wc -l |sort|uniq

files=$(djb2all $srcdir |wc -l)
files_uniq=$(djb2all $srcdir |wc -l |sort|uniq)
[[ $files != $files_uniq ]] && echo "error *** hash collisions detected!!!" && exit -3;

# gzip content to save spiffs space and speed up http response.
for f in $(find $srcdir -type f -regextype posix-extended -iregex '.*\.(css|csv|html?|js|svg|txt|xml)'); do
    echo $f;
    zopfli -i500 $f;
    mv ${f}.gz ${f}
done

for f in $(find "$srcdir" -type f -printf "%P\n"); do
    # ${DJB2BIN} "$f"
    dst=$(${DJB2BIN} "$f" |awk '{print $1}')
    cp $srcdir/$f $dstdir/$dst
done
