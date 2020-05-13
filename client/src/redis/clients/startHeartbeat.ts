import { Client } from './'
import { CLIENTS, HEARTBEAT } from '../../constants'
const HEARTBEAT_TIMER = 5e3

const startHeartbeat = (client: Client) => {
  const setHeartbeat = () => {
    if (client.connected) {
      console.log('CONNECTED')
      client.publisher.hget(CLIENTS, client.uuid, (err, r) => {
        if (!err && r) {
          if (Number(r) < Date.now() - HEARTBEAT_TIMER * 5) {
            console.log('Client timedout - re send subscriptions')
            this.sendSubcriptions()
          }
        }
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

export default startHeartbeat
