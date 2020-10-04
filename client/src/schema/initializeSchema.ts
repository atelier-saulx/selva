import { SelvaClient } from '..'

const initlializeSchema = async (client: SelvaClient, opts: any) => {
  const dbName = (typeof opts === 'object' && opts.$db) || 'default'

  if (!client.schemas[dbName]) {
    await client.getSchema(dbName)
  }

  if (!client.schemaObservables[dbName]) {
    client.subscribeSchema(dbName)
  }
}

export default initlializeSchema
