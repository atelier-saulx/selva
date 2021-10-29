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
