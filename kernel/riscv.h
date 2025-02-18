// 获取当前 hart（核心）的编号
static inline uint64
r_mhartid() {
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r" (x)); // 使用汇编指令读取 mhartid 寄存器
  return x;
}

// Machine Status Register (mstatus)
#define MSTATUS_MPP_MASK (3L << 11) // 用于屏蔽机器模式的上一个特权级
#define MSTATUS_MPP_M (3L << 11)    // 机器模式
#define MSTATUS_MPP_S (1L << 11)    // 监督模式
#define MSTATUS_MPP_U (0L << 11)    // 用户模式
#define MSTATUS_MIE (1L << 3)       // 机器模式中断使能

static inline uint64
r_mstatus() {
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r" (x)); // 读取 mstatus 寄存器
  return x;
}

static inline void
w_mstatus(uint64 x) {
  asm volatile("csrw mstatus, %0" : : "r" (x)); // 写入 mstatus 寄存器
}

// Machine Exception Program Counter (mepc)
static inline void
w_mepc(uint64 x) {
  asm volatile("csrw mepc, %0" : : "r" (x)); // 写入 mepc 寄存器
}

// Supervisor Status Register (sstatus)
#define SSTATUS_SPP (1L << 8)   // 上一个特权级（1表示监督模式，0表示用户模式）
#define SSTATUS_SPIE (1L << 5)  // 监督模式上一个中断使能
#define SSTATUS_UPIE (1L << 4)  // 用户模式上一个中断使能
#define SSTATUS_SIE (1L << 1)   // 监督模式中断使能
#define SSTATUS_UIE (1L << 0)   // 用户模式中断使能

static inline uint64
r_sstatus() {
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x)); // 读取 sstatus 寄存器
  return x;
}

static inline void
w_sstatus(uint64 x) {
  asm volatile("csrw sstatus, %0" : : "r" (x)); // 写入 sstatus 寄存器
}

// Supervisor Interrupt Pending (sip)
static inline uint64
r_sip() {
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x)); // 读取 sip 寄存器
  return x;
}

static inline void
w_sip(uint64 x) {
  asm volatile("csrw sip, %0" : : "r" (x)); // 写入 sip 寄存器
}

// Supervisor Interrupt Enable (sie)
#define SIE_SEIE (1L << 9) // 外部中断
#define SIE_STIE (1L << 5) // 定时器中断
#define SIE_SSIE (1L << 1) // 软件中断

static inline uint64
r_sie() {
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x)); // 读取 sie 寄存器
  return x;
}

static inline void
w_sie(uint64 x) {
  asm volatile("csrw sie, %0" : : "r" (x)); // 写入 sie 寄存器
}

// Machine-mode Interrupt Enable (mie)
#define MIE_MEIE (1L << 11) // 外部中断
#define MIE_MTIE (1L << 7)  // 定时器中断
#define MIE_MSIE (1L << 3)  // 软件中断

static inline uint64
r_mie() {
  uint64 x;
  asm volatile("csrr %0, mie" : "=r" (x)); // 读取 mie 寄存器
  return x;
}

static inline void
w_mie(uint64 x) {
  asm volatile("csrw mie, %0" : : "r" (x)); // 写入 mie 寄存器
}

// Supervisor Exception Program Counter (sepc)
static inline void
w_sepc(uint64 x) {
  asm volatile("csrw sepc, %0" : : "r" (x)); // 写入 sepc 寄存器
}

static inline uint64
r_sepc() {
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x)); // 读取 sepc 寄存器
  return x;
}

// Machine Exception Delegation (medeleg)
static inline uint64
r_medeleg() {
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r" (x)); // 读取 medeleg 寄存器
  return x;
}

static inline void
w_medeleg(uint64 x) {
  asm volatile("csrw medeleg, %0" : : "r" (x)); // 写入 medeleg 寄存器
}

// Machine Interrupt Delegation (mideleg)
static inline uint64
r_mideleg() {
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r" (x)); // 读取 mideleg 寄存器
  return x;
}

static inline void
w_mideleg(uint64 x) {
  asm volatile("csrw mideleg, %0" : : "r" (x)); // 写入 mideleg 寄存器
}

// Supervisor Trap-Vector Base Address (stvec)
static inline void
w_stvec(uint64 x) {
  asm volatile("csrw stvec, %0" : : "r" (x)); // 写入 stvec 寄存器
}

static inline uint64
r_stvec() {
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x)); // 读取 stvec 寄存器
  return x;
}

// Machine-mode interrupt vector (mtvec)
static inline void
w_mtvec(uint64 x) {
  asm volatile("csrw mtvec, %0" : : "r" (x)); // 写入 mtvec 寄存器
}

// 使用 RISC-V 的 Sv39 页面表方案
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// Supervisor Address Translation and Protection (satp)
// 用于虚拟地址到物理地址的转换，包含页面表的基地址
static inline void
w_satp(uint64 x) {
  asm volatile("csrw satp, %0" : : "r" (x)); // 写入 satp 寄存器
}

static inline uint64
r_satp() {
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x)); // 读取 satp 寄存器
  return x;
}

// Supervisor Scratch register (sscratch)
static inline void
w_sscratch(uint64 x) {
  asm volatile("csrw sscratch, %0" : : "r" (x)); // 写入 sscratch 寄存器
}

// Machine Scratch register (mscratch)
static inline void
w_mscratch(uint64 x) {
  asm volatile("csrw mscratch, %0" : : "r" (x)); // 写入 mscratch 寄存器
}

// Supervisor Trap Cause (scause)
static inline uint64
r_scause() {
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x)); // 读取 scause 寄存器
  return x;
}

// Supervisor Trap Value (stval)
static inline uint64
r_stval() {
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x)); // 读取 stval 寄存器
  return x;
}

// Machine-mode Counter-Enable (mcounteren)
static inline void
w_mcounteren(uint64 x) {
  asm volatile("csrw mcounteren, %0" : : "r" (x)); // 写入 mcounteren 寄存器
}

static inline uint64
r_mcounteren() {
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r" (x)); // 读取 mcounteren 寄存器
  return x;
}

// machine-mode cycle counter (时间寄存器)
static inline uint64
r_time() {
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x)); // 读取 time 寄存器
  return x;
}

// 启用设备中断（设置 SSTATUS_SIE 位）
static inline void
intr_on() {
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// 禁用设备中断（清除 SSTATUS_SIE 位）
static inline void
intr_off() {
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// 判断设备中断是否启用
static inline int
intr_get() {
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

// 获取堆栈指针（sp）
static inline uint64
r_sp() {
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x)); // 将 sp 的值移动到变量 x
  return x;
}

// 获取和设置线程指针（tp），保存当前 hart 的编号
static inline uint64
r_tp() {
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x)); // 将 tp 的值移动到变量 x
  return x;
}

static inline void
w_tp(uint64 x) {
  asm volatile("mv tp, %0" : : "r" (x)); // 将变量 x 的值移动到 tp
}

// 获取返回地址（ra）
static inline uint64
r_ra() {
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x)); // 将 ra 的值移动到变量 x
  return x;
}

// 刷新 TLB（Translation Lookaside Buffer）
static inline void
sfence_vma() {
  asm volatile("sfence.vma zero, zero"); // 刷新所有 TLB 入口
}

// 页面管理相关定义
#define PGSIZE 4096 // 页面大小，单位为字节
#define PGSHIFT 12  // 页面偏移位数

#define PGROUNDUP(sz) (((sz)+PGSIZE-1) & ~(PGSIZE-1)) // 向上对齐到页面大小
#define PGROUNDDOWN(a) ((a) & ~(PGSIZE-1))            // 向下对齐到页面大小

#define PTE_V (1L << 0) // 页面表项有效位
#define PTE_R (1L << 1) // 读权限
#define PTE_W (1L << 2) // 写权限
#define PTE_X (1L << 3) // 执行权限
#define PTE_U (1L << 4) // 用户权限（用户模式可以访问）

// 将物理地址转换为页面表项格式
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

// 将页面表项转换为物理地址
#define PTE2PA(pte) ((pte) << 12 >> 10 << 12)

// 提取页面表项的标志位
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 从虚拟地址中提取三级页表索引
#define PXMASK          0x1FF  // 9 位掩码
#define PXSHIFT(level) (PGSHIFT + (9 * (level))) // 计算每次右移的位数
#define PX(level, va) (((uint64)(va) >> PXSHIFT(level)) & PXMASK) // 提取索引

#define MAXVA (1L << (9 + 9 + 9 + 12 - 1)) // 最大虚拟地址

typedef uint64 pte_t;       // 页面表项类型
typedef uint64 *pagetable_t; // 页面表指针类型
