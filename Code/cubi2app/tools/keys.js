// keys - the only place in the app that listens to the keyboard.
// One of the two places addEventListener lives, because the whole page has to be listened to.
//
//   new Keys()
//     .onAlways('Escape', closeTheWindows)   // even while something is being typed
//     .on('ArrowUp', goForward)              // only when nobody is typing
//     .start()
//
// It remembers which key does what for as long as the app is open, so it is a class.

// while the finger is in one of these, the keys belong to it and not to us
const whileTyping = ['INPUT', 'SELECT', 'TEXTAREA']

class Keys {
  #stop = new AbortController()

  constructor() {
    this.acts = new Map()        // key -> what to do, when nobody is typing
    this.alwaysActs = new Map()  // key -> what to do, typing or not
  }

  on = (key, act) => {
    this.acts.set(key, act)
    return this                  // it gives itself back, so the keys can be written one after another
  }

  onAlways = (key, act) => {
    this.alwaysActs.set(key, act)
    return this
  }

  start = () => {
    window.addEventListener('keydown', this.#pressed, { signal: this.#stop.signal })
    return this
  }

  // every listener goes away at once
  clear = () => {
    this.#stop.abort()
    this.#stop = new AbortController()
  }

  #pressed = event => {
    const always = this.alwaysActs.get(event.key)
    if (always && always(event) !== false) return

    if (whileTyping.includes(event.target?.tagName)) return

    const act = this.acts.get(event.key)
    if (!act) return

    // the page must not scroll under us when an arrow or the space bar is pressed
    event.preventDefault()
    act(event)
  }
}

export default Keys
