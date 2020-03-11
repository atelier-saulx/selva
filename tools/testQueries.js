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

  console.log(
    await client.get({
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
              }
            ]
          }
        }
      }
    })
  )

  await client.destroy()
}

run().catch(e => {
  console.error(e)
})
