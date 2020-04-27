import { GetOptions, Inherit, List, Sort, Find, Filter } from './types'
import { SelvaClient } from '..'

function checkAllowed(props: GetOptions, allowed: Set<string>): true | string {
  for (const key in props) {
    if (!allowed.has(key)) {
      return key
    }
  }

  return true
}

function validateField(
  client: SelvaClient,
  field: string | string[] | { path: string | string[]; value: GetOptions },
  path: string
): void {
  if (typeof field === 'string') {
    return
  }

  if (typeof field === 'object') {
    if (Array.isArray(field)) {
      return
    }

    const allowed = checkAllowed(<GetOptions>field, new Set(['path', 'value']))
    if (allowed !== true) {
      throw new Error(
        `Unsupported option ${allowed} in operator $field for ${path}.$field`
      )
    }

    return validateTopLevel(client, field.value, path)
  }

  throw new Error(
    `Unsupported type in operator $field for ${path}.$field. Required type string, array of strings or object { path: string | string[]; value: GetOptions }`
  )
}

function validateInherit(
  client: SelvaClient,
  inherit: Inherit,
  path: string
): void {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $inherit'
    }

    throw new Error(
      `${mainMsg} for ${path}.$inherit. Required type boolean or object with any of the following singatures: 
        {
          $item: string | string[] (type)
          $required: string | string[] (field name) (optional)
        } 
        or
        {
          $type: string | string[] or $name?: string | string[] but not both (optional)
          $merge: boolean (optional)
        }
    `
    )
  }

  if (typeof inherit === 'boolean') {
    return
  }

  // TODO: check more types in $name and $type
  if (typeof inherit === 'object') {
    if (inherit.$type) {
      if (inherit.$name) {
        err('Both $type and $name are not supported')
      }

      const allowed = checkAllowed(inherit, new Set(['$type', '$merge']))
      if (allowed !== true) {
        err(`Field or operator ${allowed} not allowed in inherit with $type`)
      }

      return
    } else if (inherit.$name) {
      const allowed = checkAllowed(inherit, new Set(['$name', '$merge']))
      if (allowed !== true) {
        err(`Field or operator ${allowed} not allowed in inherit with $name`)
      }

      return
    } else if (inherit.$item) {
      const allowed = checkAllowed(inherit, new Set(['$item', '$required']))
      if (allowed !== true) {
        err(`Field or operator ${allowed} not allowed in inherit with $type`)
      }

      if (!Array.isArray(inherit.$item) && typeof inherit.$item !== 'string') {
        err(`Inherit by $item must target a specific type or array of types`)
      }

      if (
        !Array.isArray(inherit.$required) &&
        typeof inherit.$required !== 'string'
      ) {
        err(
          `In inherit by $type the $required operator must be a field name or array of field names`
        )
      }

      return
    }

    err(`Object for $inherit without furhter operators specified`)
  }

  err()
}

function validateSort(client: SelvaClient, sort: Sort, path: string): void {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $sort'
    }

    throw new Error(
      `${mainMsg} for ${path}.$sort. Required type object with the following properties:
        {
          $field: string
          $order: 'asc' | 'desc' (optional)
        }
    `
    )
  }

  const allowed = checkAllowed(sort, new Set(['$field', '$order']))
  if (allowed !== true) {
    err(`Unsupported operator or field ${allowed}`)
  }

  if (!sort.$field || typeof sort.$field !== 'string') {
    err(`Unsupported type of operator $field with value ${sort.$field}`)
  }

  if (sort.$order) {
    const order = sort.$order.toLowerCase()
    if (order !== 'asc' && order !== 'desc') {
      err(`Unsupported sort order ${sort.$order}, 'asc'|'desc' required`)
    }
  }
}

function validateFilter(
  client: SelvaClient,
  filter: Filter,
  path: string
): void {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $filter'
    }

    throw new Error(
      `${mainMsg} for ${path}. Required type object with the following properties:
        {
          $operator: '=' | '!=' | '>' | '<' | '..'
          $field: string
          $value: string | number | (string | number)[]

          $and: Filter (chain more filters with and clause) (optional)
          $or: Filter (chain more filters with or clause) (optional)
        }
        
        or for geo filters

        {      
          $operator: 'distance'
          $field: string
          $value: {
            $lat: number
            $lon: number
            $radius: number
          }

          $and: Filter (chain more filters with and clause) (optional)
          $or: Filter (chain more filters with or clause) (optional)
        }

        or for exists filter

        {
          $operator: 'exists' | 'notExists'
          $field: string

          $and: Filter (chain more filters with and clause) (optional)
          $or: Filter (chain more filters with or clause) (optional)
        }
    `
    )
  }

  if (!filter.$field || typeof filter.$field !== 'string') {
    err(
      `Filter ${filter.$operator} should have a string field, got ${filter.$field}`
    )
  }

  if (
    filter.$operator !== '=' &&
    filter.$operator !== '!=' &&
    filter.$operator !== '>' &&
    filter.$operator !== '<' &&
    filter.$operator !== '..' &&
    filter.$operator !== 'distance' &&
    filter.$operator !== 'exists' &&
    filter.$operator !== 'notExists'
  ) {
    err(
      `Unsupported $operator ${filter.$operator}, has to be one of =, !=, >, <, .., distance, exists, notExists`
    )
  }

  if (filter.$operator === 'exists' || filter.$operator === 'notExists') {
    if (filter.$value) {
      err(`$value not allowed for filter type 'exists/notExists'`)
    }

    const allowed = checkAllowed(
      filter,
      new Set(['$operator', '$field', '$and', '$or'])
    )

    if (allowed !== true) {
      err(`Unsupported operator or field ${allowed}`)
    }
  } else if (filter.$operator === 'distance') {
    if (!filter.$value || typeof filter.$value !== 'object') {
      err(
        `$value of distance filter should be provided and should be an object with $lat, $lon and $radius'`
      )
    }

    if (!filter.$value.$lat || !filter.$value.$lon || !filter.$value.$radius) {
      err(
        `$value of distance filter should be provided and should be an object with $lat, $lon and $radius'`
      )
    }

    if (typeof filter.$value.$lat !== 'number') {
      err(
        `$value.$lat of distance filter should be provided and should be a number`
      )
    }

    if (typeof filter.$value.$lon !== 'number') {
      err(
        `$value.$lon of distance filter should be provided and should be a number`
      )
    }

    if (typeof filter.$value.$radius !== 'number') {
      err(
        `$value.$radius of distance filter should be provided and should be a number`
      )
    }

    const allowed = checkAllowed(
      filter,
      new Set(['$operator', '$field', '$value', '$and', '$or'])
    )

    if (allowed !== true) {
      err(`Unsupported operator or field ${allowed}`)
    }
  } else if (filter.$operator === '=' || filter.$operator === '!=') {
    if (
      !filter.$value ||
      typeof filter.$value !== 'string' ||
      typeof filter.$value !== 'number' ||
      !Array.isArray(filter.$value)
    ) {
      err(
        `$value of ${filter.$operator} filter should be provided and should be a string or number or an array of strings or numbers'`
      )
    }
  } else if (filter.$operator === '<' || filter.$operator === '>') {
    if (
      !filter.$value ||
      typeof filter.$value !== 'string' ||
      typeof filter.$value !== 'number'
    ) {
      err(
        `$value of ${filter.$operator} filter should be provided and should be a string or number'`
      )
    }
  } else if (filter.$operator === '..') {
    if (
      !filter.$value ||
      typeof filter.$value !== 'string' ||
      typeof filter.$value !== 'number' ||
      !Array.isArray(filter.$value)
    ) {
      err(
        `$value of ${filter.$operator} filter should be provided and should be a string or number or an array of strings or numbers'`
      )
    }
  }

  if (filter.$and) {
    validateFilter(client, filter.$and, path + '.$and')
  }

  if (filter.$or) {
    validateFilter(client, filter.$or, path + '.$or')
  }
}

function validateFind(client: SelvaClient, find: Find, path: string): void {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $find'
    }

    throw new Error(
      `${mainMsg} for ${path}.$sort. Required type object with the following properties:
        {
          $traverse: 'descendants' | 'ancestors' | string | string[] (optional)
          $filter: FilterOptions | FilterOptions[] (and by default) (optional)
          $find: FindOptions (find within results of the find) (optional)


        FilterOptions:
          {
            $operator: '=' | '!=' | '>' | '<' | '..'
            $field: string
            $value: string | number | (string | number)[]
            $and: FilterOptions (adds an additional condition) (optional)
            $or: FilterOptions (adds optional condition) (optional)
          }
        `
    )
  }

  const allowed = checkAllowed(find, new Set(['$traverse', '$filter', '$find']))
  if (allowed !== true) {
    err(`Unsupported operator or field ${allowed}`)
  }

  if (find.$traverse) {
    if (typeof find.$traverse !== 'string' && !Array.isArray(find.$traverse)) {
      err(`Unupported type for $traverse ${find.$traverse}`)
    }
  }

  if (find.$find) {
    validateFind(client, find.$find, path + '.$find')
  }

  if (find.$filter) {
    if (Array.isArray(find.$filter)) {
      for (const filter of find.$filter) {
        validateFilter(client, filter, path + '.$find')
      }
    } else {
      validateFilter(client, find.$filter, path + '.$find')
    }
  }
}

function validateList(client: SelvaClient, list: List, path: string): void {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $list'
    }

    throw new Error(
      `${mainMsg} for ${path}.$list. Required type boolean or object with any of the following properties:
        {
          $offset: number (optional)
          $limit: number (optional)
          $sort: { $field: string, $order?: 'asc' | 'desc' } or array of these sort objects (optional)
          $find: FindOptions (optional) -- see below
          $inherit: InheritOptions (optional) -- see below            
        }

        FindOptions:
          {
            $traverse: 'descendants' | 'ancestors' | string | string[] (optional)
            $filter: Filter | FilterOptions[] (and by default) (optional)
            $find: FindOptions (recursive find to find within the results) (optional) 
          }

        FilterOptions:
          {
            $operator: '=' | '!=' | '>' | '<' | '..'
            $field: string
            $value: string | number | (string | number)[]
            $and: FilterOptions (adds an additional condition) (optional)
            $or: FilterOptions (adds optional condition) (optional)
          }

        // TODO: put these in an object so they don't have to be copied
        InheritOptions:
        true
        or
        {
          $item: string | string[] (type)
          $required: string | string[] (field name) (optional)
        } 
        or
        {
          $type: string | string[] or $name?: string | string[] but not both (optional)
          $merge: boolean (optional)
        }
    `
    )
  }

  if (typeof list === 'boolean') {
    return
  }

  if (typeof list === 'object') {
    for (const field in list) {
      if (!field.startsWith('$')) {
        err(
          `Only operators starting with $ are allowed in $list, ${field} not allowed`
        )
      } else if (field === '$offset') {
        if (typeof list.$offset !== 'number') {
          err(`$offset has to be an number, ${list.$offset} specified`)
        }
      } else if (field === '$limit') {
        if (typeof list.$limit !== 'number') {
          err(`$limit has to be an number, ${list.$limit} specified`)
        }
      } else if (field === '$sort') {
        if (Array.isArray(list.$sort)) {
          for (const sort of list.$sort) {
            validateSort(client, sort, path + '.$list')
          }
        } else {
          validateSort(client, list.$sort, path + '.$list')
        }
      } else if (field === '$find') {
        validateFind(client, list.$find, path + '.$find')
      } else if (field === '$inherit') {
        validateInherit(client, list.$inherit, path + '.$inherit')
      } else {
        err(`Operator ${field} not allowed`)
      }
    }

    return
  }

  err()
}

function validateNested(
  client: SelvaClient,
  props: GetOptions | true,
  path: string
): void {
  if (props === true) {
    // TODO: validate from schema if id?
    return
  }

  if (props.$id || props.$alias) {
    return validateTopLevel(client, props, path)
  }

  for (const field in props) {
    if (field.startsWith('$')) {
      // TODO: validate that options that aren't supported together are not put together
      if (field === '$field') {
        validateField(client, props.$field, path)
      } else if (field === '$inherit') {
        validateInherit(client, props.$inherit, path)
      } else if (field === '$list') {
        validateList(client, props.$list, path)
      } else if (field === '$find') {
        validateFind(client, props.$find, path)
      } else if (field === '$default') {
        // TODO: validate from schema if id?
        continue
      } else if (field === '$all') {
        if (typeof props.$all !== 'boolean') {
          throw new Error(
            `Operator $all for ${path}.$all must be a boolean, got ${props.$all}`
          )
        }
      } else if (field === '$value') {
        // basically anything is allowed in $value
        continue
      } else {
        throw new Error(
          `Operator ${field} is not supported in nested fields for ${path +
            '.' +
            field}`
        )
      }
    }
  }

  for (const field in props) {
    if (!field.startsWith('$')) {
      validateNested(client, props[field], path + '.' + field)
    }
  }
}

function validateTopLevel(
  client: SelvaClient,
  props: GetOptions,
  path: string
): void {
  for (const field in props) {
    if (field.startsWith('$')) {
      if (field === '$id') {
        if (typeof props.$id !== 'string' && !Array.isArray(props.$id)) {
          if (path !== '' && typeof props.$id === 'object') {
            const allowed = checkAllowed(props.$id, new Set(['$field']))
            if (allowed !== true) {
              throw new Error(
                `${path}.$id is an object and with unallowed field ${allowed}, only $field is allowed in $id of nested ueries`
              )
            }

            continue
          }

          if (path !== '') {
            throw new Error(
              `$id ${props.$id} in a nested query should be a string, an array of strings or an object with $field reference`
            )
          } else {
            throw new Error(
              `$id ${props.$id} should be a string or an array of strings`
            )
          }
        }
      } else if (field === '$alias') {
        if (typeof props.$alias !== 'string' && !Array.isArray(props.$alias)) {
          throw new Error(
            `${path}.$alias ${props.$alias} should be a string or an array of strings`
          )
        }
      } else if (field === '$version') {
        if (typeof props.$id !== 'string') {
          throw new Error(`$version should be a string`)
        }
      } else if (field === '$language') {
        if (
          typeof props.$language !== 'string' ||
          !client.schema.languages ||
          !client.schema.languages.includes(props.$language)
        ) {
          throw new Error(
            `$language ${
              props.$language
            } is unsupported, should be a string and one of ${[].join(', ')}`
          )
        }
      } else if (field === '$rawAncestors') {
        if (typeof props.$rawAncestors !== 'boolean') {
          throw new Error(`$rawAncestors should be a boolean value`)
        }
      } else if (field === '$all') {
        if (typeof props.$all !== 'boolean') {
          throw new Error(
            `Operator $all for ${path}.$all must be a boolean, got ${props.$all}`
          )
        }
      } else {
        throw new Error(`
          Top level query operator ${field} is not supported. Did you mean one of the following supported top level query options?
            - $id
            - $alias
            - $all
            - $version
            - $language
          `)
      }
    }
  }

  for (const field in props) {
    if (!field.startsWith('$')) {
      validateNested(client, props[field], path + '.' + field)
    }
  }
}

export default function(client: SelvaClient, props: GetOptions): void {
  validateTopLevel(client, props, '')
}
