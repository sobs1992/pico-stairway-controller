TARGET=main
BUILD_DIR=build
BOARD=pico_w

TOOLS_PATHS=
ifdef PICO_TOOL_PATH
TOOLS_PATHS+=-Dpicotool_DIR=$(PICO_TOOL_PATH)
endif

UF2_FILE = $(BUILD_DIR)/$(TARGET).uf2
PICO_DRIVE = /media/$(USER)/RPI-RP2

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
		hexdump -v -e '1/1 "0x%02X, " "\n"' "$$f" > "$$out"; \
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
	@while [ ! -d "$(PICO_DRIVE)" ]; do \
		sleep 1 > /dev/null 2>&1;\
	done
	@echo "Pico detected. Flashing..."
	cp $(UF2_FILE) $(PICO_DRIVE)
	@echo "Done!"

run: flash
	sleep 1
	putty -serial /dev/ttyACM0 -sercfg 115200