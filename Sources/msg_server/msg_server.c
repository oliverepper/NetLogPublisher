#include "error_handling.h"
#include "linked_list.h"
#include "msg_server.h"
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/event.h>

struct msg_server_instance_t {
  char *service_name;
  // MAYBE: Allow the installation of multiple callbacks
  void (*cb)(const char *msg, void *ctx);
  void *ctx;
};

static _Atomic(list_node_t) atomic_head = NULL;

static int kq = 0;
static pthread_t evt_thread;

msg_server_t _get_server_for_servname(const char *service_name);
msg_server_t _create(const char *service_name);
msg_server_t _remember(msg_server_t server);
msg_server_status_t _setup_socket(const char *, msg_server_error_t *);

void *handle_event(void *arg) {
  int kq = *(int *)arg;
  struct kevent event;

  for (;;) {
    int n_events = kevent(kq, NULL, 0, &event, 1, NULL);

    if (event.filter == EVFILT_READ) {
      int current_socket = (int)event.ident;
      const char *servname = (const char *)event.udata;

      char buffer[256] = {0};
      ssize_t bytes_received =
        recvfrom(current_socket, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
      if (bytes_received < 0)
        continue;
      buffer[bytes_received] = '\0';
      msg_server_t current_server = _get_server_for_servname(servname);
      if (current_server->cb)
        current_server->cb(buffer, current_server->ctx);
    } else if (event.filter == EVFILT_TIMER) {
      printf("Timer event received.\n");
    }
  }

  return NULL;
}

msg_server_status_t msg_server_init(msg_server_error_t *error) {
  if (kq != 0) {
    msg_server_set_error(error, 0, "msg_server is already initialized");
    return MSG_SERVER_FAILURE;
  }

  // initialise the event queue
  kq = kqueue();
  if (kq < 0) {
    msg_server_set_error_errno(error, "kqueue()");
    return MSG_SERVER_FAILURE;
  }

  // setup the keep-alive signal
  struct kevent change;
  EV_SET(&change, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, 1000, 0);
  if (kevent(kq, &change, 1, NULL, 0, NULL) < 0) {
    msg_server_set_error_errno(error, "kevent()");
    return MSG_SERVER_FAILURE;
  }

  // spawn a handler thread
  pthread_create(&evt_thread, NULL, handle_event, &kq);

  return MSG_SERVER_SUCCESS;
}

msg_server_status_t msg_server_stop() {
  pthread_join(evt_thread, NULL);
  return MSG_SERVER_SUCCESS;
}

msg_server_t msg_server_create(const char *service_name) {
  // "0" is meant to be the default value
  if (!service_name)
    service_name = "0";

  // return existing server
  if (strcmp(service_name, "0") != 0) {
    msg_server_t existing_server = _get_server_for_servname(service_name);
    if (existing_server)
      return existing_server;
  }

  // create and remember a new server
  msg_server_t new_instance = _remember(_create(service_name));

  return new_instance;
}

bool _same_service_name(const void *left, const void *right) {
  return strcmp(((msg_server_t)left)->service_name,
                ((msg_server_t)right)->service_name) == 0;
}

msg_server_t _get_server_for_servname(const char *service_name) {
  struct msg_server_instance_t search = {.service_name =
                                               strdup(service_name)};
  list_node_t found_node = list_find(&atomic_head, &search, _same_service_name) ;
  free(search.service_name);
  msg_server_t found_server = list_data(found_node);

  return found_server;
}

void msg_server_add_callback(msg_server_t instance,
                             void (*cb)(const char *, void *), void *ctx) {
  if (instance) {
    instance->cb = cb;
    instance->ctx = ctx;
  }
}

msg_server_t _create(const char *service_name) {
  msg_server_t instance = calloc(1, sizeof(*instance));
  if (instance) {
    instance->service_name = strdup(service_name);
    msg_server_error_t error;
    if (_setup_socket(instance->service_name, &error) != MSG_SERVER_SUCCESS) {
      fprintf(stderr, "%s\n", error.message);
      free(instance);
      instance = NULL;
    }
  }
  return instance;
}

msg_server_t _remember(msg_server_t server) {
  list_node_t saved_node = list_insert(&atomic_head, server);
  msg_server_t saved_server = list_data(saved_node);
  return saved_server;
}


msg_server_status_t _setup_socket(const char *service_name,
                               msg_server_error_t *error) {
  // create socket
  int _socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (_socket < 0) {
    msg_server_set_error_errno(error, "socket()");
    return MSG_SERVER_FAILURE;
  }

  // set socket options
  int reuse = 1;
  if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    msg_server_set_error_errno(error, "setsockopt()");
    close(_socket);
    return MSG_SERVER_FAILURE;
  }

  // configure address
  struct addrinfo hints = {.ai_flags = AI_PASSIVE,
                           .ai_family = AF_INET6,
                           .ai_socktype = SOCK_DGRAM,
                           .ai_protocol = IPPROTO_UDP};
  struct addrinfo *server_info;
  int status = getaddrinfo(NULL, service_name, &hints, &server_info);
  if (status != 0) {
    msg_server_set_error_vprintf(error, status, "getaddrinfo(): %s", gai_strerror(status));
    close(_socket);
    freeaddrinfo(server_info);
    return MSG_SERVER_FAILURE;
  }

  // bind the socket
  if (bind(_socket, server_info->ai_addr, server_info->ai_addrlen) < 0) {
    msg_server_set_error_errno(error, "bind()");
    close(_socket);
    freeaddrinfo(server_info);
    return MSG_SERVER_FAILURE;
  }

  freeaddrinfo(server_info);
  server_info = NULL;

  // install kqueue event
  struct kevent change;
  // TODO: In case of dynamic ports we cannot save service_name. It
  // needs to be updated to the dynamically choosen port.
  EV_SET(&change, _socket, EVFILT_READ, EV_ADD, 0, 0, (void *)service_name);
  printf("Adding EVFILT_READ read for %s\n", service_name);
  if (kevent(kq, &change, 1, NULL, 0, NULL) < 0) {
    msg_server_set_error_errno(error, "kevent()");
    close(_socket);
    return MSG_SERVER_FAILURE;
  }

  return MSG_SERVER_SUCCESS;
}
