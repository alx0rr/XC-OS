CC     = gcc
AS     = nasm
LD     = ld

CFLAGS = -m32 -O2 -ffreestanding -fno-stack-protector -fno-pie \
         -nodefaultlibs -I. -I./lib
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -nostdlib -T link.ld

LIB_OBJ = lib/crt0.o lib/libc.o

all: $(LIB_OBJ)

lib/crt0.o: lib/crt0.asm
	$(AS) $(ASFLAGS) $< -o $@

lib/libc.o: lib/libc.c
	$(CC) $(CFLAGS) -c $< -o $@

examples/%.elf: examples/%.c $(LIB_OBJ)
	$(CC) $(CFLAGS) -c $< -o examples/$*.o
	$(LD) $(LDFLAGS) $(LIB_OBJ) examples/$*.o -o $@
	rm -f examples/$*.o

clean:
	rm -f lib/*.o examples/*.o examples/*.elf
