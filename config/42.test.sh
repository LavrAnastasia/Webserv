#!/bin/sh

set -e

tester=${1:?usage: config/42.test.sh <path to cgi_tester>}
root=YoupiBanane

mkdir -p "$root/directory/nop" "$root/directory/Yeah"
touch "$root/youpi.bad_extension" \
      "$root/directory/youpi.bad_extension" \
      "$root/directory/youpi.bla" \
      "$root/directory/nop/youpi.bad_extension" \
      "$root/directory/nop/other.pouic" \
      "$root/directory/Yeah/not_happy.bad_extension"
cp "$tester" /tmp/cgi_tester
chmod +x /tmp/cgi_tester
