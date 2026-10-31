import * as THREE from '../lib/three.module.js'

const step = 0.005        // one unit of p is this many mm
const normalStep = 1 / 127

// the packed text of the file back into numbers
const unpack = text => Uint8Array.from(atob(text), letter => letter.charCodeAt(0)).buffer

// one part of the file -> a shape. shift moves the part so it sits where it belongs on the cube.
const meshPart = (part, shift = [0, 0, 0]) => {
  const packedCorners = new Int16Array(unpack(part.p))
  const packedNormals = new Int8Array(unpack(part.n))
  const triangles = new Uint32Array(unpack(part.i))

  const corners = new Float32Array(packedCorners.length)
  const normals = new Float32Array(packedNormals.length)

  for (let k = 0; k < packedCorners.length; k += 3) {
    corners[k] = packedCorners[k] * step - shift[0]
    corners[k + 1] = packedCorners[k + 1] * step - shift[1]
    corners[k + 2] = packedCorners[k + 2] * step - shift[2]
  }

  for (let k = 0; k < packedNormals.length; k++) normals[k] = packedNormals[k] * normalStep

  const shape = new THREE.BufferGeometry()
  shape.setAttribute('position', new THREE.BufferAttribute(corners, 3))
  shape.setAttribute('normal', new THREE.BufferAttribute(normals, 3))
  shape.setIndex(new THREE.BufferAttribute(triangles, 1))
  return shape
}

// The box and its covers: whatever faces INTO the box is painted dark - it is the inside of a closed
// box. The letters CUBI 2 are cut right through the covers, so through them that dark inside shows,
// just as on the real cube.
const darkInside = shape => {
  const corners = shape.attributes.position.array
  const normals = shape.attributes.normal.array
  const paint = new Float32Array(corners.length)

  for (let k = 0; k < corners.length; k += 3) {
    const far = Math.hypot(corners[k], corners[k + 1], corners[k + 2]) || 1
    // how much this corner looks toward the middle of the cube: -1 straight in, +1 straight out
    const inwards = (corners[k] * normals[k] + corners[k + 1] * normals[k + 1] + corners[k + 2] * normals[k + 2]) / far
    const light = inwards < -0.25 ? 0.13 : 1

    paint[k] = light
    paint[k + 1] = light
    paint[k + 2] = light
  }

  shape.setAttribute('color', new THREE.BufferAttribute(paint, 3))
  return shape
}

export { meshPart, darkInside }
