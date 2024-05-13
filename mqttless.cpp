//g++ -std=c++14 mqttless.cpp -o mqttless

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <string>

class SimpleGameStateManager
{
public:
    void run()
    {
        std::string input = "0";
        resetGame();
        while (true)
        {
            while (input == "0")
            {
                std::cout << "Enter the number of players (or 'exit' to quit): ";

                std::cin >> input; // Read user input to update the game state

                if (input == "exit")
                {
                    std::cout << "Exiting. Resetting game state..." << std::endl;
                    resetGame();
                    return; // Exit game
                }

                // Assume valid input for the number of players or reset if needed
                if (!input.empty() && input != "0")
                {
                    std::cout << "processing" << std::endl;
                    writeToFile(input, "numPlayers.txt");
                    writeToFile("1", "gameStart.txt");
                }
                else
                {
                    // Invalid input or "0", reset and ask again
                    input = "0";
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            while (input != "0")
            {
                std::cin >> input; // Read user input to update the game state

                if (input == "exit")
                {
                    std::cout << "Exiting. Resetting game state..." << std::endl;
                    resetGame();
                    return; // Exit game
                }

                if (readFromFile("scanningComplete.txt") == "1")
                {
                    // Reset game state
                    std::cout << "Scanning complete. Resetting game state..." << std::endl;
                    resetGame();
                    input = "0";
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    void run2()
    {
        std::string input = "0";
        while (true)
        {

            while (input == "0")
            {
                std::cout << "Enter the number of players (or 'exit' to quit): ";

                std::cin >> input;

                if (input == "exit")
                {
                    std::cout << "Exiting. Resetting game state..." << std::endl;
                    resetGame();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::cout << "processing" << std::endl;

            // Update numPlayers.txt
            writeToFile(input, "numPlayers.txt");
            writeToFile("1", "gameStart.txt");

            std::this_thread::sleep_for(std::chrono::seconds(1));

std::cout << "brrrr" << std::endl;

            while (input != "0")
            {

                // Periodically check if scanning is complete
                std::string test = readFromFile("scanningComplete.txt");
                std::cout << test << std::endl;
                if (readFromFile("scanningComplete.txt") == "1")
                {
                    // Reset game state
                    std::cout << "Generating complete. Resetting game state..." << std::endl;
                    resetGame();
                    input = "0";
                }

                // Adding delay to avoid spinning too fast on file reads
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

private:
    void resetGame()
    {
        writeToFile("0", "numPlayers.txt");
        writeToFile("0", "scanningComplete.txt");
        writeToFile("0", "gameStart.txt");
        writeToFile("0", "done.txt");
    }

    void writeToFile(const std::string &value, const std::string &fileName)
    {
        std::ofstream outFile(fileName);
        if (outFile.is_open())
        {
            outFile << value;
            outFile.close();
        }
        else
        {
            std::cerr << "Unable to open " << fileName << " for writing." << std::endl;
        }
    }

    std::string readFromFile(const std::string &fileName)
    {
        std::ifstream inFile(fileName);
        std::string value;
        if (inFile.is_open())
        {
            inFile >> value;
            inFile.close();
            return value;
        }
        else
        {
            std::cerr << "Unable to open " << fileName << " for reading." << std::endl;
            return "";
        }
    }
};

int main()
{
    SimpleGameStateManager manager;
    manager.run();
    return 0;
}
