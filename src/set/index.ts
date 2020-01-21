import { SetOptions } from './types'
import { SelvaClient } from '..'

// ---------------------------------------------------------------
async function set(client: SelvaClient, payload: SetOptions): Promise<string> {
  const redis = client.redis
  const modifyResult = await client.modify({
    kind: 'update',
    payload: <SetOptions & { $id: string }>payload // assure TS that id is actually set :|
  })

  return modifyResult[0]
}

export { set, SetOptions }
