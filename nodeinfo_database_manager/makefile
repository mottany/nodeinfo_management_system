############################################################
# Build settings (Linux native / Android NDK / Termux)
############################################################

# Detect NDK root from common env vars (overrideable)
NDK_ROOT ?= $(or $(ANDROID_NDK_HOME),$(NDK_HOME))

# Set ANDROID=1 to enable NDK cross compile
ANDROID ?= 0
# Android ABI: arm64-v8a | armeabi-v7a | x86_64
ABI ?= arm64-v8a
# Android API level
API ?= 21

# Auto-detect compiler if not set
ifeq ($(origin CC), default)
	CC := $(or $(shell command -v gcc 1>/dev/null 2>&1 && echo gcc),$(shell command -v clang 1>/dev/null 2>&1 && echo clang),gcc)
endif

CFLAGS ?= -std=gnu11 -Wall -Wextra -O2 -g
LDFLAGS ?=

TARGET := nodeinfo_database_manager.bin
SRCS := main.c ctrl_service.c nodedata_receiver.c db_manager.c
OBJS := $(SRCS:.c=.o)

# Android NDK toolchain selection
ifeq ($(ANDROID),1)
	ifeq ($(NDK_ROOT),)
		$(error ANDROID=1 が指定されましたが NDK_ROOT(=ANDROID_NDK_HOME も可) が未設定です)
	endif
	HOST_TAG := linux-x86_64
	TOOLCHAIN := $(NDK_ROOT)/toolchains/llvm/prebuilt/$(HOST_TAG)/bin
	ifeq ($(ABI),arm64-v8a)
		CLANG_PREFIX := aarch64-linux-android$(API)
	else ifeq ($(ABI),armeabi-v7a)
		CLANG_PREFIX := armv7a-linux-androideabi$(API)
	else ifeq ($(ABI),x86_64)
		CLANG_PREFIX := x86_64-linux-android$(API)
	else
		$(error 未対応の ABI: $(ABI) (arm64-v8a|armeabi-v7a|x86_64 から選択))
	endif
	CC := $(TOOLCHAIN)/$(CLANG_PREFIX)-clang
	LD := $(CC)
	CFLAGS += -D__ANDROID_API__=$(API)
endif

.PHONY: all clean run rebuild android android-arm64 android-armv7 android-x86_64 test test-run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) test_db_manager.bin

rebuild: clean all

# Convenience targets for Android NDK cross build
android: ANDROID=1
android: all

android-arm64: ANDROID=1
android-arm64: ABI=arm64-v8a
android-arm64: all

android-armv7: ANDROID=1
android-armv7: ABI=armeabi-v7a
android-armv7: all

android-x86_64: ANDROID=1
android-x86_64: ABI=x86_64
android-x86_64: all

# --- tests ---
test: test_db_manager

test_db_manager: db_manager.o
	$(CC) $(CFLAGS) test_db_manager.c db_manager.o -o test_db_manager.bin $(LDFLAGS)

test-run: test_db_manager
	./test_db_manager.bin
