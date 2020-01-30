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
          {
            // $or: {}, $and: {} if field is not the same thencgo crazy
            $operator: '=',
            $field: 'type',
            $value: 'match' // this is an OR
          },
          // in the array is an AND
          {
            $operator: '=',
            $field: 'flup',
            $value: ['flap', 'snurfy'], // this is an OR
            $or: {
              $operator: '=',
              $value: ['snurkyshine'],
              $field: 'name',
              $and: {
                $operator: '=',
                $value: ['snurx'],
                $field: 'flurpe'
              },
              $or: {
                $operator: '=',
                $value: ['flurpeshine'],
                $field: 'type'
              }
            },
            $and: {
              $operator: '=',
              $value: 300,
              $field: 'status',
              $or: {
                $operator: '=',
                $value: 100,
                $field: 'weirdStatus',
                $and: {
                  $operator: '=',
                  $value: 100,
                  $field: 'strangeStatus'
                }
              }
            }
          },
          {
            $operator: '=',
            $field: 'name',
            $value: ['flurp', 'flap'],
            $and: {
              $operator: '=',
              $field: 'type',
              $value: ['match', 'video']
            }
          }
        ]
      }
    }
  }

  /*
 // $find: {
        //   // this must return the ancestors not children
        //   // any other field else is a bit harder (nested qeury)

        //   // ALLWAYS NESTED AGAIN
        //   $traverse: 'ancestors',
        //   $filter: [
        //     {
        //       $operator: '=',
        //       $field: 'id',
        //       $value: ['de']
        //     },
        //     {
        //       $operator: '=',
        //       $field: 'type',
        //       $value: ['region', 'match']
        //     }
        //   ]

        //   //   $find: {
        //   //       $traverse: 'descendants'
        //   //   }
        // }
  */

  //   `@ancestors:volleyball @type:{match|video} ((@flup:{flap|snurfy} (@status:300|(@weirdStatus:100 @strangeStatus:100)))|(@name:{snurkyshine} @flurpe:{snurx})|@type:{flurpeshine}) @name:{flurp|flap}`

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
