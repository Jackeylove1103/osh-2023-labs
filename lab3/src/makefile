CC = gcc
CFLAGS = -pthread
SERVER = server

all: $(SERVER)

$(SERVER): server.c
	$(CC) $(CFLAGS) -o $@ server.c

clean:
	rm -f $(SERVER)
