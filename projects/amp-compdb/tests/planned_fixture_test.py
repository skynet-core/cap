"""Exercise the planner-produced database with GCC and clangd."""

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "fixtures/workspace"
GXX = "/home/linuxbrew/.linuxbrew/bin/g++-16"
CLANGD = "/home/linuxbrew/.linuxbrew/bin/clangd"


def main():
    generator = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="amp compdb ") as tmp:
        build = Path(tmp)
        command = [generator, str(ROOT), str(build), GXX]
        data = subprocess.check_output(command)
        assert data == subprocess.check_output(command)
        rows = json.loads(data)
        expected = []
        for source, output in [
            ("lib/src/fixture.cpp", "fixture.o"),
            ("app/src space/second.cpp", "second.o"),
        ]:
            file = str(ROOT / source)
            obj = str(build / output)
            expected.append(
                {
                    "directory": str(build),
                    "file": file,
                    "arguments": [
                        GXX,
                        "-std=c++26",
                        "-fmodules",
                        "-c",
                        file,
                        "-o",
                        obj,
                    ],
                    "output": obj,
                }
            )
        assert rows == expected
        (build / "compile_commands.json").write_bytes(data)
        for row in rows:
            subprocess.run(row["arguments"], cwd=row["directory"], check=True)
        subprocess.run(
            [
                CLANGD,
                "--experimental-modules-support",
                f"--query-driver={GXX}",
                f"--compile-commands-dir={build}",
                f"--check={rows[1]['file']}",
            ],
            check=True,
        )


if __name__ == "__main__":
    main()
