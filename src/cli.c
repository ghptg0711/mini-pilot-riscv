#include "cli.h"
#include "common.h"
#include "flusher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

static const char *build_arch(void) {
#if defined(__riscv) && __riscv_xlen == 64
  return "riscv64";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__aarch64__)
  return "aarch64";
#else
  return "unknown";
#endif
}

/* Minimal ops surface: version / build arch / run arch / capabilities.
 * Mirrors the upstream deliverable "loongsuite-pilot status/info works". */
int cli_status_cmd(void) {
  struct utsname u;
  printf("mini-pilot %s\n", MINI_PILOT_VERSION);
  printf("build arch : %s\n", build_arch());
  if (uname(&u) == 0)
    printf("run  arch  : %s (%s %s)\n", u.machine, u.sysname, u.release);
  printf("node       : %s\n",
         getenv("COLLECTOR_NODE") ? getenv("COLLECTOR_NODE") : "local");
  printf("capabilities:\n");
  printf("  - agents        : claude-code, codex\n");
  printf("  - outputs       : jsonl(stdout)\n");
  printf("  - content/mask  : opt-in\n");
  return 0;
}

int cli_parse(int argc, char **argv, cli_options_t *opt, int *exit_code) {
  memset(opt, 0, sizeof(*opt));
  if (argc >= 2 && strcmp(argv[1], "status") == 0) {
    *exit_code = cli_status_cmd();
    return 1;
  }
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--content") == 0) opt->emit_content = 1;
    else if (strcmp(argv[i], "--mask") == 0) opt->mask = 1;
    else if (strcmp(argv[i], "--agent") == 0 && i + 1 < argc) {
      opt->is_codex = (strcmp(argv[++i], "codex") == 0);
    } else if (!opt->path) {
      opt->path = argv[i];
    }
  }
  if (!opt->path) {
    fprintf(stderr,
            "usage: %s <session.jsonl> [--agent claude-code|codex] [--content] [--mask]\n"
            "       %s status\n",
            argv[0], argv[0]);
    *exit_code = 1;
    return 1;
  }
  return 0;
}
