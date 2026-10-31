import {
  vector, add, subtract, scale, dot, cross, length, unit, component, axisVector, noTurn,
  rotate, rotateBack, rotateInBody, solve, noise, angleDegrees, blend, tilted, degreesPerRadian
} from './math.js'
import Balance from './balance.js'
import state from './state.js'
import settings from './settings.js'

// the numbers all live in settings.js
const {
  mass, gravity, inertia: cubeInertia, wheelInertia, twistFriction, airFriction,
  cornerTip, edgeTip, wheelMax, fallDegrees, takeOverDegrees, takeOverStill
} = settings.cube

// the corner gains were saved from the 18 minute run, the edge ones come from the model
const cornerGains = settings.cubeGains.corner
const edgeGains = settings.cubeGains.edge

// lowering onto an edge - the firmware's LOWER_RATE, LOWER_LAND, LOWER_MS
const lowerRate = settings.lowering.rateDegrees / degreesPerRadian
const lowerLand = settings.lowering.landDegrees / degreesPerRadian
const lowerSeconds = settings.lowering.seconds
const lowerTiltStop = settings.lowering.tiltStopDegrees

// the eight corner tips of the cube, in mm
const tips = []
for (let k = 0; k < 8; k++) tips.push(vector(k & 1 ? 71.4 : -71.4, k & 2 ? 71.4 : -71.4, k & 4 ? 71.4 : -71.4))

const edgeAxisOf = mode => (mode === 'corner' ? -1 : 'xyz'.indexOf(mode.slice(-1)))

// the way inside the cube that points up when it stands on this corner or lies on this edge
const standDirection = mode => {
  const axis = edgeAxisOf(mode)
  const way = vector(1, 1, 1)

  if (axis >= 0) way[['x', 'y', 'z'][axis]] = 0
  return unit(way)
}

const balance = new Balance()

class CubeSim {
  constructor() {
    this.mode = 'corner'            // 'corner', or 'edge x' / 'edge y' / 'edge z'
    this.physicsMode = 'corner'     // what it really stands on, while the controller is already elsewhere
    this.edgeAxis = -1
    this.state = 'held'             // 'held', 'on', 'falling', 'fallen'

    this.orientation = noTurn()
    this.bodyRate = vector(0, 0, 0)       // how fast the cube itself turns
    this.wheelSpeeds = vector(0, 0, 0)
    this.wheelAngles = [0, 0, 0]
    this.inertia = noTurn()

    this.gravityOffset = vector(0, 0, 0)  // how far its center of gravity sits off the middle
    this.pivot = vector(0, 0, 0)          // the tip it turns on, in its own frame
    this.fromPivot = vector(0, 0, 0)      // from that tip to the center of gravity
    this.pivotPlace = vector(0, 0, 0)     // where that tip is in the room, in mm

    this.runTime = 0
    this.liftTime = 0
    this.fault = ''
    this.lowering = null            // while it is being lowered onto an edge
    this.landingOn = null           // while it is falling the last part onto that edge
    this.hand = null                // while the hand lifts it back onto the corner
    this.gainsFor = ''

    // where each way of standing thinks its balance point is - learned, and kept
    this.savedPoints = {
      corner: standDirection('corner'),
      'edge x': standDirection('edge x'),
      'edge y': standDirection('edge y'),
      'edge z': standDirection('edge z')
    }

    this.said = []   // what has happened, for whoever runs this to show
  }

  // ----- what the outside asks of it -------------------------------------------------------

  // Stand it up on a corner, or on an edge.
  stand = mode => {
    if (this.state === 'on') return this.#alreadyStanding(mode)

    this.mode = mode
    this.#useGeometry(mode)
    balance.edgeAxis = this.edgeAxis
    if (this.gainsFor !== mode) this.#useGains(mode)
    balance.balancePoint = { ...this.savedPoints[mode] }

    this.wheelSpeeds = vector(0, 0, 0)
    this.wheelAngles = [0, 0, 0]

    // the hand holds it within the take-over angle of its balance point
    const around = this.edgeAxis >= 0
      ? axisVector(this.edgeAxis)
      : unit(cross(balance.balancePoint, vector(1, -1, 0)))
    this.#pose(tilted(balance.balancePoint, around, 2.5))

    this.pivotPlace = vector(0, 0, 0)
    this.lowering = null
    this.landingOn = null
    this.hand = null
    this.state = 'on'
    this.runTime = 0
    this.fault = ''
    state.lastTorque = vector(0, 0, 0)

    this.#say('BALANCING - ' + mode, true)
  }

  // Waiting: held on its corner at its balance point, the wheels off.
  held = () => {
    this.#useGeometry(this.mode)
    balance.edgeAxis = this.edgeAxis
    balance.balancePoint = { ...this.savedPoints[this.mode] }

    this.#pose(this.trueBalancePoint())
    this.wheelSpeeds = vector(0, 0, 0)
    this.pivotPlace = vector(0, 0, 0)
    this.lowering = null
    this.landingOn = null
    this.state = 'held'
    this.fault = ''
  }

  // The wheels stop and it falls.
  off = why => {
    this.hand = null

    if (this.lowering) {
      // the firmware's keepLearned: what it had learned on the corner is kept
      balance.balancePoint = this.lowering.from
      this.savedPoints.corner = { ...this.lowering.from }
      this.lowering = null
    }

    this.state = 'falling'
    this.fault = why
    state.lastTorque = vector(0, 0, 0)

    const speeds = [this.wheelSpeeds.x, this.wheelSpeeds.y, this.wheelSpeeds.z]
    this.#say(why || 'off - wheels ' + speeds.map(one => (one * 9.549).toFixed(0)).join(' ') + ' rpm', !!why)
  }

  // The hand of the simulation: from an edge back onto corner 0.
  lift = () => {
    if (this.state !== 'on' || this.mode === 'corner' || this.lowering) {
      return this.#say('lift: first balance on an edge (on the corner: Edge X / Y / Z)')
    }
    if (this.hand) return

    this.landingOn = null
    const corner = this.#tipInTheRoom()
    this.#useGeometry('corner')
    this.pivotPlace = add(corner, rotate(this.orientation, scale(this.pivot, 1000)))

    this.hand = { time: 0 }
    this.#say('the hand lifts it onto corner 0 ...')
  }

  // A nudge, the way a finger would give it.
  push = () => {
    if (this.state !== 'on') return this.#say('push: first Corner or an Edge')

    const kick = this.edgeAxis >= 0
      ? scale(axisVector(this.edgeAxis), (Math.random() < 0.5 ? -1 : 1) * 0.35)
      : scale(unit(cross(rotateBack(this.orientation, vector(0, 0, 1)), unit(noise(1)))), 0.3)

    this.bodyRate = add(this.bodyRate, kick)
    this.#say('push - ' + (length(kick) * 57.3).toFixed(0) + ' deg/s')
  }

  // Move its center of gravity off the middle, across the corner diagonal.
  setGravityOffset = mm => {
    this.gravityOffset = scale(unit(vector(1, -1, 0)), mm / 1000)
    this.#say('center of gravity ' + (+mm).toFixed(1) +
      ' mm off the center, across the corner diagonal (towards +x -y)')

    this.#useGeometry(this.physicsMode || this.mode)
    if (this.state !== 'on') this.held()
  }

  // the way that really points up, out of where the tip and the center of gravity are
  trueBalancePoint = () => {
    let up = unit(this.fromPivot)
    if (this.edgeAxis < 0) return up

    up = subtract(up, scale(axisVector(this.edgeAxis), component(up, this.edgeAxis)))
    return unit(up)
  }

  // ----- one step of time -------------------------------------------------------------------

  tick = dt => {
    if (this.state === 'on') return this.#tickStanding(dt)
    if (this.state === 'falling') return this.#tickFalling(dt)
    if (this.state === 'fallen') return this.#tickFallen(dt)
  }

  #tickStanding = dt => {
    this.runTime += dt
    this.#walkTheLowering(dt)
    if (this.hand) this.#handLifts(dt)

    const up = rotateBack(this.orientation, vector(0, 0, 1))
    const gyro = add(this.bodyRate, noise(0.3 / 57.3))
    const learning = !this.lowering && this.runTime > 2
    const torque = balance.step(unit(add(up, noise(0.002))), gyro, this.wheelSpeeds, dt, learning)

    if (this.hand) {
      // held by the hand: the hand takes the wheels' push, the cube only turns where it is put
      rotateInBody(this.orientation, this.bodyRate, dt)
      this.wheelSpeeds = add(this.wheelSpeeds, scale(state.lastTorque, dt / wheelInertia))
    } else {
      this.#physicsStep(state.lastTorque, dt)
    }

    state.lastTorque = torque

    this.#maybeCornerTakesOver(up, dt)
    this.#maybeLandOnTheEdge()
    this.#maybeEdgeTakesOver(up)
    this.#maybeStop()
  }

  #tickFalling = dt => {
    this.#physicsStep(this.#brakeWheels(dt), dt)
    if (this.#lowestTip() >= -1) return

    this.state = 'fallen'
    this.bodyRate = vector(0, 0, 0)
    this.#say('on the floor - Corner or an Edge to stand it up again')
  }

  #tickFallen = dt => {
    this.wheelSpeeds = add(this.wheelSpeeds, scale(this.#brakeWheels(dt), dt / wheelInertia))
    this.#turnTheWheels(dt)
  }

  // ----- the physics --------------------------------------------------------------------------

  // What it stands on decides where its tip is and how hard it is to turn about that tip.
  #useGeometry = mode => {
    this.physicsMode = mode
    this.edgeAxis = edgeAxisOf(mode)

    const down = vector(-1, -1, -1)
    if (this.edgeAxis >= 0) down[['x', 'y', 'z'][this.edgeAxis]] = 0

    this.pivot = scale(unit(down), this.edgeAxis < 0 ? cornerTip : edgeTip)
    this.fromPivot = subtract(this.gravityOffset, this.pivot)

    // its own inertia, carried out to the tip it turns on (the parallel axis rule)
    const arm = this.fromPivot
    const armSquared = dot(arm, arm)
    this.inertia = [
      cubeInertia + mass * (armSquared - arm.x * arm.x), -mass * arm.x * arm.y, -mass * arm.x * arm.z,
      -mass * arm.y * arm.x, cubeInertia + mass * (armSquared - arm.y * arm.y), -mass * arm.y * arm.z,
      -mass * arm.z * arm.x, -mass * arm.z * arm.y, cubeInertia + mass * (armSquared - arm.z * arm.z)
    ]
  }

  // Put it down with this way of its own pointing straight up (an edge is kept level).
  #pose = upInBody => {
    const up = unit(upInBody)
    let x = this.edgeAxis >= 0 ? axisVector(this.edgeAxis) : subtract(vector(1, 0, 0), scale(up, up.x))

    x = unit(subtract(x, scale(up, dot(up, x))))
    const y = cross(up, x)

    this.orientation = [x.x, x.y, x.z, y.x, y.y, y.z, up.x, up.y, up.z]
    this.bodyRate = vector(0, 0, 0)
  }

  #physicsStep = (torque, dt) => {
    const { orientation, inertia, bodyRate } = this
    const gravityInBody = rotateBack(orientation, vector(0, 0, -gravity))
    const fromGravity = cross(this.fromPivot, scale(gravityInBody, mass))
    const upInBody = rotateBack(orientation, vector(0, 0, 1))

    const momentum = add(rotate(inertia, bodyRate), scale(this.wheelSpeeds, wheelInertia))
    let turning = subtract(subtract(fromGravity, cross(bodyRate, momentum)), torque)

    // the tip rubbing as it twists, and a little air
    turning = subtract(turning, add(
      scale(upInBody, twistFriction * dot(bodyRate, upInBody)),
      scale(bodyRate, airFriction)
    ))

    const speedingUp = this.edgeAxis < 0 ? solve(inertia, turning) : this.#alongTheEdge(turning)
    this.bodyRate = add(bodyRate, scale(speedingUp, dt))

    // on an edge it can only turn about that edge, nothing else
    if (this.edgeAxis >= 0) {
      const axis = axisVector(this.edgeAxis)
      this.bodyRate = scale(axis, dot(this.bodyRate, axis))
    }

    this.wheelSpeeds = add(this.wheelSpeeds, scale(subtract(scale(torque, 1 / wheelInertia), speedingUp), dt))
    rotateInBody(this.orientation, this.bodyRate, dt)
    this.#turnTheWheels(dt)
  }

  #alongTheEdge = turning => {
    const axis = axisVector(this.edgeAxis)
    return scale(axis, dot(axis, turning) / dot(axis, rotate(this.inertia, axis)))
  }

  #turnTheWheels = dt => {
    this.wheelAngles[0] += this.wheelSpeeds.x * dt
    this.wheelAngles[1] += this.wheelSpeeds.y * dt
    this.wheelAngles[2] += this.wheelSpeeds.z * dt
  }

  // slowing the wheels down when the cube is no longer standing
  #brakeWheels = dt => {
    const speeds = [this.wheelSpeeds.x, this.wheelSpeeds.y, this.wheelSpeeds.z]
    const [x, y, z] = speeds.map(one => -Math.sign(one) * Math.min(0.05, Math.abs(one) * wheelInertia / dt))
    return vector(x, y, z)
  }

  // how far below the floor the lowest corner tip has gone, in mm (the tip it turns on is at 0)
  #lowestTip = () => Math.min(...tips.map(tip => this.#tipHeight(tip)))

  #tipHeight = tip => this.pivotPlace.z + rotate(this.orientation, subtract(tip, scale(this.pivot, 1000))).z

  #tipInTheRoom = () => subtract(this.pivotPlace, rotate(this.orientation, scale(this.pivot, 1000)))

  // ----- the things that can happen while it stands -------------------------------------------

  // The controller's balance point walks from the corner's towards the edge's, slowly.
  #walkTheLowering = dt => {
    const lowering = this.lowering
    if (!lowering) return

    lowering.time += dt
    lowering.angle = Math.min(lowering.total, lowering.angle + lowerRate * dt)
    const howFar = lowering.total > 1e-5 ? lowering.angle / lowering.total : 1

    balance.balancePoint = blend(lowering.from, lowering.to, howFar)
  }

  // The hand turns it about its tip, slowing down as it arrives.
  #handLifts = dt => {
    const up = rotateBack(this.orientation, vector(0, 0, 1))
    const wanted = this.trueBalancePoint()
    const away = angleDegrees(up, wanted)
    const rate = away > 0.2 ? Math.min(20, Math.max(1, 3 * away)) / degreesPerRadian : 0

    // the way to turn that brings 'up' towards where we want it
    this.bodyRate = scale(unit(cross(wanted, up)), rate)
    this.hand.time += dt
  }

  // On an edge, held within the corner's take-over angle and quiet: the corner takes over.
  #maybeCornerTakesOver = (up, dt) => {
    if (this.mode === 'corner' || this.lowering) return

    const close = angleDegrees(up, this.savedPoints.corner) < takeOverDegrees && length(this.bodyRate) < 0.3
    if (!close) {
      this.liftTime = 0
      return
    }

    this.liftTime += dt
    if (this.liftTime <= takeOverStill) return

    const was = this.mode
    this.savedPoints[was] = { ...balance.balancePoint }
    this.liftTime = 0
    this.#switchTo('corner', up)

    if (this.hand) {
      this.hand = null
      this.#say('the hand lets go')
    }
    this.#say('on the corner - BALANCING (from ' + was + ')', true)
  }

  // The floor: the edge's other corner touches, and the cube comes to lie on that edge.
  #maybeLandOnTheEdge = () => {
    const landingOn = this.landingOn
    if (!landingOn) return

    const axis = edgeAxisOf(landingOn)
    const tip = tips[1 << axis]
    if (this.#tipHeight(tip) > 0) return

    this.landingOn = null
    const corner = this.#tipInTheRoom()
    const along = axisVector(axis)

    // the knock takes away everything but the turning along the edge
    this.bodyRate = scale(along, dot(this.bodyRate, along))
    this.#useGeometry(landingOn)
    this.pivotPlace = add(corner, rotate(this.orientation, scale(this.pivot, 1000)))

    this.#say('corner ' + (1 << axis) + ' on the floor at ' +
      (length(this.bodyRate) * 57.3).toFixed(0) + ' deg/s left along the edge')
  }

  // Within 4 degrees of the edge's own point, the edge takes over.
  #maybeEdgeTakesOver = up => {
    const lowering = this.lowering
    if (!lowering || angleDegrees(up, lowering.to) >= lowerLand * degreesPerRadian) return

    this.savedPoints.corner = { ...lowering.from }
    const to = lowering.mode
    this.lowering = null
    this.#switchTo(to, up)

    this.#say('on ' + to + ' - BALANCING (' + lowering.time.toFixed(1) + ' s)', true)
  }

  #maybeStop = () => {
    const fastest = Math.max(Math.abs(this.wheelSpeeds.x), Math.abs(this.wheelSpeeds.y), Math.abs(this.wheelSpeeds.z))
    if (fastest > wheelMax) return this.off('STOPPED: a wheel too fast')
    if (this.lowering && this.lowering.time > lowerSeconds) return this.off('STOPPED: did not reach the edge')

    // on an edge, only the tilt ABOUT that edge counts
    const acrossTheEdge = balance.edgeAxis >= 0
      ? Math.asin(Math.min(1, Math.abs(component(balance.tilt, balance.edgeAxis)))) * degreesPerRadian
      : balance.tiltDegrees

    const tilt = this.lowering ? balance.tiltDegrees : acrossTheEdge
    const tooFar = this.lowering ? lowerTiltStop : fallDegrees
    if (tilt > tooFar) this.off('STOPPED: tilted ' + fallDegrees + ' deg - it falls')
  }

  // ----- the small inside helpers ---------------------------------------------------------------

  // the controller moves to a new way of standing, keeping the tilt it already sees
  #switchTo = (mode, up) => {
    this.mode = mode
    balance.edgeAxis = edgeAxisOf(mode)
    this.#useGains(mode)
    balance.balancePoint = { ...this.savedPoints[mode] }
    this.runTime = 0

    balance.tilt = cross(up, balance.balancePoint)
    balance.tiltDegrees = angleDegrees(up, balance.balancePoint)
  }

  #useGains = mode => {
    const gains = mode === 'corner' ? cornerGains : edgeGains

    balance.angleGain = gains.angleGain
    balance.rateGain = gains.rateGain
    balance.wheelGain = gains.wheelGain
    this.gainsFor = mode
  }

  // 'go' while it is already balancing: the firmware says stop first, unless it is corner -> edge
  #alreadyStanding = mode => {
    if (this.mode === 'corner' && mode !== 'corner' && !this.lowering) return this.#startLowering(mode)
    if (this.mode !== 'corner' && mode === 'corner') {
      return this.#say('lift it by hand onto corner 0 - the corner takes over within 4 deg (Lift)')
    }
    this.#say(this.lowering ? 'lowering - wait' : 'stop first')
  }

  #startLowering = mode => {
    const from = { ...balance.balancePoint }
    const to = { ...this.savedPoints[mode] }
    const total = Math.acos(Math.max(-1, Math.min(1, dot(from, to))))

    this.lowering = { mode, from, to, total, angle: 0, time: 0 }
    this.landingOn = mode   // it stays on the corner tip until that edge's other corner touches

    this.#say('lowering onto ' + mode + ' (' + (total * degreesPerRadian).toFixed(0) +
      ' deg) - it falls the last part, braked')
  }

  #say = (text, strong) => {
    this.said.push({ text, strong: !!strong })
  }
}

const cubeSim = new CubeSim()

export default CubeSim
export { cubeSim, balance, tips, standDirection }
export { mass, gravity, cubeInertia, wheelInertia, cornerTip, edgeTip, wheelMax, fallDegrees }
