#include "jsonlite.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// 定位 "key" : value 的 value 起始指针；未找到返回 NULL。
static const char *find_value(const char *line, const char *key) {
  size_t klen = strlen(key);
  const char *p = line;
  while ((p = strstr(p, key)) != NULL) {
    // 前后必须是引号边界，且这是 key 位置（形如 "key"）
    int prev_ok = (p == line) || (*(p - 1) == '"' || *(p - 1) == '{' || *(p - 1) == ',' || isspace((unsigned char)*(p - 1)));
    if (*p != '"') { p++; continue; }
    if (p > line && *(p - 1) != '"' && *(p - 1) != '{' && *(p - 1) != ',' && !isspace((unsigned char)*(p - 1))) { p++; continue; }
    const char *q = p + 1;              // 跳过开引号
    q += klen;
    if (*q != '"') { p = q; continue; } // key 不完全匹配
    (void)prev_ok;
    q++;                                 // 跳过闭引号
    while (*q && isspace((unsigned char)*q)) q++;
    if (*q == ':') {
      q++;
      while (*q && isspace((unsigned char)*q)) q++;
      return q;
    }
    p = q;
  }
  return NULL;
}

int json_get_str(const char *line, const char *key, char *out, int outsz) {
  const char *v = find_value(line, key);
  if (!v || *v != '"') return 0;
  v++;
  int i = 0;
  while (*v && *v != '"' && i < outsz - 1) out[i++] = *v++;
  out[i] = '\0';
  return 1;
}

int json_get_num(const char *line, const char *key, double *out) {
  const char *v = find_value(line, key);
  if (!v || *v == '"') return 0;
  *out = strtod(v, NULL);
  return 1;
}

int json_get_bool(const char *line, const char *key, int *out) {
  const char *v = find_value(line, key);
  if (!v) return 0;
  if (strncmp(v, "true", 4) == 0) { *out = 1; return 1; }
  if (strncmp(v, "false", 5) == 0) { *out = 0; return 1; }
  return 0;
}
