RESP Protocol
=============

Selva extends the Redis RESP serialization protocol, slightly. This file
documents some aspects of the protocol including the extensions.

| Prefix    | Description                                                   |
|-----------|---------------------------------------------------------------|
| `+`       | Simple string with no CR or LF characters.                    |
| `-`       | Error message, same as simple string.                         |
| `:`       | Integer. Signed 64 bit integer stringified into ASCII DEC.    |
| `$<size>` | Bulk String.                                                  |
| `*<size>` | Array.                                                        |
| `@<size>` | Binary blob, a Selva extension.                               |
