import test from 'ava'
import {
  startOrigin,
  startRegistry,
  startSubscriptionManager,
  startSubscriptionRegistry,
} from '@saulx/selva-server'
import { wait, worker } from './assertions'
import getPort from 'get-port'

test.serial('destroy works', async (t) => {
  const port = await getPort()
  const selvas = await Promise.all([
    startRegistry({
      port,
    }),
    startOrigin({
      default: true,
      registry: { port },
    }),
    startSubscriptionRegistry({ registry: { port } }),
    startSubscriptionManager({ registry: { port } }),
  ])
  await wait(1e3)
  await Promise.all(selvas.map((selva) => selva.destroy()))
  await wait(1e3)
  await t.connectionsAreEmpty()
})

test.serial('destroy works in workers', async (t) => {
  const port = await getPort()
  const res = await Promise.all([
    worker(
      async ({ startRegistry }, { port }) => {
        const s = await startRegistry({
          port,
        })
        return () => {
          s.destroy()
        }
      },
      { port }
    ),

    worker(
      async ({ startOrigin }, { port }) => {
        const s = await startOrigin({
          default: true,
          registry: { port },
        })
        return () => s.destroy()
      },
      { port }
    ),

    worker(
      async ({ startSubscriptionRegistry }, { port }) => {
        const s = await startSubscriptionRegistry({ registry: { port } })
        return () => s.destroy()
      },
      { port }
    ),

    worker(
      async ({ startSubscriptionManager }, { port }) => {
        const s = await startSubscriptionManager({ registry: { port } })
        return () => s.destroy()
      },
      { port }
    ),
  ])
  await wait(1e3)
  await Promise.all(res.map(([, , kill]) => kill()))
  await wait(1e3)
  await t.connectionsAreEmpty()
})
