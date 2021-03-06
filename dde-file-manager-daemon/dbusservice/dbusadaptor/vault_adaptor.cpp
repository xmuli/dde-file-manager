/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -i ../vault/vaultmanager.h -c VaultAdaptor -l VaultManager -a dbusadaptor/vault_adaptor vault.xml
 *
 * qdbusxml2cpp is Copyright (C) 2017 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "dbusadaptor/vault_adaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class VaultAdaptor
 */

VaultAdaptor::VaultAdaptor(VaultManager *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

VaultAdaptor::~VaultAdaptor()
{
    // destructor
}

qulonglong VaultAdaptor::getLastestTime()
{
    // handle method call com.deepin.filemanager.daemon.VaultManager.getLastestTime
    return parent()->getLastestTime();
}

qulonglong VaultAdaptor::getSelfTime()
{
    // handle method call com.deepin.filemanager.daemon.VaultManager.getSelfTime
    return parent()->getSelfTime();
}

void VaultAdaptor::setRefreshTime(qulonglong time)
{
    // handle method call com.deepin.filemanager.daemon.VaultManager.setRefreshTime
    parent()->setRefreshTime(time);
}

bool VaultAdaptor::checkAuthentication(QString type)
{
    return parent()->checkAuthentication(type);
}

