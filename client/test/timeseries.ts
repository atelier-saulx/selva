/*
 * TIMESERIES READS???
const case1 = {
  $id: 'ma1',
  id: true,
  title: true,
  description: true,
  visits: {
    $list: true, // let's say this defaults to all from the last shard? I dno, what's the best way to do this? then we also subscribe to new entries
  },
}

const case2 = {
  $id: 'ma1',
  id: true,
  title: true,
  description: true,
  visits: {
    // no fields specified, let's say this works when it's a primitive value? :thinking:
    $list: {
      $find: {
        $filter: [
          {
            $field: 'createdAt',
            $operator: '>',
            $field: 'now-1d',
          },
        ],
      },
    },
  },
}

const case3 = {
  $id: 'ma1',
  id: true,
  title: true,
  description: true,
  visits: {
    // now it's an object
    name: true,
    description: true,
    $list: {
      $find: {
        $filter: [
          {
            $field: 'createdAt',
            $operator: '>',
            $field: 'now-1d',
          },
        ],
      },
    },
  },
}
*/

import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' },
          },
        },
      },
    },
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          strRec: {
            type: 'record',
            values: {
              type: 'string',
            },
          },
          textRec: {
            type: 'record',
            values: {
              type: 'text',
            },
          },
          objRec: {
            type: 'record',
            values: {
              type: 'object',
              properties: {
                floatArray: { type: 'array', items: { type: 'float' } },
                intArray: { type: 'array', items: { type: 'int' } },
                objArray: {
                  type: 'array',
                  items: {
                    type: 'object',
                    properties: {
                      hello: { type: 'string' },
                      value: { type: 'int' },
                    },
                  },
                },
                hello: {
                  type: 'string',
                },
                nestedRec: {
                  type: 'record',
                  values: {
                    type: 'object',
                    properties: {
                      value: {
                        type: 'number',
                      },
                      hello: {
                        type: 'string',
                      },
                    },
                  },
                },
                value: {
                  type: 'number',
                },
                stringValue: {
                  type: 'string',
                },
              },
            },
          },
          thing: { type: 'set', items: { type: 'string' } },
          ding: {
            type: 'object',
            properties: {
              dong: { type: 'set', items: { type: 'string' } },
              texty: { type: 'text' },
              dung: { type: 'number' },
              dang: {
                type: 'object',
                properties: {
                  dung: { type: 'number' },
                  dunk: { type: 'string' },
                },
              },
              dunk: {
                type: 'object',
                properties: {
                  ding: { type: 'number' },
                  dong: { type: 'number' },
                },
              },
            },
          },
          dong: { type: 'json' },
          dingdongs: { type: 'array', items: { type: 'string' } },
          floatArray: { type: 'array', items: { type: 'float' } },
          intArray: { type: 'array', items: { type: 'int' } },
          refs: { type: 'references' },
          value: { type: 'number', timeseries: true },
          age: { type: 'number' },
          auth: {
            type: 'json',
            timeseries: true,
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            timeseries: true,
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      custom: {
        prefix: 'cu',
        fields: {
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
      club: {
        prefix: 'cl',
        fields: {
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
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' },
        },
      },
      yesno: {
        prefix: 'yn',
        fields: {
          bolYes: { type: 'boolean' },
          bolNo: { type: 'boolean' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

// TODO: this will work once branch schema update valiation is merged: https://github.com/atelier-saulx/selva/blob/schema-update-validation/client/src/schema/types.ts#L10
// import { FIELD_TYPES } from '../src/schema/types'
// import { SELVA_TO_SQL_TYPE } from "../../server/src/server/timeseriesWorker"
//
// test.serial('ensure type mappings are in sync', async (t) => {
//   const selvaTypes = new Set(FIELD_TYPES)
//   const timeseriesTypes = new Set(Object.keys(SELVA_TO_SQL_TYPE))
//
//   for (let type of selvaTypes) {
//     if (!timeseriesTypes.has(type)) {
//       t.fail(`${type} is missing from the timeseries mapping, this will make us fail to manage timeseries for this type`)
//     }
//   }
//   t.true(selvaTypes.size  <= timeseriesTypes.size)
// })

test.serial('get - basic value types timeseries', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice!',
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin',
      },
    },
    image: {
      thumb: 'lol',
    },
  })

  console.log('BLABLA', await client.redis.hgetall('___selva_schema'))

  await wait(4e3)

  console.log(
    await client.get({
      things: {
        $list: {
          $find: {
            $traverse: 'descendants',
          },
        },
        id: true,
        title: true,
        value: true,
        allValues: {
          $field: 'value',
          $list: { $limit: 10 },
        },
        filteredValues: {
          $list: {
            $find: {
              $traverse: 'value',
              $filter: [
                {
                  $field: 'value',
                  $operator: '>',
                  $value: 0,
                  $or: {
                    $field: 'value',
                    $operator: '=',
                    $value: 0,
                  },
                },
                {
                  $field: 'value',
                  $operator: '<',
                  $value: 1000,
                },
                {
                  $field: 'ts',
                  $operator: '>',
                  $value: 'now-5d',
                },
              ],
            },
            $sort: {
              $field: 'value',
              // $order: 'asc',
              $order: 'desc',
            },
          },
        },
        thumbnails: {
          $field: 'image',
          thumb: true,
          $list: true,
        },
      },
    })
  )

  await wait(5000e3)

  // t.deepEqual(
  //   await client.get({
  //     $id: 'viA',
  //     auth: true,
  //   }),
  //   {
  //     auth: { role: { id: ['root'], type: 'admin' } },
  //   },
  //   'get role'
  // )

  // not supported without 'properties'
  // t.deepEqual(
  //   await client.get({
  //     $id: 'viA',
  //     auth: { role: { id: true } }
  //   }),
  //   {
  //     auth: { role: { id: ['root'] } }
  //   },
  //   'get role nested'
  // )

  await client.delete('root')

  await client.destroy()
})
