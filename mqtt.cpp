// gcc -std=c++11 -g mqtt.cpp -o mqtt -lstdc++ -lmosquitto

// mosquitto_pub -h localhost -t test/testing -m "on" -q 1
// mosquitto_sub -h localhost -t test/testing -q 1
// https://cedalo.com/blog/mqtt-subscribe-publish-mosquitto-pub-sub-example/
// mosquitto_pub -h localhost -t alch/FaceInator -m "{\"sender\":\"server\",\"startScanning\":\"1\",\"method\":\"put\"}" -q 1

#include <iostream>
#include <mosquitto.h>
#include <cstring>
#include <fstream>
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

        // Initialize the outputFiles unordered_map
        outputFiles.insert({"numPlayers", std::ofstream("numPlayers.txt", std::ofstream::out)});
        outputFiles.insert({"gameStart", std::ofstream("gameStart.txt", std::ofstream::out)});

        connect();
        subscribe(topic);
    }

    ~MosquittoClient()
    {
        // Close the files in the destructor
        for (auto &file : outputFiles)
        {
            file.second.close();
        }

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

    static void setNumberPlayers(const std::string &value) { numberPlayers = value; }

    static void setScanningComplete(const std::string &value) { scanningComplete = value; }

    static void setGameStart(const std::string &value) { gameStart = value; }

    static std::string getNumberPlayers() { return numberPlayers; }

    static std::string getScanningComplete() { return scanningComplete; }

    static std::string getGameStart() { return gameStart; }

private:
    struct mosquitto *mosq;
    static std::string numberPlayers;
    static std::string scanningComplete;
    static std::string gameStart;

    std::unordered_map<std::string, std::ofstream> outputFiles;

    static void fileWriter(std::string value, std::string name)
    {
        std::string fileName;

        fileName = name + ".txt";

        std::ofstream outFile(fileName);
        if (outFile.is_open())
        {
            outFile << value;
            outFile.close();
            std::cout << "Value has been stored in the file." << std::endl;
        }
        else
        {
            std::cerr << "Unable to open the file." << std::endl;
        }
    }

    static void doStuff(json Data)
    {
        try
        {

            // Access jsonData and perform actions based on keys
            if (Data.count("numPlayers") != 0)
            {
                // Key exists, extract the value
                numberPlayers = Data["numPlayers"];
                std::cout << "Value for key numPlayers:" << numberPlayers << std::endl;
                fileWriter(numberPlayers, "numPlayers");
            }
            else if (Data.count("gameStart") != 0)
            {
                // Key exists, extract the value
                gameStart = Data["gameStart"];
                std::cout << "Value for key gameStart: " << gameStart << std::endl;
                fileWriter(gameStart, "gameStart");
            }
            else
            {
                std::cout << "no known key found" << std::endl;
            }
        }

        catch (const std::exception &e)
        {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        }
    }

    static void
    message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
    {
        json recievedData;
        if (message->payloadlen)
        {
            // Assuming the received message is a JSON string
            std::string jsonStr(static_cast<const char *>(message->payload), message->payloadlen);

            try
            {
                // Parse the received JSON string
                recievedData = nlohmann::json::parse(jsonStr);
                // Now 'receivedJson' contains the parsed JSON data

                std::cout << "recieved json: " << recievedData << std::endl;

                // Call doStuff with the payload string
                doStuff(recievedData);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            }
        }
    }

    static json deconstructMessage(const std::string &jsonString)
    {
        json messageData;

        try
        {
            // Parse the JSON string
            messageData = json::parse(jsonString);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        }

        return messageData;
    }
};

std::string MosquittoClient::numberPlayers = "0";
std::string MosquittoClient::scanningComplete = "0";
std::string MosquittoClient::gameStart = "0";

int main()
{
    mosquitto_lib_init();

    MosquittoClient mosquittoClient;

    mosquittoClient.askPlayers();

    // mosquittoClient.makeMessage("info", "setPlayers", "0");

    mosquittoClient.loopForever();

    return 0;
}
