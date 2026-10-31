import { make } from '../tools/make.js'
import { byId, makeCard } from './functions.js'
import settings from './settings.js'
import names from './names.js'

const svgNamespace = 'http://www.w3.org/2000/svg'
const { seconds, everySecond, fullScale, wheelLimit } = settings.wheelsChart
const points = seconds * everySecond   // how many points the chart holds
const lines = [-4000, -2000, 0, 2000, 4000]
const leftGap = 34                     // room for the numbers down the left, in pixels
const airAbove = 6

const themeValue = name => getComputedStyle(document.documentElement).getPropertyValue(name).trim()

class WheelsCard {
  constructor() {
    this.element = document.createElement('section')
    this.chart = document.createElement('canvas')
    this.history = []

    this.keys = [
      { id: names.wheelKey('x'), axis: 'X', dash: '' },
      { id: names.wheelKey('y'), axis: 'Y', dash: '5 3' },
      { id: names.wheelKey('z'), axis: 'Z', dash: '1.5 2.5' }
    ]
  }

  _render = () => {
    const { element, chart, keys } = this
    const around = make('div', { class: 'chartwrap' })
    // .keys is a grid of three columns in the style file, so each key has to be one cell of it -
    // that is the third level of plates here, and the only one
    const keyRow = make('div', { class: 'keys' })

    makeCard(element, 'label-wheels', 'Wheels, rpm - last ' + seconds + ' s')
    element.classList.add('grow')

    chart.setAttribute('id', names.chart)
    chart.setAttribute('aria-label',
      'Graph of the three wheel speeds over the last ' + seconds + ' seconds, limit ' + wheelLimit + ' rpm')

    keys.forEach(key => {
      const { id, axis, dash } = key
      const one = make('span')

      one.appendChild(this.#keyLine(dash))
      one.appendChild(make('b', { id }, axis + ' 0'))
      keyRow.appendChild(one)
    })

    around.appendChild(chart)
    element.appendChild(around)
    element.appendChild(keyRow)
  }

  // the little solid, dashed or dotted line drawn beside each letter
  #keyLine = dash => {
    const picture = document.createElementNS(svgNamespace, 'svg')
    const line = document.createElementNS(svgNamespace, 'line')

    picture.setAttribute('width', '18')
    picture.setAttribute('height', '6')
    picture.setAttribute('aria-hidden', 'true')

    line.setAttribute('x1', '0')
    line.setAttribute('y1', '3')
    line.setAttribute('x2', '18')
    line.setAttribute('y2', '3')
    line.setAttribute('stroke', 'currentColor')
    line.setAttribute('stroke-width', '2')
    if (dash) line.setAttribute('stroke-dasharray', dash)

    picture.appendChild(line)
    return picture
  }

  clear = () => {
    this.history.length = 0
  }

  // one point, a tenth of a second apart
  draw = rpm => {
    const { history, keys } = this

    history.push([...rpm])
    if (history.length > points) history.shift()

    keys.forEach((key, i) => {
      byId(key.id).textContent = key.axis + ' ' + rpm[i].toFixed(0)
    })

    const paper = this.#fitTheCanvas()
    if (!paper) return

    this.#drawLines(paper)
    this.#drawLimit(paper)
    this.#drawWheels(paper)
  }

  // the canvas is made as many real pixels as the screen has, so the lines stay sharp
  #fitTheCanvas = () => {
    const { chart } = this
    const box = chart.getBoundingClientRect()
    if (box.width < 1 || box.height < 1) return null

    const pixels = Math.min(2, window.devicePixelRatio || 1)
    const wide = Math.max(10, Math.round(box.width * pixels))
    const high = Math.max(10, Math.round(box.height * pixels))

    if (chart.width !== wide || chart.height !== high) {
      chart.width = wide
      chart.height = high
    }

    const paint = chart.getContext('2d')
    paint.setTransform(pixels, 0, 0, pixels, 0, 0)
    paint.clearRect(0, 0, box.width, box.height)

    const top = airAbove
    const bottom = box.height - airAbove
    const middle = (top + bottom) / 2

    // how many pixels one rpm is worth
    return { paint, wide: box.width, middle, scale: (bottom - top) / 2 / fullScale }
  }

  // the lines across, with their numbers down the left
  #drawLines = ({ paint, wide, middle, scale }) => {
    paint.font = '10px ' + themeValue('--f-mono')
    paint.fillStyle = themeValue('--muted')
    paint.textAlign = 'right'
    paint.textBaseline = 'middle'
    paint.strokeStyle = themeValue('--rule')
    paint.lineWidth = 1

    lines.forEach(rpm => {
      const y = Math.round(middle - rpm * scale) + 0.5

      paint.beginPath()
      paint.moveTo(leftGap, y)
      paint.lineTo(wide, y)
      paint.stroke()
      paint.fillText(rpm === 0 ? '0' : rpm / 1000 + 'k', leftGap - 5, y)
    })
  }

  // the dashed line where the real wheels run out
  #drawLimit = ({ paint, wide, middle, scale }) => {
    paint.strokeStyle = themeValue('--muted')
    paint.setLineDash([2, 4])

    ;[wheelLimit, -wheelLimit].forEach(rpm => {
      const y = Math.round(middle - rpm * scale) + 0.5

      paint.beginPath()
      paint.moveTo(leftGap, y)
      paint.lineTo(wide, y)
      paint.stroke()
    })

    paint.setLineDash([])
  }

  #drawWheels = ({ paint, wide, middle, scale }) => {
    const { history, keys } = this
    const count = history.length
    const stepX = (wide - leftGap) / (points - 1)
    const hold = rpm => Math.max(-fullScale, Math.min(fullScale, rpm))

    keys.forEach((key, i) => {
      paint.strokeStyle = themeValue('--ink')
      paint.lineWidth = 1.7
      paint.setLineDash(key.dash ? key.dash.split(' ').map(Number) : [])
      paint.beginPath()

      for (let k = 0; k < count; k++) {
        const x = leftGap + (points - count + k) * stepX
        const y = middle - hold(history[k][i]) * scale

        if (k) paint.lineTo(x, y)
        else paint.moveTo(x, y)
      }

      paint.stroke()
      paint.setLineDash([])

      // a dot on the newest point, so where each wheel is now can be seen at a glance
      if (!count) return
      paint.fillStyle = themeValue('--ink')
      paint.beginPath()
      paint.arc(wide - 2, middle - hold(history[count - 1][i]) * scale, 2.4, 0, 7)
      paint.fill()
    })
  }
}

const wheelsCard = new WheelsCard()

export { wheelsCard, WheelsCard }
