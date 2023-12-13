all:
	find src/* -maxdepth 0 -exec $(MAKE) -C {} \;

clean:
	find src/* -maxdepth 0 -exec $(MAKE) -C {} clean \;

.PHONY: clean
