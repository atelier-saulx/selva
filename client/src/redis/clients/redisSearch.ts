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

redisSearchCommands.forEach(cmd => {
  // type definition is wrong its not on the client
  // @ts-ignore
  redis.add_command(`FT.${cmd}`)
})

// @ts-ignore
redis.add_command('selva.id')
