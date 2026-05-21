#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>

enum class NodeType {
    AND = 0,
    OR,
    COMPARE,
    NODE_TYPES_N
};

struct ASTNode {
    NodeType type;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    // для листьев сравнения
    std::string param;
    std::string op;    // ">", "<", ">=", "<=", "==", "!="
    double value;

    static std::unique_ptr<ASTNode> makeAnd(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r);
    static std::unique_ptr<ASTNode> makeOr(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r);
    static std::unique_ptr<ASTNode> makeCompare(const std::string& param, const std::string& op, double value);
};

// Парсер выражений
class ExpressionParser {
public:
    static std::unique_ptr<ASTNode> parse(const std::string& expression);
};

// Движок сценариев
class ScenarioEngine {
public:
    // Добавить или обновить сценарий
    void addScenario(int scenarioId, const std::string& expression);
    
    // Удалить сценарий
    void removeScenario(int scenarioId);
    
    // Обновить значение параметра (например, с датчика)
    // Возвращает ID сценариев, которые стали истинными после этого обновления
    std::vector<int> updateParameter(const std::string& param, double value);
    
    // Текущие значения параметров (для отладки)
    const std::unordered_map<std::string, double>& getCurrentValues() const { return currentValues; }

private:
    struct ScenarioData {
        std::unique_ptr<ASTNode> ast;
        std::unordered_set<std::string> dependencies;
    };
    
    std::unordered_map<int, ScenarioData> scenarios;                     // id -> данные сценария
    std::unordered_map<std::string, std::vector<int>> paramToScenarios; // параметр -> список id
    std::unordered_map<std::string, double> currentValues;              // текущие значения
    std::unordered_map<int, bool> lastResult;                           // предыдущее состояние сценария
    
    bool evaluateNode(const ASTNode* node) const;
    void collectDependencies(const ASTNode* node, std::unordered_set<std::string>& out);
};