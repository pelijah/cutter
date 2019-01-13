#include <Python.h>
#include <marshal.h>

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

#include "plugins/CutterPythonPlugin.h"
#include "PythonManager.h"
#include "PythonAPI.h"
#include "QtResImporter.h"

static PythonManager *uniqueInstance = nullptr;

PythonManager *PythonManager::getInstance()
{
    if (!uniqueInstance) {
        uniqueInstance = new PythonManager();
    }
    return uniqueInstance;
}

PythonManager::PythonManager()
{
}

PythonManager::~PythonManager()
{
    QList<CutterPlugin *> plugins = Core()->getCutterPlugins();
    for (CutterPlugin *plugin : plugins) {
        delete plugin;
    }

    restoreThread();

    if (cutterNotebookAppInstance) {
        PyObject_CallMethod(cutterNotebookAppInstance, "stop", nullptr);
        Py_DECREF(cutterNotebookAppInstance);
    }

    Py_Finalize();

    if (pythonHome) {
        PyMem_RawFree(pythonHome);
    }
}

void PythonManager::initialize()
{
#if defined(APPIMAGE) || defined(MACOS_PYTHON_FRAMEWORK_BUNDLED)
    if (customPythonHome.isNull()) {
        auto pythonHomeDir = QDir(QCoreApplication::applicationDirPath());
#   ifdef APPIMAGE
        // Executable is in appdir/bin
        pythonHomeDir.cdUp();
        qInfo() << "Setting PYTHONHOME =" << pythonHomeDir.absolutePath() << " for AppImage.";
#   else // MACOS_PYTHON_FRAMEWORK_BUNDLED
        // @executable_path/../Frameworks/Python.framework/Versions/Current
        pythonHomeDir.cd("../Frameworks/Python.framework/Versions/Current");
        qInfo() << "Setting PYTHONHOME =" << pythonHomeDir.absolutePath() <<
                " for macOS Application Bundle.";
#   endif
        customPythonHome = pythonHomeDir.absolutePath();
    }
#endif

    if (!customPythonHome.isNull()) {
        qInfo() << "PYTHONHOME =" << customPythonHome;
        pythonHome = Py_DecodeLocale(customPythonHome.toLocal8Bit().constData(), nullptr);
        Py_SetPythonHome(pythonHome);
    }

    addApiModulesToInittab();
    addQtResModuleToInittab();
    Py_Initialize();
    PyEval_InitThreads();

    RegQtResImporter();

    saveThread();

    // Import other modules
    cutterJupyterModule = createModule("cutter_jupyter");
    cutterPluginModule = createModule("cutter_plugin");
}

void PythonManager::addPythonPath(char *path) {
    restoreThread();

    PyObject *pythonPath = PySys_GetObject("path");
    if (!pythonPath) {
        saveThread();
        return;
    }
    PyObject *_path = PyUnicode_FromString(path);
    if (!_path) {
        Py_DECREF(pythonPath);
        saveThread();
        return;
    }
    PyList_Append(pythonPath, _path);

    saveThread();
}

bool PythonManager::startJupyterNotebook()
{
	if (!cutterJupyterModule) {
		return false;
	}

    restoreThread();

    PyObject* startFunc = PyObject_GetAttrString(cutterJupyterModule, "start_jupyter");
    if (!startFunc) {
        qWarning() << "Couldn't get attribute start_jupyter.";
        return false;
    }

    cutterNotebookAppInstance = PyObject_CallObject(startFunc, nullptr);
    saveThread();

    return cutterNotebookAppInstance != nullptr;
}

QString PythonManager::getJupyterUrl()
{
    restoreThread();

    auto urlWithToken = PyObject_GetAttrString(cutterNotebookAppInstance, "url_with_token");
    auto asciiBytes = PyUnicode_AsASCIIString(urlWithToken);
    auto urlWithTokenString = QString::fromUtf8(PyBytes_AsString(asciiBytes));
    Py_DECREF(asciiBytes);
    Py_DECREF(urlWithToken);

    saveThread();

    return urlWithTokenString;
}

PyObject* PythonManager::createModule(const char *module)
{
    PyObject *result = nullptr;

    restoreThread();
    result = QtResImport(module);
    saveThread();

    return result;
}

CutterPythonPlugin* PythonManager::loadPlugin(const char *pluginName) {
    CutterPythonPlugin *plugin = nullptr;
    if (!cutterPluginModule) {
        return nullptr;
    }

    restoreThread();
    PyObject *pluginModule = PyImport_ImportModule(pluginName);
    if (!pluginModule) {
        qWarning() << "Couldn't load the plugin" << QString(pluginName);
        PyErr_PrintEx(10);
    } else {
        plugin = new CutterPythonPlugin(pluginModule);
    }
    saveThread();

    return plugin;
}

void PythonManager::restoreThread()
{
    if (pyThreadState) {
        PyEval_RestoreThread(pyThreadState);
    }
}

void PythonManager::saveThread()
{
    pyThreadState = PyEval_SaveThread();
}

void PythonManager::setCutterCore(void *addr)
{
    restoreThread();

    auto arg = PyLong_FromVoidPtr(addr);
    PyObject_CallMethod(cutterPluginModule, "set_cutter_core", "N", arg);

    saveThread();
}
