import { make } from '../tools/make.js'

class HintCard {
  constructor(kind) {
    this.element = document.createElement('section')
    this.kind = kind

    // every piece is either bold or plain text, and they are put down one after the other
    this.hints = {
      sphere: [
        { bold: 'Try:' },
        { text: ' CG offset 0 mm - the moves land where you ask, and the sphere rests on any navel. 3-5 mm (about the real Cubi): turns still work, rolls lift the center of gravity, the wheels speed up to 4106 rpm and it rolls back towards where it hangs lowest. Keys ' },
        { bold: '↑ ↓ ← →' },
        { text: ', ' },
        { bold: 'space' },
        { text: '.' }
      ],
      cube: [
        { bold: 'Try:' },
        { text: ' Start (it balances on corner 0), then Push. On the corner, Edge X / Y / Z lowers it onto that edge; on an edge, Lift is your hand putting it back on the corner. Zero, Fix, Save, Spin, Twist, Learn: Settings. CG offset 2 mm: the wheels speed up, then \'learn\' moves the balance point and they slow down again. Fix or Zero set the point at once, Save keeps it. Stop: it falls.' }
      ]
    }
  }

  _render = () => {
    const { element, kind, hints } = this

    element.setAttribute('class', 'card hintcard hint')
    element.setAttribute('aria-label', 'What to try')
    element.setAttribute('data-kind', kind)
    if (kind === 'cube') element.hidden = true

    hints[kind].forEach(piece => {
      const { bold, text } = piece
      element.appendChild(bold ? make('b', {}, bold) : document.createTextNode(text))
    })
  }
}

export default HintCard
