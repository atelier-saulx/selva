Selva Protocol
==============

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

`-` means empty message payload.

| Name                      | cmd   | Request payload       | Response payload      | Module | Description                                                      |
|---------------------------|------:|-----------------------|-----------------------|--------|------------------------------------------------------------------|
| PING                      |     0 | `"ping"` or nothing   | `pong`                | server | Traditional ping/pong.                                           |
| ECHO                      |     1 | string(s)             | strings(s)            | server | Echoes back the same strings that were present in the request.   |
| LSCMD                     |     2 | -                     | an array of strings   | server | List all currently registered commands.                          |
| LSLANG                    |     3 | -                     | an array of strings   | db     | List supported languages.                                        |
| resolve.nodeid            |    16 |                       |                       | db     |                                                                  |
| hierarchy.find            |    17 |                       |                       | db     |                                                                  |
| hierarchy.inherit         |    18 |                       |                       | db     |                                                                  |
| hierarchy.aggregate       |    19 |                       |                       | db     | Aggregate.                                                       |
| hierarchy.aggregateIn     |    20 |                       |                       | db     | Aggregate in.                                                    |
| hierarchy.addConstraint   |    21 |                       |                       | db     | edge add constraints.                                            |
| hierarchy.listConstraints |    22 |                       |                       | db     | edge list constraints.                                           |
| hierarchy.del             |    23 |                       |                       | db     |                                                                  |
| hierarchy.heads           |    24 |                       |                       | db     |                                                                  |
| hierarchy.parents         |    25 |                       |                       | db     |                                                                  |
| hierarchy.children        |    26 |                       |                       | db     |                                                                  |
| hierarchy.edgeList        |    27 |                       |                       | db     |                                                                  |
| hierarchy.edgeGet         |    28 |                       |                       | db     |                                                                  |
| hierarchy.edgeGetMetadata |    29 |                       |                       | db     |                                                                  |
| hierarchy.compress        |    30 |                       |                       | db     |                                                                  |
| hierarchy.listCompressed  |    31 |                       |                       | db     |                                                                  |
| hierarchy.ver             |    32 |                       |                       | db     |                                                                  |
| hierarchy.types.add       |    33 |                       |                       | db     |                                                                  |
| hierarchy.types.clear     |    34 |                       |                       | db     |                                                                  |
| hierarchy.types.list      |    35 |                       |                       | db     |                                                                  |
| index.list                |    36 |                       |                       | db     |                                                                  |
| index.new                 |    37 |                       |                       | db     |                                                                  |
| index.del                 |    38 |                       |                       | db     |                                                                  |
| index.debug               |    39 |                       |                       | db     |                                                                  |
| modify                    |    40 |                       |                       | db     |                                                                  |
| rpn.evalBool              |    41 |                       |                       | db     |                                                                  |
| rpn.evalDouble            |    42 |                       |                       | db     |                                                                  |
| rpn.evalString            |    43 |                       |                       | db     |                                                                  |
| rpn.evalSet               |    44 |                       |                       | db     |                                                                  |
| object.del                |    45 |                       |                       | db     |                                                                  |
| object.exists             |    46 |                       |                       | db     |                                                                  |
| object.get                |    47 |                       |                       | db     |                                                                  |
| object.len                |    48 |                       |                       | db     |                                                                  |
| object.set                |    49 |                       |                       | db     |                                                                  |
| object.type               |    50 |                       |                       | db     |                                                                  |
| object.getMeta            |    51 |                       |                       | db     |                                                                  |
| object.setMeta            |    52 |                       |                       | db     |                                                                  |
| subscriptions.add         |    53 |
| subscriptions.addAlias    |    54 |
| subscriptions.addMissing  |    55 |
| subscriptions.addTrigger  |    56 |
| subscriptions.refresh     |    57 |
| subscriptions.list        |    58 |
| subscriptions.listMissing |    59 |
| subscriptions.debug       |    60 |
| subscriptions.del         |    61 |
| subscriptions.delmarker   |    62 |
