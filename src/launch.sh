#!/bin/sh
set -e

_dirname="$(realpath "$(dirname "$0")")"
if [ ! "$_dirname" = "$(pwd)" ]; then
	echo "Forcing CWD=\"$_dirname\""
	cd "$_dirname"
fi
unset _dirname

export VKA2_SHADER_PATH=./shaders
export VKA2_ASSET_PATH=./assets

unset pre_command
[ "$1" = '-d' ] && pre_command='valgrind' && shift
[ "$1" = '-l' ] && pre_command='valgrind --leak-check=full --show-leak-kinds=all' && shift
exec $pre_command vkapp2/vkapp2
