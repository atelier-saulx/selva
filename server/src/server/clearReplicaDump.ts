import { clear } from 'pidusage'

import fs from 'fs'
import { join } from 'path'

const clearReplicaDump = (dir: string): Promise<void> =>
  new Promise(r => {
    fs.exists(dir, exists => {
      if (exists) {
        fs.readdir(dir, (_err, x) => {
          if (x && x.length) {
            const rdb = x.filter(v => /\.rdb$/.test(v))
            if (rdb.length) {
              let cnt = rdb.length
              rdb.forEach(v => {
                fs.unlink(join(dir, v), _err => {
                  cnt--
                  if (cnt === 0) {
                    console.info('Remove dump for replica', join(dir, v))
                    r()
                  }
                })
              })
            } else {
              r()
            }
          } else {
            r()
          }
        })
      } else {
        r()
      }
    })
  })

export default clearReplicaDump
