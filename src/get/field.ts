// field is just a wrapper exverywhere as well

// in inherit (looks up)
// not in inherit does not use the field when looking up

// make that dynamic schema everywhere - will make a big impact

// maybe some pair programming to start

// schema needs to update indexes!

/*

All hierarchy options

// omit and pick?
hierarchy: {
    team: { ignore: ['league'] }
},

// ignores team as an ancestor
hierarchy: {
    team: { ignore: true }
},


// specific for certain fields :/? e.g where to inherit al from
hierarchy: {
    team: { ignore: ['league'] }
},

hierarchy: false // no inheritance ancestors whatsoever

const schema = {
  match: {
    hierarchy: {
      team: { ignore: ['league'] }
    },
    fields: [
      'start',
      'end',
      'video',
      'image',
      'title',
      'description',
      { field: 'value', type: 'number' }
    ]
  }
}

// client.getSchema()
```

These schemas are stored on the db itself and when ancestors get updated , or inherit gets fired it reads this file. Schemas are also used to do dynamic input validation.

```javascript
client.setSchema({
  flurp: {
    fields: ['value'],
    hierarchy: false
  }
})
```

*/
