LIBESL = /usr/local/freeswitch/lib/libesl.a

CFLAGS = -ggdb -pthread -lpthread

all:myesl charge acd

myesl:myesl.c
	gcc $(CFLAGS) -o myesl myesl.c $(LIBESL)

charge:charge.c
	gcc $(CFLAGS) -o charge charge.c $(LIBESL)

acd:acd.c
	gcc $(CFLAGS) -o acd acd.c $(LIBESL)

clean:
	rm -f myesl charge
