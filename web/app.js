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

function updateLines() {
  const count = source.value.split('\n').length;
  $('line-numbers').textContent = Array.from({length: count}, (_, index) => index + 1).join('\n');
}

function focusLine(line, column = 1) {
  if (!line || line < 1) return;
  const lines = source.value.split('\n');
  const start = lines.slice(0, line - 1).reduce((total, text) => total + text.length + 1, 0);
  const offset = start + Math.min(Math.max(0, column - 1), (lines[line - 1] || '').length);
  source.focus();
  source.setSelectionRange(offset, Math.min(offset + 1, source.value.length));
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
  $('position-slider').max = length;
  $('position-slider').value = current;
  $('previous').disabled = current === 0;
  $('next').disabled = current === length;
  $('position-label').textContent = current ? result.moves[current - 1].label : 'Starting position';
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
  $('pipeline-output').textContent = pipelineText();
  $('pipeline-count').textContent = result ? result.ir.length + ' IR operations' : '—';
  document.querySelectorAll('.tab').forEach(tab => {
    const selected = tab.dataset.tab === selectedTab;
    tab.classList.toggle('active', selected);
    tab.setAttribute('aria-selected', String(selected));
  });
}

function showError(error) {
  result = null;
  showMoves();
  showPosition(0);
  updatePipeline();
  $('game-title').textContent = 'Chess board';
  const diagnostic = $('diagnostic');
  diagnostic.hidden = false;
  diagnostic.replaceChildren();
  const title = document.createElement('strong');
  title.textContent = (error.category || 'ERROR') + (error.line ? ` at line ${error.line}, column ${error.column || 1}` : '');
  const message = document.createElement('span');
  message.textContent = error.message || 'The program could not be compiled.';
  diagnostic.append(title, message);
  $('source-note').textContent = 'Compilation stopped';
  focusLine(error.line, error.column);
}

async function compile() {
  $('run').disabled = true;
  $('source-note').textContent = 'Compiling…';
  $('diagnostic').hidden = true;
  try {
    const response = await fetch('/api/compile', {
      method: 'POST',
      headers: {'Content-Type': 'text/plain; charset=utf-8'},
      body: source.value,
    });
    const payload = await response.json();
    if (!payload.ok) { showError(payload.error || {}); return; }
    result = payload;
    $('game-title').textContent = payload.name;
    $('source-note').textContent = `Compiled ${payload.moves.length} moves successfully`;
    showMoves();
    updatePipeline();
    showPosition(payload.moves.length);
  } catch (error) {
    showError({category: 'SERVER', message: 'Could not reach the local compiler. Restart it with make serve.'});
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

source.addEventListener('input', () => {
  updateLines();
  $('source-note').textContent = 'Source changed · run to update the board';
  $('diagnostic').hidden = true;
});
source.addEventListener('scroll', () => { $('line-numbers').scrollTop = source.scrollTop; });
source.addEventListener('keydown', event => {
  if (event.key === 'Tab') {
    event.preventDefault();
    const start = source.selectionStart;
    source.setRangeText('    ', start, source.selectionEnd, 'end');
    updateLines();
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
