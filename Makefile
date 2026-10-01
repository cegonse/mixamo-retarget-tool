BUILD_DIR := build
CEST_RUNNER := external/cest/cest-runner

.PHONY: all init test clean ensure-init configure

all: configure
	cmake --build $(BUILD_DIR) --target anim-retarget -j

init:
	./scripts/init.sh

ensure-init:
	@if [ ! -x $(CEST_RUNNER) ] || [ ! -s external/cest/cest ] || [ ! -f third_party/cgltf/cgltf.h ]; then ./scripts/init.sh; fi

configure:
	@cmake -S . -B $(BUILD_DIR) >/dev/null

test: ensure-init all
	cmake --build $(BUILD_DIR) --target build_tests -j
	$(CEST_RUNNER) $(BUILD_DIR)/

clean:
	rm -rf $(BUILD_DIR)
