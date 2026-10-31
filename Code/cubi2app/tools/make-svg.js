// makeSvg - the same as make, for the shapes of a drawing: svg, line, path, circle, rect, text.
// makeSvg('circle', { cx: 40, cy: 20, r: 6, class: 'dot' })
//
// Why a second one: the shapes of an SVG drawing belong to another family of tags than the
// page (its 'namespace'). document.createElement('circle') makes a circle that never shows.

const svgNamespace = 'http://www.w3.org/2000/svg'

const makeSvg = (tag, attributes = {}, text) => {
  const shape = document.createElementNS(svgNamespace, tag)

  Object.entries(attributes).forEach(([name, value]) => {
    if (value === undefined || value === null) return
    shape.setAttribute(name, String(value))
  })

  if (text !== undefined && text !== null) shape.textContent = String(text)

  return shape
}

export { makeSvg }
