import { getSchema, getSearchIndexes } from '../schema/index'
import * as logger from '../logger'

export function addFieldToSearch(
  id: string,
  field: string,
  value: string
): void {
  const searchIndex = getSearchIndexes()
  const schema = getSchema()
  //   logger.info(schema)
  // first check if schema has it defined
  // then add it here
  //   logger.info(searchIndex)
  for (const indexKey in searchIndex) {
    const index = searchIndex[indexKey]
    if (index[field]) {
      logger.info('add to index ' + id + ' ' + field)
      redis.call(
        'ft.add',
        indexKey,
        id,
        '1',
        'NOSAVE',
        'REPLACE',
        'PARTIAL',
        'FIELDS',
        field,
        value
      )
    }
  }
}
