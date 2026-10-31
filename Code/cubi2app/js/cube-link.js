import settings from './settings.js'

const waitAtMost = settings.link.waitAtMost   // ms - a cube that has not answered by now is not there

// a cube that is not there answers with nothing at all
const reached = answer => {
  if (answer.status === 0) throw new Error(answer.text || 'no connection')
  return answer
}

const get = async url => {
  if (window.cubiLink) return reached(await window.cubiLink.get(url))

  // AbortSignal.timeout - the asking gives up by itself, so the app never hangs on a dead cube
  const answer = await fetch(url, { cache: 'no-store', signal: AbortSignal.timeout(waitAtMost) })
  return { ok: answer.ok, status: answer.status, text: await answer.text() }
}

const post = async (url, body, type) => {
  if (window.cubiLink) return reached(await window.cubiLink.post(url, body, type))

  const answer = await fetch(url, {
    method: 'POST',
    body: body ?? '',
    headers: { 'Content-Type': type ?? 'text/plain' },
    cache: 'no-store',
    signal: AbortSignal.timeout(waitAtMost)
  })
  return { ok: answer.ok, status: answer.status, text: await answer.text() }
}

// What the cube is doing now. sequence is the last line we already have, so it only sends the new ones.
// Comes back as { sequence, age, log, telemetry } - age is how old its reading is, in ms.
const askCube = async (address, sequence) => {
  const answer = await get(address + '/t?s=' + sequence)
  const said = JSON.parse(answer.text)

  return {
    sequence: said.seq,
    age: said.age,
    log: said.log ?? [],
    telemetry: said.t
  }
}

// one command, exactly as it would be typed in the terminal
const sendCommand = (address, command) => get(address + '/c?x=' + encodeURIComponent(command))

// a new program for the cube
const sendProgram = (address, what, name, bytes) =>
  post(address + '/fw/t?op=' + encodeURIComponent(what) + '&n=' + encodeURIComponent(name),
    bytes, 'application/octet-stream')

// The address as the cube wants it: with http:// in front and no slash at the end.
// An address that starts with a slash, such as /cube, is on our own server already - while we
// work, tools/serve.mjs passes everything under /cube on to the real cube. It is left as it is.
const tidyAddress = address => {
  const clean = address.trim().replace(/\/+$/, '')

  if (clean.startsWith('/')) return clean
  return /^https?:\/\//.test(clean) ? clean : 'http://' + clean
}

export { get, post, askCube, sendCommand, sendProgram, tidyAddress }
