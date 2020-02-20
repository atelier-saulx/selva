export default function sendEvent(id: string, field: string, type: string) {
  if (field === '') {
    redis.call('publish', `___selva_events:${id}`, type)
  } else {
    redis.call('publish', `___selva_events:${id}.${field}`, type)
  }
}
