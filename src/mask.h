#ifndef MINI_PILOT_MASK_H
#define MINI_PILOT_MASK_H

/* mask component: irreversible content fingerprints for Opt-In fields.
 * Mirrors upstream's src/mask/ concept (entry-masker / pii-detectors).
 * Teaching-grade note: FNV-1a is NOT cryptographic; production should
 * follow upstream docs/masking.md rules or use SHA-256. */

unsigned long long mask_fnv1a(const char *s);

#endif
