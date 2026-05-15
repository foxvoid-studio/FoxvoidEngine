#include "ChangeTagCommand.hpp"

ChangeTagCommand::ChangeTagCommand(GameObject* object, const std::string& oldTag, const std::string& newTag)
    : m_object(object), m_oldTag(oldTag), m_newTag(newTag)
{

}

void ChangeTagCommand::Execute() {
    if (m_object) {
        m_object->tag = m_newTag;
    }
}

void ChangeTagCommand::Undo() {
    if (m_object) {
        m_object->tag = m_oldTag;
    }
}
