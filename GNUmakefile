MAKEFLAGS = -j$(shell nproc)

CC = cc
CFLAGS = \
	-Wall -Wextra -Wpedantic -Werror \
	-Wno-unused-parameter -D_GNU_SOURCE \
	-pipe -std=c11 \
	$(shell pkg-config --cflags $(libs) 2>/dev/null)
OFLAGS = -O3 -march=native -mtune=native
LDLIBS = $(shell pkg-config --libs $(libs) 2>/dev/null)

srcs = $(wildcard src/$@/*.c)
objs = $(patsubst src/%.c,obj/%.o, $(srcs))

targets = ewd ewctl
objdirs = $(addprefix obj/,$(targets))

all: $(objdirs) $(targets)

ewctl: libs += libjxl libjxl_threads
ewd ewctl: libs += wayland-client

.SECONDEXPANSION:
$(targets): $$(objs)
	@$(CC) $(LDLIBS) -o $@ $(objs)
	@printf 'LD\t%s\n' $@

$(objdirs)&:
	@mkdir -p $(objdirs)
	@printf 'MKDIR\t%s\n' $(objdirs)

obj/%.o: src/%.c
	@$(CC) $(CFLAGS) -o $@ -c $<
	@printf 'CC\t%s\n' $@

clean:
	rm -rf obj $(targets)

release: CFLAGS += $(OFLAGS)
release: all

.PHONY: clean release
