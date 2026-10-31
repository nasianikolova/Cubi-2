// make - a new element from its tag, its attributes and its text. The one place the page is built from.
// make('button', { id: 'b-fwd', 'aria-pressed': 'false' }, 'Forward')

// attributes = {} - a default: called with no attributes, make does not break
const make = (tag, attributes = {}, text) => {
  const element = document.createElement(tag)

  // Object.entries turns { id: 'b-fwd' } into [['id', 'b-fwd']] - an object walked like a list
  Object.entries(attributes).forEach(([name, value]) => {
    // a value that is missing is left out. false stays - aria-pressed="false" says something.
    // without this line a forgotten value writes the word "undefined" into the page, quietly
    if (value === undefined || value === null) return
    element.setAttribute(name, String(value))
  })

  if (text !== undefined && text !== null) element.textContent = String(text)

  return element
}

export { make }
