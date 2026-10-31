import * as THREE from '../lib/three.module.js'
import state from './state.js'
import settings from './settings.js'
import { subtract, scale, rotate } from './math.js'
import { sphereSim, control } from './sphere-sim.js'
import { cubeSim, tips } from './cube-sim.js'
import { tick } from './commands.js'
import { view } from './view3d.js'
import { showPanel } from './panel.js'
import { route } from './route.js'

const { panelEvery, longestStep, smoothLive, lookAtSphere, lookAtCube } = settings.loop
const step = settings.simulation.step
const radiansToRpm = 9.549
const degreesPerRadian = 57.29578

// the cube's nine numbers as a turn three.js understands
const asTurn = matrix => new THREE.Matrix4().set(
  matrix[0], matrix[1], matrix[2], 0,
  matrix[3], matrix[4], matrix[5], 0,
  matrix[6], matrix[7], matrix[8], 0,
  0, 0, 0, 1
)

class Loop {
  #frameAsked = null

  constructor() {
    this.last = 0
    this.owed = 0            // time the simulation still has to catch up, in seconds
    this.sincePanel = 0

    // what we draw for the real cube between its readings
    this.liveWheelAngles = [0, 0, 0]
    this.livePlace = null
    this.liveTurn = null
    this.tipPoints = null
  }

  start = () => {
    this.last = performance.now()
    this.#frameAsked = requestAnimationFrame(this.#frame)
  }

  stop = () => {
    if (this.#frameAsked !== null) cancelAnimationFrame(this.#frameAsked)
    this.#frameAsked = null
  }

  #frame = now => {
    // after a long wait (another window, a sleeping laptop) we do not try to catch up all of it
    const elapsed = Math.min(longestStep, (now - this.last) / 1000)
    this.last = now

    const where = state.liveOn ? this.#followTheRealCube(elapsed) : this.#walkTheSimulation(elapsed)

    view.draw({
      kind: state.kind,
      px: where.px,
      py: where.py,
      heading: where.heading,
      lookZ: where.lookZ,
      cg: state.kind === 'sphere' ? sphereSim.gravityOffset : cubeSim.gravityOffset,
      skip: state.updating
    })

    this.#everyTenthOfASecond(elapsed)
    route.tick(now)

    this.#frameAsked = requestAnimationFrame(this.#frame)
  }

  #everyTenthOfASecond = elapsed => {
    this.sincePanel += elapsed
    if (this.sincePanel <= panelEvery) return

    this.sincePanel = 0
    showPanel()
  }

  // ----- the simulation ----------------------------------------------------------------------

  #walkTheSimulation = elapsed => {
    this.owed += elapsed

    // always the same small step, however fast or slow the screen is
    while (this.owed >= step) {
      tick(step)
      this.owed -= step
    }

    return state.kind === 'sphere' ? this.#drawTheSphere() : this.#drawTheCube()
  }

  #drawTheSphere = () => {
    const { body } = view

    body.quaternion.setFromRotationMatrix(asTurn(sphereSim.orientation))
    body.position.set(sphereSim.placeX * 1000, sphereSim.placeY * 1000, view.sphereRadius)
    this.#turnTheWheels(sphereSim.wheelAngles)

    return { px: sphereSim.placeX, py: sphereSim.placeY, heading: control.wantedHeading, lookZ: lookAtSphere }
  }

  #drawTheCube = () => {
    const { body } = view

    // the tip it turns on - a corner or an edge - sits on the floor at the middle of the scene
    body.quaternion.setFromRotationMatrix(asTurn(cubeSim.orientation))
    const tipInTheRoom = subtract(cubeSim.pivotPlace, rotate(cubeSim.orientation, scale(cubeSim.pivot, 1000)))

    body.position.set(tipInTheRoom.x, tipInTheRoom.y, tipInTheRoom.z)
    this.#turnTheWheels(cubeSim.wheelAngles)

    return { px: 0, py: 0, heading: 0, lookZ: lookAtCube }
  }

  // ----- the real cube ---------------------------------------------------------------------------

  #followTheRealCube = elapsed => {
    this.owed = 0
    this.#readyTheLivePieces()

    const reading = state.liveTelemetry
    if (reading?.q) this.#smoothTowardsTheReading(reading, elapsed)

    this.#turnTheWheels(this.liveWheelAngles)
    return state.kind === 'sphere' ? this.#drawTheLiveSphere(reading) : this.#drawTheLiveCube()
  }

  #readyTheLivePieces = () => {
    if (this.livePlace) return

    this.livePlace = new THREE.Vector3(0, 0, view.sphereRadius)
    this.liveTurn = new THREE.Quaternion()
    this.tipPoints = tips.map(tip => new THREE.Vector3(tip.x, tip.y, tip.z))
  }

  // The cube speaks about ten times a second. Jumping to each reading would look like stuttering,
  // so the picture walks part of the way there every frame, and the eye sees one smooth move.
  #smoothTowardsTheReading = (reading, elapsed) => {
    const howMuch = 1 - Math.exp(-elapsed / smoothLive)

    this.liveTurn.set(reading.q[1], reading.q[2], reading.q[3], reading.q[0])
    view.body.quaternion.slerp(this.liveTurn, howMuch)

    if (reading.sph) {
      this.livePlace.lerp(new THREE.Vector3(reading.p[0] * 10, reading.p[1] * 10, view.sphereRadius), howMuch)
    }

    for (let i = 0; i < 3; i++) this.liveWheelAngles[i] += (reading.w[i] / radiansToRpm) * elapsed
  }

  #drawTheLiveSphere = reading => {
    view.body.position.copy(this.livePlace)

    return {
      px: this.livePlace.x / 1000,
      py: this.livePlace.y / 1000,
      heading: reading?.sph ? reading.h / degreesPerRadian : 0,
      lookZ: lookAtSphere
    }
  }

  // the bare cube: whichever of its eight tips is lowest is the one standing on the floor
  #drawTheLiveCube = () => {
    const { body } = view
    let lowest = 1e9

    this.tipPoints.forEach(tip => {
      const z = tip.clone().applyQuaternion(body.quaternion).z
      if (z < lowest) lowest = z
    })

    body.position.set(0, 0, -lowest)
    return { px: 0, py: 0, heading: 0, lookZ: lookAtCube }
  }

  #turnTheWheels = angles => {
    const { wheels } = view

    wheels[0].rotation.set(angles[0], 0, 0)
    wheels[1].rotation.set(0, angles[1], 0)
    wheels[2].rotation.set(0, 0, angles[2])
  }
}

const loop = new Loop()

export { loop }
