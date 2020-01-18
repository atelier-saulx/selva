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
  title: { $default: 'my smurfypants' }
})

const { data: myItem } = await db.get({
  $id: 'mydingdong',
  theme: { $inherit: true } // uses default order for inheritance
})

const { data: myItem } = await db.get({
  $id: 'mydingdong',
  theme: { $inherit: { $type: ['sport', 'genre', 'region'] } } // prefers first in the list, if it cannot be found uses the next
})


const { data: myItem } = await db.get({
  $id: 'mydingdong',
  theme: { $inherit: { $name: ['mydingdongName'] } } // prefers first in the list, if it cannot be found uses the next
})

const { data: myItem } = await db.get({
  $id: 'myclub24',
  speshTitle: {
    // map title to spesh title
    $field: ['title'],
    $inherit: true,
    $default: 'cdsds'
  }
})

// { speshTitle: 'NO TITLE FUN' }
// { speshTitle: 'XXX }


const { data: myItem } = await db.get({
  $id: 'mydingdong',
  speshTitle: {
    $inherit: {
      $field: ['title', 'description']
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
      $item: ['club'] // cannot combine this with field
    },
    title: true,
    children: {
      $field: ['childrenzzz'],
      $list: {
        $range: [0, 1000]
      },
      title: true
    }
  }
})

//-----------------------------------------------------
const { data: myItem } = await db.get({
  $id: 'mydingdong', // top only
  $version: 'jims', // top only
  clubs: {
    $list: {
      $range: [0, 100],
      $find: {
        $traverse: 'descendants',
        $filter: {
          $operator: '=',
          $field: 'type',
          $value: ['club', 'team']
        }
      }
    },
    title: true,
    children: {
      title: true,
      $list: {
        $range: [0, 1000]
      }
    }
  }
})

const { data: items } = await db.get({
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
      $sort: [{ field: 'start', order: 'ascending' },
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
  })

const { data: items } = await db.get({
  $id: 'volleyball',
  title: true,
  items: {
    title: true,
    theme: { $inherit: true },
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          {
            $operator: '=',
            $field: 'type',
            $value: ['match']
          },
        ],
        $find: {
          $filter: [
            {
              $operator: '=',
              $field: 'id',
              $value: ['de']
            }
          ],
          $traverse: 'ancestors'
        }
      }
    }
  }
})

// redis-cli ft.search masterIndex @ancestors:{volleyball} @ancestors:{de} @type:{match|video}
{
$id: 'volleyball',
  items: {
    $list: {
      $find: {
        // non redis search if id and direct ancestors
        $traverse: 'ancestors',
        $filter: [
          $operator: '=',
          $field: 'type',
          $value: 'region'
        ],
        // non redis
        $find: {
          $traverse: 'decendants',
          $filter: [{
            $operator: '=',
            $field: 'type',
            $value:  [ 'match'],
            $and: {
              $operator: '=',
              $field: 'name',
              $value:  [ 'flurpy'],
              $and: {
                $operator: '=',
                $field: 'status',
                $value:  [ 100'],
              }
            }
          }]
        }
      }
    }
  }
}

// $filter: ['type', '=', ['match', 'video']]

const { data: items } = await db.get({
  $id: 'volleyball',
  $subscriptionDescriptor: true, // returs a subscritpion descriptor
  matches: [
    {
      title: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $operator: '=',
            $field: 'type',
            $value: [ 'match', 'video']
          }
        }
      }
    }
  ],
  ancestors: { id: true, title: true, $list: { $range: [0, 2] } }
})
```

## Subscription Descriptor

```javascript
{
   version: 'flurpy',
   key: ['root.match', 'root.video'],
   date: 1578060349513
}
```

## Item

### Types

```javascript
match,
  person,
  character,
  organisation,
  club,
  video,
  team,
  genre,
  movie,
  show,
  event,
  location,
  sport,
  camera,
  category,
  tag,
  ad
```

```javascript
{
  // U <--- id
   id: 'myid',
   url: ['url', 'url2'], // needs a url map in a seperate field (specfic)
   type: 'match',

   children: ['id', 'id2'], // redis SET
   parents: ['id', 'id3'], // redis SET

   ancestors: ['id', 'id3'], // ---> string seperated list
   // ancestors_index: 'xxx,yyy,zzz',

   // its a set not an array
   externalId: ['5432','jhgfds'], // redis SET
  //  descendants
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
  image: { thumb, poster, cover, icon },
  title: { en, de, fr, nl },
  description: { en, de, fr, nl },
  article: { en, de, fr, nl },

  access: {
      geo: ['de', 'en' ],
      needsSubscription: boolean,
      payedItem: booleam
   },

  name: string,

  contact:
    firstName: string,
    lastName: string
    email: string,
    phone: int,
  },

  auth: {
    password: string, --> MD5
    google: id,
    facebook: id,
    role: {
      id: [id],
      type: 'admin' | 'owner' | 'user' | 'api'?
    }
  },

  age: int,

  price: real,

  geo: {},
  address: { street, city, zip },

  theme: { ... ? },
  ads: {},
  dictionary: {},
  menu: {},
  social: {},
  layout: {
      default: { components: [] },
      match: { components: [] },
      video: { $field: 'layout.match' }
      // '$layout.match' // this is is a ref onnly within your own object - store with a bit mask -- make the api in set powerfull and good for this
  }
}
```

### Special behaviours

- decendants, does not exists just a colleciton of all children (deep) - gaurd against circulair stuff
- ancestors, returns array

## Rules

- Hierarchies need to be configurable
- Custom types (no type custom)
- Pick allowed fields

```bash
.
├── leagueA
│   ├── matchA
│   └── teamA
│       └──matchA
│       └──matchB
├── leagueB
│   ├── matchB
│   └── teamA
│       ├──matchA
│       └──matchB
```

This case is impossible and arbitrary to fix without making custom rules. Imagine adding a `competition` or `tournament` no system will make this work. This means we will need to allow adding types and rules in the cms (first select some good defaults).

- Type indexing dynamic (think about typescrip as well)
- Id map of prefixes for types stable (so extra characters)
- Rule system for hierarchies
- Select fields for types using a json object

Schema example

```javascript
const schema = {
  match: {
    hierarchy: {
      team: { ignore: ['league'] }
    },
    fields: ['start', 'end', 'video', 'image', 'title', 'description']
  }
}
```

These schemas are stored on the db it self and when ancestors get updated , or inherit gets fired it reads this file.

```javascript
client.setSchema({
  flurp: {
    fields: ['value'],
    hierarchy: false
  }
})
```
