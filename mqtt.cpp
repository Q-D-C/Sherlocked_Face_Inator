// gcc -std=c++11 -g mqtt.cpp -o mqtt -lstdc++ -lmosquitto

// mosquitto_pub -h localhost -t kitchen/coffeemaker -m "on" -q 1
// mosquitto_sub -h localhost -t test/testing -q 1
// https://cedalo.com/blog/mqtt-subscribe-publish-mosquitto-pub-sub-example/

#include <iostream>
#include <mosquitto.h>
 
const char *broker_address = "127.0.0.1";
const int broker_port = 1883;
const char *topic = "test/testing";  // Replace with the actual topic you want to subscribe to

void message_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    if (message->payloadlen) {
        std::cout << "Received message on topic '" << message->topic << "': " << (char *)message->payload << std::endl;
    } else {
        std::cout << "Received empty message on topic '" << message->topic << "'" << std::endl;
    }
}

int main() {
    mosquitto_lib_init();

    struct mosquitto *mosq = mosquitto_new(NULL, true, nullptr);
    if (!mosq) {
        std::cerr << "Error: Unable to create Mosquitto instance." << std::endl;
        return 1;
    }

    mosquitto_message_callback_set(mosq, message_callback);

    if (mosquitto_connect(mosq, broker_address, broker_port, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Error: Unable to connect to the broker." << std::endl;
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }

    if (mosquitto_subscribe(mosq, nullptr, topic, 0) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Error: Unable to subscribe to the topic." << std::endl;
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }

    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}