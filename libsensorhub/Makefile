CC= kwwrap -o /tmp/1.trace gcc
CFLAGS=-shared -fPIC

libsensorhub.so: src/lib/libsensorhub.c src/utils/utils.c
	$(CC) $(CFLAGS) -o libsensorhub.so src/lib/libsensorhub.c src/utils/utils.c -Werror

sensorhubd: src/daemon/main.c src/utils/utils.c
	$(CC) -o sensorhubd src/daemon/main.c src/utils/utils.c -lpthread -Werror

