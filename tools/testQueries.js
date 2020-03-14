const { connect } = require('@saulx/selva')

async function run() {
  const client = await connect({ port: 6061 }, { loglevel: 'info' })
  const result = await client.get({
    $id: 're7GonM2a',
    // $id: 'root',
    title: true,
    id: true,
    $language: 'en',
    // theme: { $inherit: true },
    // ads: { $inherit: true },
    components: [
      {
        component: { $value: 'Table' },
        children: {
          id: true,
          title: true,
          startTime: true,
          $list: {
            $find: {
              // $traverse: 'children',
              $traverse: 'descendants',
              $filter: [
                {
                  $operator: '=',
                  $field: 'type',
                  $value: 'match'
                },
                {
                  $operator: '>',
                  $field: 'startTime',
                  $value: 'now'
                }
              ]
            }
          }
        }
        //components: [
        //  {
        //    component: { $value: 'GridSmall' },
        //    title: true,
        //    children: {
        //      title: true,
        //      id: true,
        //      image: true,
        //      $list: {
        //        $find: {
        //          $traverse: 'descendants',
        //          $filter: {
        //            $field: 'id',
        //            $operator: '=',
        //            $value: ['relK767V5', 'renk1qW6p', 'rez5lmBya', 're7GonM2a']
        //          }
        //        }
        //      }
        //    }
        // children: [
        //   {
        //     $id: 'relK767V5',
        //     title: true,
        //     id: true,
        //     image: true
        //   }
        // ]
      }
      // {
      //   component: { $value: 'gridLarge' },
      //   showall: { $value: true },
      //   children: {
      //     title: true,
      //     $list: {
      //       $range: [0, 100],
      //       $find: {
      //         $traverse: 'descendants',
      //         $filter: [
      //           {
      //             $field: 'type',
      //             $operator: '=',
      //             $value: 'team'
      //           }
      //         ]
      //       }
      //     }
      //   }
      // },
      // {
      //   component: { $value: 'list' },
      //   children: {
      //     title: true,
      //     image: { icon: true, thumb: true },
      //     sport: { title: true, $inherit: { $item: 'sport' } },
      //     $list: {
      //       $sort: { $field: 'start', $order: 'asc' },
      //       $find: {
      //         $traverse: 'descendants',
      //         $filter: [
      //           {
      //             $field: 'type',
      //             $operator: '=',
      //             $value: 'match'
      //           },
      //           {
      //             $field: 'start',
      //             $operator: '<',
      //             $value: 'now'
      //           },
      //           {
      //             $field: 'end',
      //             $operator: '>',
      //             $value: 'now'
      //           }
      //         ]
      //       }
      //     }
      //   }
      // }
    ]
  })

  console.log(JSON.stringify(result, false, 2))

  // console.log(
  //   (
  //     await client.get({
  //       $language: 'de',
  //       items: {
  //         ancestors: true,
  //         $list: {
  //           $find: {
  //             $traverse: 'descendants',
  //             $filter: [
  //               {
  //                 $field: 'type',
  //                 $operator: '=',
  //                 $value: 'league'
  //               }
  //             ]
  //           }
  //         }
  //       }
  //     })
  //   ).items.find(({ title }) => title)
  // )

  /*
  console.log(
    (
      await client.get({
        $id: 'root',
        children: {
          id: true,
          type: true,
          title: true,
          $list: {
            $find: {
              $filter: [
                {
                  $operator: '=',
                  $field: 'type',
                  $value: ['team', 'season']
                }
              ]
            }
          }
        }
      })
    ).children.filter(x => {
      return !!x.title
    })
  )
  */

  // console.log(
  //   await client.get({
  //     items: {
  //       id: true,
  //       title: true,
  //       $list: {
  //         $limit: 100,
  //         $find: {
  //           $traverse: 'descendants',
  //           $filter: [
  //             {
  //               $field: 'type',
  //               $operator: '=',
  //               $value: 'match'
  //             }
  //           ]
  //         }
  //       }
  //     }
  //   })
  // )

  // console.log(
  //   await client.get({
  //     $language: 'de',
  //     items: {
  //       id: true,
  //       title: true,
  //       $list: {
  //         $limit: 100,
  //         $find: {
  //           $traverse: 'descendants',
  //           $filter: [
  //             {
  //               $field: 'type',
  //               $operator: '=',
  //               $value: 'match'
  //             },
  //             {
  //               $field: 'title',
  //               $operator: '=',
  //               $value: 'Greifswalder'
  //             }
  //           ]
  //         }
  //       }
  //     }
  //   })
  // )

  // console.log(
  //   await client.get({
  //     $language: 'de',
  //     items: {
  //       id: true,
  //       title: true,
  //       $list: {
  //         $limit: 100,
  //         $find: {
  //           $traverse: 'descendants',
  //           $filter: [
  //             {
  //               $field: 'type',
  //               $operator: '=',
  //               $value: 'sport'
  //             }
  //             // {
  //             //   $field: 'title',
  //             //   $operator: '=',
  //             //   $value: 'fu'
  //             // }
  //           ]
  //         }
  //       },
  //       matches: {
  //         id: true,
  //         title: true,
  //         $list: {
  //           $find: {
  //             $traverse: 'descendants',
  //             $filter: [
  //               {
  //                 $field: 'type',
  //                 $operator: '=',
  //                 $value: 'match'
  //               }
  //               // {
  //               //   $field: 'title',
  //               //   $operator: '=',
  //               //   $value: 'bayer'
  //               // }
  //             ]
  //           }
  //         }
  //       }
  //     }
  //   })
  // )

  // console.log(
  //   JSON.stringify(
  //     await client.get({
  //       $language: 'de',
  //       items: {
  //         id: true,
  //         title: true,
  //         $list: {
  //           $limit: 100,
  //           $find: {
  //             $traverse: 'descendants',
  //             $filter: [
  //               {
  //                 $field: 'type',
  //                 $operator: '=',
  //                 $value: 'match'
  //               },
  //               {
  //                 $field: 'title',
  //                 $operator: '=',
  //                 $value: 'Greifswalder'
  //               }
  //             ]
  //           }
  //         },
  //         parents: {
  //           id: true,
  //           title: true,
  //           $list: {
  //             $find: {
  //               $filter: [
  //                 {
  //                   $field: 'type',
  //                   $operator: '=',
  //                   $value: 'team'
  //                 }
  //               ]
  //             }
  //           }
  //         }
  //       }
  //     })
  //   )
  // )

  // console.log(
  //   await client.get({
  //     $alias: '47-adac-total-24h-rennen-onboard-6',
  //     $language: 'de',
  //     id: true,
  //     title: true,
  //     caption: true
  //   })
  // )

  // console.log(
  //   await client.get({
  //     $alias: 'sas-8a222205-7171-4e6e-9696-43de318288d2',
  //     $language: 'de',
  //     id: true,
  //     title: true,
  //     caption: true
  //   })
  // )

  await client.destroy()
}

run().catch(e => {
  console.error(e)
})
