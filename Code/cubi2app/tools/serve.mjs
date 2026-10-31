import { createServer } from 'node:http'
import { readFile, writeFile, rename } from 'node:fs/promises'
import { extname, join, normalize, sep } from 'node:path'

const root = process.cwd()

// what was asked for on the line, or what we use when nothing was asked
const asked = (what, instead) => {
  const at = process.argv.indexOf('--' + what)
  return at > 0 ? process.argv[at + 1] : instead
}

const port = Number(asked('port', 8000))
const cubeAddress = asked('cube', 'http://192.168.4.1')
const waitAtMost = 2500

// what each kind of file is, so the browser reads it right
const kinds = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.woff2': 'font/woff2',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.webp': 'image/webp',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon'
}

// ----- keeping the runs ---------------------------------------------------------------------
// A page in a browser cannot write a file. So the app sends each run here, and this server writes
// it into data/runs.json - the one file Nasia opens by hand to add the note of what was changed.
// The last run's recording goes into data/last-run.json, which nobody needs to open.
// Only these two files can ever be written, and nothing else.

const runsFile = join(root, 'data', 'runs.json')
const lastRunFile = join(root, 'data', 'last-run.json')
const mostThatMayCome = 2 * 1024 * 1024   // bytes - a recording is far smaller than this

const readBody = request => new Promise((done, fail) => {
  const parts = []
  let size = 0
  request.on('data', part => {
    size += part.length
    if (size > mostThatMayCome) fail(new Error('too big'))
    parts.push(part)
  })
  request.on('end', () => done(Buffer.concat(parts).toString('utf8')))
  request.on('error', fail)
})

// written to a side file first and then moved over, so a half-written file is never left behind
const writeSafely = async (file, value) => {
  await writeFile(file + '.new', JSON.stringify(value, null, 2) + '\n')
  await rename(file + '.new', file)
}

const readRuns = async () => {
  try {
    return JSON.parse(await readFile(runsFile, 'utf8'))
  } catch {
    return { runs: [] }
  }
}

// one write after the other, never two at once
let lastWrite = Promise.resolve()
const oneAtATime = job => (lastWrite = lastWrite.then(job, job))

const keepRun = async (request, response) => {
  try {
    const run = JSON.parse(await readBody(request))

    await oneAtATime(async () => {
      // read again every time, so a note Nasia has just written by hand is kept
      const kept = await readRuns()
      kept.runs.push({ ...run, note: run.note ?? '' })
      await writeSafely(runsFile, kept)
    })

    console.log('run kept: ' + run.seconds + ' s on the ' + run.mode)
    response.writeHead(200, { 'Content-Type': 'text/plain' }).end('kept')
  } catch (trouble) {
    console.log('a run could not be kept: ' + trouble.message)
    response.writeHead(400, { 'Content-Type': 'text/plain' }).end('not kept')
  }
}

const keepLastRun = async (request, response) => {
  try {
    const recording = JSON.parse(await readBody(request))
    await oneAtATime(() => writeSafely(lastRunFile, recording))
    response.writeHead(200, { 'Content-Type': 'text/plain' }).end('kept')
  } catch (trouble) {
    console.log('the last run could not be kept: ' + trouble.message)
    response.writeHead(400, { 'Content-Type': 'text/plain' }).end('not kept')
  }
}

// everything the page asks for under /cube is passed on to the real cube, and its answer back
const passItOn = async (request, response) => {
  const url = cubeAddress + request.url.slice('/cube'.length)
  const body = request.method === 'POST' ? await new Promise(done => {
    const parts = []
    request.on('data', part => parts.push(part))
    request.on('end', () => done(Buffer.concat(parts)))
  }) : undefined

  try {
    const fromCube = await fetch(url, {
      method: request.method,
      body,
      headers: request.headers['content-type'] ? { 'Content-Type': request.headers['content-type'] } : undefined,
      signal: AbortSignal.timeout(waitAtMost)
    })
    const kind = fromCube.headers.get('content-type') ?? 'text/plain'

    response.writeHead(fromCube.status, { 'Content-Type': kind })
    response.end(Buffer.from(await fromCube.arrayBuffer()))
  } catch {
    console.log('the cube did not answer: ' + url)
    response.writeHead(502, { 'Content-Type': 'text/plain' }).end('the cube did not answer')
  }
}

const server = createServer(async (request, response) => {
  if (request.url.startsWith('/cube/')) return passItOn(request, response)
  if (request.method === 'POST' && request.url === '/runs/add') return keepRun(request, response)
  if (request.method === 'POST' && request.url === '/runs/last') return keepLastRun(request, response)

  const asked = decodeURIComponent(request.url.split('?')[0])
  const path = asked === '/' ? '/html/index.html' : asked
  const file = join(root, normalize(path))

  // nothing outside the app folder is ever served
  if (!file.startsWith(root + sep)) {
    response.writeHead(403).end('outside the app folder')
    return
  }

  try {
    const body = await readFile(file)
    // ?? - the plain kind only when the extension is not in the list
    const kind = kinds[extname(file)] ?? 'application/octet-stream'
    // nothing is kept by the browser: a file changed by hand is seen at the next look
    response.writeHead(200, { 'Content-Type': kind, 'Cache-Control': 'no-store' }).end(body)
  } catch {
    // catch with no error name - we do not need it, the path says enough
    console.log('not found: ' + path)
    response.writeHead(404).end('not found: ' + path)
  }
})

server.on('error', trouble => {
  if (trouble.code !== 'EADDRINUSE') throw trouble
  console.log('door ' + port + ' is taken by something else - try: node tools/serve.mjs --port ' + (port + 1))
  process.exit(1)
})

server.listen(port, () => {
  console.log(`Cubi2 app on http://localhost:${port}`)
  console.log(`the cube at ${cubeAddress} - in the app's Cubi card put: /cube`)
})
