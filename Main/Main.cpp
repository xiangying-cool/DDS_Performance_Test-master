// Main.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <ctime>

// --- 项目头文件 ---
#include "Main.h"
#include "DDSManager_Bytes.h"
#include "DDSManager_ZeroCopyBytes.h"
#include "Config.h"
#include "Logger.h"
#include "GloMemPool.h"
#include "Throughput_Bytes.h"
#include "Throughput_ZeroCopyBytes.h"  
#include "MetricsReport.h"
#include "TestRoundResult.h"
#include "ResourceUtilization.h"

namespace {
    std::string json_file_path = GlobalConfig::DEFAULT_JSON_CONFIG_PATH;
    std::string qos_file_path = GlobalConfig::DEFAULT_QOS_XML_PATH;
    std::string logDir = GlobalConfig::LOG_DIRECTORY;
    std::string logPrefix = GlobalConfig::LOG_FILE_PREFIX;
    std::string logSuffix = GlobalConfig::LOG_FILE_SUFFIX;
    std::string resultDir = GlobalConfig::DEFAULT_RESULT_PATH;
    bool loggingEnabled = true;
}

int main() {
    try {
        // ================= 初始化全局内存池 =================
        if (!GloMemPool::initialize()) {
            std::cerr << "[Error] GloMemPool 初始化失败！" << std::endl;
            std::cin.get(); // 等待用户按键，防止窗口关闭
            return EXIT_FAILURE;
        }
        Logger::getInstance().logAndPrint("[Memory] 使用 GloMemPool 管理全局内存");

        // ================= 初始化日志系统 =================
        Logger::setupLogger(logDir, logPrefix, logSuffix);

        // ================= 加载并选择配置 =================
        Config config(json_file_path);
        if (!config.promptAndSelectConfig(&Logger::getInstance())) {
            Logger::getInstance().logAndPrint("用户取消选择或配置加载失败");
            std::cin.get(); // 等待用户按键，防止窗口关闭
            return EXIT_FAILURE;
        }

        const ConfigData& base_config = config.getCurrentConfig();
        const int total_rounds = base_config.m_loopNum;

        Logger::getInstance().logAndPrint("\n=== 当前选中的配置模板 ===");
        std::ostringstream cfgStream;
        config.printCurrentConfig(cfgStream);
        Logger::getInstance().logAndPrint(cfgStream.str());

        if (total_rounds <= 0) {
            Logger::getInstance().logAndPrint("[Error] m_loopNum 必须大于 0");
            std::cin.get(); // 等待用户按键，防止窗口关闭
            return EXIT_FAILURE;
        }

        Logger::getInstance().logAndPrint("开始执行 " + std::to_string(total_rounds) + " 轮测试...");

        // ==================== 根据配置决定传输模式 ====================
        bool is_zero_copy_mode = (base_config.m_typeName == "DDS::ZeroCopyBytes");

        std::unique_ptr<DDSManager_Bytes> bytes_manager;
        std::unique_ptr<DDSManager_ZeroCopyBytes> zc_manager;

        std::unique_ptr<Throughput_Bytes> throughput_bytes;
        std::unique_ptr<Throughput_ZeroCopyBytes> throughput_zc;

        MetricsReport metricsReport;

        // ========== 主循环：多轮测试 ==========
        int total_result = EXIT_SUCCESS;

        for (int round = 0; round < total_rounds; ++round) {
            Logger::getInstance().logAndPrint(
                "=== 第 " + std::to_string(round + 1) + "/" + std::to_string(total_rounds) +
                " 轮测试开始 (m_activeLoop=" + std::to_string(round) + ") ==="
            );

            // 创建本轮配置副本
            ConfigData current_cfg = base_config;
            current_cfg.m_activeLoop = round;

            // 打印本轮参数
            std::ostringstream roundCfgStream;
            Config::printConfigToStream(current_cfg, roundCfgStream);
            Logger::getInstance().logAndPrint(roundCfgStream.str());

            // ------------------- 创建 Throughput 实例（如果尚未创建）-------------------
            // 注意：只在第一轮创建，后续复用
            if (round == 0) {
                if (is_zero_copy_mode) {
                    Logger::getInstance().logAndPrint("启动 ZeroCopyBytes 模式");

                    zc_manager = std::make_unique<DDSManager_ZeroCopyBytes>(current_cfg, qos_file_path);

                    throughput_zc = std::make_unique<Throughput_ZeroCopyBytes>(*zc_manager,
                        [&metricsReport](const TestRoundResult& result) {
                            metricsReport.addResult(result);
                        }
                    );
                }
                else {
                    Logger::getInstance().logAndPrint("启动 Bytes 模式");

                    bytes_manager = std::make_unique<DDSManager_Bytes>(current_cfg, qos_file_path);

                    throughput_bytes = std::make_unique<Throughput_Bytes>(*bytes_manager,
                        [&metricsReport](const TestRoundResult& result) {
                            metricsReport.addResult(result);
                        }
                    );
                }

                // ==================== 在创建 DDSManager 之后，第一轮测试开始前初始化 ResourceUtilization ====================
                // 将初始化放在这里，尝试在 ZRDDS 实体创建后、主要数据流开始前来初始化监控，
                // 希望能解决 PDH_NO_DATA 问题或提高成功率。
                if (!ResourceUtilization::instance().initialize()) {
                    Logger::getInstance().logAndPrint("[Warning] ResourceUtilization 初始化失败！CPU 监控可能无效。");
                    // 根据需求决定是否退出或继续
                    // return EXIT_FAILURE; // 如果 CPU 监控是必须的，可以取消注释
                }
                else {
                    Logger::getInstance().logAndPrint("[Resource] ResourceUtilization 初始化成功");
                }
                // =================================================================================================
            }

            // ------------------- 重新初始化 DDSManager -------------------
            bool init_success = false;

            // 每轮都重新定义回调，避免 move 后失效
            if (is_zero_copy_mode) {
                if (current_cfg.m_isPositive) {
                    init_success = zc_manager->initialize();
                }
                else {
                    auto end_callback = [&]() { throughput_zc->onEndOfRound(); };
                    init_success = zc_manager->initialize(
                        [&](const DDS::ZeroCopyBytes& sample, const DDS::SampleInfo& info) {
                            throughput_zc->onDataReceived(sample, info);
                        },
                        end_callback
                    );
                }
            }
            else {
                if (current_cfg.m_isPositive) {
                    init_success = bytes_manager->initialize();
                }
                else {
                    auto end_callback = [&]() { throughput_bytes->onEndOfRound(); };
                    init_success = bytes_manager->initialize(
                        [&](const DDS::Bytes& sample, const DDS::SampleInfo& info) {
                            throughput_bytes->onDataReceived(sample, info);
                        },
                        end_callback
                    );
                }
            }

            if (!init_success) {
                Logger::getInstance().logAndPrint("[Error] DDSManager 初始化失败（第 " + std::to_string(round + 1) + " 轮）");
                total_result = EXIT_FAILURE;
                break;
            }

            // ------------------- Publisher: 等待 Subscriber 重连（从第二轮开始）-------------------
            if (current_cfg.m_isPositive && round > 0) {
                Logger::getInstance().logAndPrint("等待订阅者重新上线以启动第 " + std::to_string(round + 1) + " 轮...");

                bool connected = false;
                if (is_zero_copy_mode) {
                    connected = throughput_zc->waitForSubscriberReconnect(std::chrono::seconds(10));
                }
                else {
                    connected = throughput_bytes->waitForSubscriberReconnect(std::chrono::seconds(10));
                }

                if (!connected) {
                    Logger::getInstance().logAndPrint("警告：未检测到订阅者重连，超时继续...");
                }
            }

            // ------------------- 运行单轮测试 -------------------
            int result = 0;
            if (current_cfg.m_isPositive) {
                result = is_zero_copy_mode
                    ? throughput_zc->runPublisher(current_cfg)
                    : throughput_bytes->runPublisher(current_cfg);
            }
            else {
                result = is_zero_copy_mode
                    ? throughput_zc->runSubscriber(current_cfg)
                    : throughput_bytes->runSubscriber(current_cfg);
            }

            if (result == 0) {
                Logger::getInstance().logAndPrint("第 " + std::to_string(round + 1) + " 轮测试完成。");
            }
            else {
                Logger::getInstance().logAndPrint("第 " + std::to_string(round + 1) + " 轮测试发生错误。");
                total_result = EXIT_FAILURE;
            }

            // ------------------- 清理本轮回合资源 -------------------
            if (is_zero_copy_mode) {
                zc_manager->shutdown();
            }
            else {
                bytes_manager->shutdown();
            }

            // 防止端口冲突或资源竞争
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // ==================== 测试结束，生成报告 ====================
        Logger::getInstance().logAndPrint("\n--- 开始生成系统资源使用报告 ---");
        metricsReport.generateSummary();

        // 关闭资源采集
        ResourceUtilization::instance().shutdown();
        GloMemPool::finalize();

        // --- 新增：程序结束前暂停，防止 cmd 窗口关闭 ---
        std::cout << "\n程序执行完毕，按任意键退出..." << std::endl;
        std::cin.get();
        // --- 新增结束 ---

        return total_result == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;

    }
    catch (const std::exception& e) {
        std::string errorMsg = "[Error] 异常: " + std::string(e.what());
        if (loggingEnabled) {
            Logger::getInstance().logAndPrint(errorMsg);
        }
        else {
            std::cerr << errorMsg << std::endl;
        }
        std::cout << "\n程序因异常终止，按任意键退出..." << std::endl;
        std::cin.get(); // 等待用户按键，防止窗口关闭
        return EXIT_FAILURE;
    }
    catch (...) {
        std::string errorMsg = "[Error] 未捕获的异常";
        if (loggingEnabled) {
            Logger::getInstance().logAndPrint(errorMsg);
        }
        else {
            std::cerr << errorMsg << std::endl;
        }
        std::cout << "\n程序因未捕获异常终止，按任意键退出..." << std::endl;
        std::cin.get(); // 等待用户按键，防止窗口关闭
        return EXIT_FAILURE;
    }
}