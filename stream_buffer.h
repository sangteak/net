#pragma once

#include <iostream>
#include <array>

template <std::size_t DefaultCapacity = 1024>
class StreamBuffer 
{
    static constexpr int INITIAL_VALUE = -1;
    
public:
    StreamBuffer()
        : m_capacity(DefaultCapacity)
        , m_front(-1)
        , m_rear(-1)
        , m_pointer(new uint8_t[DefaultCapacity])
    {
    }

    ~StreamBuffer()
    {
        if (m_pointer)
        {
            delete [] m_pointer;
            m_pointer = nullptr;
        }
    }
    
    std::size_t GetLength() 
    {
        if (INITIAL_VALUE == m_front)
        {
            return 0;
        }

        if (m_front < m_rear)
        {
            return m_rear - m_front;
        }

        return ((m_capacity - m_front) + m_rear);
    }

    // Check if the queue is full
    bool IsFull(int32_t t_length) 
    {
        if (m_front == 0 && m_rear == m_capacity - 1) 
        {
            return true;
        }

        if (m_front == m_rear + 1) 
        {
            return true;
        }

        // 한바퀴 돌지 않았을 경우
        int availableSpace = 0;
        if (m_front < m_rear)
        {
            availableSpace = m_capacity - (m_rear - m_front);
        }
        else
        {
            availableSpace = m_capacity - m_front + m_rear;
        }

        if (t_length > availableSpace)
        {
            return true;
        }

        return false;
    }

    // Check if the queue is empty
    bool IsEmpty() 
    {
        if (INITIAL_VALUE == m_front)
        {
            return true;
        }

        return false;
    }
    
    // Adding an element
    void Write(const uint8_t* t_data, std::size_t t_length)
    {
        // 공간이 부족하다면 여기서 충분히 할당하도록 한다.
        if (IsFull(t_length))
        {
            std::cout << "Queue is full" << std::endl;
            Grow();
        }

        // 처음이면...
        if (INITIAL_VALUE == m_front)
        {
            m_front = 0;
            m_rear = 0;
        }

        std::array<int, 2> split;

        // front에서 capa까지 얼마나 남아있찌??
        if (m_rear + t_length <= m_capacity)
        {
            split[0] = t_length;
            split[1] = 0;
            // 공간이 있으니까 그대로 넣는다.
        }
        // 한바퀴 돌았다면....
        else 
        {
            split[0] = m_capacity - m_rear;
            split[1] = t_length - split[0];
        }

        int offset = 0;
        for (auto& n : split)
        {
            if (n <= 0)
                break;

            memcpy(m_pointer + m_rear, t_data + offset, n);
            
            offset += n;
            m_rear = (m_rear + n) % m_capacity;
        }

        //std::cout << "Inserted " << element << std::endl;
    }

    

    bool Read(uint8_t* t_data, std::size_t t_length)
    {
        if (IsEmpty())
        {
            return false;
        }

        // 한바퀴 돌지 않았을 경우
        std::array<int, 2> split;
        if (m_front < m_rear)
        {
            split[0] = m_rear - m_front;
            split[1] = 0;
        }
        else
        {
            split[0] = m_capacity - m_front;
            split[1] = m_rear;
        }

        // 초과되었으므로 복사할수 없다.(잘못된 요청)
        if (split[0] + split[1] < t_length)
        {
            return false;
        }

        auto front = m_front;

        // 여기도 하나씩 꺼내오는 방식이지만 binary에서는 이렇게 사용 할수가 없다.
        // element = items[m_front];
        if (split[0] >= t_length)
        {
            memcpy(t_data, m_pointer + front, t_length);
        }
        else
        {
            // 실제 복사되어야할 크기로 조정
            split[1] = t_length - split[0];
            
            int offset = 0;
            for (auto& n : split)
            {
                if (n <= 0)
                    break;

                memcpy(t_data + offset, m_pointer + front, n);
                offset += n;
                front = (front + n) % m_capacity;
            }
        }

        return true;
    }

    void Consume(std::size_t t_length)
    {
        if (IsEmpty())
        {
            return;
        }

        // 한바퀴 돌지 않았을 경우
        std::array<int, 2> split;
        if (m_front < m_rear)
        {
            split[0] = m_rear - m_front;
            split[1] = 0;
        }
        else
        {
            split[0] = m_capacity - m_front;
            split[1] = m_rear;
        }

        // 초과되었으므로 복사할수 없다.(잘못된 요청)
        if (split[0] + split[1] < t_length)
        {
            return;
        }

        // 여기도 하나씩 꺼내오는 방식이지만 binary에서는 이렇게 사용 할수가 없다.
        if (split[0] >= t_length)
        {
            m_front = (m_front + t_length) % m_capacity;
        }
        else
        {
            // 실제 복사되어야할 크기로 조정
            split[1] = t_length - split[0];

            for (auto& n : split)
            {
                if (n <= 0)
                    break;

                m_front = (m_front + n) % m_capacity;
            }
        }

        if (m_front == m_rear) {
            m_front = INITIAL_VALUE;
            m_rear = INITIAL_VALUE;
        }
    }

    bool ReadAndConsume(uint8_t* t_data, std::size_t t_length)
    {
        if (IsEmpty())
        {
            return false;
        }

        // 한바퀴 돌지 않았을 경우
        int dataLength = 0;
        std::array<int, 2> split;
        if (m_front < m_rear)
        {
            split[0] = m_rear - m_front;
            split[1] = 0;
        }
        else
        {
            split[0] = m_capacity - m_front;
            split[1] = m_rear;
        }

        // 초과되었으므로 복사할수 없다.(잘못된 요청)
        if (split[0] + split[1] < t_length)
        {
            return false;
        }

        // 여기도 하나씩 꺼내오는 방식이지만 binary에서는 이렇게 사용 할수가 없다.
        //element = items[m_front];
        if (split[0] >= t_length)
        {
            memcpy(t_data, m_pointer + m_front, t_length);
            m_front = (m_front + t_length) % m_capacity;
        }
        else
        {
            // 실제 복사되어야할 크기로 조정
            split[1] = t_length - split[0];

            int offset = 0;
            for (auto& n : split)
            {
                if (n <= 0)
                    break;

                memcpy(t_data + offset, m_pointer + m_front, n);
                offset += n;
                m_front = (m_front + n) % m_capacity;
            }
        }

        if (m_front == m_rear) {
            m_front = INITIAL_VALUE;
            m_rear = INITIAL_VALUE;
        }

        return true;
    }

private:
    void Grow()
    {
        int capacity = m_capacity << 1; // 2배!! 엄청 증가하지 않겠지.... 모니터링 필요함.

        uint8_t* buffer = new uint8_t[capacity];
        
        // 가장 앞으로정렬 시킨다.
        std::array<int, 2> split;
        if (m_front < m_rear)
        {
            split[0] = m_rear - m_front;
            split[1] = 0;
        }
        else
        {
            split[0] = m_capacity - m_front;
            split[1] = m_rear;
        }

        int offset = 0;
        for (auto& n : split)
        {
            if (n <= 0)
                break;

            memcpy(buffer + offset, m_pointer + m_front, n);
            offset += n;
            m_front = (m_front + n) % capacity;
        }

        m_front = 0;
        m_rear = offset % capacity;
        m_capacity = capacity;
        
        if (nullptr != m_pointer)
        {
            delete m_pointer;
            m_pointer = nullptr;
        }

        m_pointer = buffer;
    }

    int32_t m_capacity;
    uint8_t* m_pointer;
    int32_t m_front;
    int32_t m_rear;
};

/*
int main() {
    Queue q;

    // Fails because front = -1
    q.deQueue();

    q.enQueue(1);
    q.enQueue(2);
    q.enQueue(3);
    q.enQueue(4);
    q.enQueue(5);

    // Fails to enqueue because front == 0 && rear == SIZE - 1
    q.enQueue(6);

    q.display();

    int elem = q.deQueue();

    if (elem != -1)
        cout << endl
        << "Deleted Element is " << elem;

    q.display();

    q.enQueue(7);

    q.display();

    // Fails to enqueue because front == rear + 1
    q.enQueue(8);

    return 0;
}
*/