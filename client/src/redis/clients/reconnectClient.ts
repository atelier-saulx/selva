import {
  getClient,
  RedisSubscriptions,
  addCommandToQueue,
  destroyClient
} from './'
import handleListenerClient from './handleListenerClient'
import getServerDescriptor from '../getServerDescriptor'

const mergeSubscriptions = (
  oldSubs: RedisSubscriptions,
  newSubs: RedisSubscriptions
) => {
  for (let key in oldSubs.psubscribe) {
    newSubs.psubscribe[key] = true
  }
  for (let key in oldSubs.subscribe) {
    newSubs.subscribe[key] = true
  }
}

const reconnectClient = (client, retry: number = 0) => {
  const { type, name } = client
  const clients = [...client.clients.values()]
  const aSelvaClient = clients[0]

  const q = [...client.queue, ...client.queueBeingDrained]

  if (!aSelvaClient) {
    console.error(
      'Ok dont have a selva client in reconnectClient thats a problem!',
      client.clients
    )
    return
  }

  // need a Selva client here to get a registry
  getServerDescriptor(aSelvaClient, {
    type,
    name
  }).then(descriptor => {
    if (descriptor.host + ':' + descriptor.port === client.id && retry < 5) {
      setTimeout(() => {
        reconnectClient(client, retry + 1)
      }, 1e3)
      return
    }

    let newClient
    clients.forEach(selvaClient => {
      setTimeout(() => {
        selvaClient.selvaClient.emit('reconnect', descriptor)
      }, 500)
      newClient = getClient(selvaClient, descriptor)
    })

    for (let event in client.redisListeners) {
      client.redisListeners[event].forEach(callback => {
        handleListenerClient(newClient, 'on', event, callback)
      })
    }

    mergeSubscriptions(client.redisSubscriptions, newClient.redisSubscriptions)

    for (const key in newClient.redisSubscriptions.subscribe) {
      addCommandToQueue(newClient, { command: 'subscribe', args: [key] })
    }

    for (const key in newClient.redisSubscriptions.psubscribe) {
      addCommandToQueue(newClient, {
        command: 'psubscribe',
        args: [key]
      })
    }

    q.forEach(command => {
      addCommandToQueue(newClient, command)
    })

    destroyClient(client)
  })
}

export default reconnectClient
