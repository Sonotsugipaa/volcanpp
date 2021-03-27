#!/bin/sh
set -e

_dirname="$(realpath "$(dirname "$0")")"
if [ ! "$_dirname" = "$(pwd)" ]; then
	echo "Forcing CWD=\"$_dirname\""
	cd "$_dirname"
fi

unset _error
find tests/ -maxdepth 1 -executable -name 'unit-test-*' | while read _file; do
	echo $'Running "\033[33m'"$(basename "$_file")"$'\033[m"'
	"$_file" || _error=1
done; unset _file

[ -n "$_error" ] && exit 1
unset _error
