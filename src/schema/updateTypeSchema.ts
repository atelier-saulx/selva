import { SelvaClient } from '..'
import { Types, SearchIndexes, FieldSchema } from './'
import updateIndex from './updateIndex'

const parseField = (
  type: string,
  field: FieldSchema,
  searchIndexes: SearchIndexes,
  changedIndexes: string[]
) => {
  // here we go!
}

// needs to potentially re-index everythign
const updateHierarchy = async () => {}

async function updateTypeSchema(
  client: SelvaClient,
  props: Types,
  types: Types,
  searchIndexes: SearchIndexes
): Promise<void> {
  // need to know if new!
  let changedTypes: string[] = []
  let changedIndexes: string[] = []

  for (let type in props) {
    if (!types[type]) {
      const fields = props[type].fields
      types[type] = {}
      if (fields) {
        for (let field in fields) {
          //   console.info(field)
          parseField(type, fields[field], searchIndexes, changedIndexes)
        }
      }
      changedTypes.push(type)
    } else {
      console.log('type exists tricky!')
      // never change type, only make new (merge merge)
    }
  }

  await Promise.all(
    changedTypes.map(type => {
      return client.redis.hset('types', type, JSON.stringify(types[type]))
    })
  )

  if (changedIndexes.length) {
    await client.redis.hset(
      'schema',
      'searchIndexes',
      JSON.stringify(searchIndexes)
    )
    await Promise.all(
      changedIndexes.map(index => {
        return updateIndex(client.redis, index, searchIndexes[index])
      })
    )
  }
}

export default updateTypeSchema
