export default function sendEvent(id: string, field: string) {
  redis.call('publish', `___selva_events:${id}.${field}`, '')
}
