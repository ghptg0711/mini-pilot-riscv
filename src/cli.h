#ifndef MINI_PILOT_CLI_H
#define MINI_PILOT_CLI_H

/* cli component: argument parsing and the `status` ops subcommand,
 * mirroring upstream's cli/ directory (token-usage, status, ...). */

typedef struct {
  const char *path;
  int is_codex;
  int emit_content;
  int mask;
} cli_options_t;

/* Returns 0 and fills `opt` for the collect pipeline; returns 1 when a
 * subcommand (e.g. `status`) was handled and the process should exit
 * with the returned code in *exit_code. */
int cli_parse(int argc, char **argv, cli_options_t *opt, int *exit_code);

int cli_status_cmd(void);

#endif
