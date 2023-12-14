MAKEFLAGS = --no-print-directory

gen = find src/* -maxdepth 0 -not -name 'common' -exec $(MAKE) -C {} $1 \;

all:
	$(call gen)

clean:
	$(call gen,clean)

install:
	$(call gen,install)

.PHONY: clean install
