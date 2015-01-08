#!/bin/sh
# -*- mode: shell-script; indent-tabs-mode: nil; sh-basic-offset: 4; -*-
# ex: ts=8 sw=4 sts=4 et filetype=sh

#
# This allows some parameter passing to bcache during boot on the kernel
# cmdline. Because the kernel cmdline is limited in size, the parameters
# have short aliases.
#
declare -A parm_map

parm_map[sco]="sequential_cutoff"
parm_map[crdthr]="cache/congested_read_threshold_us"
parm_map[cwrthr]="cache/congested_write_threshold_us"
parm_map[sequential_cutoff]="sequential_cutoff"
parm_map[congested_read_threshold_us]="cache/congested_read_threshold_us"
parm_map[congested_write_threshold_us]="cache/congested_write_threshold_us"

for i in /sys/block/bcache*/bcache
do
    [ -e "$i" ] || continue
    DNAME=${i%/*}
    DNAME=${DNAME##*/}
    for j in `getarg "rd.$DNAME"`
    do
        ARGS="$j,"
        while [ "$ARGS" != "" ]
        do
            ARG=${ARGS%%,*}
            ARGS=${ARGS#*,}
            [ "$ARG" == "" ] && continue

            FN="${parm_map[${ARG%=*}]}"
            VAL=${ARG#*=}
            [ "$FN" == "" ] && continue

            FILE="$i/$FN"
            [ -e $FILE ] || continue
            echo "$VAL" > "$FILE"
        done
    done
done
