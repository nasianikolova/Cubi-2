import { vector, rotateBack, angleDegrees, degreesPerRadian } from './math.js'
import state from './state.js'
import { byId, centimeters } from './functions.js'
import { sphereSim, control } from './sphere-sim.js'
import { going, turning, settling } from './sphere-control.js'
import { cubeSim, balance } from './cube-sim.js'
import {
  showMode, showNavel, showEdgeLabels, showStartStop, showReading, showState,
  sphereRunning, cubeRunning
} from './marks.js'
import { wheelsCard } from './wheels-card.js'
import names from './names.js'

const radiansToRpm = 9.549
const nothing = '-'

// the real Cubi says a navel number above 10 when it is still on its way there
const onItsWay = 10

const toRpm = speeds => [speeds.x * radiansToRpm, speeds.y * radiansToRpm, speeds.z * radiansToRpm]

// ----- the real Cubi -------------------------------------------------------------------------

const showLive = () => {
  const reading = state.liveTelemetry ?? {}
  const onTheSphere = reading.sph

  if (state.kind === 'sphere') showLiveSphereReadings(reading, onTheSphere)
  else showLiveCubeReadings(reading)

  byId(names.volts).textContent = reading.v ? reading.v.toFixed(1) : nothing
  byId(names.charge).textContent = reading.bp != null ? reading.bp.toFixed(0) : nothing
  byId(names.cubeState).textContent = reading.st ? reading.st.toLowerCase() : nothing
  wheelsCard.draw(reading.w ?? [0, 0, 0])

  const lost = !state.liveOk || reading.st === 'STOPPED'
  const moving = reading.mv > 0 || (reading.st === 'BALANCING' && !onTheSphere)
  showState(liveWords(reading), lost ? 'bad' : moving ? 'moving' : '')

  showNavel(reading.mv === 3 ? reading.nv % onItsWay : -1)
  showMode(!onTheSphere && (reading.st === 'BALANCING' || reading.st === 'READY') ? reading.m : '')
  showEdgeLabels(!onTheSphere && reading.st === 'BALANCING' && reading.m === 'corner')
}

const showLiveSphereReadings = (reading, onTheSphere) => {
  showReading(0, 'x', onTheSphere ? reading.p[0].toFixed(1) : nothing, 'cm')
  showReading(1, 'y', onTheSphere ? reading.p[1].toFixed(1) : nothing, 'cm')
  showReading(2, 'heading', onTheSphere ? reading.h.toFixed(0) : nothing, 'deg')
}

const showLiveCubeReadings = reading => {
  showReading(0, 'tilt', reading.tilt != null ? reading.tilt.toFixed(2) : nothing, 'deg')
  showReading(1, 'mode', reading.m || nothing, '')
  showReading(2, 'gains', reading.k ? reading.k[0].toFixed(2) : nothing, 'angle')
}

const liveWords = reading => {
  if (!state.liveOk) return 'No link'
  if (reading.st === 'STOPPED') return 'Stopped'
  if (reading.st === 'OFF') return 'Off'

  if (reading.mv === 1) return 'Rolling'
  if (reading.mv === 2) return 'Turning'
  if (reading.mv === 3) {
    return reading.nv < onItsWay ? 'On navel ' + reading.nv : 'To navel ' + (reading.nv - onItsWay)
  }

  if (reading.sph) return 'Hold'
  if (reading.st === 'BALANCING') return 'Balancing - ' + reading.m
  if (reading.st === 'READY') return 'Ready - ' + reading.m
  return String(reading.m ?? '')
}

// ----- the cube in the simulation ---------------------------------------------------------------

const showCube = () => {
  const up = rotateBack(cubeSim.orientation, vector(0, 0, 1))
  const standing = cubeSim.state === 'on'
  const tilt = standing ? balance.tiltDegrees : angleDegrees(up, balance.balancePoint)

  // how far its balance point has wandered from where the shape says it should be
  const sameAsPhysics = (cubeSim.physicsMode || cubeSim.mode) === cubeSim.mode
  const pointOff = sameAsPhysics
    ? angleDegrees(balance.balancePoint, cubeSim.trueBalancePoint()).toFixed(2)
    : nothing

  showReading(0, 'tilt', tilt.toFixed(2), 'deg')
  showReading(1, 'mode', cubeSim.mode, '')
  showReading(2, 'point off', pointOff, 'deg')
  wheelsCard.draw(toRpm(cubeSim.wheelSpeeds))

  // the physics does not touch the screen any more, so the lit button is set from here
  showMode()
  showEdgeLabels(standing && cubeSim.mode === 'corner' && !cubeSim.lowering)
  showState(cubeWords(), standing ? 'moving' : cubeSim.state === 'held' ? '' : 'bad')
}

const cubeWords = () => {
  if (cubeSim.state === 'on') {
    if (cubeSim.lowering) return 'Lowering - ' + cubeSim.lowering.mode
    if (cubeSim.hand) return 'Lifted by hand'
    return 'Balancing - ' + cubeSim.mode
  }

  if (cubeSim.state === 'held') return 'Off - held'
  if (cubeSim.state === 'falling') return 'Falling'
  return cubeSim.fault ? 'Stopped' : 'Off'
}

// ----- the sphere in the simulation ---------------------------------------------------------------

const showSphere = () => {
  showReading(0, 'x', centimeters(control.placeX), 'cm')
  showReading(1, 'y', centimeters(control.placeY), 'cm')
  showReading(2, 'heading', (control.heading * degreesPerRadian).toFixed(0), 'deg')
  wheelsCard.draw(toRpm(sphereSim.wheelSpeeds))

  showState(sphereWords(), !state.running ? 'bad' : control.doing ? 'moving' : '')
  showNavel(state.running && control.doing === settling ? control.navel : -1)
}

const sphereWords = () => {
  if (!state.running) return state.fault ? 'Stopped' : 'Off'
  if (control.doing === going) return 'Rolling'
  if (control.doing === turning) return 'Turning'
  if (control.doing === settling) return (control.navelReached ? 'On navel ' : 'To navel ') + control.navel
  return 'Hold'
}

// ----- all of it ------------------------------------------------------------------------------

const showPanel = () => {
  showStartStop('sphere-go', sphereRunning(), 'go sphere')
  showStartStop('cube-go', cubeRunning(), 'go corner')

  if (state.liveOn) return showLive()
  if (state.kind === 'cube') return showCube()
  showSphere()
}

export { showPanel }
