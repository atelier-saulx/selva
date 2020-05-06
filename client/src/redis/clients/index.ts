import { RedisClient } from 'redis'
import { RedisCommand } from '../types'
import { ServerType } from '../../types'

// script shas
type client = {
  subscriber: RedisClient
  publisher: RedisClient
  buffer: RedisCommand[]
  name: string // for logging
  id: string // url:port
  connected: boolean
  bufferInProgress: boolean
  busy: boolean // can be written from the registry
  heartbeatTimout: NodeJS.Timeout
  type: ServerType
}

// subscriptions - not relevant for every client
