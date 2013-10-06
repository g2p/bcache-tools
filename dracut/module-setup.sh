#!/bin/bash
# -*- mode: shell-script; indent-tabs-mode: nil; sh-basic-offset: 4; -*-
# ex: ts=8 sw=4 sts=4 et filetype=sh

#
# At some point (util-linux v2.24) blkid will be able to identify bcache
# but until every system has this version of util-linux, probe-bcache is
# provided as an alternative.
#

check() {
    if [[ $hostonly ]] || [[ $mount_needs ]]
    then
        for fs in "${host_fs_types[@]}"; do
            [[ $fs = "bcache" ]] && return 0
        done
        return 255
    fi

    return 0
}

depends() {
    return 0
}

installkernel() {
    instmods bcache
}

install() {
    inst_multiple ${udevdir}/probe-bcache ${udevdir}/bcache-register
    inst_rules 69-bcache.rules
}
