#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_LINE 512

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs <command> [args...]\n");
        exit(1);
    }

    char line[MAX_LINE];
    char *cmd_argv[MAXARG];
    int orig_argc = argc - 1;

    for (int i = 0; i < orig_argc; i++) {
        cmd_argv[i] = argv[i + 1];
    }

    while (1) {
        int pos = 0;
        int read_result;
        // 读取一行输入
        while (1) {
            char c;
            read_result = read(0, &c, 1);
            if (read_result <= 0 || c == '\n') {
                break;
            }
            if (pos < MAX_LINE - 1) {
                line[pos++] = c;
            }
        }
        line[pos] = '\0';

        if (read_result < 0) {
            fprintf(2, "xargs: read error\n");
            exit(1);
        }

        // 处理EOF或空行
        if (read_result == 0 && pos == 0) {
            break;
        }

        // 分割参数
        char *line_argv[MAXARG];
        int line_argc = 0;
        char *p = line;
        while (*p != '\0') {
            // 跳过空格
            while (*p == ' ') {
                *p = '\0';
                p++;
            }
            if (*p == '\0') {
                break;
            }
            line_argv[line_argc++] = p;
            // 寻找参数结束位置
            while (*p != ' ' && *p != '\0') {
                p++;
            }
        }

        // 检查参数总数是否超出限制
        int total_args = orig_argc + line_argc;
        if (total_args >= MAXARG) {
            fprintf(2, "xargs: too many arguments\n");
            exit(1);
        }

        // 构造新参数数组
        char *new_argv[MAXARG];
        for (int i = 0; i < orig_argc; i++) {
            new_argv[i] = cmd_argv[i];
        }
        for (int i = 0; i < line_argc; i++) {
            new_argv[orig_argc + i] = line_argv[i];
        }
        new_argv[total_args] = 0;

        // 执行命令
        int pid = fork();
        if (pid < 0) {
            fprintf(2, "xargs: fork failed\n");
            exit(1);
        } else if (pid == 0) {
            exec(new_argv[0], new_argv);
            fprintf(2, "xargs: exec %s failed\n", new_argv[0]);
            exit(1);
        } else {
            wait(0);
        }
    }

    exit(0);
}
