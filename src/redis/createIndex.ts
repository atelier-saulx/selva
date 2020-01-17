import RedisClient from './'
import { searchSchema } from '../schema'

// const create = async (index, schema) => {
//   const args = [index, 'SCHEMA']
//   for (const field in schema) {
//     args.push(field, ...schema[field])
//   }
//   try {
//     return client.ft_create(args)
//   } catch (e) {}
// }

// ftInfo
const makeSchema = async (client: RedisClient) => {
  const index = 'selvaIndex'
  try {
    const info = await client.ftInfo(index)
    console.log('info', info)
    const fields = info[info.indexOf('fields') + 1]
    const set = new Set()
    let changed = fields.find(([field, x, ...type]) => {
      const scheme = searchSchema[field]
      set.add(field)
      return scheme && scheme.find((key, i) => type[i] !== key)
    })
    if (!changed) {
      for (const field in searchSchema) {
        // console.log('!!', field, set.has(field))
        if (!set.has(field)) {
          changed = true
          break
        }
      }
    }
    if (changed) {
      // return alter(index, searchSchema)
    }
  } catch (e) {
    if (/Unknown Index name/.test(e)) {
      // return create(index, searchSchema)
    }
  }
}
