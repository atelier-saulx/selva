const fs = require('fs').promises
const path = require('path')
const os = require('os')

const START_ID = 'cujpQXzXZ'
const REF_FIELD = 'children'

function find(ancestry, db, id) {
  console.log(db[id])
  const referenceStr = db[id][REF_FIELD]
  if (!referenceStr || referenceStr == '' || referenceStr === '{}') {
    return
  }

  const references = JSON.parse(referenceStr)
  console.log(id, '-->', references)

  const newIds = []
  for (const reference of references) {
    if (reference === id) {
      continue
    }

    let count = ancestry[reference]
    if (!count) {
      count = 0
    }

    ancestry[reference] = count + 1
    if (ancestry[reference] > 100) {
      console.log('COUNT', Object.keys(ancestry).length)
      for (const ancestor in ancestry) {
        console.log('ancestor', ancestor, ancestry[ancestor])
      }

      return
    }

    newIds.push(reference)
  }

  for (const id of newIds) {
    find(ancestry, db, id)
  }
}

async function run() {
  const dump = JSON.parse(
    await fs.readFile(path.join(os.homedir(), 'Downloads', 'dump-last.json'))
  )

  for (const db of dump) {
    find({}, db, START_ID)
  }
}

run().catch(e => {
  console.error(e)
})
