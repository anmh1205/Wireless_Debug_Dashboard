#ifndef MQTTBROKER_H
#define MQTTBROKER_H

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <vector> // Add this include for std::vector

// Thông tin MQTT broker port
#define MQTT_BROKER_PORT 1883

class MQTTClient
{
public:
    MQTTClient(String clientID, WiFiClient *client, String username = "", String password = "");
    bool connected();
    bool disconnect();

    WiFiClient *client;
    String clientID;
    String username;
    String password;
    bool isConnected;
    unsigned long lastActivity;
};

class MQTTSubscription
{
public:
    MQTTSubscription(String topic, MQTTClient *client);
    String topic;
    MQTTClient *client;
};

class MQTTBroker
{
public:
    MQTTBroker();
    void begin();
    void loop();
    bool publish(String topic, String payload, bool retain = false);

private:
    WiFiServer server = WiFiServer(MQTT_BROKER_PORT);
    std::vector<MQTTClient *> clients;
    std::vector<MQTTSubscription *> subscriptions;
    std::vector<String> retainedMessages;
    std::vector<String> retainedTopics;

    void handleClient(WiFiClient client);
    void processConnect(MQTTClient *client, byte *packet, unsigned int length);
    void processPing(MQTTClient *client);
    void processDisconnect(MQTTClient *client);
    void processPublish(MQTTClient *client, byte *packet, unsigned int length);
    void processSubscribe(MQTTClient *client, byte *packet, unsigned int length);
    void broadcastMessage(String topic, String payload, MQTTClient *sender);
    bool matchTopicSubscribe(String topic, String subscription);
    void sendRetainedMessages(MQTTClient *client);
};

#endif // MQTTBROKER_H
