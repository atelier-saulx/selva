import { promises as fs } from 'fs'
import aws from 'aws-sdk'

export type S3Api = {
  getBuckets: () => Promise<aws.S3.Bucket[]>
  createBucket: (bucketName: string, acl: string) => Promise<void>
  ensureBucket: (bucketName: string, acl: string) => Promise<void>
  listObjects: (bucketName: string) => Promise<aws.S3.ObjectList>
  getObject: (bucketName: string, filepath: string) => Promise<aws.S3.Body>
  deleteObject: (bucketName: string, filepath: string) => Promise<void>
  storeFile: (
    bucketName: string,
    destFilepath: string,
    sourceFilepath: string
  ) => Promise<void>
}

export function createApi(
  opts: {
    accessKeyId: string
    secretAccessKey: string
  },
  endpoint: string
): S3Api {
  if (!opts.accessKeyId || !opts.secretAccessKey) {
    throw new Error('No accessKeyId or secretAccessKey provided')
  }

  aws.config.update(opts)
  const s3 = new aws.S3({ endpoint: endpoint })
  const api: S3Api = {
    getBuckets() {
      return new Promise((resolve, reject) => {
        s3.listBuckets((err, res) => {
          if (err) {
            return reject(err)
          }

          return resolve(res.Buckets)
        })
      })
    },
    createBucket(bucketName, acl) {
      return new Promise((resolve, reject) => {
        s3.createBucket({ Bucket: bucketName, ACL: acl }, (err, _res) => {
          if (err) {
            return reject(err)
          }

          resolve()
        })
      })
    },
    async ensureBucket(bucketName, acl) {
      const buckets = await api.getBuckets()
      const found = buckets.find((bucket) => {
        return bucket.Name === bucketName
      })

      if (found) {
        return
      }

      await api.createBucket(bucketName, acl)
    },
    listObjects(bucketName) {
      return new Promise((resolve, reject) => {
        s3.listObjects({ Bucket: bucketName }, (err, res) => {
          if (err) {
            return reject(err)
          }

          resolve(res.Contents)
        })
      })
    },
    getObject(bucketName, filepath) {
      return new Promise((resolve, reject) => {
        s3.getObject({ Bucket: bucketName, Key: filepath }, (err, res) => {
          if (err) {
            return reject(err)
          }

          resolve(res.Body)
        })
      })
    },
    deleteObject(bucketName, filepath) {
      return new Promise((resolve, reject) => {
        s3.deleteObject({ Bucket: bucketName, Key: filepath }, (err, _res) => {
          if (err) {
            return reject(err)
          }

          resolve()
        })
      })
    },
    async storeFile(bucketName, destFilepath, sourceFilepath) {
      const content = await fs.readFile(sourceFilepath)
      return new Promise((resolve, reject) => {
        s3.upload(
          { Bucket: bucketName, Key: destFilepath, Body: content },
          (err, _res) => {
            if (err) {
              return reject(err)
            }

            resolve()
          }
        )
      })
    },
  }

  return api
}
