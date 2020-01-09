import { Id } from './schema'
import { SelvaClient } from '.'

type DeleteOptions =
  | Id
  | {
      $id: Id
      $hierarchy?: boolean
    }

async function deleteItem(
  client: SelvaClient,
  options: DeleteOptions
): Promise<boolean> {
  let hierarchy = true
  let id
  if (typeof options == 'object') {
    id = options.$id
    if (options.$hierarchy === false) {
      hierarchy = false
    }
  } else {
    id = options
  }

  if (hierarchy) {
    const children = await client.redis.smembers(id + '.children')
    const parents = await client.redis.smembers(id + '.parents')
    for (let parent of parents) {
      await client.redis.srem(parent + '.children', id)
    }
    for (let child of children) {
      const key = child + '.parents'
      await client.redis.srem(key, id)
      const size = await client.redis.scard(key)
      if (!size) {
        await deleteItem(client, { $id: child })
      }
    }
  }
  await client.redis.del(id + '.children')
  await client.redis.del(id + '.parents')
  await client.redis.del(id + '.ancestors')
  // returns true if it existed
  return client.redis.del(id)
}

export { deleteItem, DeleteOptions }
