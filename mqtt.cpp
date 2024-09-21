#include <iostream>
#include <mosquitto.h>
#include <fstream>
#include <thread>
#include <cstring>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
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
const char *topic = "alch/faceinator";
const char *serverTopic = "alch";
char _cfg_name[] = "faceinator";

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
        sendDisconnectionMessage(); // Send disconnection message
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

    static std::string getIPAddress()
    {
        struct ifaddrs *ifaddr, *ifa;
        char ip[NI_MAXHOST];

        if (getifaddrs(&ifaddr) == -1)
        {
            perror("getifaddrs");
            exit(EXIT_FAILURE);
        }

        std::string ipAddress = "127.0.0.1"; // Default to localhost

        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr)
                continue;

            int family = ifa->ifa_addr->sa_family;
            if (family == AF_INET)
            {
                int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ip, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                if (s != 0)
                {
                    std::cerr << "getnameinfo() failed: " << gai_strerror(s) << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (strcmp(ifa->ifa_name, "lo") != 0) // Skip loopback interface
                {
                    ipAddress = ip;
                    break;
                }
            }
        }

        freeifaddrs(ifaddr);
        return ipAddress;
    }

    void sendInfoMessage()
    {
        std::string ip = getIPAddress();
        json message = {
            {"sender", _cfg_name},
            {"connected", true},
            {"ip", ip},
            {"version", "v0.1.0"},
            {"method", "info"},
            {"trigger", "startup"}};
        publish(serverTopic, message.dump());
    }

    void sendDisconnectionMessage()
    {
        json message = {
            {"sender", _cfg_name},
            {"connected", false},
            {"method", "info"}};
        publish(serverTopic, message.dump());
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
            else if (data.contains("method") && data["method"] == "get" && data.contains("info") && data["info"] == "system")
            {
                std::string ip = getIPAddress();
                json message = {
                    {"sender", _cfg_name},
                    {"connected", true},
                    {"ip", ip},
                    {"version", "v0.1.0"},
                    {"method", "info"},
                    {"trigger", "request"}};
                publish(serverTopic, message.dump());
            }
            else if (data.contains("method") && data["method"] == "put" && data.contains("outputs") && data["outputs"] == "reset")
            {
                std::cout << "Reset command received." << std::endl;
                std::string message = makeMessage(_cfg_name, "info", 1, IDLE);
                resetStates();
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error handling message: " << e.what() << std::endl;
        }
    }

    std::string getGameStart()
    {
        return gameStart;
    }

    void resetInternalValues()
    {
        numberPlayers = "0";
        scanningComplete = "0";
        gameStart = "0";
        PlayersHasBeenAsked = false;
        ScanningHasBeenInformed = false;
    }

    void resetStates()
    {
        resetInternalValues();
        FileHandler::writeToFile("0", SCANNINGKEY);
        FileHandler::writeToFile("0", PLAYERSKEY);
        FileHandler::writeToFile("0", STARTKEY);
        FileHandler::writeToFile("0", DONEKEY);
    }

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

    GameLogic(MosquittoClient *client) : mosqClient(client) {}

    void logic()
    {
        while (true)
        {
            // Only ask players if the game has started and we haven't asked for players yet
            if (mosqClient->getGameStart() == "1" && !mosqClient->PlayersHasBeenAsked)
            {
                askPlayers();
                std::cout << "Players have been asked" << std::endl;
                mosqClient->PlayersHasBeenAsked = true;
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
        if (value == "1" && !mosqClient->ScanningHasBeenInformed)
        {
            std::cout << "scanningcomplete.txt = 1" << std::endl;
            std::string message = MosquittoClient::makeMessage(_cfg_name, "info", 1, DONE);
            MosquittoClient::publish(serverTopic, message);
            mosqClient->ScanningHasBeenInformed = true;
        }
    }

    void checkDone()
    {
        std::string value = FileHandler::readFromFile(DONEKEY);
        if (value == "1")
        {
            std::cout << "done.txt = 1" << std::endl;
            std::string message = MosquittoClient::makeMessage(_cfg_name, "info", 1, IDLE);
            MosquittoClient::publish(serverTopic, message);
            resetStates();
        }
    }

    void resetStates()
    {
        mosqClient->resetInternalValues();
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
