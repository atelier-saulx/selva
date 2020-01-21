import { SetOptions } from './types'
import { SelvaClient } from '..'
import getTypeFromId, { getPrefixes } from '../getTypeFromId'

const parseSetObject = (payload: SetOptions): SetOptions => {
  const result: SetOptions = {}

  console.log('Do it payload')
  return result
}

// ---------------------------------------------------------------
async function set(client: SelvaClient, payload: SetOptions): Promise<string> {
  const redis = client.redis

  let prefixes, type
  if (!payload.type) {
    prefixes = await getPrefixes(client)
    type = await getTypeFromId(client, payload.$id, prefixes)
  } else {
    type = payload.type
  }

  console.log('--->', type)
  // const schema =
  // get type

  const parsed = parseSetObject(payload)

  console.log(parsed)

  // const modifyResult = await client.modify({
  //   kind: 'update',
  //   payload: <SetOptions & { $id: string }>payload // assure TS that id is actually set :|
  // })

  // return modifyResult[0]
  return 'ok'
}

export { set, SetOptions }
