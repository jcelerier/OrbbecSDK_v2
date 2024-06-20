#include "EnvConfig.hpp"
#include "utils/Utils.hpp"
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(ob);

namespace libobsensor {

std::mutex               EnvConfig::instanceMutex_;
std::weak_ptr<EnvConfig> EnvConfig::instanceWeakPtr_;

std::shared_ptr<EnvConfig> EnvConfig::getInstance(const std::string &configFilePath) {
    std::unique_lock<std::mutex> lock(instanceMutex_);
    auto                         ctxInstance = instanceWeakPtr_.lock();
    if(!ctxInstance) {
        ctxInstance      = std::shared_ptr<EnvConfig>(new EnvConfig(configFilePath));
        instanceWeakPtr_ = ctxInstance;
    }
    return ctxInstance;
}

constexpr const char *defaultConfigFile = "./OrbbecSDKConfig.xml";
EnvConfig::EnvConfig(const std::string &configFile) {
    // add external config file
    auto extConfigFile = configFile;
    if(extConfigFile.empty()) {
        extConfigFile = defaultConfigFile;
    }
    if(utils::fileExists(extConfigFile.c_str())) {
        auto xmlReader = std::make_shared<XmlReader>(extConfigFile);
        xmlReaders_.push_back(xmlReader);
    }

    // add default config file (resource)
    auto fs           = cmrc::ob::get_filesystem();
    auto def_xml      = fs.open("config/OrbbecSDKConfig.xml");
    auto defXmlReader = std::make_shared<XmlReader>(def_xml.begin(), def_xml.size());
    xmlReaders_.push_back(defXmlReader);
}

bool EnvConfig::getIntValue(const std::string &nodePathName, int &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getIntValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

bool EnvConfig::getBooleanValue(const std::string &nodePathName, bool &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getBooleanValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

bool EnvConfig::getFloatValue(const std::string &nodePathName, float &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getFloatValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

bool EnvConfig::getDoubleValue(const std::string &nodePathName, double &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getDoubleValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

bool EnvConfig::getStringValue(const std::string &nodePathName, std::string &t) {
    for(auto xmlReader: xmlReaders_) {
        if(xmlReader->getStringValue(nodePathName, t)) {
            return true;
        }
    }
    return false;
}

}  // namespace libobsensor