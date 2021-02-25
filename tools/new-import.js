const { connect } = require('@saulx/selva')
const { start } = require('@saulx/selva-server')
const path = require('path')

async function migrate() {
  if (!process.argv[2]) {
    throw new Error(
      'You must specify the path to the import dump (JSON generated with new-import.js) as the first executable argument'
    )
  }

  const srv = await start({ port: 6061 })
  const client = connect({ port: 6061 } /*, { loglevel: 'info' }*/)

  const dump = require(path.join(process.cwd(), process.argv[2]))

  const db = process.argv[3] || 'default'

  console.log('DUMP', dump)
  const schema = dump.___selva_schema

  await client.updateSchema(schema.types, db)

  for (const key in dump) {
    if (key.startsWith('___')) {
      continue
    }

    const setOpts = Object.assign({}, dump[key])
    setOpts.$id = setOpts.id
    setOpts.$db = db
    delete setOpts.id

    if (setOpts.parents && setOpts.parents.length) {
      setOpts.parents = { $add: setOpts.parents }
    } else {
      delete setOpts.parents
    }

    if (setOpts.children && setOpts.children.length) {
      setOpts.children = { $add: setOpts.children }
    } else {
      delete setOpts.children
    }

    console.log('SET OPTS', setOpts)
    await client.set(setOpts)
  }

  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 1000e3)
  })

  await client.destroy()
  await srv.destroy()
}

migrate()
  .then(() => {
    process.exit(0)
  })
  .catch((e) => {
    console.error(e)
    process.exit(1)
  })
