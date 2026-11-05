const endOfTable = 0x06054b50   // the four bytes that mark the end of the table

// unzip - the files inside a .zip, as { name: bytes }. Our own, so the app needs no library for it.
//   const parts = await unzip(await file.arrayBuffer())
// A zip keeps a table of its files at its very end. The table says where each file starts, how
// long it is and how it is packed: 0 = as it is, 8 = squeezed ('deflate').
const unzip = async buffer => {
  // DataView reads numbers out of the bytes; true = the small byte first, as zip writes them
  const bytes = new DataView(buffer)
  const files = {}

  // the end of the table is in the last 22 bytes, or a little before them
  let end = buffer.byteLength - 22
  while (end >= 0 && bytes.getUint32(end, true) !== endOfTable) end--
  if (end < 0) throw new Error('not a cubi2 firmware file')

  const count = bytes.getUint16(end + 10, true)
  let place = bytes.getUint32(end + 16, true)

  for (let k = 0; k < count; k++) {
    const packing = bytes.getUint16(place + 10, true)
    const packedSize = bytes.getUint32(place + 20, true)
    const nameLength = bytes.getUint16(place + 28, true)
    const extraLength = bytes.getUint16(place + 30, true)
    const noteLength = bytes.getUint16(place + 32, true)
    const header = bytes.getUint32(place + 42, true)

    const name = new TextDecoder().decode(new Uint8Array(buffer, place + 46, nameLength))
    const start = header + 30 + bytes.getUint16(header + 26, true) + bytes.getUint16(header + 28, true)
    let data = new Uint8Array(buffer.slice(start, start + packedSize))

    // DecompressionStream - the browser itself opens what deflate squeezed
    if (packing === 8) {
      const opened = new Blob([data]).stream().pipeThrough(new DecompressionStream('deflate-raw'))
      data = new Uint8Array(await new Response(opened).arrayBuffer())
    } else if (packing !== 0) {
      throw new Error('unknown packing in the file')
    }

    files[name] = data
    place += 46 + nameLength + extraLength + noteLength
  }

  return files
}

export { unzip }
