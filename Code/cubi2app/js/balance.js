import { vector, add, subtract, scale, dot, cross, length, unit, component, axisVector, degreesPerRadian } from './math.js'
import settings from './settings.js'

const start = settings.cubeGains.corner
const how = settings.balance
const cube = settings.cube

class Balance {
  constructor() {
    // how hard the wheels answer: to the lean, to how fast it is leaning, to their own speed
    this.angleGain = start.angleGain
    this.rateGain = start.rateGain
    this.wheelGain = start.wheelGain
    this.maxTorque = cube.maxTorque      // in Nm - the motors cannot give more

    this.learnRate = how.learnRate       // how fast it moves its balance point while it stands
    this.twist = how.twist               // how much it fights a turn around the corner it stands on
    this.spin = how.spin                 // how much it lets the wheels unwind their own spin
    this.brakeGain = how.brakeGain       // on an edge, the two free wheels are slowed gently
    this.brakeMax = how.brakeMax

    this.edgeAxis = -1         // -1 on the corner; 0, 1 or 2 when it lies on that edge
    this.balancePoint = unit(vector(1, 1, 1))   // the way inside the cube that has to point up
    this.torque = vector(0, 0, 0)
    this.tilt = vector(0, 0, 0)                 // how far it leans, as a direction
    this.tiltDegrees = 0
  }

  // One step. up: where up is, seen by the cube. gyro: how fast it is turning.
  // wheels: how fast the three wheels spin. dt: the time of this step.
  step = (up, gyro, wheels, dt, learning) => {
    const { balancePoint } = this

    this.tilt = cross(up, balancePoint)
    this.tiltDegrees = Math.asin(Math.min(1, length(this.tilt))) * degreesPerRadian
    if (dot(up, balancePoint) < 0) this.tiltDegrees = 180 - this.tiltDegrees

    // the turn around the corner it stands on is handled apart from the rest
    const gyroAlong = scale(balancePoint, dot(gyro, balancePoint))
    let rate = add(subtract(gyro, gyroAlong), scale(gyroAlong, this.twist))
    let tilt = this.tilt
    let spinning = wheels

    // lying on an edge: only the wheel of that edge works
    if (this.edgeAxis >= 0) {
      const axis = axisVector(this.edgeAxis)
      tilt = scale(axis, component(tilt, this.edgeAxis))
      rate = scale(axis, component(gyro, this.edgeAxis))
      spinning = scale(axis, component(wheels, this.edgeAxis))
    }

    // on the corner, the spin the wheels have built up around the balance point is unwound slowly
    let spinningAlong = vector(0, 0, 0)
    if (this.edgeAxis < 0) {
      spinningAlong = scale(balancePoint, dot(spinning, balancePoint))
      spinning = subtract(spinning, spinningAlong)
    }

    const wanted = add(
      add(add(scale(tilt, this.angleGain), scale(rate, this.rateGain)), scale(spinning, this.wheelGain)),
      scale(spinningAlong, -this.spin * this.wheelGain)
    )

    if (this.edgeAxis >= 0) this.#brakeTheOtherTwo(wanted, wheels)

    const hold = turn => Math.max(-this.maxTorque, Math.min(this.maxTorque, turn))
    this.torque = vector(hold(wanted.x), hold(wanted.y), hold(wanted.z))

    if (learning) this.#moveTheBalancePoint(spinning, dt)

    return this.torque
  }

  // On an edge the other two wheels are not needed, so they are slowed gently instead of left free.
  #brakeTheOtherTwo = (wanted, wheels) => {
    const names = ['x', 'y', 'z']

    names.forEach((name, k) => {
      if (k === this.edgeAxis) return
      const slow = -this.brakeGain * component(wheels, k)
      wanted[name] = Math.max(-this.brakeMax, Math.min(this.brakeMax, slow))
    })
  }

  // While it stands quietly, wheels that keep speeding up one way mean the balance point is a
  // little off. It is moved towards where the wheels say it should be - this is 'learn'.
  #moveTheBalancePoint = (spinning, dt) => {
    const { balancePoint } = this
    if (this.learnRate <= 0 || this.tiltDegrees >= how.learnBelowDegrees) return

    const across = subtract(spinning, scale(balancePoint, dot(spinning, balancePoint)))
    const lean = scale(across, -this.wheelGain / this.angleGain)

    this.balancePoint = unit(add(balancePoint, scale(cross(balancePoint, lean), this.learnRate * dt)))
  }
}

export default Balance
