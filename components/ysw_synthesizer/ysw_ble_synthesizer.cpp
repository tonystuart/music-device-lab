
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"

#define TAG "BLE"

extern "C" {
#include "esp_log.h"
#include "ysw_common.h"
#include "ysw_synthesizer.h"
#include "ysw_task.h"
#include "ysw_ble_synthesizer.h"
}

static QueueHandle_t input_queue;
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

uint8_t midiPacket[] = {
    0x80,  // header
    0x80,  // timestamp, not implemented 
    0x00,  // status
    0x3c,  // 0x3c == 60 == middle c
    0x00   // velocity
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

static void initialize() {
    BLEDevice::init("ESP32 MIDI Synthesizer Client");

    // Create the BLE Server
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
            BLEUUID(CHARACTERISTIC_UUID),
            BLECharacteristic::PROPERTY_READ   |
            BLECharacteristic::PROPERTY_WRITE  |
            BLECharacteristic::PROPERTY_NOTIFY |
            BLECharacteristic::PROPERTY_WRITE_NR
            );

    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    // Create a BLE Descriptor
    pCharacteristic->addDescriptor(new BLE2902());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->start();
}

static inline void on_note_on(ysw_synthesizer_note_on_t *m)
{
    if (deviceConnected) {
        ESP_LOGD(TAG, "on_note_on channel=%d, note=%d, velocity=%d", m->channel, m->midi_note, m->velocity);
        midiPacket[2] = 0x90 | m->channel;
        midiPacket[3] = m->midi_note;
        midiPacket[4] = m->velocity;
        pCharacteristic->setValue(midiPacket, sizeof(midiPacket));
        pCharacteristic->notify();
    } else {
        ESP_LOGE(TAG, "on_note_on not connected");
    }
}

static inline void on_note_off(ysw_synthesizer_note_off_t *m)
{
    if (deviceConnected) {
        ESP_LOGD(TAG, "on_note_off channel=%d, note=%d", m->channel, m->midi_note);
        midiPacket[2] = 0x80 | m->channel;
        midiPacket[3] = m->midi_note;
        midiPacket[4] = 0;
        pCharacteristic->setValue(midiPacket, sizeof(midiPacket));
        pCharacteristic->notify();
    } else {
        ESP_LOGE(TAG, "on_note_off not connected");
    }
}

static inline void on_program_change(ysw_synthesizer_program_change_t *m)
{
    if (deviceConnected) {
        ESP_LOGD(TAG, "on_program_change channel=%d, program=%d", m->channel, m->program);
        midiPacket[2] = 0xC0 | m->channel;
        midiPacket[3] = m->program;
        midiPacket[4] = 0;
        pCharacteristic->setValue(midiPacket, sizeof(midiPacket));
        pCharacteristic->notify();
    } else {
        ESP_LOGE(TAG, "on_program_change not connected");
    }
}

static void process_message(ysw_synthesizer_message_t *message) {
    switch (message->type) {
        case YSW_SYNTHESIZER_NOTE_ON:
            on_note_on(&message->note_on);
            break;
        case YSW_SYNTHESIZER_NOTE_OFF:
            on_note_off(&message->note_off);
            break;
        case YSW_SYNTHESIZER_PROGRAM_CHANGE:
            on_program_change(&message->program_change);
            break;
        default:
            break;
    }
}

static void run_ble_synth(void* parameters) {
    initialize();
    for (;;) {
        ysw_synthesizer_message_t message;
        BaseType_t is_message = xQueueReceive(input_queue, &message, portMAX_DELAY);
        if (is_message) {
            process_message(&message);
        }
    }
}

QueueHandle_t ysw_ble_synthesizer_create_task() {
    ysw_task_create_standard((char*)TAG, run_ble_synth, &input_queue, sizeof(ysw_synthesizer_message_t));
    return input_queue;
}
