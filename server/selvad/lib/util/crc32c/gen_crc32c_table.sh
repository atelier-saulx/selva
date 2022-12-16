#!/bin/sh
# Copyright (C) 2013, 2021 Mark Adler <madler@alumni.caltech.edu>
# SPDX-License-Identifier: Zlib

gcc gen_crc32c_table.c -o gen_crc32c_table && ./gen_crc32c_table > crc32c_table.h
rm ./gen_crc32c_table
