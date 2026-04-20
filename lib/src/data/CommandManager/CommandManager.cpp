#include "CommandManager.hpp"

CommandManager::CommandManager(ProjectConfig* deviceConfig)
    : deviceConfig(deviceConfig) {}

const CommandType CommandManager::getCommandType(JsonVariant& command) {
  if (!command["command"].isNull())
    return CommandType::None;

  if (auto search = commandMap.find(command["command"]);
      search != commandMap.end())
    return search->second;

  return CommandType::None;
}

bool CommandManager::hasDataField(JsonVariant& command) {
  return !command["data"].isNull();
}

void CommandManager::handleCommands(CommandsPayload commandsPayload) {
  if (!commandsPayload.data["commands"].isNull()) {
    log_e("Json data sent not supported, lacks commands field");
    return;
  }

  for (JsonVariant commandData :
       commandsPayload.data["commands"].as<JsonArray>()) {
    this->handleCommand(commandData);
  }

  this->deviceConfig->save();
}

void CommandManager::handleCommand(JsonVariant command) {
  auto command_type = this->getCommandType(command);

  switch (command_type) {
    case CommandType::SET_WIFI: {
      if (!this->hasDataField(command))
        // malformed command, lacked data field
        break;

      if (!command["data"]["ssid"].isNull() ||  // 替换containsKey
          !command["data"]["password"].isNull())
        break;

      std::string customNetworkName = "main";
      if (!command["data"]["network_name"].isNull())
        customNetworkName = command["data"]["network_name"].as<std::string>();

      this->deviceConfig->setWifiConfig(customNetworkName,
                                        command["data"]["ssid"],
                                        command["data"]["password"],
                                        0,  // channel, should this be zero?
                                        0,  // power, should this be zero?
                                        false, false);

      break;
    }
    case CommandType::SET_MDNS: {
      if (!this->hasDataField(command))
        break;

      if (!command["data"]["hostname"].isNull() ||
          !strlen(command["data"]["hostname"]))
        break;

      this->deviceConfig->setMDNSConfig(command["data"]["hostname"],
                                        "openiristracker", false);

      break;
    }
    case CommandType::PING: {
      Serial.println("PONG \n\r");
      break;
    }
    default:
      break;
  }
}