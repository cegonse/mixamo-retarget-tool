BUILD_DIR := build
WEB_BUILD_DIR := build-web
CEST_RUNNER := external/cest/cest-runner

.PHONY: all init test clean ensure-init configure init-web ensure-init-web web web-serve web-dist

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

init-web:
	./scripts/init-web.sh

ensure-init-web:
	@if [ ! -f external/raylib/lib/libraylib.web.a ]; then ./scripts/init-web.sh; fi

web: ensure-init-web
	@emcmake cmake -S web-tool -B $(WEB_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release >/dev/null
	cmake --build $(WEB_BUILD_DIR) --target anim-retarget-web -j

web-serve: web
	python3 -m http.server --directory $(WEB_BUILD_DIR) 8080

WEB_DIST_DIR := $(WEB_BUILD_DIR)/dist
WEB_DIST_FILES := index.html app.js style.css anim-retarget-web.js anim-retarget-web.wasm

web-dist: web
	rm -rf $(WEB_DIST_DIR)
	mkdir -p $(WEB_DIST_DIR)/mappings
	cp $(addprefix $(WEB_BUILD_DIR)/,$(WEB_DIST_FILES)) $(WEB_DIST_DIR)/
	cp $(WEB_BUILD_DIR)/mappings/*.map $(WEB_DIST_DIR)/mappings/
	@echo "deployable files in $(WEB_DIST_DIR):" && cd $(WEB_DIST_DIR) && find . -type f | sort

clean:
	rm -rf $(BUILD_DIR) $(WEB_BUILD_DIR)
