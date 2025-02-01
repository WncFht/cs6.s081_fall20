#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *target) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if (st.type != T_DIR) {
        fprintf(2, "find: %s is not a directory\n", path);
        close(fd);
        return;
    }

    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too long\n");
        close(fd);
        return;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
            continue;

        // 构造完整路径
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        // 获取当前条目的stat信息
        if (stat(buf, &st) < 0) {
            printf("find: cannot stat %s\n", buf);
            continue;
        }

        // 提取当前条目的名称
        char name[DIRSIZ + 1];
        memmove(name, de.name, DIRSIZ);
        name[DIRSIZ] = 0;

        // 检查是否为文件且名称匹配
        if (st.type == T_FILE && strcmp(name, target) == 0) {
            printf("%s\n", buf);
        }

        // 如果是目录且非.和..，递归处理
        if (st.type == T_DIR && strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            find(buf, target);
        }
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage: find <directory> <filename>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}
