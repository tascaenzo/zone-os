# ===== Kernel Makefile =====

# === Output ELF e build directory ===
KERNEL_ELF := ../../.build/kernel.elf
BUILD_DIR := ../../.build/kernel
LD_SCRIPT := ../../tools/linker.ld

# === Oggetti architettura separati (precompilati esternamente) ===
ARCH_OBJ_DIR := ../../.build/arch/x86_64
ARCH_OBJECTS := $(shell find $(ARCH_OBJ_DIR) -name '*.o' 2>/dev/null)

# === Sorgenti C e ASM nel progetto ===
C_SOURCES := $(shell find . -name '*.c')
ASM_SOURCES := $(shell find . -name '*.s' -o -name '*.S')

# === Oggetti compilati ===
C_OBJECTS := $(patsubst ./%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
ASM_S_OBJECTS := $(patsubst ./%.s, $(BUILD_DIR)/%.o, $(filter %.s, $(ASM_SOURCES)))
ASM_S_CAP_OBJECTS := $(patsubst ./%.S, $(BUILD_DIR)/%.o, $(filter %.S, $(ASM_SOURCES)))
KERNEL_OBJECTS := $(C_OBJECTS) $(ASM_S_OBJECTS) $(ASM_S_CAP_OBJECTS)

# === Include dinamico per header ===
HEADER_DIRS := $(sort $(dir $(shell find . -name '*.h')))
INCLUDE_FLAGS := -I. $(foreach dir,$(HEADER_DIRS),-I$(dir))

# === Flag compilatore ===
CFLAGS := -target x86_64-pc-none-elf \
          -ffreestanding -nostdlib -nostdinc \
          -mcmodel=kernel -DDEBUG \
          $(INCLUDE_FLAGS)

ifdef VMM_BOOT_DEBUG
CFLAGS += -DVMM_BOOT_DEBUG
endif

# === Regole principali ===
all: $(KERNEL_ELF)

# === Link finale kernel ELF ===
$(KERNEL_ELF): $(KERNEL_OBJECTS) $(LD_SCRIPT)
	@mkdir -p $(dir $@)
	ld.lld -n -T $(LD_SCRIPT) -o $@ $(ARCH_OBJECTS) $(KERNEL_OBJECTS)

# === Compilazione sorgenti C ===
$(BUILD_DIR)/%.o: ./%.c
	@mkdir -p $(dir $@)
	clang $(CFLAGS) -c $< -o $@

# === Compilazione sorgenti ASM (.s - NASM syntax) ===
$(BUILD_DIR)/%.o: ./%.s
	@mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@

# === Compilazione sorgenti ASM (.S - GAS syntax) ===
$(BUILD_DIR)/%.o: ./%.S
	@mkdir -p $(dir $@)
	clang -target x86_64-pc-none-elf -ffreestanding -nostdlib -nostdinc -c $< -o $@

# === Clean build ===
clean:
	rm -rf $(BUILD_DIR) $(KERNEL_ELF)

.PHONY: all clean
