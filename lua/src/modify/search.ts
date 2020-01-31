import { getSchema, getSearchIndexes } from '../schema/index'
import * as logger from '../logger'

export function addFieldToSearch(
  id: string,
  field: string,
  value: string
): void {
  const searchIndex = getSearchIndexes()
  const schema = getSchema()
  // logger.info(schema)
  // first check if schema has it defined
  // then add it here
  //   logger.info(searchIndex)
  //   logger.info('add to index ' + id + ' ' + field)

  for (const indexKey in searchIndex) {
    const index = searchIndex[indexKey]
    if (index[field]) {
      logger.info('add to index ' + id + ' ' + field)

      // prob want to call this at the end of a set (all fields)
      // but ok for now
      redis.call(
        'ft.add',
        indexKey,
        id,
        '1',
        'NOSAVE',
        'REPLACE', // more difficult need to check if it needs to remove the one that points to the same id
        'PARTIAL',
        'FIELDS',
        field,
        value
      )
    }
  }
}
