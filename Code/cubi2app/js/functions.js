import { make } from '../tools/make.js'
import names, { valueOf } from './names.js'

// The element with this id. If it is not on the page, that is a mistake in our own code and it
// says so at once - a quiet null would travel on and break something else, far from here.
const byId = id => {
  const part = document.getElementById(id)
  if (!part) throw new Error('there is no \'' + id + '\' on the page - look in names.js')
  return part
}

// for the few places that may not be there yet, and that is no mistake
const byIdIfThere = id => document.getElementById(id)

// A button with its big word, and the command it sends written small underneath.
const keyButton = (id, word, key, title) => {
  const button = make('button', { id, title })
  button.appendChild(document.createTextNode(word))
  button.appendChild(make('span', { class: 'k' }, key))
  return button
}

// A card of the side columns: its small title on top. kind: 'sphere' or 'cube', or nothing for both.
const makeCard = (element, labelId, labelText, kind) => {
  element.setAttribute('class', 'card')
  element.setAttribute('aria-labelledby', labelId)
  if (kind) element.setAttribute('data-kind', kind)
  if (kind === 'cube') element.hidden = true
  element.appendChild(make('div', { class: 'label', id: labelId }, labelText))
}

// A slider with its name and its value: { id, label, min, max, step, value, out }
const sliderRow = item => {
  const { id, label, min, max, step, value, out } = item
  const row = make('label', { class: 'row', for: id })
  const slider = make('input', { id, type: 'range', min, max, step, value })
  const shown = make('output', { id: valueOf(id) }, out)

  row.appendChild(document.createTextNode(label))
  row.appendChild(slider)
  row.appendChild(shown)
  return row
}

// A line in the terminal, and the same words as the short message over the 3D view.
// The text is put in as text, never as page code - so a '<' from the cube stays a '<'.
const say = (text, strong) => {
  const log = byId(names.log)
  const line = make('div')

  line.appendChild(strong ? make('b', {}, text) : document.createTextNode(text))
  log.appendChild(line)
  log.scrollTop = log.scrollHeight
  byId(names.message).textContent = text
}

// meters to centimeters, for the screen
const centimeters = meters => (meters * 100).toFixed(1)

// A small window opens and closes with its own button in the top bar.
const showWindow = (windowId, buttonId, on) => {
  byId(windowId).hidden = !on
  byId(buttonId).setAttribute('aria-pressed', String(on))
}

const showTerminal = on => {
  showWindow(names.terminal, names.terminalButton, on)
  if (!on) return

  byId(names.log).scrollTop = byId(names.log).scrollHeight
  byId(names.commandInput).focus({ preventScroll: true })
}

const showSettings = on => showWindow(names.settings, names.settingsButton, on)

// the Runs window draws itself the moment it opens - see runs-window.js
const showRuns = on => showWindow(names.runs, names.runsButton, on)

export { byId, byIdIfThere, keyButton, makeCard, sliderRow, valueOf, say, centimeters, showWindow, showTerminal, showSettings, showRuns }
