CC := gcc
CFLAGS := -Wall -Wextra -Wshadow -g -O0
LDFLAGS :=

TARGET := termux-build-core
SOURCES := termux-build-core.c sysexec.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	./$(TARGET) --help 2>&1 | grep -q "Usage" && echo "✓ Binary runs" || echo "✗ Binary failed"

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	install -D -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)
