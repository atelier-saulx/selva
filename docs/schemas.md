# Selva Schema Definition

Schemas define the data structure to be used.  
They allow for data to be automatically validated and the right type of index to be used for searching.

  - [_root_ object](#rootobject)
  - [Types](#types)
    - [Field index types](#fieldindexes)
    - [Field data types](#fielddatatypes)

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
  - `type`
  - `timestamp`
