#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 外部输入源以 Role::Input 区别于组合门，继承 SourceDefinition 的零输入、一个输出。
// 输出位宽继承至少一位的 bitWidth，位置为 (max(60,bitWidth*22+8)/2,0)，随显示宽度移动。
// 不覆写 compute：继承空结果，三个计算参数均不用，本定义不做四态变换或位序重排。
// 默认继承 zeroPorts，初始化一个全零输出；实际源值由外部角色处理逻辑提供。
// typeId 返回的 input 是持久化类型标识及注册表键，不是实例名称。
// 文件末尾工厂由生成表显式引用，保证静态库链接保留类型而不依赖全局自注册。
class InputDefinition final : public SourceDefinition {
public:
    QString typeId() const override { return "input"; }
    Role role() const override { return Role::Input; }
};
}
std::unique_ptr<Definition> createInputDefinition() { return std::make_unique<InputDefinition>(); }
} // namespace Components
