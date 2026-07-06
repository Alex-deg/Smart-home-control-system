#pragma once

#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <memory>
#include <string>

class Node {
public:
    virtual ~Node() = default;
    virtual bool evaluate(const std::unordered_map<std::string, double>& values) const = 0;
    virtual void collectDependencies(std::unordered_set<std::string>& out) const = 0;
};

class CompareNode : public Node {
    std::string param_;
    std::string op_;
    double value_;
public:
    CompareNode(const std::string& param, const std::string& op, double value);
    bool evaluate(const std::unordered_map<std::string, double>& values) const override;
    void collectDependencies(std::unordered_set<std::string>& out) const override;
};

class AndNode : public Node {
    std::unique_ptr<Node> left_;
    std::unique_ptr<Node> right_;
public:
    AndNode(std::unique_ptr<Node> left, std::unique_ptr<Node> right);
    bool evaluate(const std::unordered_map<std::string, double>& values) const override;
    void collectDependencies(std::unordered_set<std::string>& out) const override;    
};

class OrNode : public Node {
    std::unique_ptr<Node> left_;
    std::unique_ptr<Node> right_;
public:
    OrNode(std::unique_ptr<Node> left, std::unique_ptr<Node> right);
    bool evaluate(const std::unordered_map<std::string, double>& values) const override;
    void collectDependencies(std::unordered_set<std::string>& out) const override;
};
