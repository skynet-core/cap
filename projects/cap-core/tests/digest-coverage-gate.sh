#!/bin/sh
set -eu

awk -v source="$2" '
  /^SF:/ { in_scope = substr($0, 4) == source; if (in_scope) records++ }
  in_scope && /^LF:/ { found = substr($0, 4); found_count++ }
  in_scope && /^LH:/ { hit = substr($0, 4); hit_count++ }
  in_scope && /^end_of_record$/ { terminated++; in_scope = 0 }
  END {
    if (records != 1 || terminated != 1 || found_count != 1 || hit_count != 1 ||
        found !~ /^[0-9]+$/ || hit !~ /^[0-9]+$/ || found + 0 == 0 || hit + 0 > found + 0) {
      print "ERROR: missing or invalid digest coverage record" > "/dev/stderr"
      exit 1
    }
    printf "digest coverage gate: %d/%d lines (minimum 95%%)\n", hit, found
    if (100 * hit < 95 * found) {
      print "ERROR: digest line coverage below 95%" > "/dev/stderr"
      exit 1
    }
  }
' "$1"
