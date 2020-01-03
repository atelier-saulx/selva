# get

```javascript
import selva from 'selva-client'
const db = selva.connect({ host: 'bla.com', port: 8080 })

const { data: myItem } = await db.get({
  $id: 'mydingdong',
  id: true,
  title: true
})

const { data: myItem } = await db.get({
  $id: 'mydingdong',
  theme: { $inherit: true } // uses default order for inheritance
})

const { data: myItem } = await db.get({
  $id: 'mydingdong',
  theme: { $inherit: { type: ['sport', 'genre', 'region'] } } // prefers first in the list, if it cannot be found uses the next
})

const { data: myItem } = await db.get({
  $id: 'myclub24',
  speshTitle: {
    // map title to spesh title
    $field: ['title'],
    $inherit: true
  }
})

const { data: myItem } = await db.get({
  $id: 'mydingdong',
  speshTitle: {
    $inherit: {
      field: ['title', 'description']
    }
  }
})

const { data: myItem } = await db.get({
  $id: 'mydingdong',
  layout: {
    $field: ['layout.$type', 'layout.default'],
    $inherit: true
  }
})

const { data: myItem } = await db.get({
  $id: 'mydingdong',
  club: {
    $inherit: {
      item: ['club'] // cannot combine this with field
    },
    title: true,
    children: [
      {
        title: true,
        $range: [0, 1000]
      }
    ]
  }
})

const { data: myItem } = await db.get({
  $id: 'mydingdong', // top only
  $version: 'jims"s version', // top only
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
})

const { data: items } = await db.get([
  {
    $query: {
      scope: [{ id: 'volleyball' }, { id: 'football' }],
      type: ['match', 'video']
    },
    $filter: [{ field: 'start', operator: '>', value: 'now' }], // not everything
    $range: [0, 100],
    $sort: [{ field: 'start', order: 'ascending' }],
    title: true,
    ancestors: true,
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
          scope: [{ ancestor: ['league'] }] // tricky how to know if its home / away?
        }
      }
    ]
  }
])

const { data: items } = await db.get({
  $id: 'volleyball',
  items: [
    {
      $query: {
        scope: [{ id: '$id' }, { id: 'de' }],
        type: ['match', 'video']
      }
    }
  ]
})

const { data: items } = await db.get({
  $id: 'volleyball',
  items: [
    {
      $query: {
        scope: [{ ancestor: ['region'] }],
        type: ['match', 'video']
      }
    }
  ]
})

const { data: items } = await db.get({
  $id: 'volleyball',
  $subscriptionDescriptor: true, // returs a subscritpion descriptor
  matches: [
    {
      title: true,
      $query: {
        scope: [{ id: '$id' }],
        type: ['match', 'video']
      }
    }
  ],
  ancestors: [{ id: true, title: true, $range: [0, 2] }]
})
```

## Subscription descriptor

```javascript
{
   version: 'flurpy',
   id: ['root.match', 'root.video']
}
```
