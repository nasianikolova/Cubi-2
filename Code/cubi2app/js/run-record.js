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
    this.waitingForDump = null // the run that has ended, while its recording comes in
    this.dumpLines = null      // the lines of the recording, once it has started coming
    this.why = ''              // the last 'STOPPED: ...' line the cube wrote
    this.lastRun = null
    this.giveUp = null

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
  // It answers true when the line belonged to a recording, so the Terminal leaves it out.

  takeLine = line => {
    if (this.running && line.startsWith('STOPPED')) this.why = line.trim()
    if (!this.waitingForDump) return false

    if (line.trim() === '> dump') return true

    if (isDumpStart(line)) {
      this.dumpLines = []
      return true
    }

    if (!this.dumpLines) return false

    if (isDumpEnd(line)) {
      this.#dumpFinished()
      return true
    }

    this.dumpLines.push(line)
    return true
  }

  // ----- the steps of a run ------------------------------------------------------------------

  #started = reading => {
    this.running = { from: new Date(), mode: reading.m || 'corner' }
    this.why = ''
  }

  #ended = reading => {
    const run = this.running
    this.running = null

    this.waitingForDump = {
      ...run,
      // STOPPED means the cube gave up by itself; anything else is a hand that stopped it standing
      stoppedBy: reading.st === 'STOPPED' ? 'cubi' : 'hand',
      why: this.why,
      // in case the recording never comes, the app's own clock is all there is
      clockSeconds: (Date.now() - run.from.getTime()) / 1000
    }
    this.dumpLines = null

    sendCommand(state.liveAddress, 'dump').catch(() => {})
    clearTimeout(this.giveUp)
    this.giveUp = setTimeout(this.#noDumpCame, settings.runs.dumpWaitAtMost)
  }

  #dumpFinished = () => {
    clearTimeout(this.giveUp)
    const readings = readDump(this.dumpLines)
    this.#keep(readings)
  }

  #noDumpCame = () => {
    if (!this.waitingForDump) return
    this.#keep([])
  }

  #keep = readings => {
    const run = this.waitingForDump
    const summed = sumUp(readings)

    this.waitingForDump = null
    this.dumpLines = null

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
    try {
      const answer = await fetch(where, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(what)
      })
      if (!answer.ok) throw new Error(String(answer.status))
    } catch {
      say('the run could not be written - open the app with tools/serve.mjs to keep the runs')
    }
  }

  // the table is cleared: nothing is left waiting
  clear = () => {
    clearTimeout(this.giveUp)
    this.running = null
    this.waitingForDump = null
    this.dumpLines = null
  }
}

const runRecord = new RunRecord()

export { runRecord, dayOf, dateOf }
