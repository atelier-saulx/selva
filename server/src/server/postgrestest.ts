import PostgresManager from './postgresManager'
import { Client } from 'pg'
import { PG } from './pg'

const password = `baratta`

const master = new PostgresManager({ port: 5444, password, name: `main` })
const node1 = new PostgresManager({ port: 5445, password, name: `node1` })
const node2 = new PostgresManager({ port: 5446, password, name: `node2` })

const nodes = [master, node1, node2]

function sleep(ms: number) {
  return new Promise((resolve) => setTimeout(resolve, ms))
}

async function ensureStarted(node: PostgresManager) {
  const port = node.getPort()
  let ctr = 0
  while (ctr < 1000) {
    ++ctr
    try {
      const client = new Client({
        connectionString: `postgres://postgres:${password}@127.0.0.1:${port}`,
      })
      await client.connect()
      await client.query(`select 1`, [])
      await client.end()
      break
    } catch (e) {
      // nop
    }
    console.log(`sleeping`)
    await sleep(1000)
  }
}

async function main() {
  await Promise.all(nodes.map((node) => node.start()))
  await Promise.all(nodes.map((node) => ensureStarted(node)))

  const masterClient = new PG({
    connectionString: `postgres://postgres:${password}@127.0.0.1:${master.getPort()}`,
  })
  const node1Client = new PG({
    connectionString: `postgres://postgres:${password}@127.0.0.1:${node1.getPort()}`,
  })
  const node2Client = new PG({
    connectionString: `postgres://postgres:${password}@127.0.0.1:${node2.getPort()}`,
  })

  await masterClient.execute(`CREATE EXTENSION postgres_fdw`, [])
  await node1Client.execute(`CREATE EXTENSION postgres_fdw`, [])
  await node2Client.execute(`CREATE EXTENSION postgres_fdw`, [])

  function createServerSQL(nodeName: string, nodePort: number): string {
    return `CREATE SERVER ${nodeName} FOREIGN DATA WRAPPER postgres_fdw OPTIONS (dbname 'postgres', host '192.168.57.12', port ${
      `'` + nodePort + `'`
    });`
  }

  console.log(`Create SERVER`)
  await masterClient.execute(createServerSQL(`node1`, node1.getPort()), [])
  await masterClient.execute(createServerSQL(`node2`, node2.getPort()), [])

  function createUserMappingSQL(nodeName: string, password: string) {
    return `CREATE USER MAPPING for postgres SERVER ${nodeName} OPTIONS (user 'postgres', password ${
      `'` + password + `'`
    });`
  }
  console.log(`Create USER`)

  await masterClient.execute(createUserMappingSQL(`node1`, password), [])
  await masterClient.execute(createUserMappingSQL(`node2`, password), [])

  //const table = `CREATE TABLE analytics_data (created: TIMESTAMP, properties: jsonb)`
  const table = `
     (
       time TIMESTAMP WITHOUT TIME ZONE NOT NULL,
       timezone_shift int NULL,
       city_name text NULL,
       temp_c double PRECISION NULL,
       feels_like_c double PRECISION NULL,
       temp_min_c double PRECISION NULL,
       temp_max_c double PRECISION NULL,
       pressure_hpa double PRECISION NULL,
       humidity_percent double PRECISION NULL,
       wind_speed_ms double PRECISION NULL,
       wind_deg int NULL,
       rain_1h_mm double PRECISION NULL,
       rain_3h_mm double PRECISION NULL,
       snow_1h_mm double PRECISION NULL,
       snow_3h_mm double PRECISION NULL,
       clouds_percent int NULL,
       weather_type_id int NULL
     )
  `

  console.log(`Create Table`)
  await masterClient.execute(
    `CREATE TABLE IF NOT EXISTS weather_metrics ${table} PARTITION BY LIST(city_name) `,
    []
  )
  await node1Client.execute(
    `CREATE TABLE IF NOT EXISTS weather_metrics ${table}`,
    []
  )
  await node2Client.execute(
    `CREATE TABLE IF NOT EXISTS weather_metrics ${table}`,
    []
  )

  await masterClient.execute(
    `create foreign table weather_non_usa  partition of weather_metrics  for values in ('Vienna', 'Pietermaritzburg', 'Lisbon','Nairobi','Stockholm','Toronto') SERVER node1 OPTIONS (schema_name 'public', table_name 'weather_metrics', batch_size '10000');`,
    []
  )
  await masterClient.execute(
    `create foreign table weather_usa  partition of weather_metrics  for values in ('Austin', 'New York', 'San Francisco', 'Princeton') SERVER node2 OPTIONS (schema_name 'public', table_name 'weather_metrics', batch_size '10000');`,
    []
  )

  ;('Vienna')
  ;('Pietermaritzburg')
  ;('Lisbon')
  ;('Nairobi')
  ;('Stockholm')
  ;('Toronto')

  ;('Austin')
  ;('New York')
  ;('San Francisco')
  ;('Princeton')
  console.log(table)
  //await masterClient.execute(`
  //    CREATE FOREIGN TABLE weather1
  //    ${table}
  //    SERVER node1
  //    OPTIONS (schema_name 'public', table_name 'weather_metrics');
  //`, [])
  //await masterClient.execute(`
  //    CREATE FOREIGN TABLE weather2
  //    ${table}
  //    SERVER node2
  //    OPTIONS (schema_name 'public', table_name 'weather_metrics');
  //`, [])
}

process.on('SIGINT', function () {
  console.log(`exitting`)
  master.destroy()
  //timescale1.destroy()
  process.exit(1)
})

main().then(console.log, console.error)
