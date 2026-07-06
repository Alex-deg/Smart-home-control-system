#include <gtest/gtest.h>
#include "../../cpp-server/include/ScenarioParser.hpp"
#include "../../cpp-server/include/ASTNodes.hpp"

class ScenarioParser : public ::testing::Test {
protected:
    std::unordered_map<std::string, double> values;
    
    void SetUp() override {
        values = {
            {"temp",   25.0},
            {"hum",    50.0},
            {"motion", 1.0},
            {"light",  300.0}
        };
    }
};

TEST_F(ScenarioParser, ParseSimpleCondition) {
    auto ast = Parser::parse("temp > 25");
    ASSERT_NE(ast, nullptr);
    
    values["temp"] = 30.0;
    EXPECT_TRUE(ast->evaluate(values));
    
    values["temp"] = 20.0;
    EXPECT_FALSE(ast->evaluate(values));
}

TEST_F(ScenarioParser, ParseDifferentOperators) {
    
    // Больше
    auto ast_gt = Parser::parse("temp > 25");
    EXPECT_TRUE(ast_gt->evaluate({{"temp", 30.0}}));
    EXPECT_FALSE(ast_gt->evaluate({{"temp", 20.0}}));
    
    // Меньше
    auto ast_lt = Parser::parse("temp < 25");
    EXPECT_TRUE(ast_lt->evaluate({{"temp", 20.0}}));
    EXPECT_FALSE(ast_lt->evaluate({{"temp", 30.0}}));
    
    // Больше или равно
    auto ast_ge = Parser::parse("temp >= 25");
    EXPECT_TRUE(ast_ge->evaluate({{"temp", 25.0}}));
    EXPECT_TRUE(ast_ge->evaluate({{"temp", 30.0}}));
    
    // Меньше или равно
    auto ast_le = Parser::parse("temp <= 25");
    EXPECT_TRUE(ast_le->evaluate({{"temp", 25.0}}));
    EXPECT_TRUE(ast_le->evaluate({{"temp", 20.0}}));
    
    // Равно
    auto ast_eq = Parser::parse("temp == 25");
    EXPECT_TRUE(ast_eq->evaluate({{"temp", 25.0}}));
    EXPECT_FALSE(ast_eq->evaluate({{"temp", 26.0}}));
    
    // Не равно
    auto ast_neq = Parser::parse("temp != 25");
    EXPECT_TRUE(ast_neq->evaluate({{"temp", 26.0}}));
    EXPECT_FALSE(ast_neq->evaluate({{"temp", 25.0}}));
}

TEST_F(ScenarioParser, ParseAndOperator) {
    auto ast = Parser::parse("temp > 25 AND hum < 50");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 30.0}, {"hum", 40.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 20.0}, {"hum", 40.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 30.0}, {"hum", 60.0}}));
}

TEST_F(ScenarioParser, ParseOrOperator) {
    auto ast = Parser::parse("temp > 25 OR motion == 1");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 30.0}, {"motion", 0.0}}));
    EXPECT_TRUE(ast->evaluate({{"temp", 20.0}, {"motion", 1.0}}));
    EXPECT_TRUE(ast->evaluate({{"temp", 30.0}, {"motion", 1.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 20.0}, {"motion", 0.0}}));
}

TEST_F(ScenarioParser, ParseWithParentheses) {
    auto ast = Parser::parse("(temp > 25 AND hum < 50) OR motion == 1");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 30.0}, {"hum", 40.0}, {"motion", 0.0}}));
    EXPECT_TRUE(ast->evaluate({{"temp", 20.0}, {"hum", 60.0}, {"motion", 1.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 20.0}, {"hum", 60.0}, {"motion", 0.0}}));
}

TEST_F(ScenarioParser, OperatorPrecedence) {
    // a AND b OR c должно интерпретироваться как (a AND b) OR c
    auto ast = Parser::parse("temp > 25 AND hum < 50 OR motion == 1");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 20.0}, {"hum", 40.0}, {"motion", 1.0}}));
    
    EXPECT_FALSE(ast->evaluate({{"temp", 20.0}, {"hum", 40.0}, {"motion", 0.0}}));
}

TEST_F(ScenarioParser, NestedParentheses) {
    auto ast = Parser::parse("(temp > 25 AND (hum < 50 OR light > 500))");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 30.0}, {"hum", 40.0}, {"light", 300.0}}));
    EXPECT_TRUE(ast->evaluate({{"temp", 30.0}, {"hum", 60.0}, {"light", 600.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 20.0}, {"hum", 60.0}, {"light", 300.0}}));
}

TEST_F(ScenarioParser, HandleSyntaxError) {
    EXPECT_THROW(Parser::parse("temp >"), std::runtime_error);
    EXPECT_THROW(Parser::parse("temp > 25 AND"), std::runtime_error);
    EXPECT_THROW(Parser::parse("temp > 25 AND (hum < 50"), std::runtime_error);
    EXPECT_THROW(Parser::parse("temp > 25 AND hum <"), std::runtime_error);
    EXPECT_THROW(Parser::parse("temp > 25 AND hum"), std::runtime_error);
}

TEST_F(ScenarioParser, HandleInvalidOperator) {
    EXPECT_THROW(Parser::parse("temp >> 25"), std::runtime_error);
    EXPECT_THROW(Parser::parse("temp <-> 25"), std::runtime_error);
}

TEST_F(ScenarioParser, HandleInvalidNumber) {
    EXPECT_THROW(Parser::parse("temp > abc"), std::runtime_error);
}

TEST_F(ScenarioParser, EmptyExpression) {
    auto ast = Parser::parse("");
    EXPECT_EQ(ast, nullptr);
}

TEST_F(ScenarioParser, WhitespaceOnly) {
    auto ast = Parser::parse("   ");
    EXPECT_EQ(ast, nullptr);
}

TEST_F(ScenarioParser, LongAndChain) {
    auto ast = Parser::parse("temp > 25 AND hum < 50 AND light > 200 AND motion == 1");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 30.0}, {"hum", 40.0}, {"light", 300.0}, {"motion", 1.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 20.0}, {"hum", 40.0}, {"light", 300.0}, {"motion", 1.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 30.0}, {"hum", 60.0}, {"light", 300.0}, {"motion", 1.0}}));
}

TEST_F(ScenarioParser, LongOrChain) {
    auto ast = Parser::parse("temp > 25 OR hum < 50 OR light > 200 OR motion == 1");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 30.0}, {"hum", 60.0}, {"light", 100.0}, {"motion", 0.0}}));
    EXPECT_TRUE(ast->evaluate({{"temp", 20.0}, {"hum", 40.0}, {"light", 100.0}, {"motion", 0.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 20.0}, {"hum", 60.0}, {"light", 100.0}, {"motion", 0.0}}));
}

TEST_F(ScenarioParser, MixedOperators) {
    auto ast = Parser::parse("temp > 25 OR (hum < 50 AND light > 200)");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 30.0}, {"hum", 60.0}, {"light", 100.0}}));
    EXPECT_TRUE(ast->evaluate({{"temp", 20.0}, {"hum", 40.0}, {"light", 300.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 20.0}, {"hum", 60.0}, {"light", 100.0}}));
}

TEST_F(ScenarioParser, FloatingPointComparison) {
    auto ast = Parser::parse("temp > 25.5");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 25.6}}));
    EXPECT_FALSE(ast->evaluate({{"temp", 25.4}}));
}

TEST_F(ScenarioParser, NegativeNumbers) {
    auto ast = Parser::parse("temp > -10.0");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temp", 0.0}}));
    EXPECT_FALSE(ast->evaluate({{"temp", -15.0}}));
}

TEST_F(ScenarioParser, DifferentParameterNames) {
    auto ast = Parser::parse("temperature > 25 AND humidity < 50");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_TRUE(ast->evaluate({{"temperature", 30.0}, {"humidity", 40.0}}));
    EXPECT_FALSE(ast->evaluate({{"temperature", 20.0}, {"humidity", 60.0}}));
}

TEST_F(ScenarioParser, NonExistentParameter) {
    auto ast = Parser::parse("temp > 25");
    ASSERT_NE(ast, nullptr);
    
    EXPECT_FALSE(ast->evaluate({{"humidity", 40.0}}));
}

TEST_F(ScenarioParser, CollectDependencies) {
    auto ast = Parser::parse("(temp > 25 AND hum < 50) OR motion == 1");
    ASSERT_NE(ast, nullptr);
    
    std::unordered_set<std::string> deps;
    ast->collectDependencies(deps);
    
    EXPECT_EQ(deps.size(), 3);
    EXPECT_TRUE(deps.count("temp") > 0);
    EXPECT_TRUE(deps.count("hum") > 0);
    EXPECT_TRUE(deps.count("motion") > 0);
}