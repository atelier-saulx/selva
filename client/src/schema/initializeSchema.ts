import { SelvaClient } from '..'

const initlializeSchema = async (
  client: SelvaClient,
  opts: any,
  waitForSchema: boolean = true
) => {
  const dbName = (typeof opts === 'object' && opts.$db) || 'default'

  if (waitForSchema && !client.schemas[dbName]) {
    const obs = client.subscribeSchema(dbName)

    const p: Promise<void> = new Promise((resolve, reject) => {
      const sub = obs.subscribe(
        () => {
          resolve()
          sub.unsubscribe()
        },
        (e) => {
          reject(e)
        }
      )
    })

    return p
  }

  if (!client.schemas[dbName]) {
    await client.getSchema(dbName)
  }

  if (!client.schemaObservables[dbName]) {
    client.subscribeSchema(dbName)
  }
}

export default initlializeSchema
