MAKEFLAGS = -j$(shell nproc)

CC = cc
CFLAGS := \
	-Wall -Wextra -Wpedantic -Werror \
	-D_GNU_SOURCE -I../common \
	-std=c2x -pipe \
	$(CFLAGS) $(shell pkg-config --cflags $(lib) 2>/dev/null)
LDLIBS += $(shell pkg-config --libs $(lib) 2>/dev/null)

PREFIX = /usr/local
DPREFIX = $(DESTDIR)$(PREFIX)

ifeq ($(BUILD),release)
	CFLAGS += -O3 -march=native -mtune=native -flto
	LDFLAGS += -flto
else
	CFLAGS += -g -ggdb3
endif

mandir  = $(patsubst .%,$(DPREFIX)/share/man/man%,$(suffix $1))
mandirs = $(foreach m,$(man),$(call mandir,$m))

obj = $(src:.c=.o)

all: $(bin)
$(bin): $(obj)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $^

install:
	mkdir -p $(DPREFIX)/bin $(mandirs)
	cp $(bin) $(DPREFIX)/bin
	$(foreach m,$(man),cp $m $(call mandir,$m);)

clean:
	rm -f $(bin) $(obj)

.PHONY: clean install
