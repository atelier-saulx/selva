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
  await client.updateSchema({
    languages: ['cs', 'en', 'de', 'fi', 'gsw'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          awayTeam: { type: 'reference' },
          homeTeam: { type: 'reference' },
        },
      },
      team: {
        prefix: 'te',
        fields: {
          title: {
            type: 'text',
          },
        },
      },
    },
  })
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('$lang should change the order when relevant', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'match1',
    awayTeam: 'team1',
    homeTeam: 'team2',
    children: [
      {
        $id: 'team1',
        title: {
          cs: 'Öäpelin pallo',
          de: 'Öäpelin pallo',
          en: 'Öäpelin pallo',
          fi: 'Öäpelin pallo',
          gsw: 'Öäpelin pallo',
        },
      },
      {
        $id: 'team2',
        title: {
          cs: 'Aopelin pallo',
          de: 'Aopelin pallo',
          en: 'Aopelin pallo',
          fi: 'Aopelin pallo',
          gsw: 'Aopelin pallo',
        },
      },
      {
        $id: 'team3',
        title: {
          cs: 'OOpelin pallo',
          de: 'OOpelin pallo',
          en: 'Oopelin pallo',
          fi: 'OOpelin pallo',
          gsw: 'OOpelin pallo',
        },
      },
      {
        $id: 'team4',
        title: {
          cs: 'Ääpelin pallo',
          de: 'Ääpelin pallo',
          en: 'Ääpelin pallo',
          fi: 'Ääpelin pallo',
          gsw: 'Ääpelin pallo',
        },
      },
      {
        $id: 'team5',
        title: {
          cs: 'öäpelin pallo',
          de: 'öäpelin pallo',
          en: 'öäpelin pallo',
          fi: 'öäpelin pallo',
          gsw: 'öäpelin pallo',
        },
      },
      {
        $id: 'team6',
        title: {
          cs: 'aopelin pallo',
          de: 'aopelin pallo',
          en: 'aopelin pallo',
          fi: 'aopelin pallo',
          gsw: 'aopelin pallo',
        },
      },
      {
        $id: 'team7',
        title: {
          cs: 'oOpelin pallo',
          de: 'oOpelin pallo',
          en: 'oopelin pallo',
          fi: 'oOpelin pallo',
          gsw: 'oOpelin pallo',
        },
      },
      {
        $id: 'team8',
        title: {
          cs: 'ääpelin pallo',
          de: 'ääpelin pallo',
          en: 'ääpelin pallo',
          fi: 'ääpelin pallo',
          gsw: 'ääpelin pallo',
        },
      },
      {
        $id: 'team9',
        title: {
          cs: 'hrnec pallo',
          de: 'hrnec pallo',
          en: 'hrnec pallo',
          fi: 'hrnec pallo',
          gsw: 'hrnec pallo',
        },
      },
      {
        $id: 'team10',
        title: {
          cs: 'chrt pallo',
          de: 'chrt pallo',
          en: 'chrt pallo',
          fi: 'chrt pallo',
          gsw: 'chrt pallo',
        },
      },
    ],
  })

  t.deepEqual(
    await client.get({
      $language: 'en',
      $id: 'match1',
      children: {
        myId: { $field: 'id' },
        $list: {
          $sort: { $field: 'title', $order: 'asc' },
          $find: {
            $traverse: 'descendants',
          },
        },
      },
    }),
    {
      children: [
        { myId: 'team8' },
        { myId: 'team4' },
        { myId: 'team6' },
        { myId: 'team2' },
        { myId: 'team10' },
        { myId: 'team9' },
        { myId: 'team5' },
        { myId: 'team1' },
        { myId: 'team7' },
        { myId: 'team3' },
      ],
    }
  )

  t.deepEqual(
    await client.get({
      $language: 'de',
      $id: 'match1',
      children: {
        id: true,
        $list: {
          $sort: { $field: 'title', $order: 'asc' },
          $find: {
            $traverse: 'descendants',
          },
        },
      },
    }),
    {
      children: [
        { id: 'team8' },
        { id: 'team4' },
        { id: 'team6' },
        { id: 'team2' },
        { id: 'team10' },
        { id: 'team9' },
        { id: 'team5' },
        { id: 'team1' },
        { id: 'team7' },
        { id: 'team3' },
      ],
    }
  )

  t.deepEqual(
    await client.get({
      $language: 'fi',
      $id: 'match1',
      children: {
        id: true,
        $list: {
          $sort: { $field: 'title', $order: 'asc' },
          $find: {
            $traverse: 'descendants',
          },
        },
      },
    }),
    {
      children: [
        { id: 'team6' },
        { id: 'team2' },
        { id: 'team10' },
        { id: 'team9' },
        { id: 'team7' },
        { id: 'team3' },
        { id: 'team8' },
        { id: 'team4' },
        { id: 'team5' },
        { id: 'team1' },
      ],
    }
  )

  t.deepEqual(
    await client.get({
      $language: 'cs',
      $id: 'match1',
      children: {
        id: true,
        $list: {
          $sort: { $field: 'title', $order: 'asc' },
          $find: {
            $traverse: 'descendants',
          },
        },
      },
    }),
    {
      children: [
        { id: 'team8' },
        { id: 'team4' },
        { id: 'team6' },
        { id: 'team2' },
        { id: 'team9' },
        { id: 'team10' },
        { id: 'team5' },
        { id: 'team1' },
        { id: 'team7' },
        { id: 'team3' },
      ],
    }
  )

  t.deepEqual(
    await client.get({
      $language: 'gsw',
      $id: 'match1',
      children: {
        id: true,
        $list: {
          $sort: { $field: 'title', $order: 'asc' },
          $find: {
            $traverse: 'descendants',
          },
        },
      },
    }),
    {
      children: [
        { id: 'team8' },
        { id: 'team4' },
        { id: 'team6' },
        { id: 'team2' },
        { id: 'team10' },
        { id: 'team9' },
        { id: 'team5' },
        { id: 'team1' },
        { id: 'team7' },
        { id: 'team3' },
      ],
    }
  )

  await client.delete('root')
  await client.destroy()
})
