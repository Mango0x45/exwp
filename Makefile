gen = find src/* -maxdepth 0 -exec $(MAKE) -C {} $1 \;

all:
	$(call gen)

clean:
	$(call gen,clean)

install:
	$(call gen,install)

.PHONY: clean install
