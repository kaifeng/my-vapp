SRC_COMMON = common/common.c \
             common/debug.c \
			 common/unsock.c \
			 common/fd_list.c \
			 common/stat.c \
			 common/vring.c \
			 common/shm.c

SOURCES = main.c common/common.c common/debug.c common/unsock.c
SOURCES += common/fd_list.c common/stat.c common/vring.c common/shm.c
SOURCES += vhost_server.c vhost_client.c

HEADERS = include/common.h include/unsock.h
HEADERS += include/fd_list.h include/stat.h include/vring.h include/shm.h
HEADERS += include/vhost_server.h include/vhost_client.h include/vhost_user.h
HEADERS += include/packet.h

CFLAGS += -Wall -Werror -Iinclude -I.
CFLAGS += -ggdb3 -O0
LFLAGS = -lrt

SRC_VHOST_SERVER = ${SRC_COMMON} vhost_server.c run_server.c
SRC_VHOST_CLIENT = ${SRC_COMMON} vhost_client.c run_client.c

all: vhost run_server run_client

vhost: ${SOURCES} ${HEADERS}
		${CC} ${CFLAGS} ${SOURCES} -o $@ ${LFLAGS}

run_server: ${SRC_VHOST_SERVER} ${HEADERS}
		${CC} ${CFLAGS} ${SRC_VHOST_SERVER} -o $@ ${LFLAGS}

run_client: ${SRC_VHOST_CLIENT} ${HEADERS}
		${CC} ${CFLAGS} ${SRC_VHOST_CLIENT} -o $@ ${LFLAGS}

clean:
		rm -rf vhost run_server run_client
