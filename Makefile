CC=gcc

all: sxcom

sxcom: sxcom.c log.c signals.c
	$(CC) -I/usr/include/freetype2 -o sxcom sxcom.c signals.c log.c -lX11 -lXcomposite -lXft

clean:
	rm -f sxcom
