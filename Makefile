# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Alternatively, this software may be distributed under the terms of the
# GNU General Public License ("GPL") version 2 as published by the Free
# Software Foundation.

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

test: $(BINARY)
  # library level test
	$(CC) $(CFLAGS) -o lib/lib_vpd_test.o -c lib/lib_vpd_test.c
	$(CC) $(CFLAGS) -o vpd_test lib/lib_vpd.o lib/lib_vpd_test.o
	./vpd_test

	# test cases for vpd executable
	make -C tests

clean:
	rm -f *.o lib/*.o vpd vpd_test
