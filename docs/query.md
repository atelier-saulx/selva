# Selva Query DSL Documentation

Selva uses a JSON query DSL to specify the data to be retrieved or subscribed from the database.

  - [**&lt;any field name&gt;**](#any-field-name)
  - [**$id**](#id)
  - [**$all**](#all)
  - [**$value**](#id)
  - [**$default**](#default)
  - [**$language**](#language)
  - [**$inherit**]($inherit)
  - [**$list**]($list)
  - [**$sort**]($sort)
  - [**$offset**]($offset)
  - [**$limit**]($limit)
  - [**$find**]($find)
  - [**$traverse**]($traverse)
  - [**$filter**]($filter)
  - [**$or**]($or)
  - [**$and**]($and)


[Available data types](#available-data-types)

## Clauses

### Default fields

 - `name`: _string_
 - `children`: _references_
 - `parents`: _references_

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

### `$field`: _string_, _Array&lt;string&gt;_

The `$field` clause is used to alias a field to another field in the same document.

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

Because _text datatype_ fields interanlly are a simple nested object, the `$field` clause can be used to get a specific language from a text field.
```javascript
const result = await client.get({
  $id: 'mo2001ASpaceOdyssey',
  englishTitle: { $field: 'title.en' }
})
```

The `$field` clause can take an Array instead of a string. The array can have several field names or paths for the alias to point to. The first that is defined in the document will be returned.

```javascript
const = await client.get({
  $id: 'mo2001ASpaceOdyssey',
  by: { $field: ['producer', 'director'] }
})
```

[See test](../client/test/examples/clauses/field.ts)

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

### `$list`: _boolean_, _object_

Sets the field to return a collection of documents.
Used in conjuction with the `$find` operator.

### `$sort`: _object_

Property of `$list` clause.  
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

### `$offset`: _integer_

Property of `$list` clause.  
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

### `$limit`: _integer_

Property of `$list` clause.  
Limits the `$list` amount of items returned in a `$list`.

[See test](../client/test/examples/clauses/list.ts)

### `$find`: _object_

Traverses ancestors or descendants and assigns matched results to a field.
Can be used inside a `$list` clause to specify collection search terms or independently to reference a single document to a field. In that case it will return the first matched document.

### `$traverse`: _string_

Property of `$find`.
Allowed values: `ancestors`, `descendents`, `children`, `parents`.
Sets direction to search the document hierarchy.

### `$filter`: _object_, _array_

Property of `$find`.

Sets a search term for the `$find` clause.
Has the following properties:

  - `$operator`: _string_ - Operator to use in the comparisson. Allowdd values are: `=`, `>`, `<`, `..`, `!=`, `distance`, `exists`, `notExists`.
  - `$field`: _string_ - Field name to compare the value to.
  - `$value`: _string_ - Value to compare the field to.

Search terms can be composed with the `$or` and `$and` clauses, and nested to create complex logic.
If an array of search terms is used, each term acts as an AND.

### `$or`: _object_

Property of `$filter`.
Adds a OR search term to the filter.
Can be nested.

### `$and`: _object_

Property of `$filter`.
Adds a AND search term to the filter.
Can be nested.

