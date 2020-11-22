LIBESL = /usr/local/freeswitch/lib/libesl.a

CFLAGS = -ggdb -Wall
LDFLAGS = -pthread -lpthread

PROGS =	charge acd myesl t_esl testclient testserver testserver_fork

LDLIBS+=$(LIBESL)

all:$(PROGS)

%:	%.c $(LIBAPUE)
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(PROGS) $(TEMPFILES) *.o
