// 互斥自旋锁实现

#include "types.h"       // 包含类型定义
#include "param.h"       // 包含系统参数
#include "memlayout.h"   // 包含内存布局定义
#include "spinlock.h"    // 包含自旋锁相关定义
#include "riscv.h"       // 包含 RISC-V 相关定义
#include "proc.h"        // 包含进程相关定义
#include "defs.h"        // 包含其他定义

// 初始化自旋锁
void initlock(struct spinlock *lk, char *name) {
  lk->name = name;        // 设置锁的名称，便于调试和跟踪
  lk->locked = 0;         // 初始化锁状态为未锁定
  lk->cpu = 0;            // 初始化锁所属的 CPU 为 0（表示未被任何 CPU 持有）
}

// 获取自旋锁
void acquire(struct spinlock *lk) {
  push_off();             // 禁用中断，避免因中断导致的死锁

  if (holding(lk)) {
    panic("acquire");     // 如果当前 CPU 已经持有该锁，触发 panic，避免死锁
  }

  // 使用原子操作尝试获取锁
  // 在 RISC-V 中，这会编译为一个原子交换指令
  while (__sync_lock_test_and_set(&lk->locked, 1) != 0) {
    ; // 空循环，自旋等待锁被释放
  }

  // 内存屏障，确保在获取锁之后的内存操作不会被重排到锁获取之前
  __sync_synchronize();

  // 记录当前 CPU 持有该锁，便于后续的调试和检查
  lk->cpu = mycpu();
}

// 释放自旋锁
void release(struct spinlock *lk) {
  if (!holding(lk)) {
    panic("release");     // 如果当前 CPU 不持有该锁，触发 panic，避免未持有锁却释放的情况
  }

  lk->cpu = 0;             // 将锁的所属 CPU 置为 0，表示锁已被释放

  // 内存屏障，确保在释放锁之前的内存操作都完成，并且对其他 CPU 可见
  __sync_synchronize();

  // 使用原子操作释放锁
  // 在 RISC-V 中，这会编译为一个原子交换指令
  __sync_lock_release(&lk->locked);

  pop_off();              // 恢复中断状态
}

// 检查当前 CPU 是否持有该锁
int holding(struct spinlock *lk) {
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// 禁用中断，并累计禁用次数
void push_off(void) {
  int old = intr_get();   // 获取当前中断状态

  intr_off();             // 禁用中断
  if (mycpu()->noff == 0) {
    mycpu()->intena = old; // 记录禁用中断前的中断状态
  }
  mycpu()->noff += 1;      // 增加禁用中断的计数
}

// 恢复中断，并减少禁用次数
void pop_off(void) {
  struct cpu *c = mycpu();
  if (intr_get()) {
    panic("pop_off - interruptible"); // 如果在 pop_off 时中断是开启的，触发 panic
  }
  if (c->noff < 1) {
    panic("pop_off");     // 可能 pop_off 被调用的次数过多，导致计数器为负数，触发 panic
  }
  c->noff -= 1;           // 减少禁用中断的计数
  if (c->noff == 0 && c->intena) {
    intr_on();            // 如果计数器为 0 且之前中断是开启的，重新开启中断
  }
}
