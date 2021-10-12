import { promises as fs } from 'fs'
import { BackupFns } from '../../backups'
import { createApi, S3Api } from './s3api'

export { S3Api } from './s3Api'

type S3Opts = {
  config: {
    accessKeyId: string
    secretAccessKey: string
  }
  endpoint: string
  bucketName: string
  backupRetentionInDays?: number
}

async function cleanUpOldBackups(
  s3: S3Api,
  bucketName: string,
  retentionInDays: number
): Promise<void> {
  const objects = await s3.listObjects(bucketName)
  const oldBackups = objects.filter((object) => {
    const validSince = new Date(
      Date.now() - 1000 * 60 * 60 * 24 * retentionInDays
    )
    return new Date(object.Key) < validSince
  })

  await Promise.all(
    oldBackups.map((object) => {
      // console.log(`Deleting object ${object.Key}`)
      // return s3.deleteObject(bucketName, object.Key)
    })
  )
}

export default async function mkBackupFn(opts: S3Opts): Promise<BackupFns> {
  const { endpoint, backupRetentionInDays = 30, bucketName, config } = opts
  const s3 = createApi(config, endpoint)
  await s3.ensureBucket(bucketName, 'private')

  return {
    async sendBackup(rdbFilePath: string) {
      const dstFilepath = new Date().toISOString()
      await s3.storeFile(bucketName, dstFilepath, rdbFilePath)
      await cleanUpOldBackups(s3, bucketName, backupRetentionInDays)
    },
    async loadBackup(rdbFilePath: string, rdbLastModified: Date) {
      const objects = await s3.listObjects(bucketName)
      if (!objects.length) {
        console.log(`Bucket ${bucketName} is empty, skipping backup loading`)
        return
      }

      const latest = objects.reduce((max, o) => {
        if (new Date(o.LastModified) > new Date(max.LastModified)) {
          return o
        }

        return max
      })

      if (!rdbLastModified || new Date(latest.LastModified) > rdbLastModified) {
        const body: Buffer = <Buffer>await s3.getObject(bucketName, latest.Key)
        await fs.writeFile(rdbFilePath, body)
      }
    },
  }
}
