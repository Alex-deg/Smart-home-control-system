#include <gtest/gtest.h>
#include "../../cpp-server/include/DataBase.hpp"
#include <filesystem>
#include <fstream>

class IDataBaseTest : public IDataBase {
public:
    using IDataBase::executeQuery;
    using IDataBase::executeRequest;
};

class IDatabaseTest : public ::testing::Test {
protected:

    IDataBaseTest db;

    void SetUp() override {
        db.open(":memory:");
    }

    void TearDown() override {
        db.close();
    }
};

TEST_F(IDatabaseTest, OpenAndCloseDatabase) {
    EXPECT_TRUE(db.isOpen());
    db.close();
    EXPECT_FALSE(db.isOpen());
}

TEST_F(IDatabaseTest, CreateTable) {
    db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    EXPECT_TRUE(db.isTableExists("users"));
}

TEST_F(IDatabaseTest, InsertData) {
    db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    db.executeRequest("INSERT INTO users (name) VALUES (?)", {"Alice"});
    auto result = db.executeQuery("SELECT COUNT(*) as cnt FROM users");
    EXPECT_EQ(result.get<int>(0, "cnt"), 1);
}

TEST_F(IDatabaseTest, SelectData) {
    db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    db.executeRequest("INSERT INTO users (name) VALUES (?)", {"Alice"});
    db.executeRequest("INSERT INTO users (name) VALUES (?)", {"Bob"});
    auto result = db.executeQuery("SELECT name FROM users ORDER BY name");
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result.get<std::string>(0, "name"), "Alice");
    EXPECT_EQ(result.get<std::string>(1, "name"), "Bob");
}

TEST_F(IDatabaseTest, UpdateData) {
    db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    db.executeRequest("INSERT INTO users (name) VALUES (?)", {"Alice"});
    db.executeRequest("UPDATE users SET name = ? WHERE name = ?", {"Alicia", "Alice"});
    auto result = db.executeQuery("SELECT name FROM users WHERE name = 'Alicia'");
    EXPECT_EQ(result.size(), 1);
}

TEST_F(IDatabaseTest, DeleteData) {
    db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    db.executeRequest("INSERT INTO users (name) VALUES (?)", {"Alice"});
    db.executeRequest("DELETE FROM users WHERE name = ?", {"Alice"});
    auto result = db.executeQuery("SELECT COUNT(*) as cnt FROM users");
    EXPECT_EQ(result.get<int>(0, "cnt"), 0);
}

TEST_F(IDatabaseTest, HandleSqlError) {
    EXPECT_THROW(
        db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
        db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)"),  // дублирование
        DataBaseException
    );
}

TEST_F(IDatabaseTest, GetTableColumnNames) {
    db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
    QueryResult response = db.executeQuery("SELECT * FROM users;");
    std::vector<std::string> columns = response.columnNames;
    EXPECT_EQ(columns[0], "id");
    EXPECT_EQ(columns[1], "name");
}

TEST_F(IDatabaseTest, VacuumDatabase) {
    db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
    for (int i = 0; i < 100; i++) {
        db.executeRequest("INSERT INTO users (name) VALUES (?)", {std::to_string(i)});
    }
    db.executeRequest("DELETE FROM users");
    EXPECT_NO_THROW(db.vacuum());
}

TEST_F(IDatabaseTest, QueryWithParameters) {
    db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");
    db.executeRequest("INSERT INTO users (name, age) VALUES (?, ?)", {"Alice", 25});
    db.executeRequest("INSERT INTO users (name, age) VALUES (?, ?)", {"Bob", 30});
    
    auto result = db.executeQuery("SELECT * FROM users WHERE age > ?", {28});
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result.get<std::string>(0, "name"), "Bob");
}

TEST_F(IDatabaseTest, QueryResultAccessMethods) {
    db.executeRequest("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");
    db.executeRequest("INSERT INTO users (name, age) VALUES (?, ?)", {"Alice", 25});
    
    auto result = db.executeQuery("SELECT id, name, age FROM users");
    
    EXPECT_EQ(result.get<int>(0, 0), 1);
    EXPECT_EQ(result.get<std::string>(0, 1), "Alice");
    EXPECT_EQ(result.get<int>(0, 2), 25);
    EXPECT_EQ(result.get<int>(0, "id"), 1);
    EXPECT_EQ(result.get<std::string>(0, "name"), "Alice");
    EXPECT_EQ(result.get<int>(0, "age"), 25);
}