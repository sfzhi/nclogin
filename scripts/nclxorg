#!/bin/bash

readonly syslog=user.info

fatal() {
  echo "$*"
  sleep 5
  exit 1
} >&2

if [ "${1:0:1}" == ':' ]
then
  display="$1"
  exec 2> >(exec /bin/logger -p "$syslog" -t "nclxorg[$$]" -s)

  [[ "${CTTY:=$(/bin/ps -o tty= $$)}" =~ ^tty([1-9][0-9]?)$ ]] || \
    fatal "Cannot determine virtual terminal name for '$CTTY'"
  printf -v console 'vt%02u' "${BASH_REMATCH[1]}"; unset CTTY

  mcookie="$(/usr/bin/mcookie)" && test -n "$mcookie" || \
    fatal "Failed to generate magic cookie for xauth"

  serverauth="/var/tmp/xorg.auth.$$"
  trap '/bin/rm -f "$serverauth"' EXIT
  test -e "$serverauth" || > "$serverauth"

  [[ "$display" =~ ^:[0-9]+$ ]] || fatal "Invalid display: '$display'"
  /usr/X11/bin/xauth -q -f "$serverauth" add "$display" . "$mcookie"

  printf -v xparams ' %q' \
    "$mcookie" "$display" "$console" -auth "$serverauth" "${@:2}"
  NCLXORG_XPARAMS="${xparams:1}" exec /usr/bin/nohup \
    /bin/su -s /bin/sh -lpc "'$0'" "$USER" >&2 < /dev/null
elif [ $# -eq 0 -a -n "$NCLXORG_XPARAMS" ]
then
  declare -a xparams=($NCLXORG_XPARAMS); unset NCLXORG_XPARAMS
  test ${#xparams[@]} -ge 5 -a "${xparams[1]:0:1}" == ':' || \
    fatal "Internal error or invalid command line parameters"

  readonly mcookie="${xparams[0]}" display="${xparams[1]}"
  clientauth="${XAUTHORITY:-$HOME/.Xauthority}"
  test -e "$clientauth" || /usr/bin/touch "$clientauth"
  /usr/X11/bin/xauth -q add "$display" . "$mcookie"

  exec &> >(exec /bin/logger -p "$syslog" -t "xorg[$$]" &> /dev/null)
  exec /usr/X11/bin/xinit -- /usr/X11/bin/X "${xparams[@]:1}"
else
  fatal "Unrecognized invocation context:" "$0" "$@"
fi
