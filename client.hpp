#pragma once
#include "mqtt_client.hpp"
#include "device.hpp"
#include "utils.hpp"
#include "client_event_handler.hpp"
#include <set>

// Dependencies of homie_mqtt
#include <queue>
#include <TaskSchedulerDeclarations.h>
#include <MqttClient.h>

#ifdef HOMIE_MQTT_USE_TLS
    #include <WiFiClientSecure.h>
#else
    #include <WifiClient.h>
#endif

#define HOMIE_MQTT_DEFAULT_MAINTENANCE_INTERVAL    100 // Milliseconds

namespace homie {
	class homie_mqtt : public mqtt_client {
		public:
			Scheduler* scheduler;
            Task* taskMaintain;
            MqttClient* mqtt_client;
            std::string brokerHost;
            int brokerPort;
            homie::mqtt_event_handler* eventHandler;
            struct Message {
                std::string topic;
                std::string payload;
                int qos;
                bool retain;
            };
            homie_mqtt(
                Scheduler* scheduler,
                const std::string& clientId,
                const std::string& password,
                const std::string& brokerHost,
                const int brokerPort,
                long maintenanceInterval,
#ifdef HOMIE_MQTT_USE_TLS
#ifdef ESP32
                const char* brokerCACert,
#elif defined(ESP8266)
                const BearSSL::X509List* x509 = new BearSSL::X509List(ca_crt, ca_crt_size),
#endif
                WiFiClientSecure* wifiClient = new WiFiClientSecure()) {
#else
                WiFiClient* wifiClient = new WiFiClient()) {
#endif
                    this->scheduler = scheduler;
                    this->brokerHost = brokerHost;
                    this->brokerPort = brokerPort;
                    mqtt_client = new MqttClient(wifiClient);
                    taskMaintain = new Task(maintenanceInterval * TASK_MILLISECOND, TASK_FOREVER, [this]{
                        if (mqtt_client->connected()) {
                            mqtt_client->poll();
                        } else { // Reconnect when disconnected
							open();
                        }
                    }, scheduler);
                    taskMaintain->setSchedulingOption(TASK_INTERVAL);
#ifdef HOMIE_MQTT_USE_TLS
#ifdef ESP32
                    wifiClient->setCACert(brokerCACert);
#elif defined(ESP8266)
                    wifiClient->setTrustAnchors(x509);
#endif
#endif
                    mqtt_client->setId(clientId.c_str());
                    mqtt_client->setUsernamePassword(clientId.c_str(), password.c_str());
                    mqtt_client->onMessage([this](MqttClient *client, int payloadSize) {
                        String topic = client->messageTopic();
                        char payload[payloadSize];
                        payload[payloadSize] = '\0';
                        client->readBytes(payload, payloadSize);
                        eventHandler->on_message(std::string(topic.c_str()), std::string(payload));
                    });
                }
            ~homie_mqtt() {
                eventHandler->on_closing();
                delete(mqtt_client);
                eventHandler->on_closed();
            }
            virtual void set_event_handler(homie::mqtt_event_handler* eventHandler) override {
                this->eventHandler = eventHandler;
            }
            virtual void open(const std::string& will_topic, const std::string& will_payload, int will_qos, bool will_retain) override {
                mqtt_client->beginWill(will_topic.c_str(), will_retain, will_qos);
                mqtt_client->print(will_payload.c_str());
                mqtt_client->endWill();
#ifdef HOMIE_MQTT_USE_TLS
#ifdef ESP8266
                if (wifiClient->probeMaxFragmentLength(brokerHost.c_str(), brokerPort, 512)) {
                    wifiClient->setBufferSizes(512, 512);
                } else if (wifiClient->probeMaxFragmentLength(brokerHost.c_str(), brokerPort, 1024)) {
                    wifiClient->setBufferSizes(1024, 512);
                } else if (wifiClient->probeMaxFragmentLength(brokerHost.c_str(), brokerPort, 2048)) {
                    wifiClient->setBufferSizes(2048, 512);
                } else if (wifiClient->probeMaxFragmentLength(brokerHost.c_str(), brokerPort, 4096)) {
                    wifiClient->setBufferSizes(4096, 512);
                } else {
                    // TLS server does not support MFLN
                }
#endif
#endif
                if (!mqtt_client->connect(brokerHost.c_str(), brokerPort)) {
                    eventHandler->on_offline();
                    return;
                }
                eventHandler->on_connect(false, false);
                taskMaintain->enable();
            }
            virtual void open() override {
#ifdef HOMIE_MQTT_USE_TLS
#ifdef ESP8266
                if (wifiClient->probeMaxFragmentLength(brokerHost.c_str(), brokerPort, 512)) {
                    wifiClient->setBufferSizes(512, 512);
                } else if (wifiClient->probeMaxFragmentLength(brokerHost.c_str(), brokerPort, 1024)) {
                    wifiClient->setBufferSizes(1024, 512);
                } else if (wifiClient->probeMaxFragmentLength(brokerHost.c_str(), brokerPort, 2048)) {
                    wifiClient->setBufferSizes(2048, 512);
                } else if (wifiClient->probeMaxFragmentLength(brokerHost.c_str(), brokerPort, 4096)) {
                    wifiClient->setBufferSizes(4096, 512);
                } else {
                    // TLS server does not support MFLN
                }
#endif
#endif
                if (!mqtt_client->connect(brokerHost.c_str(), brokerPort)) {
                    eventHandler->on_offline();
                    return;
                }
                eventHandler->on_connect(false, true);
                taskMaintain->enable();
            }
            virtual void publish(const std::string& topic, const std::string& payload, int qos, bool retain) override {
                mqtt_client->beginMessage(topic.c_str(), payload.size(), retain, qos);
                mqtt_client->print(payload.c_str());
                mqtt_client->endMessage();
            }
            virtual void subscribe(const std::string& topic, int qos) override {
                mqtt_client->subscribe(topic.c_str(), qos);
            }
            virtual void unsubscribe(const std::string& topic) override {
                mqtt_client->unsubscribe(topic.c_str());
            }
            virtual bool is_connected() const override {
                return mqtt_client->connected();
            }
    };

    class queing_homie_mqtt : public homie_mqtt {
        private:
            std::queue<Message> sendQueue;
            Task* taskPublish;
        public:
            queing_homie_mqtt(Scheduler* scheduler,
                const std::string& clientId,
                const std::string& password,
                const std::string& brokerHost,
                const int brokerPort,
                long maintenanceInterval,
#ifdef HOMIE_MQTT_USE_TLS
#ifdef ESP32
                const char* brokerCACert,
#elif defined(ESP8266)
                const BearSSL::X509List* x509 = new BearSSL::X509List(ca_crt, ca_crt_size),
#endif
                WiFiClientSecure* wifiClient = new WiFiClientSecure())
#else
                WiFiClient* wifiClient = new WiFiClient())
#endif
                    : homie_mqtt(scheduler,
                        clientId,
                        password,
                        brokerHost,
                        brokerPort,
                        maintenanceInterval,
#ifdef HOMIE_MQTT_USE_TLS
#ifdef ESP32
                        brokerCACert,
#elif defined(ESP8266)
                        x509,
#endif
#endif
                        wifiClient) {
                taskPublish = new Task(TASK_IMMEDIATE, TASK_FOREVER, [this]{
                    if (!sendQueue.empty()) {
                        Message message = sendQueue.front();
                        mqtt_client->beginMessage(message.topic.c_str(), message.payload.size(), message.retain, message.qos);
                        mqtt_client->print(message.payload.c_str());
                        mqtt_client->endMessage();
                        sendQueue.pop();
                    } else {
                        taskPublish->disable();
                    }
                }, scheduler);
            }

            queing_homie_mqtt(Scheduler* schedulerMaintenance,
                Scheduler* schedulerPublishing,
                const std::string& clientId,
                const std::string& password,
                const std::string& brokerHost,
                const int brokerPort,
                long maintenanceInterval,
#ifdef HOMIE_MQTT_USE_TLS
#ifdef ESP32
                const char* brokerCACert,
#elif defined(ESP8266)
                const BearSSL::X509List* x509 = new BearSSL::X509List(ca_crt, ca_crt_size),
#endif
                WiFiClientSecure* wifiClient = new WiFiClientSecure())
#else
                WiFiClient* wifiClient = new WiFiClient())
#endif
                    : queing_homie_mqtt(schedulerMaintenance,
                        clientId,
                        password,
                        brokerHost,
                        brokerPort,
                        maintenanceInterval,
#ifdef HOMIE_MQTT_USE_TLS
#ifdef ESP32
                        brokerCACert,
#elif defined(ESP8266)
                        x509,
#endif
#endif
                        wifiClient) {
                taskPublish = new Task(TASK_IMMEDIATE, TASK_FOREVER, [this]{
                    if (!sendQueue.empty()) {
                        Message message = sendQueue.front();
                        mqtt_client->beginMessage(message.topic.c_str(), message.payload.size(), message.retain, message.qos);
                        mqtt_client->print(message.payload.c_str());
                        mqtt_client->endMessage();
                        sendQueue.pop();
                    } else {
                        taskPublish->disable();
                    }
                }, schedulerPublishing);
            }

            virtual void publish(const std::string& topic, const std::string& payload, int qos, bool retain) override {
                sendQueue.push({
                    .topic = topic,
                    .payload = payload,
                    .qos = qos,
                    .retain = retain
                });
                taskPublish->enable();
            }
    };
	class client : private mqtt_event_handler {
		mqtt_client& mqtt;
		std::string base_topic;
		device_ptr dev;
		client_event_handler* handler;

		// Inherited by mqtt_event_handler
		virtual void on_connect(bool session_present, bool reconnected) override {
			if (reconnected) {
				mqtt.publish(base_topic + dev->get_id() + "/$state", enum_to_string(dev->get_state()), 1, true);
			}
			else {
				publish_device_info();
			}
			mqtt.subscribe(base_topic + dev->get_id() + "/+/+/set", 1);
		}
		virtual void on_closing() override {
			mqtt.publish(base_topic + dev->get_id() + "/$state", enum_to_string(device_state::disconnected), 1, true);
		}
		virtual void on_closed() override {}
		virtual void on_offline() override {}
		virtual void on_message(const std::string & topic, const std::string & payload) override {
			// Check basetopic
			if (topic.size() < base_topic.size())
				return;
			if (topic.compare(0, base_topic.size(), base_topic) != 0)
				return;

			auto parts = utils::split<std::string>(topic, "/", base_topic.size());
			if (parts.size() < 2)
				return;
			for (auto& e : parts) if (e.empty()) return;
			if (parts[0][0] == '$') {
				if (parts[0] == "$broadcast") {
					this->handle_broadcast(parts[1], payload);
				}
			}
			else if(parts[0] == dev->get_id()) {
				if (parts.size() != 4
					|| parts[3] != "set"
					|| parts[2][0] == '$')
					return;
				this->handle_property_set(parts[1], parts[2], payload);
			}
		}

		void handle_property_set(const std::string& snode, const std::string& sproperty, const std::string& payload) {
			if (snode.empty() || sproperty.empty())
				return;

			int64_t id = 0;
			bool is_array_node = false;
			std::string rnode = snode;
			auto pos = rnode.find('_');
			if (pos != std::string::npos) {
				id = std::stoll(rnode.substr(pos + 1));
				is_array_node = true;
				rnode.resize(pos);
			}

			auto node = dev->get_node(rnode);
			if (node == nullptr || node->is_array() != is_array_node) return;
			auto prop = node->get_property(sproperty);
			if (prop == nullptr) return;

			if (is_array_node)
				prop->set_value(id, payload);
			else prop->set_value(payload);
		}

		void handle_broadcast(const std::string& level, const std::string& payload) {
			if(handler)
				handler->on_broadcast(level, payload);
		}

		void publish_device_info() {
			// Signal initialisation phase
			this->publish_device_attribute("$state", enum_to_string(device_state::init));

			// Public device properties
			this->publish_device_attribute("$homie", "3.0.0");
			this->publish_device_attribute("$name", dev->get_name());
			this->publish_device_attribute("$localip", dev->get_localip());
			this->publish_device_attribute("$mac", dev->get_mac());
			this->publish_device_attribute("$fw/name", dev->get_firmware_name());
			this->publish_device_attribute("$fw/version", dev->get_firmware_version());
			this->publish_device_attribute("$implementation", dev->get_implementation());
			this->publish_device_attribute("$stats/interval", std::to_string(dev->get_stats_interval().count()));

			// Publish nodes
			std::string nodes = "";
			for (auto& nodename : dev->get_nodes()) {
				auto node = dev->get_node(nodename);
				if (node->is_array()) {
					nodes += node->get_id() + "[],";
					this->publish_node_attribute(node, "$array", std::to_string(node->array_range().first) + "-" + std::to_string(node->array_range().second));
					for (int64_t i = node->array_range().first; i <= node->array_range().second; i++) {
						auto n = node->get_name(i);
						if(n != "")
						this->publish_device_attribute(node->get_id() + "_" + std::to_string(i) + "/$name", n);
					}
				}
				else {
					nodes += node->get_id() + ",";
				}
				this->publish_node_attribute(node, "$name", node->get_name());
				this->publish_node_attribute(node, "$type", node->get_type());

				// Publish node properties
				std::string properties = "";
				for (auto& propertyname : node->get_properties()) {
					auto property = node->get_property(propertyname);
					properties += property->get_id() + ",";
					this->publish_property_attribute(node, property, "$name", property->get_name());
					this->publish_property_attribute(node, property, "$settable", property->is_settable() ? "true" : "false");
					this->publish_property_attribute(node, property, "$retained", property->is_retained() ? "true" : "false");
					this->publish_property_attribute(node, property, "$unit", property->get_unit());
					this->publish_property_attribute(node, property, "$datatype", enum_to_string(property->get_datatype()));
					this->publish_device_attribute(node->get_id() + "/" + property->get_id() + "/$format", property->get_format());
					if (!node->is_array()) {
						auto val = property->get_value();
						if (!val.empty())
							this->publish_node_attribute(node, property->get_id(), val);
					}
					else {
						for (int64_t i = node->array_range().first; i <= node->array_range().second; i++) {
							auto val = property->get_value(i);
							if(!val.empty())
								this->publish_device_attribute(node->get_id() + "_" + std::to_string(i) + "/" + property->get_id(), val);
						}
					}
				}
				if (!properties.empty())
					properties.resize(properties.size() - 1);
				this->publish_node_attribute(node, "$properties", properties);
			}
			if (!nodes.empty())
				nodes.resize(nodes.size() - 1);
			this->publish_device_attribute("$nodes", nodes);

			// Publish stats
			std::string stats = "";
			for (auto& stat : dev->get_stats()) {
				stats += stat + ",";
				this->publish_device_attribute("$stats/" + stat, dev->get_stat(stat));
			}
			if (!stats.empty())
				stats.resize(stats.size() - 1);
			this->publish_device_attribute("$stats", stats);

			// Everything done, set device to real state
			this->publish_device_attribute("$state", enum_to_string(dev->get_state()));
		}

		void publish_device_attribute(const std::string& attribute, const std::string& value, const bool retained) {
			mqtt.publish(base_topic + dev->get_id() + "/" + attribute, value, 1, retained);
		}

		void publish_device_attribute(const std::string& attribute, const std::string& value) {
			publish_device_attribute(attribute, value, true);
		}

		void publish_node_attribute(const_node_ptr node, const std::string& attribute, const std::string& value) {
			publish_device_attribute(node->get_id() + "/" + attribute, value);
		}

		void publish_property_attribute(const_node_ptr node, const_property_ptr prop, const std::string& attribute, const std::string& value) {
			publish_node_attribute(node, prop->get_id() + "/" + attribute, value);
		}

		void notify_property_changed_impl(const std::string& snode, const std::string& sproperty, const int64_t* idx) {
			if (snode.empty() || sproperty.empty())
				return;

			auto node = dev->get_node(snode);
			if (!node) return;
			auto prop = node->get_property(sproperty);
			if (!prop) return;
			if (node->is_array()) {
				if (idx != nullptr) {
					this->publish_device_attribute(node->get_id() + "_" + std::to_string(*idx) + "/" + prop->get_id(), prop->get_value(*idx), prop->is_retained());
				}
				else {
					auto range = node->array_range();
					for (auto i = range.first; i <= range.second; i++) {
						this->publish_device_attribute(node->get_id() + "_" + std::to_string(i) + "/" + prop->get_id(), prop->get_value(i), prop->is_retained());
					}
				}
			}
			else {
				this->publish_device_attribute(node->get_id() + "/" + prop->get_id(), prop->get_value(), prop->is_retained());
			}
		}
	public:
		client(mqtt_client& con, device_ptr pdev, std::string basetopic = "homie/")
			: mqtt(con), base_topic(basetopic), dev(pdev), handler(nullptr)
		{
			if (!pdev) throw std::invalid_argument("device is null");
			mqtt.set_event_handler(this);

			mqtt.open(base_topic + dev->get_id() + "/$state", enum_to_string(device_state::lost), 1, true);
		}

		~client() {
			this->publish_device_attribute("$state", enum_to_string(device_state::disconnected));
			this->mqtt.unsubscribe(base_topic + dev->get_id() + "/+/+/set");
			mqtt.set_event_handler(nullptr);
		}

		void notify_property_changed(const std::string& snode, const std::string& sproperty) {
			notify_property_changed_impl(snode, sproperty, nullptr);
		}

		void notify_property_changed(const std::string& snode, const std::string& sproperty, int64_t idx) {
			notify_property_changed_impl(snode, sproperty, &idx);
		}

		void notify_stats_changed() {
			for (auto& stat : dev->get_stats()) {
				this->publish_device_attribute("$stats/" + stat, dev->get_stat(stat));
			}
		};

		void set_event_handler(client_event_handler* hdl) {
			handler = hdl;
		}
	};
}