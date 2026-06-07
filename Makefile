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

all: build

assets:
	@find $(ASSETS_PATH) -type f | while read f; do \
		out="$(ASSETS_GEN_PATH)/$${f#$(ASSETS_PATH)/}.h"; \
		mkdir -p "$$(dirname $$out)"; \
		html-minifier-terser \
			--collapse-whitespace \
			--remove-comments \
			--minify-css true \
			--minify-js true \
			"$$f" | \
		hexdump -v -e '1/1 "0x%02X, " "\n"' > "$$out"; \
	done

prepare: assets
	cmake -S . -B $(BUILD_DIR) -DPICO_BOARD=$(BOARD) $(TOOLS_PATHS)

build: prepare
	cmake --build $(BUILD_DIR) --target $(TARGET) -j12

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