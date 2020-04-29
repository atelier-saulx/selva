# _Set_ Method Query Reference

  - [**$id**](#id-string)
  - [**$alias**](#alias-string---string)
  - [**operation**](#operation-string)
  - [**&lt;any field name&gt;**](#any-field-name-boolean-object)

## `$id`: _string_ 

Id of the object to set. If set, it has precedence over `$alias`. Used to look up the record to be operated on. By default (_upsert_) the record is created if nothing exists with the provided `$id`. This behaviour can be changed by providing [`$operation`](#oepration-string).

## `$alias`: _string_  | _string[]_

When no `$id` is provided, `$alias` can be used to reference user provided alternative names to records, such as human readable identifiers or url paths and the like. 

```javascript
const result = await client.set({
  $alias: '/hello',
  type: 'match',
  $id: 'muASxsd3',
  title: {
    en: 'hello'
  }
})
```

If `type` is provided, such as it is above, if the provided `$alias` option doesn't resolve to an existing id a new record will be created, and the provided `$alias` created for it automatically. This behaviour can be controlled with `$operation` also.

One can provide a list of aliases instead of a single alias. In this case all the aliases will be checked before failure or creating a new record if `type` is provided and the `$operation` allows for it.


```javascript
const result = await client.set({
  $alias: ['/hello', '/hey', '/hi`],
  type: 'match',
  $id: 'muASxsd3',
  title: {
    en: 'hello'
  }
})
```

If a new record is created, these aliases are created for it automatically.

NOTE: if the record does exist already, the missing aliases are not created.

To add new aliases to existing records:
TODO: add link to `references` type set operations / syntax

## `$operation` : _string_

Operation has to be one of _upsert_, _update_, _create_.

### Upsert

Upsert mode is the default set operation type. It updates an existing record or creates a new one if no record exists.

```javascript
const result = await client.set({
  $operation: 'upsert', // optional to provide $operation
  $id: 'muASxsd3',
  title: {
    en: 'hello'
  }
})
```

Upsert acts both as _create_ and _update_.

### Create

Create mode fails if a record already exists and returns undefined instead of the id of the created entry. If no entry exists with the specified `$id` or `$alias`.


```javascript
const result = await client.set({
  $operation: 'create',
  $id: 'maASxsd3',
  title: {
    en: 'hello'
  }
})

/*
If no record exists, value of `const result`: `maASxsd3`
If the record already exists, value of `const result`: `undefined`. In this case nothing is set in the database and the record remains as it was.
*/
```

The same applies to `$alias`, if does not resolve nothing is done.

```javascript
const result = await client.set({
  $operation: 'create',
  $alias: 'myAlias',
  title: {
    en: 'hello'
  }
})

/*
If no record exists, value of `const result`: `maASxsd3`
If the record already exists, value of `const result`: `undefined`. In this case nothing is set in the database and the record remains as it was.
*/
```

If neither `$id` nor `$alias` is provided but `type` is provided, a completely new record is created and and id is generated for it.

```javascript
const result = await client.set({
  $operation: 'create',
  type: 'match',
  title: {
    en: 'hello'
  }
})

/*
Value of `const result`: ma<random string> such as `maASxsd3`
Resulting record in database:
{
  id: 'maASxsd3',
  type: 'match',
  title: {
    en: 'hello'
  }
}
*/
```

### Update

Update mode is the opposite of _create_, in that it fails if the record being updated does not exist. The API signature is the same both for `$id` and `$alias`, and if neither is provided the operation fails.


```javascript
let result = await client.set({
  $operation: 'create',
  type: 'match',
  title: {
    en: 'hello'
  }
})

/*
Value of `const result`: `undefined`
Record in the database remains untouched
*/

result = await client.set({
  $operation: 'create',
  $id: 'maASxsd3',
  title: {
    en: 'hello'
  }
})

/*
If the record exists, value of `const result`: `maASxsd3`
If the record does not exist, value of `const result`: `undefined`. In this case nothing is set in the database and the record remains as it was.
*/
```
