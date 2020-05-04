import crypto from 'crypto'

// later move this to redis server
export default (payload: string) => {
  // think about this secret (how to configure)
  return crypto
    .createHmac('sha256', process.env.SELVA_DIGEST_SECRET || 'selva-client')
    .update(payload)
    .digest('hex')
}
