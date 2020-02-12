import fetch from 'node-fetch'
import { promises as fs } from 'fs'
import { BackupFns } from '../../backups'
import { Dropbox, DropboxOptions } from 'dropbox'

export default async function mkBackupFn(
  opts: DropboxOptions,
  path: string,
  rdbFilePath: string
): Promise<BackupFns> {
  const dropbox = new Dropbox({ ...opts, ...{ fetch } })

  return {
    async sendBackup() {
      const content = await fs.readFile(rdbFilePath)
      try {
        await dropbox.filesUpload({
          path,
          contents: content
        })
      } catch (e) {
        // const chunks: Buffer[] = []
        // const body = await new Promise((resolve, reject) => {
        //   e.response.body.on(
        //     'data',
        //     (chunk: Buffer) =>
        //       chunks.push(chunk) && console.log(chunk.toString('utf8'))
        //   )
        //   e.response.body.on('error', reject)
        //   e.response.body.on('end', () =>
        //     resolve(Buffer.concat(chunks).toString('utf8'))
        //   )
        //   e.response.body.on('finish', () =>
        //     resolve(Buffer.concat(chunks).toString('utf8'))
        //   )
        // })
        // console.error(body)
        throw e
      }
    },
    async loadBackup() {
      // TODO
    }
  }
}
