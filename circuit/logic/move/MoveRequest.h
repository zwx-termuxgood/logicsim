#ifndef MOVEREQUEST_H
#define MOVEREQUEST_H

#include <QtGlobal>

class MoveGesture;

class MoveRequest {
public:
    MoveRequest(MoveGesture* gesture, int dx, int dy)
    : m_gesture(gesture), m_dx(dx), m_dy(dy) {}

    MoveGesture* gesture() const { return m_gesture; }
    int dx() const { return m_dx; }
    int dy() const { return m_dy; }

    bool equals(const MoveRequest& o) const {
        return m_gesture == o.m_gesture && m_dx == o.m_dx && m_dy == o.m_dy;
    }

    int hash() const {
        return (int)(reinterpret_cast<quintptr>(m_gesture) * 31) * 31 + m_dx * 31 + m_dy;
    }

private:
    MoveGesture* m_gesture;
    int m_dx, m_dy;
};

#endif
