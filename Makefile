ARCH ?= x86_64

.DEFAULT_GOAL := all

.PHONY: all build kernel image qemu qemu-gui qemu-debug run gui debug clean clean-all help

all build:
	@./build.sh build $(ARCH)

kernel:
	@./build.sh kernel $(ARCH)

image:
	@./build.sh image $(ARCH)

qemu run:
	@./build.sh run $(ARCH)

qemu-gui gui:
	@./build.sh gui $(ARCH)

qemu-debug debug:
	@./build.sh debug $(ARCH)

clean:
	@./build.sh clean $(ARCH)

clean-all:
	@./build.sh clean-all $(ARCH)

help:
	@./build.sh help
