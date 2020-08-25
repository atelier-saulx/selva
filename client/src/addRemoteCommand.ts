const addCommand = () => {
  // dont handle this kind of stuff here
  if (!this.registry.connection) {
    console.log('no connection yet push to queue')
    this.queue.push({ command, selector })
  } else {
    if (selector.type === 'registry' || selector.name === 'registry') {
      // this is nessecary scince you need to start somewhere

      // if this.registry connection....

      //   selvaClientId: string

      // this was the get from command one
      // this is fine with get server descriptor better even
      // ADD COMMAND TO CONNECTION
      addCommandToQueue(this.registry.connection, command)
    } else {
      getServerDescriptor(this.registry, selector).then(descriptor => {
        addCommandToQueue(getConnection(descriptor, this.registry), command)
      })
    }
  }
}
