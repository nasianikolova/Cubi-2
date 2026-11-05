import settings from './settings.js'
import { sendProgramStep } from './cube-link.js'
import { say } from './functions.js'
import { radioFile } from './update-file.js'

const { linesAtOnce, triesForAPart, triesToReach, pauseBetweenTries, waitAtMost } = settings.update

const pause = ms => new Promise(done => { setTimeout(done, ms) })

// The answers that mean 'that part did not arrive right' - then the same part goes again
const damagedWords = [
  'lines lost', 'reached the Wi-Fi board', 'no answer', 'no connection', 'bad character',
  'checksum', 'wrong length', 'not a hex line', 'line too long'
]
const damaged = new RegExp(damagedWords.join('|'))   // one search for any of them

// One step, asked again when the Wi-Fi board does not answer at all.
// It always comes back with the cube's answer in words, such as 'fw ready' or 'fw e checksum'.
const step = async (address, name, count, text = '') => {
  for (let tries = 1; ; tries++) {
    try {
      const answer = await sendProgramStep(address, name, count, text)
      return answer.text.trim()
    } catch {
      if (tries >= triesToReach) return `fw e no connection to the Wi-Fi board (${address})`
      await pause(pauseBetweenTries)
    }
  }
}

// The Teensy's new program, 64 lines at a time. show(fraction, words) moves its bar.
// Nothing changes on the cube until the last line has arrived and been checked: a broken
// sending leaves the old program.
const sendBrain = async (address, lines, show) => {
  const started = performance.now()
  show(0, 'clearing the free flash ...')
  say('UPDATE - the Teensy: ' + lines.length + ' lines', true)

  let answer = await step(address, 'begin', 0)
  if (!answer.startsWith('fw ready')) throw new Error(answer)

  let sentAgain = 0
  try {
    for (let from = 0; from < lines.length; from += linesAtOnce) {
      const part = lines.slice(from, from + linesAtOnce)
      const upTo = from + part.length

      for (let tries = 1; ; tries++) {
        answer = await step(address, 'data', part.length, part.join('\n'))

        if (answer.startsWith('fw k')) {
          const counted = parseInt(answer.slice(5))
          if (counted !== upTo) throw new Error(`the Teensy counted ${counted} lines, ${upTo} were sent`)
          break
        }

        // a line damaged on the way: the Teensy keeps nothing of this part - the same part again
        if (!damaged.test(answer) || tries > triesForAPart) throw new Error(answer)
        const line = answer.match(/^fw e line (\d+)/)?.[1]   // ?.[1] - the number, if there is one
        if (line) {
          sentAgain++
          show(from / lines.length, `line ${line} arrived damaged - sending it again (${tries})`)
        }
      }

      const seconds = (performance.now() - started) / 1000
      const left = seconds * (lines.length - upTo) / upTo
      show(upTo / lines.length, `${upTo} / ${lines.length} lines · ${Math.round(seconds)} s, about ${Math.round(left)} s left`)
    }

    answer = await step(address, 'end', lines.length)
    if (!answer.startsWith('fw done')) throw new Error(answer)
  } catch (trouble) {
    step(address, 'abort', 0)   // not waited for: the old program stays either way
    throw trouble
  }

  if (sentAgain) say(`UPDATE - ${sentAgain} damaged part(s) sent again, all arrived right`, true)
  show(1, answer.slice(3) + '\nwait ~15 s (the motor drivers start in 10 s)')
  say(answer.slice(3), true)
}

// The Wi-Fi board's new program, in one piece.
// XMLHttpRequest and not fetch: only XMLHttpRequest tells how much of the sending has gone.
const sendRadio = (address, bytes, show) => new Promise((done, fail) => {
  say('UPDATE - the Wi-Fi board: ' + Math.round(bytes.length / 1024) + ' KB', true)

  const form = new FormData()
  form.append('file', new Blob([bytes]), radioFile)

  const sending = new XMLHttpRequest()
  const broke = () => fail(new Error('the connection broke - if the Wi-Fi board restarted, join Cubi2 again'))

  sending.open('POST', address + '/fw/r')
  sending.timeout = waitAtMost

  sending.upload.onprogress = event => {
    if (!event.lengthComputable) return
    const all = event.loaded === event.total
    // through tools/serve.mjs the computer takes it all at once, then passes it on to the cube
    show(event.loaded / event.total, all
      ? 'on its way to the Wi-Fi board ...'
      : Math.round(event.loaded / 1024) + ' / ' + Math.round(event.total / 1024) + ' KB')
  }

  sending.onload = () => {
    const answer = sending.responseText.trim()
    if (!answer.startsWith('fw done')) return fail(new Error(answer))

    show(1, answer.replace(/^fw done - /, ''))
    say(answer.slice(3), true)
    done()
  }

  sending.onerror = broke
  sending.ontimeout = broke
  sending.send(form)
})

export { sendBrain, sendRadio }
