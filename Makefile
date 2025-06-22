CMAKE_BUILD_DIR := .build
.DEFAULT_GOAL := build
.PHONY: bench build clean test $(CMAKE_BUILD_DIR)/kvmserver

bench: $(CMAKE_BUILD_DIR)/kvmserver
	$(MAKE) -C examples bench KVMSERVER=$(PWD)/$(CMAKE_BUILD_DIR)/kvmserver

build: $(CMAKE_BUILD_DIR)/Makefile
	$(MAKE) -C $(CMAKE_BUILD_DIR)

clean:
	rm -rf $(CMAKE_BUILD_DIR)
	$(MAKE) -C examples clean

$(CMAKE_BUILD_DIR)/Makefile: CMakeLists.txt
	cmake -DCMAKE_BUILD_TYPE=Release -B $(CMAKE_BUILD_DIR)

$(CMAKE_BUILD_DIR)/kvmserver: $(CMAKE_BUILD_DIR)/Makefile
	$(MAKE) -C $(@D) $(@F)

test: $(CMAKE_BUILD_DIR)/kvmserver
	$(MAKE) -C examples test KVMSERVER=$(PWD)/$(CMAKE_BUILD_DIR)/kvmserver
