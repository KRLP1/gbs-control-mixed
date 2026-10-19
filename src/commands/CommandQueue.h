#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

struct CommandQueue {
    static const uint8_t CAPACITY = 64;

    bool push(char c)
    {
        uint8_t next = (uint8_t)((head + 1) % CAPACITY);
        if (next == tail) {
            return false;
        }
        buf[head] = c;
        head = next;
        return true;
    }

    bool pop(char &c)
    {
        if (tail == head) {
            return false;
        }
        c = buf[tail];
        tail = (uint8_t)((tail + 1) % CAPACITY);
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