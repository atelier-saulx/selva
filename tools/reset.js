const fs = require('fs')
const path = require('path')
const execa = require('execa')
const { start } = require('@saulx/selva-server')
const { connect } = require('@saulx/selva')

const SOURCE_DUMP =
  process.env.SOURCE_DUMP ||
  path.join(process.cwd(), 'services', 'db', 'tmp', 'dump.rdb')

const TARGET_DIR = proces.env.TARGET_DIR || process.cwd()

async function loadDump() {
  const { stdout, stderr } = await execa('rdb', ['-c', 'json', SOURCE_DUMP])
  const dump = JSON.parse(stdout)

  return dump[0]
}

function collectEntry(db, schema, key) {
  const item = db[key]
  const id = key
  const typeSchema = schema.types[item.type]

  const ancestors = db[key + '.ancestors']
  const parents = db[key + '.parents']
  const children = db[key + '.children']

  return { id, item, ancestors, parents, children }
}

function makeSetPayload(db, typeSchema, entry) {
  const { id, item, ancestors, parents, children } = entry
  const payload = {}

  if (id) {
    payload.$id = id
  }

  for (const key in item) {
    let val = item[key]

    if (!typeSchema.fields[key]) {
      continue
    }

    if (key.startsWith('___')) {
      continue
    }

    if (key === 'ancestors') {
      continue
    }

    if (val === '___selva_$set') {
      val = db[id + '.' + key]
    }

    // TODO: add more
    if (
      ['int', 'float', 'number', 'timestamp'].includes(
        typeSchema.fields[key].type
      )
    ) {
      val = Number(val)
    } else if (typeSchema.fields[key].type === 'boolean') {
      val = Boolean(val)
    } else if (typeSchema.fields[key].type === 'object') {
      const newVal = makeSetPayload(
        db,
        { fields: typeSchema.fields[key].properties },
        {
          item: val
        }
      )

      val = newVal
    }

    payload[key] = val
  }

  return payload
}

async function startSelva(schema) {
  const server = await start({ port: 9019, dir: TARGET_DIR })
  const client = connect({ port: 9019 })

  await client.updateSchema(schema)

  return { server, client }
}

async function main() {
  const db = await loadDump()
  const schema = JSON.parse(db['___selva_schema'].types)

  const selva = await startSelva(schema)

  for (const key in db) {
    if (/[\.:]/.test(key)) {
      continue
    }

    if (/^___/.test(key)) {
      continue
    }

    if (key === 'dictionary' || key === 'sug' || key === 'sug_counts') {
      continue
    }

    const entry = collectEntry(db, schema, key)
    const typeSchema =
      key === 'root' ? schema.rootType : schema.types[entry.item.type]
    const setPayload = makeSetPayload(db, typeSchema, entry)
    await selva.client.set(setPayload)
  }

  await selva.client.redis.save()

  await selva.server.destroy()
  await selva.client.destroy()
}

main()
  .then(() => {
    console.log('DONE')
    process.exit(0)
  })
  .catch(e => {
    console.error(e)
    process.exit(1)
  })
