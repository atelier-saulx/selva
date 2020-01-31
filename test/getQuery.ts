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
  console.log(queryParser(simpleQeury, 'volleyball'))

  // simple nested - single query
  const simpleQeury2 = {
    title: true,
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          {
            $operator: '=',
            $field: 'type',
            $value: 'match' // this is an OR
          },
          {
            $operator: '=',
            $field: 'name',
            $value: 'gurk',
            $and: {
              $operator: '=',
              $field: 'type',
              $value: ['match', 'video'] //bit weird merge but ok
            }
          },
          {
            // merge this
            $operator: '=',
            $field: 'ancestors',
            $value: 're*'
          }
        ]
      }
    }
  }

  console.log(queryParser(simpleQeury2, 'root'))

  // simple nested - single query
  const simpleQeury3 = {
    title: true,
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          {
            $operator: '=',
            $field: 'type',
            $value: 'match' // this is an OR
          },
          {
            $operator: '=',
            $field: 'name',
            $value: 'gurk',
            $and: {
              $operator: '=',
              $field: 'type',
              $value: 'video' //bit weird merge but ok
            }
          }
        ]
      }
    }
  }

  try {
    console.log(queryParser(simpleQeury3, 'root'))
  } catch (err) {
    console.log(err.message)
  }

  // simple nested - single query
  const simpleQeury4 = {
    title: true,
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          {
            $operator: '!=',
            $field: 'type',
            $and: {
              $operator: '!=',
              $field: 'name',
              $value: ['pietje', 'flappie'] // this is an OR
            },
            $value: 'match' // this is an OR
          },
          {
            $operator: '!=',
            $field: 'name',
            $value: ['pietje', 'mr snurfels'] // this is an OR
          },
          {
            $operator: '=',
            $field: 'status',
            $value: 'gurk',
            $and: {
              $operator: '!=',
              $field: 'type',
              $value: 'video' //bit weird merge but ok
            }
          }
        ]
      }
    }
  }

  console.log(queryParser(simpleQeury4, 'root'))

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
