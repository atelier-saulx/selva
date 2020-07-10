const fs = require('fs')
const path = require('path')
const execa = require('execa')
const { start } = require('@saulx/selva-server')
const { connect } = require('@saulx/selva')

const SOURCE_DUMP =
  process.env.SOURCE_DUMP ||
  path.join(process.cwd(), 'services', 'db', 'tmp', 'dump.rdb')

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

async function startSelva(schema) {
  const server = await start({ port: 9019 })
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

    if (key === 'dictionary' || key === 'sug') {
      continue
    }

    const entry = collectEntry(db, schema, key)
    console.log(entry)
  }
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
