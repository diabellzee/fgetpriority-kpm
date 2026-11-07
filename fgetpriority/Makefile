FGETPRIORITY_VERSION := 0.0.4
V ?= 0

SELF_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
REPO_ROOT := $(patsubst %/,%,$(dir $(SELF_DIR)))

KP_MARKER := kernel/linux/include/linux/kernel.h
ifndef KP_DIR
    KP_CANDIDATES := $(REPO_ROOT)/KernelPatch $(CURDIR)/../KernelPatch $(CURDIR)/KernelPatch
    KP_FOUND := $(firstword $(foreach d,$(KP_CANDIDATES),$(if $(wildcard $(d)/$(KP_MARKER)),$(d))))
    ifneq ($(KP_FOUND),)
        KP_DIR := $(KP_FOUND)
    else
        KP_DIR := $(REPO_ROOT)/KernelPatch
    endif
endif
KP_DIR := $(patsubst %/,%,$(KP_DIR))

OS_NAME := $(shell uname | tr A-Z a-z)
MACHINE := $(shell uname -m)
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
    OBJCOPY := $(TARGET_COMPILE)objcopy
else ifdef NDK_PATH
    CC := $(NDK_PATH)/aarch64-linux-android31-clang
    OBJCOPY := $(NDK_PATH)/llvm-objcopy
endif

CC_ORIGIN := $(origin CC)

CFLAGS := -Wall -Wextra -O2 \
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
    -fno-exceptions \
    -std=gnu99 \
    -DFGETPRIORITY_VERSION=\"$(FGETPRIORITY_VERSION)\" \
    -MMD -MP

INCLUDE_DIRS := . include patch/include linux linux/include linux/arch/arm64/include linux/tools/arch/arm64/include
INCLUDE_FLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(KP_DIR)/kernel/$(dir))

srcs := $(wildcard *.c)
objs := $(srcs:.c=.o)
deps := $(objs:.o=.d)
target := fgetpriority_$(FGETPRIORITY_VERSION).kpm

ifeq ($(V),0)
    Q := @
else
    Q :=
endif

.PHONY: all clean help check_kp_dir check_toolchain check_arch

all: check_kp_dir check_toolchain check_arch $(target)

check_kp_dir:
	$(Q)if [ ! -f "$(KP_DIR)/$(KP_MARKER)" ]; then \
		echo "error: KernelPatch headers not found (checked: $(KP_CANDIDATES))"; \
		echo "       resolved KP_DIR=$(KP_DIR)"; \
		echo "       expected to find: $(KP_DIR)/$(KP_MARKER)"; \
		echo "       fix: git -C \"$(REPO_ROOT)\" submodule update --init --recursive"; \
		echo "       or:  make KP_DIR=/path/to/KernelPatch"; \
		exit 1; \
	fi

check_toolchain:
	$(Q)if [ "$(CC_ORIGIN)" = "default" ] || [ "$(CC_ORIGIN)" = "undefined" ] || [ -z "$(strip $(CC))" ]; then \
		echo "error: no aarch64 cross compiler selected (CC would default to the host's '$(CC)')."; \
		echo "       set TARGET_COMPILE=prefix- (e.g. aarch64-none-elf-, aarch64-linux-gnu-)"; \
		echo "       or point ANDROID_NDK_HOME at an installed NDK"; \
		echo "       or pass CC=/path/to/aarch64-*-gcc explicitly"; \
		exit 1; \
	else \
		cc_bin="$(firstword $(CC))"; \
		if ! command -v "$$cc_bin" >/dev/null 2>&1 && [ ! -x "$$cc_bin" ]; then \
			echo "error: compiler '$$cc_bin' (from CC=$(CC)) not found or not executable"; \
			exit 1; \
		fi; \
	fi

check_arch: $(objs)
	$(Q)for f in $(objs); do \
		m=$$(od -An -tu2 -j 18 -N 2 "$$f" 2>/dev/null | tr -d ' '); \
		if [ "$$m" != "183" ]; then \
			echo "error: $$f is not an AArch64 object (e_machine=$${m:-?}, expected 183)"; \
			echo "       CC=$(CC) is not an aarch64 cross compiler"; \
			exit 1; \
		fi; \
	done

$(target): $(objs)
	$(Q)echo "  LD      $@"
	$(Q)$(CC) -r -nostdlib -Wl,--no-eh-frame-hdr -o $@ $^
ifneq ($(strip $(OBJCOPY)),)
	$(Q)echo "  OBJCOPY $@"
	$(Q)$(OBJCOPY) --remove-section=.eh_frame --remove-section=.eh_frame_hdr --remove-section=.rela.eh_frame $@
endif
	$(Q)echo "built: $@ (KP_DIR=$(KP_DIR))"

%.o: %.c
	$(Q)echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) $(INCLUDE_FLAGS) -c -o $@ $<

-include $(deps)

clean:
	$(Q)rm -f $(objs) $(deps) *.kpm

help:
	@echo "targets: all (default), clean"
	@echo "vars:    KP_DIR=path          override KernelPatch checkout (auto-detected otherwise)"
	@echo "         TARGET_COMPILE=pfx   e.g. aarch64-none-elf- / aarch64-linux-gnu-"
	@echo "         V=1                  verbose build output"
