import { ServerDescriptor } from './types'

import { SelvaClient } from '.'

export const wait = (t: number = 0): Promise<void> =>
  new Promise(r => setTimeout(r, t))

export const waitUntilEvent = (
  selvaClient: SelvaClient,
  event: string
): Promise<void> => {
  return new Promise(resolve => {
    selvaClient.once(event, () => {
      resolve()
    })
  })
}

export const isEmptyObject = (obj: { [key: string]: any }): boolean => {
  for (const k in obj) {
    return false
  }
  return true
}

export const serverId = (serverDescriptor: ServerDescriptor): string => {
  return serverDescriptor.host + ':' + serverDescriptor.port
}
