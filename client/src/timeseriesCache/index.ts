import { constants, SelvaClient, ServerDescriptor } from '..'

class TimeseriesCache {
  private client: SelvaClient
  private index: {
    [timeseriesName: string]: {
      [timestamp: number]: { descriptor: ServerDescriptor; meta: any }
    }
  }

  constructor(client: SelvaClient) {
    this.client = client
    this.index = {}
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
    const current = await this.client.redis.hgetall(
      { type: 'timeseriesRegistry' },
      'servers'
    )

    for (const id in current) {
      this.updateIndexByInstance(id, JSON.parse(current[id]))
    }

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
          // TODO
        } catch (e) {
          console.error('Invalid registry update payload', e)
          return
        }
      }
    )
  }

  unsubscribe() {
    this.client.redis.unsubscribe(
      { type: 'timeseriesRegistry' },
      constants.TS_REGISTRY_UPDATE
    )

    this.client.redis.removeListener({ type: 'timeseriesRegistry' }, 'message')
  }
}

export default TimeseriesCache
