#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H
#include <atomic>

struct CommandQueue {
    static const uint8_t CAPACITY = 64;

    bool push(char c)
    {
        uint8_t h = head;
        uint8_t next = (uint8_t)((h + 1) % CAPACITY);
        if (next == tail) {
            return false;
        }
        buf[h] = c;
        // Byte-Store sichtbar machen, BEVOR head veröffentlicht wird
        std::atomic_thread_fence(std::memory_order_release);
        head = next;
        return true;
    }

    bool pop(char &c)
    {
        uint8_t t = tail;
        if (t == head) {
            return false;
        }
        // head-Load abgeschlossen, BEVOR der Buffer gelesen wird
        std::atomic_thread_fence(std::memory_order_acquire);
        c = buf[t];
        tail = (uint8_t)((t + 1) % CAPACITY);
        return true;
    }

    bool empty() const { return tail == head; }

private:
    volatile uint8_t head = 0;
    volatile uint8_t tail = 0;
    char buf[CAPACITY];
};

extern CommandQueue serialCommandQueue;
extern CommandQueue userCommandQueue;

#endif