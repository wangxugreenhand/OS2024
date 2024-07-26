#include "x86.h"
#include "device.h"

extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern int displayRow;
extern int displayCol;

void GProtectFaultHandle(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);
void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallPrint(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);

void irqHandle(struct StackFrame *sf)
{ // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds" ::"a"(KSEL(SEG_KDATA)));
	/*XXX Save esp to stackTop */
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;

	switch (sf->irq)
	{
	case -1:
		break;
	case 0xd:
		GProtectFaultHandle(sf);
		break;
	case 0x20:
		timerHandle(sf);
		break;
	case 0x80:
		syscallHandle(sf);
		break;
	default:
		assert(0);
	}
	/*XXX Recover stackTop */
	pcb[current].stackTop = tmpStackTop;
}

void GProtectFaultHandle(struct StackFrame *sf)
{
	assert(0);
	return;
}

void timerHandle(struct StackFrame *sf)
{
	// TODO
	for(int i =0; i < MAX_PCB_NUM; i++){
		if(pcb[i].state == STATE_BLOCKED){
			pcb[i].sleepTime--;
			if(pcb[i].sleepTime == 0){
				pcb[i].state = STATE_RUNNABLE;
			}
		}
	}	
	pcb[current].timeCount++;
	int temp = current;
	if(pcb[current].timeCount < MAX_TIME_COUNT)
		return;
	if (pcb[current].timeCount >= MAX_TIME_COUNT)
	{
		pcb[current].state = STATE_RUNNABLE;
		pcb[current].timeCount = 0;
		int i;
		for (i = (current + 1) % MAX_PCB_NUM; i != current; i = (i + 1) % MAX_PCB_NUM)
			if (pcb[i].state == STATE_RUNNABLE )
				break;
		current = i;
		pcb[current].state = STATE_RUNNING;
	}
	if (temp != current)
	{
		uint32_t tmpStackTop = pcb[current].stackTop;
		pcb[current].stackTop = pcb[current].prevStackTop;
		tss.esp0 = (uint32_t)&(pcb[current].stackTop);
		asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
	
	return;
}

void syscallHandle(struct StackFrame *sf)
{
	switch (sf->eax)
	{ // syscall number
	case 0:
		syscallWrite(sf);
		break; // for SYS_WRITE
	/*TODO Add Fork,Sleep... */
	case 1:
		syscallFork(sf);
		break;
	case 3:
		syscallSleep(sf);
		break;
	case 4:
		syscallExit(sf);
		break;
	default:
		break;
	}
}

void syscallWrite(struct StackFrame *sf)
{
	switch (sf->ecx)
	{ // file descriptor
	case 0:
		syscallPrint(sf);
		break; // for STD_OUT
	default:
		break;
	}
}

void syscallPrint(struct StackFrame *sf)
{
	int sel = sf->ds; // segment selector for user data, need further modification
	char *str = (char *)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	for (i = 0; i < size; i++)
	{
		asm volatile("movb %%es:(%1), %0" : "=r"(character) : "r"(str + i));
		if (character == '\n')
		{
			displayRow++;
			displayCol = 0;
			if (displayRow == 25)
			{
				displayRow = 24;
				displayCol = 0;
				scrollScreen();
			}
		}
		else
		{
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000));
			displayCol++;
			if (displayCol == 80)
			{
				displayRow++;
				displayCol = 0;
				if (displayRow == 25)
				{
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
		}
		// asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
		// asm volatile("int $0x20":::"memory"); //XXX Testing irqTimer during syscall
	}

	updateCursor(displayRow, displayCol);
	// take care of return value
	return;
}

// TODO syscallFork ...
void syscallFork(struct StackFrame *sf)
{
	//遍历寻找一个空的PCB
	int new = -1;
	for (int i = 0; i < MAX_PCB_NUM; i++) {
		if(pcb[i].state == STATE_DEAD){
			new = i;
			break;
		}
	}
	//如果没有空的PCB
	if (new == -1) {
		pcb[current].regs.eax = -1;
		return;
	}
	else{
		enableInterrupt(); // 开中断
		int i = 0;
		// 复制内存
		for (i = 0; i < 0x100000; i++) {
			*(uint8_t *)((new + 1) * 0x100000 + i) = *(uint8_t *)(i + 0x100000 * (current + 1));
		}
		disableInterrupt(); // 关中断

		// 复制PCB
		for ( i = 0; i < sizeof(ProcessTable); i++)
		{
			*((uint8_t *)(&pcb[new]) + i) = *((uint8_t *)(&pcb[current]) + i);
		}	

		//	改变pcb的信息
		pcb[new].stackTop = (uint32_t)&(pcb[new].regs);
		pcb[new].prevStackTop = (uint32_t)&(pcb[new].stackTop);
		pcb[new].state = STATE_RUNNABLE;
		pcb[new].timeCount = 0;
		pcb[new].sleepTime = 0;
		pcb[new].pid = new;

		//	改变regs的信息
		pcb[new].regs.ss = USEL(2 * new + 2);
		pcb[new].regs.ds = USEL(2 * new + 2);
		pcb[new].regs.es = USEL(2 * new + 2);
		pcb[new].regs.fs = USEL(2 * new + 2);
		pcb[new].regs.gs = USEL(2 * new + 2);
		pcb[new].regs.cs = USEL(2 * new + 1);

		// return value
		pcb[current].regs.eax = new;
		pcb[new].regs.eax = 0;
		return;

	}

}

void syscallSleep(struct StackFrame *sf)
{
	// TODO
	pcb[current].state = STATE_BLOCKED;
	pcb[current].sleepTime = sf->ecx;
	int i;
	for (i = (current + 1) % MAX_PCB_NUM; i != current; i = (i + 1) %MAX_PCB_NUM)
			if (pcb[i].state == STATE_RUNNABLE )
				break;
	current = i;
	pcb[current].state = STATE_RUNNING;
	//asm volatile("int $0x20");
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].stackTop = pcb[current].prevStackTop;
	tss.esp0 = (uint32_t)&(pcb[current].stackTop);
	asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
	asm volatile("popl %gs");
	asm volatile("popl %fs");
	asm volatile("popl %es");
	asm volatile("popl %ds");
	asm volatile("popal");
	asm volatile("addl $8, %esp");
	asm volatile("iret");
	return;
}

void syscallExit(struct StackFrame *sf)
{
	// TODO
	pcb[current].state = STATE_DEAD;
	int i;
	for (i = (current + 1) % MAX_PCB_NUM; i != current; i = (i + 1) %MAX_PCB_NUM)
			if (pcb[i].state == STATE_RUNNABLE)
				break;
	current = i;
	pcb[current].state = STATE_RUNNING;
	//asm volatile("int $0x20");
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].stackTop = pcb[current].prevStackTop;
	tss.esp0 = (uint32_t)&(pcb[current].stackTop);
	asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
	asm volatile("popl %gs");
	asm volatile("popl %fs");
	asm volatile("popl %es");
	asm volatile("popl %ds");
	asm volatile("popal");
	asm volatile("addl $8, %esp");
	asm volatile("iret");
}