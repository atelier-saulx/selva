import { ServerDescriptor } from './types'

import { SelvaClient } from '.'

export const wait = (t: number = 0): Promise<void> => {
  // eslint-disable-next-line
  return new Promise((r) => setTimeout(r, t))
}

export const waitUntilEvent = (
  selvaClient: SelvaClient,
  event: string
): Promise<void> => {
  return new Promise((resolve) => {
    selvaClient.once(event, () => {
      resolve()
    })
  })
}

export const isEmptyObject = (obj: { [key: string]: any }): boolean => {
  // eslint-disable-next-line
  for (const _k in obj) {
    return false
  }
  return true
}

export const FIELD_REGEX = /^[a-zA-Z0-9]{1}[a-zA-Z0-9-_]*$/
export const FILTER_FIELD_REGEX = /^[a-zA-Z0-9]{1}[a-zA-Z0-9-_.]*$/

export function validateFieldName(path: string, fieldName: string) {
  if (!FIELD_REGEX.test(fieldName)) {
    throw new Error(
      `${path}.${fieldName} contains unsupported characters for field names, supported characters are a-z, A-Z, 0-9, - and _`
    )
  }
}

export function validateFieldPath(fieldName: string) {
  if (!FILTER_FIELD_REGEX.test(fieldName)) {
    throw new Error(
      `${fieldName} contains unsupported characters for field names, supported characters are a-z, A-Z, 0-9, - and _`
    )
  }
}

export const serverId = (serverDescriptor: ServerDescriptor): string => {
  return serverDescriptor.host + ':' + serverDescriptor.port
}

export const padId = (id: string): string => {
  return id.padEnd(10, '\0')
}

export const joinIds = (ids: string[]): string =>
  ids.map((id) => padId(id)).join('')

export const NODE_ID_SIZE = 10
export const EMPTY_ID = '\0'.repeat(10)
