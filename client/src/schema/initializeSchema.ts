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

    const timeout: Promise<void> = new Promise((resolve, _reject) => {
      setTimeout(() => {
        // let's it  fail on normal set validation by letting it proceed, so resolve is correct
        resolve()
      }, 1e3 * 20)
    })

    return Promise.race([p, timeout])
  }

  if (!client.schemas[dbName]) {
    await client.getSchema(dbName)
  }

  if (!client.schemaObservables[dbName]) {
    client.subscribeSchema(dbName)
  }
}

export default initlializeSchema
