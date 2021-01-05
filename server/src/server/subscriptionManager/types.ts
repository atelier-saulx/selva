import { GetOptions, SelvaClient, ServerDescriptor } from '@saulx/selva'
import { Worker } from 'worker_threads'

export type Tree = Record<string, any>

export type SubTree = Record<string, any>

export type RefreshSubscriptions = {
  nextRefresh: number
  subscriptions: Subscription[]
}

export type Subscription = {
  clients: Set<string>
  originDescriptors: Record<string, ServerDescriptor>
  get: GetOptions
  version?: number
  tree?: SubTree
  treeVersion?: number
  inProgress?: boolean
  channel: string
  refreshAt?: number
  origins: string[]
  processNext?: boolean
  beingProcessed?: boolean
}

export type SubscriptionManager = {
  client: SelvaClient
  inProgressCount: number
  incomingCount: number
  stagedForUpdates: Set<Subscription>
  stagedInProgess: boolean
  stagedTimeout?: NodeJS.Timeout
  memberMemCacheSize: number
  // cache by database name and by field
  memberMemCache: Record<string, Record<string, Record<string, true>>>
  // to check if the server is still ok
  refreshNowQueriesTimeout?: NodeJS.Timeout
  // revalidates subs ones in a while
  revalidateSubscriptionsTimeout?: NodeJS.Timeout
  refreshSubscriptions?: RefreshSubscriptions
  clients: Record<string, { lastTs: number; subscriptions: Set<string> }>
  subscriptions: Record<string, Subscription>
  tree: Tree
  selector: { port: number; host: string }
  originListeners: Record<
    string,
    {
      subscriptions: Set<Subscription>
      listener: (...args: any[]) => void
      reconnectListener: (descriptor: ServerDescriptor) => void
    }
  >
}

// use this so we can reconnect the state on dc
export type SubscriptionManagerState = { worker?: Worker }
