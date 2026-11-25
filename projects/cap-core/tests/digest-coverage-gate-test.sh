#!/bin/sh
set -eu

gate=projects/cap-core/tests/digest-coverage-gate.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/digest-coverage-test.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
source=/project/projects/cap-core/digest.cppm

check() {
	expected=$1
	if sh "$gate" "$tmp/module.info" "$source"; then actual=pass; else actual=fail; fi
	if [ "$actual" != "$expected" ]; then
		printf 'expected %s, got %s\n' "$expected" "$actual" >&2
		exit 1
	fi
}

printf 'SF:%s\nLF:20\nLH:20\nend_of_record\n' "$source" >"$tmp/module.info"
check pass
printf 'SF:/other/before.cppm\nLF:1\nLH:1\nend_of_record\nSF:%s\nLF:20\nLH:20\nend_of_record\nSF:/other/after.cppm\nLF:1\nLH:1\nend_of_record\n' "$source" >"$tmp/module.info"
check pass
printf 'SF:%s\nLF:20\nLH:19\nend_of_record\n' "$source" >"$tmp/module.info"
check pass
printf 'SF:%s\nLF:20\nLH:20\n' "$source" >"$tmp/module.info"
check fail
printf 'SF:%s\nLF:20\nLH:20\nSF:/other.cppm\nLF:1\nLH:1\nend_of_record\n' "$source" >"$tmp/module.info"
check fail
printf 'SF:%s\nend_of_record\nLF:20\nLH:20\n' "$source" >"$tmp/module.info"
check fail
printf 'SF:%s\nLF:20\nLH:20\nend_of_record\nSF:%s\nLF:20\nLH:20\nend_of_record\n' "$source" "$source" >"$tmp/module.info"
check fail
printf 'SF:%s\nLF:10000\nLH:9499\nend_of_record\n' "$source" >"$tmp/module.info"
check fail
printf 'SF:/other/digest.cppm\nLF:20\nLH:20\nend_of_record\n' >"$tmp/module.info"
check fail
printf 'SF:%s\nLF:0\nLH:0\nend_of_record\n' "$source" >"$tmp/module.info"
check fail
printf '%s\n' 'digest coverage gate regression: passed'
