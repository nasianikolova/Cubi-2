import state from './state.js'
import settings from './settings.js'
import { say } from './functions.js'
import { sendCommand } from './cube-link.js'
import { isDumpStart, isDumpEnd, readDump, sumUp } from './dump.js'

const { keepAt, keepLastAt } = settings.runs
const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']

// 31 Oct, the way the days are written on the chart
const dayOf = when => when.getDate() + ' ' + months[when.getMonth()]

// 2026-10-31, the way the days are kept in the file
const dateOf = when => [
  when.getFullYear(),
  String(when.getMonth() + 1).padStart(2, '0'),
  String(when.getDate()).padStart(2, '0')
].join('-')

class RunRecord {
  constructor() {
    this.running = null        // the run going on now: when it started, and on what
    this.waiting = []          // runs that have ended, oldest first, each waiting for its recording
    this.dumpLines = null      // the lines of a recording, while one is coming in
    this.lateUntil = 0         // a recording given up on may still come until then - it stays out of the Terminal
    this.why = ''              // the last 'STOPPED: ...' line the cube wrote during the run
    this.lastRun = null

    // the Runs window puts its own redraw here, so it is told when a new run has been kept
    this.whenKept = null
  }

  // ----- every reading of the real cube comes here -------------------------------------------

  watch = reading => {
    if (!reading || reading.sph) return   // the sphere's rolling is not a run on a corner

    const balancing = reading.st === 'BALANCING'

    if (balancing && !this.running) return this.#started(reading)
    if (!balancing && this.running) this.#ended(reading)
  }

  // ----- every line the cube writes comes here first ---------------------------------------
  // It answers true when the line belongs to a recording we asked for, so the Terminal leaves it out.

  takeLine = line => {
    const text = line.trim()
    if (text.startsWith('STOPPED')) this.#reason(text)

    // a recording is coming in: every line of it is ours, up to its end
    if (this.dumpLines) {
      if (isDumpEnd(text)) this.#dumpFinished()
      else this.dumpLines.push(line)
      return true
    }

    // no recording of ours is expected: a 'dump' typed in the Terminal is shown as always
    const expected = this.waiting.length > 0 || Date.now() < this.lateUntil
    if (!expected) return false

    if (text === '> dump') return true
    if (isDumpStart(text)) {
      this.dumpLines = []
      return true
    }
    return false
  }

  // ----- the steps of a run ------------------------------------------------------------------

  #started = reading => {
    this.running = { from: new Date(), mode: reading.m || 'corner' }
    this.why = ''
  }

  // The cube's reason - it can come a moment after the reading that already said STOPPED
  #reason = text => {
    if (this.running) {
      this.why = text
      return
    }
    const last = this.waiting.at(-1)
    if (last?.stoppedBy === 'cubi' && !last.why) last.why = text
  }

  #ended = reading => {
    const run = {
      ...this.running,
      // STOPPED means the cube gave up by itself; anything else is a hand that stopped it standing
      stoppedBy: reading.st === 'STOPPED' ? 'cubi' : 'hand',
      why: this.why,
      // in case the recording never comes, the app's own clock is all there is
      clockSeconds: (Date.now() - this.running.from.getTime()) / 1000,
      giveUp: null
    }
    this.running = null

    run.giveUp = setTimeout(() => this.#noDumpCame(run), settings.runs.dumpWaitAtMost)
    this.waiting.push(run)
    sendCommand(state.liveAddress, 'dump').catch(() => {})   // if it never arrives, the timer keeps the run
  }

  // Does this recording belong to this run? The cube's time and the app's clock must agree.
  // The cube's time may be much longer - when the app was opened while the cube already stood -
  // but never much shorter.
  #agrees = (run, summed) => summed.timeOnTheCorner >= run.clockSeconds - Math.max(2, run.clockSeconds * 0.1)

  #dumpFinished = () => {
    const readings = readDump(this.dumpLines)
    const summed = sumUp(readings)
    this.dumpLines = null

    if (!summed) return   // an empty recording: the run is kept by its timer, with the app's clock

    // of the runs still waiting, the one whose time is closest to the recording's own;
    // none agrees - the recording is nobody's, and it is left out
    const distance = run => Math.abs(run.clockSeconds - summed.timeOnTheCorner)
    const run = this.waiting
      .filter(one => this.#agrees(one, summed))
      .sort((a, b) => distance(a) - distance(b))[0]
    if (run) this.#keep(run, readings)
  }

  #noDumpCame = run => {
    if (!this.waiting.includes(run)) return
    this.lateUntil = Date.now() + settings.runs.dumpWaitAtMost
    this.#keep(run, [])
  }

  #keep = (run, readings) => {
    clearTimeout(run.giveUp)
    this.waiting = this.waiting.filter(one => one !== run)
    const summed = sumUp(readings)

    // the cube's own time is exact; the app's clock is only used when there is no recording
    const seconds = Math.round((summed?.timeOnTheCorner ?? run.clockSeconds) * 100) / 100

    const kept = {
      date: dateOf(run.from),
      day: dayOf(run.from),
      mode: run.mode,
      seconds,
      stoppedBy: run.stoppedBy,
      why: run.why
    }

    this.lastRun = { ...kept, readings, ...summed }
    say('run kept: ' + seconds + ' s on the ' + run.mode)

    this.#send(keepAt, kept)
    if (readings.length) this.#send(keepLastAt, this.lastRun)
    this.whenKept?.()
  }

  // the run goes to whoever serves the app, which writes it into data/
  #send = async (where, what) => {
    let answer
    try {
      answer = await fetch(where, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(what)
      })
    } catch {
      say('the run could not be written - open the app with tools/serve.mjs to keep the runs')
      return
    }
    // the server says why it did not write it
    if (!answer.ok) say('the run could not be written - ' + await answer.text())
  }

  // the table is cleared: nothing is left waiting, no timer is left running
  clear = () => {
    this.waiting.forEach(run => clearTimeout(run.giveUp))
    this.running = null
    this.waiting = []
    this.dumpLines = null
    this.lateUntil = 0
  }
}

const runRecord = new RunRecord()

export { runRecord, dayOf, dateOf }
