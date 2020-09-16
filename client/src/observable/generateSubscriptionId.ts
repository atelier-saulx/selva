import { GetOptions } from '../get'
import { createHash } from 'crypto'
import { ObservableOptions } from './types'

const generateSubscriptionId = (opts: ObservableOptions) => {
  if (opts.type === 'get') {
    const hash = createHash('sha256')
    hash.update(JSON.stringify(opts.options))
    return hash.digest('hex')
  } else if (opts.type === 'schema') {
    return 'SCHEMA_SUBS_' + opts.db
  }
}

export default generateSubscriptionId
