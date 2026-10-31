import * as THREE from '../lib/three.module.js'
import Orbit from '../tools/orbit.js'
import { meshPart, darkInside } from './cube-mesh.js'
import settings from './settings.js'

const sphereRadius = 130.05        // mm, the sphere the cube sits in
const { floorSize, gridStep, gridReach, gridPiece, trailMost, trailEvery, gravityDotTimes } = settings.view

// a color from the style file, so the view wears the same clothes as the page
const themeColor = (name, fallback) =>
  new THREE.Color(getComputedStyle(document.documentElement).getPropertyValue(name).trim() || fallback)

const material = (color, extra) =>
  new THREE.MeshStandardMaterial({ color, roughness: 0.62, metalness: 0.02, ...extra })

// a soft round shadow, painted once into a small picture
const shadowPicture = () => {
  const picture = document.createElement('canvas')
  picture.width = 128
  picture.height = 128

  const paint = picture.getContext('2d')
  const fade = paint.createRadialGradient(64, 64, 4, 64, 64, 64)
  fade.addColorStop(0, 'rgba(0,0,0,0.65)')
  fade.addColorStop(1, 'rgba(0,0,0,0)')
  paint.fillStyle = fade
  paint.fillRect(0, 0, 128, 128)
  return new THREE.CanvasTexture(picture)
}

class View {
  #watchSize = null

  constructor() {
    this.sphereRadius = sphereRadius
    this.canvas = null
    this.orbit = null

    this.sphereParts = []   // the shell, the navels and the arcs' bolts: hidden on the bare cube
    this.wheels = []
    this.trail = []         // where the sphere has been, in mm
  }

  // Everything is built once, on the page's canvas and from the mesh file. After this it only draws.
  start = (canvas, mesh) => {
    this.canvas = canvas
    this.orbit = new Orbit(canvas, settings.camera)

    const renderer = new THREE.WebGLRenderer({ canvas, antialias: true })
    renderer.setPixelRatio(Math.min(2, window.devicePixelRatio || 1))

    const scene = new THREE.Scene()
    const camera = new THREE.PerspectiveCamera(38, 1, 30, 12000)
    camera.up.set(0, 0, 1)

    this.renderer = renderer
    this.scene = scene
    this.camera = camera

    this.#addLights()
    this.#addFloor()
    this.#addGrid()
    this.#addBody(mesh)
    this.#addMarkers()
    this.#paintTheme()

    this.orbit.start()
    this.#watchSize = new ResizeObserver(this.#fitCanvas)
    this.#watchSize.observe(canvas)
    this.#fitCanvas()
  }

  // the view gives everything back: the listeners, the watcher, and the memory of the shapes
  clear = () => {
    this.orbit?.clear()
    this.#watchSize?.disconnect()
    this.#watchSize = null

    this.scene?.traverse(thing => {
      thing.geometry?.dispose()
      const paint = thing.material
      ;[paint].flat().forEach(one => one?.dispose())
    })

    this.renderer?.dispose()
    this.trail = []
    this.sphereParts = []
    this.wheels = []
  }

  #addLights = () => {
    const { scene } = this
    const sky = new THREE.HemisphereLight(0xdfefff, 0x1a2230, 0.75)
    const sun = new THREE.DirectionalLight(0xffffff, 0.9)
    const side = new THREE.DirectionalLight(0x9cc8ff, 0.35)

    sun.position.set(-600, -900, 1400)
    side.position.set(900, 700, 300)

    scene.add(sky)
    scene.add(sun)
    scene.add(side)
  }

  // the floor is drawn first and never hides the grid
  #addFloor = () => {
    const paint = new THREE.MeshBasicMaterial({ color: 0xffffff })
    const floor = new THREE.Mesh(new THREE.PlaneGeometry(floorSize, floorSize), paint)

    paint.depthWrite = false
    floor.position.z = -3
    floor.renderOrder = -1

    this.floorPaint = paint
    this.scene.add(floor)
  }

  // The 10 cm grid is drawn as short pieces of 20 cm, not as whole 8 m lines: one long line
  // passes behind the camera and some graphics drivers then drop the whole line - the grid went
  // missing right next to the cube.
  #addGrid = () => {
    const points = []

    for (let along = -gridReach; along <= gridReach; along += gridStep) {
      for (let across = -gridReach; across < gridReach; across += gridPiece) {
        points.push(along, across, 0, along, across + gridPiece, 0)
        points.push(across, along, 0, across + gridPiece, along, 0)
      }
    }

    const shape = new THREE.BufferGeometry()
    shape.setAttribute('position', new THREE.Float32BufferAttribute(points, 3))

    const grid = new THREE.LineSegments(shape, new THREE.LineBasicMaterial({ color: 0xffffff }))
    grid.position.z = 0.5

    this.grid = grid
    this.scene.add(grid)
  }

  #addBody = mesh => {
    const body = new THREE.Group()
    const paints = {
      shell: material(0xeef4f7),
      conn: material(0x3fae6e),
      bolts: material(0xa9b4bf, { metalness: 0.6, roughness: 0.4 }),
      cage: material(0xa9c1d3),
      box: material(0xdde3e8),
      motor: material(0x4a525c, { metalness: 0.45, roughness: 0.4 })
    }
    const onlyOnTheSphere = ['shell', 'conn', 'bolts']

    paints.box.vertexColors = true   // the box carries its own dark inside

    Object.entries(paints).forEach(([name, paint]) => {
      const shape = meshPart(mesh[name])
      const part = new THREE.Mesh(name === 'box' ? darkInside(shape) : shape, paint)

      body.add(part)
      if (onlyOnTheSphere.includes(name)) this.sphereParts.push(part)
    })

    // each wheel, with its screws, moved out to its own corner of the cube
    ;['x', 'y', 'z'].forEach(axis => {
      const corner = mesh._axes[axis]
      const wheel = new THREE.Group()

      wheel.add(new THREE.Mesh(meshPart(mesh['wheel_' + axis], corner), material(0xdde3e8)))
      if (mesh['wscrew_' + axis]) wheel.add(new THREE.Mesh(meshPart(mesh['wscrew_' + axis], corner), paints.bolts))

      wheel.position.set(corner[0], corner[1], corner[2])
      body.add(wheel)
      this.wheels.push(wheel)
    })

    this.body = body
    this.scene.add(body)
  }

  // the red dot of the balance point, the shadow under the cube, the front arrow,
  // the trail it leaves behind, and the route drawn on the floor
  #addMarkers = () => {
    const { scene, body } = this
    const gravityDot = new THREE.Mesh(new THREE.SphereGeometry(4, 12, 8), material(0xb3261e))

    const shadow = new THREE.Mesh(
      new THREE.PlaneGeometry(330, 330),
      new THREE.MeshBasicMaterial({ map: shadowPicture(), transparent: true, depthWrite: false })
    )
    shadow.position.z = 1

    const arrow = new THREE.ArrowHelper(new THREE.Vector3(1, 0, 0), new THREE.Vector3(0, 0, 2), 230, 0xb3261e, 40, 22)

    const trailShape = new THREE.BufferGeometry()
    const trailLine = new THREE.Line(trailShape, new THREE.LineBasicMaterial({ color: 0x2456b8 }))

    const routeShape = new THREE.BufferGeometry()
    const routeLine = new THREE.Line(routeShape, new THREE.LineBasicMaterial({ color: 0xb3261e }))
    routeLine.position.z = 2

    Object.assign(this, { gravityDot, shadow, arrow, trailShape, trailLine, routeShape, routeLine })

    body.add(gravityDot)
    scene.add(shadow)
    scene.add(arrow)
    scene.add(trailLine)
    scene.add(routeLine)
  }

  #paintTheme = () => {
    const floorColor = themeColor('--floor', '#070c14')

    this.scene.background = floorColor
    this.floorPaint.color = floorColor
    this.grid.material.color = themeColor('--grid', '#1b2735')
    this.trailLine.material.color = themeColor('--trail', '#cfe9f6')
  }

  #fitCanvas = () => {
    const { canvas, renderer, camera } = this
    const size = canvas.getBoundingClientRect()

    renderer.setSize(size.width, size.height, false)
    camera.aspect = size.width / Math.max(1, size.height)
    camera.updateProjectionMatrix()
  }

  // the point on the floor under the pointer, or null when the pointer is above the horizon
  floorAt = event => {
    const { canvas, camera } = this
    const size = canvas.getBoundingClientRect()
    const onScreen = new THREE.Vector2(
      ((event.clientX - size.left) / size.width) * 2 - 1,
      -((event.clientY - size.top) / size.height) * 2 + 1
    )

    const ray = new THREE.Raycaster()
    ray.setFromCamera(onScreen, camera)

    const spot = new THREE.Vector3()
    return ray.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0, 0, 1), 0), spot) ? spot : null
  }

  showRoute = points => {
    this.routeShape.setAttribute('position',
      new THREE.Float32BufferAttribute(points.flatMap(point => [point.x, point.y, 0]), 3))
    this.routeShape.computeBoundingSphere()
  }

  // the sphere, or the bare cube: what is shown
  showKind = kind => {
    const sphere = kind === 'sphere'

    this.sphereParts.forEach(part => {
      part.visible = sphere
    })

    this.arrow.visible = sphere
    this.trailLine.visible = sphere
    this.routeLine.visible = sphere
    this.shadow.scale.setScalar(sphere ? 1 : 0.85)
    this.orbit.distance = sphere ? settings.camera.sphereDistance : settings.camera.cubeDistance
  }

  // One picture: the body where it is now, the camera following it.
  // The places come in meters, the scene is in mm.
  draw = ({ kind, px, py, heading, lookZ, cg, skip }) => {
    const { body, gravityDot, shadow, arrow, camera, grid, orbit, renderer, scene } = this

    // the balance point is drawn further out than it is, or it could not be seen
    const times = 1000 * gravityDotTimes
    gravityDot.position.set(cg.x * times, cg.y * times, cg.z * times)

    if (kind === 'sphere') {
      shadow.position.set(px * 1000, py * 1000, 1)
      arrow.position.set(px * 1000, py * 1000, 2)
      arrow.setDirection(new THREE.Vector3(Math.cos(heading), Math.sin(heading), 0))
      this.#addToTrail(px * 1000, py * 1000)
    } else {
      shadow.position.set(body.position.x, body.position.y, 1)
    }

    const atX = kind === 'sphere' ? px * 1000 : body.position.x
    const atY = kind === 'sphere' ? py * 1000 : body.position.y
    const { turn, height, distance } = orbit

    camera.position.set(
      atX + distance * Math.cos(height) * Math.cos(turn),
      atY + distance * Math.cos(height) * Math.sin(turn),
      60 + distance * Math.sin(height)
    )
    camera.lookAt(atX, atY, lookZ)

    // the grid travels with the cube, in whole 10 cm steps, so it never seems to slide
    grid.position.x = Math.round(atX / gridStep) * gridStep
    grid.position.y = Math.round(atY / gridStep) * gridStep

    // a program is being sent to the cube: the computer's time goes to that, not to drawing
    if (!skip) renderer.render(scene, camera)
  }

  #addToTrail = (x, y) => {
    const { trail, trailShape } = this
    const last = trail.at(-1)   // .at(-1) - the last one

    if (last && Math.hypot(last[0] - x, last[1] - y) <= trailEvery) return

    trail.push([x, y])
    if (trail.length > trailMost) trail.shift()

    trailShape.setAttribute('position',
      new THREE.Float32BufferAttribute(trail.flatMap(point => [point[0], point[1], 1.5]), 3))
  }
}

// the one view of the app - code.js gives it the canvas and the mesh
const view = new View()

export default View
export { view }
