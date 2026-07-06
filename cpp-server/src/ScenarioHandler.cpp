#include "../include/ScenarioHandler.hpp"

void ScenarioHandler::addScenario(int scenarioId, const std::string& expression) {
    
    auto ast = Parser::parse(expression);
    if (!ast) {
        throw std::runtime_error("Failed to parse expression: " + expression);
    }

    ScenarioData data;
    data.ast = std::move(ast);
    data.ast->collectDependencies(data.dependencies);

    removeScenario(scenarioId);

    scenarios_[scenarioId] = std::move(data);

    // Обновляем обратный индекс: { параметр : список сценариев }
    for (const auto& param : scenarios_[scenarioId].dependencies) {
        paramToScenarios_[param].push_back(scenarioId);
    }

    lastResult_[scenarioId] = false;
}

void ScenarioHandler::removeScenario(int scenarioId) {
    
    auto it = scenarios_.find(scenarioId);
    if (it == scenarios_.end()) return;

    // Удаление из обратного индекса
    for (auto& entry : paramToScenarios_) {
        auto& vec = entry.second;
        vec.erase(std::remove(vec.begin(), vec.end(), scenarioId), vec.end());
    }

    scenarios_.erase(it);
    lastResult_.erase(scenarioId);
}

std::vector<int> ScenarioHandler::updateParameter(const std::string& param, double value) {
    
    currentValues_[param] = value;

    auto it = paramToScenarios_.find(param);
    if (it == paramToScenarios_.end()) {
        return {};
    }

    std::vector<int> newlyTriggered;
    for (int sid : it->second) {
        auto sit = scenarios_.find(sid);
        if (sit == scenarios_.end()) continue;

        bool now = sit->second.ast->evaluate(currentValues_);
        bool prev = lastResult_[sid];

        if (now && !prev) {
            newlyTriggered.push_back(sid);
        }
        lastResult_[sid] = now;
    }
    return newlyTriggered;
}

const std::unordered_map<std::string, double> &ScenarioHandler::getCurrentValues() const { return currentValues_; }
