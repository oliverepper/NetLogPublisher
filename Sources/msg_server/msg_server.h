#ifndef msg_server_h
#define msg_server_h

#include "error_handling.h"

typedef enum {
  MSG_SERVER_SUCCESS,
  MSG_SERVER_FAILURE
} msg_server_status_t;

typedef struct msg_server_instance_t *msg_server_t;
typedef int msg_server_callback_handle; ;

msg_server_status_t msg_server_init(msg_server_error_t *error);
msg_server_status_t msg_server_stop();
msg_server_t msg_server_create(const char *service_name);
void msg_server_add_callback(msg_server_t instance, void (*cb)(const char *msg, void *ctx), void *ctx);

#endif // msg_server_h
