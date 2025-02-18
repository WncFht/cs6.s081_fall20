// 睡眠锁相关实现

#include "types.h"          // 基本类型定义
#include "riscv.h"          // RISC-V 架构相关定义
#include "defs.h"           // 公用宏和工具函数
#include "param.h"          // 系统参数
#include "memlayout.h"      // 内存布局定义
#include "spinlock.h"       // 自旋锁相关定义和函数
#include "proc.h"           // 进程管理相关结构和函数
#include "sleeplock.h"      // 睡眠锁相关定义

// 初始化睡眠锁
void initsleeplock(struct sleeplock *lk, char *name) {
  initlock(&lk->lk, "sleep lock"); // 初始化睡眠锁内部的自旋锁
  lk->name = name;                 // 设置睡眠锁的名字
  lk->locked = 0;                  // 初始化锁定状态为未锁定
  lk->pid = 0;                     // 初始化进程 ID 为 0，表示没有进程持有锁
}

// 获取睡眠锁
void acquiresleep(struct sleeplock *lk) {
  acquire(&lk->lk); // 获取睡眠锁内部的自旋锁，确保原子操作
  while (lk->locked) { // 如果睡眠锁已被其他进程持有
    sleep(lk, &lk->lk); // 当前进程进入睡眠状态，等待锁被释放
  }
  lk->locked = 1;       // 设置睡眠锁为已锁定状态
  lk->pid = myproc()->pid; // 记录当前持有锁的进程 ID
  release(&lk->lk);    // 释放睡眠锁内部的自旋锁
}

// 释放睡眠锁
void releasesleep(struct sleeplock *lk) {
  acquire(&lk->lk); // 获取睡眠锁内部的自旋锁，确保原子操作
  lk->locked = 0;   // 设置睡眠锁为未锁定状态
  lk->pid = 0;      // 清除持有锁的进程 ID
  wakeup(lk);       // 唤醒所有等待该锁的进程
  release(&lk->lk); // 释放睡眠锁内部的自旋锁
}

// 检查当前进程是否持有指定的睡眠锁
int holdingsleep(struct sleeplock *lk) {
  int r; // 用于存储检查结果
  acquire(&lk->lk); // 获取睡眠锁内部的自旋锁，确保原子操作

  // 检查睡眠锁是否已锁定且被当前进程持有
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk); // 释放睡眠锁内部的自旋锁
  return r; // 返回检查结果
}
