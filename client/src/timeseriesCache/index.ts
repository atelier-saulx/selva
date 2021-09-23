import { constants, SelvaClient, ServerDescriptor } from '..'

const INDEX_REFRESH_TIMEOUT = 1e3 * 60 * 1

class TimeseriesCache {
  private client: SelvaClient
  private index: {
    [timeseriesName: string]: {
      [timestamp: number]: { descriptor: ServerDescriptor; meta: any }
    }
  }
  private refreshTimer: NodeJS.Timeout

  constructor(client: SelvaClient) {
    this.client = client
    this.index = {}
  }

  async refreshIndex() {
    const current = await this.client.redis.hgetall(
      { type: 'timeseriesRegistry' },
      'servers'
    )

    this.index = {}
    for (const id in current) {
      this.updateIndexByInstance(id, JSON.parse(current[id]))
    }

    this.refreshTimer = setTimeout(() => {
      this.refreshIndex()
    }, INDEX_REFRESH_TIMEOUT)
  }

  updateIndexByInstance(id: string, payload: any) {
    const [host, portStr] = id.split(':')
    const port = Number(portStr)

    const tableMeta = payload?.stats?.tableMeta
    if (!tableMeta) {
      return
    }

    for (const tableName in tableMeta) {
      const { tableSizeBytes, relationSizeBytes } = tableMeta[tableName]
      const { cpu, memory, timestamp } = payload.stats

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
    }
  }

  async subscribe() {
    await this.refreshIndex()

    this.client.redis.subscribe(
      { type: 'timeseriesRegistry' },
      constants.TS_REGISTRY_UPDATE
    )

    this.client.redis.on(
      { type: 'timeseriesRegistry' },
      'message',
      (channel, msg) => {
        if (channel !== constants.TS_REGISTRY_UPDATE) {
          return
        }

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
          // TODO: this should be sent by timeseriesWorker if it creates a new shard based on current allocation
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
