import { SelvaClient } from '.'
import { ServerSelector } from './types'
import { getSchema } from './schema/getSchema'
import { createHash } from 'crypto'
import { FieldSchema } from './schema'

function sha1hex(str: string): string {
  const hash = createHash('sha1')
  hash.update(str)
  return hash.digest('hex')
}

export async function deleteType(
  client: SelvaClient,
  name: string,
  selector: ServerSelector
): Promise<void> {
  const { schema } = await getSchema(client, selector)
  if (!schema.types[name]) {
    return
  }

  const allItems = await client.get({
    $db: selector.name,
    items: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $operator: '=',
              $field: 'type',
              $value: name,
            },
          ],
        },
      },
    },
  })

  await Promise.all(
    allItems.items.map(({ id }) => {
      return client.delete({
        $db: selector.name,
        $id: id,
      })
    })
  )

  delete schema.types[name]
  schema.sha = sha1hex(JSON.stringify(schema))
  await client.redis.hset(
    selector,
    '___selva_schema',
    'types',
    JSON.stringify(schema)
  )

  await client.redis.publish(
    selector,
    '___selva_events:schema_update',
    'schema_update'
  )
}

export async function deleteField(
  client: SelvaClient,
  type: string,
  name: string,
  selector: ServerSelector
): Promise<void> {
  const { schema } = await getSchema(client, selector)
  const typeSchema = schema.types[type]
  if (!typeSchema) {
    return
  }

  const parts = name.split('.')
  let it: any = typeSchema
  let fieldPart: any = {}
  let fIt: any = fieldPart
  for (let i = 0; i < parts.length; i++) {
    const acc = it.fields || it.properties || it
    if (!acc[parts[i]]) {
      return
    }

    it = acc[parts[i]]
    fIt[parts[i]] = i === parts.length - 1 ? true : {}

    if (fIt[parts[i]] === true) {
      delete acc[parts[i]]
    }

    fIt = fIt[parts[i]]
  }

  const allItems = await client.get({
    $db: selector.name,
    items: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $operator: '=',
              $field: 'type',
              $value: type,
            },
          ],
        },
      },
    },
  })

  await Promise.all(
    allItems.items.map(({ id }) => {
      return client.delete({
        $db: selector.name,
        $id: id,
        ...fieldPart,
      })
    })
  )

  schema.sha = sha1hex(JSON.stringify(schema))
  await client.redis.hset(
    selector,
    '___selva_schema',
    'types',
    JSON.stringify(schema)
  )

  await client.redis.publish(
    selector,
    '___selva_events:schema_update',
    'schema_update'
  )
}

export async function castField(
  client: SelvaClient,
  type: string,
  name: string,
  newType: FieldSchema,
  selector: ServerSelector
): Promise<void> {
  const { schema } = await getSchema(client, selector)
  const typeSchema = schema.types[type]
  if (!typeSchema) {
    return
  }

  const parts = name.split('.')
  let it: any = typeSchema
  for (let i = 0; i < parts.length - 1; i++) {
    const acc = it.fields || it.properties || it
    if (!acc[parts[i]]) {
      return
    }

    it = acc[parts[i]]
  }

  it = it.fields || it.properties || it
  it[parts[parts.length - 1]] = newType

  schema.sha = sha1hex(JSON.stringify(schema))
  await client.redis.hset(
    selector,
    '___selva_schema',
    'types',
    JSON.stringify(schema)
  )

  await client.redis.publish(
    selector,
    '___selva_events:schema_update',
    'schema_update'
  )
}
