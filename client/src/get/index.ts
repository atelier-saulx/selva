import { SelvaClient } from '..'
import { GetResult, GetOptions } from './types'
import validate from './validate'

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  validate(client, props)
  // const getResult = await client.fetch(props)
  // return getResult
  return {}
}

export { get, GetResult, GetOptions }
