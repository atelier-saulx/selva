import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import queryParser from '../src/query'
import './assertions'

test.serial('get - queryParser', async t => {
  // simple nested - single query
  const simpleQeury = {
    title: true,
    theme: { $inherit: true },
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          // in the array is an AND
          {
            $operator: '=',
            $field: 'type',
            $value: ['match', 'video'] // this is an OR
            // dont support nested ORS or ands for now
          },
          {
            $operator: '=',
            $field: 'name',
            $value: ['flurp', 'flap']
          }
        ],
        $find: {
          // anything else is a bit harder
          $traverse: 'ancestors',
          $filter: [
            {
              $operator: '=',
              $field: 'id',
              $value: ['de']
            },
            {
              $operator: '=',
              $field: 'type',
              $value: ['region', 'match']
            }
          ]
        }
      }
    }
  }

  console.log(queryParser(simpleQeury, 'volleyball'))

  const listQ = {
    title: true,
    theme: { $inherit: true },
    $list: {
      $range: [0, 100],
      $sort: [{ field: 'start', order: 'ascending' }]
    }
  }

  //   console.log(queryParser(listQ, 'volleyball', 'children'))

  const listQ2 = {
    title: true,
    theme: { $inherit: true },
    $list: {
      $sort: [{ field: 'start', order: 'ascending' }]
    }
  }

  //   console.log(queryParser(listQ, 'volleyball', 'children'))

  // complex nested double query
  const nestedQuery = {
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          {
            $operator: '=',
            $field: 'type',
            $value: ['match', 'video']
          },
          {
            $field: 'start',
            $operator: '>',
            $value: 'now'
          },
          {
            $traverse: 'ancestors',
            $field: 'id',
            $operator: '=',
            $value: 'fo143'
          }
        ]
      },
      $range: [0, 100],
      $sort: [{ field: 'start', order: 'ascending' }],
      title: true,
      ancestors: true,
      teams: {
        title: true,
        id: true,
        $list: {
          $find: {
            $traverse: 'ancestors',
            $filter: {
              $operator: '=',
              $field: 'type',
              $value: ['team']
            }
          }
        }
      },
      relatedVideos: {
        id: true,
        title: true,
        $list: {
          $range: [0, 100],
          $find: {
            $traverse: 'ancestors',
            $filter: {
              $operator: '=',
              $field: 'type',
              $value: ['league', 'genre', 'category']
            },
            $find: {
              $traverse: 'descendants',
              $filter: {
                $operator: '=',
                $field: 'type',
                $value: ['match', 'video']
              }
            }
          }
        }
      }
    }
  }

  //   console.log(queryParser(nestedQuery))

  t.true(true)
})
