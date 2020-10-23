#!/bin/bash

set -e
set -o xtrace

check_executable () {
    if [[ ! -x $MESON_BUILDDIR/$1 ]]; then
        echo "$1 not found after build"
        exit 1
    fi
    return 0
}

if [[ -z "$MESON_BUILDDIR" ]]; then
    echo "\$MESON_BUILDDIR not set"
    exit 1
fi

[[ "$BUILD_XVFB" == true ]]     && check_executable "hw/vfb/Xvfb"
# Xwayland is not optional
check_executable "hw/xwayland/Xwayland"

exit 0
