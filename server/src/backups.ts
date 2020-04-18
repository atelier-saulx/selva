import { promises as fs } from 'fs'
import { connect, SelvaClient } from '@saulx/selva'
import { join as pathJoin } from 'path'

function msSinceMidnight() {
  const d = new Date()
  d.setHours(0, 0, 0, 0)
  return Date.now() - d.getTime()
}

async function nextBackupTime(
  lastBackupTime: number,
  backupInterval: number
): Promise<number> {
  return Math.ceil(lastBackupTime / backupInterval) * backupInterval
}

export type BackupFns = { sendBackup: SendBackup; loadBackup: LoadBackup }
export type SendBackup = (rdbFilePath: string) => Promise<void>
export type LoadBackup = (
  rdbFilePath: string,
  rdbLastModified?: Date
) => Promise<void>

export async function loadBackup(redisDir: string, backupFns: BackupFns) {
  const dumpFile = pathJoin(redisDir, 'dump.rdb')
  try {
    const stat = await fs.stat(dumpFile)
    await backupFns.loadBackup(dumpFile, stat.mtime)
  } catch (e) {
    await backupFns.loadBackup(dumpFile)
  }
}

// loads the latest backup, but only if it's newer than local dump.rdb
export async function saveAndBackUp(
  redisDir: string,
  redisPort: number,
  backupFns: BackupFns
): Promise<void> {
  const client = connect({ port: redisPort })

  try {
    await client.redis.save()
    await backupFns.sendBackup(pathJoin(redisDir, 'dump.rdb'))
  } catch (e) {
    console.error(`Failed to back up ${e.stack}`)
    throw e
  } finally {
    client.destroy()
  }
}

export async function scheduleBackups(
  redisDir: string,
  redisPort: number,
  intervalInMinutes: number,
  backupFns: BackupFns
) {
  const client = connect({ port: redisPort })
  const dumpPath = pathJoin(redisDir, 'dump.rdb')
  await new Promise((resolve, _reject) => setTimeout(resolve, 500))
  const backupInterval = intervalInMinutes * 60 * 1000

  while (true) {
    const lastBackupTime = Number(
      await client.redis.get('___selva_backup_timestamp')
    )
    const nextBackup = await nextBackupTime(lastBackupTime, backupInterval)
    const timeOfDay = msSinceMidnight()

    const stat = await fs.stat(dumpPath)
    if (stat.mtime < new Date(Date.now() - intervalInMinutes * 60 * 1000)) {
      console.log(`No changes since ${stat.mtime}, skipping backup`)
      const delay = Math.max(nextBackup - timeOfDay, 1000 * 60) // wait at least 1 minute in between runs
      await new Promise((resolve, _reject) => setTimeout(resolve, delay))
      continue
    }

    if (timeOfDay >= nextBackup) {
      console.log('Trying to create backup', String(timeOfDay))

      try {
        await backupFns.sendBackup(dumpPath)
        await client.redis.set('___selva_backup_timestamp', String(timeOfDay))
        console.log('Backup created', String(timeOfDay))
      } catch (e) {
        console.error(`Failed to back up ${String(timeOfDay)} ${e}`)
        const delay = Math.max(nextBackup - timeOfDay, 1000 * 60) // wait at least 1 minute in between runs
        await new Promise((resolve, _reject) => setTimeout(resolve, delay))
      }
    } else {
      const delay = Math.max(nextBackup - timeOfDay, 1000 * 60) // wait at least 1 minute in between runs
      await new Promise((resolve, _reject) => setTimeout(resolve, delay))
    }
  }
}
