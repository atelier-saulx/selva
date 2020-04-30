# Selva Schema Definition

Schemas define the data structure to be used.  
They allow for data to be automatically validated and the right type of index to be used for searching.

  - [_root_ object](#root-object)
  - [Types](#types)
    - [`Object` type](#object-type)
  - [Type Fields](#type-fields)
    - [Field index types](#field-indexes-types)
    - [Field data types](#field-data-types)

### Default fields

These fields are added automatically as part of each type.

 - `name`: _string_
 - `children`: _references_
 - `parents`: _references_


### _root_ object

#### Properties

| Name | type | atributes | description |
| ---  | ---  | ---       | ---         |
| `languages` | Array | optional | Sets the available languages in the data |
| `types` | Object | | Defines the [types](#types). Each object key is the type name and it's value the type definition. |


### Types

```javascript
{
  //...
  types: {
    league: { // type name
      prefix: 'le', // type prefix
      fields: { 
        name: { type: 'string', search: { type: ['TAG'] } }
      }
    }
  }
}
```

#### Properties

| Name | type | atributes | description |
| ---  | ---  | ---       | ---         |
| `prefix` | string | optional | Two character string that identifies the type. Used as a prefix for each object/id |
| `fields` | Object | | Defines the [fields](#typefields) for the type. Each object key is a field name and its value the field difinition. |

### Type Fields

Defines the fields available to each type.

#### Properties

| Name | type | atributes | description |
| ---  | ---  | ---       | ---         |
| `type` | string | | Two character string that identifies the type. Used as a prefix for each object/id |
| `fields` | Object | | [Data type](#fielddatatypes) for the field.
| `search` | Object | optional | [Index](#field-indexes) type for the field.

#### `Object` type

Object allow for nested structures, but unlike the JSON type, they allow it's properties to have type validation and indexes.
In field of type object, the subfield `properties` is a reserved field name for this feature.

##### Example

```javascript
{
  // ...
  movie: {
    prefix: 'mo',
    fields: {
      title: { type: 'text' },
      technicalData: {
        type: 'object',
        properties: {
          runtime: { type: 'int' },
          color: { type: 'string' },
          aspectRatio: { type: 'string' }
        }
      }
    }
  }
}
```

#### Field indexes

Field indexes are described in the schema objects of the following format:

```
{
  index: 'default' // a `string` value, optional, defaults to the `default` index
  type: ['TAG', 'EXISTS'] // array of field index types
}
```

See [index types](#fieldindextypes) below for more information.

#### Field indexes types

For the fields to be searchable, it needs an index.
There are several types of indexes avaliable. One field may have multiple index types.


  - [`EXISTS`](#exists)
  - [`TAG`](#tag)
  - [`NUMERIC`](#numeric)
  - [`SORTABLE`](#sortable)
  - [`TEXT-LANGUAGE`](#text-language)
  - [`GEO`](#geo)
  - [`TEXT-LANGUAGE-SUG`](#text-language-sug)

The following combinations are supported:
- `['TAG']`
- `['TAG', 'EXISTS']`
- `['NUMERIC']`
- `['NUMERIC', 'EXISTS']`
- `['NUMERIC', 'SORTABLE']`
- `['NUMERIC', 'SORTABLE', 'EXISTS']`
- `['GEO']`
- `['GEO', 'EXISTS']`
- `['TEXT-LANGUAGE']`
- `['TEXT-LANGUAGE', 'EXISTS']`
- `['TEXT-LANGUAGE-SUG']`
- `['TEXT-LANGUAGE-SUG', 'EXISTS']`

Below you will find a description for all the existing index types.

##### TAG

The `TAG` index type is most commonly applied to values of the following field types:

- _string_
- _digest_
- _boolean_
- _url_
- _email_
- _phone_

but can also be applied to the following types:

- _number_
- _float_
- _int_
- _timestamp_

The _type_ field is automatically indexed as a `TAG`.

When indexed as `TAG`, the field becomes queryable with [`$filter`](./get_query.md#filter-object-array) using the following operators:

- `$operator: '='`
- `$operator: '!='`

The filter will match the field values as an exact match of the values.


##### NUMERIC

The `NUMERIC` index type is most commonly applied to values of the following field types:

- _number_
- _float_
- _int_

but can also be applied to the following types:

- _timestamp_

When indexed as `NUMERIC`, the field becomes queryable with [`$filter`](./get_query.md#filter-object-array) using the following operators:

- `$operator: '='` (exact equality of the numeric values)
- `$operator: '!='` (exact inequality of the numeric values)
- `$operator: '>'` (larger than `$value`)
- `$operator: '<'` (smaller than `$value`)
- `$operator: '..'` (is in the range from `$value[0]` to `$value[1]`)

##### SORTABLE

The `SORTABLE` index type is most commonly applied to values of the following field types (indexed as `NUMERIC`):

- _number_
- _float_
- _int_
- _timestamp_

but can also be applied to the following types:

- _string_ (as `TAG`)

When indexed as `SORTABLE`, the field becomes queryable with [`$sort`](./get_query.md#sort-object).

##### TEXT-LANGUAGE

The `TEXT-LANGUAGE` index type con be applied to the `text` type only.

When indexed as `TEXT-LANGUAGE`, the field becomes queryable with [`$filter`](./get_query.md#filter-object-array) using the following operators:

- `$operator: '='` 
- `$operator: '!='` 

The text match will be a text search approximate string match, including preprocennig such as:
- Removing special characters
- Word stemming
- Excluding most common words if they don't affect the search results

Some additional operators are possible in the `$value` filter property:

```
$value: 'baye*' // would match bayern
```

```
$value: '%heylo%' // would match "hello", with one character substitution from "hello"
```

```
$value: '%%heyyo%%' // would match "hello", with two character substitutions from "hello"
```

##### TEXT-LANGUAGE-SUG

The `TEXT-LANGUAGE-SUG` index type con be applied to the `text` type only.

When indexed as `TEXT-LANGUAGE-SUG`, the field becomes queryable with [`$filter`](./get_query.md#filter-object-array) using the following operators:

- `$operator: '='` 
- `$operator: '!='` 

Matching with `TEXT-LANGUAGE-SUG` works much the same way as with `TEXT-LANGUAGE`, however, the `$value` property is first passed through a search autocomplete index. It is mostly used for things such as implementing the search bar for movie titles.

No operators are possible in the `$value` filter property itself, unlike with `TEXT-LANGUAGE`.

##### GEO

The `GEO` index type con be applied to the `geo` type only.

When indexed as `GEO`, the field becomes queryable with [`$filter`](./get_query.md#filter-object-array) using the following operator:

- `$operator: 'distance'`

The `distance` operator can be used to return only results within a certain radius of a lot/lon location.

##### EXISTS

The `EXISTS` index type con be applied to thea any type. When other indexing is enabled, it can be added as the last index type. It enables the following [`$filter`](./get_query.md#filter-object-array) operators:

- `$operator: 'exists'` (filter matches only records where the field has been set)
- `$operator: 'notExists'` (filter matches only records where the field has _not_ been set)

#### Field data types

  - `float`
  - `boolean`
  - `number`
  - `int`
  - `references`
  - `string`
  - `text`
  - `id`
  - `digest`
  - `url`
  - `email`
  - `phone`
  - `geo`
  - `timestamp`
  - `set`
  - `array`
  - [`object`](#object-type)
  - `json`

For more information on what kind of values are stored in each of these types, you can visit the [set query](./set_query.md#any-field-name) documentation.
