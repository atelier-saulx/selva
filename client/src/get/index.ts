import { SelvaClient } from '..'
import { GetResult, GetOptions } from './types'
import validate from './verify'

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  validate(client, props)
  const getResult = await client.fetch(props)
  // TODO: verify props
  return getResult
}

export { get, GetResult, GetOptions }
