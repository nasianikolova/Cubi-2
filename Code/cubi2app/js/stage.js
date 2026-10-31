import { make } from '../tools/make.js'
import names from './names.js'

class Stage {
  constructor() {
    this.element = document.createElement('div')

    // the two pills that sit over the view
    this.pills = [
      { id: names.statePill, class: 'pill state', text: 'Hold' },
      { id: names.message, class: 'pill', text: 'go sphere' }
    ]
  }

  _render = () => {
    const { element, pills } = this
    const view = make('canvas', {
      id: names.view,
      'aria-label': '3D view of Cubi2 on a floor with a 10 cm grid'
    })

    // the pills sit side by side, so they need the one holder the style asks for (.hud)
    const overlay = make('div', { class: 'hud' })
    const legend = make('div', { class: 'legend', id: names.legend },
      'grid 10 cm · red arrow = front · drag to orbit, wheel to zoom')

    element.setAttribute('class', 'stage')

    pills.forEach(pill => {
      const { id, text } = pill
      overlay.appendChild(make('span', { class: pill.class, id }, text))
    })

    element.appendChild(view)
    element.appendChild(overlay)
    element.appendChild(legend)
  }
}

export default Stage
