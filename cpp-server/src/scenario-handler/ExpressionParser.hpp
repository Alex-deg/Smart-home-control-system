#ifndef EXPRESSION_PARSER_HPP
#define EXPRESSION_PARSER_HPP

#include <memory>
#include <string>
#include "ASTNodes.hpp"

class ExpressionParser {
public:
    static std::unique_ptr<Node> parse(const std::string& expression);
};

#endif 