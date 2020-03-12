const { connect } = require('@saulx/selva')

async function run() {
  const client = await connect({ port: 6061 }, { loglevel: 'info' })
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

  console.log(
    JSON.stringify(
      await client.get({
        $language: 'de',
        items: {
          id: true,
          title: true,
          $list: {
            $limit: 100,
            $find: {
              $traverse: 'descendants',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'match'
                },
                {
                  $field: 'title',
                  $operator: '=',
                  $value: 'Greifswalder'
                }
              ]
            }
          },
          parents: {
            id: true,
            title: true,
            $list: {
              $find: {
                $filter: [
                  {
                    $field: 'type',
                    $operator: '=',
                    $value: 'team'
                  }
                ]
              }
            }
          }
        }
      })
    )
  )

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
