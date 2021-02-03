const start = require('../../lib').start
const redis = require('redis')

const ACCESS_TOKEN = process.env.ACCESS_TOKEN

const mkDropbox = require('../../lib/backup-plugins/dropbox').default

const backups = require('../../lib/backups')

mkDropbox(
  {
    ACCESS_TOKEN: ACCESS_TOKEN
  },
  'backup.rdb'
).then(async backupFn => {
  const server = start({
    port: 6061,
    developmentLogging: true
  })

  setTimeout(() => {
    backups
      .saveAndBackUp(process.cwd(), 6061, backupFn)
      .then(() => {
        console.log(`Backed up successfully`)
      })
      .catch(e => {
        console.error('Failed to back up', e)
      })
      .finally(() => {
        setTimeout(() => {
          server.destroy().catch(e => {
            console.error(e)
          })
        }, 1000)
      })
  }, 500)
})
