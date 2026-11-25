"""Run the hand-authored reference commands, not an AMP build."""

import copy
import json
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE / "workspace"
GXX = "/home/linuxbrew/.linuxbrew/bin/g++-16"
AR = "/home/linuxbrew/.linuxbrew/bin/gcc-ar-16"


def run(args, cwd):
    print("+", json.dumps(args), flush=True)
    return subprocess.run(args, cwd=cwd, check=True, text=True, capture_output=True)


def validate(actual, expected):
    assert actual == expected, "compilation database differs from executed commands"
    assert all(set(e) == {"directory", "file", "arguments", "output"} for e in actual)
    assert all(isinstance(e["arguments"], list) for e in actual)


def selected(sources, platform, include_tests):
    other = "windows" if platform == "linux" else "linux"
    return [
        source
        for source in sources
        if f"_{other}." not in source
        and (include_tests or not source.endswith("_test.cpp"))
    ]


def main():
    workspace = tomllib.loads((ROOT / "amp.toml").read_text())
    lib = tomllib.loads((ROOT / "lib/amp.toml").read_text())
    app = tomllib.loads((ROOT / "app/amp.toml").read_text())
    assert workspace["workspace"]["members"] == ["lib", "app"]
    assert lib["lib"]["type"] == "static"
    assert [b["name"] for b in app["bin"]] == ["first", "second"]
    sources = app["bin"][0]["sources"]
    assert "src space/platform_windows.cpp" in sources
    assert "src space/platform_linux.cpp" in sources
    assert "src space/platform_test.cpp" in sources
    assert selected(sources, "linux", False) == [
        "src space/first.cpp",
        "src space/platform_linux.cpp",
    ]
    assert selected(sources, "windows", True) == [
        "src space/first.cpp",
        "src space/platform_windows.cpp",
        "src space/platform_test.cpp",
    ]

    with tempfile.TemporaryDirectory(prefix="amp-compdb-fixture.") as tmp:
        build = Path(tmp)
        reference = (HERE / "reference.json").read_text()
        reference = reference.replace("@ROOT@", str(ROOT)).replace(
            "@BUILD@", str(build)
        )
        expected = json.loads(reference)
        files = [str(ROOT / "lib" / lib["lib"]["sources"][0])]
        files += [str(ROOT / "app" / s) for s in selected(sources, "linux", True)]
        files.insert(3, str(ROOT / "app" / app["bin"][1]["entry"]))
        assert [entry["file"] for entry in expected] == files
        normal = [f for f in files if not f.endswith("_test.cpp")]
        assert len(normal) == 4 and len(files) == 5
        actual = []
        for file, obj in zip(
            files,
            ["fixture.o", "first.o", "platform.o", "second.o", "test.o"],
            strict=True,
        ):
            output = str(build / obj)
            args = [GXX, "-std=c++26", "-fmodules"]
            if obj == "fixture.o":
                args += ["-x", "c++"]
            args += ["-c", file, "-o", output]
            run(args, build)
            actual.append(
                {
                    "directory": str(build),
                    "file": file,
                    "arguments": args,
                    "output": output,
                }
            )
        validate(actual, expected)
        bad = copy.deepcopy(actual)
        bad[0]["arguments"][1:3] = reversed(bad[0]["arguments"][1:3])
        try:
            validate(bad, expected)
        except AssertionError:
            print("PASS: reordered arguments rejected")
        else:
            raise AssertionError("reordered arguments accepted")
        (build / "compile_commands.json").write_text(
            json.dumps(actual, indent=2) + "\n"
        )
        run([AR, "rcs", str(build / "libfixture.a"), str(build / "fixture.o")], build)
        for name, objects, output in [
            ("first", ["first.o", "platform.o"], "first: 43\n"),
            ("second", ["second.o"], "second: 42\n"),
            ("test", ["test.o", "platform.o"], ""),
        ]:
            run([GXX, *objects, "libfixture.a", "-o", name], build)
            assert run([str(build / name)], build).stdout == output
            print(f"PASS: {name}")
        print("PASS: reference arguments, Linux selection, and fixture outputs")


if __name__ == "__main__":
    sys.exit(main())
