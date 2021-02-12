export const schema: any = {
  languages: ['en', 'nl'],
  types: {
    genre: {
      prefix: 'ge',
      fields: {
        name: { type: 'text' },
        icon: { type: 'string' },
      },
    },
    movie: {
      prefix: 'mo',
      fields: {
        title: { type: 'text' },
        year: { type: 'int', search: true },
        director: { type: 'string' },
        icon: { type: 'string' },
        technicalData: {
          type: 'object',
          properties: {
            runtime: { type: 'int' },
            color: { type: 'string' },
            aspectRatio: { type: 'string' },
          },
        },
      },
    },
    person: {
      prefix: 'pe',
      fields: {
        name: { type: 'string' },
        born: { type: 'int' },
        died: { type: 'int' },
      },
    },
  },
}
