#ifndef SCENARIO_ENGINE_HPP
#define SCENARIO_ENGINE_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ExpressionParser.hpp"
#include "ASTNodes.hpp"
#include <algorithm>
#include <stdexcept>

class Node;

class ScenarioEngine {
public:
    void addScenario(int scenarioId, const std::string& expression);
    void removeScenario(int scenarioId);
    std::vector<int> updateParameter(const std::string& param, double value);
    const std::unordered_map<std::string, double>& getCurrentValues() const { return currentValues_; }

private:
    struct ScenarioData {
        std::unique_ptr<Node> ast;
        std::unordered_set<std::string> dependencies;
    };

    std::unordered_map<int, ScenarioData> scenarios_;                    // id -> данные сценария
    std::unordered_map<std::string, std::vector<int>> paramToScenarios_; // параметр -> список id
    std::unordered_map<std::string, double> currentValues_;              // текущие значения параметров
    std::unordered_map<int, bool> lastResult_;                           // предыдущее состояние сценария
};

#endif 