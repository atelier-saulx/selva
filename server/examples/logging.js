const start = require('../lib').start
const redis = require('redis')

start({ port: 6061, developmentLogging: true }).then((server) => {
  setTimeout(() => {
    const pub = redis.createClient({ port: 6061 })
    pub.publish('___selva_lua_logs', 'log log log')
    pub.publish('___selva_lua_logs', 'lekker man')
  }, 500)

  setTimeout(() => {
    server.destroy().catch((e) => {
      console.error(e)
    })
  }, 1000)
})
