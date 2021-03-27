#!/bin/bash

# MIT License
#
# Copyright (c) 2021 Parola Marco
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


unset options;
function add-opt {
	options="${options}$1"
}

function reset-cache {
	case "$options" in
		*r*)
			rm -rf ./* \
			|| return 1;;
	esac
}

function build {
	case "${options}" in
		*i*)
			echo 'build.sh: no installation process defined' 1>&2;;
		*)
			echo 'Building with profile "'"$1"'"'
			cmake -DCMAKE_BUILD_TYPE="$1" ../src -G Ninja &&\
			cmake --build . || return 1;;
	esac
}

function process-arg {
	mkdir -p "$1" \
	&& cd "$1" \
	&& reset-cache "$2" \
	&& build "$2" \
	|| return 1
}

# Process all options, then leave the arguments
arg="$1"
while [[ -n "$arg" && "$arg" != -- ]]; do
	case "$arg" in
		-*)
			if [[ "$arg" = -r || "$arg" = --rm-cache ]]; then
				add-opt r;  fi
			if [[ "$arg" = -i || "$arg" = --install ]]; then
				add-opt i;  fi
			shift 1
			arg="$1";;
		*)
			arg=--;;
	esac
done
unset arg

unset error
[[ -z "$1" ]] && set -- release
while [[ -n "$1" ]]; do
	case "$1" in
		default)
			profile=default;
			profile_val=;;
		debug)
			profile=debug;
			profile_val=Debug;;
		release)
			profile=release;
			profile_val=Release;;
		release-debug)
			profile=release-dbginfo;
			profile_val=RelWithDebInfo;;
		release-size)
			profile=release-minsize;
			profile_val=MinSizeRel;;
		*)  error=;;
	esac
	(process-arg "$profile" "$profile_val") || error=
	shift
done
[[ -v error ]] && exit 1 || exit 0
