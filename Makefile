# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CC?=cc
VERSION := $(shell ./util/getversion.sh)
CFLAGS+=-g -Iinclude/ -Werror -Wall -D'VPD_VERSION="$(VERSION)"'
LDFLAGS+=-luuid
BINARY=vpd

OBJS=	lib/flashrom.o \
	lib/fmap.o \
	lib/lib_smbios.o \
	lib/lib_vpd.o \
	lib/math.o \
	vpd.o

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) $(CFLAGS) -o vpd $(OBJS) -luuid
	$(CC) $(CFLAGS) -static -o vpd_s $(OBJS) -luuid

test: $(OBJS)
	$(CC) $(CFLAGS) -o lib/lib_vpd_test.o -c lib/lib_vpd_test.c
	$(CC) $(CFLAGS) -o vpd_test lib/lib_vpd.o lib/lib_vpd_test.o
	./vpd_test

clean:
	rm -f *.o lib/*.o vpd vpd_test
