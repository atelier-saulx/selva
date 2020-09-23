import { createHash } from 'crypto'
import { ObservableOptions } from './types'
import { SCHEMA_SUBSCRIPTION } from '../constants'

const generateSubscriptionId = (opts: ObservableOptions) => {
  if (opts.type === 'get') {
    const hash = createHash('sha256')
    hash.update(JSON.stringify(opts.options))
    return hash.digest('hex')
  } else if (opts.type === 'schema') {
    return SCHEMA_SUBSCRIPTION + ':' + opts.db
  }
}

export default generateSubscriptionId
