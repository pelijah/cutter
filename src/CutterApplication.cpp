#include "common/PythonManager.h"
#include "CutterApplication.h"
#ifdef CUTTER_ENABLE_JUPYTER
#include "common/JupyterConnection.h"
#endif

#include <QApplication>
#include <QFileOpenEvent>
#include <QEvent>
#include <QMessageBox>
#include <QCommandLineParser>
#include <QTextCodec>
#include <QStringList>
#include <QProcess>
#include <QPluginLoader>
#include <QDir>
#include <QTranslator>
#include <QLibraryInfo>

#include "CutterConfig.h"

#include <cstdlib>

CutterApplication::CutterApplication(int &argc, char **argv) : QApplication(argc, argv)
{
    // Setup application information
    setOrganizationName("Cutter");
    setApplicationName("Cutter");
    setApplicationVersion(CUTTER_VERSION_FULL);
    setWindowIcon(QIcon(":/img/cutter.svg"));
    setAttribute(Qt::AA_DontShowIconsInMenus);
    setLayoutDirection(Qt::LeftToRight);

    // WARN!!! Put initialization code below this line. Code above this line is mandatory to be run First
    // Load translations
    QTranslator *t = new QTranslator;
    QTranslator *qtBaseTranslator = new QTranslator;
    QTranslator *qtTranslator = new QTranslator;
    QString language = Config()->getCurrLocale().bcp47Name();
    auto allLocales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript,
                                               QLocale::AnyCountry);

    QString langPrefix;
    if (language != "en") {
        for (const QLocale &it : allLocales) {
            langPrefix = it.bcp47Name();
            if (langPrefix == language) {
                const QString &cutterTranslationPath = QCoreApplication::applicationDirPath() + QDir::separator()
                    + "translations" + QDir::separator() + QString("cutter_%1.qm").arg(langPrefix);

                if (t->load(cutterTranslationPath)) {
                    installTranslator(t);
                }
                QApplication::setLayoutDirection(it.textDirection());
                QLocale::setDefault(it);

                QString translationsPath(QLibraryInfo::location(QLibraryInfo::TranslationsPath));
                if (qtTranslator->load(it, "qt", "_", translationsPath)) {
                    installTranslator(qtTranslator);
                } else {
                    delete qtTranslator;
                }

                if (qtBaseTranslator->load(it, "qtbase", "_", translationsPath)) {
                    installTranslator(qtBaseTranslator);
                } else {
                    delete qtBaseTranslator;
                }

                break;
            }
        }
    }
 
    // Load fonts
    int ret = QFontDatabase::addApplicationFont(":/fonts/Anonymous Pro.ttf");
    if (ret == -1) {
        qWarning() << "Cannot load Anonymous Pro font.";
    }

    ret = QFontDatabase::addApplicationFont(":/fonts/Inconsolata-Regular.ttf");
    if (ret == -1) {
        qWarning() << "Cannot load Incosolata-Regular font.";
    }


    // Set QString codec to UTF-8
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
#endif
    QCommandLineParser cmd_parser;
    cmd_parser.setApplicationDescription(
        QObject::tr("A Qt and C++ GUI for radare2 reverse engineering framework"));
    cmd_parser.addHelpOption();
    cmd_parser.addVersionOption();
    cmd_parser.addPositionalArgument("filename", QObject::tr("Filename to open."));

    QCommandLineOption analOption({"A", "anal"},
                                  QObject::tr("Automatically open file and optionally start analysis. Needs filename to be specified. May be a value between 0 and 2: 0 = no analysis, 1 = aaa, 2 = aaaa (experimental)"),
                                  QObject::tr("level"));
    cmd_parser.addOption(analOption);

    QCommandLineOption scriptOption("i",
                                    QObject::tr("Run script file"),
                                    QObject::tr("file"));
    cmd_parser.addOption(scriptOption);

#ifdef CUTTER_ENABLE_JUPYTER
    QCommandLineOption pythonHomeOption("pythonhome", QObject::tr("PYTHONHOME to use for Jupyter"),
                                        "PYTHONHOME");
    cmd_parser.addOption(pythonHomeOption);
#endif

    cmd_parser.process(*this);

    QStringList args = cmd_parser.positionalArguments();

    // Check r2 version
    QString r2version = r_core_version();
    QString localVersion = "" R2_GITTAP;
    if (r2version != localVersion) {
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msg.setWindowTitle(QObject::tr("Version mismatch!"));
        msg.setText(QString(
                        QObject::tr("The version used to compile Cutter (%1) does not match the binary version of radare2 (%2). This could result in unexpected behaviour. Are you sure you want to continue?")).arg(
                        localVersion, r2version));
        if (msg.exec() == QMessageBox::No) {
            std::exit(1);
        }
    }

    // Init python
    if (cmd_parser.isSet(pythonHomeOption)) {
        Python()->setPythonHome(cmd_parser.value(pythonHomeOption));
    }
    Python()->initialize();


    bool analLevelSpecified = false;
    int analLevel = 0;

    // Initialize CutterCore and set default settings
    Core()->setSettings();

    if (cmd_parser.isSet(analOption)) {
        analLevel = cmd_parser.value(analOption).toInt(&analLevelSpecified);

        if (!analLevelSpecified || analLevel < 0 || analLevel > 2) {
            printf("%s\n",
                   QObject::tr("Invalid Analysis Level. May be a value between 0 and 2.").toLocal8Bit().constData());
            std::exit(1);
        }
    }

    loadPlugins();

    mainWindow = new MainWindow();
    installEventFilter(mainWindow);

    if (args.empty()) {
        if (analLevelSpecified) {
            printf("%s\n",
                   QObject::tr("Filename must be specified to start analysis automatically.").toLocal8Bit().constData());
            std::exit(1);
        }

        mainWindow->displayNewFileDialog();
    } else { // filename specified as positional argument
        InitialOptions options;
        options.filename = args[0];
        if (analLevelSpecified) {
            switch (analLevel) {
            case 0:
            default:
                options.analCmd = {};
                break;
            case 1:
                options.analCmd = { "aaa" };
                break;
            case 2:
                options.analCmd = { "aaaa" };
                break;
            }
        }
        options.script = cmd_parser.value(scriptOption);
        mainWindow->openNewFile(options, analLevelSpecified);
    }

#ifdef CUTTER_APPVEYOR_R2DEC
    qputenv("R2DEC_HOME", "radare2\\lib\\plugins\\r2dec-js");
#endif
}

CutterApplication::~CutterApplication()
{
    delete mainWindow;
    delete Python();
}

bool CutterApplication::event(QEvent *e)
{
    if (e->type() == QEvent::FileOpen) {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(e);
        if (openEvent) {
            if (m_FileAlreadyDropped) {
                // We already dropped a file in macOS, let's spawn another instance
                // (Like the File -> Open)
                QString fileName = openEvent->file();
                QProcess process(this);
                process.setEnvironment(QProcess::systemEnvironment());
                QStringList args = QStringList(fileName);
                process.startDetached(qApp->applicationFilePath(), args);
            } else {
                QString fileName = openEvent->file();
                m_FileAlreadyDropped = true;
                mainWindow->closeNewFileDialog();
                InitialOptions options;
                options.filename = fileName;
                mainWindow->openNewFile(options);
            }
        }
    }
    return QApplication::event(e);
}

void CutterApplication::loadPlugins()
{
    QList<CutterPlugin *> plugins;
    QDir pluginsDir(qApp->applicationDirPath());
#if defined(Q_OS_WIN)
    if (pluginsDir.dirName().toLower() == "debug" || pluginsDir.dirName().toLower() == "release")
        pluginsDir.cdUp();
#elif defined(Q_OS_MAC)
    if (pluginsDir.dirName() == "MacOS") {
        pluginsDir.cdUp();
        pluginsDir.cdUp();
        pluginsDir.cdUp();
    }
#endif
    if (!pluginsDir.cd("plugins")) {
        return;
    }
    Python()->addPythonPath(pluginsDir.absolutePath().toLatin1().data());

    CutterPlugin *cutterPlugin = nullptr;
    foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {
        if (fileName.endsWith(".py")) {
            // Load python plugins
            QStringList l = fileName.split(".py");
            cutterPlugin = (CutterPlugin*) Python()->loadPlugin(l.at(0).toLatin1().constData());
        } else {
            // Load C++ plugins
            QPluginLoader pluginLoader(pluginsDir.absoluteFilePath(fileName));
            QObject *plugin = pluginLoader.instance();
            if (plugin) {
                cutterPlugin = qobject_cast<CutterPlugin *>(plugin);
            }
        }

        if (cutterPlugin) {
            cutterPlugin->setupPlugin(Core());
            plugins.append(cutterPlugin);
        }
    }

    qInfo() << "Loaded" << plugins.length() << "plugins.";
    Core()->setCutterPlugins(plugins);
}
