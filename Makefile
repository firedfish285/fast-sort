CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra
TARGET  = fast_sort

# macOS 若默认 SDK 链接报错（tapi error 等），可指定旧版 SDK：
#   make SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk
ifdef SYSROOT
CFLAGS += -isysroot $(SYSROOT)
endif

all: $(TARGET)

$(TARGET): fast_sort.c
	$(CC) $(CFLAGS) -o $@ $<

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all test clean
