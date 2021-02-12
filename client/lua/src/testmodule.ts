import * as redis from './redis'

export function modify(): string[] {
  const pong = redis.ping()
  redis.hset('my_testkey', 'my_dingdong', 'dk_donkeykong')
  redis.hset('my_testkey', 'my_dongding', 'dk_diddykong')

  // play with arrays, building a more complex array response
  // noticed that concat includes the shim so not using it
  // also [...hashKeys, pong, ...hashValues] has a bug,
  // some values are missing
  const resultAry: any[] = []
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
  totalLength += hashValues.length

  // build a nice object
  const hashObj: Record<string, string> = {}
  const allFields = redis.hgetall('my_testkey')
  for (let i = 0; i < allFields.length; i += 2) {
    hashObj[allFields[i]] = allFields[i + 1]
  }

  resultAry[totalLength] = cjson.encode({
    PING: pong,
    hash: hashObj,
  })
  totalLength++

  for (let key in hashObj) {
    resultAry[totalLength] = hashObj[key]
    totalLength++
  }

  return resultAry
}
