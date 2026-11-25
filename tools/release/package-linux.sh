#!/usr/bin/env bash
set -euo pipefail
umask 022

if [[ $# -ne 4 ]]; then
	printf 'usage: %s VERSION CAP DOCS OUT\n' "$0" >&2
	exit 2
fi

version=$1
cap=$2
docs=$3
out=$4
if [[ ! $version =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
	printf 'invalid version: %s\n' "$version" >&2
	exit 2
fi
if [[ ! -f $cap || ! -x $cap || -L $cap ]]; then
	printf 'CAP must be an executable regular file\n' >&2
	exit 2
fi
if [[ ! -d $docs || ! -d $out || -L $out ]]; then
	printf 'DOCS and OUT must be directories\n' >&2
	exit 2
fi
shopt -s nullglob dotglob
out_entries=("$out"/*)
if [[ ${#out_entries[@]} -ne 0 ]]; then
	printf 'OUT must be empty\n' >&2
	exit 2
fi

doc_names=(
	LICENSE
	NOTICE
	BOOST-LICENSE
	GCC-GPL-3
	GCC-RUNTIME-EXCEPTION
)
entries=("$docs"/*)
if [[ ${#entries[@]} -ne ${#doc_names[@]} ]]; then
	printf 'DOCS must contain exactly the required documents\n' >&2
	exit 2
fi
for name in "${doc_names[@]}"; do
	if [[ ! -f $docs/$name || -L $docs/$name ]]; then
		printf 'missing or invalid document: %s\n' "$name" >&2
		exit 2
	fi
done
tmp=$(mktemp -d /tmp/cap-release.XXXXXXXXXX)
trap 'rm -rf -- "$tmp"' EXIT
stage=$tmp/stage
mkdir -p "$stage/deb/DEBIAN" "$stage/deb/usr/bin" \
	"$stage/deb/usr/share/doc/cap-cli" "$stage/rpmroot/usr/bin" \
	"$stage/rpmroot/usr/share/doc/cap-cli" "$stage/tar/bin" \
	"$stage/tar/share/doc/cap-cli" "$tmp/rpmbuild/BUILD" \
	"$tmp/rpmbuild/BUILDROOT" "$tmp/rpmbuild/RPMS" \
	"$tmp/rpmbuild/SOURCES" "$tmp/rpmbuild/SPECS" "$tmp/rpmbuild/SRPMS"

cp -- "$cap" "$stage/deb/usr/bin/cap"
cp -- "$cap" "$stage/rpmroot/usr/bin/cap"
cp -- "$cap" "$stage/tar/bin/cap"
chmod 0755 "$stage/deb/usr/bin/cap" "$stage/rpmroot/usr/bin/cap" "$stage/tar/bin/cap"
for name in "${doc_names[@]}"; do
	cp -- "$docs/$name" "$stage/deb/usr/share/doc/cap-cli/$name"
	cp -- "$docs/$name" "$stage/rpmroot/usr/share/doc/cap-cli/$name"
	cp -- "$docs/$name" "$stage/tar/share/doc/cap-cli/$name"
done
chmod 0644 "$stage/deb/usr/share/doc/cap-cli/"* \
	"$stage/rpmroot/usr/share/doc/cap-cli/"* "$stage/tar/share/doc/cap-cli/"*

cat >"$stage/deb/DEBIAN/control" <<EOF
Package: cap-cli
Version: $version
Section: utils
Priority: optional
Architecture: amd64
Depends: libc6 (>= 2.41)
Maintainer: CAP maintainers <47940233+skynet-core@users.noreply.github.com>
Description: CAP command-line application
EOF
deb_name=cap-cli_${version}_amd64.deb
dpkg-deb --root-owner-group --build "$stage/deb" "$stage/$deb_name" >/dev/null

cat >"$tmp/rpmbuild/SPECS/cap-cli.spec" <<EOF
Name: cap-cli
Version: $version
Release: 1
Summary: CAP command-line application
License: Apache-2.0 AND BSL-1.0 AND GPL-3.0-or-later WITH GCC-exception-3.1
Requires: glibc >= 2.41
BuildArch: x86_64
%global debug_package %{nil}
%global __os_install_post %{nil}
%global _build_id_links none
%description
CAP command-line application.
%install
mkdir -p "%{buildroot}/usr/bin" "%{buildroot}/usr/share/doc/cap-cli"
cp -a "$stage/rpmroot/usr/bin/cap" "%{buildroot}/usr/bin/cap"
cp -a "$stage/rpmroot/usr/share/doc/cap-cli/." "%{buildroot}/usr/share/doc/cap-cli/"
%files
/usr/bin/cap
/usr/share/doc/cap-cli/*
EOF
rpmbuild -bb --define "_topdir $tmp/rpmbuild" \
	--define "_rpmdir $tmp/rpmbuild/RPMS" "$tmp/rpmbuild/SPECS/cap-cli.spec"
rpm_name=cap-cli-${version}-1.x86_64.rpm
cp -- "$tmp/rpmbuild/RPMS/x86_64/$rpm_name" "$stage/$rpm_name"

tar_name=cap-cli-${version}-linux-x86_64.tar.gz
tar --create --gzip --file="$stage/$tar_name" --directory="$stage/tar" \
	--owner=0 --group=0 --numeric-owner bin/cap share/doc/cap-cli

(cd "$stage" && sha256sum "$deb_name" "$rpm_name" "$tar_name" >SHA256SUMS)
cp -- "$stage/$deb_name" "$stage/$rpm_name" "$stage/$tar_name" "$stage/SHA256SUMS" "$out/"
