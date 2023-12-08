#pragma once

#include "property.hpp"
#include "node.hpp"
#include "device.hpp"
#include "client.hpp"

#ifdef ESP32
    #include <WiFi.h>
#endif

#ifdef HOMIE_USE_UPTIME_LIB
	#include <uptime.h>
#endif

#define HOMIE_DEFAULT_FIRMWARE_NAME	"homie"
#ifndef HOMIE_FIRMWARE_NAME
	#define HOMIE_FIRMWARE_NAME HOMIE_DEFAULT_FIRMWARE_NAME
#endif

#define HOMIE_DEFAULT_FIRMWARE_VERSION "3.0.0"
#ifndef HOMIE_FIRMWARE_VERSION
	#define HOMIE_FIRMWARE_VERSION HOMIE_DEFAULT_FIRMWARE_VERSION
#endif

#define HOMIE_DEFAULT_STATS_INTERVAL	60 // Seconds
#ifndef HOMIE_STATS_INTERVAL
	#define HOMIE_STATS_INTERVAL HOMIE_DEFAULT_STATS_INTERVAL
#endif

namespace homie {
    struct generic_device : public basic_device {
        std::string id = "generic_device";
        std::map<std::string, node_ptr> nodes;
        std::map<std::string, std::string> attributes;
        std::shared_ptr<homie::client> client;

        generic_device() {
            attributes["name"] = "Generic device";
            attributes["state"] = "ready";
#ifdef ESP32
            attributes["localip"] = WiFi.localIP().toString().c_str();
            attributes["mac"] = WiFi.macAddress().c_str();
#endif
            attributes["fw/name"] = HOMIE_FIRMWARE_NAME;
            attributes["fw/version"] = HOMIE_FIRMWARE_VERSION;
            attributes["implementation"] = "homie-cpp";
#ifdef ESP32
#ifdef HOMIE_EXTENDED_STATS
            if (ESP.getPsramSize() > 0) {
                attributes["stats"] = "uptime,temperature,chipmodel,chipcores,chiprevision,cpufreqmhz,cyclecount,efusemac,flashchipmode,flashchipsize,flashchipspeed,freeheap,freepsram,freesketchspace,heapsize,maxallocheap,maxallocpsram,minfreeheap,minfreepsram,psramsize,sdkversion,sketchmd5,sketchsize";
            } else {
                attributes["stats"] = "uptime,temperature,chipmodel,chipcores,chiprevision,cpufreqmhz,cyclecount,efusemac,flashchipmode,flashchipsize,flashchipspeed,freeheap,freepsram,freesketchspace,heapsize,maxallocheap,minfreeheap,minfreepsram,psramsize,sdkversion,sketchmd5,sketchsize";
            }
#endif
            attributes["stats"] = "uptime,temperature";
#else
            attributes["stats"] = "uptime";
#endif
            attributes["stats/interval"] = std::to_string(HOMIE_STATS_INTERVAL);
#ifdef HOMIE_USE_UPTIME_LIB
            uptime::calculateUptime();
            std::string uptime("P");
            uptime.append(std::to_string(uptime::getDays()));
            uptime.append("DT");
            uptime.append(std::to_string(uptime::getHours()));
            uptime.append("H");
            uptime.append(std::to_string(uptime::getMinutes()));
            uptime.append("M");
            uptime.append(std::to_string(uptime::getSeconds()));
            uptime.append("S");
            attributes["stats/uptime"] = uptime;
#else
            attributes["stats/uptime"] = millis();
#endif
#ifdef ESP32
            attributes["stats/temperature"] = std::to_string(temperatureRead());
#ifdef HOMIE_EXTENDED_STATS
            attributes["stats/chipmodel"] =  ESP.getChipModel();
            attributes["stats/chipcores"] = std::to_string(ESP.getChipCores());
            attributes["stats/chiprevision"] = std::to_string(ESP.getChipRevision());
            attributes["stats/cpufreqmhz"] = std::to_string(ESP.getCpuFreqMHz());
            attributes["stats/cyclecount"] = std::to_string(ESP.getCycleCount());
            attributes["stats/efusemac"] = std::to_string(ESP.getEfuseMac());
            attributes["stats/flashchipmode"] = std::to_string(ESP.getFlashChipMode());
            attributes["stats/flashchipsize"] = std::to_string(ESP.getFlashChipSize());
            attributes["stats/flashchipspeed"] = std::to_string(ESP.getFlashChipSpeed());
            attributes["stats/freeheap"] = std::to_string(ESP.getFreeHeap());
            attributes["stats/freepsram"] = std::to_string(ESP.getFreePsram());
            attributes["stats/freesketchspace"] = std::to_string(ESP.getFreeSketchSpace());
            attributes["stats/heapsize"] = std::to_string(ESP.getHeapSize());
            attributes["stats/maxallocheap"] = std::to_string(ESP.getMaxAllocHeap());
            if (ESP.getPsramSize() > 0) {
                attributes["stats/maxallocpsram"] = std::to_string(ESP.getMaxAllocPsram());
                attributes["stats/minfreepsram"] = std::to_string(ESP.getMinFreePsram());
            }
            attributes["stats/minfreeheap"] = std::to_string(ESP.getMinFreeHeap());
            attributes["stats/minfreepsram"] = std::to_string(ESP.getMinFreePsram());
            attributes["stats/psramsize"] = std::to_string(ESP.getPsramSize());
            attributes["stats/sdkversion"] = ESP.getSdkVersion();
            attributes["stats/sketchmd5"] = ESP.getSketchMD5().c_str();
            attributes["stats/sketchsize"] = std::to_string(ESP.getSketchSize());
#endif
#endif
        }

        void add_node(node_ptr ptr) {
            nodes.insert({ ptr->get_id(), ptr });
        }

        virtual std::string get_id() const override {
            return id;
        }
        void set_id(std::string id) {
            this->id = id;
        }
        virtual std::set<std::string> get_nodes() const override {
            std::set<std::string> res;
            for (auto& e : nodes) res.insert(e.first);
            return res;
        }
        virtual node_ptr get_node(const std::string& id) override {
            return nodes.count(id) ? nodes.at(id) : nullptr;
        }
        virtual const_node_ptr get_node(const std::string& id) const override {
            return nodes.count(id) ? nodes.at(id) : nullptr;
        }

        virtual std::set<std::string> get_attributes() const override {
            std::set<std::string> res;
            for (auto& e : attributes) res.insert(e.first);
            return res;
        }
        virtual std::string get_attribute(const std::string& id) const {
            auto it = attributes.find(id);
            if (it != attributes.cend()) return it->second;
            return "";
        }
        virtual void set_attribute(const std::string& id, const std::string& value) {
            attributes[id] = value;
        }
    };
    struct generic_property : public basic_property {
        std::string id = "generic_property";
        std::string value = "0";
        std::map<std::string, std::string> attributes;
        std::weak_ptr<homie::node> node;
        std::function<void(std::string value)> callback;

        generic_property(std::weak_ptr<homie::node> ptr) : node(ptr) {
            attributes["name"] = "Generic property";
            attributes["settable"] = "true";
            attributes["datatype"] = "string";
            attributes["unit"] = " ";
            attributes["format"] = " ";
            attributes["retained"] = "true";
        }

        virtual node_ptr get_node() { return node.lock(); }
        virtual const_node_ptr get_node() const { return node.lock(); }

        virtual std::string get_id() const {
            return id;
        }
        void set_id(std::string id) {
            this->id = id;
        }

        void set_callback(std::function<void(std::string value)> callback) {
            this->callback = callback;
        }

        virtual std::string get_value(int64_t node_idx) const { return ""; }
        virtual void set_value(int64_t node_idx, const std::string& value) { }
        virtual std::string get_value() const { return value; }
        virtual void set_value(const std::string& value) {
            this->value = value;
            if (callback) {
                callback(value);
            }
            // auto client = ((generic_device*) get_node()->get_device().get())->client;
            // if (client) {
            //     client->notify_property_changed(get_node()->get_id(), get_id());
            // }
        }

        virtual std::set<std::string> get_attributes() const override {
            std::set<std::string> res;
            for (auto& e : attributes) res.insert(e.first);
            return res;
        }
        virtual std::string get_attribute(const std::string& id) const override {
            auto it = attributes.find(id);
            if (it != attributes.cend()) return it->second;
            return "";
        }
        virtual void set_attribute(const std::string& id, const std::string& value) override {
            attributes[id] = value;
        }

    };
    struct generic_node : public basic_node {
        std::string id = "generic_node";
        std::map<std::string, property_ptr> properties;
        std::map<std::string, std::string> attributes;
        std::map<std::pair<int64_t, std::string>, std::string> attributes_array;
        std::weak_ptr<homie::device> device;

        generic_node(std::weak_ptr<homie::device> dev) : device(dev) {
            attributes["name"] = "Generic node";
            attributes["type"] = " ";
        }

        void add_property(property_ptr ptr) {
            properties.insert({ptr->get_id(), ptr});
        }

        void remove_property(const std::string& id) {
            properties.erase(id);
        }

        virtual device_ptr get_device() override {
            return device.lock();
        }
        virtual const_device_ptr get_device() const override {
            return device.lock();
        }
        virtual std::string get_id() const override {
            return id;
        }
        void set_id(std::string id) {
            this->id = id;
        }
        virtual std::set<std::string> get_properties() const override {
            std::set<std::string> res;
            for (auto& e : properties) res.insert(e.first);
            return res;
        }
        virtual homie::property_ptr get_property(const std::string& id) override {
            return properties.count(id) ? properties.at(id) : nullptr;
        }
        virtual homie::const_property_ptr get_property(const std::string& id) const override {
            return properties.count(id) ? properties.at(id) : nullptr;
        }

        virtual std::set<std::string> get_attributes() const override {
            std::set<std::string> res;
            for (auto& e : attributes) res.insert(e.first);
            return res;
        }
        virtual std::set<std::string> get_attributes(int64_t idx) const override {
            std::set<std::string> res;
            for (auto& e : attributes_array)
                if(e.first.first == idx)
                    res.insert(e.first.second);
            return res;
        }
        virtual std::string get_attribute(const std::string& id) const override {
            auto it = attributes.find(id);
            if (it != attributes.cend()) return it->second;
            return "";
        }
        virtual void set_attribute(const std::string& id, const std::string& value) override {
            attributes[id] = value;
        }
        virtual std::string get_attribute(const std::string& id, int64_t idx) const override {
            auto it = attributes_array.find({ idx, id });
            if (it != attributes_array.cend()) return it->second;
            return "";
        }
        virtual void set_attribute(const std::string& id, const std::string& value, int64_t idx) override {
            attributes_array[{idx, id}] = value;
        }
    };
}