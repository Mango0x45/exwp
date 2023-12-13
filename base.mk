MAKEFLAGS = -j$(shell nproc)

CC = cc
CFLAGS := \
	-Wall -Wextra -Wpedantic -Werror -D_GNU_SOURCE -pipe \
	$(CFLAGS) $(shell pkg-config --cflags $(lib) 2>/dev/null)
LDLIBS += $(shell pkg-config --libs $(lib) 2>/dev/null)

ifeq ($(BUILD),release)
	CFLAGS += -O3 -march=native -mtune=native
else
	CFLAGS += -g -ggdb3
endif

all: $(bin)
$(bin): $(obj)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $^

clean:
	rm -f $(bin) $(obj)

.PHONY: clean
