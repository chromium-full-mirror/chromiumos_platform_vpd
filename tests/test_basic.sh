#!/bin/sh
#
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
set -e

. ./functions.sh

BINARY="../vpd"
TMP_DIR=$(mktemp -d)
BIOS_PACK="empty.vpd.tbz"
BIOS="$TMP_DIR/empty.vpd"

unpack_bios $BIOS_PACK $TMP_DIR

#
# List only
# Expect SUCCESS and the mtime is NOT changed
org_mtime=$(mtime $BIOS)
org_chksum=$(chksum $BIOS)
sleep 2  # ensure the system has changed.
RUN 0 "$BINARY -f $BIOS -l"
new_mtime=$(mtime $BIOS)
new_chksum=$(chksum $BIOS)
EXPECT_EQ "$org_mtime" "$new_mtime" \
          "The file mtime has been modified unexpectedly."
EXPECT_EQ "$org_chksum" "$new_chksum" \
          "The file content has been modified unexpectedly."

#
# -O to reset all
# expect SUCCESS and nothing exists
RUN 0 "$BINARY -f $BIOS -O"
RUN 1 "$BINARY -f $BIOS -l | grep -e '.*'"

#
# Add one string
# Expect SUCCESS
RUN 0 "$BINARY -f $BIOS -s ABC=DEF"
# Make sure ABC has been added
RUN 0 "$BINARY -f $BIOS -l | grep ABC | grep DEF"
# Check format, expect the key string is in front of value string
RUN 0 "$BINARY -f $BIOS -l | grep -r 'ABC.*DEF'"

#
# Test replacement
RUN 0 "$BINARY -f $BIOS -s ABC=123"
# Make sure ABC has been replaced
RUN 0 "$BINARY -f $BIOS -l | grep ABC | grep 123"

#
# Test -d NONE
# expect return 1 because non-existed key
RUN 1 "$BINARY -f $BIOS -d NONE"

#
# Delete single.
# expect SUCCESS and nothing left.
RUN 0 "$BINARY -f $BIOS -d ABC"
RUN 1 "$BINARY -f $BIOS -l | grep -e '.*'"

#
# Test -O and -s at the same command
# expect SUCCESS and new key has been added
RUN 0 "$BINARY -f $BIOS -O -s 5566=183"
RUN 0 "$BINARY -f $BIOS -l | grep 5566 | grep 183"

#
# -O to reset all
# expect SUCCESS and nothing exists
RUN 0 "$BINARY -f $BIOS -O"
RUN 1 "$BINARY -f $BIOS -l | grep -e '.*'"

clean_up

exit 0
