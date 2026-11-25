# Standalone native binary fixture

This package has one binary and no library, workspace, package dependencies,
registry, or SDK requirements. Its importer needs both the named module and
the C object; all three source paths contain spaces.

## Future CLI acceptance (not yet run)

Once `amp build` is integrated, invoke it with trusted absolute tool paths and
a fresh private build root, for example:

```sh
amp build --cc /home/linuxbrew/.linuxbrew/bin/gcc-16 \
  --cxx /home/linuxbrew/.linuxbrew/bin/g++-16 --ar /usr/bin/ar \
  --manifest-path /absolute/path/to/native-standalone/amp.toml \
  --build-root /absolute/path/to/fresh-private-build-root
```

Verify successful exit, the reported executable and compilation database,
and that the database contains the actual executed compile arguments for all
three sources, including paths with spaces. Run the reported executable
separately: it must exit 0 and print exactly `standalone: 42` followed by a
newline. This is an acceptance procedure, not evidence that `amp build` ran.
