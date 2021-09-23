import { SelvaClient, ServerDescriptor } from '..'

const removeServer = (selvaClient: SelvaClient, server: ServerDescriptor) => {
  const id = `${server.host}:${server.port}`
  if (selvaClient.servers.ids.has(id)) {
    const { type } = server
    selvaClient.servers.ids.delete(id)

    if (type === 'origin') {
      delete selvaClient.servers.origins[server.name]
    } else if (type === 'replica') {
      const replicas = selvaClient.servers.replicas[server.name]
      if (replicas) {
        for (let i = 0; i < replicas.length; i++) {
          const replica = replicas[i]
          // eslint-disable-next-line
          if (replica.host === server.host && replica.port == server.port) {
            replicas.splice(i, 1)
            break
          }
        }
        if (replicas.length === 0) {
          delete selvaClient.servers.replicas[server.name]
        }
      }
    } else if (type === 'subscriptionManager') {
      const subsManagers = selvaClient.servers.subsManagers
      for (let i = 0; i < subsManagers.length; i++) {
        const subscriptionManager = subsManagers[i]
        if (
          subscriptionManager.host === server.host &&
          // eslint-disable-next-line
          subscriptionManager.port == server.port
        ) {
          subsManagers.splice(i, 1)
          break
        }
      }
    } else if (type === 'subscriptionRegistry') {
      delete selvaClient.servers.subRegisters[id]
    } else if (type === 'timeseriesRegistry') {
      delete selvaClient.servers.tsRegisters[id]
    } else if (type === 'timeseries') {
      delete selvaClient.servers.timeseries[id]
    }
    return true
  } else {
    // console.warn('💁🏻 Remove server id does not exist', server)
  }
}

export default removeServer
