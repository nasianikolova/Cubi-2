import { makeSvg } from '../tools/make-svg.js'

const tensOfTime = [
  { seconds: 1, text: '1 s' },
  { seconds: 10, text: '10 s' },
  { seconds: 60, text: '1 min' },
  { seconds: 600, text: '10 min' },
  { seconds: 3600, text: '1 h' }
]

const axes = ['x', 'y', 'z']
const gapInRecording = 0.05   // seconds: the cube writes a line every 16 ms, so more than this is a hole
const lineHeight = 15         // of the notes
const letterWidth = 7         // about, for the notes

// 1.4 s, 17.9 min, 1.2 h
const durationText = seconds => {
  if (seconds < 60) return seconds.toFixed(1) + ' s'
  if (seconds < 3600) return (seconds / 60).toFixed(1) + ' min'
  return (seconds / 3600).toFixed(1) + ' h'
}

// A fresh, empty drawing as big as its place on the screen, or null when it has no place yet
// (a hidden window has no size, so there is nothing to draw on).
const startDrawing = svg => {
  svg.replaceChildren()          // newer: empties it in one go
  const { width, height } = svg.getBoundingClientRect()
  if (!width || !height) return null

  svg.setAttribute('viewBox', `0 0 ${width} ${height}`)
  // appendChild gives back what it added, so a dot can get its own little title afterwards
  const add = (tag, attributes, text) => svg.appendChild(makeSvg(tag, attributes, text))
  return { width, height, add }
}

// A round step for the lines of a chart: 1, 2 or 5 times ten to some power - never 3.7
const niceStep = rough => {
  const power = 10 ** Math.floor(Math.log10(rough))
  return [1, 2, 5, 10].map(times => times * power).find(step => step >= rough)
}

// how many numbers after the point a step needs: 0.2 → 1, 5 → 0
const decimalsOf = step => Math.max(0, -Math.floor(Math.log10(step)))

// The scale of a chart: from a little below the smallest number to a little above the largest,
// with lines at the round numbers inside it.
const scaleOf = (smallest, largest) => {
  const room = (largest - smallest) * 0.06 || 0.5       // || - when all numbers are the same
  const low = smallest - room
  const high = largest + room
  const step = niceStep((high - low) / 5)
  const first = Math.ceil(low / step)
  const last = Math.floor(high / step)

  // Array.from({ length }) - a list of that many places, filled by the function
  const ticks = Array.from({ length: last - first + 1 }, (_, k) => Number(((first + k) * step).toFixed(10)))
  return { low, high, step, ticks }
}

// The runs, day by day: [{ day: '22 Oct', first: 0, last: 12 }, ...]
const daysOf = runs => runs.reduce((days, run, index) => {
  const today = days.at(-1)
  if (today?.date === run.date) today.last = index
  else days.push({ date: run.date, day: run.day, first: index, last: index })
  return days
}, [])

// A long note goes on more lines, two words on each: 'first balance 8.7 s' → 'first balance', '8.7 s'
const linesOf = note => {
  if (note.length <= 12) return [note]
  const words = note.split(' ')
  return words.filter((_, k) => k % 2 === 0).map((word, k) => [word, words[k * 2 + 1]].join(' ').trim())
}

const overlaps = (a, b) => a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom
const inside = (a, b) => a.left >= b.left && a.right <= b.right && a.top >= b.top && a.bottom <= b.bottom
const between = (value, low, high) => Math.min(Math.max(value, low), high)

// Where a note can go around its dot: above first, then below, then to the sides.
const noteTries = [[0, -36], [0, 40], [-58, -30], [58, -30], [-66, 0], [66, 0], [0, -66], [0, 70]]

// The box a note covers when its last line sits at (x, y)
const noteBox = (lines, x, y) => {
  const half = Math.max(...lines.map(line => line.length)) * letterWidth / 2 + 3
  return { left: x - half, right: x + half, top: y - (lines.length - 1) * lineHeight - 12, bottom: y + 5 }
}

// ---- ALL RUNS ----

const drawAllRuns = (svg, runs) => {
  const drawing = startDrawing(svg)
  if (!drawing) return
  const { width, height, add } = drawing

  const left = 62
  const right = width - 14
  const top = 30
  const bottom = height - 46

  add('rect', { class: 'frame', x: left, y: top, width: right - left, height: bottom - top })
  add('text', { class: 'chart-label', x: left, y: top - 10 }, 'TIME ON THE CORNER')

  if (!runs.length) {
    add('text', { class: 'empty', x: (left + right) / 2, y: (top + bottom) / 2 }, 'no runs yet - the runs in Live come here by themselves')
    return
  }

  // the scale of tens: 1 s to 10 s is as tall as 10 s to 100 s
  const atLeast = seconds => Math.max(seconds, 0.01)    // a run of 0 s would fall off the bottom
  const allSeconds = runs.map(run => atLeast(run.seconds))
  const low = Math.log10(Math.min(0.8, Math.min(...allSeconds) * 0.8))
  const high = Math.log10(Math.max(3000, Math.max(...allSeconds) * 3))
  const slot = (right - left) / runs.length
  const x = index => left + (index + 0.5) * slot
  const y = seconds => top + (high - Math.log10(atLeast(seconds))) / (high - low) * (bottom - top)

  tensOfTime
    .filter(({ seconds }) => y(seconds) > top && y(seconds) < bottom)
    .forEach(({ seconds, text }) => {
      add('line', { class: 'grid', x1: left, x2: right, y1: y(seconds), y2: y(seconds) })
      add('text', { class: 'tick to-left', x: left - 8, y: y(seconds) + 4 }, text)
    })

  // the things a note must not cover
  const taken = []

  // the days: a dashed line between them, the day on top, the runs of a day joined by a line
  daysOf(runs).forEach((day, number) => {
    const middle = (x(day.first) + x(day.last)) / 2
    if (number > 0) {
      const edge = x(day.first) - slot / 2
      add('line', { class: 'day-line', x1: edge, x2: edge, y1: top, y2: bottom })
    }
    add('text', { class: 'day', x: middle, y: top + 26 }, day.day)
    taken.push({ left: middle - 30, right: middle + 30, top: top + 10, bottom: top + 32 })

    const path = runs
      .slice(day.first, day.last + 1)
      .map((run, k) => (k ? 'L' : 'M') + x(day.first + k) + ' ' + y(run.seconds))
      .join(' ')
    add('path', { class: 'run-line', d: path })
  })

  // the key, top left: what a full and an empty dot mean
  const keyX = left + 16
  const keyY = top + 54
  ;[['hand', 'still standing: I stopped it'], ['cubi', 'Cubi stopped itself']].forEach(([who, text], k) => {
    add('circle', { class: 'dot ' + who, cx: keyX, cy: keyY + k * 24, r: 6 })
    add('text', { class: 'key', x: keyX + 16, y: keyY + k * 24 + 5 }, text)
  })
  taken.push({ left: keyX - 10, right: keyX + 230, top: keyY - 12, bottom: keyY + 34 })

  runs.forEach((run, index) => {
    taken.push({ left: x(index) - 8, right: x(index) + 8, top: y(run.seconds) - 8, bottom: y(run.seconds) + 8 })
  })

  // the notes, each in the first place around its dot where it covers nothing
  const frame = { left, right, top, bottom }
  const notes = []
  runs.forEach((run, index) => {
    if (!run.note) return
    const lines = linesOf(run.note)
    const dotX = x(index)
    const dotY = y(run.seconds)
    const boxAt = ([dx, dy]) => noteBox(lines, dotX + dx, dotY + dy)
    const place = noteTries.find(move => {
      const box = boxAt(move)
      return inside(box, frame) && !taken.some(other => overlaps(box, other))
    }) ?? noteTries[0]
    const box = boxAt(place)

    taken.push(box)
    notes.push({ lines, dotX, dotY, box, noteX: dotX + place[0], noteY: dotY + place[1] })
  })

  // the thin line from a dot to the nearest edge of its note
  notes.forEach(({ dotX, dotY, box }) => {
    add('line', {
      class: 'note-line',
      x1: dotX, y1: dotY,
      x2: between(dotX, box.left, box.right), y2: between(dotY, box.top, box.bottom)
    })
  })

  // the dots: full when it was stopped by hand, still standing - empty when Cubi stopped itself
  const labelEvery = [1, 2, 5, 10, 20, 50, 100].find(every => slot * every >= 22) ?? 200
  runs.forEach((run, index) => {
    const dot = add('circle', { class: 'dot ' + run.stoppedBy, cx: x(index), cy: y(run.seconds), r: 6 })
    const stopped = run.stoppedBy === 'hand' ? 'stopped by hand' : 'Cubi stopped itself'
    dot.appendChild(makeSvg('title', {}, `run ${index + 1} · ${durationText(run.seconds)} · ${stopped}`))

    if ((index + 1) % labelEvery === 0 || labelEvery === 1) {
      add('text', { class: 'tick', x: x(index), y: bottom + 18 }, index + 1)
    }
  })

  notes.forEach(({ lines, noteX, noteY }) => {
    lines.forEach((line, k) => {
      add('text', { class: 'note', x: noteX, y: noteY - (lines.length - 1 - k) * lineHeight }, line)
    })
  })

  add('text', { class: 'axis-name', x: (left + right) / 2, y: height - 6 }, 'run')
}

// ---- THE LAST RUN ----

// One chart: tilt or wheels, X solid, Y dashed, Z dotted, against the seconds of the recording.
// timeName is the name under the seconds - only the lower chart has one.
const drawReadings = (svg, readings, what, label, keyWord, timeName) => {
  const drawing = startDrawing(svg)
  if (!drawing) return
  const { width, height, add } = drawing

  const left = 62
  const right = width - 10
  const top = 30
  const bottom = height - (timeName ? 46 : 28)

  add('rect', { class: 'frame', x: left, y: top, width: right - left, height: bottom - top })
  add('text', { class: 'chart-label', x: left, y: top - 10 }, label)

  if (!readings?.length) {
    add('text', { class: 'empty', x: (left + right) / 2, y: (top + bottom) / 2 }, 'no recording yet - it comes after the first run in Live')
    return
  }

  const start = readings[0].time
  const span = Math.max(readings.at(-1).time - start, 0.001)
  const values = readings.flatMap(reading => reading[what])
  const { low, high, step, ticks } = scaleOf(Math.min(...values), Math.max(...values))
  const x = time => left + (time - start) / span * (right - left)
  const y = value => top + (high - value) / (high - low) * (bottom - top)

  const valueDecimals = decimalsOf(step)
  ticks.forEach(value => {
    add('line', { class: 'grid', x1: left, x2: right, y1: y(value), y2: y(value) })
    add('text', { class: 'tick to-left', x: left - 8, y: y(value) + 4 }, value.toFixed(valueDecimals))
  })

  // the seconds, counted from the first line of the recording
  const timeStep = niceStep(span / 6)
  const timeDecimals = decimalsOf(timeStep)
  for (let k = 0; k * timeStep <= span + 1e-9; k++) {
    const seconds = k * timeStep
    add('line', { class: 'grid', x1: x(start + seconds), x2: x(start + seconds), y1: top, y2: bottom })
    add('text', { class: 'tick', x: x(start + seconds), y: bottom + 18 }, seconds.toFixed(timeDecimals))
  }

  // one line for each axis; where the recording has a hole, the line has one too
  axes.forEach((axis, k) => {
    const path = readings
      .map((reading, index) => {
        const hole = index > 0 && reading.time - readings[index - 1].time > gapInRecording
        return (index === 0 || hole ? 'M' : 'L') + x(reading.time) + ' ' + y(reading[what][k])
      })
      .join(' ')
    add('path', { class: 'reading axis-' + axis, d: path })

    const keyX = right - 330 + k * 110
    add('line', { class: 'key-line axis-' + axis, x1: keyX, x2: keyX + 28, y1: top - 14, y2: top - 14 })
    add('text', { class: 'key mono', x: keyX + 36, y: top - 9 }, keyWord + ' ' + axis.toUpperCase())
  })

  if (timeName) add('text', { class: 'axis-name', x: (left + right) / 2, y: height - 6 }, timeName)
}

// The recording of a long run holds only its last seconds - then the seconds say after how long.
const timeNameOf = (readings, mode) => {
  const start = readings?.[0]?.time ?? 0
  return start > 1 ? `seconds, after ${durationText(start)} on the ${mode}` : 'seconds'
}

const drawLastRun = (tiltSvg, wheelsSvg, lastRun) => {
  const readings = lastRun?.readings
  drawReadings(tiltSvg, readings, 'tilt', 'TILT, degrees', 'tilt', '')
  drawReadings(wheelsSvg, readings, 'wheels', 'WHEELS, rpm', 'wheel', timeNameOf(readings, lastRun?.mode))
}

export { drawAllRuns, drawLastRun, durationText, niceStep, scaleOf, linesOf, daysOf }
