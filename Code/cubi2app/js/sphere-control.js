import { vector, add, subtract, scale, dot, cross, length, unit, noTurn, rotate, rotateBack, rotateInBody, rotateInWorld } from './math.js'
import settings from './settings.js'

const sphere = settings.sphere
const gains = settings.sphereGains
const moves = settings.sphereMoves

// what the sphere is busy with
const stopped = 0
const going = 1      // rolling a distance
const turning = 2    // turning on the spot
const settling = 3   // coming to rest on a navel

class Sphere {
  constructor() {
    this.sphereRadius = sphere.controlRadius   // in meters

    // how hard the wheels answer
    this.speedGain = gains.speed
    this.placeGain = gains.place
    this.headingGain = gains.heading
    this.headingRateGain = gains.headingRate
    this.unwindGain = gains.unwind       // standing still: it unwinds the spin the wheels have kept
    this.navelGain = gains.navel         // coming to rest on a navel

    this.wheelInertia = sphere.wheelInertia
    this.maxTorque = sphere.maxTorque    // in Nm
    this.speed = moves.speed             // how fast it rolls, in m/s
    this.accel = moves.accel             // how fast it picks that up
    this.turnRate = moves.turnRate
    this.tiltSettle = moves.tiltSettle   // how fast it trusts gravity again, in seconds

    this.orientation = noTurn() // which way the sphere is turned, seen from the room
    this.heading = 0            // where its front points
    this.placeX = 0             // where it is on the floor, in meters
    this.placeY = 0
    this.worldRate = vector(0, 0, 0)

    this.wantedX = 0            // where we have asked it to be
    this.wantedY = 0
    this.wantedHeading = 0
    this.wantedSpeed = 0
    this.wantedTurnRate = 0

    this.wayX = 1               // the way it is headed while it rolls; while it turns, which way round
    this.wayY = 0
    this.leftToGo = 0
    this.doing = stopped

    this.navel = -1             // which navel it is going to rest on, -1 for none
    this.navelReached = false
    this.navelUp = vector(0, 0, 1)
    this.torque = vector(0, 0, 0)
  }

  // Standing it up: the sphere is told which way is up, and it builds its own frame from that.
  begin = up => {
    const z = unit(up)
    let x = subtract(vector(1, 0, 0), scale(z, z.x))

    // if x came out too short, up was nearly along x - so the other side is used
    if (length(x) < 0.3) x = subtract(vector(0, 1, 0), scale(z, z.y))
    x = unit(x)

    const y = cross(z, x)
    this.orientation = [x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z]

    this.heading = 0
    this.placeX = 0
    this.placeY = 0
    this.#forgetWhereItWasGoing()
  }

  // 'front' is where the side of wheel X points now
  front = () => {
    this.heading = 0
    this.wantedHeading = 0
  }

  halt = () => {
    this.#forgetWhereItWasGoing()
    this.wantedX = this.placeX
    this.wantedY = this.placeY
    this.wantedHeading = this.heading
    this.navel = -1
  }

  // come to rest on navel k. The three bits of k say which way each corner of the cube leans.
  restOn = k => {
    this.halt()
    this.navel = k
    this.navelReached = false
    this.navelUp = unit(vector(k & 1 ? -1 : 1, k & 2 ? -1 : 1, k & 4 ? -1 : 1))
    this.doing = settling
  }

  // roll this far, in meters. A number below zero rolls backwards.
  move = meters => {
    this.halt()
    const back = meters >= 0 ? 1 : -1

    this.wayX = Math.cos(this.wantedHeading) * back
    this.wayY = Math.sin(this.wantedHeading) * back
    this.leftToGo = Math.abs(meters)
    this.doing = going
  }

  // turn this far on the spot, in radians
  turn = radians => {
    this.halt()
    this.wayX = radians >= 0 ? 1 : -1
    this.leftToGo = Math.abs(radians)
    this.doing = turning
  }

  // One step. acceleration and gyro are what the sphere feels; wheels is how fast they spin.
  step = (acceleration, gyro, wheels, dt) => {
    this.#followTheGyro(acceleration, gyro, dt)
    this.#keepCount(gyro, dt)
    this.#walkTheWantedPlace(dt)

    const wanted = this.#wantedTurn(wheels)
    const turnedBack = scale(rotateBack(this.orientation, wanted), -1)
    const hold = turn => Math.max(-this.maxTorque, Math.min(this.maxTorque, turn))

    this.torque = vector(hold(turnedBack.x), hold(turnedBack.y), hold(turnedBack.z))
    return this.torque
  }

  #forgetWhereItWasGoing = () => {
    this.doing = stopped
    this.wantedSpeed = 0
    this.wantedTurnRate = 0
    this.leftToGo = 0
  }

  // The gyro says how it turned. Gravity is trusted slowly, so a bump does not fool it.
  #followTheGyro = (acceleration, gyro, dt) => {
    rotateInBody(this.orientation, gyro, dt)
    const strength = length(acceleration)

    // only when what it feels is about one g - otherwise it is being shaken, not held
    if (strength > 0.8 && strength < 1.2) {
      const inWorld = rotate(this.orientation, scale(acceleration, 1 / strength))
      rotateInWorld(this.orientation, scale(cross(inWorld, vector(0, 0, 1)), dt / this.tiltSettle))
    }
  }

  // where it has got to: its heading, and how far it has rolled on the floor.
  // A ball that turns this fast rolls its radius times that far.
  #keepCount = (gyro, dt) => {
    this.worldRate = rotate(this.orientation, gyro)

    this.heading += this.worldRate.z * dt
    this.placeX += this.sphereRadius * this.worldRate.y * dt
    this.placeY -= this.sphereRadius * this.worldRate.x * dt
  }

  #walkTheWantedPlace = dt => {
    if (this.doing === going) {
      const topSpeed = Math.min(this.speed, Math.sqrt(2 * this.accel * Math.max(this.leftToGo, 0)))
      this.wantedSpeed = Math.min(this.wantedSpeed + this.accel * dt, topSpeed)

      const stepDistance = Math.min(this.wantedSpeed * dt, this.leftToGo)
      this.wantedX += this.wayX * stepDistance
      this.wantedY += this.wayY * stepDistance
      this.leftToGo -= stepDistance

      if (this.leftToGo <= 0.0005) this.#forgetWhereItWasGoing()
      return
    }

    if (this.doing === turning) {
      const topTurnRate = Math.min(this.turnRate, Math.sqrt(2 * Math.max(this.leftToGo, 0)))
      this.wantedTurnRate = Math.min(this.wantedTurnRate + dt, topTurnRate)

      const stepAngle = Math.min(this.wantedTurnRate * dt, this.leftToGo)
      this.wantedHeading += this.wayX * stepAngle
      this.leftToGo -= stepAngle

      if (this.leftToGo <= 0.001) this.#forgetWhereItWasGoing()
    }
  }

  #wantedTurn = wheels => {
    const { sphereRadius, worldRate } = this
    const alongX = this.doing === going ? this.wayX * this.wantedSpeed : 0
    const alongY = this.doing === going ? this.wayY * this.wantedSpeed : 0
    const aroundZ = this.doing === turning ? this.wayX * this.wantedTurnRate : 0

    const wanted = vector(
      this.speedGain * (-alongY / sphereRadius - worldRate.x),
      this.speedGain * (alongX / sphereRadius - worldRate.y),
      0
    )

    if (this.doing === going) this.#pullBackToTheLine(wanted)
    else if (this.doing === settling) this.#leanOntoTheNavel(wanted)
    else this.#unwindTheWheels(wanted, wheels)

    wanted.z = this.headingGain * (this.wantedHeading - this.heading) +
      this.headingRateGain * (aroundZ - worldRate.z)
    return wanted
  }

  // while rolling: how far it has drifted off the line it was asked to follow
  #pullBackToTheLine = wanted => {
    const offX = this.wantedX - this.placeX
    const offY = this.wantedY - this.placeY

    wanted.x += this.placeGain * (-offY / this.sphereRadius)
    wanted.y += this.placeGain * (offX / this.sphereRadius)
  }

  // coming to rest: it leans until the navel we asked for is the one pointing up
  #leanOntoTheNavel = wanted => {
    const navelInWorld = rotate(this.orientation, this.navelUp)
    const across = cross(navelInWorld, vector(0, 0, 1))
    const size = length(across)
    const angle = Math.atan2(size, navelInWorld.z)

    if (size > 1e-6) {
      const gain = this.navelGain * Math.min(angle, 0.6) / size
      wanted.x += gain * across.x
      wanted.y += gain * across.y
    }

    if (angle < 0.035) this.navelReached = true
    this.wantedX = this.placeX
    this.wantedY = this.placeY
  }

  // standing still: the spin the wheels have kept is given back slowly, so they do not sit fast
  #unwindTheWheels = (wanted, wheels) => {
    this.wantedX = this.placeX
    this.wantedY = this.placeY

    const spinInWorld = rotate(this.orientation, scale(wheels, this.wheelInertia))
    wanted.x += this.unwindGain * spinInWorld.x
    wanted.y += this.unwindGain * spinInWorld.y
  }
}

export default Sphere
export { stopped, going, turning, settling }
