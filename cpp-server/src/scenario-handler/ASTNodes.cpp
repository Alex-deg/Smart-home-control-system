#include "ASTNodes.hpp"

CompareNode::CompareNode(const std::string& param, const std::string& op, double value)
    : param_(param), op_(op), value_(value) {}

bool CompareNode::evaluate(const std::unordered_map<std::string, double>& values) const {
    auto it = values.find(param_);
    if (it == values.end()) return false;
    double v = it->second;

    if (op_ == ">")  return v >  value_;
    if (op_ == "<")  return v <  value_;
    if (op_ == ">=") return v >= value_;
    if (op_ == "<=") return v <= value_;
    if (op_ == "==") return v == value_;
    if (op_ == "!=") return v != value_;

    return false;
}

void CompareNode::collectDependencies(std::unordered_set<std::string>& out) const {
    out.insert(param_);
}

AndNode::AndNode(std::unique_ptr<Node> left, std::unique_ptr<Node> right)
    : left_(std::move(left)), right_(std::move(right)) {}

bool AndNode::evaluate(const std::unordered_map<std::string, double>& values) const {
    return left_->evaluate(values) && right_->evaluate(values);
}

void AndNode::collectDependencies(std::unordered_set<std::string>& out) const {
    left_->collectDependencies(out);
    right_->collectDependencies(out);
}

OrNode::OrNode(std::unique_ptr<Node> left, std::unique_ptr<Node> right)
    : left_(std::move(left)), right_(std::move(right)) {}

bool OrNode::evaluate(const std::unordered_map<std::string, double>& values) const {
    return left_->evaluate(values) || right_->evaluate(values);
}

void OrNode::collectDependencies(std::unordered_set<std::string>& out) const {
    left_->collectDependencies(out);
    right_->collectDependencies(out);
}