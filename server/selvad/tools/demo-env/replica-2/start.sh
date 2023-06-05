#!/bin/sh
# Copyright (c) 2023 SAULX
#
# SPDX-License-Identifier: MIT
LOCPATH=../../../binaries/Linux_x86_64/locale SELVA_PORT=3002 SERVER_SO_REUSE=1 SELVA_REPLICATION_MODE=2 exec ../../../selvad
