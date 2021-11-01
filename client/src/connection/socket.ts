import net from 'net'

export const connectSocket = (opts: net.NetConnectOpts): net.Socket => {
  //   net.NetConnectOpts

  //  port: number;
  // host?: string;

  console.info('create socket', opts)

  const socket = net.connect(opts)

  return socket
}
