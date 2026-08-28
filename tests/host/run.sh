#!/bin/sh
# The pure halves of the firmware, compiled by a desktop compiler.
#
# Two harnesses, and both exist because the thing they check cannot be checked
# on a clock in any reasonable time:
#
#   test_factory_profile   the evaluator, against the golden vectors the
#                          Python model writes - the same questions, and the
#                          same integer percentage required back
#   test_residual_store    the user layer, whose every rule is about which of
#                          two statements survives a month of ordinary use
#
# There is no Arduino here and there must not be: the day either of these needs
# a shim is the day the module it tests stopped being pure.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=${TMPDIR:-/tmp}/qlock-host-tests
mkdir -p "$out"

"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror \
    -o "$out/factory-profile" \
    "$here/test_factory_profile.cpp" "$root/src/FactoryProfile.cpp"
"$out/factory-profile" "$root/tests/golden"

"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror \
    -o "$out/residual-store" \
    "$here/test_residual_store.cpp" "$root/src/ResidualStore.cpp"
"$out/residual-store"
