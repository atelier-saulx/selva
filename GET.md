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
    theme: { inherit: true } // uses default order for inheritance
  }
)

// single item get inherit
const myItem = await db.get(
  {
    id: 'myclub24'
  },
  {
    theme: { $inherit: { $type: ['sport', 'genre', 'region'] } } // prefers first in the list, if it cannot be found uses the next
  }
)

// rename a field to a new field
const myItem = await db.get(
  {
    id: 'myclub24'
  },
  {
    club: {
      $inherit: {
        $field: 'title' // by default if uses the field name 'club'
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
        $entity: ['club']
      },
      title: true,
      children: [
        {
          title: true
        }
      ]
    }
  }
)

// query
const children = await db.get(
  { id: 'root' },
  {
    items: [
      {
        $query: {
          scope: [{ queryType: 'ancestor', ancestorType: ['match', 'video'] }], // if match is found before
          type: ['match', 'video'],
          filter: [{ field: 'start', operator: '>', value: 'now' }],
          range: [0, 100],
          sort: [{ field: 'start', order: 'ascendin' }]
        },
        title: true
      }
    ]
  }
)
```
