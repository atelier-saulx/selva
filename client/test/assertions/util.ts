import test from 'ava'
import { SelvaClient } from '../../src/index'

export const wait = (timeMs: number = 500): Promise<void> =>
  new Promise((r) => setTimeout(r, timeMs))

export const idExists = async (
  client: SelvaClient,
  id: string
): Promise<boolean> => {
  try {
    await client.redis.selva_object_exists(id, 'id')
  } catch (err) {
    return false
  }
  return true
}

export const getIndexingState = async (client: SelvaClient) => {
  const l = await client.redis.selva_index_list('___selva_hierarchy')
  const stateMap = {}

  for (let i = 0; i < l.length; i += 2) {
    const key = l[i].split('.')
    const expression = Buffer.from(key[key.length - 1], 'base64').toString()
    const state = {
      expression: expression,
      take_max_ave: l[i + 1][0],
      tot_max_ave: l[i + 1][1],
      ind_take_max_ave: l[i + 1][2],
      card: l[i + 1][3],
    }

    stateMap[l[i]] = state
  }

  return stateMap
}
