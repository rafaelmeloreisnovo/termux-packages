.PHONY: all install

PREFIX ?= /usr/local

all: hello

hello: hello.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o hello hello.c

install: hello
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	cp hello "$(DESTDIR)$(PREFIX)/bin/"

clean:
	rm -f hello
