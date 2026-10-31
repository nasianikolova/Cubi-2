import { byId } from './functions.js'
import state from './state.js'
import { cubeSim } from './cube-sim.js'
import names from './names.js'

const spaceThatHolds = ' '   // a space that does not let the line collapse

// The Balance buttons: the way it is standing is the one lit.
// Called with nothing, it asks the cube itself what it is doing.
const showMode = mode => {
  const now = mode ?? (cubeSim.state === 'on' ? cubeSim.mode : '')

  document.querySelectorAll('#' + names.cubePad + ' button[data-mode]').forEach(button => {
    button.setAttribute('aria-pressed', String(button.dataset.mode === now))
  })
}

// the navel the sphere rests on is lit
const showNavel = which => {
  document.querySelectorAll('#' + names.navels + ' button').forEach(button => {
    button.setAttribute('aria-pressed', String(Number(button.dataset.navel) === which))
  })
}

// While it balances on the corner, the Edge buttons LOWER it onto that edge - their small line says so.
const showEdgeLabels = onCorner => {
  ;['x', 'y', 'z'].forEach(axis => {
    const small = byId(names.cubeEdge(axis)).querySelector('.k')
    const says = onCorner ? '↓ lower ' + axis : 'go edge ' + axis

    if (small.textContent !== says) small.textContent = says
  })
}

// The middle button: Start when nothing runs, and the same button is Stop while it runs.
const showStartStop = (id, running, startCommand) => {
  const button = byId(id)
  const word = running ? 'Stop' : 'Start'
  const command = running ? 'stop' : startCommand

  if (button.firstChild.textContent !== word) {
    button.firstChild.textContent = word
    button.querySelector('.k').textContent = command
    button.setAttribute('aria-label', word)
  }

  // lit while it runs, dark like the others when it is stopped
  button.classList.toggle('primary', running)
}

// one of the three readings: its name, its number, its unit
const showReading = (which, label, value, unit) => {
  byId(names.readingLabel(which)).textContent = label
  byId(names.readingValue(which)).textContent = value
  byId(names.readingUnit(which)).textContent = unit || spaceThatHolds
}

// the pill over the 3D view: what the cube is doing right now
const showState = (text, kind) => {
  const pill = byId(names.statePill)

  pill.className = 'pill state' + (kind ? ' ' + kind : '')
  pill.textContent = text
}

const showLink = (text, kind) => {
  const line = byId(names.linkState)

  line.textContent = text
  line.className = 'linkstate' + (kind ? ' ' + kind : '')
}

// Is it really running? In Live that is what the real cube says; in the simulation, ours.
const sphereRunning = () => {
  const reading = state.liveTelemetry

  if (!state.liveOn) return state.running
  return !!(reading && reading.sph && reading.st === 'BALANCING')
}

const cubeRunning = () => {
  const reading = state.liveTelemetry

  if (!state.liveOn) return cubeSim.state === 'on'
  return !!(reading && !reading.sph && (reading.st === 'BALANCING' || reading.st === 'READY'))
}

export { showMode, showNavel, showEdgeLabels, showStartStop, showReading, showState, showLink, sphereRunning, cubeRunning }
