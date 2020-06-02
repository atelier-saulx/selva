const redis = require('redis')
const dataRecord = require('data-record')
const promisify = require('util').promisify

const INCREMENT_DEF = dataRecord.compile([
  { name: 'index', type: 'int32_le' },
  { name: '$default', type: 'int32_le' },
  { name: '$increment', type: 'int32_le' }
])

const REFS_DEF = dataRecord.compile([
  { name: 'isReference', type: 'int32_le' },
  { name: '$add', type: 'cstring_p' },
  { name: '$delete', type: 'cstring_p' },
  { name: '$value', type: 'cstring_p' }
])

redis.add_command('selva.modify')

const r = redis.createClient(6379, '127.0.0.1')
r.monitor()

r.on('monitor', (_time, args, _rawReply) => {
  console.log('MONITOR', args)
})

async function yesh() {
  console.log(
    await promisify(r['selva.modify']).bind(r)(
      'testytestt',
      '1',
      'field_a',
      'abbba',
      '0',
      'field_b',
      'habahaba',
      '4',
      'incr',
      dataRecord
        .createRecord(INCREMENT_DEF, { index: 1, $increment: 12, $default: 3 })
        .toString(),
      '5',
      'children',
      dataRecord
        .createRecord(REFS_DEF, {
          isReference: 1,
          $value: 'testytest1testytest2testytest3testytest4',
          $add: '',
          $delete: ''
        })
        .toString(),
      '5',
      'myset',
      dataRecord.createRecord(REFS_DEF, {
        isReference: 0,
        $value: 'abba\0baba\0habahaba\0',
        $add: '',
        $delete: ''
      })
    )
  )

  console.log(
    await promisify(r['selva.modify']).bind(r)(
      'testytestt',
      '0',
      'field_a',
      'bbab',
      '0',
      'field_b',
      'haba'
    )
  )

  console.log(
    await r['selva.modify'](
      'testytestt',
      '5',
      'myset',
      dataRecord.createRecord(REFS_DEF, {
        isReference: 0,
        $value: null,
        $add: null,
        $delete: 'abba\0baba\0'
      }),
      '5',
      'children',
      dataRecord.createRecord(REFS_DEF, {
        isReference: 1,
        $value: null,
        $add: null,
        $delete: 'testytest2testytest3'
      })
    )
  )

  console.log(
    await promisify(r['selva.modify']).bind(r)(
      'testytestt',
      '5',
      'children',
      dataRecord.createRecord(REFS_DEF, {
        isReference: 1,
        $value: null,
        $add: 'testytest7',
        $delete: null
      }),
      '5',
      'myset',
      dataRecord.createRecord(REFS_DEF, {
        isReference: 0,
        $value: null,
        $delete: null,
        $add: 'hmm\0bumdumtsh\0'
      })
    )
  )
}

yesh()
  .catch(e => {
    console.error(e)
  })
  .finally(() => {
    console.log('DONE')
    process.exit(0)
  })

// perf test
// let counter = 0
// for (let i = 0; i < 100000; i++) {
//   r['selva.modify'](
//     'se6866fc53',
//     '0',
//     'field_b',
//     'habahaba' + i,
//     (err, res) => {
//       counter++
//
//       if (counter === 10000) {
//         console.log('DONE, LAST ONE')
//       }
//
//       if (err) {
//         console.error(err)
//       } else {
//         console.log('res', res)
//       }
//     }
//   )
// }
