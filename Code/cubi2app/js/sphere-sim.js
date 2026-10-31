import { vector, add, subtract, scale, cross, length, unit, noTurn, rotate, rotateBack, rotateInBody } from './math.js'
import Sphere from './sphere-control.js'
import GravityMeasure from './gravity-measure.js'
import settings from './settings.js'

// the real sphere, in kilos, meters and seconds - the numbers live in settings.js
const { mass, gravity, radius: sphereRadius, cubeInertia, wheelInertia, spinFriction, wheelMax } = settings.sphere

// navel 0 - the corner where the three wheels are, seen in the cube's own frame
const navelZero = unit(vector(-1, -1, -1))

class SphereSim {
  constructor() {
    this.orientation = noTurn()
    this.worldRate = vector(0, 0, 0)      // how fast the whole sphere turns, seen from the room
    this.wheelSpeeds = vector(0, 0, 0)
    this.wheelAngles = [0, 0, 0]          // how far each wheel has turned in all
    this.placeX = 0                       // where it has rolled to, in meters
    this.placeY = 0
    this.gravityOffset = vector(0, 0, 0)  // how far the center of gravity sits off the middle
  }

  // Left alone, the sphere comes to rest with its center of gravity straight down.
  rest = () => {
    const howFar = length(this.gravityOffset)
    const z = howFar > 1e-6 ? unit(scale(this.gravityOffset, -1)) : unit(vector(1, 1, 1))

    let x = subtract(vector(1, 0, 0), scale(z, z.x))
    // if x came out too short, the center of gravity lay along x - so the other side is used
    if (length(x) < 0.3) x = subtract(vector(0, 1, 0), scale(z, z.y))
    x = unit(x)

    const y = cross(z, x)
    this.orientation = [x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z]
    this.worldRate = vector(0, 0, 0)
  }

  // One step of the physics. torque is what the three wheels push with, in the cube's own frame.
  step = (torque, dt) => {
    const { orientation } = this

    // gravity pulls on the center of gravity, which sits a little off the middle - that is what tips it
    const offsetInWorld = rotate(orientation, this.gravityOffset)
    const fromGround = add(vector(0, 0, sphereRadius), offsetInWorld)
    const fromGravity = cross(fromGround, vector(0, 0, -mass * gravity))

    // the wheels push the cube one way, so they push the sphere the other way
    const fromWheels = scale(rotate(orientation, torque), -1)
    const total = add(add(fromGravity, fromWheels), vector(0, 0, -spinFriction * this.worldRate.z))

    // tipping over is harder than spinning on the spot: the weight is further from the floor
    const tipInertia = cubeInertia + mass * sphereRadius * sphereRadius + 3 * wheelInertia
    const spinInertia = cubeInertia + 3 * wheelInertia
    const speedingUp = vector(total.x / tipInertia, total.y / tipInertia, total.z / spinInertia)

    this.worldRate = add(this.worldRate, scale(speedingUp, dt))

    // the wheels gain what their motors give them, less what the whole sphere is already doing
    const speedingUpInBody = rotateBack(orientation, speedingUp)
    this.wheelSpeeds = add(this.wheelSpeeds, scale(subtract(scale(torque, 1 / wheelInertia), speedingUpInBody), dt))

    rotateInBody(orientation, rotateBack(orientation, this.worldRate), dt)
    this.#rollOnTheFloor(dt)
  }

  // A ball that turns this fast rolls its radius times that far.
  #rollOnTheFloor = dt => {
    this.placeX += sphereRadius * this.worldRate.y * dt
    this.placeY -= sphereRadius * this.worldRate.x * dt

    this.wheelAngles[0] += this.wheelSpeeds.x * dt
    this.wheelAngles[1] += this.wheelSpeeds.y * dt
    this.wheelAngles[2] += this.wheelSpeeds.z * dt
  }
}

// the one sphere of the simulation, its controller, and its measuring of the center of gravity
const sphereSim = new SphereSim()
const control = new Sphere()
const measure = new GravityMeasure(mass, wheelInertia)

export { sphereSim, control, measure, navelZero }
export { mass, gravity, sphereRadius, cubeInertia, wheelInertia, spinFriction, wheelMax }
