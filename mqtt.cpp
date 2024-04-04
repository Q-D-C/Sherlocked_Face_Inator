// gcc -std=c++14 -g mqtt.cpp -o mqtt -lstdc++ -lmosquitto

// mosquitto_pub -h localhost -t test/testing -m "on" -q 1
// mosquitto_sub -h localhost -t test/testing -q 1
// https://cedalo.com/blog/mqtt-subscribe-publish-mosquitto-pub-sub-example/
// mosquitto_pub -h localhost -t alch/FaceInator -m "{\"sender\":\"server\",\"startScanning\":\"1\",\"method\":\"put\"}" -q 1

#include <iostream>
#include <mosquitto.h>
#include <fstream>
#include <thread>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

constexpr char SCANNINGKEY[] = "scanningComplete";
constexpr char STARTKEY[] = "gameStart";
constexpr char PLAYERSKEY[] = "numPlayers";

constexpr int IDLE = 0;
constexpr int PROCESSING = 1;
constexpr int DONE = 2;

// Define gegevens for MQTT connection
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
    MosquittoClient()
    {
        mosq = mosquitto_new(nullptr, true, nullptr);
        if (!mosq)
        {
            std::cerr << "Error: Unable to create Mosquitto instance." << std::endl;
            exit(1); // Or handle the error as appropriate
        }

        // Set message callback function

        mosquitto_message_callback_set(mosq, message_callback);

        // Subscribe to the specified topic and connect to MQTT
        connect();
        subscribe(topic);
    }

    ~MosquittoClient()
    {
        // Destroy Mosquitto instance and clean up
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
    }

    // Connect to the MQTT broker
    void connect()
    {
        if (mosquitto_connect(mosq, broker_address, broker_port, 60) != MOSQ_ERR_SUCCESS)
        {
            std::cerr << "Error: Unable to connect to the broker." << std::endl;
            std::exit(1);
        }
    }

    // Subscribe to a specific MQTT topic
    void subscribe(const char *topic)
    {
        if (mosquitto_subscribe(mosq, nullptr, topic, 1) != MOSQ_ERR_SUCCESS)
        {
            std::cerr << "Error: Unable to subscribe to the topic." << std::endl;
            std::exit(1);
        }
    }

    // Subscribe to a specific MQTT topic
    static void publish(const char *topic, const std::string &message)
    {
        if (mosq)
        {
            mosquitto_publish(mosq, nullptr, topic, message.length(), message.c_str(), 1, false);
        }
        else
        {
            std::cerr << "Mosquitto instance is not initialized." << std::endl;
        }
    }

    // Listen for MQTT messages for ever and ever and ever and ever and ever and ever and ever
    void listenForever()
    {
        mosquitto_loop_forever(mosq, -1, 1);
    }

    // Main logic of the code, see this as the game loooop
    void logic()
    {
        while (1)
        {
            // If the game has started but we dont know the player count, ask about the player count.
            if (gameStart != "0" && numberPlayers == "0" && PlayersHasBeenAsked == false)
            {
                this->askPlayers();
                PlayersHasBeenAsked = true;
                std::cout << "Player amount has been aksed" << std::endl;
            }

            // Check if scanning has been completed
            this->checkScan();
        }
    }

    static MosquittoClient *getInstance()
    {
        if (instance == nullptr)
        {
            instance = new MosquittoClient();
        }
        return instance;
    }

    // Lots of getters and setters, probbly not needed but here just in case!
    static void setNumberPlayers(const std::string &value) { numberPlayers = value; }
    static void setScanningComplete(const std::string &value) { scanningComplete = value; }
    static void setGameStart(const std::string &value) { gameStart = value; }

    static std::string getNumberPlayers() { return numberPlayers; }
    static std::string getScanningComplete() { return scanningComplete; }
    static std::string getGameStart() { return gameStart; }

    // Private copy constructor and assignment operator to prevent multiple instances
    MosquittoClient(const MosquittoClient &) = delete;
    MosquittoClient &operator=(const MosquittoClient &) = delete;

private:
    static MosquittoClient *instance;
    static struct mosquitto *mosq;
    static std::string numberPlayers;
    static std::string scanningComplete;
    static std::string gameStart;
    bool PlayersHasBeenAsked = false;

    // Write value to a file
    static void fileWriter(std::string value, std::string name)
    {
        FileHandler::writeToFile(value, name);
    }

    // Read value from a file
    std::string fileReader(std::string name)
    {
        return FileHandler::readFromFile(name);
    }

    // Ask ACE for the player count
    void askPlayers()
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "{\"sender\":\"%s\",\"numPlayers\":\"null\",\"method\":\"get\"}", _cfg_name);
        this->publish(serverTopic, buffer);
    }

    static std::string makeMessage(std::string sender, std::string method, int id, int value)
    {
        // Directly create the JSON object with id and value inside
        json message = {
            {"sender", sender},
            {"method", method},
            {"outputs", json::array({{{"id", id}, {"value", value}}})}};

        // Convert the JSON object to a string
        std::string output = message.dump(); // Use dump(4) for pretty printing with indent of 4 spaces
        std::cout << "sending: " << output << std::endl;
        return output;
    }

    // Check if scanning (done externally) has been completed
    void checkScan()
    {

        std::string Value;

        Value = fileReader(SCANNINGKEY);

        if (Value == "1")
        {
            std::cout << "SCANNING HASZ BEEN CXOMPLETED" << std::endl;
            // Tell ACE we are allll done here
            std::string message = makeMessage(_cfg_name, "info", 1, DONE);
            publish(serverTopic, message.c_str());

            // Reset all the states, ready for another round
            // gameStart = "0";
            // numberPlayers = "0";
            PlayersHasBeenAsked = false;
            fileWriter("0", SCANNINGKEY);
            fileWriter("0", PLAYERSKEY);
            fileWriter("0", STARTKEY);
        }
    }

    // Process recieved messages
    // static void doStuff(json Data)
    // {
    //     try
    //     {
    //         // Access jsonData and perform actions based on keys
    //         if (Data.count(PLAYERSKEY) != 0)
    //         {
    //             // Key exists, extract the value
    //             numberPlayers = Data[PLAYERSKEY];
    //             std::cout << "Value for key numPlayers: " << numberPlayers << std::endl;
    //             fileWriter(numberPlayers, PLAYERSKEY);
    //         }
    //         else if (Data.count(STARTKEY) != 0)
    //         {
    //             // Key exists, extract the value
    //             gameStart = Data[STARTKEY];
    //             std::cout << "Value for key gameStart: " << gameStart << std::endl;
    //             fileWriter(gameStart, STARTKEY);
    //         }
    //         else
    //         {
    //             std::cout << "no known key found" << std::endl;
    //         }
    //     }

    //     catch (const std::exception &e)
    //     {
    //         std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    //     }
    // }

    static void doStuff(const json &Data)
    {
        try
        {
            // Check if the message is from itself
            if (Data.contains("sender") && Data["sender"] == _cfg_name)
            {
                std::cout << "Message from self, ignoring." << std::endl;
                return; // Skip processing if the message is from itself
            }
            if (Data.contains("outputs") && Data["outputs"].is_array())
            {
                for (const auto &output : Data["outputs"])
                {
                    if (output.contains("id") && output.contains("value"))
                    {
                        int id = output["id"];
                        int value = output["value"];

                        // Check for specific id-value combinations
                        if (id == 1 && value == 1) // For example, id=1 and value=1 means start game
                        {
                            std::cout << "Game start command received." << std::endl;
                            // Example: set gameStart to true, write to file, etc.
                            fileWriter("1", STARTKEY);
                            std::string message = makeMessage(_cfg_name, "info", 1, PROCESSING);
                            publish(serverTopic, message.c_str());
                            gameStart = 1;
                        }
                    }
                }
            }
            else if (Data.count(PLAYERSKEY) != 0)
            {
                // Key exists, extract the value
                numberPlayers = Data[PLAYERSKEY];
                std::cout << "Value for key numPlayers: " << numberPlayers << std::endl;
                fileWriter(numberPlayers, PLAYERSKEY);
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

    // Callback function for MQTT messages
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
                std::cout << "recieved json: " << recievedData << std::endl;

                // doStuff with the recieved string
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

// Static member variable initialization
std::string MosquittoClient::numberPlayers = "0";
std::string MosquittoClient::scanningComplete = "0";
std::string MosquittoClient::gameStart = "0";

MosquittoClient *MosquittoClient::instance = nullptr;
struct mosquitto *MosquittoClient::mosq = nullptr;

int main()
{
    // Initialize Mosquitto library
    mosquitto_lib_init();

    // Create an instance of MosquittoClient
    MosquittoClient mosquittoClient;

    // Create a new thread to run MQTT for ever and ever and ever and ever and ever and ever
    std::thread mqttThread([&]()
                           { mosquittoClient.listenForever(); });

    // Run the main logic of the application
    mosquittoClient.logic();

    // Join the thread with the main thread
    mqttThread.join();

    return 0;
}
