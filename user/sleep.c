#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char const *argv[])
{
    int ticks;

    // 参数错误
    if (argc != 2) {
      fprintf(2, "usage: sleep <time>\n");
      exit(1);
    }

    // 将命令行参数转换为整数
    ticks = atoi(argv[1]);
    if (ticks <= 0) {
        fprintf(2, "Ticks must be a positive integer\n");
        exit(1);
    }

    // 调用系统调用 sleep
    if (sleep(ticks) < 0) {
        fprintf(2, "Sleep failed\n");
        exit(1);
    }

    // 正常退出
    exit(0);
}
