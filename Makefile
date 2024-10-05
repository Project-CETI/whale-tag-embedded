#Change target to build substeps in Docker
TARGET ?= $(TARGET_IMG)

SHELL := /bin/bash
MAKEFILE_DIR := $(shell realpath $(dir $(lastword $(MAKEFILE_LIST))))
DOCKER_IMAGE ?= sdcard-builder
PACKAGE_DIR ?= packages
#Todo: package list can probably be programatically determined
PACKAGES = ceti-tag-data-capture

DOS2UNIX_FILES = $(shell ls $(BUILD_DIR)/*.sh \
	$(PACKAGE_DIR)/*.sh \
	$(PACKAGE_DIR)/*/debian/* \
	2>/dev/null)

OVERLAY_DIR ?= overlay
OVERLAY_FILES = $(shell find $(OVERLAY_DIR) -type f 2> /dev/null)

OUT_DIR ?= out
TARGET_IMG = $(OUT_DIR)/sdcard.img

IMG_DIR ?= raspios
RASPIOS_IMG = $(IMG_DIR)/raspios.img
ENV_IMG = $(IMG_DIR)/environment.img

#Generated directories
DIRS = $(IMG_DIR) $(OUT_DIR)

#Tools
BUILD_DIR ?= build
PACKAGE_BUILD = $(BUILD_DIR)/make_dpkg.sh
ENV_SETUP = $(BUILD_DIR)/setup_image.sh
PACKAGE_INSTALL = $(BUILD_DIR)/install_packages.sh
BUILD_SCRIPTS = $(PACKAGE_BUILD) $(ENV_SETUP) $(PACKAGE_INSTALL)
DOS2UNIX_TIMESTAMPS = $(patsubst %.sh, %.timestamp, $(BUILD_SCRIPTS))

RPI_APPEND   = sudo $(BUILD_DIR)/rpi-image append
RPI_DOWNLOAD = $(BUILD_DIR)/rpi-image download
RPI_EXPAND   = sudo $(BUILD_DIR)/rpi-image expand
RPI_RUN 	 = sudo $(BUILD_DIR)/rpi-image run
RPI_TOOL_TS = $(BUILD_DIR)/rpi-image.timestamp

.PHONY: \
	help \
	clean \
	deep_clean \
	build \
	test \
	packages \
	lint \
	docker-image \
	docker-image-remove \
	docker-shell \
	$(PACKAGES)

.DEFAULT := build-sdcard-img

help:
	@echo "make build"
	@echo "make clean"
	@echo "make deep_clean"
	@echo "make packages"

#convert dos2unix files
$(DOS2UNIX_TIMESTAMPS): %.timestamp : %.sh
	dos2unix $^
	chmod a+x $^
	touch $@

$(RPI_TOOL_TS): %.timestamp : %
	dos2unix $^
	chmod a+x $^
	touch $@

#Builds target inside docker image
# This should be the default way to build as it standardizes the build
# environment with Docker (so we don't have to ship our PC to anyone trying 
# to use the code). 
# - However Docker CAN be bypassed by running nonPHONY targets `make <target_file>`. 
# - Likewise Docker can be used to make any valid target by manually
# setting the `TARGET` variable for the `build` recipe. See `packages` recipe
# for example. 
build: $(DOCKER_IMAGE)
	@echo "Building $(TARGET) inside docker image"
	docker run --privileged -i --tty --workdir /whale-tag-embedded \
		--volume $(shell pwd):/whale-tag-embedded \
		$(DOCKER_IMAGE) /bin/bash -c ' \
			groupadd --gid $(shell id -g) $(shell id -g -n); \
			useradd -m -e "" -s /bin/bash --gid $(shell id -g) --uid $(shell id -u) $(shell id -u -n); \
			passwd -d $(shell id -u -n); \
			echo "$(shell id -u -n) ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers; \
			sudo -E -u $(shell id -u -n) $(MAKE) $(TARGET)'

clean:
	rm -f $(DOS2UNIX_TIMESTAMPS) $(RPI_TOOL_TS)
	rm -rf $(BUILD_DIR)/__pycache__
	rm -rf $(OUT_DIR)

deep_clean: clean docker-image-remove
	$(foreach dir, $(DIR), rm -rf $(dir);)
	$(foreach package, $(PACKAGES), $(MAKE) clean -C $(PACKAGE_DIR)/$(package);)

test:
	$(foreach package, $(PACKAGES), $(MAKE) test -C $(PACKAGE_DIR)/$(package);)

packages:
	@$(MAKE) build TARGET="$(PACKAGES)"

# Create directories
$(DIRS):
	mkdir -p $@

# Download starting image
$(RASPIOS_IMG): | $(IMG_DIR)
	@echo "Downloading the latest raspios..."
	$(RPI_DOWNLOAD) --suffix raspios-bullseye-arm64-lite --output "$@"

# Setup raspberry pi environment
$(ENV_IMG): $(RASPIOS_IMG) $(patsubst %.sh, %.timestamp, $(ENV_SETUP)) $(OVERLAY_FILES) $(RPI_TOOL_TS)
	cp -f $(RASPIOS_IMG) $@.tmp
	$(RPI_EXPAND) --size +512M --image "$@.tmp"
	$(RPI_APPEND) --size 128M --filesystem ext4 --label cetiData --image "$@.tmp"
	$(RPI_RUN) --image "$@.tmp" \
		--bind "$(OVERLAY_DIR):/overlay" \
		--bind-ro "$(ENV_SETUP):/setup_image.sh" \
		"/setup_image.sh" "/overlay"
	mv -f $@.tmp $@

# Create debian packages
$(PACKAGES): $(ENV_IMG) $(patsubst %.sh, %.timestamp, $(PACKAGE_BUILD)) $(RPI_TOOL_TS) | $(OUT_DIR)
	dos2unix $(PACKAGE_DIR)/$@/debian/*
	$(RPI_RUN) --image "$(ENV_IMG)" \
		--bind "$(PACKAGE_DIR)/$@:/$(PACKAGE_DIR)" \
		--bind "$(OUT_DIR):/$(OUT_DIR)" \
		--bind-ro "$(PACKAGE_BUILD):/make_dpkg.sh" \
		"/make_dpkg.sh" "/$(PACKAGE_DIR)" "/$(OUT_DIR)"
	@echo "$$(< $(BUILD_DIR)/logo.txt)"

 
# Generate target image with installed packages
$(TARGET_IMG): $(ENV_IMG) $(PACKAGES) $(patsubst %.sh, %.timestamp, $(PACKAGE_INSTALL)) $(RPI_TOOL_TS)| $(OUT_DIR)
	cp -f $< $@.tmp
	$(RPI_RUN) --image "$@.tmp" \
		--bind "$(PACKAGE_DIR):/packages" \
		--bind "$(OUT_DIR):/out" \
		--bind-ro "$(PACKAGE_INSTALL):/install_packages.sh" \
		"/install_packages.sh" "/out"
	mv -f $@.tmp $@
	@echo "$$(< $(BUILD_DIR)/logo.txt)"
	

lint: 
	docker run \
		-e RUN_LOCAL=true \
		-e VALIDATE_CPP=false \
		-e VALIDATE_DOCKERFILE_HADOLINT=false \
		-e VALIDATE_JSCPD=false \
		-e VALIDATE_NATURAL_LANGUAGE=false \
		-v $(shell pwd):/tmp/lint \
		--rm ghcr.io/super-linter/super-linter:latest

# Docker helpers
$(DOCKER_IMAGE):
	docker build -t $@ build

docker-image: $(DOCKER_IMAGE)

docker-image-remove:
	docker rmi -f ${DOCKER_IMAGE} 2> /dev/null

docker-shell: $(DOCKER_IMAGE)
	docker run --rm --privileged --interactive --tty --workdir /src \
		--volume $(MAKEFILE_DIR):/src $< /bin/bash
