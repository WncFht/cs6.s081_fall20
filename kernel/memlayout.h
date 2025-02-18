// 物理内存布局

// QEMU 的 virt 机器类型设置如下，
// 基于 QEMU 的 hw/riscv/virt.c：
//
// 00001000 -- 启动 ROM，由 QEMU 提供
// 02000000 -- CLINT（Core Local Interruptor，核心本地中断控制器）
// 0C000000 -- PLIC（Platform Level Interrupt Controller，平台级中断控制器）
// 10000000 -- UART0（串口设备）
// 10001000 -- virtio 磁盘设备
// 80000000 -- 启动 ROM 跳转到这里的机器模式
//             - 内核加载到这里
// 80000000 之后的内存未使用。

// 内核使用物理内存的方式如下：
// 80000000 -- entry.S，然后是内核的代码和数据
// end -- 内核页分配区域的起始地址
// PHYSTOP -- 内核使用的内存结束地址

// QEMU 将 UART 寄存器放在物理内存的这个地址。
#define UART0 0x10000000L // UART0 的物理地址
#define UART0_IRQ 10      // UART0 的中断号

// virtio 内存映射 I/O 接口
#define VIRTIO0 0x10001000 // virtio 磁盘设备的物理地址
#define VIRTIO0_IRQ 1      // virtio 磁盘设备的中断号

// 本地中断控制器，包含定时器。
#define CLINT 0x2000000L   // CLINT 的物理地址
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid)) // 每个核心的定时器比较寄存器
#define CLINT_MTIME (CLINT + 0xBFF8) // 自启动以来的周期数（全局定时器）

// QEMU 将可编程中断控制器放在这里。
#define PLIC 0x0c000000L   // PLIC 的物理地址
#define PLIC_PRIORITY (PLIC + 0x0)  // 中断优先级寄存器
#define PLIC_PENDING (PLIC + 0x1000) // 待处理中断寄存器
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100) // 机器模式中断使能寄存器
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100) // 用户模式中断使能寄存器
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000) // 机器模式中断优先级寄存器
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000) // 用户模式中断优先级寄存器
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000) // 机器模式中断声明寄存器
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000) // 用户模式中断声明寄存器

// 内核期望从物理地址 0x80000000 到 PHYSTOP 之间有 RAM，
// 供内核和用户页面使用。
#define KERNBASE 0x80000000L // 内核的基地址
#define PHYSTOP (KERNBASE + 128*1024*1024) // 内核使用的物理内存结束地址（128MB）

// 将 trampoline 页面映射到最高地址，
// 同时在用户空间和内核空间中可见。
#define TRAMPOLINE (MAXVA - PGSIZE) // trampoline 页面的地址

// 内核栈位于 trampoline 之下，
// 每个栈周围都有无效的保护页面。
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE) // 计算内核栈的地址

// 用户内存布局。
// 从地址零开始：
//   text（代码段）
//   original data and bss（原始数据段和 BSS 段）
//   fixed-size stack（固定大小的栈）
//   expandable heap（可扩展的堆）
//   ...
//   TRAPFRAME（p->trapframe，由 trampoline 使用）
//   TRAMPOLINE（与内核中的 trampoline 页面相同）
#define TRAPFRAME (TRAMPOLINE - PGSIZE) // trapframe 的地址
