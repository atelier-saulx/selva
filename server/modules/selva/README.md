# Selva Redis Module

## Quick Start

Here's what you need to do to build your first module:

1. `make`
2. Run redis loading the module: `redis-server --loadmodule ./module.so`

## Documentation

### Redis

| File                                   | Description                                                                                                                               |
| -------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| [API.md](doc/redis/API.md)             | The official manual for writing Redis modules, copied from the Redis repo. Read this before starting, as it's more than an API reference. |
| [FUNCTIONS.md](doc/redis/FUNCTIONS.md) | Generated API reference documentation for both the Redis module API, and LibRMUtil.                                                       |
| [TYPES.md](doc/redis/TYPES.md)         | Describes the API for creating new data structures inside Redis modules, copied from the Redis repo.                                      |
| [BLOCK.md](doc/redis/BLOCK.md)         | Describes the API for blocking a client while performing asynchronous tasks on a separate thread.                                         |

## Degugging

See [debugging.md](doc/debugging.md)
