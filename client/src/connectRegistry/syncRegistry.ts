import { SelvaClient } from '..'
import getInitialRegistryServers from './getInitialRegistryServer'

export const startSync = async (client: SelvaClient) => {
  // sync with registry once in a while
  if (client.registrySync?.isSyncing) {
    return true
  }
  if (!client.registrySync) {
    client.registrySync = {
      isSyncing: true,
    }
  }
  client.registrySync.isSyncing = true
  const getServers = async () => {
    await getInitialRegistryServers(client)
    if (client.registrySync?.isSyncing) {
      client.registrySync.timer = setTimeout(getServers, 15e3)
    }
  }
  client.registrySync.timer = setTimeout(getServers, 15e3)
}

export const stopSync = async (client: SelvaClient) => {
  if (client.registrySync) {
    clearTimeout(client.registrySync.timer)
    delete client.registrySync
  }
}
