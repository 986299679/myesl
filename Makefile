LIBESL = /usr/local/freeswitch/lib/libesl.a

CFLAGS = -ggdb -Wall
LDFLAGS = -pthread -lpthread
TARGET_DIR = target

PROGS =	charge acd myesl t_esl testclient testserver testserver_fork\
				t_esl_recv_event_timed

LDLIBS+=$(LIBESL)

all:$(PROGS)

%:	%.c $(LIBAPUE)
	$(CC) $(CFLAGS) $@.c -o $(TARGET_DIR)/$@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET_DIR)/$(PROGS) $(TEMPFILES) *.o
