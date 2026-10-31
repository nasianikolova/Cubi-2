import Keys from '../tools/keys.js'
import { make } from '../tools/make.js'
import { byId, showTerminal, showSettings, showRuns } from './functions.js'
import names from './names.js'
import state from './state.js'
import { view } from './view3d.js'
import { goSphere } from './commands.js'
import { route } from './route.js'
import { loop } from './loop.js'
import { runRecord } from './run-record.js'

import TopBar from './top-bar.js'
import Stage from './stage.js'
import DriveCard from './drive-card.js'
import NavelsCard from './navels-card.js'
import SphereSettingsCard from './sphere-settings-card.js'
import BalanceCard from './balance-card.js'
import CubeSimCard from './cube-sim-card.js'
import LinkCard from './link-card.js'
import HintCard from './hint-card.js'
import TelemetryCard from './telemetry-card.js'
import TerminalWindow from './terminal-window.js'
import SettingsWindow from './settings-window.js'
import RunsWindow from './runs-window.js'
import { wheelsCard } from './wheels-card.js'

const topBar = new TopBar()
const stage = new Stage()

// the left column: everything you press
const driveCard = new DriveCard()
const navelsCard = new NavelsCard()
const sphereSettingsCard = new SphereSettingsCard()
const balanceCard = new BalanceCard()
const cubeSimCard = new CubeSimCard()
const linkCard = new LinkCard()
const sphereHint = new HintCard('sphere')
const cubeHint = new HintCard('cube')

const telemetryCard = new TelemetryCard()

const terminalWindow = new TerminalWindow()
const settingsWindow = new SettingsWindow()
const runsWindow = new RunsWindow()

const leftColumn = [driveCard, navelsCard, sphereSettingsCard, balanceCard, cubeSimCard, linkCard, sphereHint, cubeHint]
const rightColumn = [telemetryCard, wheelsCard]
const everyPart = [topBar, stage, ...leftColumn, ...rightColumn, terminalWindow, settingsWindow, runsWindow]

everyPart.forEach(part => part._render())

const buildThePage = () => {
  const app = make('div', { class: 'app' })
  const left = make('div', { class: 'col' })
  const right = make('div', { class: 'col' })

  leftColumn.forEach(part => left.appendChild(part.element))
  rightColumn.forEach(part => right.appendChild(part.element))

  app.appendChild(left)
  app.appendChild(stage.element)
  app.appendChild(right)

  document.body.appendChild(topBar.element)
  document.body.appendChild(app)
  document.body.appendChild(runsWindow.element)
  document.body.appendChild(settingsWindow.element)
  document.body.appendChild(terminalWindow.element)
}

const wireTheKeys = () => {
  const press = id => () => byId(id).click()

  new Keys()
    .onAlways('Escape', () => {
      if (!terminalWindow.element.hidden) return showTerminal(false)
      if (!settingsWindow.element.hidden) return showSettings(false)
      if (!runsWindow.element.hidden) return showRuns(false)
      return false   
    })
    .on('ArrowUp', whenDrivingTheSphere(press(names.sphereForward)))
    .on('ArrowDown', whenDrivingTheSphere(press(names.sphereBack)))
    .on('ArrowLeft', whenDrivingTheSphere(press(names.sphereLeft)))
    .on('ArrowRight', whenDrivingTheSphere(press(names.sphereRight)))
    .on(' ', whenDrivingTheSphere(press(names.sphereHalt)))
    .start()
}

// the arrows belong to the sphere; on the bare cube they do nothing
const whenDrivingTheSphere = act => () => {
  if (state.kind === 'sphere') act()
}

// The 3D view needs Cubi's own parts out of the 3D software, so the app waits for them.
const start = async () => {
  const mesh = await (await fetch('../data/cubi-mesh.json')).json()

  view.start(byId(names.view), mesh)
  view.orbit.floor = route   // drawing a route on the floor comes before turning the camera

  goSphere()
  loop.start()

  // every run of the real cube that is kept goes straight into the Runs window
  runRecord.whenKept = runsWindow.takeNewRun
  runsWindow.load()
}

buildThePage()
wireTheKeys()
start()
