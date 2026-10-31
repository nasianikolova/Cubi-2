import { make } from '../tools/make.js'
import { byId, showTerminal, showSettings, showRuns } from './functions.js'
import { setKind } from './modes.js'
import { setMode } from './live-link.js'
import state from './state.js'
import names from './names.js'

class TopBar {
  constructor() {
    this.element = document.createElement('div')

    this.groups = [
      {
        label: 'Sphere or cube',
        id: '',
        buttons: [
          { id: names.sphereButton, text: 'Sphere', pressed: true, act: () => state.kind !== 'sphere' && setKind('sphere') },
          { id: names.cubeButton, text: 'Cube', pressed: false, act: () => state.kind !== 'cube' && setKind('cube') }
        ]
      },
      {
        label: 'Simulation or the real Cubi',
        id: names.modeSwitch,
        buttons: [
          { id: names.simulationButton, text: 'Simulation', pressed: true, act: () => setMode(false) },
          { id: names.liveButton, text: 'Live Cubi', pressed: false, act: () => setMode(true) }
        ]
      }
    ]

    // the windows: each button opens its own window and closes it again
    this.windowButtons = [
      { id: names.runsButton, text: 'Runs', controls: 'runs', act: () => showRuns(byId(names.runs).hidden) },
      { id: names.settingsButton, text: 'Settings', controls: 'settings', act: () => showSettings(byId(names.settings).hidden) },
      { id: names.terminalButton, text: 'Terminal', controls: 'terminal', act: () => showTerminal(byId(names.terminal).hidden) }
    ]
  }

  _render = () => {
    const { element, groups, windowButtons } = this
    const title = make('h1', {}, 'Cubi2')
    const subtitle = make('span', { class: 'sub', id: names.subtitle })
    const barGroup = make('div', { class: 'bargroup' })

    element.setAttribute('class', 'topbar')

    groups.forEach(group => {
      const { label, id, buttons } = group
      // role group with a name, so a reader says what the two buttons are for
      const box = make('div', { class: 'modes', role: 'group', 'aria-label': label, id: id || undefined })

      buttons.forEach(item => {
        const { id, text, pressed, act } = item
        const button = make('button', { id, 'aria-pressed': String(pressed) }, text)
        button.onclick = act
        box.appendChild(button)
      })

      barGroup.appendChild(box)
    })

    windowButtons.forEach(item => {
      const { id, text, controls, act } = item
      const button = make('button', { class: 'tbtn', id, 'aria-pressed': 'false', 'aria-controls': controls }, text)
      button.onclick = act
      barGroup.appendChild(button)
    })

    element.appendChild(title)
    element.appendChild(subtitle)
    element.appendChild(barGroup)
  }
}

export default TopBar
