import state from './state.js'
import settings from './settings.js'
import { byId, byIdIfThere, say } from './functions.js'
import { showLink } from './marks.js'
import { askCube, sendCommand, tidyAddress } from './cube-link.js'
import { setKind } from './modes.js'
import { goSphere } from './commands.js'
import { cubeSim } from './cube-sim.js'
import { view } from './view3d.js'
import { wheelsCard } from './wheels-card.js'
import { runRecord } from './run-record.js'
import names from './names.js'

const { askEvery, askLaterWhileUpdating, freshEnough } = settings.link

// parts that may not be on the page yet - the cube's question is still to come (piece 2)
const hideIfThere = (id, hidden) => {
  const part = byIdIfThere(id)
  if (part) part.hidden = hidden
}

// ----- asking the cube, over and over ------------------------------------------------------

const keepAsking = async () => {
  if (!state.liveOn) {
    state.livePolling = false
    return
  }

  state.livePolling = true

  // a program is being sent to the cube: it has better things to do than answer us
  if (state.updating) {
    setTimeout(keepAsking, askLaterWhileUpdating)
    return
  }

  try {
    await askOnce()
  } catch {
    state.liveOk = false
    showLink('no connection - are you on the Cubi2 Wi-Fi? (' + state.liveAddress + ')', 'bad')
  }

  setTimeout(keepAsking, askEvery)
}

const askOnce = async () => {
  const said = await askCube(state.liveAddress, state.liveSequence)

  // the Wi-Fi board restarted and is counting from the beginning again
  if (said.sequence < state.liveSequence) state.liveSequence = 0

  // a line of the cube's recording is kept for the run, not shown; every other line is shown
  said.log.forEach(line => {
    if (!runRecord.takeLine(line)) say(line)
  })
  state.liveSequence = said.sequence
  state.liveTelemetry = said.telemetry
  state.liveAge = said.age

  const fresh = !!(said.telemetry && said.telemetry.st && said.age < freshEnough)
  state.liveOk = fresh

  showLink(fresh
    ? 'connected - data ' + said.age + ' ms old'
    : 'the Wi-Fi board answers, but no data from the cube (Teensy on?)', fresh ? 'ok' : 'bad')

  if (!fresh) return
  followTheCube(said.telemetry)
  runRecord.watch(said.telemetry)
}

// the app shows whatever the real cube is doing
const followTheCube = reading => {
  const onTheSphere = !!reading.sph

  if (onTheSphere && state.kind !== 'sphere') return setKind('sphere', true)
  if (!onTheSphere && reading.st === 'BALANCING' && state.kind !== 'cube') setKind('cube', true)
}

// one command to the real cube. The Wi-Fi board writes '> command' in its own log.
const sendLive = async command => {
  try {
    await sendCommand(state.liveAddress, command)
  } catch {
    say('not sent - no connection to the cube')
  }
}

// ----- Simulation or Live Cubi ---------------------------------------------------------------

const setMode = live => {
  state.liveOn = live

  showWhichMode(live)
  view.trail.length = 0
  wheelsCard.clear()
  byId(names.log).textContent = ''

  if (live) return goLive()
  goSimulation()
}

const showWhichMode = live => {
  byId(names.simulationButton).setAttribute('aria-pressed', String(!live))
  byId(names.liveButton).setAttribute('aria-pressed', String(live))

  hideIfThere(names.askBar, true)        // the cube's question belongs to Live, and starts hidden
  hideIfThere(names.linkCard, !live)     // the Cubi card is only of use in Live
  hideIfThere(names.batteryRow, !live)   // the simulation has no battery

  // in Live the center of gravity is the real one - it is not ours to move
  ;[names.sphereGravitySlider, names.cubeGravitySlider].forEach(id => {
    byId(id).disabled = live
  })

  view.gravityDot.visible = !live
}

const goLive = () => {
  say('LIVE - the commands go to the real Cubi', true)
  state.liveSequence = 0

  if (!state.livePolling) keepAsking()
}

const goSimulation = () => {
  say('SIMULATION', true)

  if (state.kind === 'sphere') return goSphere()
  cubeSim.held()
  cubeSim.said = []
}

// the address typed into the Cubi card
const connect = address => {
  const clean = tidyAddress(address)

  // the same cube again: its log is not fetched from the beginning twice
  if (clean !== state.liveAddress) {
    state.liveAddress = clean
    state.liveSequence = 0
    byId(names.log).textContent = ''
  }

  showLink('connecting to ' + state.liveAddress + ' ...')
  if (!state.livePolling) keepAsking()
}

export { keepAsking, sendLive, setMode, connect }
