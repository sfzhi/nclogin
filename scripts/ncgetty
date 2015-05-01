#!/bin/sh
# Usage: ncgetty [-s] N [additional arguments for nclogin]
if [ "$1" = '-s' ]
then
  sw=s
  shift
else
  sw=
fi
vt="$1"
shift
exec /bin/openvt -c "$vt" -e$sw -- /usr/sbin/nclogin -t linux -L en_US.UTF-8 -Wu "$@"
