import { SelvaClient } from '..'

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

  console.info('START SYNC', client.uuid)
}

export const stopSync = async (client: SelvaClient) => {
  console.info('STOP SYNC', client.uuid)
  clearTimeout(client.registrySync.timer)
  delete client.registrySync
}
