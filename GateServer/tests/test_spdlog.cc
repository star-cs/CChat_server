#include <spdlog/spdlog.h>

int main() {
    // 创建控制台日志
    spdlog::info("Hello, {}!", "spdlog");

    // 打印不同级别日志
    spdlog::warn("This is a warning!");
    spdlog::error("This is an error!");
    spdlog::debug("This is a debug message (may not show by default).");

    return 0;
}
