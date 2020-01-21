import { SelvaClient } from '..'
import { Types, SearchIndexes } from './'
import updateIndex from './updateIndex'

async function updateTypeSchema(
  client: SelvaClient,
  props: Types,
  types: Types,
  searchIndexes: SearchIndexes
): Promise<boolean> {
  // need to know if new!
  let changed: boolean = false
  let changedIndexes: string[] = []

  for (let key in props) {
    if (!types[key]) {
      console.log('new type go')
    } else {
    }
  }

  // return
  // for ()
  for (let index of changedIndexes) {
    await updateIndex(client.redis, index, searchIndexes[index])
  }

  return changed
}

export default updateTypeSchema
