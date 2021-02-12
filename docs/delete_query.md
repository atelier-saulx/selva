# _Delete_ Method Query Reference

- [**\$id**](#id--string)
- [**&lt;any field name&gt;**](#any-field-name--boolean)

## `$id` : _string_

```javascript
const result = await client.delete({
  $id: 'maASxsd3',
}) // or simply client.delete('maASxsd3')
```

If no fields are specified, the whole record is completely deleted. If the record has children that no longer have parents after the record itself is deleted, those children are deleted recursively. Thus the following will delete the entire database:

```javascript
const result = await client.delete({
  $id: 'root',
}) // or simply client.delete('root')
```

## &lt;any field name&gt; : _boolean_

When fields are specified, a `boolean` value may be provided with it. A `true` value marks the field to be deleted.

```javascript
const result = await client.delete({
  $id: 'maASxsd3',
  title: true,
  value: true,
})
```

The above example unsets the `title` and `value` fields in the record.

Instead of providing `true`, `false` can be used to indicate that all values except the provided fields are to be deleted.

```javascript
const result = await client.delete({
  $id: 'maASxsd3',
  value: true,
})
```

The above example deletes all fields except `value` (and the important internal fields like `id`, `type`, `children` and `parents`).

The `true` and `false` values are resolved for each nested level. Thus the following will delete all languages for a `text` field, except the english value, but leaves the top level fields of the record intact:

```javascript
const result = await client.delete({
  $id: 'maASxsd3',
  title: {
    en: false,
  },
})
```
