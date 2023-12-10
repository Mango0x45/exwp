MAKEFLAGS = -j$(shell nproc)

CC = cc
CFLAGS = \
	-Wall -Wextra -Wpedantic -Werror \
	-Wno-unused-parameter -D_GNU_SOURCE \
	-pipe -std=c11
OFLAGS = -O3 -march=native -mtune=native

srcs = $(wildcard src/$@/*.c)
objs = $(patsubst src/%.c,obj/%.o, $(srcs))

targets = ewd ewctl png2xrgb
objdirs = $(addprefix obj/,$(targets))

all: $(objdirs) $(targets)

ewd ewctl: LDLIBS += -lwayland-client
png2xrgb: LDLIBS += -lm
png2xrgb: CFLAGS += -Wno-unused-function

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
