// 极简 JSON 行解析器：面向"扁平对象 + 字符串/数字/布尔"的固定输入结构。
// 局限性明确声明：不支持嵌套对象/数组/转义边界，仅服务于本项目 fixture 级输入。
// 设计参照：学习 loongsuite-pilot 用成熟 parser（jsonc-parser）的边界，
// 嵌入式/教学场景下以受控输入换零依赖（便于 riscv64 静态交叉编译）。
#ifndef JSONLITE_H
#define JSONLITE_H

// 从 json 行中提取顶层字符串字段到 out（含引号去除）。找到返回 1。
int json_get_str(const char *line, const char *key, char *out, int outsz);
// 提取数字字段（整数或浮点，写入 double）。找到返回 1。
int json_get_num(const char *line, const char *key, double *out);
// 提取布尔字段。找到返回 1，值写入 *out。
int json_get_bool(const char *line, const char *key, int *out);

#endif
