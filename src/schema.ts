export type Id = string

export type Url = string

export type Timestamp = number

export type Overlay = {
  interval?: number[]
  src: Url
}

// so sad would be nice to have the languages hardcoded here :( <- sad

export type UserType = 'admin' | 'owner' | 'user'

export type Geo = {
  lat: number
  long: number
}

export type ExternalId = string | number

export type Address = {
  street: string
  number: number
  zip: number
  city: string
  region: string
  country: string
}

export type Location = {
  address?: Address
  geo?: Geo
}

// fill in later
// fonts
// color
// graphic style
export type Theme = {}

export type Ads = {}

export type Dictionary = {}

export type Menu = {}

export type Social = {
  instagram: Url
  facebook: Url
  twitter: Url
}

// any type as key?

export const languages = ['en', 'de', 'fr', 'nl', 'es', 'it', 'fi']

// order deterines fallback order
export type Language = 'en' | 'de' | 'fr' | 'nl' | 'es' | 'it' | 'fi'

// would be nice to generate this form a list of lang keys <-- so sad :(
export type Text = {
  en?: string
  de?: string
  fr?: string
  nl?: string
  es?: string
  it?: string
  fi?: string
}

export type Type =
  | 'person'
  | 'character'
  | 'organisation'
  | 'club'
  | 'match'
  | 'federation'
  | 'league'
  | 'video'
  | 'team'
  | 'genre'
  | 'movie'
  | 'show'
  | 'event'
  | 'location'
  | 'sport'
  | 'camera'
  | 'category'
  | 'tag'
  | 'ad'
  | 'root'
  | 'custom'
  | 'article'
  | 'product'
  | 'animal'

// not the place to define the component type - too much limitation for front end
export type Component = {
  type: string // List, Grid
  props: { [key: string]: any }
}

export type Layout = Record<
  Type | 'default',
  {
    components: Component[]
  }
>

// can use this in the cms
export const itemTypes = [
  'person',
  'character',
  'organisation',
  'club',
  'match',
  'federation',
  'league',
  'video',
  'team',
  'genre',
  'movie',
  'show',
  'event',
  'location',
  'sport',
  'camera',
  'category',
  'tag',
  'ad',
  'root',
  'custom',
  'article',
  'product',
  'animal'
]

export const typePrefix = {}
export const inverseTypePrefix = {}

export const getTypeFromId = id => {
  if (id === 'root') {
    return 'root'
  } else {
    return typePrefix[id.slice(0, 2)]
  }
}

// map needs to be added!
const createPrefix = (type: Type, index: number): string => {
  if (index > type.length) {
    return createPrefix(type, 0)
  }
  let prefix = type.slice(index, index + 2)
  if (typePrefix[prefix]) {
    return createPrefix(type, ++index)
  }
  inverseTypePrefix[type] = prefix
  typePrefix[prefix] = type
  return prefix
}

itemTypes.forEach((type: Type) => createPrefix(type, 0))

/*  
  // json ld it
*/

export type BaseItem = {
  id?: string
  type?: Type
  url?: string[]
  externalUrl?: string
  user?: Id // from the user db
  date?: Timestamp
  start?: Timestamp
  end?: Timestamp
  published?: boolean
  // frozen?: boolean // maybe - just means you dont see it in the cms
  status?: number
  video?: {
    hls?: Url
    mp4?: Url
    overlays?: Overlay[]
  }
  image?: {
    thumb?: Url
    poster?: Url
    cover?: Url
    icon?: Url
    banner?: Url
  }
  title?: Text
  description?: Text
  article?: Text
  access?: {
    allowGeo?: Language[]
    needsSubscription?: boolean
    payedItem?: boolean
  }
  name?: string
  contact?: {
    firstName?: string
    lastName?: string
    email?: string
    phone?: number
    address?: string
  }
  value?: number
  age?: number // only for person and wine and whiskey
  price?: number
  discount?: number
  tax?: number
  location?: Location
  theme?: Theme
  ads?: Ads
  dictionary?: Dictionary
  menu?: Menu
  social?: Social
  layout?: Layout
}

export type Ad = {
  type: 'ad'
} & Pick<
  BaseItem,
  | 'value'
  | 'image'
  | 'video'
  | 'name'
  | 'access'
  | 'title'
  | 'externalUrl'
  | 'user'
  | 'contact'
> & {
    // url is a bit different
    adType?: 'dfp' | 'custom'
    adUnit?: string
    sizes?: { width: number; height: number }[]
  }

export type Person = {
  type: 'person'
} & Omit<BaseItem, 'id' | 'name' | 'status' | 'location'> & {
    age?: number
  }

// Omit

// make this super nice
// export type AllowField = 'value' | 'name' | 'contact.firstName' | 'contact'

export type Field = string | string[]

export type Item = BaseItem & {
  children?: Id[]
  parents?: Id[]
  ancestors?: string
  descendants?: Id[]
  externalId?: ExternalId[]
  auth?: {
    password?: string
    google?: string
    facebook?: string
    role?: {
      id?: Id[]
      type?: UserType
    }
  }
}
