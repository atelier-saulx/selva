// const addCommand = () => {
//   // dont handle this kind of stuff here
//   if (!this.registry.connection) {
//     console.log('no connection yet push to queue')
//     this.queue.push({ command, selector })
//   } else {
//     if (selector.type === 'registry' || selector.name === 'registry') {
//       // this is nessecary scince you need to start somewhere

import { SelvaClient, RedisCommand } from '.'
import { ServerSelector } from './types'

//       // if this.registry connection....

//       //   selvaClientId: string

//       // this was the get from command one
//       // this is fine with get server descriptor better even
//       // ADD COMMAND TO CONNECTION
//       addCommandToQueue(this.registry.connection, command)
//     } else {
//       getServerDescriptor(this.registry, selector).then(descriptor => {
//         addCommandToQueue(getConnection(descriptor, this.registry), command)
//       })
//     }
//   }
// }

export default (
  selvaClient: SelvaClient,
  command: RedisCommand,
  selector: ServerSelector
) => {
  // split out psubscribe, subscribe, unsubscribe, punsubscribe
  console.log('yesh', command, selector)

  if (!command.id) {
    command.id = selvaClient.selvaId
  }
}
