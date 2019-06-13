#### partA
lapic:non-I/O interrupts
mp: BSP AP
cpunum:current cpu ID
device memory: **PCD PWT**

#### partC
RR:
- apic_init clock int

### bug
- trap.c中FL_IF assert：SETGATE第二个参数istrap要置0，该位与刷新FL_IF位有关
- sysenter跳过trap，需要另外在文档要求外另外处理拿锁的逻辑，为此写了包装函数sysenter_with_lock，再调用syscall。调用前需要手动push Trapframe，tf传参。

- 修改eax

boot_ap - 
### answer
1. Compare kern/mpentry.S side by side with boot/boot.S. Bearing in mind that kern/mpentry.S is compiled and linked to run above KERNBASE just like everything else in the kernel, what is the purpose of macro MPBOOTPHYS? Why is it necessary in kern/mpentry.S but not in boot/boot.S? In other words, what could go wrong if it were omitted in kern/mpentry.S? 
Hint: recall the differences between the link address and the load address that we have discussed in Lab 1.

mpentry.S不需要启用A20，使用宏MPBOOTPHYS计算地址。因为mpentry.S的代码在AP(Application Processor)运行时，AP还未开paging，使用MPBOOTPHYS是为了计算绝对物理地址，否则会得到错误的地址。

2. It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.
尽管CPU进入内核会拿锁，但不知道中断何时会来，如果另1个CPU从用户态trap到内核态时硬件会push一些trapframe的参数(见trapentry.S)，然后拿锁，可能导致死锁，而且push和pop可能会修改掉原先栈上的信息。

3. In your implementation of env_run() you should have called lcr3(). Before and after the call to lcr3(), your code makes references (at least it should) to the variable e, the argument to env_run. Upon loading the %cr3 register, the addressing context used by the MMU is instantly changed. But a virtual address (namely e) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer e be dereferenced both before and after the addressing switch?
e在user和kernel的地址映射相同，env_setup_vm中将kern_pgdir相应偏移的内容直接memmove到每个e的env_pgdir相应偏移处，所以从kernel切换到user mode不会有问题。

4. Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?
切换回旧环境时需要恢复旧的上下文，才能用正确的寄存器值、状态位、权限位等正确地运行程序。
用户程序(如ipc通信)会通过sysenter调用sched_yield()，sysenter_handler在sysenter之前会pushal、pushfl来保存寄存器(见trapentry.S)。
时钟中断会通过interrupt gate触发trap dispatch调用sched_yield()，在call trap前会pushal来保存寄存器(见trapentry.S)。
env_pop_tf中popal和iret指令可恢复寄存器。