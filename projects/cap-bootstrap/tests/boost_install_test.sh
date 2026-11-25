#!/bin/sh
set -eu
root=$(mktemp -d "${TMPDIR:-/tmp}/cap-install-test.XXXXXXXX")
trap 'rm -rf "$root"' EXIT HUP INT TERM
prefix=$root/boost-install
helper=bootstrap/boost-install.sh
version=1.2.3
commit=0123456789012345678901234567890123456789
run() { sh "$helper" "$1" "$root" "$prefix" "$version" "$commit"; }
reject() {
	if run "$1" >"$root/output" 2>&1; then
		echo "unexpected success: $*" >&2
		exit 1
	fi
	grep -q '^ERROR: Boost install integrity:' "$root/output"
}
mkdir -p "$prefix/include/boost" "$prefix/lib"
printf '%s\n' "$version" >"$prefix/boost-version.txt"
printf '%s\n' "$commit" >"$prefix/boost-commit.txt"
printf header >"$prefix/include/boost/version.hpp"
printf library >"$prefix/lib/libboost.a"
reject verify
run record
run verify
run prepare
run record
commit=1123456789012345678901234567890123456789
reject verify
reject prepare
commit=0123456789012345678901234567890123456789
printf altered >"$prefix/boost-version.txt"
reject verify
printf '%s\n' "$version" >"$prefix/boost-version.txt"
printf changed >"$prefix/include/boost/version.hpp"
reject verify
printf header >"$prefix/include/boost/version.hpp"
printf changed >"$prefix/lib/libboost.a"
reject verify
printf library >"$prefix/lib/libboost.a"
mv "$prefix/lib/libboost.a" "$root/saved"
reject verify
mv "$root/saved" "$prefix/lib/libboost.a"
printf extra >"$prefix/lib/extra"
reject verify
rm "$prefix/lib/extra"
ln -s libboost.a "$prefix/lib/link"
reject verify
rm "$prefix/lib/link"
mkfifo "$prefix/lib/fifo"
reject verify
rm "$prefix/lib/fifo"
printf invalid >"$root/boost-install.record"
reject verify
reject prepare
printf '#!/bin/sh\n: >"%s/compiler-invoked"\nexit 1\n' "$root" >"$root/compiler"
chmod +x "$root/compiler"
if make -s -o boost-build bootstrap BOOTSTRAP_ROOT="$root" BOOST_VERSION="$version" BOOST_COMMIT="$commit" CXX="$root/compiler" >"$root/output" 2>&1; then
	echo 'bootstrap accepted an invalid record' >&2
	exit 1
fi
grep -q '^ERROR: Boost install integrity:' "$root/output"
test ! -e "$root/compiler-invoked"
run record
run verify
echo 'boost install integrity fixtures passed'
