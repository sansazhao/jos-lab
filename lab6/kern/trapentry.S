/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(divide_handler, T_DIVIDE)
TRAPHANDLER_NOEC(debug_handler, T_DEBUG)
TRAPHANDLER_NOEC(nmi_handler, T_NMI)
TRAPHANDLER_NOEC(brkpt_handler, T_BRKPT)
TRAPHANDLER_NOEC(oflow_handler, T_OFLOW)
TRAPHANDLER_NOEC(bound_handler, T_BOUND)
TRAPHANDLER_NOEC(illop_handler, T_ILLOP)
TRAPHANDLER_NOEC(device_handler, T_DEVICE)
TRAPHANDLER(dblflt_handler, T_DBLFLT)
TRAPHANDLER(tss_handler, T_TSS)
TRAPHANDLER(segnp_handler, T_SEGNP)
TRAPHANDLER(stack_handler, T_STACK)
TRAPHANDLER(gpflt_handler, T_GPFLT)
TRAPHANDLER(pgflt_handler, T_PGFLT)
TRAPHANDLER_NOEC(fperr_handler, T_FPERR)
TRAPHANDLER_NOEC(align_handler, T_ALIGN)
TRAPHANDLER_NOEC(mchk_handler, T_MCHK)
TRAPHANDLER_NOEC(simderr_handler, T_SIMDERR)
TRAPHANDLER_NOEC(syscall_handler, T_SYSCALL)

TRAPHANDLER_NOEC(irq_timer_handler, IRQ_OFFSET+IRQ_TIMER)
TRAPHANDLER_NOEC(irq_kbd_handler, IRQ_OFFSET+IRQ_KBD)
TRAPHANDLER_NOEC(irq_serial_handler, IRQ_OFFSET+IRQ_SERIAL)
TRAPHANDLER_NOEC(irq_spurious_handler, IRQ_OFFSET+IRQ_SPURIOUS)
TRAPHANDLER_NOEC(irq_ide_handler, IRQ_OFFSET+IRQ_IDE)
TRAPHANDLER_NOEC(irq_error_handler, IRQ_OFFSET+IRQ_ERROR)

TRAPHANDLER_NOEC(irq_2, IRQ_OFFSET+2)
TRAPHANDLER_NOEC(irq_3, IRQ_OFFSET+3)
TRAPHANDLER_NOEC(irq_5, IRQ_OFFSET+5)
TRAPHANDLER_NOEC(irq_6, IRQ_OFFSET+6)
TRAPHANDLER_NOEC(irq_8, IRQ_OFFSET+8)
TRAPHANDLER_NOEC(irq_9, IRQ_OFFSET+9)
TRAPHANDLER_NOEC(irq_10, IRQ_OFFSET+10)
TRAPHANDLER_NOEC(irq_11, IRQ_OFFSET+11)
TRAPHANDLER_NOEC(irq_12, IRQ_OFFSET+12)
TRAPHANDLER_NOEC(irq_13, IRQ_OFFSET+13)
TRAPHANDLER_NOEC(irq_15, IRQ_OFFSET+15)


/** Trapframe
struct PushRegs tf_regs;
	uint16_t tf_es;
	uint16_t tf_padding1;
	uint16_t tf_ds;
	uint16_t tf_padding2;
	uint32_t tf_trapno;
	// hardware 
	uint32_t tf_err;
	uintptr_t tf_eip;
	uint16_t tf_cs;
	uint16_t tf_padding3;
	uint32_t tf_eflags;
	// below here only when crossing rings, such as from user to kernel 
	uintptr_t tf_esp;
	uint16_t tf_ss;
	uint16_t tf_padding4;
**/


/* push the trapframe */
.globl sysenter_handler;
sysenter_handler:
	pushl $GD_UD | 3	#tf_ss, see in memout.h and env.c
	pushl %ebp			#tf_esp
	pushfl				#tf_eflags
	pushl $GD_UT | 3	#tf_cs
	pushl %esi			#tf_eip
	pushl $0			#tf_err
	pushl $T_SYSCALL	#tf_trapno
	pushl %ds
	pushl %es
	pushal				#tf_regs
	pushl %esp			#tf, trashed by sysenter
	pushl 0x4(%ebp)		#a5 return pc
	# pushl %esi			#return pc
	pushl %edi			#a4
	pushl %ebx			#a3
	pushl %ecx			#a2
	pushl %edx			#a1
	pushl %eax			#syscallno and return value 
	call syscall_with_lock		#the wrapper of syscall
	movl %ebp, %ecx		#esp -> arg2
	movl %esi, %edx		#pc -> arg1
	sti					#unmask hardware interrupts before sysexit
	sysexit				#eip -> edx, esp -> ecx, 

/*
 * Lab 3: Your code here for _alltraps
 */
.globl _alltraps;
_alltraps:
	#Trap frame
	pushl %ds
	pushl %es
	pushal
	#load GD_KD
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es
	pushl %esp
	call trap
