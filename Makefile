CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
TARGET  = cachedb
SRCDIR  = src
SRCS    = $(SRCDIR)/main.c \
          $(SRCDIR)/server.c \
          $(SRCDIR)/store.c \
          $(SRCDIR)/protocol.c \
          $(SRCDIR)/persistence.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SRCDIR)/*.o $(TARGET)
