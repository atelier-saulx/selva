const fs = require('fs').promises
const path = require('path')
const srv = require('../../dist/index')
const redis = require('redis')

const ENDPOINT = process.env.ENDPOINT
const BUCKET = process.env.BUCKET
const ACCESS_KEY_ID = process.env.ACCESS_KEY_ID
const SECRET_ACCESS_KEY = process.env.SECRET_ACCESS_KEY

const mkS3 = require('../../dist/backup-plugins/s3').default

const backups = require('../../dist/backups')

;(async (backupFns) => {
  // force to load backup
  try {
    await fs.unlink(path.join(process.cwd(), 'dump.rdb'))
  } catch (e) {}

  const registry = srv.startRegistry({
    port: 6060,
  })

  const server = srv.startOrigin({
    default: true,
    port: 6061,
    dir: '/Users/tonykova/git/saulx/selva/server/examples/',
    registry: { port: 6060 },
    backups: {
      loadBackup: true,
      scheduled: { intervalInMinutes: 0.5 },
      backupFns: mkS3({
        endpoint: ENDPOINT,
        bucketName: BUCKET, // TODO: pass database name etc. to automate
        config: {
          accessKeyId: ACCESS_KEY_ID,
          secretAccessKey: SECRET_ACCESS_KEY,
        },
      }),
    },
  })

  // setTimeout(() => {
  //   server.destroy().catch(e => {
  //     console.error(e)
  //   })
  // }, 1000 * 60 * 5)
})().catch((e) => console.error(e))
