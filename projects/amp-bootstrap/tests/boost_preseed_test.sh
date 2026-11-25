#!/bin/sh
set -eu

root=$(CDPATH='' cd -- "$(dirname "$0")/../../.." && pwd)
tmp=$(mktemp -d /tmp/opencode/amp-boost-fixture.XXXXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
export GIT_CONFIG_NOSYSTEM=1 HOME="$tmp/home"
export GIT_AUTHOR_NAME=Fixture GIT_AUTHOR_EMAIL=fixture@example.invalid
export GIT_COMMITTER_NAME=$GIT_AUTHOR_NAME GIT_COMMITTER_EMAIL=$GIT_AUTHOR_EMAIL
export GIT_AUTHOR_DATE='2000-01-01T00:00:00Z'
export GIT_COMMITTER_DATE=$GIT_AUTHOR_DATE
export MARKER="$tmp/marker"
mkdir -p "$HOME" "$tmp/build"
build="$tmp/build"

init_repo() {
	git init -q "$1"
}
commit_repo() {
	git -C "$1" add .
	git -C "$1" -c commit.gpgsign=false commit -qm fixture
}
init_repo "$tmp/leaf"
printf 'leaf\n' >"$tmp/leaf/tracked"
commit_repo "$tmp/leaf"
init_repo "$tmp/child"
git -C "$tmp/child" -c protocol.file.allow=always submodule add -q "$tmp/leaf" nested
commit_repo "$tmp/child"
init_repo "$tmp/build/boost-src"
src="$tmp/build/boost-src"
cat >"$src/bootstrap.sh" <<'EOF'
#!/bin/sh
printf 'ran\n' >> "$MARKER"
EOF
chmod +x "$src/bootstrap.sh"
git -C "$src" -c protocol.file.allow=always submodule add -q "$tmp/child" child
git -C "$src" -c protocol.file.allow=always submodule update -q --init --recursive
commit_repo "$src"
commit=$(git -C "$src" rev-parse HEAD)
git -C "$src" tag boost-1.2.3

run() {
	make -s -C "$root" boost-bootstrap BOOTSTRAP_ROOT="$build" BOOST_VERSION=1.2.3 BOOST_COMMIT="$commit" "$@" >"$tmp/log" 2>&1
}
reject() {
	rm -f "$MARKER"
	if run "$@"; then
		cat "$tmp/log" >&2
		echo 'unexpected success' >&2
		exit 1
	fi
	test ! -e "$MARKER"
	grep -q 'ERROR:' "$tmp/log"
}

run
test -f "$MARKER"
printf 'generated\n' >"$src/generated-artifact"
printf 'generated\n' >"$src/child/nested/generated-artifact"
run
cp -Rp "$build" "$tmp/relocated"
build="$tmp/relocated"
src="$build/boost-src"
run
reject BOOST_COMMIT=0000000000000000000000000000000000000000
reject BOOST_VERSION=9.9.9
printf '# modified\n' >>"$src/bootstrap.sh"
reject
git -C "$src" update-index --assume-unchanged bootstrap.sh
reject
git -C "$src" update-index --no-assume-unchanged bootstrap.sh
git -C "$src" restore bootstrap.sh
printf '# staged\n' >>"$src/bootstrap.sh"
git -C "$src" add bootstrap.sh
git -C "$src" restore bootstrap.sh
reject
git -C "$src" restore --staged --worktree bootstrap.sh
git -C "$src" update-index --skip-worktree bootstrap.sh
reject
git -C "$src" update-index --no-skip-worktree bootstrap.sh
printf 'modified\n' >>"$src/child/nested/tracked"
reject
git -C "$src/child/nested" update-index --assume-unchanged tracked
reject
git -C "$src/child/nested" update-index --no-assume-unchanged tracked
git -C "$src/child/nested" restore tracked

for record in boost-commit.txt boost-version.txt; do
	rm "$build/$record"
	mkdir "$build/$record"
	reject
	rmdir "$build/$record"
done

cat >"$tmp/git-fail" <<'EOF'
#!/bin/sh
case " $* " in
  *" submodule status --recursive "*) exit 1 ;;
esac
exec git "$@"
EOF
chmod +x "$tmp/git-fail"
reject GIT="$tmp/git-fail"
run
echo 'boost preseed fixture: passed (not a Boost build)'
