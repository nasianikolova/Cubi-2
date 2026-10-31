// orbit - the camera around the cube: drag to go round it, the wheel to come closer.
// It keeps where the camera is for as long as the app is open, so it is a class.
// One of the two places addEventListener lives, because a drag needs several listeners together.
//
//   const orbit = new Orbit(canvas)
//   orbit.start()
//   ... orbit.turn, orbit.height, orbit.distance   while drawing each frame
//   orbit.clear()                                  takes every listener off again

// tools bring nothing in of their own, so where the camera starts and how far it may go
// is handed to it. js/settings.js holds those numbers.
const asFarAs = {
  turn: -2.2,
  height: 0.5,
  distance: 850,
  lowest: 0.08,          // the camera never goes under the floor
  highest: 1.45,         // and never straight overhead
  nearest: 400,          // in mm
  farthest: 4000,
  zoomStep: 1.1,
  turnPerPixel: 0.008,
  heightPerPixel: 0.006
}

class Orbit {
  // # means it belongs to this object alone - nothing outside can reach it
  #from = null            // where the drag began, while a drag is on
  #stop = new AbortController()

  constructor(canvas, limits) {
    this.canvas = canvas
    this.limits = { ...asFarAs, ...limits }

    this.turn = this.limits.turn          // how far round the cube the camera has gone, in radians
    this.height = this.limits.height      // how high it looks from (0 is floor level)
    this.distance = this.limits.distance  // how far away it is, in mm

    // route.js puts its three handlers here while a route is being drawn on the floor.
    // Each one answers true when it has taken the pointer, and then the camera leaves it alone.
    this.floor = null
  }

  start = () => {
    const { canvas } = this
    const signal = this.#stop.signal   // one signal for all of them, so one line takes them all off

    canvas.addEventListener('pointerdown', this.#take, { signal })
    canvas.addEventListener('pointermove', this.#move, { signal })
    canvas.addEventListener('pointerup', this.#let, { signal })
    canvas.addEventListener('pointercancel', this.#let, { signal })
    canvas.addEventListener('wheel', this.#zoom, { signal, passive: false })
  }

  // every listener goes away at once
  clear = () => {
    this.#stop.abort()
    this.#from = null
    this.#stop = new AbortController()
  }

  #take = event => {
    const { canvas, floor } = this
    canvas.setPointerCapture(event.pointerId)

    // ?. - the floor handlers are asked only if route.js has put them there
    if (floor?.down(event)) return

    this.#from = { x: event.clientX, y: event.clientY, turn: this.turn, height: this.height }
  }

  #move = event => {
    const { floor } = this
    if (floor?.move(event)) return

    const from = this.#from
    if (!from) return

    const { lowest, highest, turnPerPixel, heightPerPixel } = this.limits

    this.turn = from.turn - (event.clientX - from.x) * turnPerPixel
    this.height = Math.max(lowest, Math.min(highest, from.height + (event.clientY - from.y) * heightPerPixel))
  }

  #let = event => {
    this.floor?.up(event)
    this.#from = null
  }

  #zoom = event => {
    event.preventDefault()   // the page itself must not scroll while we zoom
    const { nearest, farthest, zoomStep } = this.limits
    const closer = event.deltaY > 0 ? zoomStep : 1 / zoomStep

    this.distance = Math.max(nearest, Math.min(farthest, this.distance * closer))
  }
}

export default Orbit
