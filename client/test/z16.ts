import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de'],
    types: {
      sport: {
        prefix: 'sp',
        fields: {
          title: {
            type: 'text',
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          }
        }
      },
      category: {
        prefix: 'ca',
        fields: {
          title: {
            type: 'text',
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'text',
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          },
          published: {
            type: 'boolean',
            search: { type: ['TAG'] }
          }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('real world highlights', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $language: 'en',
    $id: 'sp1',
    title: 'sport nice',
    children: [
      {
        $id: 'ca1',
        title: 'Highlights',
        name: 'highlights',
        children: [
          {
            $id: 'ma1',
            title: 'match 1',
            published: true
          },
          {
            $id: 'ma2',
            title: 'match 2',
            published: true
          },
          {
            $id: 'ma3',
            title: 'match 3',
            published: false
          }
        ]
      }
    ]
  })
  //t.deepEqualIgnoreOrder(

  console.log(
    await client.get({
      $id: 'sp1',
      $language: 'en',
      component: {
        $value: 'Highlights'
      },
      children: {
        type: true,
        title: true,
        id: true,
        $list: {
          $find: {
            $traverse: 'children',
            // $find: {
            //   $traverse: 'children',
            //   $filter: [
            //     {
            //       $field: 'published',
            //       $operator: '=',
            //       $value: true
            //     }
            //   ]
            // },
            $filter: [
              {
                $value: 'category',
                $field: 'type',
                $operator: '='
              },
              {
                $value: 'highlights',
                $field: 'name',
                $operator: '='
              }
            ]
          },
          $limit: 3
        },
        date: true,
        video: true
      },
      title: {
        $value: 'Bla bla'
      }
    }),
    {
      component: 'Highlights',
      title: 'Bla bla',
      children: [
        { id: 'ma1', type: 'match', title: 'match 1' },
        { id: 'ma2', type: 'match', title: 'match 2' }
      ]
    }
  )
  t.pass()

  await client.delete('root')
  await client.destroy()
})
