/*
 * Copyright (C) 2019 Deepin Technology Co., Ltd.
 *
 * Author:     Gary Wang <wzc782970009@gmail.com>
 *
 * Maintainer: Gary Wang <wangzichong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "dfmsidebarview.h"
#include "dfmsidebaritem.h"
#include "dfileservices.h"
#include "app/define.h"

#include <QDebug>
#include <dstorageinfo.h>
#include <qmimedata.h>
#include <QtConcurrent>
#include <models/dfmsidebarmodel.h>

#include <unistd.h>

//#define DRAG_EVENT_URLS "UrlsInDragEvent"
#define DRAG_EVENT_URLS ((getuid()==0) ? (QString(getlogin())+"_RootUrlsInDragEvent") :(QString(getlogin())+"_UrlsInDragEvent"))

DFM_BEGIN_NAMESPACE

DFMSideBarView::DFMSideBarView(QWidget *parent)
    : DListView(parent)
{
    setVerticalScrollMode(ScrollPerPixel);
    setIconSize(QSize(16, 16));
//    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

//    setMouseTracking(true);

    setDragDropMode(QAbstractItemView::InternalMove);
    setDragDropOverwriteMode(false);
    //QListView拖拽时会先插入后删除，于是可以通过rowCountChanged()信号来判断拖拽操作是否结束
    connect(this, &DFMSideBarView::rowCountChanged, this, &DFMSideBarView::onRowCountChanged);  //Qt5风格
    connect(this, static_cast<void (DListView::*)(const QModelIndex &)>(&DListView::currentChanged), this, &DFMSideBarView::currentChanged);

    m_lastOpTime = 0;
}

void DFMSideBarView::mousePressEvent(QMouseEvent *event)
{
    //频繁点击操作与网络或挂载设备的加载效率低两个因素的共同作用下 会导致侧边栏可能出现显示错误
    //暂时抛去部分频繁点击来规避这个问题
    if (!checkOpTime())
        return;

    if (event->button() == Qt::RightButton) {
#if 1   //fix bug#33502 鼠标挪动到侧边栏底部右键，滚动条滑动，不能定位到选中的栏目上
        event->accept();
        return;
#else
        if (m_current != indexAt(event->pos())) {
            DListView::mousePressEvent(event);
            return setCurrentIndex(m_previous);
        }
#endif
    }
    DListView::mousePressEvent(event);
}

void DFMSideBarView::mouseMoveEvent(QMouseEvent *event)
{
    DListView::mouseMoveEvent(event);
#if QT_CONFIG(draganddrop)
    if (state() == DraggingState) {
        startDrag(Qt::MoveAction);
        setState(NoState); // the startDrag will return when the dnd operation is done
        stopAutoScroll();
        QPoint pt = mapFromGlobal(QCursor::pos());
        QRect rc = geometry();
        if (!rc.contains(pt)) {
            emit requestRemoveItem(); //model()->removeRow(currentIndex().row());
        }
    }
#endif // QT_CONFIG(draganddrop)
}

void DFMSideBarView::dragEnterEvent(QDragEnterEvent *event)
{
    previousRowCount = model()->rowCount();
    fetchDragEventUrlsFromSharedMemory();

    if (isAccepteDragEvent(event)) {
        return;
    }

    DListView::dragEnterEvent(event);

    if (event->source() != this) {
        event->setDropAction(Qt::IgnoreAction);
        event->accept();
    }
}

void DFMSideBarView::dragMoveEvent(QDragMoveEvent *event)
{
    if (isAccepteDragEvent(event)) {
        return;
    }

    DListView::dragMoveEvent(event);

    if (event->source() != this) {
        event->ignore();
    }
}

void DFMSideBarView::dropEvent(QDropEvent *event)
{
    dropPos = event->pos();
    DFMSideBarItem *item = itemAt(event->pos());
    if (!item) {
        return DListView::dropEvent(event);
    }

    qDebug() << "source: " << event->mimeData()->urls();
    qDebug() << "target item: " << item->groupName() << "|" << item->text() <<  "|" << item->url();

    QPoint pt = mapFromGlobal(QCursor::pos());
    QRect rc = visualRect(indexAt(event->pos()));
    if (!rc.contains(pt)) {
        qDebug() << "mouse not in my area";
        return DListView::dropEvent(event);
    }

    // bug case 24499, 这里需要区分哪些是可读的文件 或文件夹，因为其权限是不一样的，所以需要对不同权限的文件进行区分处理
    // 主要有4种场景：1.都是可读写的场景; 2.文件夹是只读属性，子集是可读写的; 3.文件夹或文件是可读写的; 4.拖动的包含 可读写的和只读的
    DUrlList urls, copyUrls;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (DUrl(url).parentUrl() == item->url()) {
            qDebug() << "skip the same dir file..." << url;
        } else {
            QFileInfo folderinfo(DUrl(url).parentUrl().path()); // 判断上层文件是否是只读，有可能上层是只读，而里面子文件或文件夾又是可以写
            QFileInfo fileinfo(url.path());
            if (!fileinfo.isWritable() || !folderinfo.isWritable()) {
                copyUrls << url;
                qDebug() << "this is a unwriteable case:" << url;
            } else {
                urls << url;
            }
        }
    }

    bool isActionDone = false;
    if (!urls.isEmpty()) {
        Qt::DropAction action = canDropMimeData(item, event->mimeData(), Qt::MoveAction);
        if (action == Qt::IgnoreAction) {
            action = canDropMimeData(item, event->mimeData(), event->possibleActions());
        }

        if (urls.size() > 0 && onDropData(urls, item->url(), action)) {
            event->setDropAction(action);
            isActionDone = true;
        }
    }
    if (!copyUrls.isEmpty()) {
        if (onDropData(copyUrls, item->url(), Qt::CopyAction)) {  // 对于只读权限的，只能进行 copy动作
            event->setDropAction(Qt::CopyAction);
            isActionDone = true;
        }
    }

    if (isActionDone) {
        event->accept();
    } else {
        DListView::dropEvent(event);
    }
}

QModelIndex DFMSideBarView::indexAt(const QPoint &p) const
{
    QModelIndex index = QListView::indexAt(p);
    if (index.data(DFMSideBarItem::ItemTypeRole) == DFMSideBarItem::Separator) {
        return QModelIndex();
    }

    return index;
}

QModelIndex DFMSideBarView::getPreviousIndex() const
{
    return  m_previous;
}

QModelIndex DFMSideBarView::getCurrentIndex() const
{
    return  m_current;
}

void DFMSideBarView::currentChanged(const QModelIndex &previous)
{
    m_current = currentIndex();
    m_previous = previous;
}

bool DFMSideBarView::onDropData(DUrlList srcUrls, DUrl dstUrl, Qt::DropAction action) const
{
    const DAbstractFileInfoPointer &dstInfo = fileService->createFileInfo(this, dstUrl);
    if (!dstInfo) {
        return false;
    }

    for (DUrl &u : srcUrls) {
        // we only do redirection for burn:// urls for the fear of screwing everything up again
        if (u.scheme() == BURN_SCHEME) {
            DAbstractFileInfoPointer fi = fileService->createFileInfo(nullptr, u);
            while (fi && fi->canRedirectionFileUrl()) {
                u = fi->redirectedFileUrl();
                fi = fileService->createFileInfo(nullptr, u);
            }
        }
    }

    // convert destnation url to real path if it's a symbol link.
    if (dstInfo->isSymLink()) {
        dstUrl = dstInfo->rootSymLinkTarget();
    }

    switch (action) {
    case Qt::CopyAction:
        // blumia: should run in another thread or user won't do another DnD opreation unless the copy action done.
        QtConcurrent::run([ = ]() {
            fileService->pasteFile(this, DFMGlobal::CopyAction, dstUrl, srcUrls);
        });
        break;
    case Qt::LinkAction:
        break;
    case Qt::MoveAction:
        fileService->pasteFile(this, DFMGlobal::CutAction, dstUrl, srcUrls);
        break;
    default:
        return false;
    }

    return true;
}

DFMSideBarItem *DFMSideBarView::itemAt(const QPoint &pt)
{
    DFMSideBarItem *item = nullptr;
    QModelIndex index = indexAt(pt);
    if (!index.isValid()) {
        return item;
    }

    DFMSideBarModel *mod = dynamic_cast<DFMSideBarModel *>(model());
    Q_ASSERT(mod);
    item = mod->itemFromIndex(index);
    Q_ASSERT(item);

    return item;
}

Qt::DropAction DFMSideBarView::canDropMimeData(DFMSideBarItem *item, const QMimeData *data, Qt::DropActions actions) const
{
    Q_UNUSED(data)
    // Got a copy of urls so whatever data was changed, it won't affact the following code.
    QList<QUrl> urls = m_urlsForDragEvent;

    if (urls.empty()) {
        return Qt::IgnoreAction;
    }

    for (const QUrl &url : urls) {
        const DAbstractFileInfoPointer &fileInfo = fileService->createFileInfo(this, url);
        if (!fileInfo || !fileInfo->isReadable()) {
            return Qt::IgnoreAction;
        }
    }

    const DAbstractFileInfoPointer &info = fileService->createFileInfo(this, item->url());

    if (!info || !info->canDrop()) {
        return Qt::IgnoreAction;
    }

    const Qt::DropActions support_actions = info->supportedDropActions() & actions;

    if (DStorageInfo::inSameDevice(urls.first(), item->url())) {
        if (support_actions.testFlag(Qt::MoveAction)) {
            return Qt::MoveAction;
        }
    }

    if (support_actions.testFlag(Qt::CopyAction)) {
        return Qt::CopyAction;
    }

    if (support_actions.testFlag(Qt::MoveAction)) {
        return Qt::MoveAction;
    }

    if (support_actions.testFlag(Qt::LinkAction)) {
        return Qt::LinkAction;
    }

    return Qt::IgnoreAction;
}

bool DFMSideBarView::isAccepteDragEvent(DFMDragEvent *event)
{
    DFMSideBarItem *item = itemAt(event->pos());
    if (!item) {
        return false;
    }

    bool accept = false;
    Qt::DropAction action = canDropMimeData(item, event->mimeData(), event->proposedAction());
    if (action == Qt::IgnoreAction) {
        action = canDropMimeData(item, event->mimeData(), event->possibleActions());
    }

    if (action != Qt::IgnoreAction) {
        event->setDropAction(action);
        event->accept();
        accept = true;
    }

    return accept;
}

//添加此函数为解决拖拽后不选中拖拽项目问题
void DFMSideBarView::onRowCountChanged()
{
    if (isHidden()) {
        return; //在显示前不处理
    }

    if (previousRowCount == model()->rowCount()) {
        setCurrentIndex(indexAt(dropPos));
        if (dragItemName != model()->data(currentIndex()).toString()) {
            setCurrentIndex(model()->index(currentIndex().row() + (dragRow > currentIndex().row() ? 1 : -1), currentIndex().column()));
        }
    } else {
        dragItemName = model()->data(currentIndex()).toString();
        dragRow = currentIndex().row();
    }
}

bool DFMSideBarView::fetchDragEventUrlsFromSharedMemory()
{
    QSharedMemory sm;
    sm.setKey(DRAG_EVENT_URLS);

    if (!sm.isAttached()) {
        if (!sm.attach()) {
            qDebug() << "FQSharedMemory detach failed.";
            return false;
        }
    }

    QBuffer buffer;
    QDataStream in(&buffer);
    QList<QUrl> urls;

    sm.lock();
    //用缓冲区得到共享内存关联后得到的数据和数据大小
    buffer.setData((char *)sm.constData(), sm.size());
    buffer.open(QBuffer::ReadOnly);     //设置读取模式
    in >> m_urlsForDragEvent;               //使用数据流从缓冲区获得共享内存的数据，然后输出到字符串中
    sm.unlock();    //解锁
    sm.detach();//与共享内存空间分离

    return true;
}

bool DFMSideBarView::checkOpTime()
{
    //如果两次操作时间间隔足够长，则返回true
    if (QDateTime::currentDateTime().toMSecsSinceEpoch() - m_lastOpTime > 200) {
        m_lastOpTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
        return true;
    }

    return false;
}

DFM_END_NAMESPACE
