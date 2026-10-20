#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_binary=$(mktemp /tmp/hud-model-test.XXXXXX)
trap 'rm -f "$test_binary"' EXIT
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -Isrc/state_machine tests/test_hud_model.c src/state_machine/hud_model.c \
  -o "$test_binary"
ASAN_OPTIONS=detect_leaks=0 "$test_binary"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -Itests/fakes tests/test_button.c -o "$test_binary"
ASAN_OPTIONS=detect_leaks=0 "$test_binary"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -Isrc/wifi tests/test_wifi_retry.c src/wifi/wifi_retry.c -o "$test_binary"
ASAN_OPTIONS=detect_leaks=0 "$test_binary"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -Itests/wifi_fakes -Itests/fakes tests/test_wifi_manager.c src/wifi/wifi_retry.c \
  -o "$test_binary"
ASAN_OPTIONS=detect_leaks=0 "$test_binary"
