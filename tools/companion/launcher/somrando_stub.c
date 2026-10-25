/* Stand-in for libsomrando until the C port exists.  See somrando_api.h. */
#include <stdio.h>
#include "somrando_api.h"

int somrando_available(void) { return 0; }
const char *somrando_version(void) { return "1.49"; }

int somrando_supports(const struct somrando_request *req, char *why, size_t why_len) {
  (void)req;
  if (why && why_len) snprintf(why, why_len, "libsomrando is not built into this app yet");
  return 0;
}

int somrando_generate(const struct somrando_request *req, somrando_progress_fn progress, void *user, char *err, size_t err_len) {
  (void)req; (void)progress; (void)user;
  if (err && err_len) snprintf(err, err_len, "libsomrando is not built into this app yet");
  return SOMRANDO_ERR_UNAVAILABLE;
}
