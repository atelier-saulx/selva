import SubscriptionManager from './subsManager'
import { Subscription, SubTree, Tree } from './'
import { prefixes } from '@saulx/selva'
import * as now from './now'
// const example = {
//   ___refreshAt: Date.now() + 3000,
//   ___contains: {
//     yUixp: { $field: 'ancestors', $value: ['flapperdrol'] },
//     dqX1x: { $field: 'ancestors', $value: ['flapdrol', 'snurkels'] }
//   },
//   title: {
//     en: {
//       ___ids: { flupperbalid: true },
//       ___types: {
//         match: { dqX1x: true }
//       },
//       ___any: {
//         yUixp: true
//       }
//     }
//   }
// }

const addTreeNested = (
  subscription: Subscription,
  targetTree: Tree,
  tree: SubTree
) => {
  for (const key in tree) {
    if (tree[key] === true) {
      if (!targetTree[key]) {
        targetTree[key] = new Set()
      }
      targetTree[key].add(subscription)
    } else {
      if (!targetTree[key]) {
        targetTree[key] = {}
      }
      addTreeNested(subscription, targetTree[key], tree[key])
    }
  }
}

const addToTree = (
  subscription: Subscription,
  targetTree: Tree,
  tree: SubTree
) => {
  for (const key in tree) {
    if (key === '___refreshAt') {
      // not handled in tree
    } else if (key === '___contains') {
      for (let k in tree.___contains) {
        if (!targetTree.___contains) {
          targetTree.___contains = {}
        }
        // add count
        targetTree.___contains[k] = tree.___contains[k]
      }
    } else {
      addTreeNested(
        subscription,
        targetTree[key] || (targetTree[key] = {}),
        tree[key]
      )
    }
  }
}

const isEmpty = (tree: Tree): boolean => {
  for (const _x in tree) {
    return false
  }
  return true
}

const removeTreeNested = (
  subscription: Subscription,
  targetTree: Tree,
  tree: SubTree
) => {
  let removed
  for (const key in tree) {
    if (targetTree[key]) {
      if (tree[key] === true) {
        targetTree[key].delete(subscription)
        if (targetTree[key].size === 0) {
          delete targetTree[key]
          removed = true
        }
      } else if (!(targetTree[key] instanceof Set)) {
        if (removeTreeNested(subscription, targetTree[key], tree[key])) {
          if (isEmpty(targetTree[key])) {
            delete targetTree[key]
            removed = true
          }
        }
      }
    }
  }
  return removed
}

const removeFromTree = (
  subscription: Subscription,
  targetTree: Tree,
  tree: SubTree
) => {
  for (const key in tree) {
    if (key === '___refreshAt') {
    } else if (key === '___contains') {
      // merge on top
    } else {
      if (targetTree[key]) {
        if (!removeTreeNested(subscription, targetTree[key], tree[key])) {
          if (isEmpty(targetTree[key])) {
            delete targetTree[key]
          }
        }
      }
    }
  }
}

export function addSubscriptionToTree(
  subsmanager: SubscriptionManager,
  subscription: Subscription
) {
  const channel = subscription.channel
  if (channel === prefixes.schemaSubscription) {
    console.log('add schema')
  } else {
    let { tree } = subscription

    if (!tree) {
      console.error('No tree on subscription', subscription)
    } else {
      console.log('add sub tree', subscription.get, tree)

      if (tree.___refreshAt) {
        subscription.refreshAt = tree.___refreshAt
        now.addSubscription(subsmanager, subscription)
      }
      addToTree(subscription, subsmanager.tree, tree)
    }
  }
}

export function removeSubscriptionFromTree(
  subsmanager: SubscriptionManager,
  subscription: Subscription
) {
  const channel = subscription.channel
  if (channel === prefixes.schemaSubscription) {
    console.log('remove schema')
  } else {
    let { tree } = subscription
    subsmanager.stagedForUpdates.delete(subscription)
    if (!tree) {
      // console.error('No tree on subscription', subscription)
    } else {
      if (tree.___refreshAt) {
        delete subscription.refreshAt
        now.removeSubscription(subsmanager, subscription)
      }
      removeFromTree(subscription, subsmanager.tree, tree)
    }
  }
}
