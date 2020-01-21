import { SelvaClient } from '../'
import { Schema } from './types'

async function updateSchema(
  client: SelvaClient,
  props: Schema
): Promise<boolean> {
  return true
}

export { updateSchema }
