import { BigQuery, TableMetadata } from '@google-cloud/bigquery'
import { BigQueryWriteClient, protos } from '@google-cloud/bigquery-storage'
import jspb from 'google-protobuf'
import { SELVA_TO_SQL_TYPE, TimeseriesContext } from '../../timeseries'

let writeClient: BigQueryWriteClient
const fieldDescriptorTypes = protos.google.protobuf.FieldDescriptorProto.Type
const writeStreamModes =
  protos.google.cloud.bigquery.storage.v1.WriteStream.Type

const createStream = async (
  projectId: string,
  datasetId: string,
  tableId: string
) => {
  const parent = `projects/${projectId}/datasets/${datasetId}/tables/${tableId}`

  writeClient = writeClient || new BigQueryWriteClient()
  const [response] = await writeClient.createWriteStream({
    parent,
    writeStream: { type: writeStreamModes.PENDING },
  })

  const writeStream = response.name
  const stream = writeClient.appendRows({
    otherArgs: {
      headers: {
        'x-goog-request-params': `write_stream=${writeStream}`,
      },
    },
  })

  return { stream, writeStream }
}

const serializeRows = (
  data: any[],
  schema: { name: string; type: string }[]
) => {
  let serializedRows: any[] = []

  // data.forEach((item) => {
  //   let writer = new jspb.BinaryWriter()
  //   writer.writeString(1, item.name)
  //   serializedRows.push(writer.getResultBuffer())
  // })

  try {
    data.forEach((item) => {
      let writer = new jspb.BinaryWriter()

      schema.forEach((field, index) => {
        if (item[field.name]) {
          if (field.type === 'STRING') {
            writer.writeString(index + 1, String(item[field.name]))
          } else if (field.type === 'INT64') {
            writer.writeInt64(index + 1, Number(item[field.name]))
          } else if (field.type === 'FLOAT64') {
            writer.writeFloat(index + 1, Number(item[field.name]))
          } else if (field.type === 'TIMESTAMP') {
            writer.writeString(
              index + 1,
              new Date(item[field.name]).toISOString()
            )
          } else if (field.type === 'JSON') {
            let value: string
            if (typeof item[field.name] === 'object') {
              value = JSON.stringify(item[field.name])
            } else {
              value = String(item[field.name])
            }
            writer.writeString(index + 1, value)
          } else {
            new Error("Can't convert type for protobuf encoding")
          }
        }
      })
      serializedRows.push(writer.getResultBuffer())
    })
  } catch (error) {
    console.error(`Error encoding fields`)
    console.error(error)
    throw error
  }

  return serializedRows
}
const makeWriteSchema = (schema: { name: string; type: string }[]) => {
  const fields = schema.map((field, index) => {
    let type: number
    if (
      field.type === 'STRING' ||
      field.type === 'JSON' ||
      field.type === 'TIMESTAMP'
    )
      type = fieldDescriptorTypes.TYPE_STRING
    else if (field.type === 'INT64') type = fieldDescriptorTypes.TYPE_INT64
    else if (field.type === 'FLOAT64') type = fieldDescriptorTypes.TYPE_FLOAT
    else throw new Error("Can't convert type for writeSchema")
    return {
      name: field.name,
      number: index + 1,
      type,
    }
  })
  const writerSchema = {
    protoDescriptor: {
      name: 'AnalyticsStreamItem',
      field: fields,
    },
  }

  return writerSchema
}

export function getTableName(tsCtx: TimeseriesContext): string {
  return tsCtx.nodeType + '___' + tsCtx.field
}

class BQConnection {
  private client: BigQuery

  constructor() {
    if (!process.env.GOOGLE_APPLICATION_CREDENTIALS) {
      console.warn(
        'GOOGLE_APPLICATION_CREDENTIALS must be set as an env variable'
      )

      return
    }

    this.client = new BigQuery()
  }

  public async execute(query: string, params: unknown[]): Promise<any[]> {
    console.log('SQL', query, params)

    const [job] = await this.client.createQueryJob({
      query,
      location: 'europe-west3',
      params,
    })
    const [rows] = await job.getQueryResults()

    return rows
  }

  public async insertStream(tsCtx: TimeseriesContext, data: any[]) {
    const bqSchema = [
      { name: 'nodeId', type: 'STRING', mode: 'REQUIRED' },
      { name: 'payload', type: SELVA_TO_SQL_TYPE[tsCtx.fieldSchema.type] },
      { name: 'ts', type: 'TIMESTAMP', mode: 'REQUIRED' },
      { name: 'fieldSchema', type: 'JSON', mode: 'REQUIRED' },
    ]
    const tableName = getTableName(tsCtx)

    const serializedRows = serializeRows(data, bqSchema)

    const { stream, writeStream } = await createStream(
      'based-310210',
      'selva_timeseries',
      tableName
    )

    const responses: any[] = []
    const errorIdx: number[] = []
    stream.on('data', (resp) => {
      console.log('RESP', resp)

      if (resp.rowErrors.length) {
        const idx = Number(resp?.appendResult?.offset?.value || -1)
        if (idx >= 0) {
          errorIdx.push(idx)
        }
      }

      responses.push(responses)

      if (responses.length === data.length) {
        stream.end()
      }
    })

    const p = new Promise((resolve, reject) => {
      stream.on('end', async () => {
        console.log('stream end')
        // API call completed.
        try {
          const [{ rowCount }] = await writeClient.finalizeWriteStream({
            name: writeStream,
          })
          console.log(`Row count: ${rowCount}`)
          const [response] = await writeClient.batchCommitWriteStreams({
            parent: `projects/based-310210/datasets/selva_timeseries/tables/${tableName}`,
            writeStreams: [writeStream],
          })

          console.log(response)
          resolve(response)
        } catch (err) {
          console.log('COMMIT ERROR', err)
          reject(err)
        }
      })
    })

    const writerSchema = makeWriteSchema(bqSchema)

    try {
      stream.write({
        writeStream,
        protoRows: {
          writerSchema,
          rows: { serializedRows },
        },
      })
    } catch (err) {
      console.error('WRITE ERROR', err)
      throw err
    }

    await p

    return responses
  }

  public async insert(tsCtx: TimeseriesContext, data: any[]) {
    const transformed = data.map((d) => {
      const { nodeId, payload, ts } = d
      return {
        nodeId,
        ts: BigQuery.datetime(new Date(ts).toISOString()),
        payload:
          typeof payload === 'object' ? JSON.stringify(payload) : payload,
        fieldSchema: JSON.stringify(tsCtx.fieldSchema || {}),
      }
    })

    console.log('INSERTINg', JSON.stringify(transformed, null, 2))

    return this.client
      .dataset('selva_timeseries', { location: 'europe-west3' })
      .table(getTableName(tsCtx))
      .insert(transformed)
  }

  public async createTable(tsCtx: TimeseriesContext) {
    const bqSchema = [
      { name: 'nodeId', type: 'STRING', mode: 'REQUIRED' },
      { name: 'payload', type: SELVA_TO_SQL_TYPE[tsCtx.fieldSchema.type] },
      { name: 'ts', type: 'TIMESTAMP', mode: 'REQUIRED' },
      { name: 'fieldSchema', type: 'JSON', mode: 'REQUIRED' },
    ]

    const schema = {
      schema: bqSchema,
      location: 'europe-west3',
      timePartitioning: { type: 'DAY', field: 'ts' },
    }

    return this.client
      .dataset('selva_timeseries', { location: 'europe-west3' })
      .createTable(getTableName(tsCtx), schema)
  }
}

export default BQConnection
