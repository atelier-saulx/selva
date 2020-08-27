import { SelvaClient, ServerDescriptor } from '..'

const removeServer = (selvaClient: SelvaClient, server: ServerDescriptor) => {
  const id = `${server.host}:${server.port}`
  if (selvaClient.servers.ids.has(id)) {
    const { type } = server
    selvaClient.servers.ids.delete(id)
    if (type === 'origin') {
      delete selvaClient.servers.origins[id]
    } else if (type === 'replica') {
    } else if (type === 'subscriptionManager') {
    }
    return true
  } else {
    console.warn('💁🏻 Remove server id does not exist', server)
  }
}

export default removeServer
