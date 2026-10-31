import { make } from '../tools/make.js'
import { makeCard } from './functions.js'
import names from './names.js'

class TelemetryCard {
  constructor() {
    this.element = document.createElement('section')

    // what they say at the start - the sphere's three
    this.readings = [
      { which: 0, label: 'x', value: '0.0', unit: 'cm' },
      { which: 1, label: 'y', value: '0.0', unit: 'cm' },
      { which: 2, label: 'heading', value: '0', unit: 'deg' }
    ]

    this.battery = [
      { id: names.volts, label: 'battery', unit: 'V' },
      { id: names.charge, label: 'charge', unit: '%' },
      { id: names.cubeState, label: 'state', unit: ' ' }
    ]
  }

  _render = () => {
    const { element, readings, battery } = this
    const row = make('div', { class: 'read' })
    const batteryRow = make('div', { class: 'read battery', id: names.batteryRow })

    makeCard(element, 'label-telemetry', 'Telemetry')
    batteryRow.hidden = true   // there is no battery in the simulation

    readings.forEach(reading => {
      const { which, label, value, unit } = reading
      const box = make('div')

      box.appendChild(make('span', { id: names.readingLabel(which) }, label))
      box.appendChild(make('strong', { id: names.readingValue(which) }, value))
      box.appendChild(make('span', { id: names.readingUnit(which) }, unit))
      row.appendChild(box)
    })

    battery.forEach(one => {
      const { id, label, unit } = one
      const box = make('div')

      box.appendChild(document.createTextNode(label))
      box.appendChild(make('strong', { id }, '-'))
      box.appendChild(document.createTextNode(unit))
      batteryRow.appendChild(box)
    })

    element.appendChild(row)
    element.appendChild(batteryRow)
  }
}

export default TelemetryCard
