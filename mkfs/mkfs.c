#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
 
#define stat xv6_stat  // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#include "param.h"
// 静态断言，当a为0时switch将无法通过编译
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
// 块数
int nblocks = (995-LOGSIZE);
// 日志块数
int nlog = LOGSIZE;
// i节点数
int ninodes = 200;
// 文件系统大小
int size = 1024;
// 文件句柄
int fsfd;
// 超级块
struct superblock sb;
//
char zeroes[512];
// 待分配的空闲块号，后面再解释
uint freeblock;
// 数据区的起始数据块块号？？？？
uint usedblocks;
// 位图块数
uint bitblocks;
// 待分配的空闲i节点号，i节点0是不用了
uint freeinode = 1;
 
void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);
 
// 将数据的存储格式转为intel字节序，也就是所谓的大端小端
// convert to intel byte order
ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}
 
uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}
 
int
main(int argc, char *argv[])
{
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[512];
  struct dinode din;
  
  // int类型必须是4个字节
  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");
  // 参数要大于2
  if(argc < 2){
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }
  // 这里的512是逻辑块的字节数，也就是逻辑块要能被i节点或目录结构体的大小整除
  assert((512 % sizeof(struct dinode)) == 0);
  assert((512 % sizeof(struct dirent)) == 0);
 
  fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
  if(fsfd < 0){
    perror(argv[1]);
    exit(1);
  }
  // 计算超级块结构体各个字段的值
  // 文件系统大小
  sb.size = xint(size);
  // 块数
  sb.nblocks = xint(nblocks); // so whole disk is size sectors
  // i节点数
  sb.ninodes = xint(ninodes);
  // 逻辑块数
  sb.nlog = xint(nlog);
  // 位图块的数量，一块有512字节，一个字节8位，+1是因为size / (512*8)可能为0
  bitblocks = size/(512*8) + 1;
  // 下面这句用 boot + sb + ninodes / IPB + bitblocks可能更好理解，
  // 这里的3让我有点蒙，不因该是2么？
  usedblocks = ninodes / IPB + 3 + bitblocks;
  // 前面提到freeblock是待分配空闲块号，而第一块的值是要根据引导块、超级、i节点条目块、位图块
  // 相加得出的
  freeblock = usedblocks;
 
  printf("used %d (bit %d ninode %zu) free %u log %u total %d\n", usedblocks,
         bitblocks, ninodes/IPB + 1, freeblock, nlog, nblocks+usedblocks+nlog);
  // 验证计算的值是否正确
  assert(nblocks + usedblocks + nlog == size);
  // 这里的作用是构建fs.img，也就是写size个块
  for(i = 0; i < nblocks + usedblocks + nlog; i++)
    wsect(i, zeroes);// 函数的作用是写一块，记住全局未初始化变量默认值为0
  
  // 下面三句的意思是将超级块结构体变量写到超级块上
  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);
  // ialloc是建立一个i节点，这里是建立根目录i节点
  rootino = ialloc(T_DIR);
  // 根目录的i节点号必须是1
  assert(rootino == ROOTINO);
  // de是目录结构变量
  bzero(&de, sizeof(de));
  
  // 一个目录必然包含.和..目录，根目录的当前目录和父目录都是自己
  // iappend函数是本程序最难理解的地方，后面做分析
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  // 这里iappend是将目录结构写入到根目录文件的数据块上
  iappend(rootino, &de, sizeof(de));
 
  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));
   
  // 这个循环是将从第三个参数开始的文件都添加到镜像里
  for(i = 2; i < argc; i++){
	// 本程序只支持将文件添加到根目录下，所以不能带有路径分隔符
    assert(index(argv[i], '/') == 0);
    
	// 打开待添加到镜像的文件
    if((fd = open(argv[i], 0)) < 0){
      perror(argv[i]);
      exit(1);
    }
    
    // Skip leading _ in name when writing to file system.
    // The binaries are named _rm, _cat, etc. to keep the
    // build operating system from trying to execute them
    // in place of system binaries like rm and cat.
    // 在构造xv6系统时，构建的xv6命令的可执行文件都被加上了"_"
	// 添加到镜像里面时就去掉"_"
	if(argv[i][0] == '_')
      ++argv[i];
    
	// 构建一个i节点它的类型是文件
    inum = ialloc(T_FILE);
    // 将此文件的目录项结构添加到根目录
    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, argv[i], DIRSIZ);// 文件名最长是DIRSIZ个字符
    iappend(rootino, &de, sizeof(de));
    
	// iappend不仅仅用于写目录项到目录文件数据块
	// 也用于将数据写到普通文件的数据块里
	// 这里是实际的将待添加到镜像的文件的数据写入到对应的数据块里
	// 这里可能比较难以理解
    while((cc = read(fd, buf, sizeof(buf))) > 0)
      iappend(inum, buf, cc);
 
    close(fd);
  }
  // 通过观察linux新建一个空目录，我们发现新建的目录的大小等于一个逻辑块的大小
  // xv6逻辑块的大小是512，下面的语句作用就在于将根目录的大小对齐到512的倍数上
  // fix size of root inode dir
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off/BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);
  
  // 起初我把balloc理解为分配块，显然我错了，
  // 这里它的作用是将已经用过的数据块的位图置位
  // 这也是为什么balloc只被调用了一次
  balloc(usedblocks);
 
  exit(0);
}
 
// 写块
void
wsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * 512L, 0) != sec * 512L){
    perror("lseek");
    exit(1);
  }
  if(write(fsfd, buf, 512) != 512){
    perror("write");
    exit(1);
  }
}
 
// 用于计算某i节点条目所在的磁盘块号
// 这里的2就是引导块、超级块
uint
i2b(uint inum)
{
  return (inum / IPB) + 2;
}
 
// 写i节点条目
void
winode(uint inum, struct dinode *ip)
{
  char buf[512];
  uint bn;
  struct dinode *dip;
   
  // 计算inum所在的块号 
  bn = i2b(inum);
  // 读取这块到buf
  rsect(bn, buf);
  // 自己思考下这句的意思
  dip = ((struct dinode*)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}
 
// 读i节点条目
void
rinode(uint inum, struct dinode *ip)
{
  char buf[512];
  uint bn;
  struct dinode *dip;
 
  bn = i2b(inum);
  rsect(bn, buf);
  // inum % IPB是计算i节点在buf中的偏移
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}
 
// 读扇区块
void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * 512L, 0) != sec * 512L){
    perror("lseek");
    exit(1);
  }
  if(read(fsfd, buf, 512) != 512){
    perror("read");
    exit(1);
  }
}
 
// 构建i节点
uint
ialloc(ushort type)
{
  // freeinode一开始是1哦
  uint inum = freeinode++;
  struct dinode din;
 
  bzero(&din, sizeof(din));
  din.type = xshort(type);// 文件类型
  din.nlink = xshort(1);// 文件的链接数
  din.size = xint(0);// 文件的大小
  winode(inum, &din);// 写到i节点条目对应的块上
  return inum;
}
 
// 分配数据块
void
balloc(int used)
{
  uchar buf[512];
  int i;
 
  printf("balloc: first %d blocks have been allocated\n", used);
  assert(used < 512*8);
  bzero(buf, 512);
  // 前面我提到balloc是将已经使用的数据块位图一次性置位
  // 这里可以用循环依次置位的前提是文件系统新建立数据都是空的
  for(i = 0; i < used; i++){
    buf[i/8] = buf[i/8] | (0x1 << (i%8));
  }
  printf("balloc: write bitmap block at sector %zu\n", ninodes/IPB + 3);
  wsect(ninodes / IPB + 3, buf);
}
 
#define min(a, b) ((a) < (b) ? (a) : (b))
 
// 函数的本意是将n个字节的数据追加到inum节点的数据块上 
void
iappend(uint inum, void *xp, int n)
{
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[512];
  uint indirect[NINDIRECT];// 一次间接块
  uint x;
 
  rinode(inum, &din);
 
  off = xint(din.size);
  while(n > 0){
    fbn = off / 512;
    assert(fbn < MAXFILE);
	// 判断为真说明是直接块
    if(fbn < NDIRECT){
	  // 下面这条判断语句信息量可以说有点大，这里我们不展开说了
	  // 这里就是文件对应的块没分配我们就给他分配一块
      if(xint(din.addrs[fbn]) == 0){
        din.addrs[fbn] = xint(freeblock++);// 为什么balloc只用调用一次关键就是freeblock
        usedblocks++;                      // 和usedblocks的累加，到时候一次性置位这些用过的块
      }
      x = xint(din.addrs[fbn]);
    } else {// 一次间接块
      if(xint(din.addrs[NDIRECT]) == 0){
        // printf("allocate indirect block\n");
        din.addrs[NDIRECT] = xint(freeblock++);
        usedblocks++;
      }
      // printf("read indirect block\n");
      rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      if(indirect[fbn - NDIRECT] == 0){
        indirect[fbn - NDIRECT] = xint(freeblock++);
        usedblocks++;
        wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(indirect[fbn-NDIRECT]);
    }
    n1 = min(n, (fbn + 1) * 512 - off);// 这句我也没看明白
    rsect(x, buf);// 如果写的是某块的某部分，那就要先读取这块
    bcopy(p, buf + off - (fbn * 512), n1);// 然后只改变某部分
    wsect(x, buf);// 然后写回去
    n -= n1;// 还需要写入的字节数n相应的减少
    off += n1;// 偏移增加
    p += n1;// 指针也做相应增加
  }
  din.size = xint(off);//
  winode(inum, &din);// 保存修改的i节点条目到盘块
}
