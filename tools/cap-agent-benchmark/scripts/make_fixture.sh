#!/usr/bin/env bash
set -euo pipefail
DEST="${1:-/tmp/cap-bench-fixture}"
mkdir -p "$DEST/tests"
cat >"$DEST/tests/test_arithmetic.py" <<'PY'
import unittest
from arithmetic import add
class ArithmeticTest(unittest.TestCase):
    def test_add(self):
        self.assertEqual(add(2, 3), 5)
        self.assertEqual(add(-1, 1), 0)
PY
(cd "$DEST" && git init -q && git config user.email benchmark@localhost && git config user.name 'CAP Benchmark' && git add . && git commit -qm 'Fixture baseline')
echo "$DEST"
