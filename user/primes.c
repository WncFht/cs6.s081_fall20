#include "kernel/types.h"
#include "user/user.h"

void sieve(int fd) {
    int p;
    // 检查读取是否成功
    if (read(fd, &p, sizeof(p)) != sizeof(p)) {
        close(fd);
        exit(0);
    }
    printf("prime %d\n", p);

    int pfd[2];
    pipe(pfd);

    if (fork() == 0) {
        close(pfd[1]);  // 子进程关闭写端
        sieve(pfd[0]);  // 递归处理下一层
    } else {
        close(pfd[0]);  // 父进程关闭读端
        int n;
        while (read(fd, &n, sizeof(n)) == sizeof(n)) {
            if (n % p != 0) {
                write(pfd[1], &n, sizeof(n));
            }
        }
        close(fd);          // 关闭原始管道读端
        close(pfd[1]);      // 关闭新管道写端
        wait(0);            // 等待子进程退出
    }
}

int main() {
    int pfd[2];
    pipe(pfd);

    if (fork() == 0) {
        close(pfd[1]);      // 子进程关闭写端
        sieve(pfd[0]);      // 启动筛法
    } else {
        close(pfd[0]);      // 主进程关闭读端
        for (int i = 2; i <= 35; i++) {
            write(pfd[1], &i, sizeof(i));
        }
        close(pfd[1]);      // 关闭写端，发送EOF
        wait(0);            // 等待筛法进程结束
    }
    exit(0);
}
