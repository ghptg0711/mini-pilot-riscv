#ifndef MINI_PILOT_FLUSHER_H
#define MINI_PILOT_FLUSHER_H

/* flusher component: emit one normalized record as a GenAI event line
 * (JSONL to stdout). Mirrors upstream's "flushers" concept; the schema
 * source of truth is upstream docs/output-event-schema.md. */

#include "input.h"

typedef struct {
  const char *agent_type;  /* gen_ai.agent.type, e.g. claude-code/codex/cursor/qoder */
  const char *provider;    /* gen_ai.provider.name (see upstream Provider Names) */
  int emit_content;        /* opt-in gen_ai.input.messages */
  int mask;                /* mask content with FNV-1a digest */
} flusher_opts_t;

void flusher_emit(const record_t *r, const flusher_opts_t *opts);

#endif
