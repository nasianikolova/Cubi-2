// Reading the cube's recording - what it prints when it is asked to 'dump'.
// Floor 2. Plain functions: lines go in, numbers come out, nothing is remembered.
//
// What the cube prints, one line every 16 ms or so:
//   t s | tilt deg x y z | rate deg/s x y z | wheel rpm x y z | torque mNm x y z
//   1.429 | +1.25 -1.07 -0.26 | -21.1 -23.6 -24.8 | +2357 -4084 +2321 | +189 -189 -37
// and at the end:
//   end of dump

const header = 't s | tilt deg x y z | rate deg/s x y z | wheel rpm x y z | torque mNm x y z'
const footer = 'end of dump'

// is this line where a recording starts, or where it ends?
const isDumpStart = line => line.trim() === header
const isDumpEnd = line => line.trim() === footer

const threeNumbers = text => text.trim().split(/\s+/).map(Number)

// One line of the recording, or null when the line is not one.
const readDumpLine = line => {
  const parts = line.split('|')
  if (parts.length !== 5 || !/^\s*\d/.test(parts[0])) return null

  const reading = {
    time: Number(parts[0]),
    tilt: threeNumbers(parts[1]),
    rate: threeNumbers(parts[2]),
    wheels: threeNumbers(parts[3]),
    torque: threeNumbers(parts[4])
  }

  // a line that is only partly numbers is a broken line, and it is left out
  const all = [reading.time, ...reading.tilt, ...reading.rate, ...reading.wheels, ...reading.torque]
  if (all.length !== 13 || all.some(Number.isNaN)) return null

  return reading
}

// All the lines of a recording, in the order they came.
const readDump = lines => lines.map(readDumpLine).filter(reading => reading !== null)

// What the three big numbers beside the chart say about a recording.
const sumUp = readings => {
  if (!readings.length) return null

  const largest = list => Math.max(...list.map(Math.abs))

  return {
    // the cube counts from the moment it started balancing, so its last time is the time on the corner
    timeOnTheCorner: readings.at(-1).time,
    largestTilt: largest(readings.flatMap(reading => reading.tilt)),
    fastestWheel: largest(readings.flatMap(reading => reading.wheels))
  }
}

export { isDumpStart, isDumpEnd, readDumpLine, readDump, sumUp }
