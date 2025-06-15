/*
 * @Author: star-cs
 * @Date: 2025-06-09 17:17:23
 * @LastEditTime: 2025-06-09 22:23:17
 * @FilePath: /CChat_server/GateServer/tests/test_mysql.cc
 * @Description:
 */
#include <iostream>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>

int main() {
    try {
        // 1. 获取驱动实例
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();

        // 2. 连接数据库
        std::unique_ptr<sql::Connection> con(driver->connect("tcp://127.0.0.1:3307", "root", "12345mysql"));
        con->setSchema("test_conn");  // 选择数据库

        // 3. 执行查询
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT * FROM users"));

        // 4. 处理结果集
        while (res->next()) {
            std::cout << "ID: " << res->getInt("id") << ", Name: " << res->getString("name") << std::endl;
        }

        // 5. 插入数据（示例）
        stmt->execute("INSERT INTO users (name) VALUES ('Alice')");

    } catch (const sql::SQLException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "MySQL Error Code: " << e.getErrorCode() << std::endl;
    }
    return 0;
}