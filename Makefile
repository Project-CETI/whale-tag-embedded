SHELL := /bin/bash
MAKEFILE_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
DOCKER_IMAGE ?= sdcard-builder
OUT_DIR ?= $(MAKEFILE_DIR)/out
RASPIOS_IMAGE="$(MAKEFILE_DIR)/raspios/raspios_lite.img"

.PHONY: \
	clean \
	build \

.DEFAULT := build-sdcard-img

help:
	@echo "make build"
	@echo "make clean"

build: docker-image
	dos2unix build/*
	dos2unix packages/*
	dos2unix packages/*/debian/*
	mkdir -p $(OUT_DIR)
	build/build.sh $(OUT_DIR)

clean:
	rm -rf build/__pycache__
	rm -rf $(OUT_DIR)

# Docker helpers
docker-image:
	docker build -t $(DOCKER_IMAGE) build

docker-image-remove:
	docker rmi $(DOCKER_IMAGE)

docker-shell: docker-image
	docker run --rm --privileged --interactive --tty --workdir /src \
		--volume $(MAKEFILE_DIR):/src $(DOCKER_IMAGE) /bin/bash
