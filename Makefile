SHELL := /bin/bash
MAKEFILE_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
OUT_DIR ?= $(MAKEFILE_DIR)/out

.PHONY: \
	build-debs \
	clean

help:
	@echo "make build-debs"
	@echo "make clean"

build-debs:
	mkdir -p $(OUT_DIR)/debs
	packages/make_dpkg.sh $(OUT_DIR)/debs

# Clean
clean:
	rm -rf $(OUT_DIR)
