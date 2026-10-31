import { make } from '../tools/make.js'
import { keyButton, makeCard, say } from './functions.js'
import state from './state.js'
import { order } from './orders.js'
import { command } from './commands.js'
import { cubeRunning } from './marks.js'
import names from './names.js'

// Lift and Push are the hand of the simulation. The real cube has a real hand - yours.
const liftIt = () => {
  if (!state.liveOn) return command('lift')
  say('lift: on an edge, lift it by hand onto corner 0 - the corner takes over within the take-over angle')
}

const pushIt = () => {
  if (!state.liveOn) return command('push')
  say('push: give the real cube a gentle push with a finger')
}

class BalanceCard {
  constructor() {
    this.element = document.createElement('section')

    this.pad = [
      { id: names.cubeEdge('x'), mode: 'edge x', word: 'Edge X', key: 'go edge x', title: 'on the corner: lowers it onto edge x' },
      { id: names.cubeEdge('y'), mode: 'edge y', word: 'Edge Y', key: 'go edge y', title: 'on the corner: lowers it onto edge y' },
      { id: names.cubeEdge('z'), mode: 'edge z', word: 'Edge Z', key: 'go edge z', title: 'on the corner: lowers it onto edge z' },

      { id: names.cubeLift, word: 'Lift', key: 'by hand', act: liftIt },
      { id: names.cubeGo, word: 'Start', key: 'go corner', act: () => order(cubeRunning() ? 'stop' : 'go corner') },
      { id: names.cubePush, word: 'Push', key: 'push', act: pushIt }
    ]
  }

  _render = () => {
    const { element, pad } = this
    const padBox = make('div', { class: 'pad', id: names.cubePad })

    makeCard(element, 'label-balance', 'Balance', 'cube')

    pad.forEach(one => {
      const { id, mode, word, key, title, act } = one
      const button = keyButton(id, word, key, title)

      // the three edge buttons carry the way of standing they ask for, so marks.js can light them
      if (mode) button.setAttribute('data-mode', mode)
      button.onclick = mode ? () => order('go ' + mode) : act

      padBox.appendChild(button)
    })

    element.appendChild(padBox)
  }
}

export default BalanceCard
