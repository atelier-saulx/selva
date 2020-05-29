import { Client } from './'
import { CLIENTS, HEARTBEAT } from '../../constants'

const HEARTBEAT_TIMER = 5e3

const startHeartbeat = (client: Client) => {
  const setHeartbeat = () => {
    if (client.connected) {
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

export default startHeartbeat
