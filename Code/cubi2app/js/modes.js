import { vector } from './math.js'
import state from './state.js'
import { byId, say } from './functions.js'
import { view } from './view3d.js'
import { cubeSim } from './cube-sim.js'
import { goSphere } from './commands.js'
import { wheelsCard } from './wheels-card.js'
import names from './names.js'

const placeholders = {
  sphere: 'fwd 30 · left 90 · help',
  cube: 'go corner · push · show · help'
}

const legends = {
  sphere: 'grid 10 cm · red arrow = front · drag to orbit, wheel to zoom',
  cube: 'grid 10 cm · red dot = center of gravity (6x further out) · drag to orbit, wheel to zoom'
}

// quiet: the mode changes because the real cube changed, so nothing is started here
const setKind = (kind, quiet) => {
  state.kind = kind

  showWhichOne(kind)
  view.showKind(kind)
  wheelsCard.clear()
  state.lastTorque = vector(0, 0, 0)

  if (state.liveOn) return sayWhatTheCubeWillDo(kind)
  if (kind === 'sphere') return startTheSphere(quiet)
  startTheCube(quiet)
}

// the two buttons, the cards of each mode, the words under the terminal and under the view
const showWhichOne = kind => {
  byId(names.sphereButton).setAttribute('aria-pressed', String(kind === 'sphere'))
  byId(names.cubeButton).setAttribute('aria-pressed', String(kind === 'cube'))

  document.querySelectorAll('[data-kind]').forEach(part => {
    part.hidden = part.dataset.kind !== kind
  })

  byId(names.commandInput).placeholder = placeholders[kind]
  byId(names.legend).textContent = legends[kind]
}

// in Live nothing is started here - the real cube is the one doing things
const sayWhatTheCubeWillDo = kind => {
  if (kind === 'sphere') return say('SPHERE - \'go sphere\' starts it', true)
  say('CUBE - Corner or an Edge starts it', true)
}

const startTheSphere = quiet => {
  if (!quiet) goSphere()
}

const startTheCube = quiet => {
  state.running = false
  cubeSim.held()
  cubeSim.said = []

  if (quiet) return
  say('CUBE - held on corner 0. Corner or an Edge: it balances (the firmware\'s balance.h).', true)
}

export { setKind }
