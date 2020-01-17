import { Id } from './schema'
import { SetOptions } from './setTypes'
import { SelvaClient } from './'

// ---------------------------------------------------------------
async function set(client: SelvaClient, payload: SetOptions): Promise<Id> {
  const redis = client.redis
  if (!payload.$id) {
    if (!payload.type) {
      throw new Error('Cannot create an item if type is not provided')
    }
    const type =
      typeof payload.type === 'string' ? payload.type : payload.type.$value
    if (
      (payload.externalId && typeof payload.externalId === 'string') ||
      Array.isArray(payload.externalId)
    ) {
      payload.$id = client.id({
        type,
        externalId: payload.externalId
      })
    } else {
      payload.$id = client.id({ type })
    }
  }

  const modifyResult = await client.modify({
    kind: 'update',
    payload: <SetOptions & { $id: string }>payload // assure TS that id is actually set :|
  })

  return modifyResult[0]
}

export { set, SetOptions }
