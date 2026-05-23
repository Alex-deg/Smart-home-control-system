#pragma once

#include "ScenarioParser.hpp"
#include "ASTNodes.hpp"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>

class Node;

class ScenarioHandler {
    struct ScenarioData {
        std::unique_ptr<Node> ast;
        std::unordered_set<std::string> dependencies;
    };
    std::unordered_map<int, ScenarioData> scenarios_;                     // { id       : данные сценария }
    std::unordered_map<std::string, std::vector<int>> paramToScenarios_;  // { параметр : список id }
    std::unordered_map<std::string, double> currentValues_;               // { параметр : текущее значение }
    std::unordered_map<int, bool> lastResult_;                            // { сценарий : его предыдущее состояние }
public:
    void addScenario(int scenarioId, const std::string& expression);
    void removeScenario(int scenarioId);
    std::vector<int> updateParameter(const std::string& param, double value);
    const std::unordered_map<std::string, double>& getCurrentValues() const;
};