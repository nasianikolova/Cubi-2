// The names of the places on the page, in one list.
// Floor 2. It brings nothing in - it is only the names.
//
// Why this file: one file makes a button, another one lights it up. If each of them wrote the
// name out by hand, the day one of them changed its mind the other would quietly stop finding
// it - and nothing would say so. Here the name is written once and both read it from here.
//
// The rule: a name that only one file ever uses may stay inside that file. The moment a second
// file needs it, it moves here.

const names = {
  // the bar at the top
  sphereButton: 'button-sphere',
  cubeButton: 'button-cube',
  simulationButton: 'button-simulation',
  liveButton: 'button-live',
  runsButton: 'button-runs',
  settingsButton: 'button-settings',
  terminalButton: 'button-terminal',
  subtitle: 'subtitle',
  modeSwitch: 'mode-switch',

  // the stage in the middle
  view: 'view',
  statePill: 'state-pill',
  message: 'message',
  legend: 'legend',
  askBar: 'ask-bar',          // the cube's question - still to come

  // the small windows
  terminal: 'terminal',
  settings: 'settings',
  runs: 'runs',
  log: 'log',
  commandInput: 'input-command',

  // the telemetry card. The three readings are label-0 / value-0 / unit-0 and so on.
  readingLabel: which => 'label-' + which,
  readingValue: which => 'value-' + which,
  readingUnit: which => 'unit-' + which,
  batteryRow: 'battery-row',
  volts: 'read-volts',
  charge: 'read-battery',
  cubeState: 'read-cube-state',

  // the Cubi link card
  linkCard: 'linkcard',
  linkState: 'link-state',
  addressForm: 'address-form',
  addressInput: 'address-input',

  // the Drive card
  sphereGo: 'sphere-go',
  sphereFront: 'sphere-front',
  sphereForward: 'sphere-forward',
  sphereBack: 'sphere-back',
  sphereLeft: 'sphere-left',
  sphereRight: 'sphere-right',
  sphereHalt: 'sphere-halt',

  // the navels card
  navels: 'navels',

  // the route drawn on the floor
  routeDraw: 'route-draw',
  routeGo: 'route-go',
  routeClear: 'route-clear',
  routeInfo: 'route-info',

  // the Balance card
  cubePad: 'cube-pad',
  cubeGo: 'cube-go',
  cubeLift: 'cube-lift',
  cubePush: 'cube-push',
  cubeEdge: axis => 'cube-edge-' + axis,

  // the sliders
  distanceSlider: 'slider-distance',
  angleSlider: 'slider-angle',
  speedSlider: 'slider-speed',
  turnRateSlider: 'slider-turn-rate',
  sphereGravitySlider: 'slider-sphere-gravity',
  cubeGravitySlider: 'slider-cube-gravity',

  // the chart of the wheels
  chart: 'chart',
  wheelKey: axis => 'key-' + axis
}

// where a slider writes its number: always its own name with -value after it
const valueOf = id => id + '-value'

export default names
export { valueOf }
