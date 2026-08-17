#ifndef MINI_PILOT_COMMON_H
#define MINI_PILOT_COMMON_H

/* Shared definitions for mini-pilot components.
 * Layout mirrors the upstream loongsuite-pilot component split:
 * input -> normalize/emit (flusher) -> mask, orchestrated by main. */

#define MINI_PILOT_VERSION "0.12.0"

#define LINE_MAX_LEN 8192
#define FIELD_MAX_LEN 512

#endif
