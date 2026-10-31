import { vector, add, subtract, scale, dot, cross, length, unit } from './math.js'
import { settling } from './sphere-control.js'

// what it is busy with
const goingToNavel = 0
const measuring = 1
const waiting = 2

const gravity = 9.81
const radiansToRpm = 9.549
const stillGyro = 0.05        // quieter than this counts as still
const stillFor = 0.3          // and it has to stay that quiet this long, in seconds
const settleAtMost = 10       // a navel it cannot settle on in this long is left out
const wheelsTooFast = 300     // in radians a second
const wheelsSlowAgain = 60
const measureFor = 0.6        // how long the wheels are watched, in seconds
const measureLeast = 30       // and at least this many readings

// the center of gravity as a line to show. measured: how many navels it came from.
const gravityText = (offset, measured) =>
  'center of gravity ' +
  [offset.x, offset.y, offset.z].map(one => (one >= 0 ? '+' : '') + (one * 1000).toFixed(2)).join(' ') +
  ' mm (cube frame), ' + (length(offset) * 1000).toFixed(2) + ' mm from the center' +
  (measured ? ' (' + measured + ' navels)' : '')

// 'where G N': from how the sphere hangs without a weight and with a known weight on it.
// downBefore and downAfter are the two ways it hung; weight in kg, at where it was put.
const gravityFromWeight = (downBefore, downAfter, mass, weight, at) => {
  const across = cross(downBefore, downAfter)
  const size = dot(across, across)

  // the two hangs were almost the same - the weight told us nothing
  if (size < 3e-5) return null

  const howFar = -weight * dot(cross(at, downAfter), across) / (mass * size)
  return howFar > 0 ? scale(downBefore, howFar) : null
}

class GravityMeasure {
  constructor(mass, wheelInertia) {
    this.mass = mass
    this.wheelInertia = wheelInertia
    this.active = false
    this.offset = null       // the answer, once all the navels are done
    this.measured = 0        // how many navels gave a reading
  }

  start = () => {
    this.active = true
    this.offset = null
    this.measured = 0
    this.navel = 0
    this.firstWait = true    // the very first wait only lets the wheels slow down
    this.doing = waiting
    this.time = 0
    this.quietFor = 0

    // what the eight measurements add up to, to be solved at the end
    this.leftSums = [0, 0, 0, 0, 0, 0, 0, 0, 0]
    this.rightSums = [0, 0, 0]
    this.#forgetThisNavel()
  }

  // One step. Returns a line to show, or null when there is nothing to say yet.
  step = (sphere, gyro, wheels, dt) => {
    const fastestWheel = Math.max(Math.abs(wheels.x), Math.abs(wheels.y), Math.abs(wheels.z))
    this.time += dt

    if (this.doing === goingToNavel) return this.#settleOnTheNavel(sphere, gyro, fastestWheel, dt)
    if (this.doing === measuring) return this.#watchTheWheels(sphere, wheels, fastestWheel)
    return this.#waitForTheWheels(fastestWheel)
  }

  // Turning onto this navel, then holding still for a moment before the reading starts.
  #settleOnTheNavel = (sphere, gyro, fastestWheel, dt) => {
    if (sphere.doing !== settling || sphere.navel !== this.navel) sphere.restOn(this.navel)

    this.quietFor = (this.time > 1.0 && length(gyro) < stillGyro) ? this.quietFor + dt : 0
    if (this.quietFor > stillFor) {
      this.doing = measuring
      this.time = 0
      this.#forgetThisNavel()
      return null
    }

    if (this.time > settleAtMost || fastestWheel > wheelsTooFast) {
      const navel = this.navel
      const why = fastestWheel > wheelsTooFast ? 'the wheels got too fast first' : 'did not settle'

      sphere.halt()
      this.doing = waiting
      this.time = 0
      return 'navel ' + navel + ': ' + why + ' - left out'
    }

    return null
  }

  // The reading: how fast the wheels are speeding up, as a straight line through their speeds.
  #watchTheWheels = (sphere, wheels, fastestWheel) => {
    const time = this.time

    this.count++
    this.sumTime += time
    this.sumTimeTime += time * time
    this.sumWheels = add(this.sumWheels, wheels)
    this.sumTimeWheels = add(this.sumTimeWheels, scale(wheels, time))
    this.upSum = add(this.upSum, vector(sphere.orientation[6], sphere.orientation[7], sphere.orientation[8]))

    const longEnough = time >= measureFor || fastestWheel > wheelsTooFast
    if (!longEnough || this.count < measureLeast) return null

    const slope = this.#wheelSlope()
    this.#addToTheAnswer(slope)
    this.measured++

    const navel = this.navel
    sphere.halt()
    this.doing = waiting
    this.time = 0

    return 'navel ' + navel + ': wheels ' +
      [slope.x, slope.y, slope.z].map(one => (one * radiansToRpm).toFixed(0)).join(' ') + ' rpm/s'
  }

  // Holding: the wheels slow down and the sphere leans back, then the next navel.
  #waitForTheWheels = fastestWheel => {
    const slowed = fastestWheel < wheelsSlowAgain || this.time > 12
    if (!slowed || this.time <= 0.2) return null

    // the very first wait is only there to let the wheels come down, no navel is finished yet
    if (!this.firstWait) this.navel++
    this.firstWait = false

    if (this.navel >= 8) return this.finish()

    this.doing = goingToNavel
    this.time = 0
    this.quietFor = 0
    return null
  }

  // the straight line through the wheel speeds: its slope is how fast they are speeding up
  #wheelSlope = () => {
    const { count, sumTime, sumTimeTime, sumWheels, sumTimeWheels } = this
    const spread = count * sumTimeTime - sumTime * sumTime

    return scale(subtract(scale(sumTimeWheels, count), scale(sumWheels, sumTime)), 1 / spread)
  }

  // This navel's share of the answer is added in. rows is what turning the offset by gravity does.
  #addToTheAnswer = slope => {
    const down = scale(unit(this.upSum), -gravity)
    const mass = this.mass
    const rows = [
      0, mass * down.z, -mass * down.y,
      -mass * down.z, 0, mass * down.x,
      mass * down.y, -mass * down.x, 0
    ]
    const rightHand = scale(slope, this.wheelInertia)

    for (let i = 0; i < 3; i++) {
      for (let j = 0; j < 3; j++) {
        this.leftSums[3 * i + j] += rows[i] * rows[j] + rows[3 + i] * rows[3 + j] + rows[6 + i] * rows[6 + j]
      }
    }

    this.rightSums[0] += rows[0] * rightHand.x + rows[3] * rightHand.y + rows[6] * rightHand.z
    this.rightSums[1] += rows[1] * rightHand.x + rows[4] * rightHand.y + rows[7] * rightHand.z
    this.rightSums[2] += rows[2] * rightHand.x + rows[5] * rightHand.y + rows[8] * rightHand.z
  }

  #forgetThisNavel = () => {
    this.count = 0
    this.sumTime = 0
    this.sumTimeTime = 0
    this.sumWheels = vector(0, 0, 0)
    this.sumTimeWheels = vector(0, 0, 0)
    this.upSum = vector(0, 0, 0)
  }

  // all eight navels are done: the measurements are put together into one answer
  finish = () => {
    this.active = false
    this.firstWait = false
    if (this.measured < 3) return 'cg: only ' + this.measured + ' navels measured - not enough'

    const left = this.leftSums
    const right = this.rightSums
    const determinant =
      left[0] * (left[4] * left[8] - left[5] * left[7]) -
      left[1] * (left[3] * left[8] - left[5] * left[6]) +
      left[2] * (left[3] * left[7] - left[4] * left[6])

    const back = [
      left[4] * left[8] - left[5] * left[7], left[2] * left[7] - left[1] * left[8], left[1] * left[5] - left[2] * left[4],
      left[5] * left[6] - left[3] * left[8], left[0] * left[8] - left[2] * left[6], left[2] * left[3] - left[0] * left[5],
      left[3] * left[7] - left[4] * left[6], left[1] * left[6] - left[0] * left[7], left[0] * left[4] - left[1] * left[3]
    ].map(one => one / determinant)

    this.offset = vector(
      back[0] * right[0] + back[1] * right[1] + back[2] * right[2],
      back[3] * right[0] + back[4] * right[1] + back[5] * right[2],
      back[6] * right[0] + back[7] * right[1] + back[8] * right[2]
    )

    return gravityText(this.offset, this.measured)
  }
}

export default GravityMeasure
export { gravityText, gravityFromWeight }
