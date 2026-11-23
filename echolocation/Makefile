# Host DSP tests without PlatformIO (uses test/shim/unity.h if real Unity is unavailable).
.PHONY: test
test:
	mkdir -p build
	gcc -std=c11 -Wall -Wextra -Werror -Ilib/dsp -Itest/shim test/test_dsp/test_dsp.c lib/dsp/dsp.c -lm -o build/test_dsp
	./build/test_dsp
