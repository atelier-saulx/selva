import { GetOperation, GetOptions } from '../types'
import find from './find'

const list = (props: GetOptions, id: string, field: string): GetOperation => {
  if (props.$list === true) {
    return {
      type: 'find',
      id,
      props,
      field: field.substr(1),
      sourceField: <string>props.$field || field.substr(1),
      options: {
        limit: -1, // no limit
        offset: 0
      }
    }
  } else if (props.$list.$find) {
    return find(
      props.$list.$find,
      props,
      id,
      field,
      false,
      props.$list.$limit,
      props.$list.$offset,
      props.$list.$sort
    )
  } else {
    return {
      type: 'find',
      id,
      props,
      field: field.substr(1),
      sourceField: <string>props.$field || field.substr(1),
      options: {
        limit: props.$list.$limit || -1,
        offset: props.$list.$offset || 0,
        sort: Array.isArray(props.$list.$sort)
          ? props.$list.$sort[0]
          : props.$list.$sort || undefined
      }
    }
  }
}

export default list
