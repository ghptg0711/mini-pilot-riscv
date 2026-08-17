#include "jsonlite.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

// 定位 "key" : value 的 value 起始指针；未找到返回 NULL。
// 遍历所有 "key" 出现位置，取第一个后跟冒号的（避免匹配到值位置，
// 如 {"type": "tool", "tool": "Write"} 中 "tool" 首次出现在值处）。
static const char *find_value(const char *line, const char *key) {
  char pat[80];
  if (snprintf(pat, sizeof(pat), "\"%s\"", key) >= (int)sizeof(pat)) return NULL;
  const char *p = line;
  while ((p = strstr(p, pat)) != NULL) {
    const char *q = p + strlen(pat);
    while (*q && isspace((unsigned char)*q)) q++;
    if (*q == ':') {
      q++;
      while (*q && isspace((unsigned char)*q)) q++;
      return q;
    }
    p += strlen(pat);
  }
  return NULL;
}

// v0.9: 转义感知拷贝——源 JSON 字符串里的 \" \\ \n \t \r 还原为字面值，
// \uXXXX 以 '?' 占位（教学级，注释声明）。修复含引号文本被截断的问题。
int json_get_str(const char *line, const char *key, char *out, int outsz) {
  const char *v = find_value(line, key);
  if (!v || *v != '"') return 0;
  v++;
  int i = 0;
  while (*v && i < outsz - 1) {
    if (*v == '\\') {
      v++;
      switch (*v) {
        case '"':  out[i++] = '"';  break;
        case '\\': out[i++] = '\\'; break;
        case 'n':  out[i++] = '\n'; break;
        case 't':  out[i++] = '\t'; break;
        case 'r':  out[i++] = '\r'; break;
        case 'u':  out[i++] = '?'; v += 4; break;   // \uXXXX 简化占位
        case '\0': out[i++] = '\\'; v--; break;
        default:   out[i++] = *v;
      }
      v++;
    } else if (*v == '"') {
      break;
    } else {
      out[i++] = *v++;
    }
  }
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
