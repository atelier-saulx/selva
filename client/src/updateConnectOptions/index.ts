import { SelvaClient, ConnectOptions } from '..'
import { createConnection } from '../connection'

export default (selvaClient: SelvaClient, connectOptions: ConnectOptions) => {
  console.log('_ _ _ _ connect make options do it')
  if (connectOptions instanceof Promise) {
  } else if (typeof connectOptions === 'function') {
  } else {
    const { port = 6379, host = '0.0.0.0' } = connectOptions

    if (selvaClient.registryConnection) {
      console.log('update existing connection to registry')
    } else {
      selvaClient.registryConnection = createConnection({
        type: 'registry',
        name: 'registry',
        port,
        host
      })

      console.log('ok made start of registry connection')
    }
  }
}
