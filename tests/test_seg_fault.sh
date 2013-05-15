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

clean_up() {
  rm -rf $TMP_DIR
}

die() {
  echo $1
  exit 1
}

BINARY="../vpd"
TMP_DIR=$(mktemp -d)
BIOS_PACK="empty.vpd.tbz"
BIOS="$TMP_DIR/empty.vpd"

# $1: the tbz file
# $2: target directory
unpack_bios() {
  tar -jxf $1 -C $2
}

unpack_bios $BIOS_PACK $TMP_DIR

#
# Test -s= (empty key) case.
# Expect return 1 for invalid parameter.
$BINARY -f $BIOS -s=  &> /dev/null && rc=$? || rc=$?
if [[ $rc != 1 ]]; then
  exit $rc
fi

#
# Test -s=DEF (empty key) case.
# Expect return 1 for invalid parameter.
$BINARY -f $BIOS -s=DEF  &> /dev/null && rc=$? || rc=$?
if [[ $rc != 1 ]]; then
  exit $rc
fi

#
# Test -sABC= (empty value) case.
# Expect SUCCESS
$BINARY -f $BIOS -sABC=  &> /dev/null && rc=$? || rc=$?
if [[ $rc != 0 ]]; then
  exit $rc
fi
# Make sure ABC has been set
$BINARY -f $BIOS -l | grep ABC  &> /dev/null && rc=$? || rc=$?
if [[ $rc != 0 ]]; then
  exit $rc
fi

#
# Test -sABC=DEF case.
# Expect SUCCESS
$BINARY -f $BIOS -sABC=DEF  &> /dev/null && rc=$? || rc=$?
if [[ $rc != 0 ]]; then
  exit $rc
fi
# Make sure ABC has been set
$BINARY -f $BIOS -l | grep DEF  &> /dev/null && rc=$? || rc=$?
if [[ $rc != 0 ]]; then
  exit $rc
fi

#
# Test -d DEF case.
# expect return 1 because non-existed key
$BINARY -f $BIOS -d DEF  &> /dev/null && rc=$? || rc=$?
if [[ $rc != 1 ]]; then
  exit $rc
fi

#
# Test -d ABC case.
# expect SUCCESS and nothing left.
$BINARY -f $BIOS -d ABC  &> /dev/null && rc=$? || rc=$?
if [[ $rc != 0 ]]; then
  exit $rc
fi
$BINARY -f $BIOS -l | grep -e ".*"  &> /dev/null && rc=$? || rc=$?
if [[ $rc != 1 ]]; then
  exit $rc
fi


clean_up

exit 0
