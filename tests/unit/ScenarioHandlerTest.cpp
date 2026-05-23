#include <gtest/gtest.h>
#include "../../cpp-server/include/ScenarioHandler.hpp"

class ScenarioHandlerTest : public ::testing::Test {
protected:

    std::unique_ptr<ScenarioHandler> handler = nullptr;

    void SetUp() override {
        handler = std::make_unique<ScenarioHandler>();
    }
    void TearDown() override {
        handler.reset();
    }
    
};

TEST_F(ScenarioHandlerTest, AddScenario) {
    EXPECT_NO_THROW(handler->addScenario(1, "temp > 25"));
    EXPECT_NO_THROW(handler->addScenario(2, "hum < 40"));
}

TEST_F(ScenarioHandlerTest, UpdateScenario) {

    handler->addScenario(1, "temp > 25");
    
    handler->addScenario(1, "temp > 30");
    
    auto triggered = handler->updateParameter("temp", 28.0);
    EXPECT_EQ(triggered.size(), 0);
    
    triggered = handler->updateParameter("temp", 32.0);
    EXPECT_EQ(triggered.size(), 1);
}

TEST_F(ScenarioHandlerTest, RemoveScenario) {
    handler->addScenario(1, "temp > 25");
    handler->addScenario(2, "hum < 40");
    
    handler->removeScenario(1);
    
    auto triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 0);
    
    triggered = handler->updateParameter("hum", 30.0);
    EXPECT_EQ(triggered.size(), 1);
}

TEST_F(ScenarioHandlerTest, UpdateParameterTriggersScenario) {
    handler->addScenario(1, "temp > 25");
    handler->addScenario(2, "temp > 40");
    auto triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 1);
    EXPECT_EQ(triggered[0], 1);
}

TEST_F(ScenarioHandlerTest, ScenarioTriggersOnFalseToTrueTransition) {
    handler->addScenario(1, "temp > 25");
    
    // Первое обновление → срабатывает
    auto triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 1);
    
    // Второе обновление (всё ещё true) → не срабатывает
    triggered = handler->updateParameter("temp", 31.0);
    EXPECT_EQ(triggered.size(), 0);
    
    // Стало false → не срабатывает
    triggered = handler->updateParameter("temp", 20.0);
    EXPECT_EQ(triggered.size(), 0);
    
    // Снова true → срабатывает
    triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 1);
}

TEST_F(ScenarioHandlerTest, MultipleConditionsAnd) {
    handler->addScenario(1, "temp > 25 AND hum < 40");
    
    auto triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 0);
    
    triggered = handler->updateParameter("hum", 35.0);
    EXPECT_EQ(triggered.size(), 1);
}

TEST_F(ScenarioHandlerTest, MultipleConditionsOr) {
    handler->addScenario(1, "temp > 25 OR hum < 40");
    
    auto triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 1);
    
    triggered = handler->updateParameter("temp", 20.0);
    EXPECT_EQ(triggered.size(), 0);

    triggered = handler->updateParameter("hum", 35.0);
    EXPECT_EQ(triggered.size(), 1);
}

TEST_F(ScenarioHandlerTest, ComplexConditionWithParentheses) {
    handler->addScenario(1, "(temp > 25 AND hum < 40) OR motion == 1");
    
    auto triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 0);
    
    triggered = handler->updateParameter("hum", 35.0);
    EXPECT_EQ(triggered.size(), 1);
    
    triggered = handler->updateParameter("temp", 20.0);
    EXPECT_EQ(triggered.size(), 0);

    triggered = handler->updateParameter("motion", 1.0);
    EXPECT_EQ(triggered.size(), 1);
}

TEST_F(ScenarioHandlerTest, MultipleIndependentScenarios) {
    
    handler->addScenario(1, "temp > 25");
    handler->addScenario(2, "hum < 40");
    handler->addScenario(3, "motion == 1");
    handler->addScenario(4, "temp < 40");
    
    auto triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 2);
    EXPECT_TRUE(std::find(triggered.begin(), triggered.end(), 1) != triggered.end());
    EXPECT_TRUE(std::find(triggered.begin(), triggered.end(), 4) != triggered.end());
    
    triggered = handler->updateParameter("hum", 35.0);
    EXPECT_EQ(triggered.size(), 1);
    EXPECT_TRUE(std::find(triggered.begin(), triggered.end(), 2) != triggered.end());
    
    triggered = handler->updateParameter("motion", 1.0);
    EXPECT_EQ(triggered.size(), 1);
    EXPECT_TRUE(std::find(triggered.begin(), triggered.end(), 3) != triggered.end());
}

TEST_F(ScenarioHandlerTest, ScenarioWithMultipleDependencies) {
    handler->addScenario(1, "temp > 25 AND hum < 40 AND motion == 1");
    
    handler->updateParameter("temp", 30.0);
    handler->updateParameter("hum", 35.0);
    auto triggered = handler->updateParameter("motion", 1.0);
    
    EXPECT_EQ(triggered.size(), 1);
}

TEST_F(ScenarioHandlerTest, NoTriggerWithoutParameters) {
    handler->addScenario(1, "temp > 25 AND hum < 40");
    
    auto triggered = handler->updateParameter("light", 300.0);
    EXPECT_EQ(triggered.size(), 0);
}

TEST_F(ScenarioHandlerTest, NumericComparison) {
    handler->addScenario(1, "temp >= 25.5");
    
    auto triggered = handler->updateParameter("temp", 25.5);
    EXPECT_EQ(triggered.size(), 1);
    
    triggered = handler->updateParameter("temp", 25.4);
    EXPECT_EQ(triggered.size(), 0);
}

TEST_F(ScenarioHandlerTest, BooleanValues) {

    handler->addScenario(1, "motion == 1");
    
    auto triggered = handler->updateParameter("motion", 1.0);
    EXPECT_EQ(triggered.size(), 1);
    
    triggered = handler->updateParameter("motion", 0.0);
    EXPECT_EQ(triggered.size(), 0);
}

TEST_F(ScenarioHandlerTest, ReloadScenario) {

    handler->addScenario(1, "temp > 25");
    
    auto triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 1);
    
    handler->addScenario(1, "temp < 25");
    
    triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 0);
    
    triggered = handler->updateParameter("temp", 20.0);
    EXPECT_EQ(triggered.size(), 1);
}

TEST_F(ScenarioHandlerTest, RemoveNonExistentScenario) {
    EXPECT_NO_THROW(handler->removeScenario(999));
}

TEST_F(ScenarioHandlerTest, ManyScenarios) {
    for (int i = 0; i < 100; i++) {
        handler->addScenario(i, "temp > " + std::to_string(i));
    }
    
    auto triggered = handler->updateParameter("temp", 50.0);
    EXPECT_EQ(triggered.size(), 50);  
}

TEST_F(ScenarioHandlerTest, HandleParseError) {
    EXPECT_THROW(handler->addScenario(1, "temp >"), std::runtime_error);
}

TEST_F(ScenarioHandlerTest, ComplexScenarioWithMultipleConditions) {
    handler->addScenario(1, "(temp > 25 AND hum < 40) OR (light > 500 AND motion == 1)");
    
    auto triggered = handler->updateParameter("temp", 30.0);
    EXPECT_EQ(triggered.size(), 0);
    
    triggered = handler->updateParameter("hum", 35.0);
    EXPECT_EQ(triggered.size(), 1);
    
    handler->updateParameter("temp", 20.0);
    triggered = handler->updateParameter("light", 600.0);
    EXPECT_EQ(triggered.size(), 0);
    
    triggered = handler->updateParameter("motion", 1.0);
    EXPECT_EQ(triggered.size(), 1);
}