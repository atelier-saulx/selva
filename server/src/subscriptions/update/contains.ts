// const createBatch = (subsManager: SubscriptionManager) => {
//     execBatch = subsManager.client.redis.redis.client.batch()
//     process.nextTick(() => {
//       execBatch.exec((err, d) => {
//         if (err) {
//           console.error(err)
//         } else {
//           d.forEach((m, i) => {
//             const field = fieldsInBatch[i]
//             const members = (memberMemCache[field] = {})
//             m.forEach(v => (members[v] = true))
//             const listeners = fieldsProgress[field]
//             for (let i = 0; i < listeners.length - 1; i += 2) {
//               const v = listeners[i + 1]
//               console.log('totally', v, members, m)
//               if (members[v]) {
//                 addToUpdateQueue(subsManager, listeners[i])
//               }
//             }
//           })
//         }
//         fieldsProgress = {}
//         fieldsInBatch = []
//         execBatch = undefined
//       })
//     })
//   }

//   const addAncestorsToBatch = (
//     subsManager: SubscriptionManager,
//     key: string,
//     field: string,
//     v: string
//   ) => {
//     if (!execBatch) {
//       createBatch(subsManager)
//     }

//     if (!fieldsProgress[field]) {
//       console.log('go ', field, key, v)
//       execBatch.zrange(field, 0, -1)
//       fieldsInBatch.push(field)
//       fieldsProgress[field] = [key, v]
//     } else {
//       fieldsProgress[field].push(key, v)
//     }
//   }

//   const addMembersToBatch = (
//     subsManager: SubscriptionManager,
//     key: string,
//     field: string,
//     v: string
//   ) => {
//     if (!execBatch) {
//       createBatch(subsManager)
//     }
//     if (!fieldsProgress[field]) {
//       execBatch.smembers(field)
//       fieldsInBatch.push(field)
//       fieldsProgress[field] = [key, v]
//     } else {
//       fieldsProgress[field].push(key, v)
//     }
//   }

//   const membersContainsId = (
//     subsManager: SubscriptionManager,
//     id: string,
//     item: QuerySubscription,
//     key: string
//   ): boolean => {
//     const member = item.member
//     for (let j = 0; j < member.length; j++) {
//       const m = member[j]
//       const value = m.$value

//       if (m.$field === 'ancestors') {
//         for (let k = 0; k < value.length; k++) {
//           const v = value[k]

//           console.log(id, value, k)
//           if (v === id) {
//             return false
//           }
//           console.log('ok check if ', v, 'is in ancestors.', id)

//           if (v === 'root') {
//             return true
//           }
//           const field = `${id}.ancestors`
//           let f = memberMemCache[field]
//           if (!f) {
//             addAncestorsToBatch(subsManager, key, field, v)
//           } else if (f[v]) {
//             return true
//           }
//         }
//       } else {
//         for (let k = 0; k < value.length; k++) {
//           const v = value[k]
//           const field = `${id}.${m.$field}`
//           let f = memberMemCache[field]
//           if (!f) {
//             addMembersToBatch(subsManager, key, field, v)
//           } else if (f[v]) {
//             return true
//           }
//         }
//       }
//     }
//     return false
//   }

const contains = (subManager, contains, id, branch) => {
  // branch is a set of subscriptions
}

export default contains
