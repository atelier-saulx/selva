# Selva Query DSL Documentation

## Fields and Operators

Selva query DSL uses a Javascript object or JSON formated object to represent both the database queries and data format for the _documents_ returned. The property keys in the object can be _fields_ or _operators_. The values for each property can be a boolean type to include the _document_ field in the result or another nested object with more fields and operators making it possible to compose complex data structures.
The _fields_ can be existing properties of the documents, or new fields added dynamically as part of the result.
_Operators_ do not show as fields but have a special meaning to Selva and manipulate the result adding, transforming, or including subqueries to the _document_ _fields_.

## Reference

- [_Get_ Method Queries](./get_query.md)
- [_Set_ Method Queries](./set_query.md)
- [_Delete_ Method Queries](./delete_query.md)
