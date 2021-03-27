#!/bin/bash


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
