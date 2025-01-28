#ifndef error_handling_h
#define error_handling_h

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int error_code;
  char *message;
} msg_server_error_t;

static void msg_server_set_error(msg_server_error_t *error, int error_code,
                                 const char *message) {
  if (error == NULL)
    return;

  // free existing message
  free(error->message);
  error->message = NULL;

  // set error and message
  error->error_code = error_code;
  error->message = message ? strdup(message) : strdup(strerror(error_code));
}

static void msg_server_set_error_errno(msg_server_error_t *error,
                                       const char *prefix) {
  if (error == NULL)
    return;

  int error_code = errno;
  const char *error_message = strerror(error_code);
  size_t message_len = strlen(error_message);
  size_t prefix_len = prefix ? strlen(prefix) : 0;
  error->message = malloc(prefix_len + message_len + 3); /* 2 for ": " + '\n' */

  if (error->message) {
    if (prefix)
      sprintf(error->message, "%s: %s", prefix, error_message);
    else
      strcpy(error->message, error_message);
  }
}

static void msg_server_set_error_vprintf(msg_server_error_t *error,
                                         int error_code, const char *format,
                                         ...) {
  if (error == NULL)
    return;

  free(error->message);
  error->message = NULL;

  va_list args;
  va_start(args, format);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  buffer[sizeof(buffer) - 1] = '\0';

  va_end(args);

  error->error_code = error_code;
  error->message = strdup(buffer);
}

#endif // error_handling_h
