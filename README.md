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

```js
const result = await client.set({
  $id: 'myId',
  $shallow: true, // defaults to false
  $version: 'mySpecialversion', // optional
  id: 'myNewId',
  foo: true
})
```

```js
const result = await client.set({
  $id: 'myId',
  $shallow: true, // defaults to false
  $version: 'mySpecialversion', // optional
  myThing: {
    title: 'blurf',
    // description: {
    //   $ifNotExists: true,
    //   $val: 'blurf'
    // },
    nestedCount: {
      // $default: 100,
      $inc: { $value: 1, $default: 100 }
    }
  }
})
```

```js
const result = await client.inc({
  $id: 'myId',
  $shallow: true, // defaults to false
  $version: 'mySpecialversion', // optional
  myCount: 1
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
haha: true
```

hkeys: foo.\*

{
foo: true
}

### client.get()

```js
const result = await client.get({})
// nice stuffz
```

### client.inc()

Same as .set() but will increment numbers instead.

```js
const result = await client.inc(
  {
    id: 'myId',
    version: 'mySpecialversion' // optional
  },
  {
    myCounter: 1,
    myBigCounter: 10,
    myNegativeCounter: -20
  }
)
```

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

## notes

how to do setShallow?
how to do SimpleClient vs Client?

## doodles

```js
db.get(
  {
    id: 'root'
  },
  {
    items: [
      {
        $query: {
          $from: ['region'],
          $ancestors: ['volleyball1', 'football1', '$id']
        }
      }
    ]
  }
)

db.get({
  $id: 'football',
  title: true,
  image: true,
  children: [
    {
      children: [
        {
          children: [{}]
        }
      ]
    }
  ]
})

{
  id: ['id1', 'id2'],
  version: 'myStaging'
}



db.get({
  $scope: 'root',
  $type: 'match',
  $filter: 'match',
  title: true,
  image: true
})
```

```js
```

### layouts

```js
```

{
layout: {

}
}

$item
$field

returnSubscription
