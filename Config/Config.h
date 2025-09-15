// Config.h
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>

struct ConfigData;  // ǰ������

class Config {
public:
    explicit Config(const std::string& json_file_path);
    ~Config() noexcept;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = default;
    Config& operator=(Config&&) = default;

    const std::vector<ConfigData>& getConfigs() const;
    const ConfigData& getCurrentConfig() const;

    void selectConfig(size_t index);
    void selectConfig(const std::string& name);

    size_t getConfigCount() const;
    void listAvailableConfigs() const;
    void printCurrentConfig(std::ostream& out = std::cout) const;

    bool promptAndSelectConfig(void* logger = nullptr);  // logger �� void* ���ּ���

    static void printConfigToStream(const ConfigData& c, std::ostream& out);

private:
    class Impl;  // Pimpl ʵ����
    std::unique_ptr<Impl> pImpl_;
};