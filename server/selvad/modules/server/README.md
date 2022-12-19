Selva Proto Server
==================

Receiving `selva_proto` messages.

```
Calls Data
        .
  |    / \      server.c           Registers on_data() cb and calls commands
  |     |       server_message.c   Receives and parses sequences/messages
 \ /    |       server_frame.c     Receives sequence frames
  `     |       tcp.c/h            Receives data from the TCP socket
-- kernel -------------------------
``` 
