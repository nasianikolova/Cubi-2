import { make } from '../tools/make.js'
import { makeSvg } from '../tools/make-svg.js'
import settings from './settings.js'
import names from './names.js'
import { showRuns } from './functions.js'
import { runRecord } from './run-record.js'
import { drawAllRuns, drawLastRun, durationText } from './run-charts.js'

const { readFrom, readLastFrom } = settings.runs

// A file of the app, read as JSON - or what to use instead when it is not there.
const readJson = async (address, otherwise) => {
  try {
    // cache: 'no-store' - always the file as it is now, never an old copy the browser kept
    const answer = await fetch(address, { cache: 'no-store' })
    return answer.ok ? await answer.json() : otherwise
  } catch {
    return otherwise   // catch with nothing after it: the reason does not matter here
  }
}

class RunsWindow {
  constructor() {
    this.element = document.createElement('section')
    this.runs = []
    this.lastRun = null
    this.tab = 'all'
    this.watchSize = null

    // the places filled in by _render
    this.tabButtons = {}
    this.title = null
    this.pill = null
    this.allPane = null
    this.lastPane = null
    this.allChart = null
    this.tiltChart = null
    this.wheelsChart = null
    this.readouts = []
  }

  _render = () => {
    const { element } = this
    const bar = make('div', { class: 'termbar' })
    const tabs = make('div', { class: 'tabs', role: 'group', 'aria-label': 'Which chart' })
    const close = make('button', { class: 'x', 'aria-label': 'Close the runs' }, '×')
    const head = make('div', { class: 'runs-title' })
    const body = make('div', { class: 'runs-body' })

    element.setAttribute('class', 'term runsw')
    element.setAttribute('id', names.runs)
    element.setAttribute('role', 'dialog')
    element.setAttribute('aria-labelledby', 'label-runs')
    element.hidden = true

    ;[['all', 'All runs'], ['last', 'The last run']].forEach(([tab, text]) => {
      const button = make('button', { 'aria-pressed': String(tab === this.tab) }, text)
      button.onclick = () => this.showTab(tab)
      this.tabButtons[tab] = button
      tabs.appendChild(button)
    })
    close.onclick = () => showRuns(false)

    bar.appendChild(make('span', { class: 'label', id: 'label-runs' }, 'Runs'))
    bar.appendChild(tabs)
    bar.appendChild(close)

    this.title = make('h2')
    this.pill = make('span', { class: 'runs-pill' })
    head.appendChild(this.title)
    head.appendChild(this.pill)

    // ALL RUNS: one chart
    this.allPane = make('div', { class: 'runs-pane' })
    this.allChart = makeSvg('svg', { class: 'runs-chart', role: 'img', 'aria-label': 'All runs' })
    this.allPane.appendChild(this.allChart)

    // THE LAST RUN: two charts, and three big numbers beside them
    this.lastPane = make('div', { class: 'runs-pane' })
    const charts = make('div', { class: 'runs-charts' })
    const readouts = make('div', { class: 'readouts' })
    this.tiltChart = makeSvg('svg', { class: 'runs-chart', role: 'img', 'aria-label': 'Tilt' })
    this.wheelsChart = makeSvg('svg', { class: 'runs-chart', role: 'img', 'aria-label': 'Wheels' })
    charts.appendChild(this.tiltChart)
    charts.appendChild(this.wheelsChart)

    this.readouts = [0, 1, 2].map(() => {
      const one = { box: make('div'), name: make('span'), value: make('strong'), unit: make('span') }
      one.box.appendChild(one.name)
      one.box.appendChild(one.value)
      one.box.appendChild(one.unit)
      readouts.appendChild(one.box)
      return one
    })

    this.lastPane.appendChild(charts)
    this.lastPane.appendChild(readouts)

    body.appendChild(this.allPane)
    body.appendChild(this.lastPane)

    element.appendChild(bar)
    element.appendChild(head)
    element.appendChild(body)

    // When the window opens, or the page changes size, its size changes - then it draws again.
    // A hidden window has no size, so it is drawn the moment it is opened, never before.
    this.watchSize = new ResizeObserver(() => this.draw())
    this.watchSize.observe(body)

    this.showTab(this.tab)
  }

  // read what was kept in the files - once, when the app starts
  load = async () => {
    const kept = await readJson(readFrom, { runs: [] })
    this.runs = kept.runs ?? []
    this.lastRun = await readJson(readLastFrom, null)
    this.draw()
  }

  // a run was kept just now, while the app is open
  takeNewRun = () => {
    const { lastRun } = runRecord
    if (!lastRun) return

    // ...run - everything else that is in lastRun: what a run in runs.json holds
    const { readings, timeOnTheCorner, largestTilt, fastestWheel, ...run } = lastRun
    this.runs.push({ ...run, note: '' })
    if (readings?.length) this.lastRun = lastRun
    this.draw()
  }

  showTab = tab => {
    this.tab = tab
    Object.entries(this.tabButtons).forEach(([which, button]) => {
      button.setAttribute('aria-pressed', String(which === tab))
    })
    this.allPane.hidden = tab !== 'all'
    this.lastPane.hidden = tab !== 'last'
    this.draw()
  }

  draw = () => {
    if (this.element.hidden) return
    if (this.tab === 'all') this.#drawAll()
    else this.#drawLast()
  }

  // the runs of one way of standing: the one of the newest run, so a corner is never mixed with an edge
  #drawAll = () => {
    const mode = this.runs.at(-1)?.mode ?? 'corner'
    const runs = this.runs.filter(run => run.mode === mode)

    this.#head(`ALL RUNS  ·  ${mode}`, `${runs.length} runs  ·  1 change each`, 'count')
    drawAllRuns(this.allChart, runs)
  }

  #drawLast = () => {
    const { lastRun } = this
    drawLastRun(this.tiltChart, this.wheelsChart, lastRun)

    if (!lastRun) {
      this.#head('THE LAST RUN', '', '')
      this.readouts.forEach(one => this.#readout(one, '', '–', ''))
      return
    }

    const { date, day, mode, seconds, stoppedBy, why, readings, largestTilt, fastestWheel } = lastRun
    const runOfTheDay = this.runs.filter(run => run.date === date).length
    const ofTheDay = runOfTheDay ? `, run ${runOfTheDay}` : ''
    const year = date.slice(0, 4)

    if (stoppedBy === 'cubi') this.#head(`THE LAST RUN  ·  ${mode}  ·  ${day} ${year}${ofTheDay}`, why || 'Cubi stopped itself', 'stopped')
    else this.#head(`THE LAST RUN  ·  ${mode}  ·  ${day} ${year}${ofTheDay}`, 'off  ·  stopped on purpose', 'hand')

    // a long run keeps only its last seconds, so the largest and fastest are of those seconds only
    const onlyTheEnd = readings[0].time > 1 ? ', in the last seconds' : ''
    const [time, tilt, wheel] = this.readouts
    this.#readout(time, 'time on the ' + mode, durationText(seconds), day)
    this.#readout(tilt, 'largest tilt', largestTilt.toFixed(largestTilt < 1 ? 2 : 1) + '°', 'degrees' + onlyTheEnd)
    this.#readout(wheel, 'fastest wheel', fastestWheel.toFixed(0), 'rpm' + onlyTheEnd)
  }

  #head = (title, pill, kind) => {
    this.title.textContent = title
    this.pill.textContent = pill
    this.pill.hidden = !pill
    this.pill.setAttribute('class', 'runs-pill ' + kind)
  }

  #readout = (one, name, value, unit) => {
    one.name.textContent = name
    one.value.textContent = value
    one.unit.textContent = unit
  }

  clear = () => {
    this.watchSize?.disconnect()
    this.watchSize = null
  }
}

export default RunsWindow
