import { default as RedisClient, ConnectOptions } from './redis'
import { id, IdOptions } from './id'
import { set, SetOptions } from './set'
import { deleteItem, DeleteOptions } from './delete'

export class SelvaClient {
  public redis: RedisClient

  constructor(opts: ConnectOptions | (() => Promise<ConnectOptions>)) {
    this.redis = new RedisClient(opts)
  }

  // need client.destroy (at least for tests)

  id(props: IdOptions) {
    return id(this, props)
  }

  set(props: SetOptions) {
    return set(this, props)
  }

  delete(props: DeleteOptions) {
    let hierarchy = true
    let id: string
    if (typeof props == 'object') {
      id = props.$id
      if (props.$hierarchy === false) {
        hierarchy = false
      }
    } else {
      id = props
    }
    return deleteItem(this, id, hierarchy)
  }
}

export function connect(
  opts: ConnectOptions | (() => Promise<ConnectOptions>)
): SelvaClient {
  return new SelvaClient(opts)
}
