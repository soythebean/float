#include <Arduino.h>
#include <SoftwareSerial.h>
#include "../lib/Time/TimeLib.h"

SoftwareSerial HC12(10, 8);
const int relay1Pin = 2;
const int relay2Pin = 3;
time_t startTime = 0;
bool relay1State = false;
bool relay2State = false;

String company_id;
uint32_t offset = 0;

bool on = true;
time_t startDelay = 10000;
int isSetup = false;

time_t lastSecond = -1;

time_t elapsed() {
    return millis() - startTime;
}

void reset_start() {
    startTime = millis();
}

void delay_start(time_t delay_interval) {
    startTime = millis() + delay_interval;
}

void flash(int d) {
    digitalWrite(13, HIGH);
    delay(d);
    digitalWrite(13, LOW);
    delay(d);
}

struct InitMessage {
    String tag;
    String identifier;
    uint32_t time{};

    InitMessage(const String& tag, const String& identifier, uint32_t time) : tag(tag), identifier(identifier), time(time) {}
};

struct InitMessage decode_init(uint8_t *message, size_t message_size) {
    HC12.println("decode_init");
    for (size_t i = 0; i < message_size; i++) {
        char buf[3];
        sprintf(buf, "%x ", message[i]);
        HC12.print(buf);
    }

    String tag = "";

    for (size_t i = 0; i < 4 && i < message_size; i++) {
        tag += (char)message[i];
    }

    if (tag != "init") {
        HC12.print("Tag mismatch: ");
        HC12.println(tag.c_str());
        return {"", "", 0};
    }

    // Find end of ID
    // Start at beginning of message, continue until 0x02
    size_t id_end_index = 0;
    while (id_end_index < message_size && message[id_end_index] != 0x2) {
        id_end_index++;
    }

    if (id_end_index == message_size) {
        HC12.println("Id end index is end of buffer");
        return {"", "", 0};
    }

    // Read identifier from end of init tag to end of ID or message
    String identifier = "";
    for (size_t i = 4; i < id_end_index && i < message_size; i++) {
        identifier += (char)message[i];
    }

    // Start at beginning of time
    size_t time_start_index = id_end_index + 1;
    uint32_t time = (uint32_t)message[time_start_index + 0]
            | (uint32_t)(message[time_start_index + 1]) << 8
            | (uint32_t)(message[time_start_index + 2]) << 16
            | (uint32_t)(message[time_start_index + 3]) << 24;
    /*for (size_t i = 0; i < 4; i++) {
        char buf[7];
        sprintf(buf, "Tc: %x ", message[time_start_index + i]);
        HC12.print(buf);
        time = time |
        time = time | (message[time_start_index + i] << (i * 8));
    }*/

    return { tag, identifier, time };
}

__attribute__((unused))
void setup() {
    delay(200);

    HC12.begin(9600);

    pinMode(relay1Pin, OUTPUT);
    pinMode(relay2Pin, OUTPUT);
    digitalWrite(relay1Pin, HIGH);
    digitalWrite(relay2Pin, HIGH);
    pinMode(13, OUTPUT);

    setTime(offset);
}

const char *months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

__attribute__((unused))
void loop() {
    if (offset != 0 && second() != lastSecond) {
        char output[64];

        sprintf(output, "%s — %s %i, %04i %02i:%02i:%02i\n",
                company_id.c_str(),
                months[month() - 1],
                day(),
                year(),
                hour(),
                minute(),
                second());

        HC12.print(output);

        lastSecond = second();
    }

    byte buffer[64];
    InitMessage msg = {"", "", 0};
    if (HC12.available()) {
        size_t total = HC12.readBytesUntil(0x4, buffer, 64);
        buffer[total] = 0x04;
        HC12.print("Read ");
        HC12.print(total);
        HC12.println(" bytes");

        msg = decode_init(buffer, total);

        if (msg.time != 0)
        {
            company_id = msg.identifier;
            offset = msg.time;
            HC12.println(offset);
            setTime(offset);
        } else {
            HC12.println("null");
        }
    }

    if (elapsed() < startDelay) return;

    if (!relay1State && !relay2State) {
        digitalWrite(relay1Pin, LOW);
        relay1State = true;
        startTime = millis();
        HC12.println("Relay 1 started");
    }

    if (relay1State && !relay2State) {
        if (elapsed() >= (20000 + startDelay)) {
            digitalWrite(relay1Pin, HIGH);
            digitalWrite(relay2Pin, LOW);
            relay1State = false;
            relay2State = true;
            startTime = millis();
            HC12.println("Relay 2 started");
        }
    }

    if (!relay1State && relay2State) {
        if (elapsed() >= (20000 + startTime)) {
            digitalWrite(relay1Pin, HIGH);
            digitalWrite(relay2Pin, HIGH);
            relay1State = false;
            relay2State = false;

            HC12.println("Stopped");
            reset_start();
        }
    }
}