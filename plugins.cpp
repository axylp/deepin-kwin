/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000    Daniel M. Duley <mosfet@kde.org>
******************************************************************/
#include <kglobal.h>
#include <kconfig.h>
#include <kstddirs.h>
#include <kdesktopfile.h>
#include <ksimpleconfig.h>
#include <klocale.h>
#include <klibloader.h>

// X11/Qt conflict
#undef Unsorted

#include <qdir.h>
#include <qfile.h>

#include "plugins.h"
#include "kdedefault.h"

PluginMenu::PluginMenu(PluginMgr *manager, QWidget *parent, const char *name)
    : QPopupMenu(parent, name)
{
    connect(this, SIGNAL(aboutToShow()), SLOT(slotAboutToShow()));
    connect(this, SIGNAL(activated(int)), SLOT(slotActivated(int)));
    mgr = manager;
}

void PluginMenu::slotAboutToShow()
{
    clear();
    fileList.clear();
    insertItem(i18n("KDE 2"), 0);
    idCount = 1;
    idCurrent = 0;

    QDir dir;
    dir.setFilter(QDir::Files);
    const QFileInfoList *list;
    int count = KGlobal::dirs()->findDirs("data", "kwin").count();
    if(count){
        dir.setPath(KGlobal::dirs()->findDirs("data", "kwin")
                    [count > 1 ? 1 : 0]);
        if(dir.exists()){
            list =  dir.entryInfoList();
            if(list){
                QFileInfoListIterator it(*list);
                QFileInfo *fi;
                for(; (fi = it.current()) != NULL; ++it){
                    if(KDesktopFile::isDesktopFile(fi->absFilePath()))
                        parseDesktop(fi);
                }
            }
        }
        if(count > 1){
            dir.setPath(KGlobal::dirs()->findDirs("data", "kwin")[0]);
            if(dir.exists()){
                list = dir.entryInfoList();
                if(list){
                    QFileInfoListIterator it(*list);
                    QFileInfo *fi;
                    for(; (fi = it.current()) != NULL; ++it){
                        if(KDesktopFile::isDesktopFile(fi->absFilePath()))
                            parseDesktop(fi);
                    }
                }
            }
        }
    }
    setItemChecked(idCurrent, true);
}

void PluginMenu::parseDesktop(QFileInfo *fi)
{
    QString tmpStr;
    KSimpleConfig config(fi->absFilePath(), true);
    config.setDesktopGroup();
    tmpStr = config.readEntry("X-KDE-Library", "");
    if(tmpStr.isEmpty()){
        qWarning("KWin: Invalid plugin: %s", fi->absFilePath().latin1());
        return;
    }
    fileList.append(tmpStr);
    if (tmpStr == mgr->currentPlugin())
       idCurrent = idCount;
    tmpStr = config.readEntry("Name", "");
    if(tmpStr.isEmpty())
        tmpStr = fi->baseName();
    insertItem(tmpStr, idCount);
    ++idCount;
}

void PluginMenu::slotActivated(int id)
{
    QString newPlugin;
    if (id > 0)
        newPlugin = fileList[id-1];
     
    KConfig *config = KGlobal::config();
    config->setGroup("Style");
    config->writeEntry("PluginLib", newPlugin);
    config->sync();
    mgr->loadPlugin(newPlugin);
}

PluginMgr::PluginMgr()
    : QObject()
{
    alloc_ptr = NULL;
    handle = 0;
    pluginStr = "standard";

    updatePlugin();
}

PluginMgr::~PluginMgr()
{
    if(handle)
        lt_dlclose(handle);
}

void
PluginMgr::updatePlugin()
{
    KConfig *config = KGlobal::config();
    config->setGroup("Style");
    QString newPlugin = config->readEntry("PluginLib", "default");
    if (newPlugin != pluginStr)
       loadPlugin(newPlugin);
}

Client* PluginMgr::allocateClient(Workspace *ws, WId w, bool tool)
{
    if(alloc_ptr)
        return(alloc_ptr(ws, w, tool));
    else
        return(new KDEClient(ws, w));
}

void PluginMgr::loadPlugin(QString nameStr)
{
    static bool dlregistered = false;
    static lt_dlhandle oldHandle = 0;

    pluginStr = nameStr;

    oldHandle = handle;

    // Rikkus: temporary change in semantics.

    if (!nameStr)
      nameStr = "default";

    if(!dlregistered){
        dlregistered = true;
        lt_dlinit();
    }
    nameStr = KLibLoader::findLibrary(nameStr.latin1());

    if(nameStr.isNull()){
        qWarning("KWin: cannot find client plugin.");
        handle = 0;
        alloc_ptr = NULL;
        pluginStr = "standard";
    }
    else{
        handle = lt_dlopen(nameStr.latin1());
        if(!handle){
            qWarning("KWin: cannot load client plugin %s.", nameStr.latin1());
            handle = 0;
            alloc_ptr = NULL;
            pluginStr = "standard";
        }
        else{
            lt_ptr_t alloc_func = lt_dlsym(handle, "allocate");
            if(alloc_func)
                alloc_ptr = (Client* (*)(Workspace *ws, WId w, int tool))alloc_func;
            else{
                qWarning("KWin: %s is not a KWin plugin.", nameStr.latin1());
                lt_dlclose(handle);
                handle = 0;
                alloc_ptr = NULL;
                pluginStr = "standard";
            }
        }
    }
    emit resetAllClients();
    if(oldHandle)
        lt_dlclose(oldHandle);
}

#include "plugins.moc"

