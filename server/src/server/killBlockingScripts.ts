import { createClient } from 'redis'

const killBlockingScripts = (host, port) => {
  console.log('Start SCRIPT KILL monitor', host, port)
  const directRedisAccess = createClient({
    host,
    port,
    retry_strategy: () => 1e3,
    no_ready_check: true
  })
  const infoTest = () => {
    directRedisAccess.info((err, res) => {
      if (
        err &&
        err.message.indexOf('BUSY Redis is busy running a script.') !== -1
      ) {
        console.warn('Got busy signal. Killing script.')
        directRedisAccess.script('KILL', (err, res) => {})
      }
    })

    setTimeout(infoTest, 1e3)
  }
  infoTest()
}

export default killBlockingScripts
