CC = gcc
CFLAGS = -O2 -Wall -I ..

all: adder subtracter

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

subtracter: subtracter.c
	$(CC) $(CFLAGS) -o subtracter subtracter.c

clean:
	rm -f adder subtracter *~
