import { make } from '../tools/make.js'
import Drag from '../tools/drag.js'
import settings from './settings.js'
import state from './state.js'
import names from './names.js'
import { showUpdate, say } from './functions.js'
import { readUpdate, whatIsInside, programsIn } from './update-file.js'
import { sendBrain, sendRadio } from './program-send.js'

const pickWords = 'choose cubi2_firmware.upd …'
const pause = ms => new Promise(done => { setTimeout(done, ms) })

// The Update window: new programs for the cube over the Wi-Fi. A small window over the page,
// moved by its title bar. It keeps the chosen file until it is sent, so it is a class.
class UpdateWindow {
  constructor() {
    this.element = document.createElement('section')
    this.drag = null
    this.parts = null      // what is inside the chosen file
    this.reading = false   // the chosen file is still being read

    // the places filled in by _render
    this.input = null
    this.pick = null
    this.send = null
    this.said = null
    this.programs = {}     // brain and radio: each one a bar and its words
  }

  _render = () => {
    const { element } = this
    const bar = make('div', { class: 'termbar' })
    const close = make('button', { class: 'x', 'aria-label': 'Close' }, '×')
    const file = make('div', { class: 'part' })
    const row = make('div', { class: 'row2' })

    element.setAttribute('class', 'term upd')
    element.setAttribute('id', names.update)
    element.setAttribute('role', 'dialog')
    element.setAttribute('aria-labelledby', 'label-update')
    element.hidden = true

    close.onclick = () => showUpdate(false)
    bar.appendChild(make('span', { class: 'label', id: 'label-update' }, 'Update - new programs over the Wi-Fi'))
    bar.appendChild(close)

    // a label for the file box: the box itself is hidden, the label is what is pressed
    this.pick = make('label', { class: 'pick', for: 'pick-file' }, pickWords)
    this.input = make('input', { type: 'file', id: 'pick-file', accept: '.upd,.hex,.bin' })
    this.send = make('button', { class: 'go' }, 'Send')
    this.said = make('div', { class: 'st' }, 'After every Build it is put next to the two program folders: firmware/cubi2_firmware.upd')

    this.input.onchange = this.#chosen
    this.send.onclick = this.#sendAll

    row.appendChild(this.pick)
    row.appendChild(this.input)
    row.appendChild(this.send)
    file.appendChild(make('h3', {}, 'cubi2 firmware'))
    file.appendChild(row)
    file.appendChild(this.said)

    element.appendChild(bar)
    element.appendChild(make('p', { class: 'note' },
      'Wheels off. Nothing changes until the whole file has arrived and been checked - ' +
      'a broken transfer leaves the old program. The USB cable always works too.'))
    element.appendChild(file)
    element.appendChild(this.#program('brain', 'Teensy - cubi2_brain'))
    element.appendChild(this.#program('radio', 'Wi-Fi board - cubi2_radio'))

    this.drag = new Drag(element, bar)
    this.drag.start()
  }

  // one program: its name, a bar that fills as it goes, and a line of words under it
  #program = (which, title) => {
    const part = make('div', { class: 'part' })
    const track = make('div', { class: 'bar' })
    const fill = make('i')
    const words = make('div', { class: 'st' }, '-')

    track.appendChild(fill)
    part.appendChild(make('h3', {}, title))
    part.appendChild(track)
    part.appendChild(words)

    this.programs[which] = { fill, words }
    return part
  }

  // how far a program has got: fraction from 0 to 1, and what to say
  #show = (which, fraction, text) => {
    const { fill, words } = this.programs[which]
    fill.style.width = Math.round(fraction * 100) + '%'
    words.textContent = text
  }

  // a file was chosen: what is inside it, and when it was built
  #chosen = async () => {
    const file = this.input.files[0]
    this.parts = null
    this.#show('brain', 0, '-')
    this.#show('radio', 0, '-')

    this.pick.textContent = file ? file.name + ' · ' + Math.round(file.size / 1024) + ' KB' : pickWords
    this.pick.classList.toggle('has', Boolean(file))
    if (!file) return

    this.reading = true
    this.said.textContent = 'reading the file ...'
    try {
      const parts = await readUpdate(file.name, await file.arrayBuffer())
      // another file was chosen while this one was read: this one is no longer wanted
      if (this.input.files[0] !== file) return

      this.parts = parts
      const { brain, radio, ready } = whatIsInside(parts)
      this.#show('brain', 0, brain)
      this.#show('radio', 0, radio)
      this.said.textContent = ready
    } catch (trouble) {
      if (this.input.files[0] === file) this.said.textContent = 'cannot read it - ' + trouble.message
    } finally {
      if (this.input.files[0] === file) this.reading = false
    }
  }

  #sendAll = async () => {
    if (!this.input.files[0]) return (this.said.textContent = 'choose the file first')
    if (this.reading) return (this.said.textContent = 'a moment - the file is still being read')
    if (!this.parts) return (this.said.textContent = 'cannot read it - choose it again')

    // the cube must stand still: a new program while it balances would drop it
    const cubeState = state.liveTelemetry?.st
    if (cubeState === 'BALANCING' || cubeState === 'READY') return (this.said.textContent = 'the cube is running - \'stop\' first')

    let programs
    try {
      programs = programsIn(this.parts)
    } catch (trouble) {
      return (this.said.textContent = 'NOT sent - ' + trouble.message)
    }

    const { brain, radio } = programs
    const address = state.liveAddress
    let now = 'brain'   // which program is on its way, so a stop is shown on the right bar

    state.updating = true   // the app stops asking the cube for data, and stops drawing
    this.send.disabled = true

    try {
      if (brain) await sendBrain(address, brain, (fraction, text) => this.#show('brain', fraction, text))
      else this.#show('brain', 0, 'not in the file - unchanged')

      now = 'radio'
      if (radio) {
        if (brain) {
          this.#show('radio', 0, 'waiting for the Teensy to restart ...')
          await pause(settings.update.waitForRestart)
        }
        await sendRadio(address, radio, (fraction, text) => this.#show('radio', fraction, text))
      } else {
        this.#show('radio', 0, 'not in the file - unchanged')
      }

      this.said.textContent = 'done' + (radio ? ' - join the Cubi2 Wi-Fi again if the computer left it' : '')
    } catch (trouble) {
      const why = trouble.message.replace(/^fw e /, '')
      say('update stopped: ' + why, true)
      this.#show(now, 0, 'NOT updated - ' + why + '\nthe old program stays')
      this.said.textContent = 'stopped - see below'
    } finally {
      // finally - whatever happened above, the app asks the cube again and the button wakes up
      state.updating = false
      this.send.disabled = false
    }
  }

  // the window goes away and takes its listeners with it
  clear = () => {
    this.drag?.clear()
    this.drag = null
  }
}

export default UpdateWindow
