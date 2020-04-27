import { SelvaClient } from '..'
import { GetResult, GetOptions } from './types'

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const getResult = await client.fetch(props)
  // TODO: verify props
  return getResult
}

export { get, GetResult, GetOptions }
