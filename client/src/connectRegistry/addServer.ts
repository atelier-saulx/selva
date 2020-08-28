import { SelvaClient, ServerDescriptor } from '..'

const addServer = (selvaClient: SelvaClient, server: ServerDescriptor) => {
  const id = `${server.host}:${server.port}`
  if (!selvaClient.servers.ids.has(id)) {
    const { type } = server
    selvaClient.servers.ids.add(id)
    if (type === 'origin') {
      selvaClient.servers.origins[server.name] = server
    } else if (type === 'replica') {
    } else if (type === 'subscriptionManager') {
    }
    return true
  } else {
    console.warn('💁🏻 Add server id allready exist', server)
  }
}

export default addServer
