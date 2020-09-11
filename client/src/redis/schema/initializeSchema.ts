import { SelvaClient } from '..'

const initlializeSchema = async (client: SelvaClient, opts: any) => {
  const dbName = (typeof opts === 'object' && opts.$db) || 'default'

  if (!client.schemas[dbName]) {
    await client.getSchema(dbName)
  }

  if (!client.schemaObservables[dbName]) {
    client.schemaObservables[dbName] = client.subscribeSchema(dbName)
    client.schemaObservables[dbName].subscribe((v: any) => {
      // console.log('Update schema subscription --->', dbName, v)
      client.schemas[dbName] = v
    })
  }
}

export default initlializeSchema
