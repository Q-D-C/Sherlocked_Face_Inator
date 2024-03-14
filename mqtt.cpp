// gcc -std=c++11 -g mqtt.cpp -o mqtt -lstdc++ -lmosquitto

// mosquitto_pub -h localhost -t test/testing -m "on" -q 1
// mosquitto_sub -h localhost -t test/testing -q 1
// https://cedalo.com/blog/mqtt-subscribe-publish-mosquitto-pub-sub-example/
// mosquitto_pub -h localhost -t alch/FaceInator -m "{\"sender\":\"server\",\"startScanning\":\"1\",\"method\":\"put\"}" -q 1

#include <iostream>
#include <mosquitto.h>
// #include <cstring>
#include <fstream>
#include <thread>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

#define SCANNINGKEY "scanningComplete"
#define STARTKEY "gameStart"
#define PLAYERSKEY "numPlayers"

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

    void listenForever()
    {
        mosquitto_loop_forever(mosq, -1, 1);
    }

    void logic()
    {
        while (1)
        {

            if (gameStart != "0" && numberPlayers == "0" && PlayersHasBeenAsked == false)
            {
                this->askPlayers();
                PlayersHasBeenAsked = true;
                std::cout << "player amount has been aksed" << std::endl;
            }

            this->checkScan();
        }
    }

    void askPlayers()
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "{\"sender\":\"%s\",\"numPlayers\":\"null\",\"method\":\"get\"}", _cfg_name);
        this->publish(serverTopic, buffer);
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
    bool PlayersHasBeenAsked = false;

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
            std::cout << "Value has been stored in " << fileName << std::endl;
        }
        else
        {
            std::cerr << "Unable to open the file for writing." << std::endl;
        }
    }

    std::string fileReader(std::string name)
    {
        std::string fileName;

        fileName = name + ".txt";

        std::string Value;
        std::ifstream inFile(fileName);
        if (inFile.is_open())
        {
            inFile >> Value;
            inFile.close();
        }
        else
        {
            std::cerr << "Unable to open the file for reading." << std::endl;
        }

        return Value;
    }

    void checkScan()
    {

        std::string Value;

        Value = fileReader(SCANNINGKEY);

        // std::cout << Value << std::endl;

        if (Value == "1")
        {
            std::cout << "SCANNING HASZ BEEN CXOMPLETED" << std::endl;
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "{\"sender\":\"%s\",\"scanningComplete\":\"1\",\"method\":\"info\"}", _cfg_name);
            this->publish(serverTopic, buffer);
            gameStart = "0";
            numberPlayers = "0";
            PlayersHasBeenAsked = false;
            fileWriter("0", SCANNINGKEY);
        }
    }

    static void doStuff(json Data)
    {
        try
        {

            // Access jsonData and perform actions based on keys
            if (Data.count(PLAYERSKEY) != 0)
            {
                // Key exists, extract the value
                numberPlayers = Data[PLAYERSKEY];
                std::cout << "Value for key numPlayers: " << numberPlayers << std::endl;
                fileWriter(numberPlayers, PLAYERSKEY);
            }
            else if (Data.count(STARTKEY) != 0)
            {
                // Key exists, extract the value
                gameStart = Data[STARTKEY];
                std::cout << "Value for key gameStart: " << gameStart << std::endl;
                fileWriter(gameStart, STARTKEY);
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

    static void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
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

    // mosquittoClient.askPlayers();

    // mosquittoClient.makeMessage("info", "setPlayers", "0");

    // Create a new thread to run mosquitto_loop_forever()
    std::thread mqttThread([&]()
                           { mosquittoClient.listenForever(); });

    mosquittoClient.logic();

    // Join the thread with the main thread
    mqttThread.join();

    return 0;
}
