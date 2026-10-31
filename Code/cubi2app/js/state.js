import settings from './settings.js'

const fields = {
  // sphere or cube
  kind: 'sphere',              // 'sphere' or 'cube'
  fault: '',                   // what went wrong, in one line - empty when all is well

  // the simulation
  running: true,               // the simulated sphere's wheels are on
  lastTorque: { x: 0, y: 0, z: 0 },  // the wheels' torque of the last step - the motors answer one step later
  wasMoving: 0,
  wasReached: false,
  simulationWhere: null,       // where the sphere stands: straight down, before the test weight

  // the real Cubi over the Wi-Fi
  liveOn: false,               // true: the commands go to the real Cubi
  liveAddress: settings.link.address,
  liveSequence: 0,             // the number of the last reading we asked for
  liveTelemetry: null,         // the cube's last telemetry
  liveAge: 1e9,                // how old it is, in ms - 1e9 means we have heard nothing
  liveOk: false,
  livePolling: false,
  updating: false              // a program is being sent: the app stops asking for data
}

// A guard, so a misspelled name is seen at once.
// Without it, state.liveAddres = x quietly makes a new field and the app keeps running wrong.
// Proxy stands in front of the object and is asked first on every read and every write.
const state = new Proxy(fields, {
  get: (target, name) => {
    if (name in target) return target[name]
    throw new Error(`state has no field named '${String(name)}'`)
  },
  set: (target, name, value) => {
    if (!(name in target)) throw new Error(`state has no field named '${String(name)}' - a new field is added in state.js`)
    target[name] = value
    return true
  }
})

export default state
