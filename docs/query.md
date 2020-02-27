# Selva Query DSL Documentation

Selva uses a JSON query DSL to specify the data to be retrieved from the database.

  - [**&lt;any field name&gt;**](#any-field-name)
  - [**$id**](#id)
  - [**$all**](#all)
  - [**$value**](#id)
  - [**$default**](#default)


[Available data types](#available-data-types)

## Clauses

### `<any field name>`: _boolean_, _object_

When truthy, includes the named field in the result.  
Objects can be nested, and other clauses used, to shape the exact format needed in the result.

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

Includes all the fields for the obect at the level where the clause is used.  
[References](./glossary.md#references) are not included. (?)

```javascript
const result = await get({
  $id: 'peASxsd3',
  $all: true
})
```

Fields can be excluded if a false is set to specific fields.
```javascript
const result = await get({
  $id: 'peASxsd3',
  $all: true,
  died: false
})
```
[See test](../client/test/examples/clauses/all.ts)

### `$value`: _any_

Overrides the current value of the field.

```javascript
const result = await get({
  $id: 'moASxsd3',
  title: { $value'Amazing movie' }
})
```
[See test](../client/test/examples/clauses/value.ts)

### `$default`: _any_

Default value to be returned in case the field has no value set.

```javascript
const result = await get({
  $id: 'moASxsd3',
  director: { $default: 'Unknown director' }
})
```
[See test](../client/test/examples/clauses/default.ts)

### `$language`: _string_

Filters document data to a set language on fields that supoprt it.

```javascript
const result = await client.get({
  $language: 'en',
  $id: 'mo2001ASpaceOdyssey',
  title: true
})
```
[See test](../client/test/examples/clauses/language.ts)

## Avilable field data types

- **string**: `"flurpy"`, `"pants"`  
- **integer**: `123`, `666`
- **float**: `12.234`, `6.6666`
- **timestamp**: `269222400`
- ...
