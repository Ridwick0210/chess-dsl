const $ = (id) => document.getElementById(id);
const source = $('source');
const glyphs = ['', '♙', '♘', '♗', '♖', '♕', '♔', '♟', '♞', '♝', '♜', '♛', '♚'];
const names = ['empty', 'white pawn', 'white knight', 'white bishop', 'white rook', 'white queen', 'white king',
  'black pawn', 'black knight', 'black bishop', 'black rook', 'black queen', 'black king'];
const initialBoard = [4, 2, 3, 5, 6, 3, 2, 4, ...Array(8).fill(1), ...Array(32).fill(0),
  ...Array(8).fill(7), 10, 8, 9, 11, 12, 9, 8, 10];

let result = null;
let current = 0;
let selectedTab = 'tokens';
let compiledSource = null;
let activeError = null;
let selectedExpansion = null;
let selectedLine = null;
let relatedLine = null;

function textElement(tag, text, className = '') {
  const element = document.createElement(tag);
  element.textContent = text;
  element.className = className;
  return element;
}

function locationButton(label, location) {
  const button = textElement('button', label, 'location-button');
  button.type = 'button';
  button.addEventListener('click', () => focusLine(location.line, location.column));
  return button;
}

function updateLines() {
  const count = source.value.split('\n').length;
  $('line-numbers').replaceChildren(...Array.from({length: count}, (_, index) => {
    const line = textElement('div', String(index + 1), 'line-number');
    line.classList.toggle('selected', index + 1 === selectedLine);
    line.classList.toggle('related', index + 1 === relatedLine);
    return line;
  }));
}

function focusLine(line, column = 1) {
  if (!line || line < 1) return;
  const lines = source.value.split('\n');
  const start = lines.slice(0, line - 1).reduce((total, text) => total + text.length + 1, 0);
  const offset = start + Math.min(Math.max(0, column - 1), (lines[line - 1] || '').length);
  source.focus();
  source.setSelectionRange(offset, start + (lines[line - 1] || '').length);
  source.scrollTop = Math.max(0, (line - 4) * 23);
}

function drawBoard(position) {
  const board = $('board');
  board.replaceChildren();
  const previousMove = current > 0 ? result?.moves[current - 1] : null;
  const described = [];
  for (let rank = 7; rank >= 0; rank--) {
    for (let file = 0; file < 8; file++) {
      const coordinate = 'abcdefgh'[file] + (rank + 1);
      const piece = position.board[rank * 8 + file];
      const tile = document.createElement('div');
      tile.className = 'square ' + ((rank + file) % 2 ? 'light' : 'dark');
      if (coordinate === previousMove?.from) tile.classList.add('from');
      if (coordinate === previousMove?.to) tile.classList.add('to');
      const before = current > 0 ? result?.states[current - 1] : null;
      if (before && before.board[rank * 8 + file] !== piece) tile.classList.add('changed');
      if (activeError && current === result?.moves.length && activeError.squares?.includes(coordinate)) {
        tile.classList.add('error-square');
      }
      tile.dataset.square = coordinate;
      tile.setAttribute('aria-label', coordinate + ': ' + names[piece]);
      if (piece) {
        const symbol = document.createElement('span');
        symbol.className = 'piece ' + (piece <= 6 ? 'white' : 'black');
        symbol.textContent = glyphs[piece];
        tile.append(symbol);
        described.push(names[piece] + ' on ' + coordinate);
      }
      if (file === 0) {
        const number = document.createElement('span');
        number.className = 'coordinate rank';
        number.textContent = rank + 1;
        tile.append(number);
      }
      if (rank === 0) {
        const letter = document.createElement('span');
        letter.className = 'coordinate file';
        letter.textContent = 'abcdefgh'[file];
        tile.append(letter);
      }
      board.append(tile);
    }
  }
  board.setAttribute('aria-label', 'Position ' + current + '. ' + described.join(', '));
}

function showPosition(index) {
  const length = result?.moves.length || 0;
  current = Math.max(0, Math.min(index, length));
  const position = result?.states[current] || {board: initialBoard, turn: 'White', status: 'Initial position'};
  drawBoard(position);
  const move = result?.moves[current - 1];
  const atFailure = activeError && current === length;
  selectedLine = atFailure ? activeError.line : move?.line;
  relatedLine = atFailure ? activeError.use?.line : move?.use?.line;
  updateLines();
  $('source-link').hidden = !selectedLine;
  $('source-link').textContent = selectedLine ? `Source line ${selectedLine}${relatedLine ? ` · expanded at line ${relatedLine}` : ''}` : '';
  showStateDetails(position, current > 0 ? result?.states[current - 1] : null);
  if (move?.use) selectedExpansion = result.expansions.findIndex(expansion =>
    expansion.use.line === move.use.line && expansion.use.column === move.use.column);
  updatePipeline();
  $('position-slider').max = length;
  $('position-slider').value = current;
  $('previous').disabled = current === 0;
  $('next').disabled = current === length;
  $('position-label').textContent = atFailure
    ? 'Position before the failing statement · preview only'
    : current ? result.moves[current - 1].label : 'Starting position';
  $('turn-label').textContent = /Checkmate|Stalemate/.test(position.status)
    ? 'Game over' : position.turn + ' to move';
  $('status-badge').textContent = position.status;
  $('status-badge').classList.toggle('ended', /Checkmate|Stalemate/.test(position.status));
  document.querySelectorAll('.move-button').forEach((button, number) => {
    const selected = number === current - 1;
    button.classList.toggle('current', selected);
    button.setAttribute('aria-current', selected ? 'step' : 'false');
  });
}

function showStateDetails(position, before) {
  const container = $('state-details');
  container.replaceChildren();
  if (!result) return;
  const fields = [['turn', 'Turn'], ['castling', 'Castling (KQ white, kq black)'],
    ['enPassant', 'En passant'], ['halfmove', 'Halfmove clock'], ['fullmove', 'Fullmove']];
  for (const [key, label] of fields) {
    const changed = before && before[key] !== position[key];
    const value = changed ? `${before[key]} → ${position[key]}` : position[key];
    container.append(textElement('span', `${label}: ${value}`, changed ? 'state-changed' : ''));
  }
  if (before) {
    const changed = position.board.flatMap((piece, index) => piece !== before.board[index]
      ? [`${'abcdefgh'[index % 8]}${Math.floor(index / 8) + 1}: ${names[before.board[index]]} → ${names[piece]}`] : []);
    container.append(textElement('div', changed.join(' · '), 'board-diff'));
  }
}

function showMoves() {
  const list = $('moves');
  list.replaceChildren();
  if (!result?.moves.length) {
    const item = document.createElement('li');
    item.className = 'empty';
    item.textContent = 'No moves in this program.';
    list.append(item);
  } else {
    result.moves.forEach((move, index) => {
      const item = document.createElement('li');
      const button = document.createElement('button');
      button.className = 'move-button';
      button.type = 'button';
      const number = document.createElement('span');
      number.className = 'move-number';
      number.textContent = (index + 1) + '.';
      const label = document.createElement('span');
      label.className = 'move-label';
      label.textContent = move.label;
      button.append(number, label);
      button.addEventListener('click', () => showPosition(index + 1));
      item.append(button);
      list.append(item);
    });
  }
  $('move-count').textContent = (result?.moves.length || 0) + ' moves';
}

function pipelineText() {
  if (!result) return 'Compile a program to inspect its stages.';
  if (selectedTab === 'tokens') return result.tokens.map(token =>
    `${String(token.line).padStart(2)}:${String(token.column).padEnd(2)}  ${token.kind.padEnd(13)} ${token.text}`).join('\n');
  if (selectedTab === 'ast') return result.ast.join('\n');
  if (selectedTab === 'symbols') return result.symbols.length
    ? result.symbols.map(name => `opening  ${name}`).join('\n')
    : 'No named openings. Declare an opening and use it in the game.';
  return result.ir.map(op => {
    const operands = op.opcode === 'SET'
      ? `${op.a} ← ${names[Number(op.b)] || op.b} ${glyphs[Number(op.b)] || ''}`
      : [op.a, op.b, op.c].filter(Boolean).join(' ');
    return `${String(op.line).padStart(2)}  ${op.opcode.padEnd(9)} ${operands}`;
  }).join('\n');
}

function updatePipeline() {
  const output = $('pipeline-output');
  output.replaceChildren();
  if (result && selectedTab === 'ir') {
    const move = result.moves[current - 1];
    result.ir.forEach((op, index) => {
      const operands = op.opcode === 'SET' ? `${op.a} ← ${names[Number(op.b)]}`
        : [op.a, op.b, op.c].filter(Boolean).join(' ');
      const row = textElement('button', `${index + 1}.  L${op.line}  ${op.opcode.padEnd(9)} ${operands}`, 'ir-row');
      row.type = 'button';
      row.classList.toggle('current', Boolean(move && index >= move.irBegin && index < move.irEnd));
      row.addEventListener('click', () => {
        const owner = result.moves.findIndex(item => index >= item.irBegin && index < item.irEnd);
        if (owner >= 0) showPosition(owner + 1);
      });
      output.append(row);
    });
    if (!result.ir.length) output.textContent = 'No IR operations in this program.';
    const selected = output.querySelector('.current');
    if (selected) output.scrollTop = Math.max(0, selected.offsetTop - 12);
  } else if (result && selectedTab === 'expansions') {
    if (!result.expansions.length) output.textContent = 'No opening references in this game.';
    result.expansions.forEach((expansion, index) => {
      const block = textElement('div', '', 'expansion');
      block.classList.toggle('current', index === selectedExpansion);
      block.append(textElement('strong', `use ${expansion.name};`));
      block.append(locationButton(`Use at ${expansion.use.line}:${expansion.use.column}`, expansion.use));
      block.append(locationButton(`Definition at ${expansion.definition.line}:${expansion.definition.column}`, expansion.definition));
      const moves = result.moves.slice(expansion.firstMove, expansion.firstMove + expansion.moveCount);
      if (!moves.length) block.append(textElement('p', 'No validated moves in this expansion.'));
      moves.forEach((move, offset) => {
        const button = textElement('button', `L${move.line}: ${move.label}`, 'ir-row');
        button.type = 'button';
        button.addEventListener('click', () => showPosition(expansion.firstMove + offset + 1));
        block.append(button);
      });
      if (activeError?.use?.line === expansion.use.line && activeError?.use?.column === expansion.use.column)
        block.append(textElement('p', `Stopped at line ${activeError.line}: ${activeError.message}`, 'expansion-error'));
      output.append(block);
    });
  } else {
    output.textContent = pipelineText();
  }
  $('pipeline-count').textContent = result ? result.ir.length + ' IR operations' + (activeError ? ' · validated prefix' : '') : '—';
  document.querySelectorAll('.tab').forEach(tab => {
    const selected = tab.dataset.tab === selectedTab;
    tab.classList.toggle('active', selected);
    tab.setAttribute('aria-selected', String(selected));
  });
}

function showWarnings(warnings = []) {
  const panel = $('warnings');
  panel.replaceChildren();
  panel.hidden = !warnings.length;
  warnings.forEach(warning => panel.append(locationButton(
    `Warning at ${warning.line}:${warning.column}: ${warning.message}`, warning)));
}

function showError(error, payload = {}) {
  result = payload.preview || null;
  activeError = result ? error : null;
  showMoves();
  showPosition(result?.moves.length || 0);
  showWarnings(result?.warnings);
  $('game-title').textContent = result ? `${result.name} · diagnostic preview` : 'Chess board';
  const diagnostic = $('diagnostic');
  diagnostic.hidden = false;
  diagnostic.replaceChildren();
  const errors = payload.errors || [error];
  errors.forEach(item => {
    const entry = textElement('div', '', 'diagnostic-entry');
    entry.append(textElement('strong', (item.category || 'ERROR') +
      (item.line ? ` at line ${item.line}, column ${item.column || 1}` : '')));
    if (item.line) entry.append(locationButton(item.message, item));
    else entry.append(textElement('span', item.message || 'The program could not be compiled.'));
    if (item.use) entry.append(locationButton(
      `Expanded from use ${item.opening} at ${item.use.line}:${item.use.column}`, item.use));
    diagnostic.append(entry);
  });
  $('source-note').textContent = `Compilation stopped · ${errors.length} error${errors.length === 1 ? '' : 's'}`;
  selectedLine = error.line;
  relatedLine = error.use?.line;
  updateLines();
  focusLine(error.line, error.column);
}

async function compile() {
  const submittedSource = source.value;
  $('run').disabled = true;
  $('source-note').textContent = 'Compiling…';
  $('diagnostic').hidden = true;
  try {
    const response = await fetch('/api/compile', {
      method: 'POST',
      headers: {'Content-Type': 'text/plain; charset=utf-8'},
      body: submittedSource,
    });
    const payload = await response.json();
    if (source.value !== submittedSource) return;
    compiledSource = submittedSource;
    activeError = null;
    selectedExpansion = null;
    if (!payload.ok) { showError(payload.error || {}, payload); return; }
    result = payload;
    showWarnings(payload.warnings);
    $('game-title').textContent = payload.name;
    $('source-note').textContent = `Compiled ${payload.moves.length} moves successfully`;
    showMoves();
    updatePipeline();
    showPosition(payload.moves.length);
  } catch (error) {
    if (source.value === submittedSource) showError({category: 'SERVER', message: 'Could not reach the local compiler. Restart it with make serve.'});
  } finally {
    $('run').disabled = false;
  }
}

async function loadSample() {
  const response = await fetch('/api/sample');
  source.value = await response.text();
  updateLines();
  await compile();
}

function sourceChanged() {
  result = null;
  activeError = null;
  compiledSource = null;
  selectedExpansion = null;
  selectedLine = relatedLine = null;
  showMoves();
  showPosition(0);
  showWarnings();
  $('game-title').textContent = 'Chess board';
  $('source-note').textContent = 'Source changed · run to update the board';
  $('diagnostic').hidden = true;
}
source.addEventListener('input', sourceChanged);
source.addEventListener('click', () => {
  if (!result || source.value !== compiledSource) return;
  const prefix = source.value.slice(0, source.selectionStart).split('\n');
  const line = prefix.length;
  const column = prefix[prefix.length - 1].length + 1;
  const locations = [...result.moves.map(move => ({line: move.line, column: move.column})),
    ...result.expansions.map(expansion => expansion.use)].filter(location => location.line === line);
  const location = locations.filter(item => item.column <= column).sort((a, b) => b.column - a.column)[0]
    || locations.sort((a, b) => a.column - b.column)[0];
  if (!location) return;
  const expansionIndex = result.expansions.findIndex(item => item.use.line === line && item.use.column === location.column);
  if (expansionIndex >= 0) {
    const expansion = result.expansions[expansionIndex];
    selectedTab = 'expansions';
    if (expansion.moveCount) showPosition(expansion.firstMove + 1);
    selectedExpansion = expansionIndex;
    selectedLine = expansion.use.line;
    relatedLine = expansion.definition.line;
    updateLines();
    updatePipeline();
    return;
  }
  const matchingMoves = result.moves.map((move, index) => ({move, index})).filter(item => item.move.line === line && item.move.column === location.column);
  const match = matchingMoves.find(item => item.index === current - 1) || matchingMoves[0];
  if (match) showPosition(match.index + 1);
});
$('source-link').addEventListener('click', () => focusLine(selectedLine));
source.addEventListener('scroll', () => { $('line-numbers').scrollTop = source.scrollTop; });
source.addEventListener('keydown', event => {
  if (event.key === 'Tab') {
    event.preventDefault();
    const start = source.selectionStart;
    source.setRangeText('    ', start, source.selectionEnd, 'end');
    sourceChanged();
  }
  if (event.key === 'Enter' && (event.ctrlKey || event.metaKey)) {
    event.preventDefault();
    compile();
  }
});
$('run').addEventListener('click', compile);
$('reset-sample').addEventListener('click', loadSample);
$('previous').addEventListener('click', () => showPosition(current - 1));
$('next').addEventListener('click', () => showPosition(current + 1));
$('position-slider').addEventListener('input', event => showPosition(Number(event.target.value)));
document.querySelectorAll('.tab').forEach(tab => tab.addEventListener('click', () => {
  selectedTab = tab.dataset.tab;
  updatePipeline();
}));
document.addEventListener('keydown', event => {
  if (event.target === source || event.target.tagName === 'INPUT') return;
  if (event.key === 'ArrowLeft') showPosition(current - 1);
  if (event.key === 'ArrowRight') showPosition(current + 1);
});
showPosition(0);
updatePipeline();
loadSample().catch(() => showError({category: 'SERVER', message: 'Could not load the sample game. Restart the local server.'}));
