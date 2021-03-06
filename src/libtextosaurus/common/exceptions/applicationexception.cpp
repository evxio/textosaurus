// This file is distributed under GNU GPLv3 license. For full license text, see <project-git-repository-root-folder>/LICENSE.md.

#include "common/exceptions/applicationexception.h"

ApplicationException::ApplicationException(const QString& message) : m_message(message) {}

QString ApplicationException::message() const {
  return m_message;
}
