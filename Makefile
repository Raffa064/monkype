CFLAGS = -DNCURSES_WIDECHAR=1 -D_POSIX_C_SOURCE=200809L -Wall -pedantic -std=c99 -g 
LDFLAGS = -lncursesw 

PREFIX ?= /usr/local
INSTALL_DIR := $(DESTDIR)$(PREFIX)/bin

SCRIPTS = $(wildcard ./scripts/*)
INSTALLED_SCRIPTS := $(patsubst ./scripts/%.sh,$(INSTALL_DIR)/monkype-%,$(SCRIPTS))
INSTALLED_SCRIPTS := $(patsubst ./scripts/%.py,$(INSTALL_DIR)/monkype-%,$(INSTALLED_SCRIPTS))

all: build/monkype
	./build/monkype

$(INSTALL_DIR):
	mkdir -p $(INSTALL_DIR)

$(INSTALL_DIR)/monkype-%: scripts/%.py | $(INSTALL_DIR)
	echo "#!/usr/bin/env python3" > $@
	echo "MONKYPE_INSTALL_DIR = '$(INSTALL_DIR)'" >> $@
	cat $^ >> $@

$(INSTALL_DIR)/monkype-%: scripts/%.sh | $(INSTALL_DIR)
	echo "#!/usr/bin/env bash" > $@
	echo "MONKYPE_INSTALL_DIR='$(INSTALL_DIR)'" >> $@
	cat $^ >> $@

build:
	mkdir -p build
	echo '*' > build/.gitignore

build/monkype: main.c | build
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

install: build/monkype $(INSTALLED_SCRIPTS)
	chmod 755 $(INSTALLED_SCRIPTS)
	install -Dm755 ./build/monkype $(INSTALL_DIR)/monkype

uninstall:
	rm -f $(INSTALL_DIR)/monkype*

clean:
	rm -rf build

.PHONY: all install uninstall clean
