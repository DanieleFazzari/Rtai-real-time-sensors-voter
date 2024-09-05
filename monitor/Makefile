prefix := $(shell rtai-config --prefix)

ifeq ($(prefix),)
$(error Please add <rtai-install>/bin to your PATH variable)
endif

CC = $(shell rtai-config --cc)
LXRT_CFLAGS = $(shell rtai-config --lxrt-cflags)
LXRT_LDFLAGS = $(shell rtai-config --lxrt-ldflags)

all: rt_monitor

rt_monitor: monitor.c parameters.h
	$(CC) $(LXRT_CFLAGS) -lpthread -o $@ $< $(LXRT_LDFLAGS)

clean:
	rm -f *.o rt_monitor

.PHONY: clean