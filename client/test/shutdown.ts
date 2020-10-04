import test from 'ava'
import { start } from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'
import { spawn, execSync } from 'child_process'

test.serial('Should find redis servers', async t => {
  const port = await getPort()
  const srv = await start({ port })

  await new Promise(resolve => setTimeout(resolve, 1000))

  t.notThrows(() => execSync(`lsof -n -i :${port} | grep LISTEN`))
  await srv.destroy()
  await new Promise(resolve => setTimeout(resolve, 1000))
})

test.serial('Should close redis after destroy', async t => {
  const port = await getPort()
  const srv = await start({ port })
  await srv.destroy()

  await new Promise(resolve => setTimeout(resolve, 1000))

  t.throws(() => execSync(`lsof -n -i :${port} | grep LISTEN`))
})

// does not work - make better test!
test.serial.skip('Should handle SIGTERM', async t => {
  t.plan(1)
  const info: any = await new Promise((resolve, reject) => {
    const spawned = spawn('node', ['test/assertions/selvaProcessRunner.js'], {
      detached: true
    })
    spawned.stdout.on('data', data => {
      // resolve(JSON.parse(data.toString()))
    })
  })
  // console.log(info)

  await new Promise(resolve => setTimeout(resolve, 1000))

  execSync(`kill -2 ${info.pid}`)

  await new Promise(resolve => setTimeout(resolve, 6000))

  t.throws(() => execSync(`lsof -n -i :${info.port} | grep LISTEN`).toString())
})
