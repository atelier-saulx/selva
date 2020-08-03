import { Id, FieldSchemaOther } from '~selva/schema/index'
import * as r from '../redis'
import sendEvent from './events'
import {
  stringEndsWith,
  splitString,
  stringStartsWith,
  joinString
} from 'lua/src/util'
import { getSchema } from 'lua/src/schema/index'
import { getTypeFromId } from 'lua/src/typeIdMapping'
import * as logger from '../logger'

function cleanUpAliases(id: Id): void {
  const itemAliases = r.zrange(id + '.aliases')
  for (const alias of itemAliases) {
    r.hdel('___selva_aliases', alias)
  }
}

export function deleteItem(id: Id, hierarchy: boolean = true): boolean {
  if (hierarchy) {
    r.delnode(id)
    //for (let parent of parents) {
    //  // TODO: needed a parent update - slow now fix later
    //  sendEvent(parent, 'children', 'update')
    //}
  }

  //const vals = r.hgetall(id)
  //logger.info('rrrefer', id, vals)
  //const existingFields: string[] = []
  //for (let i = 0; i < vals.length; i += 2) {
  //  // FIXME: a bit hacky, always assumes we have english enabled
  //  if (
  //    !stringStartsWith(vals[i], '___escaped') &&
  //    !stringStartsWith(vals[i], '$source_')
  //  ) {
  //    existingFields[existingFields.length] = vals[i]
  //  }

  //  // found a set value, cleaning up the set key
  //  if (vals[i + 1] === '___selva_$set') {
  //    r.del(id + '.' + vals[i])
  //  }
  //}
  //logger.info('exis', existingFields)

  //sendEvent(id, '', 'delete:' + joinString(existingFields, ','))
  return true
}
