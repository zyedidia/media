LFICC=/opt/aarch64-lfi-clang/bin/clang
LFICXX=/opt/aarch64-lfi-clang/bin/clang++

lib: gen/stub.elf
	lfi-bind -gen-lib $(CURDIR)/gen/stub.elf -arch arm64 -dir gen libopus/build/libopus.a
libopus/build/libopus.a:
	cd libopus && \
	rm -rf build && \
	mkdir build && \
	cd build && \
	cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=$(LFICC) && \
	ninja
gen/stub.elf: libopus/build/libopus.a
	lfi-bind -gen-stub -arch arm64 -dir gen $<
	$(LFICC) gen/embed/stub/thread.c \
		gen/embed/stub/arch/arm64/main.s \
		$< -o $@ -static-pie -O2

.PHONY: lib
