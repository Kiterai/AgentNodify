// No inference or network: implements only the three catalog protocols.
#include <QCoreApplication>
#include <QFile>
#include <QThread>
#include <nlohmann/json.hpp>
#include <iostream>

using Json = nlohmann::json;
void send(const Json &value) { std::cout << value.dump() << std::endl; }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); const auto args = app.arguments();
    const auto mode = qEnvironmentVariable("AN_CATALOG_FIXTURE_MODE");
    if (mode == "hang") { QThread::sleep(30); return 0; }
    if (mode == "error") return 3;
    if (mode == "malformed") { std::cout << "not JSON or a model id" << std::endl; return 0; }
    if (mode == "large") { std::cout << std::string(5 * 1024 * 1024, 'x') << std::endl; return 0; }
    if (mode == "delayed") QThread::msleep(60);
    if (args.contains("models")) {
        if (mode != "empty") std::cout << "openai/code-alpha\r\nopenai/code-luna\nopenai/code-alpha\n";
        return 0;
    }
    std::string line;
    while (std::getline(std::cin, line)) {
        auto value = Json::parse(line);
        if (args.contains("app-server")) {
            if (value.value("method", "") == "initialize") send({{"id", value.at("id")}, {"result", Json::object()}});
            else if (value.value("method", "") == "model/list") {
                Json models = Json::array(); const bool next = value.at("params").contains("cursor");
                if (mode != "empty") {
                    models.push_back({{"id", "code-alpha"}, {"model", "code-alpha"}, {"displayName", "Code Alpha"}, {"description", "fixture"}, {"supportedReasoningEfforts", Json::array({{{"reasoningEffort", "medium"}}, {{"reasoningEffort", "high"}}})}});
                    if (!next) models.push_back({{"model", "hidden"}, {"hidden", true}});
                    else models.push_back({{"model", "code-luna"}, {"displayName", "Code Luna"}});
                }
                Json cursor = mode == "cursor-loop" || (!next && mode != "empty") ? Json("page-2") : Json();
                send({{"id", value.at("id")}, {"result", {{"data", models}, {"nextCursor", cursor}}}});
            }
        } else if (value.value("type", "") == "control_request") {
            Json models = mode == "empty" ? Json::array() : Json::array({
                {{"value", "alpha"}, {"displayName", "Alpha"}},
                {{"value", "haiku"}, {"displayName", "Haiku"}, {"resolvedModel", "claude-haiku-5-5"}, {"supportedEffortLevels", Json::array({"low", "medium"})}}
            });
            send({{"type", "control_response"}, {"response", {{"subtype", "success"}, {"request_id", value.at("request_id")}, {"response", {{"models", models}}}}}});
        }
    }
    return 0;
}
