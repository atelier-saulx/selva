import { Id, FieldSchemaOther } from '~selva/schema/index'
import { markForAncestorRecalculation } from './ancestors'
import * as r from '../redis'
import sendEvent from './events'
import { stringEndsWith, splitString, stringStartsWith } from 'lua/src/util'
import { getSchema } from 'lua/src/schema/index'
import { getTypeFromId } from 'lua/src/typeIdMapping'
import * as logger from '../logger'

export function cleanUpSuggestions(id: string, field: string) {
  const schema = getSchema()
  const base = field.substr(0, field.length - 3)
  const type = id === 'root' ? schema.rootType : schema.types[getTypeFromId(id)]

  if (!type) {
    return
  }

  const split = splitString(base, '.')

  if (type.fields) {
    let prop: any = type.fields[split[0]]
    for (let i = 1; i < split.length; i++) {
      if (!prop.properties) {
        break
      }

      prop = prop.properties[split[i]]
    }

    if (!prop) {
      return
    }

    const fieldSchema = <FieldSchemaOther>prop
    if (
      fieldSchema.type === 'text' &&
      fieldSchema.search &&
      fieldSchema.search !== true &&
      fieldSchema.search.type[0] === 'TEXT-LANGUAGE-SUG'
    ) {
      const langs = schema.languages
      if (langs) {
        for (const lang of langs) {
          const content = r.hget(id, base + '.' + lang)
          if (content) {
            const words = splitString(content.toLowerCase(), ' ')
            for (let i = words.length - 1; i >= 0; i--) {
              let searchTerms: string = ''
              for (let j = 0; j <= i; j++) {
                searchTerms += words[j] + ' '
              }

              const str = searchTerms.substr(0, searchTerms.length - 1)

              const current: number = redis.call(
                'hincrby',
                `sug_${lang}_counts`,
                str,
                '-1'
              )

              if (current === 0) {
                redis.call('hdel', `sug_${lang}_counts`, str)
                redis.pcall('ft.sugdel', `sug_${lang}`, str)
              } else {
              }
            }
          }
        }
      }
    }
  }
}

function cleanUpAliases(id: Id): void {
  const itemAliases = r.smembers(id + '.aliases')
  for (const alias of itemAliases) {
    r.hdel('___selva_aliases', alias)
  }
}

export function deleteItem(id: Id, hierarchy: boolean = true): boolean {
  if (hierarchy) {
    const children = r.smembers(id + '.children')
    const parents = r.smembers(id + '.parents')
    for (let parent of parents) {
      r.srem(parent + '.children', id)
    }
    for (let child of children) {
      const key = child + '.parents'
      r.srem(key, id)
      const size = r.scard(key)
      if (size === 0) {
        deleteItem(child)
      } else {
        markForAncestorRecalculation(child)
      }
    }
  }

  redis.pcall('FT.DEL', 'default', id)
  r.del(id + '.children')
  r.del(id + '.parents')
  r.del(id + '.ancestors')
  r.del(id + '._depth')

  cleanUpAliases(id)

  sendEvent(id, '', 'delete')

  const vals = r.hgetall(id)
  for (let i = 0; i < vals.length; i += 2) {
    // FIXME: a bit hacky, always assumes we have english enabled
    if (
      stringEndsWith(vals[i], '.en') &&
      !stringStartsWith(vals[i], '___escaped:') &&
      !stringStartsWith(vals[i], '$source_')
    ) {
      cleanUpSuggestions(id, vals[i])
    }

    // found a set value, cleaning up the set key
    if (vals[i + 1] === '___selva_$set') {
      r.del(id + '.' + vals[i])
    }
  }

  // returns true if it existed
  return r.del(id) > 0
}
