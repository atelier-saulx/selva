import { SelvaClient } from '.'
import { ServerSelector, ServerDescriptor, ServerSelectOptions } from './types'

const getSubInprogress = {}

const getServer = (
  selvaClient: SelvaClient,
  cb: (descriptor: ServerDescriptor) => void,
  selector: ServerSelector,
  selectionOptions?: ServerSelectOptions // channel for subscriptions
): void => {
  if (selector.host && selector.port) {
    // eslint-disable-next-line
    cb({ ...selector, host: selector.host, port: selector.port })
  } else {
    let { type, name } = selector
    if (!selvaClient.registryConnection) {
      // console.log(
      //   'registry connection not created, add once listener on selvaClient.connect (means registry is connected) '
      // )
      selvaClient.addServerUpdateListeners.push(() =>
        getServer(selvaClient, cb, selector, selectionOptions)
      )
    } else {
      let server

      // this makes sense
      if (name && !type) {
        type = 'replica'
      }

      if (type === 'timeseriesRegistry') {
        // just get first for now - never had more then one yet...
        // eslint-disable-next-line
        for (let k in selvaClient.servers.tsRegisters) {
          server = selvaClient.servers.tsRegisters[k]
          break
        }
      } else if (type === 'timeseriesQueue') {
        // just get first for now - never had more then one yet...
        // eslint-disable-next-line
        for (let k in selvaClient.servers.timeseriesQueues) {
          server = selvaClient.servers.timeseriesQueues[k]
          break
        }
      } else if (type === 'subscriptionRegistry') {
        // just get first for now
        // eslint-disable-next-line
        for (const k in selvaClient.servers.subRegisters) {
          // eslint-disable-next-line
          server = selvaClient.servers.subRegisters[k]
          break
        }
      } else if (type === 'subscriptionManager') {
        // with a timeout
        if (selectionOptions && selectionOptions.subscription) {
          let isCanceled = false
          let subReg
          // eslint-disable-next-line
          for (const k in selvaClient.servers.subRegisters) {
            // eslint-disable-next-line
            subReg = selvaClient.servers.subRegisters[k]
            break
          }

          if (subReg) {
            const timer = setTimeout(() => {
              // console.log('Timeout getting from subs registry')
              isCanceled = true
              delete getSubInprogress[selectionOptions.subscription]
              getServer(selvaClient, cb, selector)
            }, 2e3)
            if (!getSubInprogress[selectionOptions.subscription]) {
              getSubInprogress[selectionOptions.subscription] =
                selvaClient.redis.get(
                  { type: 'subscriptionRegistry' },
                  selectionOptions.subscription
                )
            }
            getSubInprogress[selectionOptions.subscription].then((serverId) => {
              if (!isCanceled) {
                if (serverId) {
                  let [host, port] = serverId.split(':')
                  port = Number(port)
                  const server = selvaClient.servers.subsManagers.find((s) => {
                    if (s.host === host && s.port === port) {
                      return true
                    }
                    return false
                  })
                  if (server) {
                    cb(server)
                  } else {
                    getServer(selvaClient, cb, selector)
                  }
                } else {
                  if (selvaClient.servers.subsManagers[0]) {
                    cb(selvaClient.servers.subsManagers[0])
                  } else {
                    getServer(selvaClient, cb, selector)
                  }
                }
                clearTimeout(timer)
                delete getSubInprogress[selectionOptions.subscription]
              }
            })
          } else {
            getServer(selvaClient, cb, selector)
          }
          return
        } else {
          server = selvaClient.servers.subsManagers[0]
        }
      } else if (type === 'registry') {
        server = selvaClient.registryConnection.serverDescriptor
      } else if (type === 'origin') {
        if (!name) {
          name = 'default'
        }
        server = selvaClient.servers.origins[name]
      } else if (type === 'replica') {
        if (!name) {
          name = 'default'
        }
        const replicas = selvaClient.servers.replicas[name]
        server = replicas && replicas[0]
        if (
          !selector.strict &&
          !server &&
          (!selectionOptions || !selectionOptions.strict)
        ) {
          server = selvaClient.servers.origins[name]
        }
      }

      // for now only subscription
      // if (selectionOptions) {
      // console.log('if selection options')
      // }

      if (!server) {
        selvaClient.addServerUpdateListeners.push(() => {
          getServer(selvaClient, cb, selector, selectionOptions)
        })
      } else {
        cb(server)
      }
    }
  }
}

export default getServer
