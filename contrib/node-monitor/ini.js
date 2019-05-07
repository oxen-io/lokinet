function iniToJSON(data) {
  const lines = data.split(/\n/)
  var section = 'unknown'
  var config = {}
  for(var i in lines) {
    var line = lines[i].trim()
    if (line.match(/#/)) {
      var parts = line.split(/#/)
      line = parts[0].trim()
    }
    // done reducing
    if (!line) continue

    // check for section
    if (line[0] == '[' && line[line.length - 1] == ']') {
      section = line.substring(1, line.length -1)
      //console.log('found section', section)
      continue
    }
    // key value pair
    if (line.match(/=/)) {
      var parts = line.split(/=/)
      var key = parts.shift().trim()
      var value = parts.join('=').trim()
      if (value === 'true')  value = true
      if (value === 'false') value = false
      //console.log('key/pair ['+section+']', key, '=', value)
      if (config[section] === undefined) config[section] = {}
      config[section][key]=value
      continue
    }
    console.error('config ['+section+'] not section or key/value pair', line)
  }
  return config
}

function jsonToINI(json) {
  var lastSection = 'unknown'
  var config = ''
  for(var section in json) {
    config += "\n" + '[' + section + ']' + "\n"
    var keys = json[section]
    for(var key in keys) {
      //console.log('key', key, 'value', keys[key])
      // if keys[key] is an array, then we need to send the same key each time
      if (keys[key] !== undefined && keys[key].constructor.name == 'Array') {
        for(var i in keys[key]) {
          var v = keys[key][i]
          config += key + '=' + v + "\n"
        }
      } else {
        config += key + '=' + keys[key] + "\n"
      }
    }
  }
  return config
}

module.exports = {
  iniToJSON: iniToJSON,
  jsonToINI: jsonToINI,
}
