const { RedisClient } = require('redis')
require('@saulx/selva/dist/src/connection/redisClientExtensions')
const {
  typeCast,
} = require('@saulx/selva/dist/src/get/executeGetOperations/index')
const fs = require('fs').promises
const os = require('os')
const path = require('path')

async function migrate() {
  console.log('CONNECTING')

  console.log('LOADING DUMP', process.argv[2])
  const dump = JSON.parse(await fs.readFile(process.argv[2]))
  console.log('DUMP', dump)

  const redis = new RedisClient({ port: 80 })

  const results = {}

  for (let db of dump) {
    const schema = db.___selva_schema
    for (const key in schema) {
      schema[key] = JSON.parse(schema[key])
    }
    results.___selva_schema = schema

    const keys = Object.keys(db)
    for (const key of keys) {
      if (key.includes('_') || key.includes('.')) {
        continue
      }

      console.log('KEY', key)
      const data = await new Promise((resolve, reject) => {
        redis.send_command('selva.object.get', [key], (err, data) => {
          if (err) {
            return reject(err)
          }

          resolve(data)
        })
      })
      const obj = {}
      for (let i = 0; i < data.length; i += 2) {
        const field = data[i]
        const subData = data[i + 1]

        const subVal = typeCast(subData, key, field, schema.types)
        obj[field] = subVal
      }

      const [parents, children] = await Promise.all([
        new Promise((resolve, reject) =>
          redis.send_command(
            'selva.hierarchy.parents',
            ['___selva_hierarchy', key],
            (err, data) => {
              if (err) {
                return reject(err)
              }

              resolve(data)
            }
          )
        ),
        new Promise((resolve, reject) =>
          redis.send_command(
            'selva.hierarchy.children',
            ['___selva_hierarchy', key],
            (err, data) => {
              if (err) {
                return reject(err)
              }

              resolve(data)
            }
          )
        ),
      ])

      obj.parents = parents
      obj.children = children

      results[key] = obj
    }

    console.log('ITEMS', results)
    await fs.writeFile(
      path.join(__dirname, 'things.json'),
      JSON.stringify(results, null, 2),
      'utf8'
    )
  }

  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  process.exit(0)
}

migrate()
  .then(() => {
    process.exit(0)
  })
  .catch((e) => {
    console.error(e)
    process.exit(1)
  })
