#!/bin/sh

gcc gen_crc32c_table.c -o gen_crc32c_table && ./gen_crc32c_table > crc32c_table.h
rm ./gen_crc32c_table
