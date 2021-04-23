import * as redis from 'redis'

const redisSearchCommands = [
  'CREATE',
  'ADD',
  'ADDHASH',
  'ALTER',
  'INFO',
  'SEARCH',
  'AGGREGATE',
  'EXPLAIN',
  'DEL',
  'GET',
  'DROP',
  'SUGADD',
  'SUGGET',
  'SUGDEL',
  'SUGLEN',
  'SYNADD',
  'SYNUPDATE',
  'SYNDUMP',
  'SPELLCHECK',
  'DICTADD',
  'DICTDEL',
  'DICTDUMP',
  'TAGVALS',
  'CONFIG',
]
const selva_commands = [
  'selva.id',
  'selva.modify',
  'selva.object.del',
  'selva.object.exists',
  'selva.object.get',
  'selva.object.len',
  'selva.object.set',
  'selva.object.type',
  'selva.object.getmeta',
  'selva.object.setmeta',
  'selva.hierarchy.parents',
  'selva.hierarchy.children',
  'selva.hierarchy.edgelist',
  'selva.hierarchy.edgeget',
  'selva.hierarchy.find',
  'selva.hierarchy.findrecursive',
  'selva.hierarchy.findin',
  'selva.hierarchy.findinsub',
  'selva.hierarchy.del',
  'selva.inherit',
  'selva.resolve.nodeid',
  'selva.subscriptions.add',
  'selva.subscriptions.addmarkerfields',
  'selva.subscriptions.addalias',
  'selva.subscriptions.addmissing',
  'selva.subscriptions.addtrigger',
  'selva.subscriptions.debug',
  'selva.subscriptions.del',
  'selva.subscriptions.list',
  'selva.subscriptions.listmissing',
  'selva.subscriptions.refresh',
];

redis.RedisClient.prototype.on_info_cmd = function (err, res) {
  if (err) {
    if (err.message.includes('BUSY')) {
      this.on_ready()
      return
    }

    if (err.message === "ERR unknown command 'info'") {
      this.on_ready()
      return
    }

    if (
      err.message.includes('connection is already closed') ||
      err.message.includes(
        'ERR only (P)SUBSCRIBE / (P)UNSUBSCRIBE / PING / QUIT allowed in this context'
      ) ||
      err.message.includes(
        'Redis connection lost and command aborted. It might have been processed.'
      ) ||
      err.message.includes(
        'Stream connection ended and command aborted. It might have been processed.'
      ) ||
      err.message.includes(
        "PUBLISH can't be processed. The connection is already closed."
      )
    ) {
      err.message = 'Ready check failed: ' + err.message
      this.emit('hard-disconnect', err)
      return
    }

    err.message = 'Ready check failed: ' + err.message
    console.log(err)
    setTimeout(
      function (self) {
        self.ready_check()
      },
      1e3,
      this
    )

    return
  }

  if (!res) {
    this.on_ready()
    return
  }

  if (!this.server_info.loading || this.server_info.loading === '0') {
    // If the master_link_status exists but the link is not up, try again after 50 ms
    if (
      this.server_info.master_link_status &&
      this.server_info.master_link_status !== 'up'
    ) {
      this.server_info.loading_eta_seconds = 0.05
    } else {
      // Eta loading should change
      this.on_ready()
      return
    }
  }

  var retry_time = +this.server_info.loading_eta_seconds * 1000
  if (retry_time > 1000) {
    retry_time = 1000
  }

  setTimeout(
    function (self) {
      self.ready_check()
    },
    retry_time,
    this
  )
}

redisSearchCommands.forEach((cmd) => {
  // type definition is wrong its not on the client
  // @ts-ignore
  redis.add_command(`FT.${cmd}`)
})
selva_commands.forEach((cmd) =>
  // @ts-ignore
  redis.add_command(cmd)
)
