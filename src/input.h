#ifndef MINI_PILOT_INPUT_H
#define MINI_PILOT_INPUT_H

/* input component: read one agent session line into a normalized record.
 * Mirrors upstream's "inputs" concept: agent-native formats differ, the
 * collector normalizes them via a field-alias table before emission.
 * Supported native formats (simulated, simplified): claude-code, codex,
 * cursor, qoder. */

#include "common.h"

typedef struct {
  char ts[FIELD_MAX_LEN], type[64], status[64], model[FIELD_MAX_LEN];
  char session[FIELD_MAX_LEN], tool[FIELD_MAX_LEN], text[LINE_MAX_LEN];
  double tin, tout;
} record_t;

/* Parse one JSONL line of any known agent-native format into `r`.
 * Unknown lines yield type == "" and are skipped by the caller. */
void input_read_record(const char *line, record_t *r);

#endif
