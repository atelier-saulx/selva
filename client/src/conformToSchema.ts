import { SelvaClient } from './'
import { SetOptions } from './set'
import { Schema, FieldSchemaObject } from './schema'

function getTypeFromId(schema: Schema, id: string): string | undefined {
  if (id === 'root') {
    return 'root'
  }

  return schema.prefixToTypeMapping[id.substr(0, 2)]
}

export default async function conformToSchema(
  client: SelvaClient,
  props: SetOptions,
  dbName: string = 'default'
): Promise<SetOptions> {
  if (!props.$id && !props.type && !props.$alias) {
    return null
  }

  const schema = (await client.getSchema(dbName)).schema

  if (props.$id !== 'root') {
    if (!props.type) {
      if (props.$id) {
        props.type = getTypeFromId(schema, props.$id)
      } else {
        const typePayload = await client.get({
          $alias: props.$alias,
          type: true,
          id: true
        })

        props.type = typePayload.type
        props.$id = typePayload.id
      }
    }

    if (!props.type) {
      return null
    }
  }

  const typeSchema =
    props.$id === 'root' ? schema.rootType : schema.types[props.type]

  const newProps: SetOptions = {
    type: props.type
  }

  if (props.$id) {
    newProps.$id = props.$id
  }

  if (props.$alias) {
    newProps.$alias = props.$alias
  }

  const mergeObject: (
    x: SetOptions,
    schema: FieldSchemaObject
  ) => SetOptions = (oldObj: SetOptions, schema: FieldSchemaObject) => {
    const newObj: SetOptions = {}
    for (const key in oldObj) {
      if (schema.properties[key]) {
        if (schema.properties[key].type === 'object') {
          newObj[key] = mergeObject(
            oldObj[key],
            <FieldSchemaObject>schema.properties[key]
          )
        } else if (
          schema.properties[key].type === 'array' &&
          // @ts-ignore
          schema.properties[key].items.type === 'object'
        ) {
          newObj[key] = oldObj[key].map(x => {
            // @ts-ignore
            return mergeObject(x, schema.properties[key].items)
          })
        } else if (
          schema.properties[key].type === 'set' &&
          // @ts-ignore
          schema.properties[key].items.type === 'object'
        ) {
          newObj[key] = oldObj[key].map(x => {
            // @ts-ignore
            return mergeObject(x, schema.properties[key].items)
          })
        } else {
          newObj[key] = oldObj[key]
        }
      }
    }

    return newObj
  }

  for (const key in props) {
    if (typeSchema.fields[key]) {
      if (typeSchema.fields[key].type === 'object') {
        newProps[key] = mergeObject(
          props[key],
          <FieldSchemaObject>typeSchema.fields[key]
        )
      } else if (
        typeSchema.fields[key].type === 'array' &&
        // @ts-ignore
        typeSchema.fields[key].items.type === 'object'
      ) {
        newProps[key] = props[key].map(x => {
          // @ts-ignore
          return mergeObject(x, typeSchema.fields[key].items)
        })
      } else if (
        typeSchema.fields[key].type === 'set' &&
        // @ts-ignore
        typeSchema.fields[key].items.type === 'object'
      ) {
        newProps[key] = props[key].map(x => {
          // @ts-ignore
          return mergeObject(x, typeSchema.fields[key].items)
        })
      } else {
        newProps[key] = props[key]
      }
    }
  }

  return newProps
}
