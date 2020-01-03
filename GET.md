# get

- entity is an entry in the database, e.g. `root / [clubod]` etc

```javascript
import selva from 'selva-client'
const db = selva.connect({ host: 'bla.com', port: 8080 })

// single item get
const myItem = await db.get(
  {
    id: 'myclub24'
  },
  {
    id: true,
    title: true
  }
)

// single item get inherit (default)
const myItem = await db.get(
  {
    id: 'myclub24'
  },
  {
    theme: { $inherit: true } // uses default order for inheritance
  }
)

// single item get inherit
const myItem = await db.get(
  {
    id: 'myclub24'
  },
  {
    theme: { $inherit: { type: ['sport', 'genre', 'region'] } } // prefers first in the list, if it cannot be found uses the next
  }
)

// rename a field to a new field
const myItem = await db.get(
  {
    id: 'myclub24'
  },
  {
    speshTitle: {
      $inherit: {
        field: 'title' // by default if uses the field name 'club'
      }
    }
  }
)

// add a new field and inherit it from an entity
const myItem = await db.get(
  {
    id: 'myclub24'
  },
  {
    club: {
      $inherit: {
        entity: ['club'] // cannot combine this with field
      },
      title: true,
      children: [
        {
          title: true,
          $range: [0, 1000]
        }
      ]
    }
  }
)

// get all clubs
const myItem = await db.get(
  {
    id: 'myclub24'
  },
  {
    clubs: [
      {
        $inherit: {
          entity: ['club'] // cannot combine this with field
        },
        title: true,
        children: [
          {
            title: true,
            $range: [0, 1000]
          }
        ]
      }
    ]
  }
)

// query
const children = await db.get(
  { id: 'root' },
  {
    items: [
      {
        $query: {
          // firstEncounterOfType: true // default
          //  deep: true,
          //  shallow: true

          scope: [
            { queryType: 'id', id: 'volleyball' },
            { queryType: 'id', id: 'football' }
          ],
          type: ['match', 'video']
        },
        $filter: [{ field: 'start', operator: '>', value: 'now' }], // not everything
        $range: [0, 100],
        $sort: [{ field: 'start', order: 'ascending' }],
        title: true,
        teams: [
          {
            $inherit: { entiry: ['team'] },
            title: true,
            id: true
          }
        ],
        relatedVideos: [
          {
            $query: {
              type: ['match', 'video'],
              scope: [{ queryType: 'ancestor', ancestorType: ['league'] }] // tricky how to know if its home / away?
            }
          }
        ]
      }
    ]
  }
)

// query
const children = await db.get(
  { id: 'volleyball' },
  {
    items: [
      {
        $query: {
          scope: [{ queryType: '$self' }, { queryType: 'id', id: 'de' }],
          type: ['match', 'video']
        }
      }
    ]
  }
)

// query
const children = await db.get(
  { id: 'volleyball' },
  {
    items: [
      {
        $query: {
          scope: [{ queryType: 'ancestor', ancestorType: ['region'] }],
          type: ['match', 'video']
        }
      }
    ]
  }
)
```
