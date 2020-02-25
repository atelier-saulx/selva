# Selva Query DSL Documentation

Selva uses a JSON query DSL to specify the data to be retrieved from the database.

  - [**&lt;any field name&gt;**](#any-field-name)
  - [**$id**](#id)
  - [**$all**](#all)
  - [**$value**](#id)
  - [**$default**](#default)


[Available data types](#available-data-types)

## Field properties

### `<any field name>`: _boolean_, _object_

When truthly, includes the field named as the key. Ovjects can be nested. 

```javascript
const result = await get({
  $id: 'muASxsd3',
  title: true,
  director: true,
  technicalData: {
    runtime: true,
    color: true,
    aspectRatio: true
  }
})
```

### `$id`: _string_

Id of the object to get.  
If omited, the _root_ object is returned. Can only be used at the root of the query object.

```javascript
const result = await get({
  $id: 'muASxsd3'
})
```

### `$all`: _boolean_

Includes all the fields from the parent object 

```javascript
const result = await get({
  $id: 'muASxsd3',
  title: true,
  director: true,
  technicalData: {
    $all: true
  }
})
```

### `$value`: _any_

Overrides the field value.

```javascript
const result = await get({
  $id: 'muASxsd3',
  title: { $value'Amazing movie' }
})
```

### `$default`: _any_

Default value in case the field has no value set.

```javascript
const result = await get({
  $id: 'muASxsd3',
  director: { $default: 'Unknown director' }
})
```

## Avilable field data types

- **string**: `"flurpy"`, `"pants"`  
- **integer**: `123`, `666`
- **float**: `12.234`, `6.6666`
- **timestamp**: `269222400`
- ...
