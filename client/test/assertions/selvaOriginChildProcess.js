const { startOrigin } = require('@saulx/selva-server')

startOrigin({
  name: 'default',
  default: true,
  registry: {
    port: 2222,
  },
}).then(async (server) => {
  await server.selvaClient.updateSchema({
    languages: ['en'],
    types: {
      thing: {
        prefix: 'th',
        fields: {
          name: { type: 'string' },
        },
      },
    },
  })
  await server.selvaClient.set({ type: 'thing', name: 'yes' })
})
