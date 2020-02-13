import SubscriptionManager from './index'

const handleQuery = async (
  subsManager: SubscriptionManager,
  message: string,
  eventName: string
) => {
  // create
  // ignore fields check

  // delete
  // ignore fields check

  // update

  // double check IDS

  // TYPE

  // clean up META

  if (message === 'update') {
    const parts = eventName.split('.')
    const endField = parts.slice(1).join('.')
    const field = eventName
    const id = parts[0]
    if (endField !== 'type') {
      for (let key in subsManager.queries) {
        if (subsManager.inProgress[key]) {
          continue
        }

        const q = subsManager.queries[key]
        let needsUpdate = false
        for (let i = 0; i < q.length; i++) {
          const item = q[i]
          const idFields = item.idFields
          //   const type = item.

          if (idFields && idFields[field]) {
            needsUpdate = true
            break
          }

          const ids = item.ids

          if (ids) {
            console.log('IDS', ids)
            if (!ids[id]) {
              continue
            }
          }

          const types = item.type

          if (types) {
            let notType = true

            for (let j = 0; j < types.length; j++) {
              // pass it
            }
            if (!notType) {
              continue
            }
          }

          const fields = item.fields
          const member = item.member

          let notField = true
          for (let field in fields) {
            if (field === endField) {
              notField = false
              break
            }
          }
          if (notField) {
            continue
          }

          if (!ids) {
            let contains = false
            for (let j = 0; j < member.length; j++) {
              const m = member[j]
              const value = m.$value
              if (m.$field === 'ancestors') {
                // make this a lua script perhaps -- very heavy
                for (let k = 0; k < value.length; k++) {
                  const v = value[k]
                  // becomes async shitty
                  const x = await subsManager.client.redis.command(
                    'zscore',
                    `${id}.ancestors`,
                    v
                  )
                  if (x) {
                    contains = true
                    break
                  }
                }
              } else {
                for (let k = 0; k < value.length; k++) {
                  const v = value[k]
                  const x = await subsManager.client.redis.command(
                    'smember',
                    `${id}.${m.$field}`,
                    v
                  )
                  if (x) {
                    contains = true
                    break
                  }
                }
              }
              if (contains) {
                needsUpdate = true
                break
              }
            }
            if (!contains) {
              continue
            }
          }
          needsUpdate = true
          break
        }
        if (needsUpdate) {
          subsManager.inProgress[key] = true
          subsManager.cleanUpProgress()
          setTimeout(() => {
            subsManager.sendUpdate(key).catch(e => {
              console.error(e)
            })
          }, 0)
        }
      }
    }
  }
}

export default handleQuery
