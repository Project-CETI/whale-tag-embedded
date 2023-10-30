SHELL := /bin/bash
MAKEFILE_DIR := $(shell realpath $(dir $(lastword $(MAKEFILE_LIST))))
DOCKER_IMAGE ?= sdcard-builder
OUT_DIR ?= $(MAKEFILE_DIR)/out
PACKAGE_DIR ?= $(MAKEFILE_DIR)/packages
BUILD_DIR ?= $(MAKEFILE_DIR)/build
RASPIOS_IMAGE="$(MAKEFILE_DIR)/raspios/raspios_lite.img"
PACKAGES := $(wildcard $(PACKAGE_DIR)/*/.)

.PHONY: \
	help \
	clean \
	deep_clean \
	build \
	docker-image \
	docker-image-remove \
	docker-shell

.DEFAULT := build-sdcard-img

help:
	@echo "make build"
	@echo "make clean"

build: $(DOCKER_IMAGE)
	dos2unix $(BUILD_DIR)/*
	dos2unix $(PACKAGE_DIR)/*
	dos2unix $(PACKAGE_DIR)/*/debian/*
	mkdir -p $(OUT_DIR)
	$(BUILD_DIR)/build.sh $(OUT_DIR)

clean:
	rm -rf $(BUILD_DIR)/__pycache__
	rm -rf $(OUT_DIR)
	$(foreach package, $(PACKAGES), $(MAKE) clean -C $(package))

test:
	$(foreach package, $(PACKAGES), $(MAKE) test -C $(package))
.PHONY: test

deep_clean: clean docker-image-remove
	rm -rf raspios
	$(foreach package, $(PACKAGES), $(MAKE) clean -C $(package);)

# Docker helpers
$(DOCKER_IMAGE):
	docker build -t $@ build

docker-image: $(DOCKER_IMAGE)

docker-image-remove:
	docker rmi $(DOCKER_IMAGE)

docker-shell: $(DOCKER_IMAGE)
	docker run --rm --privileged --interactive --tty --workdir /src \
		--volume $(MAKEFILE_DIR):/src $< /bin/bash
