#ifndef DFMOPTICALMEDIAWIDGET_H
#define DFMOPTICALMEDIAWIDGET_H

#include "app/define.h"
#include "dfileservices.h"

#include <QWidget>

DFM_BEGIN_NAMESPACE
class DFileStatisticsJob;
DFM_END_NAMESPACE
DFM_USE_NAMESPACE

struct CdStatusInfo {
    bool bVolFlag = false;
    bool bMntFlag = false;
    quint64 nUsage = 0;
    quint64 nTotal = 0;
    bool bBurningOrErasing = false;
    bool bProcessLocked = false;
    QString cachePath=""; // bug202007010027:添加文件缓存区，进行统一管理
};

class DFMOpticalMediaWidgetPrivate;
class DFMOpticalMediaWidget : public QWidget
{
    Q_OBJECT
public:
    //fix: 光盘容量属性: 光盘容量状态
    enum BurnCapacityStatusAttribute {
        BCSA_BurnCapacityStatusEjct = 0, //光盘容量状态：0,光驱弹出状态
        BCSA_BurnCapacityStatusAdd, //光盘容量状态：1,光驱弹入处于添加未挂载状态
        BCSA_BurnCapacityStatusAddMount //光盘容量状态：2,光驱弹入处于添加后并挂载的状态
    };

    //fixed:CD display size error
    static QMap<QString, CdStatusInfo> g_mapCdStatusInfo;
    //fix: 动态获取刻录选中文件的字节大小
    static qint64 g_selectBurnFilesSize;
    static qint64 g_selectBurnDirFileCount;

    explicit DFMOpticalMediaWidget(QWidget *parent);
    ~DFMOpticalMediaWidget();

    void updateDiscInfo(QString dev);
    void setDiscMountPoint(const QString &strMntPath);
    bool hasFileInDir(QDir dir);
    QString getDiscMountPoint();
    //fix: 设置光盘容量属性
    static void setBurnCapacity(int status, QString strVolTag = "");

    //根据url 获取光驱标签
    static QString getVolTag(const DUrl &fileUrl);

    //判断当前磁盘是否正忙碌
    static bool hasVolProcessBusy();

    //fix: 动态更新光驱磁盘状态
private slots:
    void selectBurnFilesOptionUpdate();

private:
    QScopedPointer<DFMOpticalMediaWidgetPrivate> d_ptr;
    Q_DECLARE_PRIVATE(DFMOpticalMediaWidget)
    DFileStatisticsJob *m_pStatisticWorker = nullptr;
};

#endif // DFMOPTICALMEDIAWIDGET_H
