#!/bin/sh
set -eu
export LC_ALL=C
mode=$1 root=$2 prefix=$3 version=$4 commit=$5
fail() {
	echo "ERROR: Boost install integrity: $1" >&2
	exit 1
}

snapshot() {
	test -d "$prefix" && test ! -L "$prefix" &&
		test -d "$prefix/include" && test ! -L "$prefix/include" &&
		test -d "$prefix/lib" && test ! -L "$prefix/lib" ||
		fail 'missing or unsupported include/lib directory'
	test -f "$prefix/boost-version.txt" && test ! -L "$prefix/boost-version.txt" &&
		test -f "$prefix/boost-commit.txt" && test ! -L "$prefix/boost-commit.txt" ||
		fail 'missing or unsupported installed pin'
	printf '%s\n' "$version" >"$tmp/version"
	printf '%s\n' "$commit" >"$tmp/commit"
	if ! cmp -s "$tmp/version" "$prefix/boost-version.txt" ||
		! cmp -s "$tmp/commit" "$prefix/boost-commit.txt"; then
		fail 'installed pin mismatch'
	fi
	(
		cd "$prefix" || exit 1
		find include lib ! -type f ! -type d -print -quit >"$tmp/unsupported" || exit 1
		test ! -s "$tmp/unsupported" || exit 1
		find include lib -printf '%y %p\0' >"$tmp/entries" || exit 1
		sort -z "$tmp/entries" >"$tmp/sorted" || exit 1
		find include lib -type f -print0 >"$tmp/files" || exit 1
		sort -z "$tmp/files" >"$tmp/sorted-files" || exit 1
		printf 'amp-boost-install-v1\n%s\n%s\n' "$version" "$commit" || exit 1
		cat "$tmp/sorted" || exit 1
		xargs -0 -r sha256sum --zero <"$tmp/sorted-files" || exit 1
	) >"$tmp/snapshot" || fail 'cannot inventory installed files or unsupported entry'
}

mkdir -p "$root"
root=$(cd "$root" && pwd -P) || fail 'cannot resolve build root'
record=$root/boost-install.record
tmp=$(mktemp -d "$root/.boost-inventory.XXXXXXXX") || fail 'cannot create temporary record'
trap 'rm -f "$tmp/unsupported" "$tmp/entries" "$tmp/sorted" "$tmp/files" "$tmp/sorted-files" "$tmp/snapshot" "$tmp/version" "$tmp/commit"; rmdir "$tmp"' EXIT HUP INT TERM
case $mode in
prepare)
	if test -e "$prefix" || test -L "$prefix"; then
		test -d "$prefix" && test ! -L "$prefix" || fail 'unsupported prefix'
		find "$prefix" -mindepth 1 -print -quit >"$tmp/entries"
		if test -s "$tmp/entries"; then
			test -f "$record" && test ! -L "$record" || fail 'missing record; use a fresh build root'
			snapshot
			cmp -s "$record" "$tmp/snapshot" || fail 'pin or inventory mismatch; use a fresh build root'
		fi
	fi
	rm -f "$record"
	;;
record)
	snapshot
	mv "$tmp/snapshot" "$record"
	;;
verify)
	test -f "$record" && test ! -L "$record" || fail 'missing record'
	snapshot
	cmp -s "$record" "$tmp/snapshot" || fail 'pin or inventory mismatch'
	;;
*) fail 'invalid operation' ;;
esac
