CC = gcc

INCLUDES = -I.
CFLAGS = -std=c99 -Wall -g $(INCLUDES)
LDFLAGS = -lpthread

SRCS = cec_receiver.c
OBJS = $(SRCS:.c=.o)
TARGET = cec_receiver

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(INCLUDES) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) $(TARGET)
