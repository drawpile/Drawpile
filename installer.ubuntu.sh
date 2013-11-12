#!/bin/bash
# -*- coding: utf-8, tab-width: 2 -*-
SELFPATH="$(readlink -m "$0"/..)"


function main () {
  local BUILD_PROJ="$SELFPATH"
  local BUILD_DEST="$SELFPATH"/build

  check_packages              || return $?
  mkdir -p "$BUILD_DEST"      || return $?
  cd "$BUILD_DEST"            || return $?
  cmake "$BUILD_PROJ"         || return $?
  make "$@"                   || return $?

  echo
  echo
  echo "+++ install probably succeeded. files in $BUILD_DEST/bin: +++"
  ls -l "$BUILD_DEST"/bin
  return 0
}


function check_packages () {
  echo "checking required packages:"
  local PKG_NEED=()
  PKG_NEED+=( build-essential )
  PKG_NEED+=( cmake )
  PKG_NEED+=( libqt4-dev )
  PKG_NEED+=( libzip-dev )

  local PKG_MISS=()
  local PKG_NAME=
  for PKG_NAME in "${PKG_NEED[@]}"; do
    LANG=C aptitude search --display-format='%p %v' --disable-columns \
      '~n^'"$PKG_NAME"'$' | grep -qFe ' <none>' && PKG_MISS+=( "$PKG_NAME" )
  done
  [ "${#PKG_MISS[@]}" == 0 ] && return 0

  local SUDO_CMD=( sudo -E apt-get install "${PKG_MISS[@]}" )
  echo "W: Some packages are missing. Will try to install them by running" >&2
  echo "   ${SUDO_CMD[*]}" >&2
  "${SUDO_CMD[@]}" || return $?

  return 0
}







main "$@"; exit $?
