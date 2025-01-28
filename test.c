#include "error_handling.h"
#include "msg_server.h"
#include <stddef.h>
#include <stdio.h>

void print(const char *msg, void *ctx) { printf("%s", msg); }

int main(void) {
  msg_server_error_t error;
  int status = msg_server_init(&error);
  if (status != MSG_SERVER_SUCCESS) {
    fprintf(stderr, "Error: %s", error.message);
    free(error.message);
  } else {
    printf("Init successful\n");
  }

  msg_server_t one = msg_server_create(NULL);
  msg_server_t two = msg_server_create("1979");
  msg_server_t two_prime = msg_server_create("1979");
  msg_server_t three = msg_server_create(NULL);

  printf("%-10s %p\n", "one:", one);
  printf("%-10s %p\n", "two:", two);
  printf("%-10s %p\n", "two_prime:", two_prime);
  printf("%-10s %p\n", "three:", three);

  msg_server_add_callback(two, print, NULL);

  getchar();
}
