#include "chessscript.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>

namespace {
struct Options {
    bool tokens = false;
    bool ast = false;
    bool ir = false;
    bool run = true;
    bool json = false;
    std::string sourcePath;
    std::string htmlPath;
};

void usage() {
    std::cout << "ChessScript compiler and chess VM\n"
              << "Usage: chessscript [--tokens] [--ast] [--ir] [--no-run] "
                 "[--html output.html] file.chs\n"
              << "       chessscript --json - < file.chs\n";
}

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--tokens") options.tokens = true;
        else if (argument == "--ast") options.ast = true;
        else if (argument == "--ir") options.ir = true;
        else if (argument == "--no-run") options.run = false;
        else if (argument == "--json") options.json = true;
        else if (argument == "--html") {
            if (i + 1 >= argc) throw std::invalid_argument("--html requires an output path");
            options.htmlPath = argv[++i];
        } else if (argument != "-" && !argument.empty() && argument[0] == '-') {
            throw std::invalid_argument("unknown option: " + argument);
        } else if (options.sourcePath.empty()) {
            options.sourcePath = argument;
        } else {
            throw std::invalid_argument("only one source file is accepted");
        }
    }
    if (options.sourcePath.empty()) throw std::invalid_argument("a source file is required");
    return options;
}

std::string readSource(const std::string& path) {
    std::ostringstream buffer;
    if (path == "-") {
        buffer << std::cin.rdbuf();
    } else {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("cannot open source file: " + path);
        buffer << input.rdbuf();
    }
    return buffer.str();
}

void printTokens(const std::string& source) {
    std::cout << "TOKENS\n";
    for (const auto& token : cs::lex(source)) {
        std::cout << token.where.line << ':' << token.where.column << "  "
                  << cs::tokenName(token.kind) << "  " << token.text << '\n';
    }
}

void printStatement(const cs::Statement& statement) {
    std::visit([](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, cs::Move>)
            std::cout << "  Move(" << node.from << ", " << node.to << ")\n";
        else if constexpr (std::is_same_v<T, cs::Capture>)
            std::cout << "  Capture(" << node.from << ", " << node.to << ")\n";
        else if constexpr (std::is_same_v<T, cs::Castle>)
            std::cout << "  Castle(" << (node.kingSide ? "king_side" : "queen_side") << ")\n";
        else if constexpr (std::is_same_v<T, cs::Promote>)
            std::cout << "  Promote(" << node.from << ", " << node.to << ", " << node.piece << ")\n";
        else
            std::cout << "  Use(" << node.name << ")\n";
    }, statement);
}

void printAst(const cs::Program& program) {
    for (const auto& opening : program.openings) {
        std::cout << "AST opening " << opening.name << '\n';
        for (const auto& statement : opening.statements) printStatement(statement);
    }
    std::cout << "AST game " << program.game.name << '\n';
    for (const auto& statement : program.game.statements) printStatement(statement);
}

void printIr(const std::vector<cs::Instruction>& instructions) {
    std::cout << "CHESS IR\n";
    for (const auto& instruction : instructions) {
        std::cout << "  " << instruction.opcode << ' ' << instruction.a << ' '
                  << instruction.b << ' ' << instruction.c << "  @"
                  << instruction.where.line << ':' << instruction.where.column << '\n';
    }
}
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            usage();
            return 0;
        }
    }

    Options options;
    try {
        options = parseOptions(argc, argv);
        const std::string source = readSource(options.sourcePath);
        if (options.json) {
            const auto compilation = cs::compile(source);
            const auto states = cs::executeTrace(compilation.ir);
            std::cout << cs::renderJson(compilation, source, states) << '\n';
            return 0;
        }

        if (options.tokens) printTokens(source);
        const auto compilation = cs::compile(source);
        if (options.ast) printAst(compilation.ast);
        if (options.ir) printIr(compilation.ir);
        if (options.run || !options.htmlPath.empty()) {
            const auto states = cs::executeTrace(compilation.ir);
            if (options.run) {
                std::cout << "Game " << compilation.ast.game.name << " executed successfully ("
                          << compilation.moves.size() << " moves, " << compilation.ir.size()
                          << " IR operations)\n" << states.back().display()
                          << "\nStatus: " << cs::positionStatus(states.back()) << '\n';
            }
            if (!options.htmlPath.empty()) {
                std::ofstream output(options.htmlPath);
                if (!output) throw std::runtime_error("cannot write HTML report: " + options.htmlPath);
                output << cs::renderHtml(compilation, source, states);
                if (!output) throw std::runtime_error("failed to write HTML report: " + options.htmlPath);
                std::cout << "Visual board saved to " << options.htmlPath << '\n';
            }
        }
    } catch (const cs::Diagnostic& error) {
        if (options.json) std::cout << cs::renderErrorJson(error) << '\n';
        else std::cerr << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        if (options.json)
            std::cout << cs::renderErrorJson(cs::Diagnostic("RUNTIME", {1, 1}, error.what())) << '\n';
        else std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
