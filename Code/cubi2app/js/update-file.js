import { unzip } from '../tools/unzip.js'

// the names of the programs inside the update file
const brainFile = 'cubi2_brain.hex'   // the Teensy
const radioFile = 'cubi2_radio.bin'   // the Wi-Fi board
const aboutFile = 'about.txt'         // when each one was built

const textOf = bytes => new TextDecoder().decode(bytes)

// The update file, as { 'cubi2_brain.hex': bytes, 'cubi2_radio.bin': bytes, 'about.txt': bytes }.
// It is a zip made by firmware/cubi2_update_file.py; a .hex or a .bin alone works too.
const readUpdate = async (name, buffer) => {
  const lower = name.toLowerCase()
  if (lower.endsWith('.hex')) return { [brainFile]: new Uint8Array(buffer) }   // [ ] - a name made from a value
  if (lower.endsWith('.bin')) return { [radioFile]: new Uint8Array(buffer) }
  return unzip(buffer)
}

// When a program was built, from about.txt - its line there is 'cubi2_brain.hex 2026-10-30 18:02'
const builtOf = (parts, file) => {
  if (!parts[aboutFile]) return ''
  const line = textOf(parts[aboutFile]).split('\n').find(one => one.startsWith(file + ' '))
  return line ? ' - built ' + line.slice(file.length + 1).trim() : ''
}

// What the window says about each program, before anything is sent
const whatIsInside = parts => ({
  brain: parts[brainFile] ? brainFile + builtOf(parts, brainFile) : 'not in the file - stays as it is',
  radio: parts[radioFile] ? radioFile + builtOf(parts, radioFile) : 'not in the file - stays as it is',
  ready: 'ready - Send updates ' + [parts[brainFile] && 'the Teensy', parts[radioFile] && 'the Wi-Fi board'].filter(Boolean).join(', then ')
})

// The Teensy's program is lines of text, and every one starts with ':'
const hexLinesOf = bytes => textOf(bytes)
  .split(/\r?\n/)
  .map(line => line.trim())
  .filter(line => line.startsWith(':'))

// The two programs, checked before anything is sent. One that is not in the file is null.
const programsIn = parts => {
  const brain = parts[brainFile] ? hexLinesOf(parts[brainFile]) : null
  const radio = parts[radioFile] ?? null

  if (!brain && !radio) throw new Error('no program inside')
  if (brain && brain.length < 10) throw new Error('the Teensy\'s part is not a .hex program')
  // every program of the Wi-Fi board starts with the same byte
  if (radio && radio[0] !== 0xE9) throw new Error('the Wi-Fi board\'s part is not an ESP32 program (.bin)')

  return { brain, radio }
}

export { readUpdate, whatIsInside, programsIn, hexLinesOf, radioFile }
