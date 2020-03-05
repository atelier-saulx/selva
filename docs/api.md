# Selva API Documentation

  - Server
    - **[start()](#start)**
    - **[_server_.destrioy()](#serverdestroy)**
    - **[_server_.backup()](#serverbackup)**
  - Client
    - **[connect()](#connect)**
    - **[set()](#set)**
    - **[get()](#get)**
    - **[observe()](#observe)**
    - **[getSchema()](#getschema)**
    - **[updateSchema()](#updateschema)**
    - **[getTypeFromId()](#gettypefromid)**
    - **[delete()](#delete)**

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

Connects to a server

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

### set(_payload_)

Sets data in the database.

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

#### Example

```javascript
await client.set({
  $id: 'clA',
  title: {
    en: 'nice!'
  },
  description: {
    en: 'yesh'
  },
  image: {
    thumb: 'thumb',
    poster: 'poster'
  }
})
```

### get(_query_)

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
      [Query](query.md) to be executed.
    </td>
  </tr>
</table>

#### Returns

Promise resolving to the fetched data.

### observe(_query_)

Retrieves and sets a subscription to data.

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
      [Query](query.md) to be executed.
    </td>
  </tr>
</table>

#### Returns

Promise resolving to an observable (?? needs better definition)

### getSchema()

Gets the database schema.

#### Returns

Promise resolving to the [schema](schemas.md) currently in use.

### updateSchema(_schema_)

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

### getTypeFromId(_id_)

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

### delete(_id_)

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
