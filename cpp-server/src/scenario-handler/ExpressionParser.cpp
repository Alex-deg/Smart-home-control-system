#include "ExpressionParser.hpp"
#include <sstream>
#include <vector>
#include <cctype>
#include <stdexcept>

static std::vector<std::string> tokenize(const std::string& expr) {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : expr) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        if (ch == '(' || ch == ')') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(std::string(1, ch));
        } else {
            current += ch;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

class ParserImpl {
public:
    ParserImpl(const std::vector<std::string>& tokens) : tokens_(tokens), pos_(0) {}

    std::unique_ptr<Node> parse() {
        if (pos_ >= tokens_.size()) return nullptr;
        auto node = parseExpression();
        if (pos_ != tokens_.size())
            throw std::runtime_error("Extra tokens after expression");
        return node;
    }

private:
    const std::vector<std::string>& tokens_;
    size_t pos_;

    std::unique_ptr<Node> parseExpression() {
        auto node = parseTerm();
        while (pos_ < tokens_.size() && tokens_[pos_] == "OR") {
            ++pos_;
            auto right = parseTerm();
            node = std::make_unique<OrNode>(std::move(node), std::move(right));
        }
        return node;
    }

    std::unique_ptr<Node> parseTerm() {
        auto node = parseFactor();
        while (pos_ < tokens_.size() && tokens_[pos_] == "AND") {
            ++pos_;
            auto right = parseFactor();
            node = std::make_unique<AndNode>(std::move(node), std::move(right));
        }
        return node;
    }

    std::unique_ptr<Node> parseFactor() {
        if (pos_ >= tokens_.size())
            throw std::runtime_error("Unexpected end of expression");

        if (tokens_[pos_] == "(") {
            ++pos_;
            auto node = parseExpression();
            if (pos_ >= tokens_.size() || tokens_[pos_] != ")")
                throw std::runtime_error("Expected ')'");
            ++pos_;
            return node;
        }

        std::string param = tokens_[pos_++];
        if (pos_ >= tokens_.size())
            throw std::runtime_error("Missing operator after '" + param + "'");
        std::string op = tokens_[pos_++];
        if (pos_ >= tokens_.size())
            throw std::runtime_error("Missing value after '" + op + "'");
        double value;
        try {
            value = std::stod(tokens_[pos_++]);
        } catch (...) {
            throw std::runtime_error("Invalid numeric value: " + tokens_[pos_ - 1]);
        }

        if (op != ">" && op != "<" && op != ">=" && op != "<=" && op != "==" && op != "!=") {
            throw std::runtime_error("Invalid comparison operator: " + op);
        }

        return std::make_unique<CompareNode>(param, op, value);
    }
};

std::unique_ptr<Node> ExpressionParser::parse(const std::string& expression) {
    auto tokens = tokenize(expression);
    if (tokens.empty())
        return nullptr;
    ParserImpl parser(tokens);
    return parser.parse();
}