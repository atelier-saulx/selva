import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'

import { wait } from './assertions/util'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'en_us', 'en_uk', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
      },
    },
    types: {
      logo: {
        prefix: 'lo',
        fields: {
          name: { type: 'string' },
          bidirClub: {
            type: 'reference',
            bidirectional: {
              fromField: 'bidirLogo',
            },
          },
        },
      },
      club: {
        prefix: 'cl',
        fields: {
          specialMatch: { type: 'reference' },
          bidirMatches: {
            type: 'references',
            bidirectional: {
              fromField: 'bidirClub',
            },
          },
          relatedClubs: {
            type: 'references',
            bidirectional: {
              fromField: 'relatedClubs',
            },
          },
          bidirLogo: {
            type: 'reference',
            bidirectional: {
              fromField: 'bidirClub',
            },
          },
          nested: {
            type: 'object',
            properties: {
              specialMatch: { type: 'reference' },
            },
          },
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          value: { type: 'number' },
          title: { type: 'text' },
          description: { type: 'text' },
          bidirClub: {
            type: 'reference',
            bidirectional: {
              fromField: 'bidirMatches',
            },
          },
        },
      },
    },
  })

  await wait(500)
  //await new Promise(r => setTimeout(r, 30 * 1000))

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('simple singular reference', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  // const match1 = await client.set({
  //   $id: 'maA',
  //   title: {
  //     en: 'yesh match'
  //   }
  // })

  // const club1 = await client.set({
  //   $id: 'clA',
  //   title: {
  //     en: 'yesh club'
  //   },
  //   specialMatch: match1
  // })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club',
    },
    specialMatch: {
      $id: 'maA',
      title: {
        en: 'yesh match',
      },
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      specialMatch: true,
    }),
    {
      title: 'yesh club',
      specialMatch: 'maA',
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      specialMatch: {
        title: true,
        description: { $default: 'no description' },
      },
    }),
    {
      title: 'yesh club',
      specialMatch: {
        title: 'yesh match',
        description: 'no description',
      },
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('simple singular reference with $flatten', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club',
    },
    specialMatch: {
      $id: 'maA',
      title: {
        en: 'yesh match',
      },
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      id: true,
      // title: true,
      specialMatch: {
        $flatten: true,
        title: true,
        description: { $default: 'no description' },
      },
    }),
    {
      id: 'clA',
      title: 'yesh match',
      description: 'no description',
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('nested singular reference with $flatten', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club',
    },
    nested: {
      specialMatch: {
        $id: 'maA',
        title: {
          en: 'yesh match',
        },
      },
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      id: true,
      title: true,
      nested: {
        specialMatch: {
          $flatten: true,
          title: true,
          description: { $default: 'no description' },
        },
      },
    }),
    {
      id: 'clA',
      title: 'yesh club',
      nested: {
        title: 'yesh match',
        description: 'no description',
      },
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('singular reference inherit', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'maB',
    value: 112,
  })

  const match1 = await client.set({
    $id: 'maA',
    title: {
      en: 'yesh match',
    },
    parents: {
      $add: 'maB',
    },
  })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club',
    },
    specialMatch: match1,
  })

  // const club1 = await client.set({
  //   $id: 'clA',
  //   title: {
  //     en: 'yesh club'
  //   },
  //   specialMatch: {
  //     $id: 'maA',
  //     title: {
  //       en: 'yesh match'
  //     }
  //   }
  // })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      specialMatch: {
        title: true,
        // value: { $inherit: { $type: ['match', 'club'] } }
        value: { $inherit: true },
      },
    }),
    {
      title: 'yesh club',
      specialMatch: {
        title: 'yesh match',
        value: 112,
      },
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('singular reference $field', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    $id: 'maA',
    title: {
      en: 'yesh match',
    },
  })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club',
    },
    specialMatch: match1,
  })

  // const club1 = await client.set({
  //   $id: 'clA',
  //   title: {
  //     en: 'yesh club'
  //   },
  //   specialMatch: {
  //     $id: 'maA',
  //     title: {
  //       en: 'yesh match'
  //     }
  //   }
  // })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      match: {
        $field: 'specialMatch',
        title: true,
      },
    }),
    {
      title: 'yesh club',
      match: {
        title: 'yesh match',
      },
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('singular reference inherit reference', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'clB',
    specialMatch: 'maA',
  })

  await client.set({
    $id: 'maB',
    value: 9001,
  })

  const match1 = await client.set({
    $id: 'maA',
    title: {
      en: 'yesh match',
    },
    parents: {
      $add: 'maB',
    },
  })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club',
    },
    parents: {
      $add: 'clB',
    },
  })

  // const club1 = await client.set({
  //   $id: 'clA',
  //   title: {
  //     en: 'yesh club'
  //   },
  //   specialMatch: {
  //     $id: 'maA',
  //     title: {
  //       en: 'yesh match'
  //     }
  //   }
  // })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      special: {
        $field: 'specialMatch',
        $inherit: { $type: ['club', 'match'] },
        title: true,
        // value: { $inherit: { $type: ['club', 'match'] } }
        value: { $inherit: true },
      },
    }),
    {
      title: 'yesh club',
      special: {
        title: 'yesh match',
        value: 9001,
      },
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('list of simple singular reference', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  // const match1 = await client.set({
  //   $id: 'maA',
  //   title: {
  //     en: 'yesh match'
  //   }
  // })

  // const club1 = await client.set({
  //   $id: 'clA',
  //   title: {
  //     en: 'yesh club'
  //   },
  //   specialMatch: match1
  // })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club',
    },
    specialMatch: {
      $id: 'maA',
      title: {
        en: 'yesh match',
      },
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      specialMatch: true,
    }),
    {
      title: 'yesh club',
      specialMatch: 'maA',
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      title: true,
      specialMatch: {
        title: true,
        description: { $default: 'no description' },
      },
    }),
    {
      title: 'yesh club',
      specialMatch: {
        title: 'yesh match',
        description: 'no description',
      },
    }
  )

  const result = await client.get({
    $id: 'root',
    $language: 'en',
    children: {
      id: true,
      title: true,
      parents: true,
      specialMatch: {
        id: true,
        title: true,
      },
      $list: {
        $find: {
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'club',
            },
          ],
        },
      },
    },
  })

  t.deepEqual(result, {
    children: [
      {
        id: 'clA',
        title: 'yesh club',
        parents: ['root'],
        specialMatch: { id: 'maA', title: 'yesh match' },
      },
    ],
  })

  t.deepEqual(
    await client.get({
      $id: 'root',
      $language: 'en',
      children: {
        $all: true,
        parents: true,
        specialMatch: {
          id: true,
          title: true,
        },
        $list: {
          $find: {
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'club',
              },
            ],
          },
        },
      },
    }),
    {
      children: [
        {
          id: 'clA',
          type: 'club',
          parents: ['root'],
          title: 'yesh club',
          specialMatch: { id: 'maA', title: 'yesh match' },
        },
      ],
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'clA',
      $all: true,
      specialMatch: {
        $all: true,
      },
    }),
    {
      id: 'clA',
      type: 'club',
      title: { en: 'yesh club' },
      specialMatch: {
        id: 'maA',
        title: { en: 'yesh match' },
        type: 'match',
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'root',
      $language: 'en',
      children: {
        $all: true,
        specialMatch: {
          $all: true,
        },
        $list: {
          $find: {
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'club',
              },
            ],
          },
        },
      },
    }),
    {
      children: [
        {
          id: 'clA',
          type: 'club',
          title: 'yesh club',
          specialMatch: { id: 'maA', title: 'yesh match', type: 'match' },
        },
      ],
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('simple singular bidirectional reference', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const club1 = await client.set({
    $id: 'clA',
    title: {
      en: 'yesh club',
    },
    bidirMatches: [
      {
        $id: 'maA',
        title: {
          en: 'yesh match',
        },
      },
      {
        $id: 'maB',
        title: {
          en: 'yesh match 2',
        },
      },
    ],
    bidirLogo: {
      $id: 'lo1',
      name: 'logo 1',
    },
  })

  const club2 = await client.set({
    $id: 'clB',
    title: {
      en: 'yesh club 2',
    },
    relatedClubs: {
      $add: 'clA',
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      $language: 'en',
      id: true,
      title: true,
      bidirClub: {
        id: true,
        title: true,
        logo: {
          $field: 'bidirLogo',
          name: true,
        },
      },
    }),
    {
      id: 'maA',
      title: 'yesh match',
      bidirClub: {
        id: 'clA',
        title: 'yesh club',
        logo: {
          name: 'logo 1',
        },
      },
    }
  )

  await client.set({
    $id: 'clA',
    bidirMatches: {
      $delete: 'maA',
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'maA',
      $language: 'en',
      id: true,
      title: true,
      bidirClub: {
        id: true,
        title: true,
        logo: {
          $field: 'bidirLogo',
          name: true,
        },
      },
    }),
    {
      id: 'maA',
      title: 'yesh match',
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      id: true,
      title: true,
      bidirMatches: {
        id: true,
        title: true,
        $list: true,
      },
      bidirLogo: {
        id: true,
        name: true,
      },
    }),
    {
      id: 'clA',
      title: 'yesh club',
      bidirMatches: [
        {
          id: 'maB',
          title: 'yesh match 2',
        },
      ],
      bidirLogo: {
        id: 'lo1',
        name: 'logo 1',
      },
    }
  )

  await client.set({
    $id: 'clA',
    bidirLogo: {
      $delete: true,
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      id: true,
      title: true,
      bidirMatches: {
        id: true,
        title: true,
        $list: true,
      },
      bidirLogo: {
        id: true,
        name: true,
      },
    }),
    {
      id: 'clA',
      title: 'yesh club',
      bidirMatches: [
        {
          id: 'maB',
          title: 'yesh match 2',
        },
      ],
    }
  )

  await client.set({
    $id: 'maB',
    bidirClub: { $delete: true },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      id: true,
      title: true,
      bidirMatches: {
        id: true,
        title: true,
        $list: true,
      },
      bidirLogo: {
        id: true,
        name: true,
      },
    }),
    {
      id: 'clA',
      title: 'yesh club',
      bidirMatches: [],
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      id: true,
      title: true,
      relatedClubs: {
        id: true,
        title: true,
        $list: true,
      },
    }),
    {
      id: 'clA',
      title: 'yesh club',
      relatedClubs: [
        {
          id: 'clB',
          title: 'yesh club 2',
        },
      ],
    }
  )

  await client.set({
    $id: 'clB',
    relatedClubs: {
      $delete: 'clA',
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clA',
      $language: 'en',
      id: true,
      title: true,
      relatedClubs: {
        id: true,
        title: true,
        $list: true,
      },
    }),
    {
      id: 'clA',
      title: 'yesh club',
      relatedClubs: [],
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'clB',
      $language: 'en',
      id: true,
      title: true,
      relatedClubs: {
        id: true,
        title: true,
        $list: true,
      },
    }),
    {
      id: 'clB',
      title: 'yesh club 2',
      relatedClubs: [],
    }
  )

  await client.delete('root')
  await client.destroy()
})
