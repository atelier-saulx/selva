import test from 'ava'
import { SelvaClient } from '../../src/index'

export const wait = (timeMs: number = 500): Promise<void> =>
  new Promise((r) => setTimeout(r, timeMs))

export const idExists = async (
  client: SelvaClient,
  id: string
): Promise<boolean> => {
  return !!(await client.redis.exists(id)) && !!(await client.redis.selva_object_get('', id))
}

export const dumpDb = async (client: SelvaClient): Promise<any[]> => {
  const ids = await client.redis.keys('*')
  return (
    await Promise.all(
      ids.map((id) => {
        if (id.startsWith('___')) {
          return null
        }
        if (id.startsWith('nm:')) {
          return null
        }

        if (id.startsWith('idx:')) {
          return null
        }

        if (id.endsWith('._depth')) {
          return null
        }
        if (id.endsWith('.ancestors')) {
          return null
        }
        if (id.startsWith(`tag:`)) {
          return null
        }
        return <any>client.redis.selva_object_get('', id)
      })
    )
  )
    .map((v, i) => {
      return [ids[i], v]
    })
    .filter((x) => !!x[1])
}

export const logDb = async (client: SelvaClient) => {
  console.log(await dumpDb(client))
}
