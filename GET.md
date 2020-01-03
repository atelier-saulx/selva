# Selva-client

## Get

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
  $language: 'de', // gets [de] fields if available
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

## Subscription Descriptor

```javascript
{
   version: 'flurpy',
   id: ['root.match', 'root.video'],
   date: 1578060349513
}
```

## Item

```javascript
{
   id: 'myid',
   url: ['url', 'url2'], // needs a url map in a seperate field (specfic)
   type: 'match',
   children: ['id', 'id2'],
   parents: ['id', 'id3'],
   ancestors: ['id', 'id3'],
   date: ts,
   start: ts, // '$date'
   end: ts,
   published: boolean,
   status: integer, // deprecate?
   video: { hls, mp4, overlays: [{
     interval: [0, 10, 60], // [start, end, repeat*optional]
     src: 'scoreboard image'
   }, {
     src: 'watermark'
   },] },
   image: { thumb, poster, cover, logo }, // maybe call logo => icon?
   title: { en, de, fr, nl },
   description: { en, de, fr, nl },
   article: { en, de, fr, nl },
   access: {
       geo: ['de', 'en' ],
       needsSubscription: boolean,
       payedItem: booleam
   },
   theme: { ... ? },
   ads: {},
   dictionary: {},
   menu: {},
   social: {},


   layout: {
       default: { components: [] },
       match: { components: [] },
       video: '$layout.match' // this is is a ref onnly within your own object - store with a bit mask -- make the api in set powerfull and good for this
   }
}
```

## Meta functionality

- user id per field (edited)
- last edited field
