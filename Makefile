CPPFLAGS = -g

.PHONY: all build run clean

all: build

build: server subscriber

server: server.c
	gcc $(CPPFLAGS) -o server server.c

subscriber: subscriber.c
	gcc $(CPPFLAGS) -o subscriber subscriber.c


clean:
	rm -f server subscriber