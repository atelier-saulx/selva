import { SchemaOptions } from '@saulx/selva'

export const schema:SchemaOptions = {
  languages: ['en'],
  types: {
    movie: {
      prefix: 'mo',
      fields: {
        title: { type: 'string' },
        year: { type: 'int' },
        director: { type: 'string' },
        technicalData: {
          type: 'object',
          properties: {
            runtime: { type: 'int' },
            color: { type: 'string' },
            aspectRatio: { type: 'string' }
          }
        }
      }
    },
    person: {
      prefix: 'pe',
      fields: {
        name: { type: 'string' },
        born: { type: 'int' },
        died: { type: 'int' }
      }
    }
  }
}
