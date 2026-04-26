# Makefile
#******************************************************************************#
# Copyright 2026 Sergei Zhirikov <sfzhi@yahoo.com>                             #
# This file is a part of "nclogin" (http://github.com/sfzhi/nclogin).          #
# It is available under GPLv3 (http://www.gnu.org/licenses/gpl-3.0.txt).       #
#==============================================================================#
CFLAGS=-O2 -std=c99 -Wall -Werror -Wextra
LDFLAGS=-s -Wl,--as-needed
#==============================================================================#
ifeq ($(filter-out undefined default,$(origin CC)),)
CC=gcc
endif
#------------------------------------------------------------------------------#
.PHONY: all clean
.SUFFIXES:
#- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - #
all: vtreset

vtreset: vtreset.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f vtreset
#==============================================================================#
