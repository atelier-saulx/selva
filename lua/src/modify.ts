import * as redis from './redis'

export default function modify(): string[] {
  const pong = redis.ping()
  redis.hset('my_testkey', 'my_dingdong', 'dk_donkeykong')
  redis.hset('my_testkey', 'my_dongding', 'dk_diddykong')

  const resultAry = []
  const hashKeys = redis.hkeys('my_testkey')
  for (let i = 0; i < hashKeys.length; i++) {
    resultAry[i] = hashKeys[i]
  }
  let totalLength = hashKeys.length

  resultAry[totalLength] = pong
  totalLength++

  const hashValues = redis.hmget('my_testkey', 'my_dingdong', 'my_dongding')
  for (let i = 0; i < hashValues.length; i++) {
    resultAry[totalLength + i] = hashValues[i]
  }

  return resultAry
}
