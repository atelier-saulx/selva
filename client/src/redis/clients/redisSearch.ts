import * as redis from 'redis'

const redisSearchCommands = [
  'CREATE',
  'ADD',
  'ADDHASH',
  'ALTER',
  'INFO',
  'SEARCH',
  'AGGREGATE',
  'EXPLAIN',
  'DEL',
  'GET',
  'DROP',
  'SUGADD',
  'SUGGET',
  'SUGDEL',
  'SUGLEN',
  'SYNADD',
  'SYNUPDATE',
  'SYNDUMP',
  'SPELLCHECK',
  'DICTADD',
  'DICTDEL',
  'DICTDUMP',
  'TAGVALS',
  'CONFIG'
]

redis.RedisClient.prototype.on_error = function(err) {
  if (this.closing) {
    return
  }
  err.message =
    'Redis connection to ' + this.address + ' failed - ' + err.message
  this.connected = false
  this.ready = false
  // Only emit the error if the retry_stategy option is not set
  // if (!this.options.retry_strategy) {
  this.emit('error', err)
  // }
  // 'error' events get turned into exceptions if they aren't listened for. If the user handled this error
  // then we should try to reconnect.
  this.connection_gone('error', err)
}

redisSearchCommands.forEach(cmd => {
  // type definition is wrong its not on the client
  // @ts-ignore
  redis.add_command(`FT.${cmd}`)
})

// @ts-ignore
redis.add_command('selva.id')
// @ts-ignore
redis.add_command('selva.flurpypants')
