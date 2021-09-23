import { SelvaClient, ServerDescriptor } from '..'

const addServer = (selvaClient: SelvaClient, server: ServerDescriptor) => {
  const id = `${server.host}:${server.port}`
  if (!selvaClient.servers.ids.has(id)) {
    const { type } = server
    selvaClient.servers.ids.add(id)
    if (type === 'origin') {
      selvaClient.servers.origins[server.name] = server
    } else if (type === 'replica') {
      if (!selvaClient.servers.replicas[server.name]) {
        selvaClient.servers.replicas[server.name] = []
      }
      // double check if this is correct
      selvaClient.servers.replicas[server.name].splice(
        server.index === -1 ? 0 : server.index || 0,
        0,
        server
      )
    } else if (type === 'subscriptionManager') {
      selvaClient.servers.subsManagers.splice(
        server.index === -1 ? 0 : server.index || 0,
        0,
        server
      )
    } else if (type === 'subscriptionRegistry') {
      selvaClient.servers.subRegisters[id] = server
    } else if (type === 'timeseriesQueue') {
      selvaClient.servers.timeseriesQueues[id] = server
    } else if (type === 'timeseriesRegistry') {
      selvaClient.servers.tsRegisters[id] = server
    } else if (type === 'timeseries') {
      // different then sub managers scince you just want to use the tsReg to see where to connect to
      selvaClient.servers.timeseries[id] = server
    }
    return true
  } else {
    // console.warn('💁🏻 Add server id allready exist', server)
  }
}

export default addServer
