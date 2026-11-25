# Standalone native static library fixture

`package/` has one static library and no binaries, workspace, dependencies or
SDK requirements. Its explicitly listed sources include C, a named-module
provider and a distinct importer. The consumer outside the package uses the
header in `public include/`; its result requires both contributions.

## Local CLI acceptance

Run both `--lib` and default selection in separate fresh private build roots.
For example:

```sh
cap build --cc /home/linuxbrew/.linuxbrew/bin/gcc-16 \
  --cxx /home/linuxbrew/.linuxbrew/bin/g++-16 --ar /usr/bin/ar \
  --manifest-path /absolute/path/to/native-library/package/cap.toml \
  --build-root /absolute/path/to/fresh-private-build-root --lib
```

Verify the reported archive and compilation database, including exactly the
three executed source compile actions and provider-before-importer ordering.
The database must not include the archiver or independent consumer. Compile
and link `consumer/main.cpp` separately against the reported archive with
`-I /absolute/path/to/native-library/package/public include`, then run it:
it must exit zero and print exactly `library: 42` followed by a newline.
