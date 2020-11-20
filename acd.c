#include <esl.h>

#define HOST                  "localhost"
#define PORT                  8040
#define MAXCALL               10000
#define MAX_AGENTS            3
#define set_string(dest, str) strncpy(dest, str, sizeof(dest) - 1);

typedef enum agent_status_em {
  AGENT_IDLE,
  AGENT_BUSY,
  AGENT_FAIL
} agent_status_t;

typedef struct agent_st {
  char exten[10];
  char uuid[37];
  agent_status_t state;
} agent_t;

static esl_mutex_t *mutex;
static agent_t agents[MAX_AGENTS];
static int last_agent_index = MAX_AGENTS - 1;

/* Function headers {{{ */
static void init_agents(void);

static void acd_mycallback(esl_socket_t server_sock, esl_socket_t client_sock,
                           struct sockaddr_in *addr, void *user_data);

static agent_t *find_available_agent(void);

static void reset_agent(agent_t *agent);
/* }}} Function headers */

int main(int argc, char *argv[])
{
  esl_mutex_create(&mutex);
  init_agents();

  esl_global_set_default_logger(ESL_LOG_LEVEL_INFO);
  esl_log(ESL_LOG_INFO, "ACD Server listening at localhost:8040 ... \n");
  esl_listen_threaded(HOST, PORT, acd_mycallback, NULL, MAXCALL);

  esl_mutex_destroy(&mutex);

  return 0;
}

static void acd_mycallback(esl_socket_t server_sock, esl_socket_t client_sock,
                           struct sockaddr_in *addr, void *user_data)
{
  esl_handle_t handle = {{0}};
  esl_status_t status = ESL_SUCCESS;
  agent_t *agent = NULL;
  const char *caller_name;
  const char *caller_num;

  esl_attach_handle(&handle, client_sock, addr);

  caller_name =
    esl_event_get_header(handle.info_event, "Caller-Caller-ID-Name");
  caller_num =
    esl_event_get_header(handle.info_event, "Caller-Caller-ID-Number");
  esl_log(ESL_LOG_INFO, "New Call from \"%s\" <%s>\n", caller_name,
          caller_name);

  esl_send_recv(&handle, "myevents");
  esl_log(ESL_LOG_INFO, "%s\n", handle.last_sr_event);
  esl_send_recv(&handle, "linger 5");
  esl_log(ESL_LOG_INFO, "%s\n", handle.last_sr_reply);
  esl_execute(&handle, "answer", NULL, NULL);
  esl_execute(&handle, "set", "tts_engine=tts_commandline", NULL);
  esl_execute(&handle, "set", "tts_voice=Zira", NULL);
  esl_execute(&handle, "set", "continue_on_fail=true", NULL);
  esl_execute(&handle, "set", "hangup_after_bridge=true", NULL);
  esl_execute(&handle, "speak",
              "Hello, welcome to call to Yunda,"
              " this telphone is tranfering to agents, please hold on",
              NULL);
  sleep(5);
  esl_execute(&handle, "playback", "local_stream://moh", NULL);

  while (ESL_SUCCESS == status || ESL_BREAK == status) {
    const char *type;
    const char *application;
    /*
     * esl_recv_timed a event driver function, we receive a event,
     * put it into `handle` and now handle.last_event is the event
     * that just received.
     */
    status = esl_recv_timed(&handle, 1000);

    if (ESL_BREAK == status) {
      if (!agent) {
        agent = find_available_agent();

        if (agent) {
          char dial_string[1024];
          sprintf(dial_string, "user/%s", agent->exten);
          esl_execute(&handle, "break", NULL, NULL);
          esl_execute(&handle, "bridge", dial_string, NULL);
          esl_log(ESL_LOG_INFO, "Calling: %s\n", dial_string);
        }
      }
      continue;
    }

    type = esl_event_get_header(handle.last_event, "content-type");
    if (type && !strcmp(type, "text/event-plain")) {
      esl_event_get_header(handle.last_ievent, "Event-Name");

      switch (handle.last_ievent->event_id) {
      case ESL_EVENT_CHANNEL_BRIDGE:
        set_string(agent->uuid, esl_event_get_header(handle.last_ievent,
                                                     "Other-Leg-Unique-ID"));
        esl_log(ESL_LOG_INFO, "bridge to %s\n", agent->exten);
        break;
      case ESL_EVENT_CHANNEL_HANGUP_COMPLETE:
        esl_log(ESL_LOG_INFO, "Caller \"%s\" <%s> Hangup \n", caller_name,
                caller_num);
        if (agent) {
          reset_agent(agent);
        }
        goto end;
      case ESL_EVENT_CHANNEL_EXECUTE_COMPLETE:
        application = esl_event_get_header(handle.last_ievent, "Application");
        if (!strcmp(application, "bridge")) {
          const char *disposition = esl_event_get_header(
            handle.last_ievent, "variable_originate_disposition");
          esl_log(ESL_LOG_INFO, "Disposition: %s\n", disposition);
          if (!strcmp(disposition, "CALL_REJECTED") ||
              !strcmp(disposition, "USER_BUSY")) {
            reset_agent(agent);
            agent = NULL;
          }
        }
        break;
      default:
        break;
      }
    }
  }

end:
  esl_log(ESL_LOG_INFO, "Disconnected! status = %d\n", status);
  esl_disconnect(&handle);
}

void init_agents(void)
{
  set_string(agents[0].exten, "1000");
  set_string(agents[1].exten, "1001");
  set_string(agents[2].exten, "1002");
}

static agent_t *find_available_agent(void)
{
  int last;
  agent_t *agent;

  esl_mutex_lock(mutex);
  last = last_agent_index;

  while (1) {
    if (last_agent_index >= MAX_AGENTS - 1) {
      last_agent_index = 0;
    } else {
      last_agent_index++;
    }

    agent = &agents[last_agent_index];

    esl_log(ESL_LOG_INFO, "Comparing agent [%d:%s:%s]\n", last_agent_index,
            agent->exten, agent->state == AGENT_IDLE ? "IDLE" : "BUSY");

    if (agent->state == AGENT_IDLE) {
      agent->state = AGENT_BUSY;
      esl_mutex_unlock(mutex);
      return agent;
    }

    if (last_agent_index == last) {
      break;
    }
  }

  esl_mutex_unlock(mutex);
  return NULL;
}

static void reset_agent(agent_t *agent)
{
  esl_mutex_lock(mutex);
  agent->state = AGENT_IDLE;
  *agent->uuid = '\0';
  esl_mutex_unlock(mutex);
}
