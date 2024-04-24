export CC = gcc

export CFLAGS = -pedantic -std=c89 -Werror -Wall

all: master user node

master: master.o master_fun.o
	$(CC) $(CFLAGS) -o master master.o master_fun.o

user : user.o user_fun.o
	$(CC) $(CFLAGS) -o user user.o user_fun.o

node : node.o node_fun.o
	$(CC) $(CFLAGS) -o node node.o node_fun.o

master.o: master.c master.h header.h
	$(CC) $(CFLAGS) -c master.c

master_fun.o: master_fun.c
	$(CC) $(CFLAGS) -c master_fun.c
 
user.o: user.c user.h header.h
	$(CC) $(CFLAGS) -c user.c

user_fun.o: user_fun.c
	$(CC) $(CFLAGS) -c user_fun.c

node.o: node.c node.h header.h
	$(CC) $(CFLAGS) -c node.c

node_fun.o: node_fun.c
	$(CC) $(CFLAGS) -c node_fun.c

run:
	./master 
	
clean:
	rm -f *.o
	rm master user node

