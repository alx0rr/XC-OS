.PHONY: clean build qemu help push push-release fetch git-list rollback
SHELL := /bin/bash
.DEFAULT_GOAL := help
BRANCH ?= rft
MSG    ?=
TARGET ?=

# ─── Colours ────────────────────────────────────────────────────────────────
BLUE  := \e[1;34m
GREEN := \e[1;32m
RED   := \e[1;31m
RESET := \e[0m

step     = echo -e "$(BLUE)→$(RESET) $(1)"
ok       = echo -e "$(GREEN)✓$(RESET) $(1)"
red_step = echo -e "$(RED)→$(RESET) $(1)"
_error   = echo -e "$(RED)✗$(RESET) $(1)"

# ─── Paths ───────────────────────────────────────────────────────────────────
BUILD   := build
SRC     := src
BOOT    := $(SRC)/boot
KERNEL  := $(SRC)/kernel
KSHELL  := $(SRC)/kshell
DRIVERS := $(SRC)/drivers
INC     := $(SRC)/include
LIB     := $(SRC)/lib
CFG     := config.cfg
OS_NAME := xcos

# ─── Flags ───────────────────────────────────────────────────────────────────
CFLAGS := -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector \
          -O2 -march=i686 -mtune=generic \
          -fomit-frame-pointer -fno-exceptions \
          -I$(INC)

NASM_ELF := -f elf32
NASM_BIN := -f bin

# ─── Source → Object lists ───────────────────────────────────────────────────
LIB_OBJS := $(BUILD)/string.o $(BUILD)/time.o $(BUILD)/random.o

DRV_OBJS := $(BUILD)/vbe.o $(BUILD)/framebuffer.o $(BUILD)/text.o \
            $(BUILD)/pmm.o $(BUILD)/vmm.o $(BUILD)/tss.o \
            $(BUILD)/keyboard.o $(BUILD)/cpu.o $(BUILD)/ata.o \
            $(BUILD)/pit.o $(BUILD)/pcspk.o $(BUILD)/xcfs.o

KERN_OBJS := $(BUILD)/boot.o $(BUILD)/gdt_tss.o $(BUILD)/ring3.o $(BUILD)/isr.o $(BUILD)/idt.o \
             $(BUILD)/editor.o $(BUILD)/ring3c.o $(BUILD)/kernel.o

KSHELL_OBJS := $(BUILD)/kshell.o $(BUILD)/kshell_cmd.o

ALL_OBJS  := $(KERN_OBJS) $(KSHELL_OBJS) $(DRV_OBJS) $(LIB_OBJS)

# ─────────────────────────────────────────────────────────────────────────────

build: $(CFG)
	@$(call step,"Cleaning...")
	@rm -rf $(BUILD) && mkdir -p $(BUILD)

	@$(call step,"Loading config...")
	$(eval include $(CFG))

	@$(call step,"Generating config headers...")
	@printf '#ifndef CONFIG_H\n#define CONFIG_H\n'                         > $(INC)/config.h
	@printf '#define CFG_IMAGE_SIZE_MB       %s\n' $(IMAGE_SIZE_MB)       >> $(INC)/config.h
	@printf '#define CFG_KERNEL_START_SECTOR %s\n' $(KERNEL_START_SECTOR) >> $(INC)/config.h
	@printf '#define CFG_KERNEL_MAX_SECTORS  %s\n' $(KERNEL_MAX_SECTORS)  >> $(INC)/config.h
	@printf '#define CFG_XCFS_META_SECTOR    %s\n' $(XCFS_META_SECTOR)    >> $(INC)/config.h
	@printf '#define CFG_XCFS_META_SECTORS   %s\n' $(XCFS_META_SECTORS)   >> $(INC)/config.h
	@printf '#define CFG_XCFS_DATA_SECTOR    %s\n' $(XCFS_DATA_SECTOR)    >> $(INC)/config.h
	@printf '#endif\n'                                                     >> $(INC)/config.h
	@printf '%%define CFG_KERNEL_START_SECTOR %s\n' $(KERNEL_START_SECTOR) > $(BOOT)/config.inc
	@printf '%%define CFG_KERNEL_MAX_SECTORS  %s\n' $(KERNEL_MAX_SECTORS)  >> $(BOOT)/config.inc
	@printf '%%define CFG_XCFS_META_SECTOR    %s\n' $(XCFS_META_SECTOR)    >> $(BOOT)/config.inc
	@printf '%%define CFG_XCFS_DATA_SECTOR    %s\n' $(XCFS_DATA_SECTOR)    >> $(BOOT)/config.inc

	@$(call step,"Assembling bootloader...")
	@nasm $(NASM_BIN) $(BOOT)/stage1.asm -o $(BUILD)/stage1.bin \
	 || { echo -e "$(RED)✗$(RESET) Failed: stage1.asm" >&2; exit 1; }
	@nasm $(NASM_BIN) $(BOOT)/stage2.asm -o $(BUILD)/stage2.bin \
	 || { echo -e "$(RED)✗$(RESET) Failed: stage2.asm" >&2; exit 1; }

	@$(call step,"Assembling kernel entry...")
	@nasm $(NASM_ELF) $(KERNEL)/boot.asm -o $(BUILD)/boot.o \
	 || { echo -e "$(RED)✗$(RESET) Failed: boot.asm" >&2; exit 1; }

	@$(call step,"Assembling IDT/ISR...")
	@nasm $(NASM_ELF) $(KERNEL)/interrupts/isr.asm -o $(BUILD)/isr.o \
	 || { echo -e "$(RED)✗$(RESET) Failed: isr.asm" >&2; exit 1; }
	@nasm $(NASM_ELF) $(KERNEL)/gdt_tss.asm -o $(BUILD)/gdt_tss.o \
	 || { echo -e "$(RED)✗$(RESET) Failed: gdt_tss.asm" >&2; exit 1; }
	@nasm $(NASM_ELF) $(KERNEL)/ring3.asm -o $(BUILD)/ring3.o \
	 || { echo -e "$(RED)✗$(RESET) Failed: ring3.asm" >&2; exit 1; }
	@gcc $(CFLAGS) -c $(KERNEL)/ring3.c -o $(BUILD)/ring3c.o \
	 || { echo -e "$(RED)✗$(RESET) Failed: ring3.c" >&2; exit 1; }

	@$(call step,"Compiling libs...")
	@gcc $(CFLAGS) -c $(LIB)/string.c  -o $(BUILD)/string.o  || { echo -e "$(RED)✗$(RESET) Failed: string.c"  >&2; exit 1; }
	@gcc $(CFLAGS) -c $(LIB)/time.c    -o $(BUILD)/time.o    || { echo -e "$(RED)✗$(RESET) Failed: time.c"    >&2; exit 1; }
	@gcc $(CFLAGS) -c $(LIB)/random.c  -o $(BUILD)/random.o  || { echo -e "$(RED)✗$(RESET) Failed: random.c"  >&2; exit 1; }

	@$(call step,"Compiling drivers...")
	@gcc $(CFLAGS) -c $(DRIVERS)/graphics/vbe.c         -o $(BUILD)/vbe.o         || { echo -e "$(RED)✗$(RESET) Failed: vbe.c"         >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/graphics/framebuffer.c -o $(BUILD)/framebuffer.o || { echo -e "$(RED)✗$(RESET) Failed: framebuffer.c" >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/text/text.c            -o $(BUILD)/text.o        || { echo -e "$(RED)✗$(RESET) Failed: text.c"        >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/memory/pmm.c           -o $(BUILD)/pmm.o         || { echo -e "$(RED)✗$(RESET) Failed: pmm.c"         >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/memory/vmm.c           -o $(BUILD)/vmm.o         || { echo -e "$(RED)✗$(RESET) Failed: vmm.c"         >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/cpu/tss.c              -o $(BUILD)/tss.o         || { echo -e "$(RED)✗$(RESET) Failed: tss.c"         >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/input/keyboard.c       -o $(BUILD)/keyboard.o    || { echo -e "$(RED)✗$(RESET) Failed: keyboard.c"    >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/cpu/cpu.c              -o $(BUILD)/cpu.o         || { echo -e "$(RED)✗$(RESET) Failed: cpu.c"         >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/storage/ata.c          -o $(BUILD)/ata.o         || { echo -e "$(RED)✗$(RESET) Failed: ata.c"         >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/timer/pit.c            -o $(BUILD)/pit.o         || { echo -e "$(RED)✗$(RESET) Failed: pit.c"         >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/sound/pcspk.c          -o $(BUILD)/pcspk.o       || { echo -e "$(RED)✗$(RESET) Failed: pcspk.c"       >&2; exit 1; }
	@gcc $(CFLAGS) -c $(DRIVERS)/fs/xcfs.c              -o $(BUILD)/xcfs.o        || { echo -e "$(RED)✗$(RESET) Failed: xcfs.c"        >&2; exit 1; }

	@$(call step,"Compiling IDT...")
	@gcc $(CFLAGS) -c $(KERNEL)/interrupts/idt.c -o $(BUILD)/idt.o \
	 || { echo -e "$(RED)✗$(RESET) Failed: idt.c" >&2; exit 1; }

	@$(call step,"Compiling kernel...")
	@gcc $(CFLAGS) -c $(KERNEL)/editor.c  -o $(BUILD)/editor.o  || { echo -e "$(RED)✗$(RESET) Failed: editor.c"  >&2; exit 1; }
	@gcc $(CFLAGS) -c $(KERNEL)/kernel.c  -o $(BUILD)/kernel.o  || { echo -e "$(RED)✗$(RESET) Failed: kernel.c"  >&2; exit 1; }

	@$(call step,"Compiling kshell...")
	@gcc $(CFLAGS) -c $(KSHELL)/kshell_cmd.c -o $(BUILD)/kshell_cmd.o || { echo -e "$(RED)✗$(RESET) Failed: kshell_cmd.c" >&2; exit 1; }
	@gcc $(CFLAGS) -c $(KSHELL)/kshell.c     -o $(BUILD)/kshell.o     || { echo -e "$(RED)✗$(RESET) Failed: kshell.c"     >&2; exit 1; }

	@$(call step,"Linking kernel...")
	@ld -m elf_i386 -T $(SRC)/linker.ld -o $(BUILD)/kernel.bin \
	    --start-group \
	    $(BUILD)/boot.o       $(BUILD)/kernel.o      $(BUILD)/idt.o      $(BUILD)/isr.o  \
	    $(BUILD)/gdt_tss.o   $(BUILD)/tss.o         $(BUILD)/ring3.o    $(BUILD)/ring3c.o \
	    $(BUILD)/vbe.o        $(BUILD)/framebuffer.o $(BUILD)/text.o                     \
	    $(BUILD)/pmm.o        $(BUILD)/vmm.o                                             \
	    $(BUILD)/string.o     $(BUILD)/time.o        $(BUILD)/random.o                   \
	    $(BUILD)/keyboard.o   $(BUILD)/cpu.o         $(BUILD)/ata.o                      \
	    $(BUILD)/xcfs.o       $(BUILD)/pit.o         $(BUILD)/pcspk.o                    \
	    $(BUILD)/editor.o     $(BUILD)/kshell_cmd.o  $(BUILD)/kshell.o                   \
	    --end-group \
	 || { echo -e "$(RED)✗$(RESET) Linking failed" >&2; exit 1; }

	@KSIZE=$$(stat -c%s $(BUILD)/kernel.bin 2>/dev/null || stat -f%z $(BUILD)/kernel.bin); \
	 KMAX=$$(( $(KERNEL_MAX_SECTORS) * 512 )); \
	 if [ "$$KSIZE" -gt "$$KMAX" ]; then \
	     echo -e "$(RED)✗$(RESET) kernel.bin ($$KSIZE bytes) exceeds max ($$KMAX bytes = $(KERNEL_MAX_SECTORS) sectors)" >&2; \
	     exit 1; \
	 fi

	@$(call step,"Padding binaries...")
	@truncate -s $$(( $(KERNEL_MAX_SECTORS) * 512 )) $(BUILD)/kernel.bin
	@truncate -s $$(( $(BOOT_STAGE2_COUNT)  * 512 )) $(BUILD)/stage2.bin

	@$(call step,"Building disk image [$(IMAGE_SIZE_MB)MB]...")
	@dd if=/dev/zero of=$(BUILD)/$(OS_NAME).img bs=512 count=$$(( $(IMAGE_SIZE_MB) * 2048 )) 2>/dev/null

	@dd if=$(BUILD)/stage1.bin of=$(BUILD)/$(OS_NAME).img bs=512 count=$(BOOT_STAGE1_COUNT) conv=notrunc seek=$(BOOT_STAGE1_SECTOR) 2>/dev/null \
	 || { echo -e "$(RED)✗$(RESET) Write stage1" >&2; exit 1; }
	@dd if=$(BUILD)/stage2.bin of=$(BUILD)/$(OS_NAME).img bs=512 count=$(BOOT_STAGE2_COUNT) conv=notrunc seek=$(BOOT_STAGE2_SECTOR) 2>/dev/null \
	 || { echo -e "$(RED)✗$(RESET) Write stage2" >&2; exit 1; }
	@dd if=$(BUILD)/kernel.bin of=$(BUILD)/$(OS_NAME).img bs=512 count=$(KERNEL_MAX_SECTORS) conv=notrunc seek=$(KERNEL_START_SECTOR) 2>/dev/null \
	 || { echo -e "$(RED)✗$(RESET) Write kernel" >&2; exit 1; }

	@SI=$$(stat -c%s $(BUILD)/$(OS_NAME).img 2>/dev/null || stat -f%z $(BUILD)/$(OS_NAME).img); \
	 echo ""; \
	 echo "  Layout:"; \
	 printf "  %-18s [sector %d]\n"       "Stage 1:"          $(BOOT_STAGE1_SECTOR); \
	 printf "  %-18s [sectors %d-%d]\n"   "Stage 2:"          $(BOOT_STAGE2_SECTOR)  $$(( $(BOOT_STAGE2_SECTOR)  + $(BOOT_STAGE2_COUNT)  - 1 )); \
	 printf "  %-18s [sectors %d-%d]\n"   "Kernel (max 4MB):" $(KERNEL_START_SECTOR) $$(( $(KERNEL_START_SECTOR) + $(KERNEL_MAX_SECTORS) - 1 )); \
	 printf "  %-18s [sectors %d-%d]\n"   "XCFS metadata:"    $(XCFS_META_SECTOR)    $$(( $(XCFS_META_SECTOR)    + $(XCFS_META_SECTORS)  - 1 )); \
	 printf "  %-18s [sector %d+]\n"      "XCFS data:"        $(XCFS_DATA_SECTOR); \
	 echo ""; \
	 printf "  %-18s %d bytes (%d MB)\n"  "Image total:"      $$SI $(IMAGE_SIZE_MB); \
	 echo ""
	@$(call ok,"Done → $(BUILD)/$(OS_NAME).img")


# ─── Clean ───────────────────────────────────────────────────────────────────

clean:
	@$(call red_step,"Cleaning temporary files...")
	@rm -rf $(BUILD)
	@$(call ok,"Clean complete")


# ─── QEMU ────────────────────────────────────────────────────────────────────

qemu:
	@if [ ! -f $(BUILD)/$(OS_NAME).img ]; then \
		$(call _error,"Image not found: $(BUILD)/$(OS_NAME).img"); \
		exit 1; \
	fi
	@$(call step,"Launching QEMU...")
	@qemu-system-i386 \
		-drive format=raw,file=$(BUILD)/$(OS_NAME).img \
		-serial stdio \
		-display default
	@$(call ok,"Done")

push:
	@set -euo pipefail; \
	GIT() { git -C "$(CURDIR)" "$$@"; }; \
	IS_REL=0; \
	MSG_VAL="$(MSG)"; \
	if [ "$$MSG_VAL" = "release" ]; then \
		IS_REL=1; \
		MSG_VAL=""; \
	fi; \
	ST="$$(GIT status --porcelain)"; \
	if [ -z "$$ST" ] && [ "$$IS_REL" -eq 0 ]; then \
		echo "No changes to commit."; \
		exit 0; \
	fi; \
	if [ -z "$$MSG_VAL" ]; then \
		MSG_VAL="auto: $$(date -u +"%Y-%m-%d %H:%M:%SZ")"; \
	fi; \
	if [ -n "$$ST" ]; then \
		$(call step,"Staging changes..."); \
		GIT add -A; \
		$(call step,"Committing: $$MSG_VAL"); \
		GIT commit -m "$$MSG_VAL"; \
	fi; \
	$(call step,"Pushing to origin/$(BRANCH)..."); \
	GIT push origin $(BRANCH); \
	if [ "$$IS_REL" -eq 1 ]; then \
		$(call step,"Starting release process..."); \
		IMG="$(CURDIR)/$(BUILD)/$(OS_NAME).img"; \
		if [ ! -f "$$IMG" ]; then \
			$(call _error,"Image not found: $$IMG"); \
			exit 5; \
		fi; \
		VER="v$$(date -u +"%Y%m%d-%H%M%S")"; \
		$(call step,"Creating tag $$VER..."); \
		GIT tag -a "$$VER" -m "Release $$VER"; \
		GIT push origin "$$VER"; \
		$(call step,"Asking Copilot to generate release notes..."); \
		LOG="$$(GIT log -n 5 --oneline)"; \
		PROMPT="Generate a short, professional changelog for a GitHub Release in markdown based on these recent commits: $$LOG"; \
		NOTES=$$(gh copilot explain "$$PROMPT" 2>/dev/null || echo "Automated release. File: $(OS_NAME).img"); \
		$(call step,"Creating GitHub Release and uploading $(OS_NAME).img..."); \
		gh release create "$$VER" "$$IMG" --title "Release $$VER" --notes "$$NOTES"; \
		$(call ok,"Release $$VER created with Copilot notes!"); \
	else \
		$(call ok,"Push complete"); \
	fi

fetch:
	@$(call step,"Fetching all remotes...")
	@git -C "$(CURDIR)" fetch --all --prune
	@$(call step,"Pulling with rebase...")
	@git -C "$(CURDIR)" pull --rebase --autostash
	@$(call ok,"Fetch complete")

git-list:
	@$(call step,"Last 20 branches by date:")
	@git -C "$(CURDIR)" for-each-ref --sort=-committerdate \
		--format='%(refname:short)  %(committerdate:iso8601)' \
		refs/heads refs/remotes \
		| sed 's@refs/remotes/@@' \
		| awk '!seen[$$0]++' \
		| head -n 20

rollback:
	@if [ -z "$(TARGET)" ]; then \
		echo -e "$(RED)✗$(RESET) Usage: make rollback TARGET=<branch>" >&2; \
		exit 2; \
	fi
	@$(call step,"Fetching remotes...")
	@git -C "$(CURDIR)" fetch --all --prune
	@set -euo pipefail; \
	GIT() { git -C "$(CURDIR)" "$$@"; }; \
	if GIT show-ref --verify --quiet "refs/heads/$(TARGET)"; then \
		REF="refs/heads/$(TARGET)"; \
	elif GIT show-ref --verify --quiet "refs/remotes/origin/$(TARGET)"; then \
		REF="refs/remotes/origin/$(TARGET)"; \
	else \
		echo -e "$(RED)✗$(RESET) Branch '$(TARGET)' not found." >&2; \
		exit 3; \
	fi; \
	$(call step,"Rolling back main → $(TARGET)..."); \
	GIT checkout -B main "$$REF"; \
	GIT reset --hard "$$REF"
	@$(call ok,"Rollback to '$(TARGET)' complete")


# ─── Help ────────────────────────────────────────────────────────────────────

help:
	@printf "\n\033[1;34mAvailable targets:\033[0m\n"
	@printf "  \033[1;32mbuild\033[0m          - compile and assemble $(OS_NAME).img\n"
	@printf "  \033[1;32mclean\033[0m          - remove temporary files\n"
	@printf "  \033[1;32mqemu\033[0m           - run OS in QEMU\n"
	@printf "\n\033[1;34mGit targets:\033[0m\n"
	@printf "  \033[1;32mpush\033[0m           - stage all, commit, push  [MSG=\"your message\"]\n"
	@printf "  \033[1;32mpush MSG=release\033[0m - also tag and create a GitHub Release with $(OS_NAME).img\n"
	@printf "  \033[1;32mfetch\033[0m          - fetch --all + pull --rebase --autostash\n"
	@printf "  \033[1;32mgit-list\033[0m       - show last 20 branches sorted by date\n"
	@printf "  \033[1;32mrollback\033[0m       - force main to TARGET branch  [TARGET=<branch>]\n"
	@printf "\n"
