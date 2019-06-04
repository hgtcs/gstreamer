TARGET=v4l2-enc-appsink
CC=gcc
SRC=$(TARGET).c
OBJ=$(patsubst %.c,%.o,$(SRC))

PKG_CONFIG_PATH += -I/usr/include/gstreamer-1.0 -I/usr/lib/x86_64-linux-gnu/gstreamer-1.0/include

CFLAGS=-g

CFLAGS  += `pkg-config --cflags gstreamer-1.0 glib-2.0 gstreamer-app-1.0`
LDFLAGS += `pkg-config --libs gstreamer-1.0 glib-2.0 gstreamer-app-1.0`

all: $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $^ $(LDFLAGS)
	rm $(OBJ)

clean:
	rm $(TARGET)
