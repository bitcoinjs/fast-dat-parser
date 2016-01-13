let argv = require('minimist')(process.argv.slice(2))
let copyFrom = require('pg-copy-streams').from
let pg = require('pg')

pg.connect(process.env.POSTGRES, (err, client, free) => {
  if (err) return console.error(err)

  let target = argv.t || argv.table
  let stream = client.query(copyFrom('COPY ' + target + ' FROM STDIN BINARY'))
  stream.on('end', free)
  stream.on('error', (err) => {
    console.error(err)
  })

  process.stdin.pipe(stream)
})
