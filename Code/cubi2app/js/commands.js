import { vector, add, scale, length, unit, rotateBack, noise, angleDegrees, vectorText, degreesPerRadian } from './math.js'
import state from './state.js'
import { say, centimeters } from './functions.js'
import { sphereSim, control, measure, navelZero, mass, wheelInertia, wheelMax } from './sphere-sim.js'
import { settling } from './sphere-control.js'
import { gravityText, gravityFromWeight } from './gravity-measure.js'
import { cubeSim, balance } from './cube-sim.js'
import { view } from './view3d.js'
import settings from './settings.js'

const radiansToRpm = 9.549
const testWeightArm = settings.simulation.testWeightArm   // where a test weight is taped, in mm
const sidewaysLeast = 60         // a navel counts as 'at the side' between these, in degrees
const sidewaysMost = 120

// the way from the center of the sphere out to navel j
const navelDirection = j => unit(vector(j & 1 ? 1 : -1, j & 2 ? 1 : -1, j & 4 ? 1 : -1))

// whatever the cube has had to say since we last looked
const showWhatTheCubeSaid = () => {
  cubeSim.said.forEach(line => say(line.text, line.strong))
  cubeSim.said = []
}

// ----- the sphere ------------------------------------------------------------------------------

const goSphere = () => {
  sphereSim.rest()
  sphereSim.wheelSpeeds = vector(0, 0, 0)
  control.begin(rotateBack(sphereSim.orientation, vector(0, 0, 1)))

  state.running = true
  state.fault = ''
  view.trail.length = 0

  say('SPHERE - holding. \'front\' is where wheel X\'s side points now.', true)
}

// the center of gravity of the simulated sphere, so many mm towards navel 0
const setSphereGravity = mm => {
  sphereSim.gravityOffset = scale(navelZero, mm / 1000)
  say('center of gravity ' + (+mm).toFixed(1) + ' mm towards navel 0 - \'go sphere\' rests it')
  goSphere()
}

// 'where' in the simulation, as the firmware does it: resting, wheels off
const where = line => {
  if (state.running) return say('stop first')
  if (length(sphereSim.worldRate) > 0.01) {
    return say('it still moves (' + (length(sphereSim.worldRate) * 57.3).toFixed(1) +
      ' deg/s) - let it settle, then \'where\' again')
  }

  const down = rotateBack(sphereSim.orientation, vector(0, 0, -1))
  const given = line.split(/\s+/).slice(1).map(Number)

  if (!given.length) return whereItHangs(down)
  return whereWithAWeight(down, given)
}

// 'where' with nothing: it only tells us how it hangs, and where to tape the test weight
const whereItHangs = down => {
  state.simulationWhere = down

  say('up ' + vectorText(scale(down, -1)) + ' (cube frame)')
  say('the center of gravity points ' + [down.x, down.y, down.z].map(one => (one >= 0 ? '+' : '') + one.toFixed(3)).join(' ') +
    ' from the sphere\'s center; navel 0 is ' + angleDegrees(down, navelZero).toFixed(1) + ' deg from straight down')

  // a weight helps most on a navel that sticks out sideways, not on the one underneath
  const sideways = [0, 1, 2, 3, 4, 5, 6, 7].filter(j => {
    const away = angleDegrees(navelDirection(j), down)
    return away > sidewaysLeast && away < sidewaysMost
  })

  return say('how far: tape ~10 g on the outside at a navel at the SIDE - ' + sideways.join(' ') +
    ' - let it settle, \'where 10 N\'')
}

// 'where G N [MM]': with a known weight taped on, how far the center of gravity really is
const whereWithAWeight = (down, given) => {
  const [grams, navel, arm = testWeightArm] = given
  const sane = grams >= 1 && grams <= 500 && navel >= 0 && navel <= 7 && arm >= 50 && arm <= 200
  if (!sane) return say('where G N [MM]: G 1...500 g, N 0...7, MM 50...200')
  if (!state.simulationWhere) return say('first \'where\' WITHOUT the weight, then tape it on and \'where G N\'')

  // with the weight on, the sphere comes to rest with mass x offset + weight x arm straight down
  const at = scale(navelDirection(navel), arm / 1000)
  const nowHangs = unit(add(scale(sphereSim.gravityOffset, mass), scale(at, grams / 1000)))
  const offset = gravityFromWeight(state.simulationWhere, nowHangs, mass, grams / 1000, at)
  const moved = angleDegrees(state.simulationWhere, nowHangs)

  if (!offset) {
    return say('the weight moved the rest only ' + moved.toFixed(2) +
      ' deg (or the wrong way) - a heavier weight, or a navel more to the side')
  }

  say(grams + ' g at navel ' + navel + ' moved the rest ' + moved.toFixed(1) + ' deg')
  say(gravityText(offset, 0))
  return say('(take the test weight off)')
}

// ----- understanding a line -----------------------------------------------------------------------

const command = line => {
  const said = line.trim().toLowerCase()
  if (!said) return

  say('> ' + said)
  const [word, rest] = said.split(/\s+/)
  const number = parseFloat(rest)

  if (said === 'go sphere') return goSphere()

  const standing = said.match(/^go (corner|edge [xyz])$/)
  if (standing) {
    cubeSim.stand(standing[1])
    return showWhatTheCubeSaid()
  }

  if (state.kind === 'cube') return cubeCommand(said, word, rest, number)
  return sphereCommand(said, word, rest, number)
}

const sphereCommand = (said, word, rest, number) => {
  if (word === 'help') {
    return say('go sphere | fwd CM | back CM | left DEG | right DEG | navel 0-7 | where | where G N | ' +
      'cg (measure) | cg X Y Z (simulation) | halt | front | speed CM/S | turnrate DEG/S | cg MM | stop | ' +
      'go corner | go edge x|y|z')
  }

  if (word === 'stop') {
    state.running = false
    const speeds = [sphereSim.wheelSpeeds.x, sphereSim.wheelSpeeds.y, sphereSim.wheelSpeeds.z]
    return say('off - wheels ' + speeds.map(one => (one * radiansToRpm).toFixed(0)).join(' ') + ' rpm')
  }

  if (word === 'speed') {
    if (!(number >= 1 && number <= 40)) return say('speed 1 ... 40 cm/s')
    control.speed = number / 100
    return say('sphere speed ' + number + ' cm/s')
  }

  if (word === 'turnrate') {
    if (!(number >= 5 && number <= 120)) return say('turnrate 5 ... 120 deg/s')
    control.turnRate = number / degreesPerRadian
    return say('sphere turning ' + number + ' deg/s')
  }

  if (word === 'where') return where(said)
  if (word === 'cg') return sphereGravityCommand(said, rest, number)

  if (!state.running) return say('first \'go sphere\'')
  return movingCommand(word, rest, number)
}

const sphereGravityCommand = (said, rest, number) => {
  // 'cg' on its own measures it on the eight navels, as the real cube does
  if (rest === undefined) {
    if (!state.running) return say('first \'go sphere\'')
    measure.start()
    return say('measuring the center of gravity on the 8 navels (~1 min; \'halt\' stops it) ...')
  }

  // 'cg X Y Z': put it exactly there, in mm, in the cube's own frame
  const given = said.split(/\s+/).slice(1).map(Number)
  if (given.length === 3 && !given.some(Number.isNaN)) {
    sphereSim.gravityOffset = scale(vector(given[0], given[1], given[2]), 1 / 1000)
    say('center of gravity at ' + given.join(' ') + ' mm (cube frame)')
    return goSphere()
  }

  if (!(number >= 0 && number <= 6)) return say('cg 0 ... 6 mm')
  return setSphereGravity(number)
}

const movingCommand = (word, rest, number) => {
  if (word === 'halt' && measure.active) {
    measure.active = false
    measure.firstWait = false
    control.halt()
    return say('cg measurement stopped')
  }

  if (word === 'halt') {
    control.halt()
    return say('halt - at ' + centimeters(control.placeX) + ' ' + centimeters(control.placeY) + ' cm')
  }

  if (word === 'corner' && /^[0-7]$/.test(rest ?? '')) return say('in the sphere it is \'navel N\'')

  if (word === 'navel' && /^[0-7]$/.test(rest ?? '')) {
    control.restOn(+rest)
    const held = rest === '0' ? '' : ' (the wheels hold it there unless the center of gravity is at the center)'
    return say('rolling onto navel ' + rest + held)
  }

  if (word === 'front') {
    control.front()
    return say('front = the heading now')
  }

  if (word === 'fwd' || word === 'back') {
    if (!(number > 0 && number <= 500)) return say('fwd / back 1 ... 500 cm')
    control.move((word === 'fwd' ? 1 : -1) * number / 100)
    return say((word === 'fwd' ? 'forward ' : 'back ') + number + ' cm at ' + (control.speed * 100).toFixed(0) + ' cm/s')
  }

  if (word === 'left' || word === 'right') {
    if (!(number > 0 && number <= 720)) return say('left / right 1 ... 720 deg')
    control.turn((word === 'left' ? 1 : -1) * number / degreesPerRadian)
    return say('turning ' + word + ' ' + number + ' deg at ' + (control.turnRate * degreesPerRadian).toFixed(0) + ' deg/s')
  }

  say('unknown - \'help\'')
}

// ----- the bare cube ---------------------------------------------------------------------------

const cubeCommand = (said, word, rest, number) => {
  if (word === 'help') {
    return say('go corner | go edge x|y|z (on the corner: lowers onto it) | lift (the hand: edge -> corner) | ' +
      'stop | push | zero | fix | save | show | spin S | twist T | learn L | k A R W | cg MM | go sphere')
  }

  if (word === 'stop') {
    if (cubeSim.state !== 'on') return say('already off')
    cubeSim.off('')
    return showWhatTheCubeSaid()
  }

  if (word === 'push' || word === 'lift') {
    if (word === 'push') cubeSim.push()
    else cubeSim.lift()
    return showWhatTheCubeSaid()
  }

  if (word === 'spin' || word === 'twist' || word === 'learn') return cubeSettingCommand(word, number)
  if (word === 'k') return cubeGainsCommand(said)

  if (word === 'cg') {
    if (!(number >= 0 && number <= 6)) return say('cg 0 ... 6 mm')
    cubeSim.setGravityOffset(number)
    return showWhatTheCubeSaid()
  }

  if (word === 'zero' || word === 'fix') return cubePointCommand(word)

  if (word === 'save') {
    cubeSim.savedPoints[cubeSim.mode] = { ...balance.balancePoint }
    return say('saved (in the simulation: until the app closes)')
  }

  if (word === 'show') return cubeShow()

  if (word === 'corner' || word === 'navel') return say('on the bare cube: \'go corner\' (navels are for the sphere)')
  if (['fwd', 'back', 'left', 'right', 'halt', 'front', 'speed', 'turnrate'].includes(word)) {
    return say('that is for the sphere - \'go sphere\'')
  }

  say('unknown - \'help\'')
}

const cubeSettingCommand = (word, number) => {
  if (!(number >= 0 && number <= 1)) {
    const extra = word === 'twist' ? ' (part of the rate gain, corner only)' : ''
    return say(word + ' 0 ... 1' + extra)
  }

  if (word === 'spin') balance.spin = number
  if (word === 'twist') balance.twist = number
  if (word === 'learn') balance.learnRate = number

  say(word + ' ' + number.toFixed(2))
}

const cubeGainsCommand = said => {
  const given = said.split(/\s+/).slice(1).map(Number)
  if (given.length !== 3 || given.some(Number.isNaN)) return say('k A R W')

  balance.angleGain = given[0]
  balance.rateGain = given[1]
  balance.wheelGain = given[2]
  cubeSim.gainsFor = cubeSim.mode

  say('gains ' + cubeSim.mode + ': angle ' + given[0].toFixed(3) +
    '  rate ' + given[1].toFixed(4) + '  wheel ' + given[2].toFixed(6))
}

// 'zero': held still exactly at balance, this 'up' becomes the point.
// 'fix': the same, out of how the wheels drifted in the run just finished.
const cubePointCommand = word => {
  if (word === 'zero' && cubeSim.state === 'on') return say('hold the cube, \'stop\', then \'zero\'')
  if (word === 'fix' && cubeSim.runTime < 3) return say('fix needs a run of a few seconds first')

  const was = balance.balancePoint
  const now = cubeSim.trueBalancePoint()

  balance.balancePoint = now
  cubeSim.savedPoints[cubeSim.mode] = { ...now }

  const moved = ' (moved ' + angleDegrees(was, now).toFixed(2) + ' deg) - \'save\' to keep it'
  if (word === 'zero') return say('balance point of ' + cubeSim.mode + ' set' + moved)
  say('balance point of ' + cubeSim.mode + ': ' + vectorText(now) + moved)
}

const cubeShow = () => {
  say('state ' + cubeSim.state + ' | mode ' + cubeSim.mode +
    ' | tilt ' + balance.tiltDegrees.toFixed(2) + ' deg' +
    ' | twist ' + balance.twist.toFixed(2) +
    ' | spin ' + balance.spin.toFixed(2) +
    ' | learn ' + balance.learnRate.toFixed(2) + ' /s')

  ;['corner', 'edge x', 'edge y', 'edge z'].forEach(mode => {
    const point = mode === cubeSim.mode ? balance.balancePoint : cubeSim.savedPoints[mode]
    say('  ' + mode.padEnd(7) + ' up ' + vectorText(point))
  })

  say('  k ' + balance.angleGain.toFixed(3) + ' ' + balance.rateGain.toFixed(4) + ' ' +
    balance.wheelGain.toFixed(6) + ' | the true point now: ' + vectorText(cubeSim.trueBalancePoint()))
}

// ----- one step of the simulation (4 ms) -----------------------------------------------------------

const tick = dt => {
  if (state.kind === 'cube') {
    cubeSim.tick(dt)
    return showWhatTheCubeSaid()
  }

  if (state.running) return sphereTick(dt)
  sphereCoasts(dt)
}

const sphereTick = dt => {
  const up = rotateBack(sphereSim.orientation, vector(0, 0, 1))
  const acceleration = add(up, noise(0.005))
  const gyro = add(rotateBack(sphereSim.orientation, sphereSim.worldRate), noise(0.3 / 57.3))

  const torque = control.step(acceleration, gyro, sphereSim.wheelSpeeds, dt)
  sphereSim.step(state.lastTorque, dt)
  state.lastTorque = torque

  if (measure.active) {
    const line = measure.step(control, gyro, sphereSim.wheelSpeeds, dt)
    if (line) say(line, !measure.active)
  }

  const fastest = Math.max(Math.abs(sphereSim.wheelSpeeds.x), Math.abs(sphereSim.wheelSpeeds.y), Math.abs(sphereSim.wheelSpeeds.z))
  if (fastest > wheelMax) {
    state.running = false
    state.fault = 'a wheel too fast'
    say('STOPPED: a wheel too fast', true)
  }

  tellWhenItArrives()
}

const tellWhenItArrives = () => {
  const resting = control.doing === settling && control.navelReached
  const place = centimeters(control.placeX) + ' ' + centimeters(control.placeY) + ' cm'

  if (resting && !state.wasReached) say('resting on navel ' + control.navel + ' - at ' + place)
  state.wasReached = resting

  if (state.wasMoving && !control.doing) {
    say('done - at ' + place + ', heading ' + (control.heading * degreesPerRadian).toFixed(0) + ' deg')
  }
  state.wasMoving = control.doing
}

// wheels off: the drivers hold the wheels back, and the sphere rolls on by itself
const sphereCoasts = dt => {
  const brake = speed => -Math.sign(speed) * Math.min(0.05, Math.abs(speed) * wheelInertia / dt)
  const { x, y, z } = sphereSim.wheelSpeeds

  sphereSim.step(vector(brake(x), brake(y), brake(z)), dt)
  sphereSim.worldRate = scale(sphereSim.worldRate, 1 - 0.2 * dt)
  state.lastTorque = vector(0, 0, 0)
}

export { goSphere, setSphereGravity, command, tick }
