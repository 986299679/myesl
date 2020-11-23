#include <esl.h>

#define PORT    8040
#define HOST    "localhost"
#define TIMEOUT 100000

static void t_callback(esl_socket_t server_sock, esl_socket_t client_sock,
                       struct sockaddr_in *addr, void *user_data);

int main(int argc, char *argv[])
{
  esl_global_set_default_logger(ESL_LOG_LEVEL_INFO);
  esl_listen_threaded(HOST, PORT, t_callback, NULL, TIMEOUT);

  return 0;
}

static void t_callback(esl_socket_t server_sock, esl_socket_t client_sock,
                       struct sockaddr_in *addr, void *user_data)
{
  char *type;
  esl_handle_t handle = {{0}};
  esl_event_header_t *header = NULL;
  esl_status_t status = ESL_SUCCESS;

  if (esl_attach_handle(&handle, client_sock, addr) == ESL_FAIL) {
    return;
  }

  printf("Connected\n");

  esl_send_recv(&handle, "linger");

  esl_events(&handle, ESL_EVENT_TYPE_PLAIN, "all");
  esl_execute(&handle, "answer", NULL, NULL);

  /* TODO: How to make it disconnected automatically when channel hangup
   * <20-11-20, GenmZy_> */
  while (1) {
    status = esl_recv_event_timed(&handle, 1000, 0, NULL);
    switch (status) {
    case ESL_SUCCESS:
      printf("\nESL_SUCCESS\n");
      printf("[%d]%s\n", handle.errnum, handle.err);
      /*
       * header = handle.info_event->headers;
       * do {
       *   header = header->next;
       *   printf("%s:%s\n", header->name, header->value);
       * } while (header != handle.info_event->last_header);
       * printf("--bodies--\n%s\n\n", handle.info_event->body);
       * type = esl_event_get_header(handle.last_event, "content-type");
       * if (type == NULL || !strcmp(type, "text/disconnect-notice")) {
       *   goto disconn;
       * }
       *   goto disconn;
       * }
       */
      header = handle.last_event->headers;
      do {
        header = header->next;
        printf("%s:%s\n", header->name, header->value);
      } while (header != handle.last_event->last_header);
      printf("--bodies--\n%s\n\n", handle.last_event->body);
      type = esl_event_get_header(handle.last_event, "content-type");
      if (type == NULL || !strcmp(type, "text/disconnect-notice")) {
        goto disconn;
      }

      break;
    case ESL_FAIL:
      printf("\nESL_FAIL\n");
      printf("[%d]%s\n", handle.errnum, handle.err);
      break;
    case ESL_BREAK:
      printf("\nESL_BREAK\n");
      printf("[%d]%s\n", handle.errnum, handle.err);
      break;
    case ESL_GENERR:
      printf("\nESL_GENERR\n");
      printf("[%d]%s\n", handle.errnum, handle.err);
      break;
    case ESL_DISCONNECTED:
      printf("\nESL_DISCONNECTED\n");
      printf("[%d]%s\n", handle.errnum, handle.err);
      break;
    default:
      printf("\nUnknow\n");
      printf("[%d]%s\n", handle.errnum, handle.err);
      break;
    }
  }

disconn:
  esl_disconnect(&handle);
  printf("Disconnected!!\n");
}
