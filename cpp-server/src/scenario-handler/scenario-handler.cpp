#include "scenario-handler.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <stdexcept>

// ---------- Вспомогательные функции для AST ----------
std::unique_ptr<ASTNode> ASTNode::makeAnd(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r) {
    auto node = std::make_unique<ASTNode>();
    node->type = NodeType::AND;
    node->left = std::move(l);
    node->right = std::move(r);
    return node;
}

std::unique_ptr<ASTNode> ASTNode::makeOr(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r) {
    auto node = std::make_unique<ASTNode>();
    node->type = NodeType::OR;
    node->left = std::move(l);
    node->right = std::move(r);
    return node;
}

std::unique_ptr<ASTNode> ASTNode::makeCompare(const std::string& param, const std::string& op, double value) {
    auto node = std::make_unique<ASTNode>();
    node->type = NodeType::COMPARE;
    node->param = param;
    node->op = op;
    node->value = value;
    return node;
}

// ---------- Токенизация ----------
static std::vector<std::string> tokenize(const std::string& expr) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char ch : expr) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
            continue;
        }
        if (ch == '(' || ch == ')') {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
            tokens.push_back(std::string(1, ch));
        } else {
            cur += ch;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

// ---------- Парсер рекурсивного спуска ----------
class ParserImpl {
public:
    ParserImpl(const std::vector<std::string>& tokens) : tokens(tokens), pos(0) {}
    
    std::unique_ptr<ASTNode> parse() {
        if (pos >= tokens.size()) return nullptr;
        auto node = parseExpression();
        if (pos != tokens.size()) throw std::runtime_error("Extra tokens");
        return node;
    }
    
private:
    const std::vector<std::string>& tokens;
    size_t pos;
    
    std::unique_ptr<ASTNode> parseExpression() {
        auto node = parseTerm();
        while (pos < tokens.size() && tokens[pos] == "OR") {
            ++pos;
            auto right = parseTerm();
            node = ASTNode::makeOr(std::move(node), std::move(right));
        }
        return node;
    }
    
    std::unique_ptr<ASTNode> parseTerm() {
        auto node = parseFactor();
        while (pos < tokens.size() && tokens[pos] == "AND") {
            ++pos;
            auto right = parseFactor();
            node = ASTNode::makeAnd(std::move(node), std::move(right));
        }
        return node;
    }
    
    std::unique_ptr<ASTNode> parseFactor() {
        if (pos >= tokens.size()) throw std::runtime_error("Unexpected end");
        if (tokens[pos] == "(") {
            ++pos;
            auto node = parseExpression();
            if (pos >= tokens.size() || tokens[pos] != ")")
                throw std::runtime_error("Expected ')'");
            ++pos;
            return node;
        }
        // Формат: param op value
        std::string param = tokens[pos++];
        if (pos >= tokens.size()) throw std::runtime_error("Missing operator");
        std::string op = tokens[pos++];
        if (pos >= tokens.size()) throw std::runtime_error("Missing value");
        double value;
        try {
            value = std::stod(tokens[pos++]);
        } catch (...) {
            throw std::runtime_error("Invalid numeric value");
        }
        if (op != ">" && op != "<" && op != ">=" && op != "<=" && op != "==" && op != "!=") {
            throw std::runtime_error("Invalid comparison operator");
        }
        return ASTNode::makeCompare(param, op, value);
    }
};

// ---------- Реализация парсера ----------
std::unique_ptr<ASTNode> ExpressionParser::parse(const std::string& expression) {
    auto tokens = tokenize(expression);
    if (tokens.empty()) return nullptr;
    ParserImpl parser(tokens);
    return parser.parse();
}

// ---------- Сбор зависимостей (имён параметров) ----------
void ScenarioEngine::collectDependencies(const ASTNode* node, std::unordered_set<std::string>& out) {
    if (!node) return;
    if (node->type == NodeType::COMPARE) {
        out.insert(node->param);
    } else {
        collectDependencies(node->left.get(), out);
        collectDependencies(node->right.get(), out);
    }
}

// ---------- Вычисление узла AST ----------
bool ScenarioEngine::evaluateNode(const ASTNode* node) const {
    if (!node) return false;
    switch (node->type) {
        case NodeType::AND:
            return evaluateNode(node->left.get()) && evaluateNode(node->right.get());
        case NodeType::OR:
            return evaluateNode(node->left.get()) || evaluateNode(node->right.get());
        case NodeType::COMPARE: {
            auto it = currentValues.find(node->param);
            if (it == currentValues.end()) return false; // нет данных – условие не выполнено
            double v = it->second;
            if (node->op == ">")  return v > node->value;
            if (node->op == "<")  return v < node->value;
            if (node->op == ">=") return v >= node->value;
            if (node->op == "<=") return v <= node->value;
            if (node->op == "==") return v == node->value;
            if (node->op == "!=") return v != node->value;
            return false;
        }
        default:
            return false;
    }
}

// ---------- Публичные методы движка ----------
void ScenarioEngine::addScenario(int scenarioId, const std::string& expression) {
    auto ast = ExpressionParser::parse(expression);
    if (!ast) {
        throw std::runtime_error("Failed to parse expression: " + expression);
    }
    
    ScenarioData data;
    data.ast = std::move(ast);
    collectDependencies(data.ast.get(), data.dependencies);
    
    // Удаляем старую версию, если была
    removeScenario(scenarioId);
    
    scenarios[scenarioId] = std::move(data);
    
    // Обновляем обратный индекс: параметр → список сценариев
    for (const auto& param : scenarios[scenarioId].dependencies) {
        paramToScenarios[param].push_back(scenarioId);
    }
    
    // Инициализируем последнее состояние как false
    lastResult[scenarioId] = false;
}

void ScenarioEngine::removeScenario(int scenarioId) {
    auto it = scenarios.find(scenarioId);
    if (it == scenarios.end()) return;
    
    // Удаляем из обратного индекса
    for (auto& entry : paramToScenarios) {
        auto& vec = entry.second;
        vec.erase(std::remove(vec.begin(), vec.end(), scenarioId), vec.end());
    }
    scenarios.erase(it);
    lastResult.erase(scenarioId);
}

std::vector<int> ScenarioEngine::updateParameter(const std::string& param, double value) {
    // Обновляем текущее значение
    currentValues[param] = value;
    
    // Находим все сценарии, зависящие от этого параметра
    auto it = paramToScenarios.find(param);
    if (it == paramToScenarios.end()) {
        return {};
    }
    
    const auto& affectedScenarios = it->second;
    std::vector<int> newlyTriggered;
    
    for (int sid : affectedScenarios) {
        auto sit = scenarios.find(sid);
        if (sit == scenarios.end()) continue;
        
        bool current = evaluateNode(sit->second.ast.get());
        bool previous = lastResult[sid];
        
        if (current && !previous) {
            // Сценарий стал истинным
            newlyTriggered.push_back(sid);
        }
        lastResult[sid] = current;
    }
    
    return newlyTriggered;
}