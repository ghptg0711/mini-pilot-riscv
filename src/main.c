// mini-pilot-riscv v0.1: 逐行读取 JSONL 会话日志并回显
// 学习自 loongsuite-pilot (alibaba/loongsuite-pilot) 的输入采集形态
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX 8192

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <session.jsonl>\n", argv[0]);
    return 1;
  }
  FILE *f = fopen(argv[1], "r");
  if (!f) { perror("fopen"); return 1; }
  char line[LINE_MAX];
  long n = 0;
  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0') continue;
    printf("[line %ld] %s\n", ++n, line);
  }
  fclose(f);
  fprintf(stderr, "read %ld lines\n", n);
  return 0;
}
