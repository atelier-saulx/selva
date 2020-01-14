import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'

const isEqual = (a: any, b: any): boolean => {
  if (typeof a !== typeof b) {
    return false
  }
  if (Array.isArray(a)) {
    if (!Array.isArray(b)) {
      return false
    }
    if (a.length !== b.length) {
      return false
    }
    a.sort()
    b.sort()
    for (let i = 0; i < a.length; i++) {
      if (!isEqual(a[i], b[i])) {
        return false
      }
    }
  } else if (typeof a === 'object') {
    if (Object.keys(a).length !== Object.keys(b).length) {
      return false
    }
    for (let k in a) {
      if (!isEqual(a[k], b[k])) {
        return false
      }
    }
  } else {
    if (a !== b) {
      return false
    }
  }
  return true
}

test.before(async t => {
  await start({ port: 6062, modules: ['redisearch'] })
})

test.serial('get - basic', async t => {
  const client = connect({ port: 6062 })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice!'
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      title: true,
      value: true
    }),
    {
      id: 'viA',
      title: { en: 'nice!' },
      value: 25
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'viA',
      auth: true
    }),
    {
      auth: { role: { id: ['root'], type: 'admin' } }
    },
    'get role'
  )

  t.deepEqual(
    await client.get({
      $id: 'viA',
      auth: { role: { id: true } }
    }),
    {
      auth: { role: { id: ['root'] } }
    },
    'get role nested'
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $default', async t => {
  const client = connect({ port: 6062 })

  await client.set({
    $id: 'viflap',
    title: { en: 'flap' }
  })

  t.deepEqual(
    await client.get({
      $id: 'viflap',
      age: { $default: 100 }
    }),
    { age: 100 }
  )

  t.deepEqual(
    await client.get({
      $id: 'viflap',
      title: {
        en: { $default: 'untitled' },
        nl: { $default: 'naamloos' }
      }
    }),
    {
      title: { en: 'flap', nl: 'naamloos' }
    }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $language', async t => {
  const client = connect({ port: 6062 })
  await client.set({
    $id: 'viflap',
    title: { en: 'flap', nl: 'flurp' },
    description: { en: 'yes', nl: 'ja' }
  })

  t.deepEqual(
    await client.get({
      $id: 'viflap',
      title: true,
      description: true,
      $language: 'nl'
    }),
    {
      title: 'flurp',
      description: 'ja'
    }
  )

  await client.set({
    $id: 'viflurx',
    title: { en: 'flap', nl: 'flurp' }
  })

  t.deepEqual(
    await client.get({
      $id: 'viflurx',
      $language: 'nl',
      description: { $default: 'flurpy' }
    }),
    { description: 'flurpy' }
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - hierarchy', async t => {
  const client = connect({ port: 6062 })

  await Promise.all([
    client.set({
      $id: 'viflapx',
      children: ['vifla', 'viflo']
    }),
    client.set({
      $id: 'vifla',
      children: ['viflo', 'maflux']
    })
  ])

  t.true(
    isEqual(
      await client.get({
        $id: 'viflapx',
        descendants: true,
        children: true,
        parents: true
      }),
      {
        descendants: ['viflo', 'vifla', 'maflux'],
        children: ['viflo', 'vifla'],
        parents: ['root']
      }
    )
  )

  t.true(
    isEqual(
      await client.get({
        $id: 'maflux',
        ancestors: true
      }),
      {
        ancestors: ['root', 'vifla', 'viflapx']
      }
    )
  )

  await client.delete('root')

  client.destroy()
})

test.serial('get - $inherit', async t => {
  const client = connect({ port: 6062 })

  await Promise.all([
    client.set({
      $id: 'cuA',
      title: { en: 'snurf' },
      children: ['cuB', 'cuC']
    }),
    client.set({
      $id: 'cuB',
      children: ['cuC', 'cuD']
    }),
    client.set({
      $id: 'clClub',
      image: {
        thumb: 'bla.jpg'
      },
      children: ['cuB']
    })
  ])

  t.true(
    isEqual(
      await client.get({
        $id: 'cuD',
        title: { $inherit: true }
      }),
      {
        title: {
          en: 'snurf'
        }
      }
    )
  )

  t.true(
    isEqual(
      await client.get({
        $id: 'cuC',
        $language: 'nl',
        title: { $inherit: true }
      }),
      {
        title: 'snurf'
      }
    )
  )

  // pluging on ava would be nice
  t.true(
    isEqual(
      await client.get({
        $id: 'cuC',
        club: {
          $inherit: { $item: 'club' },
          image: true,
          id: true
        }
      }),
      {
        club: {
          image: { thumb: 'bla.jpg' },
          id: 'clClub'
        }
      }
    )
  )

  await client.delete('root')

  client.destroy()
})
