#ifndef VALUE_COMMAND_QUEUE_H
#define VALUE_COMMAND_QUEUE_H
#include <stdint.h>
#include <atomic>

// Value command queue: like CommandQueue, but each entry carries a command
// character plus an 8-bit value ('Y' = set sharpness 0..32, 'y' = scanline
// strength 0x00..0x60 switching the scanlines on, or 0x7f = switch them off).
// Producers: the /gbs/set/* HTTP handlers and the websocket "Y=" / "y=" frames;
// consumer: loop() - same release/acquire fencing as CommandQueue.
// The queue is single-producer by design. Both producers run cooperatively in one
// thread on the ESP8266 (a push contains no yield). On the ESP32 the HTTP handler
// (AsyncTCP task) and the websocket handler (loop task) can in theory push at the
// same moment; the worst case is one lost value command (never a corrupt entry or
// index), and the WebUI only falls back to HTTP while its websocket is down.
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
