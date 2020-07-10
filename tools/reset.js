const fs = require('fs')
const path = require('path')
const execa = require('execa')
const { start } = require('@saulx/selva-server')
const { connect } = require('@saulx/selva')

const SOURCE_DUMP =
  process.env.SOURCE_DUMP ||
  path.join(process.cwd(), 'services', 'db', 'tmp', 'dump.rdb')

const TARGET_DIR = process.env.TARGET_DIR || process.cwd()

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

function makeObj(item) {
  const obj = {}
  for (const k in item) {
    const split = k.split('.')
    let it = obj
    for (let i = 0; i < split.length - 1; i++) {
      const part = split[i]
      if (!it[part]) {
        it[part] = {}
      }

      it = it[part]
    }

    const last = split[split.length - 1]
    it[last] = item[k]
  }

  return obj
}

function makeSetPayload(db, typeSchema, entry) {
  let { id, item, ancestors, parents, children } = entry
  const payload = {}

  if (id) {
    item = makeObj(item)
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
      if (db[id + '.' + key]) {
        val = db[id + '.' + key]
      } else {
        val = undefined
      }
    } else if (
      ['int', 'float', 'number', 'timestamp'].includes(
        typeSchema.fields[key].type
      )
    ) {
      val = Number(val)
    } else if (typeSchema.fields[key].type === 'json') {
      val = JSON.parse(val)
    } else if (typeSchema.fields[key].type === 'array') {
      val = JSON.parse(val)
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
    } else if (typeSchema.fields[key].type === 'record') {
      const fakeSchema = { fields: {} }
      for (const k in val) {
        fakeSchema.fields[k] = typeSchema.fields[key].values
      }

      const newVal = makeSetPayload(db, fakeSchema, {
        item: val
      })

      val = newVal
    }

    if (typeof val !== 'undefined') {
      payload[key] = val
    }
  }

  if (children) {
    payload.children = children
  }

  if (parents) {
    payload.parents = parents
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

    if (key === 'sug' || key === 'sug_counts') {
      continue
    }

    console.log('PROCESSING', key)
    const entry = collectEntry(db, schema, key)
    const type =
      entry.item.type || schema.prefixToTypeMapping[entry.id.substr(0, 2)]
    const typeSchema = key === 'root' ? schema.rootType : schema.types[type]
    const setPayload = makeSetPayload(db, typeSchema, entry)
    await selva.client.set(setPayload)
  }

  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 10e3)
  })

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
