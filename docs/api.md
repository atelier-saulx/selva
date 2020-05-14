# Selva API Documentation

  - Server
    - **[start()](#startoptions)**
    - **[_server_.destroy()](#serverdestroy)**
    - **[_server_.backup()](#serverbackup)**
  - Client
    - **[connect()](#connectoptions)**
    - **[set()](#clientsetpayload)**
    - **[get()](#clientgetquery)**
    - **[observe()](#clientobservequery)**
    - **[id()](#clientidoptions)**
    - **[getSchema()](#clientgetschema)**
    - **[updateSchema()](#clientupdateschemaschema)**
    - **[getTypeFromId()](#clientgettypefromidid)**
    - **[delete()](#clientdeleteid)**

## Server

### start(_options_)

Launches a Selva server instance.

#### Parameters

<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Type</th>
      <th>Attributes</th>
      <th>Description</th>
    </tr>
  </thead>
  <tr>
    <td valign="top"><code>options</code></td>
    <td valign="top">object</td>
    <td valign="top"></td>
    <td>
      Server options.<br>
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Type</th>
            <th>Attributes</th>
            <th>Description</th>
          </tr>
        </thead>
        <tr>
          <td valign="top"><code>port</code></td>
          <td valign="top">number, Promise&lt;number&gt;</td>
          <td valign="top"></td>
          <td valign="top">
            Server port number to use
          </td>
        </tr>
        <tr>
          <td valign="top"><code>service</code></td>
          <td valign="top">object, Promise&lt;object&gt;</td>
          <td valign="top">optional</td>
          <td valign="top">
            Alternative notation for host and port.
            <pre lang="javascript">{ port: 1234, host: 'localhost' }</pre>
          </td>
        </tr>
        <tr>
          <td valign="top"><code>modules</code></td>
          <td valign="top">string[]</td>
          <td valign="top">optional</td>
          <td valign="top">Redis modules to load</td>
        </tr>
        <tr>
          <td valign="top"><code>verbode</code></td>
          <td valign="top">boolean</td>
          <td valign="top">optional</td>
          <td valign="top">Log debugging information.</td>
        </tr>
        <tr>
          <td valign="top"><code>backups</code></td>
          <td valign="top">object</td>
          <td valign="top">optional</td>
          <td valign="top">?</td>
        </tr>
        <tr>
          <td valign="top"><code>subscriptions</code></td>
          <td valign="top">boolean</td>
          <td valign="top">optional</td>
          <td valign="top">Use subscriptions. Defaults to `true`.</td>
        </tr>
      </table>
    </td>
  </tr>
</table>

#### Returns

Promise resolving to the server instance.

#### Example

```javascript
const server = await start({
  port: 123456
})
```

### _server_.destroy()

Stops the server and cleans up.

#### Returns

Promise representing successful shutdown.

## Client

### connect(_options_)

Connects to a Selva server.
It automatically reconnects to the server in case of lost connection.
Queries are batched automatically and queued in case of a disconnectet.
Results are automatically cached and optimized.

#### Parameters

<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Type</th>
      <th>Attributes</th>
      <th>Description</th>
    </tr>
  </thead>
  <tr>
    <td valign="top"><code>options</code></td>
    <td valign="top">object, Promise&lt;object&gt;</td>
    <td valign="top"></td>
    <td>
      Connection options.
      <br>
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Type</th>
            <th>Attributes</th>
            <th>Description</th>
          </tr>
        </thead>
        <tr>
          <td valign="top"><code>port</code></td>
          <td valign="top">number</td>
          <td valign="top"></td>
          <td valign="top">
            Server port to connect to.
          </td>
        </tr>
        <tr>
          <td valign="top"><code>host</code></td>
          <td valign="top">string</td>
          <td valign="top">optional</td>
          <td valign="top">
            Server host to connect to.
          </td>
        </tr>
        <tr>
          <td valign="top"><code>retryStrategy</code></td>
          <td valign="top">string</td>
          <td valign="top">optional</td>
          <td valign="top">(?)</td>
        </tr>
      </table>
    </td>
  </tr>
</table>


#### Example

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

### _client_.set(_payload_)

Set an document/object on an id. Will deep merge objects by default.

Default behaviours:

- Acenstors can never be set, children and parents update ancestors, children and parents
- Date is always added by default
- Keyword 'now' in date, start, end will add date

#### Parameters

<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Type</th>
      <th>Attributes</th>
      <th>Description</th>
    </tr>
  </thead>
  <tr>
    <td valign="top"><code>payload</code></td>
    <td valign="top">object</td>
    <td valign="top"></td>
    <td>
      Data to store in the database.  
      <br>
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Type</th>
            <th>Attributes</th>
            <th>Description</th>
          </tr>
        </thead>
        <tr>
          <td valign="top"><code>&lt;any&nbsp;key&gt;</code></td>
          <td valign="top">&lt;any&gt;</td>
          <td valign="top"></td>
          <td valign="top">
            Property key and value to set based on the existing schema.
          </td>
        </tr>
        <tr>
          <td valign="top"><code>$id</code></td>
          <td valign="top">string</td>
          <td valign="top">optional</td>
          <td valign="top">
            Id of the record (?) to update
          </td>
        </tr>
        <tr>
          <td valign="top"><code>$merge</code></td>
          <td valign="top">boolean</td>
          <td valign="top">optional</td>
          <td valign="top">
            Merge object with existing data. Defaults to true (?)
          </td>
        </tr>
        <tr>
          <td valign="top"><code>$language</code></td>
          <td valign="top">string</td>
          <td valign="top">optional</td>
          <td valign="top">
            Limits returned data to a specific _language_.
          </td>
        </tr>
        <tr>
          <td valign="top"><code>$version</code></td>
          <td valign="top">string</td>
          <td valign="top">optional, _not implemented yet_</td>
          <td valign="top">
            Limits returned data to a specific _version_.
          </td>
        </tr>
      </table>
    </td>
  </tr>
</table>

#### Returns

Promise resolving to the _id_ of the updated or created record.

#### Examples

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

### _client_.get(_query_)

Retrieves data from the database.

#### Parameters

<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Type</th>
      <th>Attributes</th>
      <th>Description</th>
    </tr>
  </thead>
  <tr>
    <td valign="top"><code>query</code></td>
    <td valign="top">object</td>
    <td valign="top"></td>
    <td>
      <a href="./query.md">Query</a> to be executed.
    </td>
  </tr>
</table>

#### Returns

Promise resolving to the fetched data.

### _client_.observe(_query_)

Executes the query and subscribes to future changes. The return value is an _Observable_ instance with a `.subscribe()` function for starting to listen on data changes. The returned `Subscription` instance can then be `.unsubscribe()`'ed.

#### Parameters

<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Type</th>
      <th>Attributes</th>
      <th>Description</th>
    </tr>
  </thead>
  <tr>
    <td valign="top"><code>query</code></td>
    <td valign="top">object</td>
    <td valign="top"></td>
    <td>
      <a href="./query.md">Query</a> to be executed.
    </td>
  </tr>
</table>

#### Returns

_Observable_ that can be subscribed to for changes. The first event is always the current state of the query.

#### Examples

```js
const sub = await client.observe({
  $id: 'myId',
  $language: 'en',
  $title: true
}).subscribe(d => {
  console.log('result data', d)
})

setTimeout(() => {
  sub.unsubscribe()
}, 10000)
```

### _client_.id(_options_)

Generates a random id for a document of a type.

#### Parameters

<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Type</th>
      <th>Attributes</th>
      <th>Description</th>
    </tr>
  </thead>
  <tr>
    <td valign="top"><code>options</code></td>
    <td valign="top">object</td>
    <td valign="top"></td>
    <td>
      <br>
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Type</th>
            <th>Attributes</th>
            <th>Description</th>
          </tr>
        </thead>
        <tr>
          <td valign="top"><code>type</code></td>
          <td valign="top">string</td>
          <td valign="top"></td>
          <td valign="top">
            Type of the document.
          </td>
        </tr>
        <tr>
          <td valign="top"><code>externalId</code></td>
          <td valign="top">string</td>
          <td valign="top">optional</td>
          <td valign="top">
            Alias for the document id (?)
          </td>
        </tr>
      </table>
    </td>
  </tr>
</table>

#### Returns

Promise resolving to the generated id.

#### Examples

```javascript
const id = await client.id({ type: 'flurpy', externalId: 'smurkysmurk' })
// flgurk
```


### _client_.getSchema()

Gets the database schema.

#### Returns

Promise resolving to the [schema](schemas.md) currently in use.

### _client_.updateSchema(_schema_)

Updates the database [schema](schemas.md)

#### Parameters

<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Type</th>
      <th>Attributes</th>
      <th>Description</th>
    </tr>
  </thead>
  <tr>
    <td valign="top"><code>schema</code></td>
    <td valign="top">object</td>
    <td valign="top"></td>
    <td>
      [Schema](schemas.md) changes. The new schema is merged with the existing one.<br>Schema fields cannot be removed.
    </td>
  </tr>
  <tr>
    <td valign="top"><code>retry</code></td>
    <td valign="top">number</td>
    <td valign="top">optional</td>
    <td>Times to retry. Defaults to `0`</td>
  </tr>
</table>

#### Returns

Promise resolving to an observable (?? needs better definition)

### _client_.getTypeFromId(_id_)

Gets the type of a document.

#### Parameters

<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Type</th>
      <th>Attributes</th>
      <th>Description</th>
    </tr>
  </thead>
  <tr>
    <td valign="top"><code>id</code></td>
    <td valign="top">string</td>
    <td valign="top"></td>
    <td>Id of the document.</td>
  </tr>
</table>

#### Returns

Promise resolving to the type of the document.

### _client_.delete(_id_)

Deletes a document.

#### Parameters

<table>
  <thead>
    <tr>
      <th>Name</th>
      <th>Type</th>
      <th>Attributes</th>
      <th>Description</th>
    </tr>
  </thead>
  <tr>
    <td valign="top"><code>id</code></td>
    <td valign="top">string, object</td>
    <td valign="top"></td>
    <td>
      Id of the document to delete or an object which includes the following properties:
      <br>
      <table>
        <thead>
          <tr>
            <th>Name</th>
            <th>Type</th>
            <th>Attributes</th>
            <th>Description</th>
          </tr>
        </thead>
        <tr>
          <td valign="top"><code>$id</code></td>
          <td valign="top">string</td>
          <td valign="top">optional</td>
          <td valign="top">
            Id of the document to delete.
          </td>
        </tr>
        <tr>
          <td valign="top"><code>$hierarchy</code></td>
          <td valign="top">boolean</td>
          <td valign="top">optional</td>
          <td valign="top">
            Defaults to false. Also removes the document children, if any, and removes it's reference from any parents.
          </td>
        </tr>
      </table>
    </td>
  </tr>
</table>

#### Returns

Promise resolving to the type of the document.

#### Examples

```javascript
await client.delete('ma12231')
await client.delete({ $id: 'ma12231' })
await client.delete({ $id: 'ma12231', $hierarchy: false })
```
