SOURCES=main.c common.c debug.c unsock.c
SOURCES+=fd_list.c stat.c vring.c shm.c
SOURCES+=vhost_server.c vhost_client.c

HEADERS=common.h unsock.h
HEADERS+=fd_list.h stat.h vring.h shm.h
HEADERS+=vhost_server.h vhost_client.h
HEADERS+=packet.h

BIN=vhost
CFLAGS += -Wall -Werror
CFLAGS += -ggdb3 -O0
LFLAGS = -lrt

all: ${BIN}

${BIN}: ${SOURCES} ${HEADERS}
		${CC} ${CFLAGS} ${SOURCES} -o $@ ${LFLAGS}

clean:
		rm -rf ${BIN}
