
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"

#define TAG "YSW_BT_SYNTH"

extern "C" {
#include "ysw_task.h"
#include "ysw_common.h"
#include "ysw_heap.h"
#include "ysw_event.h"
#include "ysw_bt_synth.h"
#include "esp_log.h"
}

typedef struct {
    BLECharacteristic *pCharacteristic;
    bool deviceConnected = false;
    uint8_t midiPacket[5];
} ysw_bt_synth_t;

class MyServerCallbacks: public BLEServerCallbacks {
    ysw_bt_synth_t *ysw_bt_synth;

    public:
    MyServerCallbacks(ysw_bt_synth_t *ysw_bt_synth) {
        this->ysw_bt_synth = ysw_bt_synth;
    }

    void onConnect(BLEServer* pServer) {
        ysw_bt_synth->deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        ysw_bt_synth->deviceConnected = false;
    }
};

static void initialize_synthesizer(ysw_bt_synth_t *ysw_bt_synth) {
    BLEDevice::init("ESP32 MIDI");

    // Create the BLE Server
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks(ysw_bt_synth));

    // Create the BLE Service
    BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

    // Create a BLE Characteristic
    ysw_bt_synth->pCharacteristic = pService->createCharacteristic(
            BLEUUID(CHARACTERISTIC_UUID),
            BLECharacteristic::PROPERTY_READ   |
            BLECharacteristic::PROPERTY_WRITE  |
            BLECharacteristic::PROPERTY_NOTIFY |
            BLECharacteristic::PROPERTY_WRITE_NR
            );

    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    // Create a BLE Descriptor
    ysw_bt_synth->pCharacteristic->addDescriptor(new BLE2902());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->start();
}

static inline void on_note_on(ysw_bt_synth_t *ysw_bt_synth, ysw_event_note_on_t *m)
{
    if (ysw_bt_synth->deviceConnected) {
        ESP_LOGD(TAG, "on_note_on channel=%d, note=%d, velocity=%d", m->channel, m->midi_note, m->velocity);
        ysw_bt_synth->midiPacket[0] = 0x80; // header;
        ysw_bt_synth->midiPacket[1] = 0x80; // timestamp (not implemented);
        ysw_bt_synth->midiPacket[2] = 0x90 | m->channel;
        ysw_bt_synth->midiPacket[3] = m->midi_note;
        ysw_bt_synth->midiPacket[4] = m->velocity;
        ysw_bt_synth->pCharacteristic->setValue(ysw_bt_synth->midiPacket, sizeof(ysw_bt_synth->midiPacket));
        ysw_bt_synth->pCharacteristic->notify();
    } else {
        ESP_LOGE(TAG, "on_note_on not connected");
    }
}

static inline void on_note_off(ysw_bt_synth_t *ysw_bt_synth, ysw_event_note_off_t *m)
{
    if (ysw_bt_synth->deviceConnected) {
        ESP_LOGD(TAG, "on_note_off channel=%d, note=%d", m->channel, m->midi_note);
        ysw_bt_synth->midiPacket[0] = 0x80; // header;
        ysw_bt_synth->midiPacket[1] = 0x80; // timestamp (not implemented);
        ysw_bt_synth->midiPacket[2] = 0x80 | m->channel;
        ysw_bt_synth->midiPacket[3] = m->midi_note;
        ysw_bt_synth->midiPacket[4] = 0;
        ysw_bt_synth->pCharacteristic->setValue(ysw_bt_synth->midiPacket, sizeof(ysw_bt_synth->midiPacket));
        ysw_bt_synth->pCharacteristic->notify();
    } else {
        ESP_LOGE(TAG, "on_note_off not connected");
    }
}

static inline void on_program_change(ysw_bt_synth_t *ysw_bt_synth, ysw_event_program_change_t *m)
{
    if (ysw_bt_synth->deviceConnected) {
        ESP_LOGD(TAG, "on_program_change channel=%d, program=%d", m->channel, m->program);
        ysw_bt_synth->midiPacket[0] = 0x80; // header;
        ysw_bt_synth->midiPacket[1] = 0x80; // timestamp (not implemented);
        ysw_bt_synth->midiPacket[2] = 0xC0 | m->channel;
        ysw_bt_synth->midiPacket[3] = m->program;
        ysw_bt_synth->midiPacket[4] = 0;
        ysw_bt_synth->pCharacteristic->setValue(ysw_bt_synth->midiPacket, sizeof(ysw_bt_synth->midiPacket));
        ysw_bt_synth->pCharacteristic->notify();
    } else {
        ESP_LOGE(TAG, "on_program_change not connected");
    }
}

static void process_event(void *context, ysw_event_t *event) {
    ysw_bt_synth_t *ysw_bt_synth = (ysw_bt_synth_t*)context;
    switch (event->header.type) {
        case YSW_EVENT_NOTE_ON:
            on_note_on(ysw_bt_synth, &event->note_on);
            break;
        case YSW_EVENT_NOTE_OFF:
            on_note_off(ysw_bt_synth, &event->note_off);
            break;
        case YSW_EVENT_PROGRAM_CHANGE:
            on_program_change(ysw_bt_synth, &event->program_change);
            break;
        default:
            break;
    }
}

void ysw_bt_synth_create_task(ysw_bus_t *bus)
{
    ysw_bt_synth_t *ysw_bt_synth = (ysw_bt_synth_t*)ysw_heap_allocate(sizeof(ysw_bt_synth_t));
    initialize_synthesizer(ysw_bt_synth);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.context = ysw_bt_synth;

    ysw_task_t *task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_EDITOR);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
}

