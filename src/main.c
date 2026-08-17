/* mini-pilot: pipeline orchestrator.
 * Layout mirrors upstream loongsuite-pilot (index.ts + core/orchestrator):
 *   cli -> input (agent format aliasing) -> flusher (GenAI event emission)
 *   with mask applied to opt-in content fields. */
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "cli.h"
#include "input.h"
#include "flusher.h"

int main(int argc, char **argv) {
  cli_options_t opt;
  int exit_code = 0;
  if (cli_parse(argc, argv, &opt, &exit_code)) return exit_code;

  FILE *f = fopen(opt.path, "r");
  if (!f) {
    perror("fopen");
    return 1;
  }

  flusher_opts_t fopt = {
      .is_codex = opt.is_codex,
      .emit_content = opt.emit_content,
      .mask = opt.mask,
  };

  char line[LINE_MAX_LEN];
  while (fgets(line, sizeof(line), f)) {
    /* drop truncated overlong lines instead of emitting garbage */
    size_t len = strlen(line);
    if (len == sizeof(line) - 1 && line[len - 1] != '\n') {
      fprintf(stderr, "warn: overlong line skipped\n");
      int ch;
      while ((ch = fgetc(f)) != '\n' && ch != EOF) {}
      continue;
    }
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0') continue;

    record_t r;
    input_read_record(line, &r);
    flusher_emit(&r, &fopt);
  }
  fclose(f);
  return 0;
}
