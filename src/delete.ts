import { Id } from './schema'
import { DeleteOptions } from './deleteTypes'
import { SelvaClient } from '.'

import { removeFromAncestors } from './ancestors'

async function deleteItem(
  client: SelvaClient,
  id: Id,
  hierarchy: boolean = true
): Promise<boolean> {
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
        await deleteItem(client, child)
      } else {
        await removeFromAncestors(client, child, [id])
      }
    }
  }
  await client.redis.del(id + '.children')
  await client.redis.del(id + '.parents')
  // returns true if it existed
  return client.redis.del(id)
}

export { deleteItem, DeleteOptions }
