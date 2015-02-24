# Makefile
#******************************************************************************#
CFLAGS=-O2 -g
LDFLAGS=-g
#==============================================================================#
ifeq ($(filter-out undefined default,$(origin CC)),)
CC=gcc
endif
#------------------------------------------------------------------------------#
NCLOGIN_CFLAGS=-std=c99 -Wall -Werror -Wextra -Wno-unused-parameter -fno-common
NCLOGIN_LDFLAGS=-Wl,--as-needed
NCLOGIN_LIBS=-lform -lncurses -lcrypt
#- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - #
NCLOGIN_SOURCES=main.c form.c auth.c ctty.c utmp.c user.c util.c
#------------------------------------------------------------------------------#
.PHONY: all clean
.SUFFIXES:
#- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - #
all: nclogin

nclogin: $(NCLOGIN_SOURCES:.c=.o)
	$(CC) $(LDFLAGS) $(NCLOGIN_LDFLAGS) $^ -o $@ $(NCLOGIN_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(NCLOGIN_CFLAGS) -c -o $@ -MMD $<

clean:
	rm -f nclogin *.o *.d
#------------------------------------------------------------------------------#
-include $(NCLOGIN_SOURCES:.c=.d)
#==============================================================================#
