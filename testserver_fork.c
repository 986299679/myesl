#include <esl.h>
#include <stdio.h>
#include <stdlib.h>

static void mycallback(esl_socket_t server_sock, esl_socket_t client_sock,
                       struct sockaddr_in *addr, void *user_data)
{
  esl_handle_t handle = {{0}};
  int done = 0;
  esl_status_t status;
  time_t exp = 0;

  if (fork()) {
    return;
  }

  if (esl_attach_handle(&handle, client_sock, addr) != ESL_SUCCESS) {
    return;
  }

  esl_log(ESL_LOG_INFO, "Connected! %d\n", handle.sock);

  esl_filter(&handle, "unique-id",
             esl_event_get_header(handle.info_event, "caller-unique-id"));
  esl_events(&handle, ESL_EVENT_TYPE_PLAIN,
             "SESSION_HEARTBEAT CHANNEL_ANSWER CHANNEL_ORIGINATE "
             "CHANNEL_PROGRESS CHANNEL_HANGUP "
             "CHANNEL_BRIDGE CHANNEL_UNBRIDGE CHANNEL_OUTGOING CHANNEL_EXECUTE "
             "CHANNEL_EXECUTE_COMPLETE DTMF CUSTOM conference::maintenance");

  esl_send_recv(&handle, "linger");

  esl_execute(&handle, "answer", NULL, NULL);
  esl_execute(&handle, "conference", "3000@default", NULL);

  while ((status = esl_recv_timed(&handle, 1000)) != ESL_FAIL) {
    if (done) {
      if (time(NULL) >= exp) {
        break;
      }
    } else if (status == ESL_SUCCESS) {
      const char *type =
        esl_event_get_header(handle.last_event, "content-type");
      if (type && !strcasecmp(type, "text/disconnect-notice")) {
        const char *dispo =
          esl_event_get_header(handle.last_event, "content-disposition");
        esl_log(ESL_LOG_INFO, "Got a disconnection notice dispostion: [%s]\n",
                dispo ? dispo : "");
        if (dispo && !strcmp(dispo, "linger")) {
          done = 1;
          esl_log(ESL_LOG_INFO,
                  "Waiting 5 seconds for any remaining events.\n");
          exp = time(NULL) + 5;
        }
      }
    }
  }

  esl_log(ESL_LOG_INFO, "Disconnected! %d\n", handle.sock);
  esl_disconnect(&handle);
}

static esl_socket_t server_sock = ESL_SOCK_INVALID;

static void handle_sig(int sig)
{
  shutdown(server_sock, 2);
}

int main(void)
{
  signal(SIGINT, handle_sig);
  signal(SIGCHLD, SIG_IGN);

  esl_global_set_default_logger(7);
  esl_listen("localhost", 8040, mycallback, NULL, &server_sock);

  return 0;
}
