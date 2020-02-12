const fs = require('fs').promises
const path = require('path')
const start = require('../../lib').start
const redis = require('redis')

const ENDPOINT = process.env.ENDPOINT
const BUCKET = process.env.BUCKET
const ACCESS_KEY_ID = process.env.ACCESS_KEY_ID
const SECRET_ACCESS_KEY = process.env.SECRET_ACCESS_KEY

const mkS3 = require('../../lib/backup-plugins/s3').default

const backups = require('../../lib/backups')

;(async backupFns => {
  // force to load backup
  await fs.unlink(path.join(process.cwd(), 'dump.rdb'))

  const server = start({
    port: 6061,
    modules: ['redisearch'],
    developmentLogging: true,
    backups: {
      loadBackup: true,
      backupFns: mkS3({
        endpoint: ENDPOINT,
        bucketName: BUCKET, // TODO: pass database name etc. to automate
        config: {
          accessKeyId: ACCESS_KEY_ID,
          secretAccessKey: SECRET_ACCESS_KEY
        }
      })
    }
    // TODO: accept backupFn as a promise, awaited in server.start()
  })

  setTimeout(() => {
    server
      .backup()
      .then(() => {
        console.log(`Backed up successfully`)
      })
      .catch(e => {
        console.error(`Failed to back up ${e}`)
      })
      .finally(() => {
        setTimeout(() => {
          server.destroy().catch(e => {
            console.error(e)
          })
        }, 1000)
      })
  }, 500)
})().catch(e => console.error(e))
