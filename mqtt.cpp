// gcc -std=c++14 -g mqtt.cpp -o mqtt -lstdc++ -lmosquitto

// mosquitto_sub -v -t '#'
// https://cedalo.com/blog/mqtt-subscribe-publish-mosquitto-pub-sub-example/
// mosquitto_pub -h localhost -t alch/FaceInator -m "{\"sender\":\"server\",\"numPlayers\":\"3\",\"method\":\"put\"}" -q 1
// mosquitto_pub -h localhost -t alch/FaceInator -m "{\"sender\":\"server\", \"method\":\"put\", \"outputs\":[{\"id\":1, \"value\":1}]}" -q 1


#include <iostream>
#include <mosquitto.h>
#include <fstream>
#include <thread>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Constants
constexpr char SCANNINGKEY[] = "scanningComplete";
constexpr char STARTKEY[] = "gameStart";
constexpr char PLAYERSKEY[] = "numPlayers";
constexpr char DONEKEY[] = "done";

constexpr int IDLE = 0;
constexpr int PROCESSING = 1;
constexpr int DONE = 2;

const char *broker_address = "127.0.0.1";
const int broker_port = 1883;
const char *topic = "alch/FaceInator";
const char *serverTopic = "alch/server";
char _cfg_name[] = "FaceInator";

class FileHandler
{
public:
    static void writeToFile(const std::string &value, const std::string &name)
    {
        std::ofstream outFile(name + ".txt");
        if (outFile.is_open())
        {
            outFile << value;
            outFile.close();
            std::cout << "Value has been stored in " << name << ".txt" << std::endl;
        }
        else
        {
            std::cerr << "Unable to open the file for writing." << std::endl;
        }
    }

    static std::string readFromFile(const std::string &name)
    {
        std::ifstream inFile(name + ".txt");
        std::string value;
        if (inFile.is_open())
        {
            inFile >> value;
            inFile.close();
        }
        else
        {
            std::cerr << "Unable to open the file for reading." << std::endl;
        }
        return value;
    }
};

class MosquittoClient
{
public:
    static MosquittoClient *instance;
    struct mosquitto *mosq;
    static std::string numberPlayers;
    static std::string scanningComplete;
    static std::string gameStart;
    bool PlayersHasBeenAsked = false;
    bool ScanningHasBeenInformed = false;

    MosquittoClient()
    {
        mosq = mosquitto_new(nullptr, true, nullptr);
        if (!mosq)
        {
            std::cerr << "Error: Unable to create Mosquitto instance." << std::endl;
            exit(1);
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
            exit(1);
        }
    }

    void subscribe(const char *topic)
    {
        if (mosquitto_subscribe(mosq, nullptr, topic, 1) != MOSQ_ERR_SUCCESS)
        {
            std::cerr << "Error: Unable to subscribe to the topic." << std::endl;
            exit(1);
        }
    }

    void listenForever()
    {
        mosquitto_loop_forever(mosq, -1, 1);
    }

    static void publish(const char *topic, const std::string &message)
    {
        if (instance && instance->mosq)
        {
            mosquitto_publish(instance->mosq, nullptr, topic, message.length(), message.c_str(), 1, false);
        }
        else
        {
            std::cerr << "Mosquitto instance is not initialized." << std::endl;
        }
    }

    static std::string makeMessage(std::string sender, std::string method, int id, int value)
    {
        json message = {
            {"sender", sender},
            {"method", method},
            {"outputs", json::array({{{"id", id}, {"value", value}}})}};
        return message.dump();
    }

    static void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
    {
        if (message->payloadlen)
        {
            std::string jsonStr(static_cast<const char *>(message->payload), message->payloadlen);
            std::cout << "Received message on topic: " << message->topic << std::endl;
            std::cout << "Message payload: " << jsonStr << std::endl;

            try
            {
                json receivedData = json::parse(jsonStr);
                std::cout << "Parsed JSON: " << receivedData.dump(4) << std::endl;
                if (instance)
                {
                    instance->handleMessage(receivedData);
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            }
        }
        else
        {
            std::cout << "Message with empty payload received on topic: " << message->topic << std::endl;
        }
    }

    void handleMessage(const json &data)
    {
        try
        {
            if (data.contains("sender") && data["sender"] == _cfg_name)
            {
                std::cout << "Message from self, ignoring." << std::endl;
                return;
            }
            if (data.contains("outputs") && data["outputs"].is_array())
            {
                for (const auto &output : data["outputs"])
                {
                    if (output.contains("id") && output.contains("value"))
                    {
                        int id = output["id"];
                        int value = output["value"];
                        if (id == 1 && value == 1)
                        {
                            std::cout << "Game start command received." << std::endl;
                            FileHandler::writeToFile("1", STARTKEY);
                            std::string message = makeMessage(_cfg_name, "info", 1, PROCESSING);
                            publish(serverTopic, message);
                            gameStart = "1";
                        }
                    }
                }
            }
            else if (data.contains(PLAYERSKEY))
            {
                numberPlayers = data[PLAYERSKEY].get<std::string>();
                std::cout << "Value for key numPlayers: " << numberPlayers << std::endl;
                FileHandler::writeToFile(numberPlayers, PLAYERSKEY);
            }
            else
            {
                std::cout << "No 'outputs' key found or 'outputs' is not an array." << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        }
    }

    static std::string getNumberPlayers() { return numberPlayers; }
    static std::string getScanningComplete() { return scanningComplete; }
    static std::string getGameStart() { return gameStart; }

    static MosquittoClient *getInstance()
    {
        if (!instance)
        {
            instance = new MosquittoClient();
        }
        return instance;
    }
};

MosquittoClient *MosquittoClient::instance = nullptr;
std::string MosquittoClient::numberPlayers = "0";
std::string MosquittoClient::scanningComplete = "0";
std::string MosquittoClient::gameStart = "0";

class GameLogic
{
public:
    MosquittoClient *mosqClient;
    bool PlayersHasBeenAsked = false;
    bool ScanningHasBeenInformed = false;

    GameLogic(MosquittoClient *client) : mosqClient(client) {}

    void logic()
    {
        while (true)
        {
            if (mosqClient->getGameStart() != "0" && mosqClient->getNumberPlayers() == "0" && !PlayersHasBeenAsked)
            {
                askPlayers();
                std::cout << "Players has been asked" << std::endl;
                PlayersHasBeenAsked = true;
            }
            checkScan();
            checkDone();
        }
    }

    void askPlayers()
    {
        json message = {
            {"sender", _cfg_name},
            {"numPlayers", nullptr}, // Using nullptr to denote null in JSON
            {"method", "get"}};
        std::string messageStr = message.dump();                   // Serialize JSON object to string
        MosquittoClient::publish(serverTopic, messageStr.c_str()); // Publish the JSON string
    }

    void checkScan()
    {
        std::string value = FileHandler::readFromFile(SCANNINGKEY);
        if (value == "1" && !ScanningHasBeenInformed)
        {
            std::string message = MosquittoClient::makeMessage(_cfg_name, "info", 1, DONE);
            MosquittoClient::publish(serverTopic, message);
            ScanningHasBeenInformed = true;
        }
    }

    void checkDone()
    {
        std::string value = FileHandler::readFromFile(DONEKEY);
        if (value == "1")
        {
            std::string message = MosquittoClient::makeMessage(_cfg_name, "info", 1, IDLE);
            MosquittoClient::publish(serverTopic, message);
            resetStates();
        }
    }

    void resetStates()
    {
        PlayersHasBeenAsked = false;
        ScanningHasBeenInformed = false;
        FileHandler::writeToFile("0", SCANNINGKEY);
        FileHandler::writeToFile("0", PLAYERSKEY);
        FileHandler::writeToFile("0", STARTKEY);
        FileHandler::writeToFile("0", DONEKEY);
    }
};

int main()
{
    mosquitto_lib_init();
    MosquittoClient *mosquittoClient = MosquittoClient::getInstance();
    GameLogic gameLogic(mosquittoClient);
    std::thread mqttThread([&]()
                           { mosquittoClient->listenForever(); });
    gameLogic.logic();
    mqttThread.join();
    return 0;
}
