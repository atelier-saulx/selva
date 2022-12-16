<!--
Copyright (c) 2022 SAULX

SPDX-License-Identifier: MIT
-->

IO
==

Selva binary dump serialization format (.sdb).

```
   | 00 01 02 03 04 05 06 07
===+=========================+
00 | 53 45 4C 56 41 00 00 00 | Magic string "SELVA\0\0\0"
   |-------------------------|
08 | 00 00 00 00 00 00 00 00 | Created with version hash
10 | 00 00 00 00 00 00 00 00 | 40 bytes
18 | 00 00 00 00 00 00 00 00 | human-readable
20 | 00 00 00 00 00 00 00 00 |
28 | 00 00 00 00 00 00 00 00 |
   |-------------------------|
30 | 00 00 00 00 00 00 00 00 | Updated with version hash
38 | 00 00 00 00 00 00 00 00 | 40 bytes
40 | 00 00 00 00 00 00 00 00 | human-readable
48 | 00 00 00 00 00 00 00 00 |
50 | 00 00 00 00 00 00 00 00 |
   |-------------------------|
58 | 00 00 00 00 00 00 00 00 | unused spare/pad
   |=========================|
60 |                         | Data stored as selva_proto structs:
   |          UDATA          | - selva_proto_double
   |          UDATA          | - selva_proto_longlong (signed and unsigned)
   |                         | - selva_proto_string (char* and selva_string)
   |=========================|
   | 00 00 00 41 56 4C 45 53 | Magic string "\0\0\0AVLES"
   |-------------------------|
   | XX XX XX XX XX XX XX XX | SHA-3 of the file
   | XX XX XX XX XX XX XX XX | from 0 to the last magic string.
   | XX XX XX XX XX XX XX XX | binary
   | XX XX XX XX XX XX XX XX | 
```
