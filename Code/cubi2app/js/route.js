import { byId, say } from './functions.js'
import state from './state.js'
import settings from './settings.js'
import { control } from './sphere-sim.js'
import { stopped } from './sphere-control.js'
import { sphereRunning } from './marks.js'
import { view } from './view3d.js'
import { order } from './orders.js'
import names from './names.js'

const { straightenWithin, shortestPart, newPointEvery, smallestTurnDegrees, longestPart, waitBetween } = settings.route
const degreesPerRadian = 57.29578
const hint = 'draw on the floor with the mouse or a finger'

const howFar = (one, other) => Math.hypot(one.x - other.x, one.y - other.y)

// Douglas-Peucker: keep the corners of the line, drop the shaking of the hand.
// It finds the point furthest from the straight line between the two ends. If even that one is
// close enough, the whole piece becomes that straight line. If not, it splits there and tries again.
const straighten = (points, within) => {
  if (points.length < 3) return [...points]

  const first = points[0]
  const last = points.at(-1)
  const span = howFar(first, last) || 1
  let furthest = 0
  let atPoint = 0

  for (let i = 1; i < points.length - 1; i++) {
    const away = Math.abs(
      (last.y - first.y) * points[i].x - (last.x - first.x) * points[i].y +
      last.x * first.y - last.y * first.x
    ) / span

    if (away > furthest) {
      furthest = away
      atPoint = i
    }
  }

  if (furthest < within) return [first, last]

  return straighten(points.slice(0, atPoint + 1), within).slice(0, -1)
    .concat(straighten(points.slice(atPoint), within))
}

class Route {
  constructor() {
    this.penOn = false       // the Draw button is pressed
    this.penDown = false     // the finger is down on the floor right now
    this.running = false     // the commands are being carried out

    this.points = []
    this.steps = []
    this.totalCm = 0
    this.lastSent = 0
  }

  // ----- drawing on the floor ------------------------------------------------------------------
  // These three answer true when the route has taken the pointer, and then the camera leaves it alone.

  down = event => {
    if (!this.penOn || state.kind !== 'sphere') return false

    const spot = view.floorAt(event)
    if (spot) {
      this.penDown = true
      this.points = [this.#whereTheSphereIs(), spot]
      view.showRoute(this.points)
    }

    return true
  }

  move = event => {
    if (!this.penDown) return false

    const spot = view.floorAt(event)
    if (spot && howFar(spot, this.points.at(-1)) > newPointEvery) {
      this.points.push(spot)
      view.showRoute(this.points)
    }

    return true
  }

  up = () => {
    if (!this.penDown) return
    this.penDown = false
    this.plan()
  }

  // ----- from a line to commands -----------------------------------------------------------------

  plan = () => {
    const corners = this.#corners()
    let pointing = this.#whereItPoints()

    this.steps = []
    this.totalCm = 0

    for (let i = 1; i < corners.length; i++) {
      const alongX = corners[i].x - corners[i - 1].x
      const alongY = corners[i].y - corners[i - 1].y
      const goes = Math.atan2(alongY, alongX)

      this.#addTurn(goes, pointing)
      this.#addStraight(alongX, alongY)
      pointing = goes
    }

    this.points = corners
    view.showRoute(this.points)
    this.#showThePlan(corners.length - 1)
  }

  // the corners of the drawn line, with the very short parts joined to the next one
  #corners = () => straighten(this.points, straightenWithin)
    .filter((point, i, all) => i === 0 || howFar(point, all[i - 1]) >= shortestPart)

  #addTurn = (goes, pointing) => {
    let turn = (goes - pointing) * degreesPerRadian

    // the same direction can be written as +350 or -10 degrees: always take the short way round.
    // at exactly 180 either way is as short as the other, and it may come out as left or right.
    while (turn > 180) turn -= 360
    while (turn < -180) turn += 360

    if (Math.abs(turn) < smallestTurnDegrees) return
    this.steps.push(turn > 0 ? 'left ' + Math.round(turn) : 'right ' + Math.round(-turn))
  }

  #addStraight = (alongX, alongY) => {
    const centimeters = Math.min(longestPart, Math.round(Math.hypot(alongX, alongY) / 10))

    this.steps.push('fwd ' + centimeters)
    this.totalCm += centimeters
  }

  // ----- carrying them out --------------------------------------------------------------------------

  // every frame: the next command, once the sphere has finished the last one
  tick = now => {
    if (!this.running) return

    if (!this.#sphereStillGoing()) {
      this.running = false
      this.steps = []
      byId(names.routeInfo).textContent = 'route stopped - the sphere is not running'
      return
    }

    if (now - this.lastSent < waitBetween || !this.#sphereIsStill()) return

    const next = this.steps.shift()
    if (!next) return this.#allDone()

    order(next)
    this.lastSent = now
    byId(names.routeInfo).textContent = next + ' (' + this.steps.length + ' left)'
  }

  go = () => {
    if (this.running) return this.#stopHalfWay()
    if (!sphereRunning()) return say('first Start (go sphere), then Go', true)

    this.running = true
    this.penOn = false
    byId(names.routeDraw).setAttribute('aria-pressed', 'false')
    byId(names.routeGo).textContent = 'Stop route'
    this.lastSent = 0
  }

  toggleDraw = () => {
    this.penOn = !this.penOn
    byId(names.routeDraw).setAttribute('aria-pressed', String(this.penOn))
    byId(names.routeInfo).textContent = this.penOn ? 'draw on the floor - from the sphere onwards' : hint
  }

  clear = () => {
    this.running = false
    this.steps = []
    this.points = []
    view.showRoute(this.points)

    byId(names.routeGo).disabled = true
    byId(names.routeClear).disabled = true
    byId(names.routeGo).textContent = 'Go'
    byId(names.routeInfo).textContent = hint
  }

  // ----- the small inside helpers ---------------------------------------------------------------------

  // where the sphere is now, in mm - the real one when we are live, ours when we are not
  #whereTheSphereIs = () => {
    if (!state.liveOn) return { x: control.placeX * 1000, y: control.placeY * 1000, z: 0 }

    const place = state.liveTelemetry?.p ?? [0, 0]
    return { x: place[0] * 10, y: place[1] * 10, z: 0 }   // the cube says cm, the scene is in mm
  }

  #whereItPoints = () => {
    if (!state.liveOn) return control.heading
    return (state.liveTelemetry?.h ?? 0) / degreesPerRadian
  }

  // the sphere has nothing left to do, so the next command can go
  #sphereIsStill = () => {
    const reading = state.liveTelemetry

    if (!state.liveOn) return state.running && control.doing === stopped
    return !!(reading && reading.sph && reading.st === 'BALANCING' && reading.mv === 0)
  }

  // it is still standing and able to carry on at all
  #sphereStillGoing = () => {
    const reading = state.liveTelemetry

    if (!state.liveOn) return state.running
    return !!(reading && reading.sph && reading.st === 'BALANCING')
  }

  #allDone = () => {
    this.running = false
    byId(names.routeInfo).textContent = 'route done'
    byId(names.routeGo).textContent = 'Go'
    byId(names.routeGo).disabled = true
  }

  #stopHalfWay = () => {
    this.running = false
    this.steps = []
    order('halt')

    byId(names.routeGo).textContent = 'Go'
    byId(names.routeInfo).textContent = 'route stopped'
  }

  #showThePlan = parts => {
    byId(names.routeGo).disabled = !this.steps.length
    byId(names.routeClear).disabled = !this.points.length
    byId(names.routeInfo).textContent = this.steps.length
      ? parts + ' straight parts, ' + this.totalCm + ' cm - Go'
      : 'too short - draw a longer line'
  }
}

const route = new Route()

export { route, straighten }
