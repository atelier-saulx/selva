const { connect } = require('@saulx/selva')
const fs = require('fs').promises
const os = require('os')
const path = require('path')
const _ = require('lodash')

const clean = data => {
  const blacklist = new Set()
  const checkBad = (item, blacklist) => {
    if (blacklist.has(item.id)) {
      return true
    }
    if (!Number(item.published)) {
      if (
        !item.parents ||
        !JSON.parse(item.parents).length ||
        /"Copy of /.test(item.title)
      ) {
        blacklist.add(item.id)
        return true
      }
      let keep
      for (let i in item) {
        if (/:from$/.test(i) && i !== 'published:from') {
          const [origin] = item[i].split('-')
          if (origin === 'set') keep = true
        }
      }
      if (!keep) {
        try {
          keep = JSON.parse(item.children).find(id => {
            const bad = checkBad(data[id], blacklist)
            return !bad
          })
        } catch (e) {}
      }
      if (!keep) {
        blacklist.add(item.id)
        return true
      }
    }
  }

  for (const id in data) {
    const item = data[id]
    if (!item.type || item.type === 'location' || item.type === 'sponsorship') {
      continue
    }
    if (checkBad(item, blacklist)) {
      delete data[id]
    }
  }

  for (const id in data) {
    const item = data[id]
    try {
      item.parents = JSON.stringify(
        JSON.parse(item.parents).filter(id => !blacklist.has(id))
      )
    } catch (e) {}
    try {
      item.children = JSON.stringify(
        JSON.parse(item.children).filter(id => !blacklist.has(id))
      )
    } catch (e) {}
  }
}

async function migrate() {
  // const srv = await start({ port: 6061 })
  const client = connect({ port: 6061 } /*, { loglevel: 'info' }*/)

  const dump = JSON.parse(
    await fs.readFile(path.join(os.homedir(), 'Downloads', 'dump-last.json'))
  )
  let db = dump[0]
  clean(db)

  let keyCount = 0
  const parentsKeys = await client.redis.keys('*.children')
  for (const key of parentsKeys) {
    const id = key.split('.')[0]

    const type = await client.redis.hget(id, 'type')
    if (!type) {
      if (!db[id] || !db[id].parents) {
        continue
      }

      if (db[id]._removed) {
        continue
      }

      const parents = JSON.parse(db[id].parents)
      if (
        parents.length <= 2 &&
        parents.includes('relnqzJe') &&
        parents.includes('root')
      ) {
        continue
      }
      console.log('TYPE MISSING IN DB', db[id], await client.redis.hgetall(id))
    }

    keyCount++
  }

  console.log('TOTAL KEYS', keyCount)

  await client.destroy()
  // await srv.destroy()
}

migrate()
  .then(() => {
    process.exit(0)
  })
  .catch(e => {
    console.error(e)
    process.exit(1)
  })
