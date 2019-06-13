// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display backtrace", mon_backtrace },
	{ "showmappings", "Display physical page mappings", mon_showmappings},
	{ "perm", "Change/Set/Clear permission bits", mon_perm},
	{ "dump", "Display the contents of a range of physical memory", mon_dump}
//	{ "time", "Display time", mon_time }
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

//display the physical page mappings and corresponding permission bits
int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 3) {
		cprintf("showmappings [start PA] [end PA]\n");
		return 0;
	}

	physaddr_t start = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
	physaddr_t end = ROUNDUP(strtol(argv[2], NULL, 16), PGSIZE);
	cprintf("virt addr     phys addr        PTE_P   PTE_W   PTE_U\n");
	for (physaddr_t i = start; i < end; i+= PGSIZE) {
		pte_t *pte = pgdir_walk(kern_pgdir, KADDR(i), 0);
		if (!pte) 
			cprintf("0x%08x    0x%08x%8x%8x%8x\n", KADDR(i), i, 0, 0, 0);
		else
			cprintf("0x%08x    0x%08x%8x%8x%8x\n", KADDR(i), i, (*pte&PTE_P) > 0, (*pte&PTE_W) > 0, (*pte&PTE_U) > 0);
	}
	return 0;
}

// perm set/clr va UR/UW/KW
int
mon_perm(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 4) {
		cprintf("perm [set/clr] [pa] [UR/UW/KW]\n");
		return 0;
	}
	physaddr_t pa = ROUNDDOWN(strtol(argv[2], NULL, 16), PGSIZE);
	pte_t *pte = pgdir_walk(kern_pgdir, KADDR(pa), 0);
	if (!pte) {
		cprintf("virt addr	PTE_W   PTE_U\n");
		cprintf("0x%08x    %8x%8x%8x\n", KADDR(pa), 0, 0, 0);
		return 0;
	}
	char *op = argv[1];

	char *perm = argv[3];
	if (!strcmp(op, "set")){
		if (!strcmp(perm, "UR"))
			*pte |= PTE_U;
		else if (!strcmp(perm, "KW"))
			*pte |= PTE_W;
		else if (!strcmp(perm, "UW"))
			*pte = (*pte) | PTE_U | PTE_W;		
		else
			cprintf("invalid perm options\n");
	}
	else if (!strcmp(op, "clr")) {
		if (!strcmp(perm, "UR") || !strcmp(perm, "UW"))
			*pte &=  ~PTE_U;	
		else if (!strcmp(perm, "KW"))
			*pte &=  ~PTE_W;	
		// else if (!strcmp(perm, "UW"))
		// 	*pte = (*pte) & ~PTE_U & ~PTE_W;
		else
			cprintf("invalid perm options\n");
	}
	else cprintf("invalid operation, only set/clr is allowed\n");
	cprintf("virt addr	PTE_W   PTE_U\n");
	cprintf("0x%08x    %8x%8x\n", KADDR(pa), (*pte&PTE_W) > 0,(*pte&PTE_U) > 0);
	return 0;
}

// dump p/v addr size
int
mon_dump(int argc, char **argv, struct Trapframe *tf)
{
	if (argc < 4) {
		cprintf("dump pa/va [addr] [size]\n");
		return 0;
	}
	char *type = argv[1];
	char *addr = argv[2];
	uintptr_t va = 0;
	size_t size = strtol(argv[3], NULL, 10);
	if (!strcmp(type, "pa")) {
		va = (uintptr_t)KADDR(strtol(addr, NULL, 16));
	}
	else if (!strcmp(type, "va")) {
		va = strtol(addr, NULL, 16);
	}
	else {
		cprintf("invalid address type %s\n", addr);
		return 0;
	}
	cprintf("virt addr     content\n");
	for (size_t i = 0; i < size; i += 4) {
		pte_t *pte = pgdir_walk(kern_pgdir, (void*)(va+i), 0);
		if (!pte) { 
			cprintf("unused memory %08x\n", va+i);
			return 0;
		}
		uint32_t data = *(uint32_t *)(va+i);
		cprintf("0x%08x    %02x %02x %02x %02x\n", 
				va+i, data&0xFF,  (data>>8)&0xFF, (data>>16)&0xFF, (data>>24)&0xFF);
	}
	return 0;
}
// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}


void
do_overflow(void)
{
    cprintf("Overflow success\n");
}

void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;

    char str[256] = {};
    int nstr = 0;
    char *pret_addr;

	pret_addr = (char*)read_pretaddr();
	cprintf("%45d%n\n", nstr, pret_addr);//0x2d
	cprintf("%9d%n\n", nstr, pret_addr+1);//0x09
	
	cprintf("%104d%n\n",nstr, pret_addr+4);//0x68
	cprintf("%10d%n\n", nstr, pret_addr+5);//0x0a
	cprintf("%16d%n\n",nstr, pret_addr+6);//0x10
	return;	
}

void
overflow_me(void)
{
        start_overflow();
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint32_t ebp = read_ebp();
	while (ebp) {
		uint32_t eip = *(int *)(ebp+4);
		cprintf("  eip %08x  ebp %08x  args %08x %08x %08x %08x %08x\n", eip, ebp, *(int*)(ebp+8), *(int*)(ebp+12), *(int*)(ebp+16), *(int*)(ebp+20), *(int*)(ebp+24));
		struct Eipdebuginfo info;
                if (debuginfo_eip(eip, &info) >= 0) {
		cprintf("         %s:%d %.*s+%d\n",
				info.eip_file, info.eip_line, 
				info.eip_fn_namelen, info.eip_fn_name, eip - info.eip_fn_addr);
		ebp = *(int *)ebp;       // old ebp
		}
	}
	overflow_me();
	cprintf("Backtrace success\n");

	return 0;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

int
mon_time(int argc, char **argv, struct Trapframe *tf)
{
        uint64_t start, end;
        asm volatile("rdtsc" : "=A" (start));
		int i;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(1, argv, tf);
	}
        runcmd(argv[1], tf);
        asm volatile("rdtsc" : "=A" (end));
        cprintf("kerninfo cycles: %d\n", end - start);
        return 0;
}


void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

