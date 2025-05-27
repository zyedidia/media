LFICC=/opt/aarch64-lfi-sysext-clang/bin/clang
LFICXX=/opt/aarch64-lfi-sysext-clang/bin/clang++

lib: gen/stub.elf
	lfi-bind -gen-lib $(CURDIR)/gen/stub.elf -arch arm64 -dir gen build/libvpx.a
gen/stub.elf: build/libvpx.a
	lfi-bind -gen-stub -arch arm64 -dir gen $<
	$(LFICC) gen/embed/stub/thread.c \
		gen/embed/stub/arch/arm64/main.s \
		$< -o $@ -static-pie -O2

.PHONY: lib
