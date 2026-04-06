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

all: build

prepare:
	cmake -S . -B $(BUILD_DIR) -DPICO_BOARD=$(BOARD) $(TOOLS_PATHS)

build: prepare
	cmake --build $(BUILD_DIR) --target $(TARGET) -j12

clean:
	rm -rf $(BUILD_DIR)

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