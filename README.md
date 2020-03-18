# Selva

## About

🌴 Selva is a realtime undirected acyclic graph database.

It was built to handle massively scalable data structures with complex hierarchies and taxonomies. Documents or graph vertices can have any number or combination of parents and children. Field values can be augmented with data exiting in other members of its hierarchy allowing for useful defaults based on a document context.  
All of this with real-time updates and transparent subscriptions making it perfect to drive dynamic websites and applications.

Here are some of its features:

  - Real-time engine and subscription model.
  - Versioning system for the data itself allowing branches of data to be merged and revised into the master data, allowing content creators to try changes before date is published to a live system. It allows collaborative version editing in real-time.
  - Schemas enforce data types with builtin validation.
  - Persistence layer using GIT-LFS allowing backups every 5 mins and all versions of the data to available at all times.
  - Custom query language and indexes optimized for the undirected acyclic graph data structure.
  - Simple JSON based query language DSL.
  - Client API uses web sockets to subscribe to data and transparently keep content updated in real-time.

## Documentation

Selva is composed of two main modules - @saulx/selva client and @saulx/selva-server.
Documentation for its API as well as the two main concepts: the schemas and its query DSL can be viewed in the links below.

  - [API](docs/api.md)
  - [Schema definition](docs/schemas.md)
  - [Query language](docs/query.md)

## Usage

llorem

## License

TBD
