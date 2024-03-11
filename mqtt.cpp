// gcc -std=c++11 -g mqtt.cpp -o mqtt -lstdc++ -lmosquitto

// mosquitto_pub -h localhost -t test/testing -m "on" -q 1
// mosquitto_sub -h localhost -t test/testing -q 1
// https://cedalo.com/blog/mqtt-subscribe-publish-mosquitto-pub-sub-example/
// mosquitto_pub -h localhost -t alch/FaceInator -m "{\"sender\":\"server\",\"startScanning\":\"1\",\"method\":\"put\"}" -q 1

#include <iostream>
#include <mosquitto.h>
#include <cstring>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

const char *broker_address = "127.0.0.1";
const int broker_port = 1883;
const char *topic = "alch/FaceInator";
const char *serverTopic = "alch/server";
char _cfg_name[] = "FaceInator";

class MosquittoClient
{
public:
    MosquittoClient() : mosq(mosquitto_new(NULL, true, nullptr))
    {
        if (!mosq)
        {
            std::cerr << "Error: Unable to create Mosquitto instance." << std::endl;
            std::exit(1);
        }

        mosquitto_message_callback_set(mosq, message_callback);

        connect();
        subscribe(topic);
    }

    ~MosquittoClient()
    {
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
    }

    void connect()
    {
        if (mosquitto_connect(mosq, broker_address, broker_port, 60) != MOSQ_ERR_SUCCESS)
        {
            std::cerr << "Error: Unable to connect to the broker." << std::endl;
            std::exit(1);
        }
    }

    void subscribe(const char *topic)
    {
        if (mosquitto_subscribe(mosq, nullptr, topic, 0) != MOSQ_ERR_SUCCESS)
        {
            std::cerr << "Error: Unable to subscribe to the topic." << std::endl;
            std::exit(1);
        }
    }

    void publish(const char *topic, const char *message)
    {
        mosquitto_publish(mosq, nullptr, topic, strlen(message), message, 0, false);
    }

    void loopForever()
    {
        mosquitto_loop_forever(mosq, -1, 1);
    }

    void askPlayers()
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "{\"sender\":\"%s\",\"numPlayers\":\"null\",\"method\":\"get\"}", _cfg_name);
        publish(serverTopic, buffer);
    }

    std::string makeMessage(std::string methodinfo, std::string function, std::string functionmessage)
    {
        std::string sender = _cfg_name;

        std::string output = "{\"sender\":\"" + sender + "\",\"" + function + "\":\"" + functionmessage + "\",\"method\":\"" + methodinfo + "\"}";
        std::cout << output << std::endl;
        return output;
    }

private:
    struct mosquitto *mosq;

    static void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
    {
        if (message->payloadlen)
        {
            std::cout << "Received message on topic '" << message->topic << "': " << (char *)message->payload << std::endl;

            // Convert payload to string
            std::string payloadString((char *)message->payload);

            std::cout << "Received payload: " << payloadString << std::endl;

            // Call the function to deconstruct the message
            deconstructMessage(payloadString);
        }
        else
        {
            std::cout << "Received empty message on topic '" << message->topic << "'" << std::endl;
        }
    }

    static void deconstructMessage(const std::string &jsonString)
    {
        try
        {
            // Parse the JSON string
            json j = json::parse(jsonString);

            // Iterate through key-value pairs
            for (auto it = j.begin(); it != j.end(); ++it)
            {
                const std::string &key = it.key();
                const json &value = it.value();

                // Print key and value
                std::cout << "Key: " << key << ", Value: " << value << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        }
    }
};

int main()
{
    mosquitto_lib_init();

    MosquittoClient mosquittoClient;

    mosquittoClient.askPlayers();

    mosquittoClient.makeMessage("info", "setPlayers", "0");

    mosquittoClient.loopForever();

    return 0;
}
