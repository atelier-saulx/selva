# _Set_ Method Query Reference

  - [**$id**](#id-string)
  - [**$alias**](#alias-string---string)
  - [**$merge**](#merge-boolean)
  - [**$language**](#language-string)
  - [**operation**](#operation-string)
  - [**&lt;any field name&gt;**](#any-field-name)

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

## `$merge`: _boolean_

Default value: `true`

The `$merge` operator can be used to specify whether any fields specified in the `.set()` should overwrite everything that exists in the database for that record currently (if it is updated), or whether any fields specified will be added or overwritten only if provided.


```javascript
/*
Let's assume the following record in database:
{
  id: 'maASxsd3',
  type: 'match',
  value: 10,
  title: {
    en: 'yes'
  }
}
*/ 

const result = await client.set({
  $merge: true, // optional, defaults to true
  type: 'match',
  title: {
    en: 'hello',
    de: 'hallo'
  },
  name: 'match'
})

/*
Value of `const result`: `maASxsd3`
Resulting record in database:
{

  id: 'maASxsd3',
  type: 'match',
  value: 10, // value remains
  title: {
    en: 'hello', // .en is overwritten
    de: 'hallo' // .de is merged in
  },
  name: 'match' // name is merged in
}
*/

/* With $merge: false */

/*
Let's assume the following record in database:
{
  id: 'maASxsd3',
  type: 'match',
  value: 10,
  title: {
    en: 'yes'
  }
}
*/ 

const result = await client.set({
  $merge: false,
  title: {
    de: 'hallo'
  },
  name: 'match'
})

/*
Value of `const result`: `maASxsd3`
Resulting record in database:
{
  id: 'maASxsd3',
  type: 'match',
  title: {
    de: 'hallo' // .de is added but .en is deleted
  },
  name: 'match' // name is added but value is deleted
}

*/
```

## `$language` : _string_

See [here](#text-type) for more details in the field type documentation.

## Any field name

TODO: add reference to schema docs
Any and all field names can be set that exist in the schema of the provided type of record. Some operators exist that are specific to the type of field being set. Accepted values and operators for each field type are outlined below.

  - [**digest**](#digest-type)
  - [**timestamp**](#timestamp-type)
  - [**url**](#url-type)
  - [**email**](#email-type)
  - [**phone**](#phone-type)
  - [**type**](#type-type)
  - [**string**](#string-type)
  - [**int**](#int-type)
  - [**float**](#float-type)
  - [**number**](#number-type)
  - [**boolean**](#boolean-type)
  - [**text**](#text-type)
  - [**array**](#array-type)
  - [**json**](#json-type)
  - [**geo**](#geo-type)
  - [**set**](#set-type)
  - [**references**](#references-type)
  - [**object**](#object-type)

### _type_ type

The type property is normally only provided when creating a new record with the following syntax:

```javascript
const result = await client.set({
  type: 'match',
  title: {
    en: 'hello'
  }
})
```

If type is specified in other cases, it must always match the type of the existing record, as well as the schema prefix of that type. (TODO: link to schema docs). Type can never be overwritten for existing records.

### _digest_ type

Only values of type `string` are accepted for the _digest_ type. Any value provided will be automatically passed through a SHA256 hashing algorithm. The digest is stored in the database instead of the actual passed string value.

```javascript
const result = await client.set({
  $id: 'usASxsd3',
  type: 'user',
  password: 'top_secret_password' // field with type 'digest'
})

/*
Value of `const result`: `usASxsd3`
Resulting record in database:
{
  id: 'usASxsd3',
  type: 'user',
  password: '4bc838fb5a7160b6433be6c4e188ed1fee0cc08337789bd5d6c77994ad6b50c8' // using default secret 'selva-client'
}
*/

TODO: basic set operators
```

### _timestamp_ type

Field type _timestamp_ accepts positive numeric values after the epoch (in milliseconds). In addition, it also accept one string value: `'now'`. When `'now'` is provided, it is automatically converted to the current time in milliseconds since epoch.

TODO: basic set operators

### _url_ type

The _url_ field type accepts any `string` values that are valid URLs.

TODO: basic set operators

### _email_ type

The _email_ field type accepts any `string` values that are valid email addresses.

TODO: basic set operators

### _phone_ type

The _phone_ field type accepts any `string` values that are valid phone numbers.

TODO: basic set operators

### _string_ type

The _string_ type accepts any `string` values.

TODO: basic set operators

### _int_ type

The _int_ type accepts any `number` values that are representable as an integer.

TODO: basic set operators

### _float_ type

The _float_ type accepts any `number` values that are representable as floating point numbers.

TODO: basic set operators

### _number_ type

The _number_ type accepts any `number` values.

TODO: basic set operators

### _boolean_ type

The _boolean_ type accepts a `boolean` value, `true` or `false`.

TODO: basic set operators

### _text_ type

The _text_ type accepts depends on whether the top level operator `$language` has been set.

If `$langauge is set`, _text_ fields only accept string values, and the value is set in the language field of the _text_ entry corresponding to the language.

```javascript
const result = await client.set({
  $language: 'en',
  $id: 'maASxsd3',
  type: 'match',
  title: 'the super bowl 2020' 
})
```

If the `$language` operator is not present, the _text_ type field accepts an object value with properties set to any languages that are present in the schema.

```javascript
const result = await client.set({
  $language: 'en',
  $id: 'maASxsd3',
  type: 'match',
  title: {
    en: 'the super bowl 2020' ,
    de: 'das super bowl 2020'
  }
})
```

No operators exist for _text_.

### _array_ type

Fields with _array_ type accept javascript array values where all entries correspond to the item type specified in the schema. 

No operators exist for _array_.

### _json_ type

The _json_ type allows any javascript values, unless `properties` has been defined for it in the schema. In that case, an `object` type value must be provided that contains only values that contain properties set in that schema definition.

No operators exist for field of the _json_ type.

### _geo_ type

Fields with type _geo_ must always be set with an object where all of the following properties are set:
```javascript
const result = await client.set({
  $id: 'maASxsd3',
  type: 'match',
  geoField: {
    lat: 60, // latitude, has to be a number (required field)
    lon: 0.2, // longitude, has to be a number (required field)
  }
})
```

No operators exist for field of the _geo_ type.

### _set_ type
### _references_ type
### _object_ type

## Search

TODO
