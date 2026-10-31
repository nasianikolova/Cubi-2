import { make } from '../tools/make.js'
import { byId, makeCard, sliderRow, valueOf } from './functions.js'
import settings from './settings.js'
import { order } from './orders.js'
import { command } from './commands.js'
import { showMoveLabels } from './drive-card.js'
import names from './names.js'

const { speed, accel, turnRate } = settings.sphereMoves
const degreesPerRadian = 57.29578

class SphereSettingsCard {
  constructor() {
    this.element = document.createElement('section')

    this.sliders = [
      {
        id: names.distanceSlider, label: 'Distance', min: 5, max: 150, step: 5, value: 30,
        out: '30 cm', show: value => value + ' cm', whileDragging: showMoveLabels
      },
      {
        id: names.angleSlider, label: 'Turn', min: 15, max: 360, step: 15, value: 90,
        out: '90°', show: value => value + '°', whileDragging: showMoveLabels
      },
      {
        id: names.speedSlider, label: 'Speed', min: 1, max: 40, step: 1, value: Math.round(speed * 100),
        out: Math.round(speed * 100) + ' cm/s', show: value => value + ' cm/s',
        whenLetGo: value => order('speed ' + value)
      },
      {
        id: names.turnRateSlider, label: 'Turn rate', min: 5, max: 120, step: 5,
        value: Math.round(turnRate * degreesPerRadian),
        out: Math.round(turnRate * degreesPerRadian) + '°/s', show: value => value + '°/s',
        whenLetGo: value => order('turnrate ' + value)
      },
      {
        id: names.sphereGravitySlider, label: 'CG offset', min: 0, max: 6, step: 0.1, value: 0,
        out: '0.0 mm', show: value => Number(value).toFixed(1) + ' mm',
        // the center of gravity is ours alone: in Live this slider is turned off
        whenLetGo: value => command('cg ' + value)
      }
    ]
  }

  _render = () => {
    const { element, sliders } = this
    const rows = make('div', { class: 'rows' })

    makeCard(element, 'label-sphere-settings', 'Settings', 'sphere')

    sliders.forEach(one => {
      const { id, show, whileDragging, whenLetGo } = one
      const row = sliderRow(one)
      const slider = row.querySelector('input')

      slider.oninput = event => {
        byId(valueOf(id)).textContent = show(event.target.value)
        if (whileDragging) whileDragging()
      }

      if (whenLetGo) slider.onchange = event => whenLetGo(event.target.value)
      rows.appendChild(row)
    })

    element.appendChild(rows)
  }
}

export default SphereSettingsCard
