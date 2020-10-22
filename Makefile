LIBESL = /usr/local/freeswitch/lib/libesl.a

CFLAGS = -ggdb -pthread -lpthread
all:myesl.c
	gcc $(CFLAGS) -o myesl myesl.c $(LIBESL)
