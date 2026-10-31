import { make } from '../tools/make.js'
import { makeCard } from './functions.js'
import settings from './settings.js'
import { connect } from './live-link.js'
import names from './names.js'

const help = 'Join the Wi-Fi Cubi2 (password cubi2cubi), then Connect.'

class LinkCard {
  constructor() {
    this.element = document.createElement('section')
    this.address = settings.link.address
  }

  _render = () => {
    const { element, address } = this
    const form = make('form', { class: 'cmd', id: names.addressForm, autocomplete: 'off' })
    const input = make('input', { id: names.addressInput, value: address, 'aria-label': 'The cube\'s address' })
    const button = make('button', { type: 'submit' }, 'Connect')
    const howItIsGoing = make('div', { class: 'linkstate', id: names.linkState }, help)

    element.setAttribute('id', names.linkCard)
    makeCard(element, 'label-link', 'Cubi link')
    element.hidden = true

    // a form, so Enter in the box connects as well as the button does
    form.onsubmit = event => {
      event.preventDefault()
      connect(input.value)
    }

    form.appendChild(input)
    form.appendChild(button)
    element.appendChild(form)
    element.appendChild(howItIsGoing)
  }
}

export default LinkCard
