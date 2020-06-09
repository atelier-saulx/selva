import { Client, addCommandToQueue } from './'
import { CLIENTS, HEARTBEAT } from '../../constants'
const HEARTBEAT_TIMER = 1e3

// subscription hearthbeat
const startSubscriptionHeartbeat = (client: Client) => {
  clearTimeout(client.heartbeatTimout)
  const setHeartbeat = () => {
    if (client.connected) {
      addCommandToQueue(client, {
        command: 'hset',
        args: [CLIENTS, client.uuid, Date.now()]
      })
      client.publisher.publish(
        HEARTBEAT,
        JSON.stringify({
          client: client.uuid,
          ts: Date.now()
        })
      )
      client.heartbeatTimout = setTimeout(setHeartbeat, HEARTBEAT_TIMER)
    }
  }

  setHeartbeat()
}

export default startSubscriptionHeartbeat
