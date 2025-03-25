#include "MQTTBroker.h"

// Khởi tạo MQTTClient
MQTTClient::MQTTClient(String clientID, WiFiClient *client, String username, String password)
{
    this->clientID = clientID;
    this->client = client;
    this->username = username;
    this->password = password;
    this->isConnected = true;
    this->lastActivity = millis();
}

bool MQTTClient::connected()
{
    return client->connected() && isConnected;
}

bool MQTTClient::disconnect()
{
    isConnected = false;
    client->stop(); // Just call stop without returning its value
    return true;    // Always return true
}

// Khởi tạo MQTTSubscription
MQTTSubscription::MQTTSubscription(String topic, MQTTClient *client)
{
    this->topic = topic;
    this->client = client;
}

// Khởi tạo MQTTBroker
MQTTBroker::MQTTBroker() {}

// Bắt đầu broker
void MQTTBroker::begin()
{
    server.begin();
    Serial.println("MQTT Broker started on port " + String(MQTT_BROKER_PORT));
}

// Vòng lặp chính của broker
void MQTTBroker::loop()
{
    // Kiểm tra client mới
    if (server.hasClient())
    {
        WiFiClient newClient = server.available();
        if (newClient)
        {
            Serial.println("New client connected: " + newClient.remoteIP().toString());
            handleClient(newClient);
        }
    }

    // Kiểm tra các client hiện có
    for (int i = 0; i < clients.size(); i++)
    {
        MQTTClient *client = clients[i];

        // Kiểm tra nếu client đã ngắt kết nối
        if (!client->connected())
        {
            Serial.println("Client disconnected: " + client->clientID);
            clients.erase(clients.begin() + i);
            i--;
            continue;
        }

        // Kiểm tra có dữ liệu đến không
        if (client->client->available())
        {
            byte buffer[512];
            int length = client->client->read(buffer, sizeof(buffer));

            // Xử lý gói tin dựa vào loại gói
            byte messageType = buffer[0] >> 4;

            switch (messageType)
            {
            case 1: // CONNECT
                processConnect(client, buffer, length);
                break;
            case 3: // PUBLISH
                processPublish(client, buffer, length);
                break;
            case 8: // SUBSCRIBE
                processSubscribe(client, buffer, length);
                break;
            case 12: // PINGREQ
                processPing(client);
                break;
            case 14: // DISCONNECT
                processDisconnect(client);
                break;
            }

            client->lastActivity = millis();
        }

        // Kiểm tra timeout
        if (millis() - client->lastActivity > 90000)
        { // 90 seconds timeout
            Serial.println("Client timeout: " + client->clientID);
            client->disconnect();
            clients.erase(clients.begin() + i);
            i--;
        }
    }
}

// Xử lý client mới kết nối
void MQTTBroker::handleClient(WiFiClient client)
{
    WiFiClient *newClient = new WiFiClient(client);
    MQTTClient *mqttClient = new MQTTClient("", newClient);
    clients.push_back(mqttClient);
}

// Xử lý gói tin CONNECT
void MQTTBroker::processConnect(MQTTClient *client, byte *packet, unsigned int length)
{
    // Xác thực kết nối và gửi CONNACK
    client->client->write((byte)0x20); // CONNACK
    client->client->write((byte)0x02); // Remaining Length
    client->client->write((byte)0x00); // No session present
    client->client->write((byte)0x00); // Connection accepted

    // Đặt clientID từ gói tin
    // Đây là phần đơn giản hóa, trong thực tế cần parse gói tin để lấy clientID
    client->clientID = "ESP32Client-" + String(random(0xffff), HEX);
    Serial.println("Client connected with ID: " + client->clientID);

    // Gửi các message được giữ lại (retained)
    sendRetainedMessages(client);
}

// Xử lý gói tin PING
void MQTTBroker::processPing(MQTTClient *client)
{
    client->client->write((byte)0xD0); // PINGRESP
    client->client->write((byte)0x00); // Zero length
}

// Xử lý gói tin DISCONNECT
void MQTTBroker::processDisconnect(MQTTClient *client)
{
    client->disconnect();
}

// Xử lý gói tin PUBLISH
void MQTTBroker::processPublish(MQTTClient *client, byte *packet, unsigned int length)
{
    // Đây là phần đơn giản hóa, trong thực tế cần parse gói tin
    // để lấy topic, payload, QoS, retained flag

    // Giả định packet[0] = MQTT header, packet[1] = Length
    // packet[2] và packet[3] = Topic length
    // packet[4] đến packet[4+topicLength-1] = Topic
    // sau đó là payload

    // Đơn giản hóa chỉ cho demo, thực tế cần parse đúng
    int topicLength = (packet[2] << 8) | packet[3];
    String topic = "";
    for (int i = 0; i < topicLength; i++)
    {
        topic += (char)packet[4 + i];
    }

    String payload = "";
    for (int i = 4 + topicLength; i < length; i++)
    {
        payload += (char)packet[i];
    }

    Serial.println("Publish from " + client->clientID + " to topic: " + topic + " with payload: " + payload);

    // Kiểm tra xem có phải là retained message không
    bool retained = (packet[0] & 0x01) == 0x01;

    if (retained)
    {
        // Kiểm tra xem topic đã tồn tại chưa
        bool found = false;
        for (int i = 0; i < retainedTopics.size(); i++)
        {
            if (retainedTopics[i] == topic)
            {
                retainedMessages[i] = payload;
                found = true;
                break;
            }
        }

        if (!found && payload.length() > 0)
        {
            retainedTopics.push_back(topic);
            retainedMessages.push_back(payload);
        }
    }

    // Phát broadcast tin nhắn đến các client đã đăng ký
    broadcastMessage(topic, payload, client);
}

// Xử lý gói tin SUBSCRIBE
void MQTTBroker::processSubscribe(MQTTClient *client, byte *packet, unsigned int length)
{
    // Đơn giản hóa cho demo
    // packet[0] = MQTT header
    // packet[1] = length
    // packet[2], packet[3] = Message ID
    // packet[4], packet[5] = Topic length
    // packet[6] đến packet[6+topicLength-1] = Topic

    int topicLength = (packet[4] << 8) | packet[5];
    String topic = "";
    for (int i = 0; i < topicLength; i++)
    {
        topic += (char)packet[6 + i];
    }

    Serial.println("Subscribe from " + client->clientID + " to topic: " + topic);

    // Thêm subscription
    MQTTSubscription *sub = new MQTTSubscription(topic, client);
    subscriptions.push_back(sub);

    // Gửi SUBACK
    client->client->write((byte)0x90);      // SUBACK
    client->client->write((byte)0x03);      // Length
    client->client->write((byte)packet[2]); // Message ID MSB
    client->client->write((byte)packet[3]); // Message ID LSB
    client->client->write((byte)0x00);      // QoS 0

    // Gửi retained messages cho subscription mới
    for (int i = 0; i < retainedTopics.size(); i++)
    {
        if (matchTopicSubscribe(retainedTopics[i], topic))
        {
            // Đơn giản hóa, thực tế cần tạo đúng MQTT packet
            String publishPacket = "";
            publishPacket += (char)0x30;                                                            // PUBLISH
            publishPacket += (char)(2 + retainedTopics[i].length() + retainedMessages[i].length()); // Length
            publishPacket += (char)0x00;                                                            // Topic length MSB
            publishPacket += (char)retainedTopics[i].length();                                      // Topic length LSB
            publishPacket += retainedTopics[i];                                                     // Topic
            publishPacket += retainedMessages[i];                                                   // Payload

            client->client->print(publishPacket);
        }
    }
}

// Phát broadcast tin nhắn đến các client đã đăng ký
void MQTTBroker::broadcastMessage(String topic, String payload, MQTTClient *sender)
{
    for (MQTTSubscription *sub : subscriptions)
    {
        if (sub->client != sender && matchTopicSubscribe(topic, sub->topic))
        {
            // Đơn giản hóa, thực tế cần tạo đúng MQTT packet
            String publishPacket = "";
            publishPacket += (char)0x30;                                    // PUBLISH
            publishPacket += (char)(2 + topic.length() + payload.length()); // Length
            publishPacket += (char)0x00;                                    // Topic length MSB
            publishPacket += (char)topic.length();                          // Topic length LSB
            publishPacket += topic;                                         // Topic
            publishPacket += payload;                                       // Payload

            sub->client->client->print(publishPacket);
        }
    }
}

// Kiểm tra xem topic có phù hợp với subscription không
bool MQTTBroker::matchTopicSubscribe(String topic, String subscription)
{
    // Đơn giản hóa cho demo, chỉ hỗ trợ exact match và wildcard #
    if (subscription.endsWith("#"))
    {
        String prefix = subscription.substring(0, subscription.length() - 1);
        return topic.startsWith(prefix);
    }
    return topic == subscription;
}

// Gửi retained messages cho client mới kết nối
void MQTTBroker::sendRetainedMessages(MQTTClient *client)
{
    // Thực hiện sau khi client subscribe
}

// Public API để publish tin nhắn từ ESP32 (broker tự gửi)
bool MQTTBroker::publish(String topic, String payload, bool retain)
{
    // Xử lý retained message
    if (retain)
    {
        bool found = false;
        for (int i = 0; i < retainedTopics.size(); i++)
        {
            if (retainedTopics[i] == topic)
            {
                retainedMessages[i] = payload;
                found = true;
                break;
            }
        }

        if (!found && payload.length() > 0)
        {
            retainedTopics.push_back(topic);
            retainedMessages.push_back(payload);
        }
    }

    // Broadcast đến tất cả client đã subscribe
    broadcastMessage(topic, payload, nullptr);
    return true;
}
