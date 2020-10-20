
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"

#define TAG "YSW_SYNTH_BT"

extern "C" {
#include "ysw_task.h"
#include "ysw_common.h"
#include "ysw_heap.h"
#include "ysw_event.h"
#include "ysw_synth_bt.h"
#include "esp_log.h"
}

typedef struct {
    BLECharacteristic *pCharacteristic;
    bool deviceConnected = false;
    uint8_t midiPacket[5];
} context_t;

class MyServerCallbacks: public BLEServerCallbacks {
    context_t *context;

    public:
    MyServerCallbacks(context_t *context) {
        this->context = context;
    }

    void onConnect(BLEServer* pServer) {
        context->deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        context->deviceConnected = false;
    }
};

static void initialize_synthesizer(context_t *context) {
    BLEDevice::init("ESP32 MIDI");

    // Create the BLE Server
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks(context));

    // Create the BLE Service
    BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

    // Create a BLE Characteristic
    context->pCharacteristic = pService->createCharacteristic(
            BLEUUID(CHARACTERISTIC_UUID),
            BLECharacteristic::PROPERTY_READ   |
            BLECharacteristic::PROPERTY_WRITE  |
            BLECharacteristic::PROPERTY_NOTIFY |
            BLECharacteristic::PROPERTY_WRITE_NR
            );

    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    // Create a BLE Descriptor
    context->pCharacteristic->addDescriptor(new BLE2902());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->start();
}

static inline void on_note_on(context_t *context, ysw_event_note_on_t *m)
{
    if (context->deviceConnected) {
        ESP_LOGD(TAG, "on_note_on channel=%d, note=%d, velocity=%d", m->channel, m->midi_note, m->velocity);
        context->midiPacket[0] = 0x80; // header;
        context->midiPacket[1] = 0x80; // timestamp (not implemented);
        context->midiPacket[2] = 0x90 | m->channel;
        context->midiPacket[3] = m->midi_note;
        context->midiPacket[4] = m->velocity;
        context->pCharacteristic->setValue(context->midiPacket, sizeof(context->midiPacket));
        context->pCharacteristic->notify();
    } else {
        ESP_LOGE(TAG, "on_note_on not connected");
    }
}

static inline void on_note_off(context_t *context, ysw_event_note_off_t *m)
{
    if (context->deviceConnected) {
        ESP_LOGD(TAG, "on_note_off channel=%d, note=%d", m->channel, m->midi_note);
        context->midiPacket[0] = 0x80; // header;
        context->midiPacket[1] = 0x80; // timestamp (not implemented);
        context->midiPacket[2] = 0x80 | m->channel;
        context->midiPacket[3] = m->midi_note;
        context->midiPacket[4] = 0;
        context->pCharacteristic->setValue(context->midiPacket, sizeof(context->midiPacket));
        context->pCharacteristic->notify();
    } else {
        ESP_LOGE(TAG, "on_note_off not connected");
    }
}

static inline void on_program_change(context_t *context, ysw_event_program_change_t *m)
{
    if (context->deviceConnected) {
        ESP_LOGD(TAG, "on_program_change channel=%d, program=%d", m->channel, m->program);
        context->midiPacket[0] = 0x80; // header;
        context->midiPacket[1] = 0x80; // timestamp (not implemented);
        context->midiPacket[2] = 0xC0 | m->channel;
        context->midiPacket[3] = m->program;
        context->midiPacket[4] = 0;
        context->pCharacteristic->setValue(context->midiPacket, sizeof(context->midiPacket));
        context->pCharacteristic->notify();
    } else {
        ESP_LOGE(TAG, "on_program_change not connected");
    }
}

static void process_event(void *caller_context, ysw_event_t *event) {
    context_t *context = (context_t*)caller_context;
    switch (event->header.type) {
        case YSW_EVENT_NOTE_ON:
            on_note_on(context, &event->note_on);
            break;
        case YSW_EVENT_NOTE_OFF:
            on_note_off(context, &event->note_off);
            break;
        case YSW_EVENT_PROGRAM_CHANGE:
            on_program_change(context, &event->program_change);
            break;
        default:
            break;
    }
}

void ysw_synth_bt_create_task(ysw_bus_h bus)
{
    context_t *context = (context_t*)ysw_heap_allocate(sizeof(context_t));
    initialize_synthesizer(context);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.caller_context = context;

    ysw_task_h task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_NOTE);
}

