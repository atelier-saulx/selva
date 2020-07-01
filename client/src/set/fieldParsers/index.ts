import simple from './simple'
import text from './text'
import geo from './geo'
import references from './references'
import reference from './reference'
import set from './set'
import json from './json'
import object from './object'
import record from './record'
import array from './array'

const fieldParsers = {
  ...simple,
  text,
  geo,
  set,
  reference,
  references,
  json,
  object,
  array,
  record
}

export default fieldParsers
