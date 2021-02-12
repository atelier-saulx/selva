import { SelvaServer } from '..'
import { constants } from '@saulx/selva'

const startServerHeartbeat = (server: SelvaServer) => {
  const setHeartbeat = () => {
    server.selvaClient.redis.publish(
      {
        host: server.host,
        port: server.port,
        type: server.type,
        name: server.name,
      },
      constants.SERVER_HEARTBEAT,
      ''
    )
    server.serverHeartbeatTimeout = setTimeout(setHeartbeat, 5e3)
  }
  setHeartbeat()
}

export default startServerHeartbeat
