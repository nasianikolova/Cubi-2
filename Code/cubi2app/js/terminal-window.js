import { make } from '../tools/make.js'
import Drag from '../tools/drag.js'
import { showTerminal } from './functions.js'
import names from './names.js'
import { order } from './orders.js'

class TerminalWindow {
  constructor() {
    this.element = document.createElement('section')
    this.drag = null
  }

  _render = () => {
    const { element } = this
    const bar = make('div', { class: 'termbar' })
    const title = make('span', { class: 'label', id: 'label-terminal' }, 'Terminal')
    const close = make('button', { class: 'x', 'aria-label': 'Close the terminal' }, '×')
    const log = make('div', { class: 'log', id: names.log, 'aria-live': 'polite' })
    const form = make('form', { class: 'cmd', id: 'command-form', autocomplete: 'off' })
    const input = make('input', {
      id: names.commandInput,
      placeholder: 'fwd 30 · left 90 · help',
      'aria-label': 'Command'
    })

    element.setAttribute('class', 'term')
    element.setAttribute('id', names.terminal)
    element.setAttribute('role', 'dialog')
    element.setAttribute('aria-labelledby', 'label-terminal')
    element.hidden = true

    close.onclick = () => showTerminal(false)

    // a form, so Enter in the line sends the command as the Send button does
    form.onsubmit = event => {
      event.preventDefault()
      order(input.value)
      input.value = ''
    }

    bar.appendChild(title)
    bar.appendChild(close)
    form.appendChild(input)
    form.appendChild(make('button', { type: 'submit' }, 'Send'))

    element.appendChild(bar)
    element.appendChild(log)
    element.appendChild(form)

    this.drag = new Drag(element, bar)
    this.drag.start()
  }

  // the window goes away and takes its listeners with it
  clear = () => {
    this.drag?.clear()
    this.drag = null
  }
}

export default TerminalWindow
