#!/bin/bash
# Self-test tables headless on Linux: each plays itself (FullTiltEngine --selftest) and the result is listed.
#   ft2/tools/selftest.sh ENGINE DIR [SECONDS]
# DIR holds the tables' .DAT files (and their programs, for texts and art), or one folder per table.
# Needs a display: under CI or SSH run it inside xvfb-run.
ENGINE=$(readlink -f "$1"); DIR=$2; SECONDS_EACH=${3:-120}
LOG="$(dirname "$ENGINE")/FullTiltEngine.log"; failed=0
for dat in $(find "$DIR" -maxdepth 2 -iname '*.dat' | grep -v -i -E 'core[ab]\.dat|partout' | sort); do
  folder=$(dirname "$dat"); name=$(basename "$dat")
  before=$( [ -f "$LOG" ] && wc -l < "$LOG" || echo 0 )
  (cd "$folder" && SDL_AUDIODRIVER=dummy timeout $((SECONDS_EACH + 60)) "$ENGINE" --table "$name" --selftest "$SECONDS_EACH" >/dev/null 2>&1)
  code=$?
  result=$(tail -n +$((before + 1)) "$LOG" | grep -m1 -E 'SELFTEST|CRASH' || echo "no result (exit code $code)")
  [ $code -ne 0 ] || ! echo "$result" | grep -q 'result=PASS' && failed=$((failed + 1))
  printf '%-14s %s\n' "$name" "$result"
done
[ $failed -eq 0 ] && echo "All tables passed" || { echo "$failed table(s) failed"; exit 1; }
