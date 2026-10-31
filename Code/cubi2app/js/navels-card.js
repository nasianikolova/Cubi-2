import { make } from '../tools/make.js'
import { makeCard } from './functions.js'
import { order } from './orders.js'
import { makeSureItIsRunning } from './drive-card.js'
import names from './names.js'

class NavelsCard {
  constructor() {
    this.element = document.createElement('section')

    // the three bits of the number say which way each corner leans: 0 is -x -y -z, 7 is +x +y +z
    this.navels = [
      { navel: 0, title: 'navel 0: -x -y -z, the three wheels\' corner' },
      { navel: 1 }, { navel: 2 }, { navel: 3 }, { navel: 4 }, { navel: 5 }, { navel: 6 },
      { navel: 7, title: 'navel 7: +x +y +z' }
    ]
  }

  _render = () => {
    const { element, navels } = this
    const row = make('div', { class: 'conns', id: names.navels })

    makeCard(element, 'label-navels', 'Rest on a navel', 'sphere')

    navels.forEach(one => {
      const { navel, title } = one
      const button = make('button', { 'data-navel': navel, title }, String(navel))

      button.onclick = () => {
        makeSureItIsRunning()
        order('navel ' + navel)
      }

      row.appendChild(button)
    })

    element.appendChild(row)
  }
}

export default NavelsCard
