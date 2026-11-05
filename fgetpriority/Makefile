FGETPRIORITY_VERSION := 0.0.4

ifndef KP_DIR
    KP_DIR = ../KernelPatch
endif

OS_NAME = $(shell uname | tr A-Z a-z)
MACHINE = $(shell uname -m)
NDK_BIN_DIR := toolchains/llvm/prebuilt/$(OS_NAME)-$(MACHINE)/bin
ifdef ANDROID_NDK_LATEST_HOME
    NDK_PATH ?= $(ANDROID_NDK_LATEST_HOME)/$(NDK_BIN_DIR)
else ifdef ANDROID_NDK
    NDK_PATH ?= $(ANDROID_NDK)/$(NDK_BIN_DIR)
else ifdef ANDROID_NDK_HOME
    NDK_PATH ?= $(ANDROID_NDK_HOME)/$(NDK_BIN_DIR)
endif

ifdef TARGET_COMPILE
    CC := $(TARGET_COMPILE)gcc
    LD := $(TARGET_COMPILE)ld
    OBJCOPY := $(TARGET_COMPILE)objcopy
else ifdef NDK_PATH
    CC := $(NDK_PATH)/aarch64-linux-android31-clang
    LD := $(NDK_PATH)/ld.lld
    OBJCOPY := $(NDK_PATH)/llvm-objcopy
endif

CFLAGS = -Wall -O0 \
    -ffreestanding \
    -fno-inline \
    -fno-builtin \
    -fno-strict-aliasing \
    -fno-PIC \
    -fno-pie \
    -fno-asynchronous-unwind-tables \
    -fno-unwind-tables \
    -fno-stack-protector \
    -fno-common \
    -fno-exceptions

CFLAGS += -DFGETPRIORITY_VERSION=\"$(FGETPRIORITY_VERSION)\"
CFLAGS += -std=gnu99

INCLUDE_DIRS := . include patch/include linux linux/include linux/arch/arm64/include linux/tools/arch/arm64/include

INCLUDE_FLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(KP_DIR)/kernel/$(dir))

objs := fgetpriority.o

all: check_kp_dir fgetpriority_$(FGETPRIORITY_VERSION).kpm

# "linux/kernel.h: No such file or directory" at the very first #include
# almost always means KP_DIR doesn't point at an initialized KernelPatch
# checkout (submodule not pulled), not a real missing-header bug. Fail fast
# with a clear reason instead of a confusing compiler error.
.PHONY: check_kp_dir
check_kp_dir:
	@if [ ! -f "$(KP_DIR)/kernel/linux/include/linux/kernel.h" ]; then \
		echo "error: KernelPatch headers not found under KP_DIR=$(KP_DIR)"; \
		echo "       (expected $(KP_DIR)/kernel/linux/include/linux/kernel.h to exist)"; \
		echo "       Run: git submodule update --init --recursive"; \
		echo "       or build with: make KP_DIR=/path/to/KernelPatch"; \
		exit 1; \
	fi

fgetpriority_$(FGETPRIORITY_VERSION).kpm: ${objs}
	${CC} -r -nostdlib -Wl,--no-eh-frame-hdr -o $@ $^
ifneq ($(strip $(OBJCOPY)),)
	${OBJCOPY} --remove-section=.eh_frame --remove-section=.eh_frame_hdr --remove-section=.rela.eh_frame $@
endif

%.o: %.c
	${CC} $(CFLAGS) $(INCLUDE_FLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf *.kpm
	find . -name "*.o" | xargs rm -f
