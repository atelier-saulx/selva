import simple from './simple'
import text from './text'
import geo from './geo'
import references from './references'
import set from './set'
import json from './json'
import object from './object'
import array from './array'

const fieldParsers = {
  ...simple,
  text,
  geo,
  set,
  references,
  json,
  object,
  array
}

export default fieldParsers
