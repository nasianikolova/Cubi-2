import { make } from '../tools/make.js'
import { byId, keyButton, makeCard } from './functions.js'
import state from './state.js'
import { order } from './orders.js'
import { goSphere } from './commands.js'
import { sphereRunning } from './marks.js'
import { route } from './route.js'
import names from './names.js'

const sliderValue = id => Number(byId(id).value)

// Pressing a move button while the sphere is off would do nothing and say nothing,
// so in the simulation it is started first. The real cube is not started behind the user's back.
const makeSureItIsRunning = () => {
  if (!state.liveOn && !state.running) goSphere()
}

const moveBy = (what, slider) => {
  makeSureItIsRunning()
  order(what + ' ' + sliderValue(slider))
}

class DriveCard {
  constructor() {
    this.element = document.createElement('section')

    // three by three, the way the keys sit under a hand. The empty ones keep the shape.
    this.pad = [
      { id: names.sphereFront, word: 'Front', key: 'front', act: () => order('front') },
      { id: names.sphereForward, word: 'Forward', key: '↑ fwd 30', act: () => moveBy('fwd', names.distanceSlider) },
      { id: names.sphereHalt, word: 'Halt', key: 'space', act: () => order('halt') },

      { id: names.sphereLeft, word: 'Left', key: '← left 90', act: () => moveBy('left', names.angleSlider) },
      { id: names.sphereGo, word: 'Start', key: 'go sphere', act: () => order(sphereRunning() ? 'stop' : 'go sphere') },
      { id: names.sphereRight, word: 'Right', key: '→ right 90', act: () => moveBy('right', names.angleSlider) },

      { blank: true },
      { id: names.sphereBack, word: 'Back', key: '↓ back 30', act: () => moveBy('back', names.distanceSlider) },
      { blank: true }
    ]

    this.routeButtons = [
      { id: names.routeDraw, text: 'Draw a route', pressed: true, act: route.toggleDraw },
      { id: names.routeGo, text: 'Go', asleep: true, act: route.go },
      { id: names.routeClear, text: 'Clear', asleep: true, act: route.clear }
    ]
  }

  _render = () => {
    const { element, pad, routeButtons } = this
    const padBox = make('div', { class: 'pad' })
    const routeBar = make('div', { class: 'routebar' })

    makeCard(element, 'label-drive', 'Drive', 'sphere')

    pad.forEach(one => {
      if (one.blank) return padBox.appendChild(make('span'))

      const { id, word, key, act } = one
      const button = keyButton(id, word, key)

      button.onclick = act
      padBox.appendChild(button)
    })

    routeButtons.forEach(one => {
      const { id, text, pressed, asleep, act } = one
      const button = make('button', { id, 'aria-pressed': pressed ? 'false' : undefined }, text)

      button.disabled = !!asleep
      button.onclick = act
      routeBar.appendChild(button)
    })

    routeBar.appendChild(make('span', { id: names.routeInfo }, 'draw on the floor with the mouse or a finger'))
    element.appendChild(padBox)
    element.appendChild(routeBar)
  }
}

// the small line under each move button follows the Distance and Turn sliders
const showMoveLabels = () => {
  const far = sliderValue(names.distanceSlider)
  const round = sliderValue(names.angleSlider)

  byId(names.sphereForward).querySelector('.k').textContent = '↑ fwd ' + far
  byId(names.sphereBack).querySelector('.k').textContent = '↓ back ' + far
  byId(names.sphereLeft).querySelector('.k').textContent = '← left ' + round
  byId(names.sphereRight).querySelector('.k').textContent = '→ right ' + round
}

export default DriveCard
export { showMoveLabels, makeSureItIsRunning }
