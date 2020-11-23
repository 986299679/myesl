#include <esl.h>
#include <stdlib.h>

#define PORT        8040
#define HOST        "localhost"
#define TIMEOUT     100000
#define LINGER_WAIT 3
#define LINGER_CMD  "linger 3"

static void t_callback(esl_socket_t server_sock, esl_socket_t client_sock,
                       struct sockaddr_in *addr, void *user_data);

int main(int argc, char *argv[])
{
  esl_global_set_default_logger(ESL_LOG_LEVEL_DEBUG);
  esl_listen_threaded(HOST, PORT, t_callback, NULL, TIMEOUT);

  return 0;
}

static void t_callback(esl_socket_t server_sock, esl_socket_t client_sock,
                       struct sockaddr_in *addr, void *user_data)
{
  char *type;
  int done = 0; // For wait linger for 5 sec
  time_t exped = 0;
  const char *uuid;
  esl_handle_t handle = {{0}};
  esl_status_t status = ESL_SUCCESS;

  if (esl_attach_handle(&handle, client_sock, addr) == ESL_FAIL) {
    return;
  }

  printf("Connected\n");

  esl_send_recv(&handle, "linger");

  esl_events(&handle, ESL_EVENT_TYPE_PLAIN, "all");
  esl_execute(&handle, "answer", NULL, NULL);
  esl_execute(&handle, "playback", "local_stream://moh", NULL);
  esl_send_recv_timed(&handle, LINGER_CMD, 1000);

  if ((uuid = esl_event_get_header(handle.info_event, "unique-id")) != NULL) {
    printf("Here comes a uuid %s\n", uuid);
  }

  while ((status = esl_recv_event_timed(&handle, 1000, 0, NULL)) != ESL_FAIL) {
    if (done && exped <= time(NULL)) {
      break;
    }

    if (ESL_SUCCESS == status) {
      if ((type = esl_event_get_header(handle.last_event, "content-type")) !=
          NULL && !strcmp(type, "text/disconnect-notice")) {
        const char *disp = esl_event_get_header(handle.last_event, "Content-Disposition");
        if (!strcmp(disp, "linger")) {
          done = 1;
          exped = time(NULL) + LINGER_WAIT;
          printf("Waiting for %d seconds for linger\n", LINGER_WAIT);
        }
      }
    }
  }

  esl_disconnect(&handle);
  printf("Disconnected!!\n");
}
