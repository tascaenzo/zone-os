# ===== Kernel Makefile =====

KERNEL_ELF := ../../.build/kernel.elf
BUILD_DIR := ../../.build/kernel
LD_SCRIPT := ../../tools/linker.ld

# Oggetti architettura
ARCH_OBJ_DIR := ../../.build/arch/x86_64
ARCH_OBJECTS := $(shell find $(ARCH_OBJ_DIR) -name '*.o' 2>/dev/null)

# Sorgenti kernel
C_SOURCES := $(shell find . -name '*.c')
ASM_SOURCES := $(shell find . -name '*.s' -o -name '*.S')

# Oggetti kernel
C_OBJECTS := $(patsubst ./%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
ASM_S_OBJECTS := $(patsubst ./%.s, $(BUILD_DIR)/%.o, $(filter %.s, $(ASM_SOURCES)))
ASM_S_CAP_OBJECTS := $(patsubst ./%.S, $(BUILD_DIR)/%.o, $(filter %.S, $(ASM_SOURCES)))

KERNEL_OBJECTS := $(C_OBJECTS) $(ASM_S_OBJECTS) $(ASM_S_CAP_OBJECTS)

all: $(KERNEL_ELF)

# Link finale
$(KERNEL_ELF): $(KERNEL_OBJECTS) $(LD_SCRIPT)
	@mkdir -p $(dir $@)
	ld.lld -n -T $(LD_SCRIPT) -o $@ $(ARCH_OBJECTS) $(KERNEL_OBJECTS)

# Compilazione C
$(BUILD_DIR)/%.o: ./%.c
	@mkdir -p $(dir $@)
	clang -target x86_64-pc-none-elf -ffreestanding -nostdlib -nostdinc -mcmodel=kernel -DDEBUG -I./include -c $< -o $@

# Compilazione ASM - .s
$(BUILD_DIR)/%.o: ./%.s
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

# Compilazione ASM - .S
$(BUILD_DIR)/%.o: ./%.S
	@mkdir -p $(dir $@)
	clang -target x86_64-pc-none-elf -ffreestanding -nostdlib -nostdinc -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(KERNEL_ELF)

.PHONY: all clean