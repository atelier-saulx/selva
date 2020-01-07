export type id = string

export type url = string

export type timestamp = number

export type overlay = {
  interval: undefined | number[]
  src: url
}

export type hmac = string

// so sad would be nice to have the languages hardcoded here :( <- sad
export type language = string

export type userType = string

export type geo = {
  lat: number
  long: number
}

export type location = {
  adress:
    | {
        street: string
        number: number
        zip: number
        city: string
        region: string
        country: string
      }
    | undefined
  geo: geo | undefined
}

// fill in later
// fonts
// color
// graphic style
export type theme = {}

export type ads = {}

export type dictionary = {}

export type menu = {}

export type social = {
  instagram: url
  facebook: url
  twitter: url
}

// any type as key?
export type layout = {}

// would be nice to generate this form a list of lang keys <-- so sad :(
export type text = {
  en: string | undefined
  de: string | undefined
  fr: string | undefined
  nl: string | undefined
  es: string | undefined
  it: string | undefined
  fi: string | undefined
}

export type Item = {
  id: string
  type: string
  url: id[]
  children: id[]
  ancestors: id[]
  parents: id[]
  externalId: (string | number)[]
  date: timestamp
  start: timestamp | undefined
  end: timestamp | undefined
  published: boolean
  status: number | undefined
  video: {
    hls: url | undefined
    mp4: url | undefined
    overlays: overlay[] | undefined
  }
  image: {
    thumb: url | undefined
    poster: url | undefined
    cover: url | undefined
    icon: url | undefined
  }
  title: text
  description: text
  article: text
  access: {
    geo: language[] | undefined
    needsSubscription: boolean
    payedItem: boolean
  }
  name: string | undefined
  contact: {
    firstName: string
    lastName: string
    email: string
    phone: number
  }
  auth: {
    password: hmac | undefined
    google: string | undefined
    facebook: string | undefined
    role:
      | {
          id: id[]
          type: userType
        }
      | {}
  }
  age: number | undefined
  price: number | undefined
  location: location | undefined

  theme: theme | undefined
  ads: ads | undefined
  dictionary: dictionary | undefined
  menu: menu | undefined
  social: social | undefined
  layout: layout | undefined
}
