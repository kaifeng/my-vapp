SOURCES = main.c common/common.c common/debug.c common/unsock.c
SOURCES += common/fd_list.c common/stat.c common/vring.c common/shm.c
SOURCES += vhost_server.c vhost_client.c

HEADERS = include/common.h include/unsock.h
HEADERS += include/fd_list.h include/stat.h include/vring.h include/shm.h
HEADERS += include/vhost_server.h include/vhost_client.h include/vhost_user.h
HEADERS += include/packet.h

BIN=vhost
CFLAGS += -Wall -Werror -Iinclude -I.
CFLAGS += -ggdb3 -O0
LFLAGS = -lrt

all: ${BIN}

${BIN}: ${SOURCES} ${HEADERS}
		${CC} ${CFLAGS} ${SOURCES} -o $@ ${LFLAGS}

clean:
		rm -rf ${BIN}
