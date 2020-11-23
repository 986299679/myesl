/* FIXME: You should `sudo apt install festival` first for
 * text2wave
 *
 * TODO: Next time, i install something like `sudo apt install
 * libttspico-utils` for pico2text
 *
 * TODO: Finally, i install some like
 * (`/mnt/e/balcon/balcon.exe -l` to show all voices)
 * `echo ${text} | /mnt/e/balcon/balcon.exe -n Microsoft Zira
 * Desktop -w ${file} -t` this can only be used in wsl...
 */
#include <esl.h>
#include <stdio.h>
#include <stdlib.h>

#define HOST    "localhost"
#define PORT    8040
#define MAXCALL 10000
/*Business Marcos*/
#define BLANCE       100
#define CHARGE       100
#define ERROR_PROMPT " 'say:无效的输入 ' "

#define set_string(dest, str) strncpy(dest, str, sizeof(dest) - 1);

#define ENSURE_INPUT(ch, input)                                                \
  do {                                                                         \
    if (!input) {                                                              \
      esl_execute(ch->handle, "speak", "再见", NULL);                          \
      esl_execute(ch->handle, "handup", NULL, NULL);                           \
      return;                                                                  \
    }                                                                          \
  } while (0)

typedef enum charge_menu_s {
  CHARGE_MENU_NONE,
  CHARGE_MENU_QUERY,
  CHARGE_MENU_CHARGE
} charge_menu_t;

typedef enum charge_state {
  CHARGE_WELCOME,
  CHARGE_MENU,
  CHARGE_WAIT_ACCOUNT,
  CHARGE_WAIT_ACCOUNT_PASSWORD,
  CHARGE_WAIT_CARD,
  CHARGE_WAIT_CONFIRM
} charge_state_t;

typedef struct charge_helper_s {
  esl_handle_t *handle;
  int balance;
  charge_state_t state;
  charge_menu_t menu;
  char account[20];
  char card[20];
} charge_helper_t;

/* Function headers {{{ */
static void charge_callback(esl_socket_t server_sock,
                            esl_socket_t client_socket,
                            struct sockaddr_in *addr, void *user_data);

static char *get_digits(esl_event_t *event);

static void event_callback(charge_helper_t *helper);

static int check_account_password(char *account, char *password);

static int check_account_card(char *account, char *card);

static int do_charge(int balance, int charge);
/* }}} Function headers */

int main(int argc, char *argv[])
{
  esl_global_set_default_logger(ESL_LOG_LEVEL_DEBUG);
  esl_listen_threaded(HOST, PORT, charge_callback, NULL, MAXCALL);

  return 0;
}

static void charge_callback(esl_socket_t server_sock,
                            esl_socket_t client_socket,
                            struct sockaddr_in *addr, void *user_data)
{
  /* TODO: DO SOMETHING <22-10-20, GenmZy_> */
  esl_handle_t handle = {{0}};
  esl_status_t status;
  charge_helper_t charge_helper = {0};

  charge_helper.handle = &handle;
  charge_helper.balance = BLANCE;
  charge_helper.state = CHARGE_WELCOME;

  esl_attach_handle(&handle, client_socket, addr);
  esl_log(ESL_LOG_INFO, "Connected! %d\n", handle.sock);
  esl_events(&handle, ESL_EVENT_TYPE_PLAIN, "CHANNEL_EXECUTE_COMPLETE");
  esl_filter(&handle, "Unique-ID",
             esl_event_get_header(handle.info_event, "Caller-Unique-ID"));
  esl_send_recv(&handle, "linger 5");
  esl_log(ESL_LOG_INFO, "%s\n", handle.last_sr_reply);

  esl_execute(&handle, "answer", NULL, NULL);
  esl_execute(&handle, "set", "tts_engine=tts_commandline", NULL);
  esl_execute(&handle, "set", "tts_voice=TingTing", NULL);
  esl_execute(&handle, "speak", "您好,欢迎拨打韵达账号充值查询系统", NULL);

  while (ESL_SUCCESS == (status = esl_recv(&handle))) {
    const char *type = esl_event_get_header(handle.last_event, "content-type");
    if (type && !strcasecmp(type, "text/event-plain")) {
      event_callback(&charge_helper);
    }
  }

  esl_log(ESL_LOG_INFO, "Disconnected! %d\n", handle.sock);
  esl_disconnect(&handle);
}

static void event_callback(charge_helper_t *ch)
{
  char *application = NULL;
  esl_event_t *event = ch->handle->last_ievent;

  esl_log(ESL_LOG_INFO, "event_id: %d\n", event->event_id);

  if (event->event_id != ESL_EVENT_CHANNEL_EXECUTE_COMPLETE) {
    return;
  }

  application = esl_event_get_header(event, "Application");
  esl_log(ESL_LOG_INFO, "State: %d App: %s\n", ch->state, application);

  switch (ch->state) {
  case CHARGE_WELCOME:
    if (!strcmp(application, "speak")) {
    select_menu:
      ch->state = CHARGE_MENU;
      esl_execute(ch->handle, "play_and_get_digits",
                  "1 1 3 5000 # "
                  " 'say: 账户查询请按1,充值请按2' " ERROR_PROMPT
                  " digits ^\\d$",
                  NULL);
    }
    break;
  case CHARGE_MENU:
    if (!strcmp(application, "play_and_get_digits")) {
      char *menu = get_digits(event);
      ENSURE_INPUT(ch, menu);
      if (!strcmp(menu, "1")) {
        ch->menu = CHARGE_MENU_QUERY;
      } else if (!strcmp(menu, "2")) {
        ch->menu = CHARGE_MENU_CHARGE;
      }

      ch->state = CHARGE_WAIT_ACCOUNT;
      esl_execute(ch->handle, "play_and_get_digits",
                  "4 5 3 5000 # "
                  " 'say: 请输入您的账号，并按井号键结束' " ERROR_PROMPT
                  " digits ^\\d{4}$",
                  NULL);
    }
    break;
  case CHARGE_WAIT_ACCOUNT:
    if (!strcmp(application, "play_and_get_digits")) {
      char *account = get_digits(event);
      ENSURE_INPUT(ch, account);
      set_string(ch->account, account);

      if (ch->menu == CHARGE_MENU_QUERY) {
        ch->state = CHARGE_WAIT_ACCOUNT_PASSWORD;
        esl_execute(ch->handle, "play_and_get_digits",
                    "4 5 3 5000 # "
                    " 'say:请输入您的密码,并按井号键结束' " ERROR_PROMPT
                    " digits ^\\d{4}$",
                    NULL);
      } else {
        ch->state = CHARGE_WELCOME;
        esl_execute(ch->handle, "speak", "账号不正确，请重新输入", NULL);
      }
    }
  case CHARGE_WAIT_ACCOUNT_PASSWORD:
    if (!strcmp(application, "play_and_get_digits")) {
      char *password = get_digits(event);
      ENSURE_INPUT(ch, password);

      if (check_account_password(ch->account, password)) {
        char buffer[1024];
        sprintf(buffer, "您的当前账户剩余余额为%d美元", ch->balance);
        ch->state = CHARGE_WELCOME;
        esl_execute(ch->handle, "speak", buffer, NULL);
      } else {
        ch->state = CHARGE_WELCOME;
        esl_execute(ch->handle, "speak", "密码输入不正确，请重新输入", NULL);
      }
    }
    break;
  case CHARGE_WAIT_CARD:
    if (!strcmp(application, "play_and_get_digits")) {
      char *card = get_digits(event);
      ENSURE_INPUT(ch, card);

      if (check_account_card(ch->account, card)) {
        char buffer[1024];
        sprintf(buffer, "You want to charge for %d dollars", CHARGE);
        esl_execute(ch->handle, "speak", buffer, NULL);
        ch->state = CHARGE_WAIT_CONFIRM;
        esl_execute(ch->handle, "play_and_get_digits",
                    "1 1 3 5000 # "
                    " 'say:确认请按1，返回上一层请按2' " ERROR_PROMPT
                    " digits ^\\d$",
                    NULL);
      } else {
        ch->state = CHARGE_WELCOME;
        esl_execute(ch->handle, "speak", "无效的输入,请重新输入", NULL);
      }
    }
    break;
  case CHARGE_WAIT_CONFIRM:
    if (!strcmp(application, "play_and_get_digits")) {
      char *confirm = get_digits(event);
      ENSURE_INPUT(ch, confirm);
      if (!strcmp(confirm, "1")) {
        char buffer[1024];
        ch->balance = do_charge(ch->balance, CHARGE);
        sprintf(buffer,
                "充值成功，您充值%d美元,当前余额%d美元"
                "感谢您的来电，再见",
                CHARGE, ch->balance);
        ch->state = CHARGE_WELCOME;
        esl_execute(ch->handle, "speak", buffer, NULL);
      } else if (!strcmp(confirm, "2")) {
        ch->state = CHARGE_WELCOME;
        goto select_menu;
      }
    }
    break;
  default:
    break;
  }
}

static char *get_digits(esl_event_t *event)
{
  char *digits = esl_event_get_header(event, "variable_digits");
  if (digits) {
    esl_log(ESL_LOG_INFO, "digits: %s\n", digits);
  }

  return digits;
}

static int check_account_password(char *account, char *password)
{
  return (!strcmp(account, "1111") && (!strcmp(password, "1111")));
}

static int check_account_card(char *account, char *card)
{
  return (!strcmp(account, "1111") && (!strcmp(card, "2222")));
}

static int do_charge(int balance, int charge)
{
  return balance + charge;
}
