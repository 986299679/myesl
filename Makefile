LIBESL = /usr/local/freeswitch/lib/libesl.a

CFLAGS = -ggdb -pthread -lpthread

all:myesl charge

myesl:myesl.c
	gcc $(CFLAGS) -o myesl myesl.c $(LIBESL)

charge:charge.c
	gcc $(CFLAGS) -o charge charge.c $(LIBESL)

clean:
	rm -f myesl charge
