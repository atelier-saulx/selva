const { start, startOrigin, startRegistry } = require('@saulx/selva-server')
const { readdir, ensureDir, copy, writeJSON, emptyDir } = require('fs-extra')
const { join } = require('path')

console.warn('This script does NOT handle NESTED REFS')

const findRefs = (fields) => {
  let obj
  for (const field in fields) {
    const { type } = fields[field]
    if (type === 'reference') {
      if (!obj) obj = {}
      obj[field] = true
    }
  }
  return obj
}

const walk = (fields) => {
  for (const field in fields) {
    fields[field].type = fields[field].type.trim()
    if (fields[field].type === 'digest') {
      console.log('digest', { field })
    }
    if (fields[field].properties) {
      walk(fields[field].properties)
    }
  }
}

const init = async () => {
  const src = join(__dirname, 'tmp/src')
  const dest = join(__dirname, 'tmp/dest')

  await ensureDir(src)
  await emptyDir(dest)

  const srcServer = await start({
    port: 3333,
    save: true,
    dir: src,
  })

  const registry = {
    port: 4777,
  }

  await startRegistry(registry)

  const destServer = await startOrigin({
    port: 6573,
    save: true,
    dir: dest,
    default: true,
    registry,
  })

  console.log('dest')

  // get src schema
  const { schema } = await srcServer.selvaClient.getSchema()
  console.log('schema')
  const { languages, rootType, types } = schema
  // create query
  const query = {
    $all: true,
    children: true,
    parents: true,
  }

  console.log('walk!!')
  for (const type in types) {
    const { fields } = types[type]
    const refs = findRefs(fields)
    if (refs) {
      Object.assign(query, refs)
    }
    // yesh
    walk(fields)
  }

  console.log('query:', query)

  // exec query
  const { descendants = [], ...rootProps } = await srcServer.selvaClient.get({
    $id: 'root',
    ...query,
    descendants: {
      ...query,
      $list: true,
    },
  })

  await destServer.selvaClient.updateSchema({ languages, rootType, types })

  //   console.info('get schema', await destServer.selvaClient.updateSchema(schema))

  await new Promise((resolve) => setTimeout(resolve, 1e3))

  // set data to dest server
  const all = [rootProps, ...descendants]
  console.info(`will now set ${all.length} items`)
  let cnt = 0
  for (const obj of all) {
    obj.$id = obj.id
    delete obj.id
    if (obj.footer && 'items' in obj.footer && obj.footer.items === undefined) {
      delete obj.footer.items
    }

    for (const i in obj) {
      if (obj[i] === undefined) {
        delete obj[i]
      }
    }

    const { password } = obj

    if (password) {
      delete obj.password
    }

    await destServer.selvaClient
      .set(obj)
      .catch((err) => console.log('ERROR', err, obj))
      .then(() => {
        if (password) {
          console.log('pw:', obj.$id, password)
          return destServer.selvaClient.redis.selva_object_set(
            obj.$id,
            'password',
            's',
            password
          )
        }
      })
    // console.info(`${++cnt} / ${all.length}`)
  }

  console.info('set is done, start save')
  await destServer.selvaClient.redis.save({ name: 'default' })
  console.error('saving complete')
  process.exit()
}

init()
