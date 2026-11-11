#include "logicbench/Note.h"

namespace lb {

Note::Note()
{
    setDelay(0);
}

std::vector<PropertyDescriptor> Note::propertyDescriptors() const
{
    return {{"text", "Text", PropertyDescriptor::Kind::Text, 0, 0}};
}

std::map<std::string, std::string> Note::properties() const
{
    return {{"text", m_text}};
}

bool Note::setProperty(const std::string& name, const std::string& value)
{
    if (name == "text") {
        m_text = value;
        return true;
    }
    return Component::setProperty(name, value);
}

} // namespace lb
