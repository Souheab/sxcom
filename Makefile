CC=gcc

all: sxcom

sxcom: sxcom.c
	$(CC) -I/usr/include/freetype2 -o sxcom sxcom.c -lX11 -lXcomposite -lXdamage -lXrender -lXext -Wall -Wextra

clean:
	rm -f sxcom
