CMAKE ?= cmake
AARCH64_READELF ?= aarch64-linux-gnu-readelf

PC_BUILD_DIR := build/pc-debug
RK_BUILD_DIR := build/rk3568-aarch64-sysroot
TOOLCHAIN_FILE := cmake/aarch64-toolchain.cmake
SYSROOT := $(HOME)/toolchains/rk3568-sysroot-clean
PAHO_INCLUDE_DIR := $(SYSROOT)/usr/local/include
PAHO_LIBRARY := $(SYSROOT)/usr/local/lib/libpaho-mqtt3c.so.1.3.13
RK_LINKER_FLAGS := -L$(SYSROOT)/usr/lib/aarch64-linux-gnu -L$(SYSROOT)/lib/aarch64-linux-gnu -Wl,-rpath-link,$(SYSROOT)/lib/aarch64-linux-gnu

RK_HOST ?= cat@192.168.0.105
RK_BINARY := /tmp/main_demo
RK_SERIAL_LINK := /tmp/rk_gateway
RK_SERIAL_DEVICE := /dev/ttyS3

.PHONY: pc-test rk-configure rk-build rk-check rk-deploy rk-run

pc-test:
	$(CMAKE) --build $(PC_BUILD_DIR) --parallel 2
	ctest --test-dir $(PC_BUILD_DIR) --output-on-failure

rk-configure:
	$(CMAKE) -S . -B $(RK_BUILD_DIR) \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
		-DCMAKE_EXE_LINKER_FLAGS="$(RK_LINKER_FLAGS)" \
		-DGATEWAY_ENABLE_SANITIZERS=OFF \
		-DPAHO_INCLUDE_DIR=$(PAHO_INCLUDE_DIR) \
		-DPAHO_MQTT3C_LIBRARY=$(PAHO_LIBRARY)

rk-build: rk-configure
	$(CMAKE) --build $(RK_BUILD_DIR) --target main_demo --parallel 2

rk-check: rk-build
	file $(RK_BUILD_DIR)/main_demo
		LC_ALL=C $(AARCH64_READELF) -h $(RK_BUILD_DIR)/main_demo | grep 'Machine:.*AArch64'

rk-deploy: rk-check
	scp $(RK_BUILD_DIR)/main_demo $(RK_HOST):$(RK_BINARY)
	ssh $(RK_HOST) 'set -e; \
		if [ -L $(RK_SERIAL_LINK) ]; then \
			test "$$(readlink -f $(RK_SERIAL_LINK))" = $(RK_SERIAL_DEVICE); \
		elif [ -e $(RK_SERIAL_LINK) ]; then \
			echo "$(RK_SERIAL_LINK) exists and is not a symlink"; exit 1; \
		else \
			ln -s $(RK_SERIAL_DEVICE) $(RK_SERIAL_LINK); \
		fi; \
		chmod +x $(RK_BINARY); \
		ldd $(RK_BINARY); \
		! ldd $(RK_BINARY) | grep -q "not found"'

rk-run: rk-deploy
	ssh -t $(RK_HOST) 'exec $(RK_BINARY)'