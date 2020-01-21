import simple from './simple'
import text from './text'
import references from './references'
import set from './set'
import json from './json'
import object from './object'

const fieldParsers = {
  ...simple,
  text,
  set,
  references,
  json,
  object
}

export default fieldParsers
