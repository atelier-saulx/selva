import { SelvaClient } from '@saulx/selva'

export const setDataSet = async (client:SelvaClient) => {
  await Promise.all([
    {
      $id: 'peCharltonHeston',
      name: 'Charlton Heston',
      born: 1923,
      died: 2008
    },
    {
      $id: 'peLeighTaylorYoung',
      name: 'Leigh Taylor-Young',
      born: 1945
    },
    {
      $id: 'peChuckConnors',
      name: 'Chuck Connors',
      born: 1921,
      died: 1992
    },
  ].map(person => client.set({ $language: 'en', ...person})))

  await Promise.all([
    {
      $id: 'mo2001ASpaceOdyssey',
      title: {
        en: '2001: A Space Odyssey',
        nl: '2001: Een zwerftocht in de ruimte'
      },
      year: 1968,
      director: 'Stanley Kubrick',
      technicalData: {
        runtime: 149,
        color: 'true',
        aspectRatio: '2.20:1'
      }
    },
    {
      $id: 'moSoylentGreen',
      title: {
        en: 'Soylent Green',
        nl: 'Groen Rantsoen'
      },
      year: 1973,
      director: 'Richard Fleischer',
      technicalData: {
        runtime: 97,
        color: 'true',
        aspectRatio: '2.35:1'
      },
      children: [
        'peCharltonHeston',
        'peLeighTaylorYoung',
        'peChuckConnors'
      ]
    },
    {
      $id: 'moMetropolis',
      title: { en: 'Metropolis' },
      year: 1927,
      director: 'Fritz Lang',
      technicalData: {
        runtime: 153,
        color: 'false',
        aspectRatio: '1.33:1'
      },
      // actors: [
      //   'Charlton Heston',
      //   'Leigh Taylor-Young'
      // ]
    }
  ].map(movie => client.set(movie)))
}
