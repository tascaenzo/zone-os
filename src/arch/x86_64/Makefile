# ===== Arch x86_64 Makefile =====

BUILD_DIR := ../../../.build/arch/x86_64

# Sorgenti
C_SOURCES   := $(shell find . -name '*.c')
ASM_SOURCES := $(shell find . -name '*.s' -o -name '*.S')

# Oggetti
C_OBJECTS   := $(patsubst %, $(BUILD_DIR)/%, $(C_SOURCES:.c=.o))
ASM_OBJECTS := $(patsubst %, $(BUILD_DIR)/%, $(ASM_SOURCES:.s=.o))
ASM_OBJECTS := $(patsubst %, %, $(ASM_OBJECTS:.S.o=.o))

ARCH_OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

all: $(ARCH_OBJECTS)

# Compilazione file C
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	clang -target x86_64-pc-none-elf -ffreestanding -c $< -o $@

# Compilazione file ASM (.s)
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

# Compilazione file ASM (.S)
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
