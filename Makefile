TARGET = HarmonicusLegio

.SHELLFLAGS += -e

DEBUG ?= 0

LIBDAISY_DIR = libDaisy
DAISYSP_DIR = DaisySP
CMSIS_DIR = $(LIBDAISY_DIR)/Drivers/CMSIS

.PHONY: project
project: submodules $(LIBDAISY_DIR) $(DAISYSP_DIR) all

.PHONY: submodules
submodules:
	git submodule update --init --recursive

.PHONY: flash
flash: program-dfu

.PHONY: libDaisy
libDaisy:
	$(MAKE) -C $(LIBDAISY_DIR) -j

.PHONY: DaisySP
DaisySP: libDaisy
	$(MAKE) -C $(DAISYSP_DIR) -j

CPP_SOURCES := HarmonicusLegio.cpp
C_DEFS += -DSHIFT_BUFFER_SIZE=2400

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

lint:
	python ./libDaisy/ci/run-clang-format.py -r ./ \
		--extensions c,cpp,h
