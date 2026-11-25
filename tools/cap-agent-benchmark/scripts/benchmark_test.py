"""Offline regression tests for benchmark failure handling."""

import io
import unittest
from unittest.mock import patch

import benchmark


class FailureHandlingTest(unittest.TestCase):
    def test_metrics_io_and_decode_failures(self):
        with patch.object(
            benchmark.urllib.request, "urlopen", side_effect=OSError("offline")
        ):
            self.assertIsNone(benchmark.get("http://example.invalid"))
        with patch.object(
            benchmark.urllib.request, "urlopen", return_value=io.BytesIO(b"\xff")
        ):
            self.assertIsNone(benchmark.get("http://example.invalid"))

    def test_request_schema_and_io_failures(self):
        with patch.object(
            benchmark.urllib.request,
            "urlopen",
            return_value=io.BytesIO(b'{"choices":[]}'),
        ):
            self.assertFalse(
                benchmark.request("http://example.invalid", "m", [], 1, 1)["ok"]
            )
        with patch.object(
            benchmark.urllib.request, "urlopen", side_effect=OSError("offline")
        ):
            self.assertFalse(
                benchmark.request("http://example.invalid", "m", [], 1, 1)["ok"]
            )

    def test_unexpected_errors_propagate(self):
        with patch.object(
            benchmark.urllib.request, "urlopen", side_effect=AssertionError("bug")
        ):
            with self.assertRaises(AssertionError):
                benchmark.get("http://example.invalid")
            with self.assertRaises(AssertionError):
                benchmark.request("http://example.invalid", "m", [], 1, 1)


if __name__ == "__main__":
    unittest.main()
