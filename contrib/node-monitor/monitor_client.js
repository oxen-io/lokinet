const lokinet = require('./lokinet') // currently using the 0.5 api
const fs = require('fs')
const ping = require("net-ping")
// horrible library that shells out to use ping...
//const ping    = require('ping')

// score successful actions
// while we monitor snode and loki addresses

var session = ping.createSession()

// probably should nuke profiles.dat each round to level the playing field
if (fs.existsSync('profiles.dat')) {
  console.log('cleaning router profiles')
  fs.unlinkSync('profiles.dat')
}

//
// start config
//

// FIXME: take in a binary_path as an option...
var lokinet_config = {
  binary_path: '../../lokinet',
  // clients will default to i2p.rocks
  //  bootstrap_url   : 'http://206.81.100.174/n-st-1.signed',
  //  rpc_ip          : '127.0.0.1',
  //  rpc_port        : 28082,
  //  auto_config_test_host: 'www.google.com',
  //  auto_config_test_port: 80,
  //  testnet         : true,
  //  verbose         : true,
}

score_time = null
score_total = 0

var lokinet_version = 'unknown'

var known_snodes = []
var snodes_stats = {}

function addSnode(snode) {
  var idx = known_snodes.indexOf(snode)
  if (idx == -1) {
    known_snodes.push(snode)

    function lookupSnode(snode) {
      lokinet.lookup(snode, function (records) {
        console.log(snode, 'mapped to', records)
        // if no response or not found, retry
        if (records === undefined || records === null) {
          // failure retry
          setTimeout(function () {
            console.log('retry lookup for', snode)
            lookupSnode(snode)
          }, 5000)
          return
        }
        if (!records) {
          console.log('records is false', records)
          return
        }
        if (!records.length) {
          console.log('record is empty array')
          return
        }
        var ip = records[0]
        // reset stats
        snodes_stats[snode] = {
          onlines: 0,
          offlines: 0,
        }
        // trigger monitor update?
        setInterval(function () {
          /*
          ping.sys.probe(ip, function(isAlive){
            if (isAlive) {
              //console.log(snode, ip, 'is online')
              snodes_stats[snode].onlines++
              score_total++
            } else {
              console.log(snode, ip, 'is offline')
              snodes_stats[snode].offlines++
            }
          })
          */
          session.pingHost(ip, function (err, target) {
            if (err) {
              console.warn(target, 'ping err', err);
              console.log(snode, ip, 'is offline')
              snodes_stats[snode].offlines++
            } else {
              //console.log (target + ": Alive")
              //console.log(snode, ip, 'is online')
              snodes_stats[snode].onlines++
              score_total++
            }
          })
        }, 1000)
      })
    }
    lookupSnode(snode)
  }
}

setInterval(function () {
  // can be known but not mapped yet...
  console.log('snode score card,', known_snodes.length, 'known routers')
  for (var snode in snodes_stats) {
    var stats = snodes_stats[snode]
    var total = stats.onlines + stats.offlines
    var onlinePer = (stats.onlines / total) * 100
    console.log(snode, stats, onlinePer + '%')
  }
}, 30 * 1000)

lokinet.onMessage = function (data) {
  console.log(`monitor: ${data}`)
  var lines = data.split(/\n/)
  for (var i in lines) {
    var tline = lines[i].trim()
    // 	lokinet-0.4.0-59e6a4bc (dev build)
    if (tline.match('lokinet-0.')) {
      var parts = tline.split('lokinet-0.')
      lokinet_version = parts[1]
      console.log('VERSION', parts[1])
    }
    if (tline.match('Using config file:')) {
      // get bootstrap info
      var parts = tline.split('Using config file: ')
      console.log('CONFIGFILE', parts[1])
    }
    if (tline.match('Added bootstrap node')) {
      // get bootstrap info
      var parts = tline.split('Added bootstrap node ')
      addSnode(parts[1])
      console.log('BOOTSTRAP', parts[1])
    }
    if (tline.match('Set Up networking for')) {
      // get interface info
      var parts = tline.split('Set Up networking for ')
      // default:9j4uido1ai7ucirbncbqii1395e8ccd6cjomo9cccp3ztx1ukwio.loki
      console.log('INTERFACE', parts[1])
    }
    //
    if (tline.match('session with ')) {
      // potential node
      var parts = tline.split('session with ')
      if (parts.length) {
        var one = parts[1].replace(' established', '')
        var two = one.split('.snode') // snode\u001b[0;0m
        var snode = two[0] + '.snode'
        addSnode(snode)
        console.log('SESSION', snode)
        score_total++
      } else {
        console.log('unknown session line', tline)
      }
    }
    if (tline.match(' routers from exploration')) {
      var parts = tline.split('\tgot ')
      if (parts.length > 1) {
        var routers = parts[1].replace(' routers from exploration', '')
        console.log('ROUTERS', routers)
        if (routers && routers != '0') {
          score_total++
        }
      } else {
        console.log('unknown exploration line', tline)
      }
    }
    // ?
    if (tline.match('Handle DHT message S relayed=0')) {
      score_total++
    }
    // relay DHT packet
    if (tline.match('Handle DHT message R relayed=0')) {
      score_total++
    }
    //
    if (tline.match(' is built, took ')) {
      // path TX=8826efd28bede66c59782af9894eed08 RX=a994770a8c6d61700b8ca65e4a3adea3 on OBContext:default:3y3ch3m6c8xgmutxwe8fger6n963hn3mkn6tk14j67w1zdxj1n1y.loki-icxqqcpd3sfkjbqifn53h7rmusqa1fyxwqyfrrcgkd37xcikwa7y.loki is built, took 2321 ms
      var parts = tline.split('path TX=')
      if (parts.length > 1) {
        var two = parts[1].split(' RX=')
        if (two.length > 1) {
          var tx = two[0]
          var three = two[1].split(' on OBContext:')
          if (three.length > 1) {
            var rx = three[0]
            var four = three[1].split(' is built, took ')
            if (four.length > 1) {
              var context = four[0]
              var ms = four[1]
              console.log('PATH BUILT', tx, rx, context, ms)
            }
          }
        }
      }
      score_total++
    }
    // happen right after the built, took
    //TX=53e833fcbad1395647f198201d4931e5 RX=b4768bc4b91b86b51b513319f443f538 on default:dnhi78pwrn6cd5yz83i83816nqudwym57s5x4bp3j4g5kbeigw5y.loki built latency=291
    if (tline.match(' is built latency ')) {
      // path latency info
    }
    //[WRN] (166) Tue Apr 30 15:26:25 2019 PDT path/pathbuilder.cpp:319	SNode::8ti485iig9q1wd6k9qr6dqmswrqp9hceohcrtwwa7pn6wcxx9wdy.snode failed to select hop 3
    // not a sure but snode extract
    if (tline.match('.snode failed to select hop')) {
      var parts = tline.split('SNode::')
      //console.log('failed to select hop parts', parts)
      if (parts.length > 1) {
        var two = parts[1].split(' failed to select hop ')
        if (two.length) {
          var hops = two[1]
          //console.log('hopSelectionFailure', snode, hops)
        }
      }
    }
    //obtained an exit via nc9y88xsr7cqmpytc6tbo5dbf5c9fa3is5dezmdwxwgfkuhemg3o.snode
    if (tline.match('obtained an exit via ')) {
      var parts = tline.split('obtained an exit via ')
      if (parts.length > 1) {
        var two = parts[1].split('.snode')
        var snode = two[0] + '.snode'
        addSnode(snode)
      }
      score_total++
    }
    //TX=219222f0f390e40ce47745af292001f2 RX=671c259dd9590173019694fc10433b63 on SNode::nc9y88xsr7cqmpytc6tbo5dbf5c9fa3is5dezmdwxwgfkuhemg3o.snode nc9y88xsr7cqmpytc6tbo5dbf5c9fa3is5dezmdwxwgfkuhemg3o.snode Granted exit
    if (tline.match('Granted exit')) {
      var one = tline.split('TX=')
      if (one.length > 1) {
        var two = one[1].split(' RX=')
        if (two.length > 1) {
          var three = two[1].split(' on SNode::')
          if (three.length > 1) {
            var tx = two[0]
            var rx = three[0]
            console.log('EXIT', tx, rx, 'rest', three[1])
          }
        }
      }
      score_total++
    }
    //service/endpoint.cpp:652	default:kzow69uftho8ukm8g4kf88y5zka84azfrmfx5okkmrg7ucdz5d1o.loki IntroSet publish confirmed
    if (tline.match('IntroSet publish confirmed')) {
      score_total++
    }
  }
}
lokinet.onError = function (data) {
  console.log(`monitorerr: ${data}`)
  // start on 2019-04-30T15:40:55.540314745-07:00 X Records
  // buffer until
  // end on \n\n
  var lines = data.toString().split(/\n/)
  for (var i in lines) {
    var tline = lines[i].trim()
    if (tline.match('utp.session.sendq.')) {
      //console.log('sendq')
      var one = tline.split('utp.session.sendq.')
      // parts[1] = 8ti485iig9q1wd6k9qr6dqmswrqp9hceohcrtwwa7pn6wcxx9wdy.snode [
      // count = 282, total = 3, min = 0, max = 1 ]
      if (one.length > 1) {
        var two = one[1].split(' [ ')
        if (two.length > 1) {
          var snode = two[0]
          var three = two[1].split(', ')
          if (three.length > 3) {
            var count = three[0]
            var total = three[1]
            var min = three[2]
            if (min == 'min = 0') {
              //console.log('no bloat')
              // not sure if this is related to our version or not...
              score_total++
            }
            var max = three[3]
            console.log('UTPSENDQ', snode, count, total, min, max)
          }
        }
      }
    }
  }
}

//
// end config
//

function checkIP(cb) {
  lokinet.getLokiNetIP(function (ip) {
    if (ip === undefined) {
      checkIP(cb)
      return
    }
    //console.log('lokinet interface ip', ip)
    cb(ip)
  })
}

lokinet.startClient(lokinet_config, function () {
  // lokinet isn't necessarily running at this point
  console.log('Starting monitor')
  score_time = Date.now()
  checkIP(function (ip) {
    console.log('lokinet interface ip', ip)
    lokinet.findLokiNetDNS(function (servers) {
      console.log('monitor detected DNS Servers', servers)
    })
    setInterval(function () {
      console.log('score', score_total, 'time', ((Date.now() - score_time) / 1000))
    }, 30000)
    // maybe run until a specific time and quit for comparison
    // 1200s (two 10min sessions)
    setTimeout(function () {
      console.log(lokinet_config.binary_path, 'version', lokinet_version, 'final score', score_total)
      process.exit()
    }, 3 * 600 * 1000) // 3x 10m sessions worth
  })
})
