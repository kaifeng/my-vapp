SOURCES=main.c common.c debug.c unsock.c
SOURCES+=fd_list.c stat.c vring.c
SOURCES+=server.c vhost_server.c
SOURCES+=client.c vhost_client.c

HEADERS=common.h unsock.h
HEADERS+=fd_list.h stat.h vring.h
HEADERS+=server.h vhost_server.h
HEADERS+=client.h vhost_client.h
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
