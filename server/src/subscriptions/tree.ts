import SubscriptionManager from './subsManager'
import { Subscription, SubTree, Tree } from './'
import { prefixes } from '@saulx/selva'

const example = {
  ___refreshAt: Date.now() + 3000,
  ___contains: {
    yUixp: { $field: 'ancestors', $value: ['flapperdrol'] },
    dqX1x: { $field: 'ancestors', $value: ['flapdrol', 'snurkels'] }
  },
  title: {
    en: {
      ___ids: { flupperbalid: true },
      ___types: {
        match: { dqX1x: true }
      },
      ___any: {
        yUixp: true
      }
    }
  }
}

const addTreeNested = (channel: string, targetTree: Tree, tree: SubTree) => {
  for (const key in tree) {
    if (tree[key] === true) {
      if (!targetTree[key]) {
        targetTree[key] = new Set()
      }
      targetTree[key].add(channel)
    } else {
      if (!targetTree[key]) {
        targetTree[key] = {}
      }
      addTreeNested(channel, targetTree[key], tree[key])
    }
  }
}

const addToTree = (channel: string, targetTree: Tree, tree: SubTree) => {
  for (const key in tree) {
    if (key === '___refreshAt') {
    } else if (key === '___contains') {
    } else {
      addTreeNested(
        channel,
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

const removeTreeNested = (channel: string, targetTree: Tree, tree: SubTree) => {
  let removed
  for (const key in tree) {
    if (targetTree[key]) {
      if (tree[key] === true) {
        targetTree[key].delete(channel)
        if (targetTree[key].size === 0) {
          delete targetTree[key]
          removed = true
        }
      } else if (!(targetTree[key] instanceof Set)) {
        if (removeTreeNested(channel, targetTree[key], tree[key])) {
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

const removeFromTree = (channel: string, targetTree: Tree, tree: SubTree) => {
  for (const key in tree) {
    if (key === '___refreshAt') {
    } else if (key === '___contains') {
    } else {
      if (targetTree[key]) {
        if (!removeTreeNested(channel, targetTree[key], tree[key])) {
          if (isEmpty(targetTree[key])) {
            delete targetTree[key]
          }
        }
      }
    }
  }
}

// updateTree has to happen

export function addSubscriptionToTree(
  subsmanager: SubscriptionManager,
  channel: string,
  subscription: Subscription
) {
  if (channel === prefixes.schemaSubscription) {
    console.log('add schema')
  } else {
    subsmanager.tree.title = {
      de: {
        ___ids: { flapsie: new Set(['suppie']) }
      }
    }

    console.log('OK ADD IT', subscription)
    let { tree } = subscription
    tree = example

    if (tree.___refreshAt) {
      console.log('ADD REFRESH LISTENER')
    }

    addToTree(channel, subsmanager.tree, tree)
    console.dir(subsmanager.tree, { depth: 10 })
  }
}

export function removeSubscriptionFromTree(
  subsmanager: SubscriptionManager,
  channel: string,
  subscription: Subscription
) {
  console.log('REMOVE DAT', channel)
  if (channel === prefixes.schemaSubscription) {
    console.log('remove schema')
  } else {
    let { tree } = subscription
    tree = example

    if (tree.___refreshAt) {
      console.log('REMOVE REFRESH LISTENER')
    }

    removeFromTree(channel, subsmanager.tree, tree)
    console.dir(subsmanager.tree, { depth: 10 })
  }
}
