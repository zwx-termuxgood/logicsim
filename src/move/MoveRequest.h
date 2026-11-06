#ifndef MOVEREQUEST_H
// 头文件保护开始：防止同一编译单元重复包含本头文件
#define MOVEREQUEST_H
// 定义守卫宏，与 #ifndef 配对，标记本文件已被包含

#include <QtGlobal>
// 引入 Qt 全局定义：quintptr（用于把指针转换为整型的无符号指针宽度类型）在此提供

class MoveGesture;
// 前置声明：MoveRequest 只持有指针，方法返回指针，无需完整定义；
// 这样 MoveGesture.h 可以反向包含 MoveRequest.h 而不产生循环依赖

class MoveRequest {
    // 一次"移动请求"的值对象：
    // 记录哪个手势（哪批选中元件）要移动多少位移；
    // 是 ConnectorThread 队列的元素类型、结果缓存的 key 依据，
    // 也是 Circuit 向 Connector 提交计算的输入
public:
    // 构造 + 访问器 + 相等/哈希

    MoveRequest(MoveGesture* gesture, int dx, int dy)
        : m_gesture(gesture), m_dx(dx), m_dy(dy) {}
    // 构造：直接保存三个字段，无额外校验；
    // gesture 为裸指针，生命周期由调用方（Circuit/ConnectorThread 使用期间）保证

    MoveGesture* gesture() const { return m_gesture; }
    // 返回手势指针：Connector 通过它取连接描述与避让地图；
    // ConnectorThread 计算完成后通过它回调 notifyResult
    int dx() const { return m_dx; }
    // 水平位移（像素）：A* 终点 = 起点 + (dx, dy)
    int dy() const { return m_dy; }
    // 垂直位移

    bool equals(const MoveRequest& o) const {
        // 判等：手势指针相同且位移相同即视为同一请求；
        // 指针比较而非内容比较，因为一个手势对象代表一次拖动会话，
        // 同一会话内相同位移的结果可以复用
        return m_gesture == o.m_gesture && m_dx == o.m_dx && m_dy == o.m_dy;
    }

    int hash() const {
        // 计算哈希：把手势指针转成整数后与 dx、dy 按 31 组合；
        // reinterpret_cast<quintptr> 保证在 32/64 位上都能安全转整数；
        // 组合方式与 QHash 常见做法一致，使不同 (gesture, dx, dy) 尽量分散；
        // 该哈希是 MoveGesture::m_cachedResults 的 key，同一请求重复提交可命中缓存
        return (int)(reinterpret_cast<quintptr>(m_gesture) * 31) * 31 + m_dx * 31 + m_dy;
    }

private:
    // 私有数据：构造后不可变，保证作为缓存 key 时稳定

    MoveGesture* m_gesture;
    // 所属手势：一个 MoveGesture 对应一次拖动会话
    int m_dx, m_dy;
    // 请求位移：相对拖动起点的偏移
};

#endif
// 结束头文件保护