# _Get_ Method Query Reference


  - [**$id**](#id-string---string)
  - [**$alias**](#alias-string---string)
  - [**&lt;any field name&gt;**](#any-field-name-boolean-object)
    - [**$value**](#value-any)
    - [**$default**](#default-any)
    - [**$inherit**](#inherit-boolean)
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

Requires the use of [`$find`](#find-object)

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

#### `$find`: _object_
See [`$find`](#find-object)




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

