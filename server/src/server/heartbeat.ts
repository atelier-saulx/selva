import { SelvaServer } from '..'
import { constants } from '@saulx/selva'

const startServerHeartbeat = (server: SelvaServer) => {
  const setHeartbeat = () => {
    server.registry.redis.publish(
      { host: server.host, port: server.port },
      constants.SERVER_HEARTBEAT,
      ''
    )
    server.serverHeartbeatTimeout = setTimeout(setHeartbeat, 2e3)
  }
  setHeartbeat()
}

export default startServerHeartbeat
