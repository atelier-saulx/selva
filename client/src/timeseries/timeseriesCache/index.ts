import { constants, SelvaClient, ServerDescriptor } from '../..'

// TODO: increase
const INDEX_REFRESH_TIMEOUT = 1e3 * 60 * 0.5

class TimeseriesCache {
  private client: SelvaClient
  public index: {
    [timeseriesName: string]: {
      [timestamp: number]: { descriptor: ServerDescriptor; meta: any }
    }
  }
  public instances: {
    [instanceId: string]: { descriptor: ServerDescriptor; meta: any }
  }
  private refreshTimer: NodeJS.Timeout

  constructor(client: SelvaClient) {
    this.client = client
    this.index = {}
    this.instances = {}
  }

  async refreshIndex() {
    console.log('REFRESHING INDEX')
    try {
      const current = await this.client.redis.hgetall(
        { type: 'timeseriesRegistry' },
        'servers'
      )
      console.log('CURRENT', current)

      if (!current) {
        throw new Error()
      }

      this.index = {}
      for (const id in current) {
        this.updateIndexByInstance(id, JSON.parse(current[id]))
      }
    } catch (e) {
      console.error('HEYOO ERROR', e)
    } finally {
      this.refreshTimer = setTimeout(() => {
        this.refreshIndex()
      }, INDEX_REFRESH_TIMEOUT)
    }
  }

  updateIndexByInstance(id: string, payload: any) {
    const [host, portStr] = id.split(':')
    const port = Number(portStr)

    const tableMeta = payload?.stats?.tableMeta
    if (!tableMeta) {
      return
    }

    const { cpu, memory, timestamp } = payload.stats

    let totalTableSizeBytes = 0
    let totalRelationSizeBytes = 0
    for (const tableName in tableMeta) {
      const { tableSizeBytes, relationSizeBytes } = tableMeta[tableName]

      const [nodeType, fieldName, startTimeStr] = tableName.split('$')
      const timeSeriesName = `${nodeType}$${fieldName}`
      const startTime = Number(startTimeStr)

      let timeseries = this.index[timeSeriesName]
      if (!timeseries) {
        timeseries = this.index[timeSeriesName] = {}
      }

      timeseries[startTime] = {
        meta: {
          size: { tableSizeBytes, relationSizeBytes },
          resources: {
            cpu,
            memory,
          },
          timestamp,
        },
        descriptor: {
          host,
          port,
          type: 'timeseries',
        },
      }

      console.log('table things', tableName, timeseries)
      totalTableSizeBytes += tableSizeBytes
      totalRelationSizeBytes += relationSizeBytes
    }

    this.instances[id] = {
      meta: {
        size: { totalTableSizeBytes, totalRelationSizeBytes },
        resources: {
          cpu,
          memory,
        },
        timestamp,
      },
      descriptor: {
        host,
        port,
        type: 'timeseries',
      },
    }
  }

  async subscribe() {
    await this.refreshIndex()

    console.log('subscribing.......................')
    this.client.redis.subscribe(
      { type: 'timeseriesRegistry' },
      constants.TS_REGISTRY_UPDATE
    )

    this.client.redis.on(
      { type: 'timeseriesRegistry' },
      'message',
      (channel, msg) => {
        console.log('WHAT IS THIS', channel, msg)
        if (channel !== constants.TS_REGISTRY_UPDATE) {
          return
        }

        console.log('HELLO GOT EVENT', channel, msg)
        let obj
        try {
          obj = JSON.parse(msg)
        } catch (e) {
          console.error('Invalid registry update payload', e)
          return
        }

        if (['new_server', 'stats_update'].includes(obj.event)) {
          this.updateIndexByInstance(obj.id, obj.data)
        } else if (obj.event === 'new_shard') {
          this.updateIndexByInstance(obj.id, obj.data)
        }
      }
    )
  }

  unsubscribe() {
    clearTimeout(this.refreshTimer)
    this.refreshTimer = undefined

    this.client.redis.unsubscribe(
      { type: 'timeseriesRegistry' },
      constants.TS_REGISTRY_UPDATE
    )

    this.client.redis.removeListener({ type: 'timeseriesRegistry' }, 'message')
  }
}

export default TimeseriesCache
