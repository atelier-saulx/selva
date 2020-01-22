import crypto from 'crypto'

export default (payload: string) => {
  // think about this secret (how to configure)
  return crypto
    .createHmac('sha256', '')
    .update(payload)
    .digest('hex')
}
