import { SelvaClient } from '../'
import { GetOptions } from './types'

export default async function resolveId(
  client: SelvaClient,
  props: GetOptions
): Promise<string | undefined> {
  if (props.$id) {
    if (Array.isArray(props.$id)) {
      const exists: boolean[] = await Promise.all(
        props.$id.map(id => {
          return client.redis.exists({ name: props.$db || 'default' }, id)
        })
      )

      const idx = exists.findIndex(x => !!x)
      if (idx === -1) {
        return null
      }

      return props.$id[idx]
    } else {
      if (props.$id === 'root') {
        return 'root'
      }

      const exists = await client.redis.exists(
        { name: props.$db || 'default' },
        props.$id
      )
      return exists ? props.$id : undefined
    }
  } else if (props.$alias) {
    const alias = Array.isArray(props.$alias) ? props.$alias : [props.$alias]
    const resolved: [string | null, boolean][] = <[string | null, boolean][]>(
      await Promise.all(
        alias.map(async alias => {
          return [
            await client.redis.hget(
              { name: props.$db || 'default' },
              '___selva_aliases',
              alias
            ),
            await client.redis.exists({ name: props.$db || 'default' }, alias)
          ]
        })
      )
    )

    const idx = resolved.findIndex(x => {
      return !!x[0] || !!x[1]
    })

    if (idx === -1) {
      return null
    }

    return resolved[idx][0] || alias[idx]
  } else {
    return 'root'
  }
}
