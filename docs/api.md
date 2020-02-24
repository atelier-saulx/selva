# Selva API Documentation

  - Server
    - **[start()](#start)**
    - **[_server_.destrioy()](#serverdestroy)**
    - **[_server_.backup()](#serverbackup)**
  - Client
    - **[connect()](#connect)**
    - **[set()](#set)**
    - **[get()](#get)**

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
    <td valign="top">&lt;optional&gt;</td>
    <td>
      Server options  
      <br>Properties
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
            Server port number to use
          </td>
        </tr>
        <tr>
          <td valign="top"><code>service</code></td>
          <td valign="top">string</td>
          <td valign="top">&lt;optional&gt;</td>
          <td valign="top">
            Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam ut efficitur nunc, vitae facilisis arcu. Donec faucibus nibh ex, non aliquet mi rhoncus finibus. Suspendisse dapibus, odio nec aliquet molestie, nunc nisi vulputate odio, eget porttitor erat tellus eu nunc. Phasellus venenatis massa id velit elementum, quis bibendum dolor lobortis. Quisque rutrum arcu quam, et scelerisque mauris iaculis a. Donec vehicula sem vel congue pharetra.
          </td>
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
    <td valign="top">object</td>
    <td valign="top">&lt;optional&gt;</td>
    <td>
      Connection options  
      <br>Properties
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
            Port to connect to.
          </td>
        </tr>
        <tr>
          <td valign="top"><code>option2</code></td>
          <td valign="top">string</td>
          <td valign="top">&lt;optional&gt;</td>
          <td valign="top">
            Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam ut efficitur nunc, vitae facilisis arcu. Donec faucibus nibh ex, non aliquet mi rhoncus finibus. Suspendisse dapibus, odio nec aliquet molestie, nunc nisi vulputate odio, eget porttitor erat tellus eu nunc. Phasellus venenatis massa id velit elementum, quis bibendum dolor lobortis. Quisque rutrum arcu quam, et scelerisque mauris iaculis a. Donec vehicula sem vel congue pharetra.
          </td>
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
      <br>Properties:
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
