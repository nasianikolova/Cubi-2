import { make } from '../tools/make.js'
import Drag from '../tools/drag.js'
import { byId, keyButton, sliderRow, showSettings, showTerminal, say } from './functions.js'
import names, { valueOf } from './names.js'
import settings from './settings.js'
import state from './state.js'
import { order } from './orders.js'
import { cubeSim, balance } from './cube-sim.js'

const { spin, twist, learnRate } = settings.balance

// most of these buttons send a command and open the Terminal, because the answer is written there
const sendAndShow = command => () => {
  order(command)
  showTerminal(true)
}

// Swing and Status only mean something on the real cube; in the simulation we answer ourselves
const swing = () => {
  if (state.liveOn) order('swing')
  else say('\'swing\' measures the real Cubi - use it in Live Cubi')
  showTerminal(true)
}

const status = () => {
  if (state.liveOn) order('status')
  else if (state.kind === 'cube') {
    say('status: ' + cubeSim.state + ', ' + cubeSim.mode + ', tilt ' + balance.tiltDegrees.toFixed(2) + ' deg')
  } else {
    say('status: sphere ' + (state.running ? 'on' : 'off'))
  }
  showTerminal(true)
}

class SettingsWindow {
  constructor() {
    this.element = document.createElement('section')
    this.drag = null

    this.parts = [
      {
        kind: 'cube',
        title: 'Balance point',
        note: 'Zero: held by hand exactly balanced. Fix: from the last run. Save: keep the points, gains and settings.',
        buttons: [
          { id: 'cube-zero', word: 'Zero', key: 'zero', act: sendAndShow('zero') },
          { id: 'cube-fix', word: 'Fix', key: 'fix', act: sendAndShow('fix') },
          { id: 'cube-save', word: 'Save', key: 'save', act: sendAndShow('save') }
        ]
      },
      {
        kind: 'cube',
        title: 'Controller',
        sliders: [
          {
            id: 'slider-spin', label: 'Spin', min: 0, max: 1, step: 0.05, value: spin,
            out: spin.toFixed(2), show: value => Number(value).toFixed(2),
            command: value => 'spin ' + Number(value).toFixed(2)
          },
          {
            id: 'slider-twist', label: 'Twist', min: 0, max: 1, step: 0.05, value: twist,
            out: twist.toFixed(2), show: value => Number(value).toFixed(2),
            command: value => 'twist ' + Number(value).toFixed(2)
          },
          {
            id: 'slider-learn', label: 'Learn', min: 0, max: 0.5, step: 0.01, value: learnRate,
            out: learnRate.toFixed(2) + ' /s', show: value => Number(value).toFixed(2) + ' /s',
            command: value => 'learn ' + Number(value).toFixed(2)
          }
        ]
      },
      {
        kind: 'sphere',
        title: 'Center of gravity',
        note: 'Where and Swing (the real Cubi, wheels off): its direction, and roughly how far. ' +
          'CG (Start first): held on each of the 8 navels, the wheels measure it to a tenth of a mm, ' +
          'and which way it points.',
        buttons: [
          { id: 'sphere-where', word: 'Where', key: 'where', act: sendAndShow('where') },
          { id: 'sphere-swing', word: 'Swing', key: 'swing', act: swing },
          { id: 'sphere-measure', word: 'CG', key: 'cg', act: sendAndShow('cg') }
        ],
        weight: true
      },
      {
        kind: '',
        title: 'Information',
        buttons: [
          { id: 'cube-show', word: 'Show', key: 'show', act: sendAndShow('show') },
          { id: 'cube-status', word: 'Status', key: 'status', act: status },
          { id: 'cube-help', word: 'Help', key: 'help', act: sendAndShow('help') }
        ]
      }
    ]
  }

  _render = () => {
    const { element, parts } = this
    const bar = make('div', { class: 'termbar' })
    const close = make('button', { class: 'x', 'aria-label': 'Close' }, '×')

    element.setAttribute('class', 'term setw')
    element.setAttribute('id', names.settings)
    element.setAttribute('role', 'dialog')
    element.setAttribute('aria-labelledby', 'label-settings')
    element.hidden = true

    close.onclick = () => showSettings(false)
    bar.appendChild(make('span', { class: 'label', id: 'label-settings' }, 'Settings'))
    bar.appendChild(close)
    element.appendChild(bar)

    parts.forEach(part => element.appendChild(this.#part(part)))

    this.drag = new Drag(element, bar)
    this.drag.start()
  }

  clear = () => {
    this.drag?.clear()
    this.drag = null
  }

  #part = ({ kind, title, note, buttons, sliders, weight }) => {
    const box = make('div', { class: 'part', 'data-kind': kind || undefined })
    if (kind === 'cube') box.hidden = true

    box.appendChild(make('h3', {}, title))
    if (note) box.appendChild(make('p', { class: 'note' }, note))
    if (buttons) box.appendChild(this.#buttons(buttons))
    if (sliders) box.appendChild(this.#sliders(sliders))
    if (weight) this.#weightRow().forEach(one => box.appendChild(one))

    return box
  }

  #buttons = buttons => {
    const pad = make('div', { class: 'pad' })

    buttons.forEach(({ id, word, key, act }) => {
      const button = keyButton(id, word, key)
      button.onclick = act
      pad.appendChild(button)
    })

    return pad
  }

  #sliders = sliders => {
    const rows = make('div', { class: 'rows' })

    sliders.forEach(one => {
      const { id, show, command } = one
      const row = sliderRow(one)
      const slider = row.querySelector('input')

      slider.oninput = event => {
        byId(valueOf(id)).textContent = show(event.target.value)
      }
      slider.onchange = event => order(command(event.target.value))

      rows.appendChild(row)
    })

    return rows
  }

  // How far the center of gravity is: a test weight taped at a navel on the side
  #weightRow = () => {
    const note = make('p', { class: 'note weight-note' },
      'How far: after Where, tape a test weight on the outside at a navel at the side, let it settle, then:')
    const row = make('div', { class: 'row2' })
    const grams = make('input', { id: 'weight-grams', type: 'number', min: 1, max: 500, value: 10, 'aria-label': 'grams' })
    const navel = make('select', { id: 'weight-navel', 'aria-label': 'navel' })
    const button = make('button', { id: 'weight-where' }, 'Where + weight')

    ;[0, 1, 2, 3, 4, 5, 6, 7].forEach(which => {
      const choice = make('option', {}, String(which))
      if (which === 4) choice.selected = true   // one at the side, where a weight tells us most
      navel.appendChild(choice)
    })

    button.onclick = () => {
      order('where ' + (Number(grams.value) || 10) + ' ' + navel.value)
      showTerminal(true)
    }

    row.appendChild(grams)
    row.appendChild(make('span', { class: 'note' }, 'g at navel'))
    row.appendChild(navel)
    row.appendChild(button)

    return [note, row]
  }
}

export default SettingsWindow
