import { join as pathJoin } from 'path'
import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait, removeDump } from './assertions'
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
    languages: ['en', 'de'],
    types: {
      user: {
        fields: {
          name: { type: 'string' },
        },
      },
      team: {
        fields: {
          name: { type: 'string' },
        },
      },
      teamMember: {
        fields: {
          name: { type: 'string' },
        },
      },
      thing: {
        fields: {
          name: { type: 'string' },
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

test.serial.only('authorize', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const userA = await client.set({
    type: 'user',
    name: 'User A',
  })

  const userId = await client.set({
    type: 'user',
    name: 'User B',
  })

  const teamMemberA = await client.set({
    parents: [userA, userB],
    type: 'teamMember',
    name: 'TeamMember A',
  })

  const teamMemberB = await client.set({
    parents: [userA, userB],
    type: 'teamMember',
    name: 'TeamMember B',
  })

  const teamA = await client.set({
    parents: [userA],
    type: 'team',
    name: 'Team A',
    children: [teamMemberA, teamMemberB],
  })

  let n = 10
  let targetId
  while (n--) {
    await client.set({
      parents: [teamA],
      type: 'thing',
      name: 'thing ' + n,
      children: await Promise.all(
        Array.from(Array(100)).map(async (_, i) => {
          targetId = await client.set({
            type: 'thing',
            name: 'thing ' + n + '-' + i,
          })
          return targetId
        })
      ),
    })
  }

  const checkAccess = async (userId, targetId) => {
    // get targets ancestors
    const { ancestors = [] } = await client.get({
      $id: targetId,
      ancestors: true,
    })
    const ancestorIds = new Set(ancestors)
    if (ancestorIds.has(userId)) {
      // I have access don't need to check team
      return true
    }
    // get my teams
    const { teams = [] } = await client.get({
      $id: userId,
      teams: {
        id: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: {
              $field: 'type',
              $operator: '=',
              $value: 'teamMember',
            },
            $find: {
              $traverse: 'parents',
              $filter: {
                $field: 'type',
                $operator: '=',
                $value: 'team',
              },
            },
          },
        },
      },
    })
  }

  // check if any of the team ids is in the
  const hasAccess = teams.find(({ id }) => ancestorIds.has(id))

  console.log({ targetId, teams, ancestors, hasAccess })

  await client.destroy()
})
