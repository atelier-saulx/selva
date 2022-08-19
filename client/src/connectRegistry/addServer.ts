import { SelvaClient, ServerDescriptor } from '..'

const addServer = (selvaClient: SelvaClient, server: ServerDescriptor) => {
  const id = `${server.host}:${server.port}`
  const { type } = server
  selvaClient.servers.ids.add(id)
  if (type === 'origin') {
    selvaClient.servers.origins[server.name] = server
  } else if (type === 'replica') {
    if (!selvaClient.servers.replicas[server.name]) {
      selvaClient.servers.replicas[server.name] = [{ ...server, index: 0 }]
    } else if (
      !selvaClient.servers.replicas[server.name].find(
        (s) =>
          s.host === server.host &&
          s.port === server.port &&
          s.type === server.type
      )
    ) {
      selvaClient.servers.replicas[server.name].splice(
        server.index === -1 ? 0 : server.index || 0,
        0,
        server
      )
    }
  } else if (type === 'subscriptionManager') {
    // TODO: should we be checking the server.ids instead?
    if (
      !selvaClient.servers.subsManagers.find(
        (s) =>
          s.host === server.host &&
          s.port === server.port &&
          s.type === server.type
      )
    ) {
      selvaClient.servers.subsManagers.splice(
        server.index === -1 ? 0 : server.index || 0,
        0,
        server
      )
    }
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
}

export default addServer
