#ifndef VALUE_COMMAND_QUEUE_H
#define VALUE_COMMAND_QUEUE_H
#include <stdint.h>
#include <atomic>

// Value command queue: like CommandQueue, but each entry carries a command
// character plus an 8-bit value ('Y' = set sharpness 0..32, 'y' = scanline
// strength 0x00..0x60 switching the scanlines on, or 0x7f = switch them off).
// Producers: the /gbs/set/* HTTP handlers and the websocket "Y=" / "y=" frames;
// consumer: loop() - same release/acquire fencing as CommandQueue.
// Both producers run cooperatively in one thread on the ESP8266. On the ESP32 the
// websocket handler runs in the loop() task and the HTTP handlers in the async_tcp
// task, so two pushes at the very same moment can lose or mix an entry (a mixed
// entry is cmd from one push and value from the other): there is no lock (known
// limitation, untested without ESP32 hardware). The WebUI only falls back to HTTP
// while its websocket is down.
struct ValueCommandQueue {
    static const uint8_t CAPACITY = 16;

    struct Entry {
        char cmd;
        uint8_t value;
    };

    bool push(char cmd, uint8_t value)
    {
        uint8_t h = head;
        uint8_t next = (uint8_t)((h + 1) % CAPACITY);
        if (next == tail) {
            return false;
        }
        buf[h].cmd = cmd;
        buf[h].value = value;
        // make the byte stores visible BEFORE head is published
        std::atomic_thread_fence(std::memory_order_release);
        head = next;
        return true;
    }

    bool pop(Entry &e)
    {
        uint8_t t = tail;
        if (t == head) {
            return false;
        }
        // finish the head load BEFORE the buffer is read
        std::atomic_thread_fence(std::memory_order_acquire);
        e = buf[t];
        tail = (uint8_t)((t + 1) % CAPACITY);
        return true;
    }

    bool empty() const { return tail == head; }

private:
    volatile uint8_t head = 0;
    volatile uint8_t tail = 0;
    Entry buf[CAPACITY];
};

extern ValueCommandQueue userValueQueue;

#endif
