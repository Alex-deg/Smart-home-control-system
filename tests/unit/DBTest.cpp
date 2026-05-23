#include <gtest/gtest.h>
#include "../../cpp-server/include/DataBase.hpp"
#include <filesystem>
#include <fstream>

template <typename I, typename O>
struct TestCase{
    I input;
    O output;
};

class DatabaseTest : public ::testing::Test {
protected:

    DataBase db;

    void SetUp() override {
        db.open(":memory:");
    }
    void TearDown() override {
        db.close();
    }
};


TEST_F(DatabaseTest, CreateTable) {
    db.createTelemetryTable();
    EXPECT_TRUE(db.isTableExists("telemetry"));
}

TEST_F(DatabaseTest, InsertData) {
    json expectedValues;
    expectedValues["module_id"] = 1;
    expectedValues["param_name"] = "temperature";
    expectedValues["param_value"] = 25.0;
    expectedValues["timestamp"] = 0;
    expectedValues["meas_unit"] = "°C";
    std::vector<TestCase<json, json>> testCasesForDataInserting = {
        {expectedValues, expectedValues}
    };

    for(auto &&test_case : testCasesForDataInserting){
        db.addTelemetry(test_case.input["module_id"], test_case.input["param_name"], test_case.input["param_value"], 
                        test_case.input["timestamp"], test_case.input["meas_unit"]);
        auto actualTelemetry = db.getTelemtry(test_case.input["module_id"], test_case.input["param_name"], 1);
        EXPECT_EQ(actualTelemetry[0]["param_value"], test_case.output["value"]);
        EXPECT_EQ(actualTelemetry[0]["timestamp"], test_case.output["timestamp"]);
        EXPECT_EQ(actualTelemetry[0]["meas_unit"], test_case.output["meas_unit"]);
    }
    
}

TEST_F(DatabaseTest, ClearTable) {
    db.createTelemetryTable();
    db.addTelemetry(1, "1", 1, 1, "1");
    db.clearTelemetryTable();
    std::vector<json> telemetry = db.getTelemtry(1, "1", 1);
    EXPECT_TRUE(telemetry.empty());
}

TEST_F(DatabaseTest, UniqueModuleIDs) {
    db.createModuleParamsTable();
    db.addModuleParams(1, 20, 40000, 0);
    db.addModuleParams(2, 40, 100000, 31);
    db.addModuleParams(1, 22, 40000, 60);
    db.addModuleParams(3, 34, 60000, 75);
    db.addModuleParams(1, 17, 50000, 120);
    std::vector<long long> expectedIDs = {1, 2, 3};
    std::vector<json> actualIDs = db.getUniqueModuleIDs();
    ASSERT_EQ(expectedIDs.size(), actualIDs.size());
    for(int i = 0; i < expectedIDs.size(); i++){
        EXPECT_EQ(actualIDs[i], expectedIDs[i]["id"]);
    }
}

TEST_F(DatabaseTest, AnomalyTagging) {
    
    db.createModuleParamsTable();
    db.addModuleParams(1, 20, 40000, 0);
    db.addModuleParams(2, 40, 100000, 31);
    db.addModuleParams(1, 22, 40000, 60);
    db.addModuleParams(3, 34, 10000, 75);
    db.addModuleParams(1, 17, 50000, 120);
    db.anomalyTagging({2, 4});
    std::vector<json> params = db.getModuleParams(1, 1000, 1);
    EXPECT_EQ(params.size(), 0);
    params = db.getModuleParams(2, 1000, 1);
    EXPECT_EQ(params[0]["anomaly"], 1);
    params = db.getModuleParams(3, 1000, 1);
    EXPECT_EQ(params[0]["anomaly"], 1);
}