<!--
Copyright (c) 2022-2023 SAULX

SPDX-License-Identifier: MIT
-->

Selva Protocol
==============


```
+-----------------------+
| selva_proto value     |
+-----------------------+
| selva_proto msg/frame |
+-----------------------+
|           TCP         |
+-----------------------+
|           IP          |
+-----------------------+
```
Protocol stack


```
  +------------------------------+
  | parse selva_proto values     |   lib/util parses
  +------------------------------+
                  |
  +------------------------------+
  |            command           |   module implementing commands
  +------------------------------+
                  |
  +------------------------------+
  | assemble selva_proto message |  server module
  +------------------------------+
                  |
  +------------------------------+
  |            TCP/IP            |
  +------------------------------+
```
Call stack


The Selva Protocol (`selva_proto`) is defined in the
[selva\_proto.h](../include/selva_proto.h) header file.

**Key features of the protocol:**

- Client server protocol over TCP
- All the message headers are sent in LE binary format
- Can multiplex multiple messages simultaneously
- Client initiates a request with a sequence number
- A request/response sequence can be long standing (i.e. streaming or sending event responses)
- Message fragments (frames) are CRC checked
- Supports virtually transparent compression

A new sequence starts with a `struct selva_proto_header` frame header that
encapsulates data. A frame contains `cmd` field that is used to pass the
message data to the correct handler. The first and last frames are marked with
flags. There is no frame order information in the frame headers and the protocol
trusts the lower level protocol (TCP) to deliver the frame in correct order.

The message request or response can contain any data that is understood by the
corresponding command but the intended way is to pack the message using the
provided value header types `SELVA_PROTO_NULL`, `SELVA_PROTO_ERROR`,
`SELVA_PROTO_DOUBLE`, `SELVA_PROTO_LONGLONG`, `SELVA_PROTO_STRING`, and
`SELVA_PROTO_ARRAY`. This allows encoding pretty much any information in an
easily parseable format.

List of Commands
----------------

The message encoding is expected to use the provided `selva_proto` encapsulation
headers unless otherwise specified.

See [commands.h](../commands.h).

selva\_proto\_scanf
-------------------

```c
int selva_proto_scanf(struct finalizer * restrict fin, const void *restrict buf, size_t szbuf, const char * restrict fmt, ...);
```

Parse selva proto buffer in `scanf`-style.
Format specifier: `%[width][.precision][length]specifier`
- `{` and `}` marks an array and `,` advances the buffer index
- Supported specifiers: i, d, u, f, c, s, p
- Length specifiers: hh, h, l, ll, j, z, t, L
- Using width specifier with `%s` changes output from `selva_string` to `char` buffer
Since using `%s` with a `selva_string` structure pointer gives a warning it's
also possible achieve the same result using `%p`.
`width` is used with char buffers to specify the maximum number of bytes to
be copied.
If `precision` is given as `*` with `%s` or `%p` then the string is passed as
a pointer and the length is written to `size_t` pointer argument preceeding the
string pointer, e.g. `selva_proto_scanf(fin, buf, sz, "%.*s", &len, &s)`.

The function returns the number of arguments successfully parsed, which can
be fewer than expected in the format string. However, if the buffer buf is
not fully consumed by the error (`SELVA_PROTO_EINVAL`).

| specifier | Type                      | Description |
|-----------|---------------------------|-------------|
| `i`, `d`  | integer                   |             |
| `u`       | unsigned integer          |             |
| `f`       | floating point number     |             |
| `c`       | character(s) or integer   |             |
| `s`       | string                    |             |
| `p`       | string                    |             |
| `...`     | array of string pointers  | As a last item on the format strings, parses the rest of the arguments into a `NULL`-terminated array of `struct selva_string` pointers. The proto values must be of type `SELVA_PROTO_STRING`. |

| width         | Description |
|---------------|-------------|
| (number)      | Valid with `c` and `s` specifiers. Number of characters copied to the buffer given as an argument. |

| .precision    | Description |
|---------------|-------------|
| .*            | The string `s` is passed as a pointer and the length is written to the preceeding `size_t` argument. |

| **length/specifiers** | `i`, `d`          | `u`                       | `f`               | `c`       | `s`, `p`                          |
|-----------------------|-------------------|---------------------------|-------------------|-----------|-----------------------------------|
| (none)                | `int*`            | `unsigned int*`           | `float*`          | `char*`   | `struct selva_string*` or `char*` |
| `hh`                  | `signed char*`    | `unsigned char*`          |                   |           |                                   |
| `h`                   | `short int*`      | `unsigned short int*`     |                   |           |                                   |
| `l`                   | `long int*`       | `unsigned long int*`      | `double*`         |           |                                   |
| `ll`                  | `long long int*`  | `unsigned long long int*` |                   |           |                                   |
| `j`                   | `intmax_t*`       | `uintmax_t*`              |                   |           |                                   |
| `z`                   | `size_t*`         | `size_t*`                 |                   |           |                                   |
| `t`                   | `ptrdiff_t*`      | `ptrdiff_t*`              |                   |           |                                   |
| `L`                   |                   |                           | `long double*`    |           |                                   |

An empty value signifies an illegal state.
