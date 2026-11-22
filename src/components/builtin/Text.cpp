#include "components/ComponentDefinition.h"

namespace Components {
namespace {
// 文本注释使用 Role::Text，继承 Definition 的零输入、零输出和空端口布局，不参与布线。
// 未覆写 compute，三个计算参数均不用且返回空结果；没有四态运算或位序含义。
// typeId 的 text 是持久化类型标识与注册表键，content 则保存实例显示内容。
// 文件末尾工厂供生成表显式引用，保证静态库链接保留这一非逻辑元件。
class TextDefinition final : public Definition {
public:
    QString typeId() const override { return "text"; }
    Role role() const override { return Role::Text; }
    // 新建时内容为“文本”，zeroPorts 按零端口生成两个空值表；子电路标识不使用。
    void defaults(QVariantMap& c, const QString&) const override {
        c["content"] = "文本";
        zeroPorts(*this, c);
    }
};
}
std::unique_ptr<Definition> createTextDefinition() { return std::make_unique<TextDefinition>(); }
} // namespace Components
