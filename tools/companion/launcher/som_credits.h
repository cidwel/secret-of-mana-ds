/* Turns data/credits.json (the single source of the "Special thanks" text) into plain text lines for any frontend: the PC menu, the Thor / Android
 * menu and the 3DS one render the same words.  Pure C99, no I/O: the host reads the file (or embeds a copy) and passes the bytes.
 * Entries are objects with "who" (or "author"), "what" (or "patch"), optional "license" and "url"; unknown keys are ignored. */
#ifndef SOM_CREDITS_H
#define SOM_CREDITS_H

#include <stddef.h>

/* Writes "TITLE\n\nWho\nWhat\nLicense: x\nurl\n\n..." into out.  Returns the length, or -1 when the JSON has no entries; when out is too
 * small the text is cut at an entry boundary (never in the middle of an entry) and the length of what was written is returned. */
int sc_format(const char *json, char *out, size_t n);

/* The text used when the file cannot be read, so the credits are never empty. */
const char *sc_fallback(void);

#endif
