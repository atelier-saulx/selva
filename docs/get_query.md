# _Get_ Method Query Reference


  - [**$id**](#id-string---string)
  - [**$alias**](#alias-string---string)
  - [**&lt;any field name&gt;**](#any-field-name-boolean-object)
    - [**$value**](#value-any)
    - [**$default**](#default-any)
    - [**$inherit**](#inherit-boolean)
      - [**$type**](#inherit-boolean)
      - [**item**](#inherit-boolean)
      - [**required**](#inherit-boolean)
    - [**$field**](#field-string-arraystring)
    - [**$list**](#list-boolean-object)
      - [**$sort**](#sort-object)
      - [**$offset**](#offset-integer)
      - [**$limit**](#limit-integer)
  - [**$all**](#all-boolean)
  - [**$language**](#language-string)
  - [**$find**](#find-object)
    - [**$traverse**](#traverse-string)
    - [**$filter**](#filter-object-array)
      - [**$or**](#or-object)
      - [**$and**](#and-object)

## Query structure

Queries are objects that hold the data structure to be returned, as well as the operators that shape it.

```javascript
const result = await get({
  $language: 'en',      // operator that sets the language to return in text fields
  $id: 'muASxsd3',      // operator that sets the id of the document to return

  title: true,          // includes the document fields title and
  year: true,           // year to the results

  technicalData: {      // adds the field technicalData which is
    runtime: true,      // a object data type with nested properties.
    camera: {           // Only `runtime` and `camera.lens` properties are
      lens: true        // included in the result
    }
  }
})

// returns:
// {
//   title: '2001: A Space Odyssey',
//   year: 1968,
//   technicalData: {
//     runtime: 97
//     camera: {
//       lens: '5-perf 65mm'
//     }
//   }
// }
```

They can mirror the existing structure in the document to filter its contents or dynamically added to include fields inherited from its ancestors. See `$inherit` for more details.

```javascript
const result = await get({
  $language: 'en',
  $id: 'muASxsd3',
  title: true,
  icon: { $inherit: true }, // icon is not part of the movie type
                            // but inherited from the parent document with
                            // type "genre"
})

// returns:
// {
//   title: '2001: A Space Odyssey',
//   icon: 'http://example.com/an_icon.png'
// }
```

A query can also have other queries nested directly in the field value or as part of a `$field` operator.

```javascript
const result = await get({
  $language: 'en',
  $id: 'muASxsd3',
  title: true,
  otherMovie: {
    $id: 'muFDedx2',    // adds another query
    title: true         // as the value of `otherMovie` field
  }
})

// returns:
// {
//   title: '2001: A Space Odyssey',
//   otherMovie: {
//     title: 'Soylent Green'
//   }
// }
```

Array syntax can also be used to concatenate different individual queries. The result of each query is concatenated in the returned array.

```javascript
const result = await get({
  $language: 'en',
  $id: 'muASxsd3',
  title: true,
  type: true,             // default field type that holds the documetn type
  extraFields: [
    {
      $id: 'muFDedx2',
      title: true,
      type: true
    },
    {
      $id: 'geGhfr4D',
      title: true,
      type: true
    }
  ]
})

// returns:
// {
//   title: '2001: A Space Odyssey',
//   type: 'movie',
//   extraFields: [
//     {
//       title: 'Soylent Green',
//       type: 'movie',
//     },
//     {
//       title: 'Sci-fi',
//       type: 'genre',
//     }
//   ]
// }
```
## `$id`: _string_  | _string[]_

Id of the object to get. If it is an array, the first existing record is used for the query.
If omited, the _root_ object id is assumed. Can only be used at the top level of a query object.

```javascript
const result = await get({
  $id: 'muASxsd3'
})
```

## `$alias`: _string_  | _string[]_

Alias of the object to get. If it is an array, the first existing record is used for the query.
An id can also be passed as $alias. If the specified alias does not exist, the value is tried as an id lookup also before moving to the next entry if an array is specified.

```javascript
const result = await get({
  $id: 'muASxsd3'
})
```

## `<any field name>`: _boolean_, _object_

When truthy, includes the named field in the result.  
Objects can be nested, and other fields and operators specified.

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

 
### `$inherit`: _boolean_

If the value for the field is not set in the document, search for the field in the ancestors.

```javascript
// `icon` is not set in the 'moSoylentGreen' document
// but exists in the parent genre document
const result = await client.get({
  $id: 'moSoylentGreen',
  icon: { $inherit: true },
})
```
[See test](../client/test/examples/clauses/inherit.ts)

#### `$type`: _string_, _array&lt;string&gt;_

Limits inheritance to a type or array of types.

```javascript
// `icon` is not set in the 'moSoylentGreen' document
// but exists in the parent genre document
const result = await client.get({
  $id: 'moSoylentGreen',
  icon: {
    $field: 'icon',                                 // inherit the field icon only
    $inherit: { $type: ['genre', 'collection'] }    // fron ancestors of type genre or collection
  }                                                // whichever comes first.
})
```

Should be used in conjunction with the [`$field`](#field-string-arraystring) operator.

#### `$item`: _string_, _array&lt;string&gt;_

Inherits multiple fields from an ancestor.

```javascript
// `icon` is not set in the 'moSoylentGreen' document
// but exists in the parent genre type and its parent collection type.
const result = await client.get({
  $language: 'en',
  $id: 'moSoylentGreen',
  title: true,
  parentImage: {
    title: true,
    image: true,                                    // image does not exist in genre but
    icon: true,                                     // icon exists so fields from genre
    $inherit: { $type: ['genre', 'collection'] }    // are included.
  }
})

// returns:
// {
//   title: 'Soylent Green',
//   parentImage: {
//     title: 'Sci-fi',
//     icon: 'http://example.com/genre_icon.png'
//   }
// }
```

#### `$required`: _string_, _array&lt;string&gt;_

Requires existing fields when using `$item`.

```javascript
// `icon` is not set in the 'moSoylentGreen' document
// but exists in the parent genre type and its parent collection type.
const result = await client.get({
  $language: 'en',
  $id: 'moSoylentGreen',
  title: true,
  parentImage: {
    title: true,
    image: true,
    icon: true,
    $inherit: {
      $type: ['genre', 'collection'],   // even though genre is an ancestor closer to the
      $required: ['image']              // target document, it does not have image. So
     }                                  // fields from the type collection are
   }                                    // returned instead.
})

// returns:
// {
//   title: 'Soylent Green',
//   parentImage: {
//     title: 'Sci-fi',
//     icon: 'http://example.com/collection_icon.png'
//     image: 'http://example.com/collection_image.jpg'
//   }
// }
```

### `$field`: _string_, _Array&lt;string&gt;_

The `$field` operator is used to create a field that fetches its results from another field.

```javascript
const result = await client.get({
  $id: 'mo2001ASpaceOdyssey',
  directedBy: { $field: 'director' }
})
```

Dot notation can be used to create a path and alias a nested field or even specific data inside a _JSON datatype_.
```javascript
const result = await client.get({
  $id: 'mo2001ASpaceOdyssey',
  ratio: { $field: 'technicalData.aspectRatio' }
})
```

Because _text datatype_ fields interanlly are a simple nested object, the `$field` operator can be used to get a specific language from a text field.
```javascript
const result = await client.get({
  $id: 'mo2001ASpaceOdyssey',
  englishTitle: { $field: 'title.en' }
})
```

The `$field` operator can take an Array instead of a string. The array can have several field names or paths for the alias to point to. The first that is defined in the document will be returned.

```javascript
const = await client.get({
  $id: 'mo2001ASpaceOdyssey',
  by: { $field: ['producer', 'director'] }
})
```

The path for the `$field` operator can have variable expansion with the `${path_for_field}` syntax.
In the example below, the `$field` operator path gets the language from the __value__ of a field called `preferedLanguage`.

```javascript
const = await client.get({
  $id: 'mo2001ASpaceOdyssey',
  titleInPreferedLanguage: { $field: 'title.${preferedLanguage}' }
})
```
The field the path points to can exist in the same document or through `$inherit` and `$find` be part of another document.

[See test](../client/test/examples/clauses/field.ts)

### `$list`: _boolean_, _object_

Sets the field to return a collection of documents.
Used in conjuction with the `$find` operator.

Needs a [`$find`](#find-object) operator to gather the items to return in the list.

#### `$sort`: _object_

Property of `$list` operator.  
Sorts the `$list` results according to the following properties:

  - `$field`: _string_ - Name of the field to sort by.
  - `$order`: _['asc', 'desc']_ - Sort in ascending or descending order.

```javascript
const result = await client.get({
  $id: 'geScifi',
  $language: 'en',
  children: {
    title: true,
    year: true,
    $list: {
      $sort: { $field: 'year', $order: 'asc' }
    }
  }
})
```

[See test](../client/test/examples/clauses/list.ts)

#### `$offset`: _integer_

Property of `$list` operator.  
Shows results of a `$list` starting at the specified index.

```javascript
const result = await client.get({
  $id: 'geScifi',
  $language: 'en',
  children: {
    title: true,
    year: true,
    $list: {
      $sort: { $field: 'year', $order: 'asc' },
      $offset: 0,
      $limit: 2,
    }
  }
})
```

#### `$limit`: _integer_

Property of `$list` operator.  
Limits the `$list` amount of items returned in a `$list`.

[See test](../client/test/examples/clauses/list.ts)




## `$all`: _boolean_

Includes all the fields in the document.

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

## `$language`: _string_

Filters document data to a set language on fields that support it.

```javascript
const result = await client.get({
  $language: 'en',
  $id: 'mo2001ASpaceOdyssey',
  title: true
})
```
[See test](../client/test/examples/clauses/language.ts)

## `$find`: _object_

Traverses ancestors or descendants and assigns matched results to a field.
Can be used inside a `$list` operator to specify collection search terms or independently to reference a single document to a field. In that case it will return the first matched document.

### `$traverse`: _string_

Property of `$find`.
Allowed values: `ancestors`, `descendents`, `children`, `parents`.
Sets direction to search the document hierarchy.

### `$filter`: _object_, _array_

Property of `$find`.

Sets a search term for the `$find` operator.
Has the following properties:

  - `$operator`: _string_ - Operator to use in the comparisson. Allowdd values are: `=`, `>`, `<`, `..`, `!=`, `distance`, `exists`, `notExists`.
  - `$field`: _string_ - Field name to compare the value to.
  - `$value`: _string_ - Value to compare the field to.

Search terms can be composed with the `$or` and `$and` operators, and nested to create complex logic.
If an array of search terms is used, each term acts as an AND.

```javascript
const result = await client.get({
  $id: 'mo2001ASpaceOdyssey',
  $language: 'en',
  title: true,
  genres: {
    name: true,
    $list: {
      $traverse: 'parents',
      $find: {
        $filter: {
          $field: 'type',
          $operator: '=',
          $value: 'genre'
        }
      }
    }
  }
})
```

#### `$or`: _object_

Property of `$filter`.
Adds a OR search term to the filter.
Can be nested.

#### `$and`: _object_

Property of `$filter`.
Adds a AND search term to the filter.
Can be nested.

```javascript
const result = await client.get({
  $id: 'geScifi',
  $language: 'en',
  name: true,
  longMovies: {
    title: true,
    $list: {
      $traverse: 'children',
      $find: {
        $filter: {
          $field: 'type',
          $operator: '=',
          $value: 'movie',
          $and: {
            $field: 'technicalData.runtime',
            $operator: '>',
            $value: 100
          }
        }
      }
    }
  }
})
```

