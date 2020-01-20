// field is just a wrapper exverywhere as well

// in inherit (looks up)
// not in inherit does not use the field when looking up

// make that dynamic schema everywhere - will make a big impact (types!)

// auto generate validators

// back up solution db-server

// schema needs to update indexes!

// reference and fields

// then queries
// subscription etc

// --> .client.observe({ }) // (DATA, [where did it change]) => {}
// kill observation

// version!

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

// "required": [ "productId", "productName", "price" ]

const schema = {
  match: {
    hierarchy: {
      team: { ignoreAncestryWith: ['league'] },
      video: false,
      person: { includeAncestryWith: ['family'] },
      '$default': { ignoreAncestryWith: ['vehicle'] }
    },
   "fields": {  
    "video":  {
        "type": "object",
        "properties": {
            "mp4": {
            "type": "url"
            },
            "hls": {
            "type": "url",
            },
            "overlays": {
                "type": "array",
                "items": {
                    "type": "object",
                    "properties": {
                        "interval": { type: "array", "items": {
                            type: "timestamp"
                        }},
                        "url": { type: "url" }
                    }
                }
            }
        }
    }
}

// stringified is json (array or json)
 
// TYPES
// float
// int
// number
// json
// array (stored as json)
// references (set of ids)
// set
// string
// object
// text
// id // reasonable length etc
// digest (password etc)
// timestamp (ms)
// url
// email
// phone 
// geo

// *array <--- can do it but later

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
