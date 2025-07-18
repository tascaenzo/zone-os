# ===== Kernel Makefile =====

KERNEL_ELF := ../../.build/kernel.elf
BUILD_DIR := ../../.build/kernel
LD_SCRIPT := ../../tools/linker.ld

# Oggetti architettura
ARCH_OBJ_DIR := ../../.build/arch/x86_64
ARCH_OBJECTS := $(shell find $(ARCH_OBJ_DIR)/ -name '*.o')

# Sorgenti kernel (C + ASM)
C_SOURCES   := $(shell find . -name '*.c')
ASM_SOURCES := $(shell find ../../boot/ -name '*.s' -o -name '*.S')
ASM_SOURCES := $(patsubst ../%, %,$(ASM_SOURCES))  # Normalizza path

# Oggetti kernel
C_OBJECTS   := $(addprefix $(BUILD_DIR)/, $(C_SOURCES:.c=.o))
ASM_OBJECTS := $(addprefix $(BUILD_DIR)/, $(ASM_SOURCES:.s=.o))
ASM_OBJECTS := $(ASM_OBJECTS:.S.o=.o)

KERNEL_OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

all: $(KERNEL_ELF)

# Link finale
$(KERNEL_ELF): $(KERNEL_OBJECTS) $(ARCH_OBJECTS) $(LD_SCRIPT)
	@echo "Creando kernel.elf in: $(abspath $@)"

	@mkdir -p $(dir $@)
	ld.lld -n -T $(LD_SCRIPT) -o $@ $(ARCH_OBJECTS) $(KERNEL_OBJECTS)

# Compilazione C
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	clang -target x86_64-pc-none-elf -ffreestanding -c $< -o $@

# Compilazione ASM - .s
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

# Compilazione ASM - .S
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(KERNEL_ELF)

.PHONY: all clean
