const parseError = (obj: { [key: string]: any }) => {
  // make this cleaner
  const err = obj.payload && obj.payload.___$error___
  if (typeof err === 'string') {
    return new Error(err)
  } else if (err.message) {
    return new Error(err.message)
  } else if (err.command) {
    const { command, args, code } = err
    if (command === 'EVALSHA') {
      return new Error(`Lua error ${args.slice(3).join(', ')}`)
    }
    return new Error(`${command} ${args.join(', ')} ${code}`)
  }
  return new Error('Unkown error')
}

export default parseError
