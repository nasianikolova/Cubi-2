import state from './state.js'
import { command } from './commands.js'
import { setKind } from './modes.js'
import { sendLive } from './live-link.js'

// the three commands that are also a change of what we are looking at
const whatItLooksAt = line => {
  if (line === 'go sphere') return 'sphere'
  if (/^go (corner|edge [xyz])$/.test(line)) return 'cube'
  return null
}

const order = line => {
  const said = line.trim().toLowerCase()
  if (!said) return

  // in Live the real cube does it, and then tells us what it did - we follow, we do not decide
  if (state.liveOn) return sendLive(said)

  // in the simulation we change first, so the command lands where it belongs
  const kind = whatItLooksAt(said)
  if (kind && state.kind !== kind) setKind(kind, true)

  command(said)
}

export { order }
