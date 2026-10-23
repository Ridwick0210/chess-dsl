"""Review 3 integration cases: compile, IR replay, special moves, and input boundaries."""
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
BINARY = str(Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / 'build/chessscript').resolve())
sys.argv = sys.argv[:1]


def run_json(source):
    process = subprocess.run([BINARY, '--json', '-'], input=source,
                             capture_output=True, timeout=10)
    result = json.loads(process.stdout)
    return process, result


def index(square):
    return (int(square[1]) - 1) * 8 + ord(square[0]) - ord('a')


def assert_ir_replay(test, result):
    """Independently apply every emitted operation and compare each VM snapshot."""
    board = result['states'][0]['board'].copy()
    turn = result['states'][0]['turn']
    rights = result['states'][0]['castling']
    en_passant = result['states'][0]['enPassant']
    halfmove = result['states'][0]['halfmove']
    fullmove = result['states'][0]['fullmove']
    state_number = 0
    cursor = 0
    for move in result['moves']:
        test.assertEqual(move['irBegin'], cursor)
        test.assertGreater(move['irEnd'], cursor)
        for op in result['ir'][move['irBegin']:move['irEnd']]:
            if op['opcode'] == 'SET':
                board[index(op['a'])] = int(op['b'])
            elif op['opcode'] == 'STATE':
                turn = 'White' if op['a'] == 'WHITE' else 'Black'
                rights = ''.join(letter for letter, flag in zip('KQkq', op['b']) if flag == '1') or '-'
                en_passant = op['c']
            elif op['opcode'] == 'CLOCK':
                halfmove, fullmove = int(op['a']), int(op['b'])
            elif op['opcode'] == 'END_MOVE':
                state_number += 1
                state = result['states'][state_number]
                test.assertEqual((board, turn, rights, en_passant, halfmove, fullmove),
                                 (state['board'], state['turn'], state['castling'],
                                  state['enPassant'], state['halfmove'], state['fullmove']))
            else:
                test.fail(f"unexpected IR opcode {op['opcode']}")
        test.assertEqual(result['ir'][move['irEnd'] - 1]['opcode'], 'END_MOVE')
        cursor = move['irEnd']
    test.assertEqual(cursor, len(result['ir']))
    test.assertEqual(state_number + 1, len(result['states']))


class EndToEnd(unittest.TestCase):
    def valid(self, source):
        process, result = run_json(source.encode('utf-8'))
        self.assertEqual(process.returncode, 0, process.stderr.decode('utf-8', errors='replace'))
        self.assertTrue(result['ok'], result)
        assert_ir_replay(self, result)
        return result

    def test_both_sides_castle(self):
        result = self.valid('game BothCastle { move e2 -> e4; move e7 -> e5; '
                            'move g1 -> f3; move b8 -> c6; move f1 -> c4; '
                            'move g8 -> f6; castle king_side; move f8 -> e7; '
                            'move d2 -> d3; castle king_side; }')
        self.assertEqual(len(result['moves']), 10)
        self.assertEqual(result['states'][7]['castling'], 'kq')
        self.assertEqual(result['states'][10]['castling'], '-')
        final = result['states'][-1]['board']
        self.assertEqual((final[index('g1')], final[index('f1')]), (6, 4))
        self.assertEqual((final[index('g8')], final[index('f8')]), (12, 10))
        self.assertEqual((final[index('e1')], final[index('e8')]), (0, 0))

    def test_en_passant(self):
        result = self.valid('game EP { move e2 -> e4; move a7 -> a6; '
                            'move e4 -> e5; move d7 -> d5; capture e5 -> d6; }')
        self.assertEqual(result['states'][4]['enPassant'], 'd6')
        self.assertEqual(result['states'][5]['enPassant'], '-')
        final = result['states'][-1]['board']
        self.assertEqual((final[index('e5')], final[index('d5')], final[index('d6')]),
                         (0, 0, 1))

    def test_promotion_by_capture(self):
        result = self.valid('game Promotion { move a2 -> a4; move h7 -> h5; '
                            'move a4 -> a5; move h5 -> h4; move a5 -> a6; '
                            'move h4 -> h3; capture a6 -> b7; capture h3 -> g2; '
                            'promote b7 -> a8 as queen; }')
        final = result['states'][-1]['board']
        self.assertEqual((final[index('b7')], final[index('a8')]), (0, 5))
        self.assertEqual(result['moves'][-1]['label'], 'Promote b7 → a8 to queen')

    def test_empty_and_near_limit_program(self):
        empty = self.valid('game Empty {}')
        self.assertEqual((len(empty['moves']), len(empty['ir']), len(empty['states'])), (0, 0, 1))
        # The Studio accepts at most 64 KiB; a comment near that boundary is valid source.
        source = '#' + 'a' * 64000 + '\ngame Edge {}'
        self.assertLess(len(source.encode('utf-8')), 65536)
        self.assertEqual(self.valid(source)['name'], 'Edge')

    def test_non_ascii_code_is_a_structured_lexical_error(self):
        process, result = run_json('game Café {}'.encode('utf-8'))
        self.assertEqual(process.returncode, 1)
        self.assertFalse(result['ok'])
        self.assertEqual(result['error']['category'], 'LEXICAL')
        self.assertIn('non-ASCII', result['error']['message'])

    def test_server_rejects_invalid_encoding_and_oversized_source(self):
        spec = importlib.util.spec_from_file_location('chessscript_server', ROOT / 'web/server.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for source, expected_status in [(b'game \xff {}', 400), (b'a' * 65537, 413)]:
            handler = module.Handler.__new__(module.Handler)
            handler.path = '/api/compile'
            handler.headers = {'Content-Length': str(len(source))}
            handler.rfile = io.BytesIO(source)
            replies = []
            handler.send_bytes = lambda status, content_type, payload: replies.append((status, payload))
            handler.do_POST()
            self.assertEqual(len(replies), 1)
            self.assertEqual(replies[0][0], expected_status)
            self.assertFalse(json.loads(replies[0][1])['ok'])

    def test_long_valid_program(self):
        sequence = 'move g1 -> f3; move g8 -> f6; move f3 -> g1; move f6 -> g8;\n'
        result = self.valid('game Long {\n' + sequence * 100 + '}')
        self.assertEqual(len(result['moves']), 400)
        self.assertEqual(len(result['states']), 401)
        self.assertEqual(result['states'][0]['board'], result['states'][-1]['board'])

    def test_portable_html_viewer(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'viewer.html'
            process = subprocess.run(
                [BINARY, '--html', str(output), str(ROOT / 'examples/scholar_mate.chs')],
                capture_output=True, text=True, timeout=10,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            html = output.read_text(encoding='utf-8')
            self.assertIn('ScholarMate', html)
            self.assertIn('const states = [', html)
            self.assertIn('Checkmate', html)
            self.assertEqual(html.count('class="move-button"'), 7)
            self.assertIn('grid-template-rows:repeat(8,minmax(0,1fr))', html)


if __name__ == '__main__':
    unittest.main()
