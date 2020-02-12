import { Meta, QuerySubscription } from './types'
import * as logger from '../../logger'

function parseSubscriptions(querySubs: QuerySubscription[], meta: Meta) {
  const sub: QuerySubscription = {
    member: [],
    fields: {}
  }

  if (meta.ast) {
    logger.info(meta.ast)
  }
}

export default parseSubscriptions
