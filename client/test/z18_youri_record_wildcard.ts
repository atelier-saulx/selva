import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  const client = connect({ port })
  const theme = {
    type: 'object',
    properties: {
      colors: {
        type: 'object',
        properties: {
          blue: { type: 'string' },
        },
      },
    },
  }

  const components: any = {
    type: 'record',
    values: {
      type: 'object',
      properties: {
        component: { type: 'string' },
        name: { type: 'string' },
        index: { type: 'int' },
        text: { type: 'text' },
        color: { type: 'string' },
        image: { type: 'url' },
        font: { type: 'string' },
        fontSize: { type: 'number' },
        inputType: { type: 'string' },

        items: {
          type: 'record',
          values: {
            type: 'object',
            properties: {
              image: { type: 'url' },
              title: {
                type: 'object',
                properties: {
                  text: { type: 'text' },
                },
              },
              subtitle: {
                type: 'object',
                properties: {
                  text: { type: 'text' },
                },
              },
              info: {
                type: 'object',
                properties: {
                  text: { type: 'text' },
                  to: { type: 'string' },
                },
              },
            },
          },
        },
      },
    },
  }

  await client.updateSchema({
    languages: ['en', 'de'],
    rootType: {
      fields: {
        // @ts-ignore
        theme,
      },
    },
    types: {
      pageTemplate: {
        prefix: 'pt',
        fields: {
          name: { type: 'string' }, // of you want a custom name
          components,
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('inherit merge works for records', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'pt1',
    $language: 'en',
    name: 'Special Page Template',
    components: {
      0: {
        component: 'Text',
        name: 'Random Text',
        text: 'Voting round 1?',
        color: '#0750C6',
      },
      1: {
        component: 'Image',
        image:
          'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
      },
      2: {
        component: 'List',
        items: {
          0: {
            title: {
              text: 'Efendi',
            },
            image:
              'https://i1.wp.com/psych2go.net/wp-content/uploads/2014/08/91df642880432da28c563dfc45fa57f5.jpg?fit=640%2C400&ssl=1',
          },
        },
      },
    },
  })

  await client.get({
    $id: 'pt1',
    $language: 'en',
    id: true,
    components: {
      '*': {
        items: {
          0: true,
        },
      },
    },
  })

  t.pass()

  await client.destroy()
})
