# project configuration
PROJECT_ROOT=$(CURDIR)
PATH+=:$(PROJECT_ROOT)/tools/gnu-tools-for-stm32/bin/:$(CUSTOM_PATH)

WORKSPACE_PATH=./STM32PlatformCode/Firmware/STM32CubeIDE/cmwx1zzabz_0xx
DEBUG_DIR=$(WORKSPACE_PATH)/Debug
RELEASE_DIR=$(WORKSPACE_PATH)/Release
DEBUG_ELF=$(DEBUG_DIR)/cmwx1zzabz_0xx.elf
RELEASE_ELF=$(RELEASE_DIR)/cmwx1zzabz_0xx.elf

FIRMWARE_ROOT=./firmwares
FIRMWARE_DEBUG=$(FIRMWARE_ROOT)/physec-firmware-debug.elf
FIRMWARE_RELEASE=$(FIRMWARE_ROOT)/physec-firmware.elf
STM32_CLI=./stm32-cli
STM32_FLASH=./stm32-flash
LOG_ANALYZER=./log-analyzer

.PHONY: all clean flash flash-debug flash-release list-targets setup \
        clean-stm32-tools clean-firmware clean-log-analyzer \
        clean-debug clean-release clean-stm32-cli clean-stm32-flash \
        debug release stm32-cli stm32-flash log-analyzer \
        stm32-tools all-stm32 all-firmware \
				$(FIRMWARE_DEBUG) $(DEBUG_ELF) $(FIRMWARE_RELEASE) $(RELEASE_ELF)

# main targets
all: all-firmware stm32-tools log-analyzer

# firmware building targets
all-firmware: debug release

debug: $(FIRMWARE_DEBUG)

release: $(FIRMWARE_RELEASE)

# tool building targets
stm32-tools: $(STM32_CLI) $(STM32_FLASH)

# actual file targets
$(DEBUG_ELF):
	PROJECT_ROOT=$(PROJECT_ROOT) $(MAKE) -C $(DEBUG_DIR) all

$(RELEASE_ELF):
	PROJECT_ROOT=$(PROJECT_ROOT) $(MAKE) -C $(RELEASE_DIR) all

$(FIRMWARE_DEBUG): $(DEBUG_ELF)
	@mkdir -p $(FIRMWARE_ROOT)
	cp $(DEBUG_ELF) $@

$(FIRMWARE_RELEASE): $(RELEASE_ELF)
	@mkdir -p $(FIRMWARE_ROOT)
	cp $(RELEASE_ELF) $@

$(STM32_CLI):
	cd tools/stm32-cli && \
		cargo build --release && \
		cd ../.. && \
		ln -sf ./tools/stm32-cli/target/release/stm32-cli $@

$(STM32_FLASH):
	cd tools/stm32-flash && \
		cargo build --release && \
		cd ../.. && \
		ln -sf ./tools/stm32-flash/target/release/stm32-flash $@

$(LOG_ANALYZER):
	cd tools/log-analyzer && \
		cargo build --release && \
		cd ../.. && \
		ln -sf ./tools/log-analyzer/target/release/log-analyzer $@

# flashing targets
flash: $(STM32_FLASH)
	@./$< $(FLASH_FILE)

flash-debug: $(STM32_FLASH) $(FIRMWARE_DEBUG)
	@./$< -e $(FIRMWARE_DEBUG)

flash-release: $(STM32_FLASH) $(FIRMWARE_RELEASE)
	@./$< -e $(FIRMWARE_RELEASE)

# cleaning targets
clean: clean-firmware clean-stm32-tools clean-log-analyzer
	rm -f libphysec/*.a libphysec/*.o || true && \
	rm -f tools/stm32-cli/src/physec_bindings/libphysec.rs tools/stm32-cli/src/physec_bindings/physec_serial.rs

clean-firmware:
	-$(MAKE) -C $(DEBUG_DIR) clean 2>/dev/null || true
	-$(MAKE) -C $(RELEASE_DIR) clean 2>/dev/null || true
	-rm -f $(FIRMWARE_DEBUG) $(FIRMWARE_RELEASE)

clean-stm32-tools: clean-stm32-cli clean-stm32-flash

clean-log-analyzer:
	-cd tools/log-analyzer && cargo clean 2>/dev/null || true
	-rm -f $(LOG_ANALYZER)

clean-debug:
	-$(MAKE) -C $(DEBUG_DIR) clean 2>/dev/null || true
	-rm -f $(FIRMWARE_DEBUG)

clean-release:
	-$(MAKE) -C $(RELEASE_DIR) clean 2>/dev/null || true
	-rm -f $(FIRMWARE_RELEASE)

clean-stm32-cli:
	-cd tools/stm32-cli && cargo clean 2>/dev/null || true
	-rm -f $(STM32_CLI)

clean-stm32-flash:
	-cd tools/stm32-flash && cargo clean 2>/dev/null || true
	-rm -f $(STM32_FLASH)

list-targets:
	@echo "Available targets:"
	@echo "  all              - Build everything (firmware + tools)"
	@echo "  all-firmware     - Build both debug and release firmware"
	@echo "  debug            - Build debug firmware"
	@echo "  release          - Build release firmware"
	@echo "  stm32-tools      - Build all STM32 tools (stm32-cli + stm32-flash)"
	@echo "  stm32-cli        - Build STM32 configuration tool"
	@echo "  stm32-flash      - Build STM32 flash tool"
	@echo "  log-analyzer     - Build log analysis tool"
	@echo "  flash            - Flash firmware (requires FLASH_FILE variable)"
	@echo "  flash-debug      - Flash debug firmware"
	@echo "  flash-release    - Flash release firmware"
	@echo "  clean            - Clean all build artifacts"
	@echo "  clean-firmware   - Clean only firmware builds"
	@echo "  clean-debug      - Clean only debug build"
	@echo "  clean-release    - Clean only release build"
	@echo "  clean-stm32-tools - Clean STM32 tool builds"
	@echo "  clean-stm32-cli - Clean STM32 tool builds"
	@echo "  clean-stm32-flash - Clean STM32 tool builds"
	@echo "  clean-log-analyzer - Clean log analyzer build"
	@echo "  list-targets     - Show this help message"
