#ifndef AST_NODES_HPP
#define AST_NODES_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Node {
public:
    virtual ~Node() = default;
    virtual bool evaluate(const std::unordered_map<std::string, double>& values) const = 0;
    virtual void collectDependencies(std::unordered_set<std::string>& out) const = 0;
};

class CompareNode : public Node {
public:
    CompareNode(const std::string& param, const std::string& op, double value);

    bool evaluate(const std::unordered_map<std::string, double>& values) const override;
    void collectDependencies(std::unordered_set<std::string>& out) const override;

private:
    std::string param_;
    std::string op_;
    double value_;
};

class AndNode : public Node {
public:
    AndNode(std::unique_ptr<Node> left, std::unique_ptr<Node> right);

    bool evaluate(const std::unordered_map<std::string, double>& values) const override;
    void collectDependencies(std::unordered_set<std::string>& out) const override;

private:
    std::unique_ptr<Node> left_;
    std::unique_ptr<Node> right_;
};

class OrNode : public Node {
public:
    OrNode(std::unique_ptr<Node> left, std::unique_ptr<Node> right);

    bool evaluate(const std::unordered_map<std::string, double>& values) const override;
    void collectDependencies(std::unordered_set<std::string>& out) const override;

private:
    std::unique_ptr<Node> left_;
    std::unique_ptr<Node> right_;
};

#endif 