import { SelvaClient } from '..'
import { GetResult, GetOptions } from './types'
import validate, { ExtraQueries } from './validate'

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const extraQueries: ExtraQueries = {}
  await validate(extraQueries, client, props)
  console.log('EXTRA QUERIES', JSON.stringify(extraQueries, null, 2))
  const getResult = await client.fetch(props)
  // TODO: verify props
  return getResult
}

export { get, GetResult, GetOptions }
