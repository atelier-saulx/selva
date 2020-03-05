# Selva Schema Definition

Schemas define da data structure to be used bla bla bla

  - [_root_ object](#rootobject)
  - [Types](#types)
    - [Available field data types](#avilablefielddatatypes)

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
| `fields` | Object | | [Data type](#avilablefielddatatypes) for the field.
| `search` | Object | optional | Indexing info blabla

##### Avilable field data types

- **string**: `"flurpy"`, `"pants"`  
- **integer**: `123`, `666`
- **float**: `12.234`, `6.6666`
- **timestamp**: `269222400`
- ...
