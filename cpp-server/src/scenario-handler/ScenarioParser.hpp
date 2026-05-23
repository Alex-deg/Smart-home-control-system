#pragma once

#include "ASTNodes.hpp"
#include <stdexcept>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <cctype>

class Parser {
public:
    static std::unique_ptr<Node> parse(const std::string& expression);
private:
    static std::vector<std::string> tokenize(const std::string& expr);
};

class ParserImpl {
    const std::vector<std::string>& tokens_;
    size_t pos_;
public:
    ParserImpl(const std::vector<std::string>& tokens) : tokens_(tokens), pos_(0) {}
    std::unique_ptr<Node> parse();
private:
    std::unique_ptr<Node> parseExpression();
    std::unique_ptr<Node> parseTerm();
    std::unique_ptr<Node> parseFactor();
};