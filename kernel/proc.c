#include "types.h"          // 基本类型定义
#include "param.h"          // 系统参数
#include "memlayout.h"      // 内存布局
#include "riscv.h"          // RISC-V 架构相关定义
#include "spinlock.h"       // 自旋锁相关定义
#include "proc.h"           // 进程管理相关定义
#include "defs.h"           // 公用宏和工具函数

// 多核 CPU 的相关信息
struct cpu cpus[NCPU];

// 进程表，存储所有进程的信息
struct proc proc[NPROC];

// 初始化第一个用户进程
struct proc *initproc;

// 下一个可用的进程标识符
int nextpid = 1;
// 保护 nextpid 的自旋锁
struct spinlock pid_lock;

// 定义在 trampoline.S 中，用于处理系统调用返回
extern void forkret(void);

// 仅在内核中使用的唤醒函数
static void wakeup1(struct proc *chan);
static void freeproc(struct proc *p);

// 定义在 trampoline.S 中，用于处理系统调用返回
extern char trampoline[]; 

// 在系统启动时初始化进程表
void
procinit(void)
{
  struct proc *p;

  // 初始化保护 nextpid 的自旋锁
  initlock(&pid_lock, "nextpid");
  
  // 遍历进程表，初始化每个进程
  for(p = proc; p < &proc[NPROC]; p++) {
    // 初始化进程的自旋锁
    initlock(&p->lock, "proc");

    // 为进程分配一个内核栈
    // 高于所有用户内存，后跟一个无效的防护页
    char *pa = kalloc(); // 申请物理内存页
    if (pa == 0)
      panic("kalloc"); // 分配失败时触发系统崩溃

    // 计算虚拟地址
    uint64 va = KSTACK((int)(p - proc));
    // 将物理地址映射到虚拟地址，权限为读写
    kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    p->kstack = va; // 保存内核栈的虚拟地址
  }

  // 初始化硬件线程
  kvminithart();
}

// 必须在关闭中断的情况下调用，以防止竞态条件
// 保证返回的 CPU ID 是准确的
int
cpuid()
{
  // 使用 RISC-V 的 tp 寄存器获取当前 CPU ID
  int id = r_tp();
  return id;
}

// 返回当前 CPU 的相关信息
// 中断必须关闭
struct cpu*
mycpu(void) {
  // 获取当前 CPU ID
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// 返回当前正在运行的进程结构
// 或在没有进程时返回 0
struct proc*
myproc(void) {
  push_off(); // 禁用中断
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off(); // 恢复中断状态
  return p;
}

// 分配一个新的进程标识符
int
allocpid() {
  int pid;

  // 获取 nextpid 锁
  acquire(&pid_lock);
  pid = nextpid; // 当前的 nextpid
  nextpid = nextpid + 1; // 更新 nextpid
  release(&pid_lock); // 释放锁

  return pid;
}

// 在进程表中查找一个未使用的进程
// 返回时会持有该进程的锁
// 如果没有未使用的进程或者内存分配失败，返回 0
static struct proc*
allocproc(void)
{
  struct proc *p;

  // 遍历进程表
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock); // 获取进程锁
    if (p->state == UNUSED) {
      goto found; // 找到未使用的进程
    } else {
      release(&p->lock); // 释放锁
    }
  }
  return 0; // 未找到

found:
  // 为进程分配新的 PID
  p->pid = allocpid();

  // 分配一个用于保存陷入内核时用户状态的缓冲区
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0) {
    release(&p->lock); // 分配失败，释放锁并返回
    return 0;
  }

  // 创建一个空的用户页表
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0) {
    // 用户页表创建失败，释放资源
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 初始化进程的上下文，跳转到 forkret 函数
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret; // 返回地址
  p->context.sp = p->kstack + PGSIZE; // 栈指针

  return p; // 返回分配的进程
}

// 释放进程结构和与之关联的数据
// 包括用户页面
// 调用时必须持有进程锁
static void
freeproc(struct proc *p)
{
  // 释放 trapframe 页
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;

  // 释放用户页表
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;

  // 清理进程状态
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// 为进程创建一个用户页表
// 包含一个空的用户内存区域，但有 trampoline 页面
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // 创建一个空的页表
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // 在最高用户虚拟地址处映射 trampoline 代码
  // 仅用于系统调用返回
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0) {
    uvmfree(pagetable, 0);
    return 0;
  }

  // 在 TRAMPOLINE 之下映射 trapframe
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// 释放进程的页表以及引用的物理内存
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  // 解映射 TRAMPOLINE 和 TRAPFRAME 页面
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz); // 释放剩余的页表
}

// initcode 是一个用户程序，它会通过 exec 调用 /init
// od -t xC initcode 显示其十六进制代码
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// 设置第一个用户进程
void
userinit(void)
{
  struct proc *p;

  p = allocproc(); // 分配一个进程
  initproc = p;

  // 分配一个用户页，并将 initcode 复制到该页
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE; // 设置用户内存大小

  // 准备第一次从内核返回用户空间
  p->trapframe->epc = 0;      // 用户程序计数器
  p->trapframe->sp = PGSIZE;  // 用户栈指针

  // 设置进程名和当前工作目录
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/"); // 获取根目录

  p->state = RUNNABLE; // 设置进程状态为可运行

  release(&p->lock); // 释放进程锁
}

// 增长或缩小用户内存 n 字节
// 成功返回 0，失败返回 -1
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0) {
    // 增长用户内存
    if ((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1; // 分配失败
    }
  } else if (n < 0) {
    // 缩小用户内存
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// 创建一个新进程，复制父进程
// 设置子进程的内核栈，使其像从 fork() 返回一样
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // 分配一个新进程
  if ((np = allocproc()) == 0) {
    return -1;
  }

  // 复制父进程的用户内存到子进程
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;
  // 设置子进程的父进程为当前进程
  np->parent = p;
  // 复制父进程的寄存器状态到子进程中
  *(np->trapframe) = *(p->trapframe);
  // 使子进程在 fork 调用时返回 0
  np->trapframe->a0 = 0;
  // 增加打开文件描述符的引用计数
  for (i = 0; i < NOFILE; i++) {
    if (p->ofile[i]) {
      np->ofile[i] = filedup(p->ofile[i]);
    }
  }
  np->cwd = idup(p->cwd); // 增加当前工作目录的引用计数
  // 复制进程名
  safestrcpy(np->name, p->name, sizeof(p->name));
  pid = np->pid; // 获取子进程的 PID
  np->state = RUNNABLE; // 设置子进程状态为可运行
  release(&np->lock); // 释放子进程锁
  return pid; // 返回子进程的 PID
}

// 将 p 的子进程重新分配给 init 进程。
// 调用者必须持有 p->lock。
void
reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++) {
    // 这里没有获取 pp->lock，因为获取锁可能导致死锁。
    // 如果 pp 或其子进程也处于 exit() 中，并且尝试获取 p 的锁，就会发生死锁。
    if (pp->parent == p) {
      // 父进程可以安全地改变其子进程的 pp->parent，无需额外锁定。
      acquire(&pp->lock); // 获取子进程锁
      pp->parent = initproc; // 将子进程的父进程设置为 init
      release(&pp->lock); // 释放锁
    }
  }
}

// 退出当前进程。不会返回。
// 退出的进程将保持僵尸状态，直到其父进程调用 wait()。
void
exit(int status)
{
  struct proc *p = myproc();

  if (p == initproc) // 如果是 init 进程退出，系统崩溃
    panic("init exiting");

  // 关闭所有打开的文件
  for (int fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd]) {
      struct file *f = p->ofile[fd];
      fileclose(f); // 关闭文件
      p->ofile[fd] = 0; // 清空文件描述符
    }
  }

  begin_op(); // 文件系统操作开始
  iput(p->cwd); // 释放当前工作目录
  end_op(); // 文件系统操作结束
  p->cwd = 0; // 清空当前工作目录

  // 唤醒 init 进程，通知其接收重新分配的子进程
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);

  // 获取当前进程的父进程指针，避免竞态条件
  acquire(&p->lock);
  struct proc *original_parent = p->parent;
  release(&p->lock);

  // 获取父进程锁，唤醒父进程（如果它在等待）
  acquire(&original_parent->lock);
  acquire(&p->lock);

  // 重新分配当前进程的子进程给 init
  reparent(p);

  // 唤醒父进程，通知其子进程已退出
  wakeup1(original_parent);

  // 设置退出状态和进程状态
  p->xstate = status;
  p->state = ZOMBIE;

  release(&original_parent->lock); // 释放父进程锁

  // 将控制权交给调度器，永远不会返回
  sched();
  panic("zombie exit"); // 正常情况下不应该到达这里
}

// 等待子进程退出并返回其 PID。
// 如果当前进程没有子进程，返回 -1。
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&p->lock); // 加锁，避免错过子进程的退出通知

  for (;;) { // 循环等待子进程退出
    havekids = 0; // 标记是否有子进程
    for (np = proc; np < &proc[NPROC]; np++) {
      if (np->parent == p) { // 找到当前进程的子进程
        acquire(&np->lock);
        havekids = 1; // 有子进程
        if (np->state == ZOMBIE) { // 子进程已退出
          pid = np->pid; // 获取子进程的 PID
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate, sizeof(np->xstate)) < 0) {
            release(&np->lock); // 释放锁
            release(&p->lock); // 释放当前进程锁
            return -1; // 复制退出状态失败
          }
          freeproc(np); // 释放子进程资源
          release(&np->lock);
          release(&p->lock);
          return pid; // 返回子进程的 PID
        }
        release(&np->lock);
      }
    }

    // 没有子进程或当前进程被杀死，返回 -1
    if (!havekids || p->killed) {
      release(&p->lock);
      return -1;
    }

    // 没有找到退出的子进程，进入睡眠等待
    sleep(p, &p->lock);
  }
}

// 每个 CPU 的进程调度器。
// 完成设置后，CPU 调用 scheduler()。
// 调度器永远不会返回。它循环执行以下操作：
// - 选择一个进程运行。
// - 切换上下文开始运行该进程。
// - 最终，该进程通过切换上下文返回调度器。
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu(); // 获取当前 CPU

  c->proc = 0; // 当前 CPU 没有正在运行的进程
  for (;;) {
    intr_on(); // 开启中断，防止设备资源死锁

    int found = 0; // 标记是否找到可运行进程
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock); // 获取进程锁
      if (p->state == RUNNABLE) { // 进程处于可运行状态
        p->state = RUNNING; // 设置进程为运行状态
        c->proc = p; // 当前 CPU 的运行进程
        swtch(&c->context, &p->context); // 切换上下文，运行进程

        // 进程运行完毕，释放上下文
        c->proc = 0;
        found = 1; // 找到可运行进程
      }
      release(&p->lock); // 释放进程锁
    }

    // 没有找到可运行进程，进入等待状态
    if (found == 0) {
      intr_on();
      asm volatile("wfi"); // 等待中断
    }
  }
}

// 切换到调度器。必须仅持有 p->lock。
// 需要改变 proc->state。保存和恢复 intena，因为它是当前内核线程的属性，
// 而不是 CPU 的属性。它是 proc->intena 和 proc->noff，但这样会导致在少数情况下出现问题，
// 因为在持有锁的同时没有进程。
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  // 检查锁状态和进程状态
  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena; // 保存中断使能状态
  swtch(&p->context, &mycpu()->context); // 切换上下文到调度器
  mycpu()->intena = intena; // 恢复中断使能状态
}

// 主动放弃 CPU 的使用。
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock); // 获取当前进程锁
  p->state = RUNNABLE; // 设置进程为可运行状态
  sched(); // 调用调度器
  release(&p->lock); // 释放锁
}

// fork 的子进程第一次被调度时，将切换到 forkret。
void
forkret(void)
{
  static int first = 1;

  // 释放从调度器继承的锁
  release(&myproc()->lock);

  if (first) {
    // 文件系统初始化必须在普通进程上下文中运行，
    // 例如，因为它会调用 sleep，因此不能从 main() 中运行。
    first = 0;
    fsinit(ROOTDEV); // 初始化文件系统
  }

  usertrapret(); // 返回用户空间
}

// 原子释放锁，并在 chan 上睡眠。
// 被唤醒后，重新获取锁。
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // 必须获取 p->lock 以更改 p->state 和调用 sched
  if (lk != &p->lock) {
    acquire(&p->lock); // 获取进程锁
    release(lk); // 释放传入的锁
  }

  p->chan = chan; // 设置睡眠通道
  p->state = SLEEPING; // 设置进程为睡眠状态

  sched(); // 调度，放弃 CPU

  // 清理，重新获取锁
  p->chan = 0;
  if (lk != &p->lock) {
    release(&p->lock); // 释放进程锁
    acquire(lk); // 重新获取传入的锁
  }
}

// 唤醒所有在 chan 上睡眠的进程。
// 调用时不能持有任何 p->lock。
void
wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock); // 获取进程锁
    if (p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE; // 唤醒进程
    }
    release(&p->lock); // 释放进程锁
  }
}

// 如果 p 在 wait() 中睡眠，唤醒它。
// 调用者必须持有 p->lock。
static void
wakeup1(struct proc *p)
{
  if (!holding(&p->lock))
    panic("wakeup1");
  if (p->chan == p && p->state == SLEEPING) {
    p->state = RUNNABLE; // 唤醒进程
  }
}

// 终止指定 pid 的进程。
// 被终止的进程在尝试返回用户空间时才会退出（参见 trap.c 中的 usertrap()）。
int
kill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock); // 获取进程锁
    if (p->pid == pid) { // 找到目标进程
      p->killed = 1; // 标记进程为被终止
      if (p->state == SLEEPING) {
        p->state = RUNNABLE; // 唤醒进程
      }
      release(&p->lock);
      return 0; // 返回成功
    }
    release(&p->lock);
  }
  return -1; // 未找到进程，返回失败
}

// 将字符串从内核地址或用户地址复制到内核地址。
// 成功返回 0，失败返回 -1。
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst) {
    return copyout(p->pagetable, dst, src, len); // 复制到用户地址
  } else {
    memmove((char *)dst, src, len); // 复制到内核地址
    return 0;
  }
}

// 从内核地址或用户地址复制字符串到内核地址。
// 成功返回 0，失败返回 -1。
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src) {
    return copyin(p->pagetable, dst, src, len); // 从用户地址复制
  } else {
    memmove(dst, (char *)src, len); // 从内核地址复制
    return 0;
  }
}

// 将进程列表打印到控制台。用于调试。
// 用户在控制台按下 ^P 时运行。
// 不加锁，以避免因锁导致机器卡死。
void
procdump(void)
{
  static char *states[] = { // 进程状态字符串数组
  [UNUSED] "unused",
  [SLEEPING] "sleep ",
  [RUNNABLE] "runble",
  [RUNNING] "run   ",
  [ZOMBIE] "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == UNUSED) // 忽略未使用的进程
      continue;
    state = "???"; // 默认状态
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state]) {
      state = states[p->state]; // 获取状态字符串
    }
    printf("%d %s %s", p->pid, state, p->name); // 打印进程信息
    printf("\n");
  }
}
