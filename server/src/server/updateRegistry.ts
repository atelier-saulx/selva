import { SelvaClient } from '@saulx/selva'
import { RegistryInfo } from '../types'

export default async function updateRegistry(
  client: SelvaClient,
  info: RegistryInfo
) {
  console.log('hello', info)
}
