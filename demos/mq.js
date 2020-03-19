const fork = require('child_process').fork
const path = require('path')

const opts = {
  fontSize: 128,
  msg: '跑馬燈測試 1 2 3 4 5 6 7 8 9 10 ~'
}

start(opts)

function start (opts) {
  const ps = fork(path.join(__dirname, 'mq_lib.js'))

  ps.send(opts)

  setTimeout(() => {
    ps.kill('SIGINT')
  }, 10000)
}
