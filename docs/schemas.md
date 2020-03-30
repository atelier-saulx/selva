# Selva Schema Definition

Schemas define the data structure to be used.  
They allow for data to be automatically validated and the right type of index to be used for searching.

  - [_root_ object](#root-object)
  - [Types](#types)
    - [`Object` type](#object-type)
  - [Type Fields](#type-fields)
    - [Field index types](#field-indexes-types)
    - [Field data types](#field-data-types)

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
| `search` | Object | optional | [Index](#fieldindexes) type for the field.

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

#### Field indexes types

For the fields to be searchable, it needs an index.
There are several types of indexes avaliable. One field may have multiple index types.


  - `EXISTS`
  - `TAG`
  - `TEXT`
  - `NUMERIC`
  - `SORTABLE`
  - `TEXT-LANGUAGE`
  - `GEO`
  - `TEXT-LANGUAGE-SUG`

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
