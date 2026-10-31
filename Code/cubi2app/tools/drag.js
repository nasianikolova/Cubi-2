// drag - a small window is moved by its title bar and never goes off the page.
// One of the two places addEventListener lives, because a drag needs three listeners together.
//
//   new Drag(terminalWindow, titleBar).start()

const edge = 4   // how close to the side of the page the window may come, in pixels

class Drag {
  constructor(box, bar) {
    this.box = box
    this.bar = bar
    this.hold = null   // where inside the bar the finger took hold, while a drag is on
  }

  start = () => {
    const { bar } = this

    bar.addEventListener('pointerdown', this._take)
    bar.addEventListener('pointermove', this._move)
    bar.addEventListener('pointerup', this._let)
    bar.addEventListener('pointercancel', this._let)
  }

  _take = event => {
    const { box, bar } = this
    if (event.target.closest('button')) return   // a button on the bar is pressed, not dragged

    const place = box.getBoundingClientRect()
    this.hold = { x: event.clientX - place.left, y: event.clientY - place.top }
    bar.setPointerCapture(event.pointerId)       // the bar keeps the finger even outside itself
  }

  _move = event => {
    const { box, hold } = this
    if (!hold) return

    // the window stays inside the page: never further than its own size from the far side
    const left = Math.min(window.innerWidth - box.offsetWidth - edge, event.clientX - hold.x)
    const top = Math.min(window.innerHeight - box.offsetHeight - edge, event.clientY - hold.y)

    box.classList.add('moved')
    Object.assign(box.style, {
      left: Math.max(edge, left) + 'px',
      top: Math.max(edge, top) + 'px',
      right: 'auto'
    })
  }

  _let = () => {
    this.hold = null
  }
}

export default Drag
