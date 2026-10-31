// The math: vectors and 3 x 3 matrices - the same as the firmware's sphere.h and balance.h
// A matrix is nine numbers in a row: the first three are its top line.

const tiny = 1e-12                        // smaller than this counts as nothing
// the firmware's own number, so the app and the real cube agree to the last digit
const degreesPerRadian = 57.29578

const vector = (x, y, z) => ({ x, y, z })
const add = (a, b) => vector(a.x + b.x, a.y + b.y, a.z + b.z)
const subtract = (a, b) => vector(a.x - b.x, a.y - b.y, a.z - b.z)
const scale = (a, k) => vector(a.x * k, a.y * k, a.z * k)
const dot = (a, b) => a.x * b.x + a.y * b.y + a.z * b.z
const cross = (a, b) => vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x)
const length = a => Math.sqrt(dot(a, a))

// the same direction, one unit long. A vector of nothing is left as it is.
const unit = a => {
  const n = length(a)
  return n > tiny ? scale(a, 1 / n) : a
}

const component = (a, i) => i === 0 ? a.x : i === 1 ? a.y : a.z
const axisVector = i => vector(i === 0 ? 1 : 0, i === 1 ? 1 : 0, i === 2 ? 1 : 0)
// no turn at all - the cube sits straight. (In math books: the identity matrix.)
const noTurn = () => [1, 0, 0, 0, 1, 0, 0, 0, 1]

const rotate = (matrix, v) => vector(
  matrix[0] * v.x + matrix[1] * v.y + matrix[2] * v.z,
  matrix[3] * v.x + matrix[4] * v.y + matrix[5] * v.z,
  matrix[6] * v.x + matrix[7] * v.y + matrix[8] * v.z
)

// the other way round - the same turn undone
const rotateBack = (matrix, v) => vector(
  matrix[0] * v.x + matrix[3] * v.y + matrix[6] * v.z,
  matrix[1] * v.x + matrix[4] * v.y + matrix[7] * v.z,
  matrix[2] * v.x + matrix[5] * v.y + matrix[8] * v.z
)

const column = (matrix, c) => vector(matrix[c], matrix[3 + c], matrix[6 + c])

const setColumn = (matrix, c, v) => {
  matrix[c] = v.x
  matrix[3 + c] = v.y
  matrix[6 + c] = v.z
}

// The cube turns by the rate its own gyro reads. The matrix is changed in place.
// After the step the three columns are straightened again, or the rounding slowly bends them.
const rotateInBody = (matrix, rate, dt) => {
  let first = column(matrix, 0)
  let second = column(matrix, 1)
  const firstStep = subtract(scale(second, rate.z), scale(column(matrix, 2), rate.y))
  const secondStep = subtract(scale(column(matrix, 2), rate.x), scale(first, rate.z))

  first = unit(add(first, scale(firstStep, dt)))
  second = add(second, scale(secondStep, dt))
  second = unit(subtract(second, scale(first, dot(first, second))))

  setColumn(matrix, 0, first)
  setColumn(matrix, 1, second)
  setColumn(matrix, 2, cross(first, second))
}

// The cube turns by a small angle seen from the room, not from itself.
const rotateInWorld = (matrix, angle) => {
  for (let c = 0; c < 3; c++) {
    const v = column(matrix, c)
    setColumn(matrix, c, add(v, cross(angle, v)))
  }

  const first = unit(column(matrix, 0))
  let second = column(matrix, 1)
  second = unit(subtract(second, scale(first, dot(first, second))))

  setColumn(matrix, 0, first)
  setColumn(matrix, 1, second)
  setColumn(matrix, 2, cross(first, second))
}

// Three unknowns from three lines - Cramer's rule. Only for a 3 x 3 matrix.
const solve = (matrix, v) => {
  const [a, b, c, d, e, f, g, h, i] = matrix
  const determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)

  return vector(
    (v.x * (e * i - f * h) - b * (v.y * i - f * v.z) + c * (v.y * h - e * v.z)) / determinant,
    (a * (v.y * i - f * v.z) - v.x * (d * i - f * g) + c * (d * v.z - v.y * g)) / determinant,
    (a * (e * v.z - v.y * h) - b * (d * v.z - v.y * g) + v.x * (d * h - e * g)) / determinant
  )
}

// One number from the bell curve, so the simulation is not perfectly clean - the real
// sensors are not either. Box-Muller: two even numbers in, one bell-curve number out.
const gauss = () => {
  let u = 0
  let v = 0
  while (!u) u = Math.random()
  while (!v) v = Math.random()
  return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v)
}

const noise = k => scale(vector(gauss(), gauss(), gauss()), k)

// how far apart two directions are, in degrees
const angleDegrees = (a, b) => {
  const c = Math.max(-1, Math.min(1, dot(unit(a), unit(b))))
  return Math.acos(c) * degreesPerRadian
}

const vectorText = v => [v.x, v.y, v.z].map(x => (x >= 0 ? '+' : '') + x.toFixed(4)).join(' ')

// from one direction to another, the short way round the sphere. t goes 0 -> 1.
const blend = (a, b, t) => {
  const c = Math.max(-1, Math.min(1, dot(a, b)))
  const angle = Math.acos(c)
  if (angle < 1e-5) return b

  return unit(add(
    scale(a, Math.sin((1 - t) * angle) / Math.sin(angle)),
    scale(b, Math.sin(t * angle) / Math.sin(angle))
  ))
}

// the direction u, tipped over by so many degrees around the axis k
const tilted = (u, k, deg) => {
  const t = deg / degreesPerRadian
  return unit(add(scale(u, Math.cos(t)), scale(cross(k, u), Math.sin(t))))
}

export {
  vector, add, subtract, scale, dot, cross, length, unit, component, axisVector, noTurn,
  rotate, rotateBack, column, setColumn, rotateInBody, rotateInWorld, solve, noise,
  angleDegrees, vectorText, blend, tilted, degreesPerRadian
}
