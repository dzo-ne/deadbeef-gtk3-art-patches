CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -fPIC -D_GNU_SOURCE
LDFLAGS ?= -shared

PREFIX ?= /usr/local
INSTALL_DIR ?= $(PREFIX)/lib/deadbeef
USER_INSTALL_DIR ?= $(HOME)/.local/lib/deadbeef

# Locate DeaDBeeF headers
DEADBEEF_INC ?= $(shell \
    if [ -f /opt/deadbeef/include/deadbeef/deadbeef.h ]; then echo "-I/opt/deadbeef/include"; \
    elif [ -f /usr/local/include/deadbeef/deadbeef.h ]; then echo "-I/usr/local/include"; \
    elif [ -f /usr/include/deadbeef/deadbeef.h ]; then echo "-I/usr/include"; \
    fi)

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS   := $(shell pkg-config --libs gtk+-3.0)

INCLUDES = -I./include $(DEADBEEF_INC) $(GTK_CFLAGS)
LIBS     = $(GTK_LIBS) -lm

TARGET = ddb_custom_medialib.so
SRCS = src/plugin.c src/artwork_cache.c src/medialib_tree.c src/ui.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean install install-user test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) -o tests/test_plugin tests/test_plugin.c $(LIBS) -ldl
	./tests/test_plugin ./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) tests/test_plugin

install: $(TARGET)
	install -d $(DESTDIR)$(INSTALL_DIR)
	install -m 755 $(TARGET) $(DESTDIR)$(INSTALL_DIR)/

install-user: $(TARGET)
	install -d $(USER_INSTALL_DIR)
	install -m 755 $(TARGET) $(USER_INSTALL_DIR)/
