const fs = require('fs')
const redis = require('redis')
const path = require('path')

// TODO: use replica fro all read operations if type !== subs manager && type !== '

let template = `
type args = (string | number | Buffer)[]
import { RedisCommand } from '../types'
import { ServerSelector } from '../../types'

abstract class RedisMethods {
  abstract addCommandToQueue(
    redisCommand: RedisCommand,
    opts?: ServerSelector
  ): void
  async command(opts: ServerSelector, key: string, ...args: args): Promise<any>
  async command(key: string, ...args: args): Promise<any>
  async command(opts: any, ...args: args): Promise<any> {
    if (typeof opts === 'object') {
      return new Promise((resolve, reject) => {
        const [key, ...commandArgs] = args
        this.addCommandToQueue({ command: <string>key, args: commandArgs, resolve, reject }, opts)
      })
    } else {
      return new Promise((resolve, reject) => {
        this.addCommandToQueue({ command: opts, args, resolve, reject })
      })
    }
  }
  $methods
}

export default RedisMethods
`

const methods = []

// redis.add_command(`FT.${cmd}`)
redis.add_command('selva.hierarchy.addconstraint')
redis.add_command('selva.hierarchy.listconstraints')
redis.add_command('selva.hierarchy.aggregate')
redis.add_command('selva.hierarchy.aggregatein')
redis.add_command('selva.hierarchy.children')
redis.add_command('selva.hierarchy.del')
redis.add_command('selva.hierarchy.edgeget')
redis.add_command('selva.hierarchy.edgegetmetadata')
redis.add_command('selva.hierarchy.edgelist')
redis.add_command('selva.hierarchy.find')
redis.add_command('selva.hierarchy.parents')
redis.add_command('selva.hierarchy.compress')
redis.add_command('selva.hierarchy.listcompressed')
redis.add_command('selva.hierarchy.types.add')
redis.add_command('selva.hierarchy.types.clear')
redis.add_command('selva.hierarchy.types.list')
redis.add_command('selva.hierarchy.ver')
redis.add_command('selva.inherit')
redis.add_command('selva.modify')
redis.add_command('selva.update')
redis.add_command('selva.resolve.nodeid')
redis.add_command('selva.object.del')
redis.add_command('selva.object.exists')
redis.add_command('selva.object.get')
redis.add_command('selva.object.len')
redis.add_command('selva.object.set')
redis.add_command('selva.object.type')
redis.add_command('selva.object.getmeta')
redis.add_command('selva.object.setmeta')
redis.add_command('selva.subscriptions.add')
redis.add_command('selva.subscriptions.addAlias')
redis.add_command('selva.subscriptions.addMissing')
redis.add_command('selva.subscriptions.addTrigger')
redis.add_command('selva.subscriptions.debug')
redis.add_command('selva.subscriptions.del')
redis.add_command('selva.subscriptions.delmarker')
redis.add_command('selva.subscriptions.list')
redis.add_command('selva.subscriptions.listMissing')
redis.add_command('selva.subscriptions.refresh')
redis.add_command('selva.index.list')
redis.add_command('selva.index.new')
redis.add_command('selva.index.del')
redis.add_command('selva.rpn.evalbool')
redis.add_command('selva.rpn.evaldouble')
redis.add_command('selva.rpn.evalstring')
redis.add_command('selva.log.dbg')
redis.add_command('selva.log.dbglist')
const proto = redis.RedisClient.prototype
for (const key in redis.RedisClient.prototype) {
  if (/[A-Z]/.test(key[0]) && typeof proto[key] === 'function') {
    const command = key.toLowerCase()
    if (command === 'command' || !/^([a-z_])+$/.test(command)) {
      continue
    }

    methods.push(
      `
async ${command}(opts: ServerSelector, ...args: args): Promise<any>
async ${command}(...args: args): Promise<any>
async ${command}(opts: any, ...args: args): Promise<any> {
  if (typeof opts === 'object') {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: '${command}', args, resolve, reject }, opts)
    })
  } else {
    return new Promise((resolve, reject) => {
      this.addCommandToQueue({ command: '${command}', args: [opts, ...args], resolve, reject })
    })
  }
}
`
    )
  }
}

template = template.replace('$methods', methods.join('\n'))

fs.writeFileSync(path.join(__dirname, '../index.ts'), template)
