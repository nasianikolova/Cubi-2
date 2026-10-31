import { make } from '../tools/make.js'
import { byId, makeCard, sliderRow } from './functions.js'
import names, { valueOf } from './names.js'
import { command } from './commands.js'

class CubeSimCard {
  constructor() {
    this.element = document.createElement('section')
    this.slider = { id: names.cubeGravitySlider, label: 'CG offset', min: 0, max: 6, step: 0.1, value: 0, out: '0.0 mm' }
  }

  _render = () => {
    const { element, slider } = this
    const rows = make('div', { class: 'rows' })
    const row = sliderRow(slider)
    const input = row.querySelector('input')

    makeCard(element, 'label-cube-settings', 'Simulation', 'cube')

    // while it is dragged only the number changes; letting go is what moves the cube's weight
    input.oninput = event => {
      byId(valueOf(slider.id)).textContent = Number(event.target.value).toFixed(1) + ' mm'
    }
    input.onchange = event => command('cg ' + event.target.value)

    rows.appendChild(row)
    element.appendChild(rows)
  }
}

export default CubeSimCard
