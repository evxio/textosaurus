// This file is distributed under GNU GPLv3 license. For full license text, see <project-git-repository-root-folder>/LICENSE.md.

#include "saurus/miscellaneous/application.h"

#include "common/exceptions/applicationexception.h"
#include "common/gui/messagebox.h"
#include "common/gui/systemtrayicon.h"
#include "common/miscellaneous/iconfactory.h"
#include "common/miscellaneous/iofactory.h"
#include "common/network-web/webfactory.h"
#include "saurus/external-tools/externaltools.h"
#include "saurus/gui/dialogs/formmain.h"
#include "saurus/gui/sidebars/outputsidebar.h"
#include "saurus/gui/statusbar.h"
#include "saurus/miscellaneous/textapplication.h"
#include "saurus/plugin-system/pluginfactory.h"

#include <QProcess>
#include <QSessionManager>

Application::Application(const QString& id, int& argc, char** argv)
  : QtSingleApplication(id, argc, argv),
  m_settings(Settings::setupSettings(this, qApp->userDataAppFolder(), qApp->userDataHomeFolder())),
  m_textApplication(new TextApplication(this)), m_mainForm(nullptr),
  m_webFactory(new WebFactory(this)),
  m_system(new SystemFactory(this)),
  m_localization(new Localization(this)), m_icons(new IconFactory(this)),
  m_trayIcon(nullptr),
  m_shouldRestart(false),
  m_isQuitting(false) {
  //: Abbreviation of language, e.g. en.
  //: Use ISO 639-1 code here combined with ISO 3166-1 (alpha-2) code.
  //: Examples: "cs", "en", "it", "cs_CZ", "en_GB", "en_US".
  QObject::tr("LANG_ABBREV");

  connect(this, &Application::aboutToQuit, this, &Application::onAboutToQuit);
  connect(this, &Application::commitDataRequest, this, &Application::onCommitData);
  connect(this, &Application::saveStateRequest, this, &Application::onSaveState);

  setQuitOnLastWindowClosed(true);
  setStyleSheet(QSL("QStatusBar::item { border: none; } "
                    "QSplitter::handle:horizontal, QSplitter::handle:vertical { width: 1px; }"));

  qDebug("Instantiated Application class.");
}

void Application::activateQtSingleMsgProcessing() {
  connect(this, &Application::messageReceived, this, &Application::processExecutionMessage);
}

bool Application::isRunning() {
  return sendMessage((QStringList() << APP_IS_RUNNING << arguments().mid(1)).join(ARGUMENTS_LIST_SEPARATOR));
}

QList<QAction*> Application::userActions() {
  QList<QAction*> user_actions;

  if (m_mainForm != nullptr) {
    user_actions = m_mainForm->allActions();
  }

  if (m_textApplication != nullptr) {
    user_actions.append(m_textApplication->userActions());
  }

  return user_actions;
}

bool Application::isFirstRun() const {
  return settings()->value(GROUP(General), SETTING(General::FirstRun)).toBool();
}

bool Application::isFirstRun(const QString& version) const {
  if (version == APP_VERSION) {
    // Check this only if checked version is equal to actual version.
    return settings()->value(GROUP(General), QString(General::FirstRun) + QL1C('_') + version, true).toBool();
  }
  else {
    return false;
  }
}

TextApplication* Application::textApplication() const {
  return m_textApplication;
}

WebFactory* Application::web() const {
  return m_webFactory;
}

SystemFactory* Application::system() {
  return m_system;
}

Localization* Application::localization() {
  return m_localization;
}

void Application::eliminateFirstRun() {
  settings()->setValue(GROUP(General), General::FirstRun, false);
}

void Application::eliminateFirstRun(const QString& version) {
  settings()->setValue(GROUP(General), QString(General::FirstRun) + QL1C('_') + version, false);
}

bool Application::isQuitting() const
{
  return m_isQuitting;
}

IconFactory* Application::icons() {
  return m_icons;
}

Settings* Application::settings() const {
  return m_settings;
}

FormMain* Application::mainForm() {
  return m_mainForm;
}

QWidget* Application::mainFormWidget() {
  return m_mainForm;
}

void Application::showTrayIcon() {
  qDebug("Showing tray icon.");
  trayIcon()->show();
}

void Application::deleteTrayIcon() {
  if (m_trayIcon != nullptr) {
    qDebug("Disabling tray icon, deleting it and raising main application window.");
    m_mainForm->display();
    delete m_trayIcon;
    m_trayIcon = nullptr;

    // Make sure that application quits when last window is closed.
    setQuitOnLastWindowClosed(true);
  }
}

SystemTrayIcon* Application::trayIcon() {
  if (m_trayIcon == nullptr) {
#if defined(Q_OS_WIN)
    QMenu* tray_menu = new TrayIconMenu(APP_NAME, m_mainForm);
#else
    QMenu* tray_menu = new QMenu(APP_NAME, m_mainForm);
#endif

    // Ensure that main window is shown before the command
    // from tray icon menu is ran.
    connect(tray_menu, &QMenu::triggered, this, [this]() {
      m_mainForm->display();
    });

    auto* separator = new QAction(tray_menu);

    separator->setSeparator(true);

    tray_menu->addActions(QList<QAction*> {
      m_mainForm->m_ui.m_actionFileNew,
      m_mainForm->m_ui.m_actionFileOpen,
      separator,
      m_mainForm->m_ui.m_actionQuit
    });

    m_trayIcon = new SystemTrayIcon(
      APP_ICON_PATH,
      tray_menu,
      [this]() {
      m_mainForm->switchVisibility();
    },
      m_mainForm);
    m_trayIcon->setToolTip(APP_LONG_NAME);
  }

  return m_trayIcon;
}

void Application::setMainForm(FormMain* main_form) {
  m_mainForm = main_form;
}

QString Application::configFolder() {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString Application::userDataAppFolder() {
  // In "app" folder, we would like to separate all user data into own subfolder,
  // therefore stick to "data" folder in this mode.
  return applicationDirPath() + QDir::separator() + QSL("data");
}

QString Application::userDataFolder() {
  if (settings()->type() == SettingsType::Portable) {
    return userDataAppFolder();
  }
  else {
    return userDataHomeFolder();
  }
}

QString Application::userDataHomeFolder() {
#if defined (Q_OS_ANDROID)
  return IOFactory::getSystemFolder(QStandardPaths::AppDataLocation);
#else
  return configFolder();
#endif
}

QString Application::homeFolder() {
  return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

void Application::processExecutionMessage(const QString& message) {
  qDebug("Received '%s' execution message from another application instance.", qPrintable(message));
  const QStringList messages = message.split(ARGUMENTS_LIST_SEPARATOR);

  if (messages.contains(APP_QUIT_INSTANCE)) {
    quitApplication();
  }
  else {
    if (messages.contains(APP_IS_RUNNING)) {
      qApp->textApplication()->loadFilesFromArgs(messages);
      qApp->mainForm()->display();
    }
  }
}

void Application::showGuiMessage(const QString& message, QMessageBox::Icon message_type, const QUrl& url, std::function<void()> handler) {
  Q_UNUSED(message_type)

  if (SystemTrayIcon::isSystemTrayActivated()) {
    trayIcon()->showMessage(APP_NAME, message, (QSystemTrayIcon::MessageIcon) message_type, TRAY_ICON_BUBBLE_TIMEOUT, handler);
  }
  else {
    m_textApplication->outputSidebar()->displayOutput(OutputSource::Application, message, message_type, url, handler);
  }
}

Application* Application::instance() {
  return static_cast<Application*>(QCoreApplication::instance());
}

void Application::onCommitData(QSessionManager& manager) {
  qDebug("OS asked application to commit its data.");
  manager.setRestartHint(QSessionManager::RestartNever);

  // Now we need to ask user to save any unsaved documents.
  if (manager.allowsInteraction()) {
    bool ok;
    emit dataSaveRequested(&ok);

    manager.release();

    if (!ok) {
      // Not all documents are saved.
      manager.cancel();
    }
  }
}

void Application::onSaveState(QSessionManager& manager) {
  qDebug("OS asked application to save its state.");
  manager.setRestartHint(QSessionManager::RestartNever);
}

void Application::quitApplication() {
  if (m_mainForm != nullptr) {
    m_isQuitting = true;
    bool closed = m_mainForm->close();

    if (closed) {
      quit();
    }
    else {
      m_isQuitting = false;
      m_shouldRestart = false;
    }
  }
}

void Application::onAboutToQuit() {
  eliminateFirstRun();
  eliminateFirstRun(APP_VERSION);
  processEvents();

  qDebug("Cleaning up resources and saving application state.");

#if defined(Q_OS_WIN)
  system()->removeTrolltechJunkRegistryKeys();
#endif

  if (mainForm() != nullptr) {
    mainForm()->saveSize();
  }

  // Now, we can check if application should just quit or restart itself.
  if (m_shouldRestart) {
    finish();
    qDebug("Killing local peer connection to allow another instance to start.");

    if (QProcess::startDetached(QString("\"") + QDir::toNativeSeparators(applicationFilePath()) + QString("\""))) {
      qDebug("New application instance was started.");
    }
    else {
      qWarning("New application instance was not started successfully.");
    }
  }
}

void Application::restart() {
  m_shouldRestart = true;
  quitApplication();
}
