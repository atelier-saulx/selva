const { start } = require('@saulx/selva-server')
const getPort = require('get-port')

;(async () => {
  setTimeout(() => {
    throw new Error('Throwed with timeout')
  }, 1000)
  const pid = process.pid
  const port = await getPort()
  await start({ port })
  console.info(JSON.stringify({ pid, port }))
})()
