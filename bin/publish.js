console.info(process.argv)

const bump = process.argv[2] || 'patch'

const fs = require('fs')
const path = require('path')

const client = path.join(__dirname, '../', 'client', 'package.json')
const server = path.join(__dirname, '../', 'server', 'package.json')
const api = path.join(__dirname, '../', 'api', 'package.json')
const tools = path.join(__dirname, '../', 'tools', 'package.json')

const clientPkg = JSON.parse(fs.readFileSync(client).toString())
const serverPkg = JSON.parse(fs.readFileSync(server).toString())
const apiPkg = JSON.parse(fs.readFileSync(api).toString())
const toolsPkg = JSON.parse(fs.readFileSync(tools).toString())

const pkgs = [clientPkg, serverPkg, apiPkg, toolsPkg]

pkgs.forEach(v => {
  const version = v.version.split('.')
  if (bump === 'major') {
    version[0] = version[0] * 1 + 1
    version[1] = 0
    version[2] = 0
  } else if (bump === 'minor') {
    version[1] = version[1] * 1 + 1
    version[2] = 0
  } else {
    version[2] = version[2] * 1 + 1
  }
  v.version = version.join('.')
})

clientPkg.devDependencies['@saulx/selva-server'] = serverPkg.version
serverPkg.dependencies['@saulx/selva'] = clientPkg.version
apiPkg.dependencies['@saulx/selva'] = clientPkg.version
apiPkg.devDependencies['@saulx/selva-server'] = serverPkg.version
toolsPkg.dependencies['@saulx/selva'] = clientPkg.version
toolsPkg.devDependencies['@saulx/selva-server'] = serverPkg.version

// dependencies: @saulx/selva
fs.writeFileSync(client, JSON.stringify(clientPkg, void 0, 2))
fs.writeFileSync(server, JSON.stringify(serverPkg, void 0, 2))
fs.writeFileSync(api, JSON.stringify(apiPkg, void 0, 2))
fs.writeFileSync(tools, JSON.stringify(toolsPkg, void 0, 2))
console.log('update version --', bump)
