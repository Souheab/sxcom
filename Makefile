CC=gcc
CFLAGS=-I/usr/include/freetype2 -o sxcom sxcom.c -lX11 -lXcomposite -lXdamage -lXrender -lXfixes -lXext -Wall -Wextra

all: sxcom

sxcom: sxcom.c
	$(CC) $(CFLAGS)

debug: CFLAGS += -DDEBUG -g
debug: sxcom

clean:
	rm -f sxcom
