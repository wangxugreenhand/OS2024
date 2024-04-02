#include "boot.h"

#define SECTSIZE 512

void bootMain(void) {
	//TODO
//	void(*my_load)(void);
//	my_load =(void(*)(void))0x8c00;
	void* my_load = (void*)0x8c00;
	readSect((void*)my_load,1);
	asm volatile("jmp %0"::"r"(my_load));
//	my_load();  
}


void waitDisk(void) { // waiting for disk
	while((inByte(0x1F7) & 0xC0) != 0x40);
}

void readSect(void *dst, int offset) { // reading a sector of disk
	int i;
	waitDisk();
	outByte(0x1F2, 1);
	outByte(0x1F3, offset);
	outByte(0x1F4, offset >> 8);
	outByte(0x1F5, offset >> 16);
	outByte(0x1F6, (offset >> 24) | 0xE0);
	outByte(0x1F7, 0x20);

	waitDisk();
	for (i = 0; i < SECTSIZE / 4; i ++) {
		((int *)dst)[i] = inLong(0x1F0);
	}
}
