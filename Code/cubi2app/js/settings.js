// All the numbers the app starts with, in one place, so they can be changed by hand.
// Floor 2. It brings nothing in, and nothing here decides anything - they are only the numbers.
//
// Change a number here and the whole app starts with the new one. Nothing else has to be touched.
// The numbers marked FIRMWARE are the real cube's own: change them only together with the cube,
// or the app will no longer show what happens on the table.

const settings = {

  // ----- the bare cube on its corner (the firmware's balance.h) -------------------------------
  cube: {
    mass: 1.75,                // kg
    gravity: 9.81,             // m/s2
    inertia: 0.0082,           // how hard the cube is to turn about its own center
    wheelInertia: 5.9e-4,      // how hard one wheel is to turn
    twistFriction: 0.004,      // the tip rubbing as it twists on the floor
    airFriction: 0.0004,
    cornerTip: 0.1237,         // from the balance point out to the corner tip, in m (the 3D software)
    edgeTip: 0.1021,           // and to an edge
    wheelMax: 430,             // the fastest a wheel may turn, rad/s
    maxTorque: 0.22,           // FIRMWARE - what the motors can give, Nm
    fallDegrees: 25,           // tilted more than this and it is falling
    takeOverDegrees: 4,        // FIRMWARE - within this of its balance point, the corner takes over
    takeOverStill: 0.3         // and it has to be this quiet first, in seconds
  },

  // how hard the wheels answer. corner: saved from the 18 minute run. edge: out of the model.
  cubeGains: {
    corner: { angleGain: 7.82, rateGain: 0.6, wheelGain: 0.000356 },   // FIRMWARE
    edge: { angleGain: 7.13, rateGain: 0.775, wheelGain: 0.000352 }
  },

  // what the controller does while it stands
  balance: {
    learnRate: 0.05,           // how fast it moves its balance point while it stands
    twist: 0.3,                // how much it fights a turn around the corner it stands on
    spin: 0.3,                 // how much it lets the wheels unwind their own spin
    brakeGain: 0.001,          // on an edge, the two free wheels are slowed gently
    brakeMax: 0.03,
    learnBelowDegrees: 3       // it only learns while it stands quieter than this
  },

  // lowering from the corner onto an edge (the firmware's LOWER_RATE, LOWER_LAND, LOWER_MS)
  lowering: {
    rateDegrees: 15,           // FIRMWARE - how fast the point walks across, deg/s
    landDegrees: 4,            // FIRMWARE - this close to the edge's point, the edge takes over
    seconds: 4,                // FIRMWARE - longer than this and it gives up
    tiltStopDegrees: 40        // while lowering it is allowed to lean further than usual
  },

  // ----- the sphere (the firmware's sphere.h) ---------------------------------------------------
  sphere: {
    mass: 2.055,               // kg, the cube inside its shell
    gravity: 9.81,
    radius: 0.130,             // m
    cubeInertia: 0.011,
    wheelInertia: 5.9e-4,
    spinFriction: 0.002,       // the floor takes a little of the spin away
    wheelMax: 430,             // rad/s
    maxTorque: 0.189,          // FIRMWARE - Nm
    controlRadius: 0.130       // FIRMWARE - the radius the controller works with
  },

  sphereGains: {
    speed: 0.20,               // FIRMWARE
    place: 0.18,               // FIRMWARE
    heading: 0.06,             // FIRMWARE
    headingRate: 0.05,         // FIRMWARE
    unwind: 0.20,              // FIRMWARE - standing still, it unwinds the spin the wheels kept
    navel: 0.25                // FIRMWARE - coming to rest on a navel
  },

  sphereMoves: {
    speed: 0.05,               // how fast it rolls, m/s
    accel: 0.10,               // how fast it picks that up, m/s2
    turnRate: 0.5236,          // FIRMWARE - how fast it turns on the spot, rad/s (30 deg/s, as the firmware rounds it)
    tiltSettle: 1.0            // how fast it trusts gravity again, in seconds
  },

  // ----- the real Cubi over the Wi-Fi -----------------------------------------------------------
  link: {
    address: 'http://192.168.4.1',   // put /cube here while working with tools/serve.mjs
    askEvery: 100,                   // ms between one question and the next
    waitAtMost: 2500,                // ms - a cube that has not answered by now is not there
    freshEnough: 1000,               // ms - a reading older than this is not trusted
    askLaterWhileUpdating: 300       // ms - while a program is being sent, we ask less often
  },

  // ----- the 3D view -----------------------------------------------------------------------------
  view: {
    floorSize: 8000,           // the floor is 8 m across, in mm
    gridStep: 100,             // a square of the grid: 10 cm
    gridReach: 3000,           // the grid is drawn 3 m out in every direction
    gridPiece: 200,            // in pieces of 20 cm - see the comment in view3d.js
    trailMost: 3000,           // how many points of the trail are kept
    trailEvery: 4,             // a new point once the sphere has moved this far, in mm
    gravityDotTimes: 6         // the balance point is drawn this many times further out, to be seen
  },

  camera: {
    turn: -2.2,                // where it starts, in radians round the cube
    height: 0.5,               // and how high it looks from
    sphereDistance: 850,       // in mm
    cubeDistance: 700,
    lowest: 0.08,              // it never goes under the floor
    highest: 1.45,             // and never straight overhead
    nearest: 400,              // in mm
    farthest: 4000,
    zoomStep: 1.1,             // one notch of the wheel
    turnPerPixel: 0.008,
    heightPerPixel: 0.006
  },

  // ----- the chart of the wheels ------------------------------------------------------------------
  wheelsChart: {
    seconds: 20,               // how far back it shows
    everySecond: 10,           // how many points a second
    fullScale: 4500,           // the top and the bottom of the chart, in rpm
    wheelLimit: 4106           // what the real wheels can do - the dashed line
  },

  // ----- the route drawn on the floor --------------------------------------------------------------
  route: {
    straightenWithin: 25,      // the hand shakes: a bend smaller than this, in mm, is straightened out
    shortestPart: 50,          // parts shorter than this, in mm, are joined to the next one
    newPointEvery: 8,          // while drawing, a new point once the finger has moved this far, in mm
    smallestTurnDegrees: 2,    // a turn smaller than this is not worth a command
    longestPart: 500,          // one 'fwd' command never goes further than this, in cm
    waitBetween: 600           // ms between one command and the next, so the sphere can finish
  },

  // ----- the loop that draws every picture --------------------------------------------------------
  loop: {
    panelEvery: 0.1,           // the readings are written ten times a second, in seconds
    longestStep: 0.1,          // a picture is never asked to catch up more than this, in seconds
    smoothLive: 0.08,          // the real cube speaks 10 times a second - this smooths it out
    lookAtSphere: 90,          // how high the camera looks, in mm
    lookAtCube: 110
  },

  // ----- keeping the runs of the real cube ----------------------------------------------------------
  runs: {
    keepAt: '/runs/add',          // where tools/serve.mjs takes a run and writes it into data/runs.json
    keepLastAt: '/runs/last',     // and the last run's recording, into data/last-run.json
    readFrom: '../data/runs.json',
    readLastFrom: '../data/last-run.json',
    dumpWaitAtMost: 15000         // ms - a recording that has not ended by now is given up on
  },

  // ----- the simulation itself ----------------------------------------------------------------------
  simulation: {
    step: 0.004,               // one step of the physics, in seconds
    testWeightArm: 132         // where a test weight is taped for 'where', in mm
  }
}

export default settings
