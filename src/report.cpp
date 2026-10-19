#include "chessscript.hpp"
#include <sstream>

namespace cs {
namespace {
std::string escapeHtml(const std::string& input) {
    std::string result;
    for (char c : input) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += c;
        }
    }
    return result;
}
}

std::string renderHtml(const Compilation& compilation, const std::string& source, const std::vector<Board>& states) {
    std::ostringstream out;
    out << R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ChessScript • Visual game</title>
<style>
:root { color-scheme: light; --ink:#152b29; --muted:#57716c; --bg:#f2f5ef; --panel:#fffdf8; --line:#d9e2d9; --accent:#196f59; --light:#e8dfca; --dark:#769988; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--ink); font-family:Inter,system-ui,-apple-system,"Segoe UI",sans-serif; line-height:1.45; }
.shell { max-width:1280px; margin:auto; padding:32px 24px 54px; }
.top { display:flex; align-items:flex-end; justify-content:space-between; gap:20px; border-bottom:1px solid var(--line); padding-bottom:22px; margin-bottom:28px; }
.eyebrow { text-transform:uppercase; font-size:12px; font-weight:800; letter-spacing:.17em; color:var(--accent); }
h1 { font-size:clamp(28px,4vw,42px); letter-spacing:-.045em; line-height:1.08; margin:8px 0 5px; }
.intro { color:var(--muted); margin:0; }
.count { color:var(--muted); font-size:14px; white-space:nowrap; }
.layout { display:grid; grid-template-columns:minmax(0,680px) minmax(290px,1fr); gap:26px; align-items:start; }
.card { background:var(--panel); border:1px solid var(--line); border-radius:18px; box-shadow:0 12px 32px rgba(19,50,41,.06); overflow:hidden; }
.board-card { padding:22px; }
.board-heading { display:flex; justify-content:space-between; align-items:baseline; gap:16px; margin-bottom:15px; }
.board-heading h2 { font-size:20px; letter-spacing:-.02em; margin:0; }
.status { color:var(--muted); font-size:14px; font-weight:600; text-align:right; }
.board { width:min(100%,580px); aspect-ratio:1; margin:auto; display:grid; grid-template-columns:repeat(8,1fr); border:1px solid #547766; border-radius:8px; overflow:hidden; box-shadow:0 9px 18px rgba(20,42,33,.16); }
.square { position:relative; display:grid; place-items:center; min-width:0; }
.square.light { background:var(--light); } .square.dark { background:var(--dark); }
.square.from::after,.square.to::after { content:""; position:absolute; inset:3px; border:4px solid #e9a72d; border-radius:4px; pointer-events:none; }
.square.from::after { border-color:#f0c872; border-style:dashed; }
.piece { font-family:"DejaVu Sans","Noto Sans Symbols 2","Segoe UI Symbol",serif; font-size:clamp(25px,5vw,55px); line-height:1; user-select:none; z-index:1; }
.piece.white { color:#fffaf0; -webkit-text-stroke:1px #33443d; text-shadow:0 2px 2px rgba(0,0,0,.65); }
.piece.black { color:#172b29; text-shadow:0 1px 1px rgba(255,255,255,.5); }
.coord { position:absolute; font-size:clamp(9px,1.2vw,12px); line-height:1; font-weight:800; opacity:.82; }
.rank { top:5px; left:5px; } .file { bottom:5px; right:5px; }
.light .coord { color:#406052; } .dark .coord { color:#f9f4e7; }
.controls { display:flex; align-items:center; gap:10px; margin-top:20px; }
.controls button { border:1px solid var(--line); background:#fff; color:var(--ink); border-radius:10px; padding:10px 16px; font:inherit; font-weight:750; cursor:pointer; }
.controls button:hover:not(:disabled),.move-button:hover { border-color:var(--accent); background:#eff7f1; }
.controls button:disabled { opacity:.4; cursor:default; }
.controls input { flex:1; min-width:60px; accent-color:var(--accent); }
.step { margin-top:10px; color:var(--muted); font-size:13px; text-align:center; }
.side { display:grid; gap:20px; }
.panel-header { padding:18px 20px 10px; } .panel-header h2 { font-size:18px; margin:0; }
.panel-header p { color:var(--muted); font-size:13px; margin:4px 0 0; }
.moves { list-style:none; padding:0 12px 12px; margin:0; max-height:380px; overflow:auto; }
.moves li { margin:4px 0; }
.move-button { width:100%; text-align:left; border:1px solid transparent; border-radius:10px; background:transparent; padding:10px 12px; color:var(--ink); font:inherit; cursor:pointer; display:flex; gap:14px; align-items:baseline; }
.move-button.current { border-color:#90bb9d; background:#e5f2e7; }
.move-number { font-size:12px; color:var(--muted); min-width:22px; font-weight:800; }
.move-label { font-weight:650; }
.source { margin:0 16px 18px; padding:14px 16px; border:1px solid var(--line); border-radius:10px; background:#f5f7f1; color:#28413b; overflow:auto; font:13px/1.65 ui-monospace,SFMono-Regular,Consolas,monospace; }
.legend { color:var(--muted); font-size:13px; margin:18px 0 0; text-align:center; }
.legend span { margin:0 5px; }
button:focus-visible,input:focus-visible { outline:3px solid #e9a72d; outline-offset:2px; }
@media (max-width:880px) { .layout { grid-template-columns:1fr; } .side { grid-template-columns:1fr 1fr; } }
@media (max-width:620px) { .shell { padding:18px 12px 30px; } .top { align-items:start; } .count { display:none; } .board-card { padding:12px; } .side { grid-template-columns:1fr; } .piece { font-size:clamp(25px,8vw,43px); } }
</style>
</head>
<body>
<main class="shell">
  <header class="top">
    <div><div class="eyebrow">ChessScript · game viewer</div><h1>)HTML";
    out << escapeHtml(compilation.ast.game.name);
    out << R"HTML(</h1><p class="intro">Follow the program one chess move at a time.</p></div>
    <div class="count">)HTML" << compilation.moves.size() << R"HTML( compiled moves</div>
  </header>
  <div class="layout">
    <section class="card board-card" aria-label="Chess board">
      <div class="board-heading"><h2 id="position-title">Initial position</h2><div class="status" id="turn-label">White to move</div></div>
      <div class="board" id="board" role="img" aria-label="Chess position"></div>
      <div class="controls">
        <button type="button" id="previous" aria-label="Previous move">← Previous</button>
        <input id="step-slider" type="range" min="0" max=")HTML" << compilation.moves.size() << R"HTML(" value="0" aria-label="Move number">
        <button type="button" id="next" aria-label="Next move">Next →</button>
      </div>
      <div class="step" id="step-label">Position 0 of )HTML" << compilation.moves.size() << R"HTML(</div>
      <p class="legend"><span>♔ White pieces</span> · <span>♚ Black pieces</span> · Gold outline marks the last move</p>
    </section>
    <div class="side">
      <section class="card"><div class="panel-header"><h2>Move sequence</h2><p>Select a move to inspect its board position.</p></div><ol class="moves" id="moves">
)HTML";
    for(std::size_t i=0;i<compilation.moves.size();++i) {
        const auto& move=compilation.moves[i];
        out << "<li><button class=\"move-button\" type=\"button\" data-step=\"" << i+1
            << "\" data-from=\"" << escapeHtml(move.from) << "\" data-to=\"" << escapeHtml(move.to)
            << "\"><span class=\"move-number\">" << i+1 << ".</span><span class=\"move-label\">"
            << escapeHtml(move.label) << "</span></button></li>\n";
    }
    out << R"HTML(</ol></section>
      <section class="card"><div class="panel-header"><h2>ChessScript source</h2><p>The moves shown above come from this program.</p></div><pre class="source">)HTML"
        << escapeHtml(source) << R"HTML(</pre></section>
    </div>
  </div>
</main>
<script>
const states = [)HTML";
    for(std::size_t s=0;s<states.size();++s) {
        if(s)out << ',';
        out << '[';
        for(std::size_t i=0;i<64;++i) {
            if(i)out << ',';
            out << static_cast<int>(states[s].cells[i]);
        }
        out << ']';
    }
    out << "];\nconst statuses = [";
    for(std::size_t i=0;i<states.size();++i) {
        if(i)out << ',';
        out << '"' << positionStatus(states[i]) << '"';
    }
    out << R"HTML(];
const glyphs = ['', '♙','♘','♗','♖','♕','♔','♟','♞','♝','♜','♛','♚'];
const names = ['empty','white pawn','white knight','white bishop','white rook','white queen','white king','black pawn','black knight','black bishop','black rook','black queen','black king'];
const board = document.getElementById('board');
const slider = document.getElementById('step-slider');
const buttons = [...document.querySelectorAll('.move-button')];
let current = 0;
function render(step) {
  current = Math.max(0, Math.min(step, states.length - 1));
  board.replaceChildren();
  const selected = buttons[current - 1];
  const from = selected?.dataset.from;
  const to = selected?.dataset.to;
  const state = states[current];
  const spoken = [];
  for (let rank = 7; rank >= 0; rank--) {
    for (let file = 0; file < 8; file++) {
      const name = 'abcdefgh'[file] + (rank + 1);
      const piece = state[rank * 8 + file];
      const tile = document.createElement('div');
      tile.className = 'square ' + ((rank + file) % 2 ? 'light' : 'dark');
      if (name === from) tile.classList.add('from');
      if (name === to) tile.classList.add('to');
      tile.setAttribute('aria-label', name + ': ' + names[piece]);
      if (piece) {
        const symbol = document.createElement('span');
        symbol.className = 'piece ' + (piece <= 6 ? 'white' : 'black');
        symbol.textContent = glyphs[piece];
        tile.append(symbol);
        spoken.push(names[piece] + ' on ' + name);
      }
      if (file === 0) { const coord = document.createElement('span'); coord.className = 'coord rank'; coord.textContent = rank + 1; tile.append(coord); }
      if (rank === 0) { const coord = document.createElement('span'); coord.className = 'coord file'; coord.textContent = 'abcdefgh'[file]; tile.append(coord); }
      board.append(tile);
    }
  }
  board.setAttribute('aria-label', 'Position ' + current + '. ' + spoken.join(', '));
  document.getElementById('position-title').textContent = current ? selected.querySelector('.move-label').textContent : 'Initial position';
  document.getElementById('turn-label').textContent = /Checkmate|Stalemate/.test(statuses[current])
    ? statuses[current] : (current % 2 ? 'Black' : 'White') + ' to move · ' + statuses[current];
  document.getElementById('step-label').textContent = 'Position ' + current + ' of ' + (states.length - 1);
  document.getElementById('previous').disabled = current === 0;
  document.getElementById('next').disabled = current === states.length - 1;
  slider.value = current;
  buttons.forEach((button,index) => { button.classList.toggle('current', index === current - 1); button.setAttribute('aria-current', index === current - 1 ? 'step' : 'false'); });
  selected?.scrollIntoView({block:'nearest'});
}
document.getElementById('previous').addEventListener('click', () => render(current - 1));
document.getElementById('next').addEventListener('click', () => render(current + 1));
slider.addEventListener('input', () => render(Number(slider.value)));
buttons.forEach(button => button.addEventListener('click', () => render(Number(button.dataset.step))));
document.addEventListener('keydown', event => {
  if (event.target.tagName === 'INPUT') return;
  if (event.key === 'ArrowLeft') render(current - 1);
  if (event.key === 'ArrowRight') render(current + 1);
});
render(0);
</script>
</body>
</html>
)HTML";
    return out.str();
}
}
