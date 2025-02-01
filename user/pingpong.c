#include "kernel/types.h"
#include "user/user.h"

int main() {
    int pipe1[2], pipe2[2];
    char byte;

    // 创建两个管道：pipe1用于父进程到子进程，pipe2用于子进程到父进程
    if (pipe(pipe1) < 0) {
        fprintf(2, "Failed to create pipe1\n");
        exit(1);
    }
    if (pipe(pipe2) < 0) {
        fprintf(2, "Failed to create pipe2\n");
        exit(1);
    }

    switch (fork()) {
        case -1:
            fprintf(2, "Fork failed\n");
            exit(1);

        // 子进程
        case 0:
            // 关闭无关的管道端
            close(pipe1[1]);  // 子进程不需要写 pipe1
            close(pipe2[0]);  // 子进程不需要读 pipe2

            // 从 pipe1 读取一个字节
            if (read(pipe1[0], &byte, 1) != 1) {
                fprintf(2, "Child failed to read from pipe1\n");
                exit(1);
            }
            printf("%d: received ping\n", getpid());

            // 向 pipe2 写入一个字节
            if (write(pipe2[1], &byte, 1) != 1) {
                fprintf(2, "Child failed to write to pipe2\n");
                exit(1);
            }

            // 关闭所有管道
            close(pipe1[0]);
            close(pipe2[1]);
            exit(0);

        // 父进程
        default:
            // 关闭无关的管道端
            close(pipe1[0]);  // 父进程不需要读 pipe1
            close(pipe2[1]);  // 父进程不需要写 pipe2

            // 向 pipe1 写入一个字节
            if (write(pipe1[1], &byte, 1) != 1) {
                fprintf(2, "Parent failed to write to pipe1\n");
                exit(1);
            }

            // 从 pipe2 读取一个字节
            if (read(pipe2[0], &byte, 1) != 1) {
                fprintf(2, "Parent failed to read from pipe2\n");
                exit(1);
            }
            printf("%d: received pong\n", getpid());

            // 关闭所有管道
            close(pipe1[1]);
            close(pipe2[0]);
            exit(0);
    }

    return 0;
}
