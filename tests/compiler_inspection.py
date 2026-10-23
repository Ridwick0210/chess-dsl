"""Compiler contract checks for diagnostics, source mapping, and expansion traces."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

BINARY = str(Path(sys.argv[1] if len(sys.argv) > 1 else './build/chessscript').resolve())
sys.argv = sys.argv[:1]


def compile_source(source, ok=True):
    run = subprocess.run([BINARY, '--json', '-'], input=source, text=True,
                         capture_output=True, timeout=5)
    result = json.loads(run.stdout)
    assert result['ok'] is ok, result
    assert run.returncode == (0 if ok else 1), run
    return result


class CompilerInspection(unittest.TestCase):
    def test_multiple_syntax_errors_and_no_execution(self):
        result = compile_source('game Bad {\n move e2 e4;\n move e7 e5;\n}', False)
        self.assertEqual([error['line'] for error in result['errors']], [2, 3])
        self.assertTrue(all(error['category'] == 'SYNTAX' for error in result['errors']))
        self.assertNotIn('preview', result)

    def test_missing_semicolon_keeps_next_statement(self):
        result = compile_source('game Bad {\n move e2 -> e4\n move e7 e5;\n}', False)
        self.assertEqual(len(result['errors']), 2)
        self.assertIn("expected ';'", result['errors'][0]['message'])
        self.assertIn("expected '->'", result['errors'][1]['message'])

    def test_missing_brace_and_recovery_bound(self):
        for source in ['game Bad {', 'opening A { move e2 e4; game B { move e7 e5; }',
                       'game Bad {' + 'move ;' * 200]:
            result = compile_source(source, False)
            self.assertLessEqual(len(result['errors']), 51)
            self.assertNotIn('preview', result)

    def test_grammar_and_lexical_errors_still_rejected(self):
        for source in ['opening A { use A; } game G {}', 'game A {} game B {}',
                       'game G { move z2 -> e4; }', 'game G { @ }']:
            compile_source(source, False)

    def test_prefix_preview_does_not_execute_failed_move(self):
        result = compile_source('game Bad { move e2 -> e4; move e7 -> e4; }', False)
        preview = result['preview']
        self.assertEqual(len(preview['moves']), 1)
        self.assertEqual(len(preview['states']), 2)
        self.assertEqual(preview['states'][-1]['turn'], 'Black')
        self.assertEqual(preview['states'][-1]['board'][52], 7)  # e7 pawn stays put
        self.assertEqual(preview['states'][-1]['board'][28], 1)  # e4 white pawn
        self.assertEqual(result['error']['squares'], ['e7', 'e4'])

    def test_blocker_and_king_squares(self):
        blocked = compile_source('game Bad { move f1 -> b5; }', False)
        self.assertIn('e2', blocked['error']['squares'])
        self.assertIn('blocked at e2', blocked['error']['message'])
        self.assertEqual(len(blocked['preview']['states']), 1)
        king = compile_source('game Bad { move e2 -> e4; move d7 -> d5; '
                              'move f1 -> b5; move a7 -> a6; }', False)
        self.assertIn('e8', king['error']['squares'])
        self.assertIn('king in check at e8', king['error']['message'])
        self.assertEqual(len(king['preview']['moves']), 3)
        pawn = compile_source('game Bad { move b1 -> c3; move a7 -> a6; move c2 -> c4; }', False)
        self.assertIn('blocked at c3', pawn['error']['message'])

    def test_pawn_explanation(self):
        result = compile_source('game Bad { move e2 -> e5; }', False)
        self.assertIn('pawns advance one square', result['error']['message'])

    def test_opening_definition_and_use_error_locations(self):
        result = compile_source('opening Bad {\n move e2 -> e5;\n}\ngame G {\n use Bad;\n}', False)
        self.assertEqual(result['error']['line'], 2)
        self.assertEqual(result['error']['use']['line'], 5)
        self.assertEqual(result['error']['opening'], 'Bad')
        self.assertEqual(result['preview']['expansions'][0]['moveCount'], 0)

    def test_repeated_expansions_have_distinct_instruction_ranges(self):
        result = compile_source('opening Cycle {\n move g1 -> f3; move g8 -> f6;\n'
                                ' move f3 -> g1; move f6 -> g8;\n}\n'
                                'game G {\n use Cycle;\n use Cycle;\n}')
        self.assertEqual([item['use']['line'] for item in result['expansions']], [6, 7])
        self.assertEqual([item['firstMove'] for item in result['expansions']], [0, 4])
        self.assertEqual([item['moveCount'] for item in result['expansions']], [4, 4])
        cursor = 0
        for move in result['moves']:
            self.assertEqual(move['irBegin'], cursor)
            cursor = move['irEnd']
            self.assertEqual(result['ir'][cursor - 1]['opcode'], 'END_MOVE')
            self.assertEqual(move['opening'], 'Cycle')
        self.assertEqual(cursor, len(result['ir']))
        self.assertEqual(result['states'][0]['board'], result['states'][-1]['board'])
        self.assertEqual(result['states'][-1]['halfmove'], 8)
        self.assertEqual(result['states'][-1]['fullmove'], 5)

    def test_warnings_and_suggestions(self):
        result = compile_source('opening Spare {} game G {}')
        self.assertEqual(len(result['warnings']), 1)
        self.assertIn('never used', result['warnings'][0]['message'])
        result = compile_source('opening Start {} game G { use Start; }')
        self.assertEqual(result['warnings'], [])
        self.assertEqual(result['expansions'][0]['moveCount'], 0)
        result = compile_source('opening Start {} game G { use Strat; }', False)
        self.assertIn("did you mean 'Start'?", result['error']['message'])
        tied = compile_source('opening Cat {} opening Bat {} game G { use Hat; }', False)
        self.assertNotIn('did you mean', tied['error']['message'])

    def test_state_metadata(self):
        result = compile_source('game G { move e2 -> e4; move g8 -> f6; }')
        self.assertEqual(result['states'][0]['castling'], 'KQkq')
        self.assertEqual(result['states'][1]['enPassant'], 'e3')
        self.assertEqual(result['states'][2]['enPassant'], '-')
        self.assertEqual(result['states'][2]['halfmove'], 1)

    def test_cli_reports_warnings_and_multiple_errors(self):
        run = subprocess.run([BINARY, '-'], input='opening Spare {} game G {}',
                             text=True, capture_output=True, timeout=5)
        self.assertEqual(run.returncode, 0)
        self.assertIn('WARNING at', run.stderr)
        run = subprocess.run([BINARY, '-'], input='game G { move e2 e4; move e7 e5; }',
                             text=True, capture_output=True, timeout=5)
        self.assertEqual(run.returncode, 1)
        self.assertEqual(run.stderr.count('SYNTAX error'), 2)


if __name__ == '__main__':
    unittest.main()
