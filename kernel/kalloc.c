// 物理内存分配器，为用户进程、内核栈、页表页和管道缓冲区分配整个 4096 字节的页面。

#include "types.h"       // 包含类型定义
#include "param.h"       // 包含系统参数
#include "memlayout.h"   // 包含内存布局定义
#include "spinlock.h"    // 包含自旋锁相关定义
#include "riscv.h"       // 包含 RISC-V 相关定义
#include "defs.h"        // 包含其他定义

void freerange(void *pa_start, void *pa_end); // 声明一个函数，用于释放指定范围的物理内存

// end 是内核的结束地址，由链路器脚本 kernel.ld 定义
extern char end[];

// 定义链表节点结构体，用于管理空闲物理内存页
struct run {
  struct run *next; // 指向下一个空闲内存页的指针
};

// 内存分配器的全局结构体
struct {
  struct spinlock lock;  // 自旋锁，用于保护自由链表
  struct run *freelist;  // 自由链表的头指针，指向第一个空闲的物理内存页
} kmem;

// 内存分配器初始化函数
void kinit() {
  // 初始化自旋锁
  initlock(&kmem.lock, "kmem");
  
  // 释放从内核结束地址到物理内存结束地址的所有内存页
  freerange(end, (void*)PHYSTOP);
}

// 释放指定地址范围内的所有物理内存页
void freerange(void *pa_start, void *pa_end) {
  char *p; // 用于逐页处理物理地址的指针
  // 将起始地址向上对齐到页的边界
  p = (char*)PGROUNDUP((uint64)pa_start);
  
  // 逐页释放内存
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    // 将每个页面加入自由链表
    kfree(p);
  }
}

// 释放指定物理地址对应的物理内存页
void kfree(void *pa) {
  struct run *r; // 用于管理空闲内存页的链表节点

  // 检查地址是否对齐，以及是否位于内核有效地址范围内
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) {
    panic("kfree"); // 如果地址不合法，触发 panic
  }

  // 填充垃圾数据，防止悬挂引用
  memset(pa, 1, PGSIZE);

  // 获取当前页面的地址
  r = (struct run*)pa;

  // 加锁，保护自由链表
  acquire(&kmem.lock);
  
  // 将当前页面添加到自由链表的头部
  r->next = kmem.freelist;
  kmem.freelist = r;
  
  // 释放锁
  release(&kmem.lock);
}

// 分配一个 4096 字节的物理内存页
void* kalloc() {
  struct run *r; // 用于管理空闲内存页的链表节点

  // 加锁，保护自由链表
  acquire(&kmem.lock);
  
  // 获取自由链表的第一个节点
  r = kmem.freelist;
  if (r) {
    // 更新自由链表的头指针
    kmem.freelist = r->next;
  }
  
  // 释放锁
  release(&kmem.lock);

  // 如果分配成功，填充垃圾数据并返回
  if (r) {
    memset((char*)r, 5, PGSIZE); // 在返回前填充垃圾数据，防止悬挂引用
  }
  
  return (void*)r; // 返回空闲内存页的地址
}
