# Selva Client

🌴 Selva is a realtime undirected acyclic graph database.

- Real time
- Versioning system for the data itself allowing branches of data to be merged and revised into the master data, allows content creators to try stuff before it goes live and allows them to collaborate on a version (in real time)
- Persistence layer using GIT-LFS allowing backups every 5 mins and keeps versions of all data available
- Custom query language and indexes optimized for the undirected acyclic graph data structure
- Simple query language

## Methods

### selva.connect(options)

Takes an object with options

- reconnects
- batches commands
- queues commands if disconnected
- \*optimized/cached results

example

```js
import { getService } from 'registry'

const client = selva.connect(() => getService('name-of-db'))

// client.redis.hget()

client.set('myId', { myShine: true }).then(result => console.log(result)) // logs OK
```

```js
const client = selva.connect({
  port: 8080,
  host: 'whatever', // defaults to localhost
  retryStrategy() {
    // optional
    return 5e3
  }
})
```

or with a promise

```js
const client = selva.connect(new Promise(resolve => {
  resolve({
    port: 8080,
    host: 'whatever',
    retryStrategy () {
      return 5e3
    }
  })
})
```

or with a (async) function

```js
const client = selva.connect(async () => {
  await doSomething()
  return {
    port: 8080,
    host: 'whatever',
    retryStrategy() {
      return 5e3
    }
  }
})
```

On every reconnect selva will call the given function. This allows you to change the configuration (eg. if a db has become unresponsive).

### client.set()

Set an object on an id. Will deep merge objects by default.

Default behaviours

- Acenstors can never be set, children and parents update ancestors, children and parents.
- Date is allways added by default
- Keyword 'now' in date, start, end will add date
- Setting a batch with adding new stuff is tricky
  - Try to set - if batch - error 'does' not exist - wait 150ms (?) order the non working results. if it does not work return error

```js
await client.set({
  $id: 'myId',
  $merge: false, // defaults to true
  $version: 'mySpecialversion', // optional
  id: 'myNewId',
  foo: true
})
```

```js
await client.set({
  $id: 'myId',
  $merge: false, // defaults to true
  $version: 'mySpecialversion', // optional
  id: 'myNewId',
  foo: true,
  children: {
    $add: 'smukytown',
    $delete: 'myblarf'
  }
})
```

```js
await client.set({
  $id: 'myId',
  children: {
    // maybe redis SET do it?
    // ---- :(
    $hierarchy: false, // defaults to true
    $add: 'smukytown',
    $delete: ['myblarf', 'xxx']
  }
})
```

```js
await client.set({
  $id: 'myId',
  children: {
    // ---- :(
    $hierarchy: false, // defaults to true
    $value: ['root']
  }
})
```

```js
await client.set({
  // gen id, add to root
  type: 'tag',
  title: 'flowers',
  externalId: 'myflower.de'
  }
})
```

```js
await client.set({
  // gen id, add to root
  type: 'tag',
  title: 'flowers',
  externalId: {
    $merge: false,
    $value: 'myflower.de'
  }
})
```

```js
await client.set({
  type: 'tag',
  title: { de: 'blümen' }
})
```

```js
await client.set({
  $id: 'myId',
  $merge: false, // defaults to true
  $version: 'mySpecialversion', // optional
  myThing: {
    title: 'blurf',
    nestedCount: {
      $default: 100,
      $inc: { $value: 1 }
    },
    access: {
      $default: {
        flurpiepants: 'my pants'
      }
    }
  }
})
```

```js
myId
myId#mySpecialversion
```

```js
const result = await client.get(
  {
    $id: 'myId',
    $version: 'mySpecialversion'
  }
)

const versioned = redisClient.get('myId#mySpecialversion')
const original = redisClient.get('myId') || {}
const result = { ...original, ...versioned }

myId
myId#mySpecialversion
```

```js
const obj = {
  foo: {
    bar: true
  },
  haha: true
}
```

```js
'foo.bar': true
'foo.foo': true
'foo.baz': true
'foo.baz.blarf': true,
haha: true
```

hkeys: foo.\*

{
foo: true
}

### client.subscribe()

```js
const result = await client.subscribe(
  {
    id: 'myId',
    version: 'mySpecialversion' // optional
  },
  (id, msg) => {
    console.log(`Fired for ${id} with message: ${msg}`)
  }
)
```

### client.subscribe()

```js
const result = await client.subscribe(
  {
    id: 'myId',
    date: 123123123,
    version: 'mySpecialversion' // optional
  },
  (id, msg) => {
    console.log(`Fired for ${id} with message: ${msg}`)
  }
)
```

or with an array for ids

```js
const result = await client.subscribe(
  {
    id: ['myId', 'myOtherId'],
    version: 'mySpecialversion' // optional
  },
  (id, msg) => {
    console.log(`Fired for ${id} with message: ${msg}`)
  }
)
```

### client.unsubscribe()

```js
const result = await client.unsubscribe(
  {
    id: 'myId',
    version: 'mySpecialversion' // optional
  },
  myCallback // if omitted will remove all listeners
)
```

### id

Generate an id

Max types 1764!

```javascript
const id = await client.id({ type: 'flurpy', externalId: 'smurkysmurk' })
// flgurk
```

### delete

```javascript
await client.delete('ma12231')
await client.delete({ $id: 'ma12231' })
await client.delete({ $id: 'ma12231', $hierarchy: false })
```
