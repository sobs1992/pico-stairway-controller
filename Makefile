TARGET=main
BUILD_DIR=build
BOARD=pico_w

TOOLS_PATHS=
ifdef PICO_TOOL_PATH
TOOLS_PATHS+=-Dpicotool_DIR=$(PICO_TOOL_PATH)
endif

UF2_FILE = $(BUILD_DIR)/$(TARGET).uf2

MAKEFLAGS += --no-print-directory

ASSETS_PATH=network/http_server/assets
ASSETS_GEN_PATH=network/http_server/generated
ASSETS_SRC_FILES := $(shell find $(ASSETS_PATH) -type f)
ASSETS_DST_FILES := $(patsubst $(ASSETS_PATH)/%, $(ASSETS_GEN_PATH)/%.h, $(ASSETS_SRC_FILES))

.PHONY: all assets prepare build clean flash run

all: build

assets: $(ASSETS_DST_FILES)

# Regenerate an asset header only when its source changes
$(ASSETS_GEN_PATH)/%.h: $(ASSETS_PATH)/%
	@mkdir -p "$(dir $@)"
	html-minifier-terser \
		--collapse-whitespace \
		--remove-comments \
		--minify-css true \
		--minify-js true \
		"$<" > "$@.min"
	hexdump -v -e '1/1 "0x%02X, " "\n"' "$@.min" > "$@"
	@rm -f "$@.min"

prepare: assets
	cmake -S . -B $(BUILD_DIR) -DPICO_BOARD=$(BOARD) $(TOOLS_PATHS)

build: prepare
	cmake --build $(BUILD_DIR) --target $(TARGET) -j$(shell nproc)

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(ASSETS_GEN_PATH)

flash: build
	@echo "Waiting for Raspberry Pi Pico in BOOTSEL mode..."
	@while [ -z "$$(lsblk -o LABEL,MOUNTPOINT -nr | awk '$$1=="RPI-RP2" {print $$2}')" ]; do \
		sleep 1; \
	done
	@PICO_DRIVE="$$(lsblk -o LABEL,MOUNTPOINT -nr | awk '$$1=="RPI-RP2" {print $$2}')"; \
	echo "Pico detected at $$PICO_DRIVE"; \
	cp $(UF2_FILE) "$$PICO_DRIVE"
	@echo "Done!"

run: flash
	sleep 1
	putty -serial /dev/ttyACM0 -sercfg 115200