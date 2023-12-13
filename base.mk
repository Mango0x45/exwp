MAKEFLAGS = -j$(shell nproc)

CC = cc
CFLAGS += \
	-Wall -Wextra -Wpedantic -Werror \
	-D_GNU_SOURCE -pipe -O3 \
	-march=native -mtune=native \
	$(shell pkg-config --cflags $(lib) 2>/dev/null)
LDLIBS += $(shell pkg-config --libs $(lib) 2>/dev/null)

all: $(bin)
$(bin): $(obj)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $^

clean:
	rm -f $(bin) $(obj)

.PHONY: clean
