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

To add new aliases to existing records add to or set the `aliases` default field which is a _set_ field of `string` items. For more information see the [_set_](#set-type) documentation.

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

For more information, please refer to the [schema documentation](./schemas.md).

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

If type is specified in other cases, it must always match the type of the existing record, as well as the schema [prefix](./schemas.md#properties-1) of that type. Type can never be overwritten for existing records.

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

```

The following operators are available with the _digest_ type:
- [`$default`](#default---any)
- [`$value`](#value---any)

### _timestamp_ type

Field type _timestamp_ accepts positive numeric values after the epoch (in milliseconds). In addition, it also accept one string value: `'now'`. When `'now'` is provided, it is automatically converted to the current time in milliseconds since epoch.

The following operators are available with the _timestamp_ type:
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

### _url_ type

The _url_ field type accepts any `string` values that are valid URLs.

The following operators are available with the _url_ type:
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

### _email_ type

The _email_ field type accepts any `string` values that are valid email addresses.

The following operators are available with the _email_ type:
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

### _phone_ type

The _phone_ field type accepts any `string` values that are valid phone numbers.

The following operators are available with the _phone_ type:
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

### _string_ type

The _string_ type accepts any `string` values.

The following operators are available with the _string_ type:
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

### _int_ type

The _int_ type accepts any `number` values that are representable as an integer.

The following operators are available with the _int_ type:
- [`$increment`](#increment---number)
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

### _float_ type

The _float_ type accepts any `number` values that are representable as floating point numbers.

The following operators are available with the _float_ type:
- [`$increment`](#increment---number)
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

### _number_ type

The _number_ type accepts any `number` values.

The following operators are available with the _number_ type:
- [`$increment`](#increment---number)
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

### _boolean_ type

The _boolean_ type accepts a `boolean` value, `true` or `false`.

The following operators are available with the _boolean_ type:
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

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

The following operators are supported both on the _text_ field itself, as well as all the language keys:
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)

### _array_ type

Fields with _array_ type accept javascript array values where all entries correspond to the item type specified in the schema. 

The following operators are available with the _array_ type:
- [`$default`](#default---any)
- [`$value`](#value---any)

### _json_ type

The _json_ type allows any javascript values, unless `properties` has been defined for it in the schema. In that case, an `object` type value must be provided that contains only values that contain properties set in that schema definition.

The following operators are available with the _json_ type:
- [`$default`](#default---any)
- [`$value`](#value---any)

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

Fields with _set_ type accept javascript array values where all entries correspond to the item type specified in the schema. They also accept a single item of the item type to set a list of one item. Any value set will reset the currently stored values of the _set_.

To add and remove items from the set, the following operators are supported:
- [`$default`](#default---any)
- [`$value`](#value---any)
- [`$ref`](#ref---string)
- [`$add`](#add---any)
- [`$delete`](#delete---any)

### _references_ type

Same as _set_ but items are always id strings.

### _object_ type

The _object_ type can receive a javascript object as long as it only contains properties specified in the schema. All operators and values of the object properties are allowed based on the type of the property.

Only _object_ specific operator is the [`$merge`](#object-merge-boolean) operator.

## Field operators

- Basic field type operators
  - [`$default`](#default---any)
  - [`$value`](#value---any)
  - [`$ref`](#ref---string)
- Number field type operators
  - [`$increment`](#increment---number)
- Set and reference field type operators
  - [`$add`](#add---any)
  - [`$delete`](#delete---any)
- Reference only field type operators
  - [`$hierarchy`](#hierarchy---boolean)
- Object field type operators
  - [`$merge`](#object-merge---boolean)

### `$increment` - _number_

Number fields support the `$increment` operator, which itself takes a number value. If specified, the number value in the database is incremented by the specified amount. If not set, the number is assumed 0 and the `$increment` value is effectively set to the field, unless [`$default`](#default-any) is specified in which case it is set before applying the `$increment`. The increment can be negative and fractional.

```javascript
const result = await client.set({
  $id: 'maASxsd3',
  type: 'match',
  value: { $increment: 10 } // can be applied to int, float, number fields
})
```

### `$default` - _any_

If provided, the value put in `$default` has to correspond the type of the field the operator is used on. It is only set if no value currently exists fo that field of the record.

```javascript
Let's assume the following record in database:
{
  id: 'maASxsd3',
  type: 'match',
  title: {
    en: 'yes'
  }
}
*/ 

let result = await client.set({
  id: 'maASxsd3',
  value: { $default: 10 }
})

/*
Value of `const result`: `maASxsd3`
Resulting record in database:
{

  id: 'maASxsd3',
  type: 'match',
  value: 10, // value is set, as it was empty
  title: {
    en: 'yes'
  }
}
*/

result = await client.set({
  id: 'maASxsd3',
  value: { $default: 11 }
})

/*
Value of `const result`: `maASxsd3`
Resulting record in database:
{

  id: 'maASxsd3',
  type: 'match',
  value: 10, // value is still 10, as it had already been set
  title: {
    en: 'yes'
  }
}
*/
```

### `$value` - _any_

Using the `$value` operator is practically always allowed instead of passing the value directly. It is rarely useful by itself, an is often only used if other operators are used with it, so specify what the actual value is to be set.

```javascript
const result = await client.set({
  $id: 'maASxsd3',
  type: 'match',
  value: { $value: 10 } // same as value: 10
})
```

### `$ref` - _string_

The `$ref` option allows referencing other fields within the same object. Whenever the value of the field is read in a query, it actually resolves to the reference stored in it.

```javascript
const result = await client.set({
  $id: 'maASxsd3',
  type: 'match',
  value: { $ref: 'otherValue' },
  otherValue: 10
})
```

The reference operator can be used in conjunction with other options, such as `$default`:

```javascript
const result = await client.set({
  $id: 'maASxsd3',
  type: 'match',
  value: { $default: { $ref: 'otherValue' } }, // the reference is established only if `.value` is not set
  otherValue: 10
})
```

### object `$merge` - _boolean_

Default value: `true`

The `$merge` option operates exactly the same way as the top-level set [`$merge` operator](#merge-boolean), but in the context of the fields of the object type. When an object is set with `$merge: false`, only the set fields will remain in the database. 

### `$add` - _any_

The `$add` operator can be used to add one or more entries to a _set_ or _references_ type field. A single item type value or an array of item type values may be specified to `$add`. All the existing values in the set will remain, but no duplicates are allowed.


```javascript
Let's assume the following record in database:
{
  id: 'maASxsd3',
  type: 'match',
  availableSeats: ['a2', 'a3', 'b5']
}
*/ 

let result = await client.set({
  id: 'maASxsd3',
  availableSeats: { $add: 'b12' }
})

/*
Value of `const result`: `maASxsd3`
Resulting record in database:
{
  id: 'maASxsd3',
  type: 'match',
  availableSeats: ['a2', 'a3', 'b5', 'b12']
}
*/

result = await client.set({
  id: 'maASxsd3',
  availableSeats: { $add: ['b13', 'b14'] }
})

/*
Value of `const result`: `maASxsd3`
Resulting record in database:
{
  id: 'maASxsd3',
  type: 'match',
  availableSeats: ['a2', 'a3', 'b5', 'b12', 'b13', 'b14']
}
*/
```

### `$delete` - _any_

The `$delete` operator can be used to remove one or more entries to a _set_ or _references_ type field. A single item type value or an array of item type values may be specified to `$delete`.


```javascript
Let's assume the following record in database:
{
  id: 'maASxsd3',
  type: 'match',
  availableSeats: ['a2', 'a3', 'b5', 'b12', 'b13', 'b14']
}
*/ 

let result = await client.set({
  id: 'maASxsd3',
  availableSeats: { $delete: ['b13', 'b14'] }
})

/*
Value of `const result`: `maASxsd3`
Resulting record in database:
{
  id: 'maASxsd3',
  type: 'match',
  availableSeats: ['a2', 'a3', 'b5', 'b12']
}
*/

result = await client.set({
  id: 'maASxsd3',
  availableSeats: { $delete: 'b12' }
})

/*
Value of `const result`: `maASxsd3`
Resulting record in database:
{
  id: 'maASxsd3',
  type: 'match',
  availableSeats: ['a2', 'a3', 'b5']
}
*/
```

### `$hierarchy` - _boolean_

Potentially dangerous to use advanced feature. Take care using this option.

Default value: `true`

The `$hierarchy` operator only applies to a very special case of _references_ type fields: `children` and `parents`. When `$hierarchy` is set to `true` (default), if parents of the record being set are updated, their children will be updated to reflect the changes in the record. The same applies to parents of its children, if the `children` field is updated. When set to `false`, this default behaviour is ignored.
