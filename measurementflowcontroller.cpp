#include "measurementflowcontroller.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>

namespace {
constexpr int kFirstMeasurementCount = 1;
constexpr int kDefaultAutomaticMeasurementCount = 3;
constexpr int kSerialWaitTimeoutMs = 5000;
constexpr int kNextMeasurementDelayMs = 500;
constexpr int kOppositeEyeMoveDelayMs = 3200;
constexpr int kRestartImageTimerDelayMs = 3000;
constexpr int kImageTimerIntervalMs = 20;
constexpr int kLeftEye = 2;
constexpr int kRightEye = 1;
constexpr int kUnknownEye = 0;
constexpr int kMaxCounterValue = 99;
constexpr int kMeasureHistoryLimit = 10;
}

MeasurementFlowController::MeasurementFlowController(MainWindow *window)
    : QObject(window),
      m_window(window)
{
}

void MeasurementFlowController::handleStartClicked()
{
    MainWindow *w = m_window;
    if (!w || !w->startMeasure) {
        return;
    }

    applySelectedLayout();

    const int currentEyeDataCount = w->getCurrentEyeDataCount(w->m_currentEyeType);

    const QString configPath =
        QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
    QSettings settings(configPath, QSettings::IniFormat);
    const bool isBothEyeMode =
        settings.value(QStringLiteral("yanceliangmoshi/shuang"), true).toBool();

    if (w->m_imageProcessTimer) {
        w->m_imageProcessTimer->stop();
    }

    if (currentEyeDataCount > 0) {
        qDebug() << "当前眼别" << w->m_currentEyeType
                 << "已有" << currentEyeDataCount
                 << "条数据，执行1次测量";
        executeMeasurement(kFirstMeasurementCount, 1);
        return;
    }

    const bool measuringOppositeEye =
        w->m_isBothEyeAutoMeasuring &&
        w->m_bothEyeCurrentEye != w->m_bothEyeFirstEye;

    if (measuringOppositeEye) {
        qDebug() << "双眼模式：对侧眼" << w->m_currentEyeType
                 << "没有数据，执行3次测量";
        executeMeasurement(kFirstMeasurementCount,
                           kDefaultAutomaticMeasurementCount);
        return;
    }

    if (!isBothEyeMode) {
        qDebug() << "单眼模式：当前眼别" << w->m_currentEyeType
                 << "没有数据，执行3次测量";
        executeMeasurement(kFirstMeasurementCount,
                           kDefaultAutomaticMeasurementCount);
        return;
    }

    qDebug() << "双眼模式：第一只眼" << w->m_currentEyeType
             << "没有数据，执行3次测量";

    w->m_isBothEyeAutoMeasuring = true;
    w->m_bothEyeFirstEye = w->m_currentEyeType;
    w->m_bothEyeCurrentEye = w->m_currentEyeType;

    executeMeasurement(kFirstMeasurementCount,
                       kDefaultAutomaticMeasurementCount);
}

void MeasurementFlowController::applySelectedLayout()
{
    MainWindow *w = m_window;
    if (!w) {
        return;
    }

    w->resetControlsToOriginalPosition();

    const bool kerOnlyMode =
        w->m_kerSelected && !w->m_alSelected && !w->m_refSelected;
    const bool alOnlyMode =
        !w->m_kerSelected && w->m_alSelected && !w->m_refSelected;
    const bool refOnlyMode =
        !w->m_kerSelected && !w->m_alSelected && w->m_refSelected;
    const bool refKerMode =
        w->m_kerSelected && !w->m_alSelected && w->m_refSelected;
    const bool alRefMode =
        !w->m_kerSelected && w->m_alSelected && w->m_refSelected;
    const bool alKerMode =
        w->m_kerSelected && w->m_alSelected && !w->m_refSelected;
    const bool allThreeMode =
        w->m_kerSelected && w->m_alSelected && w->m_refSelected;
    const bool noneSelectedMode =
        !w->m_kerSelected && !w->m_alSelected && !w->m_refSelected;

    if (refOnlyMode) {
        qDebug() << "仅REF模式：REF移动到AL位置";
        w->moveRefToAlPositions();
    } else if (kerOnlyMode) {
        w->moveKerToALPosition();
    } else if (alOnlyMode) {
        w->updateALDataDisplay();
    } else if (refKerMode) {
        w->moveRefToAlPositions();
        w->moveKerParamsToSCAPositions();
    } else if (alRefMode) {
        w->updateALDataDisplay();
    } else if (alKerMode) {
        w->movekerTorefposition();
        w->updateALDataDisplay();
    } else if (allThreeMode) {
        w->moveKerControlsToRRAPositions();
        w->updateALDataDisplay();
    } else if (noneSelectedMode) {
        w->m_kerSelected = true;
        w->m_alSelected = true;
        w->m_refSelected = true;
        w->moveKerControlsToRRAPositions();
        w->updateALDataDisplay();
        qDebug() << "三个按钮均未选择：按AL + REF + KER显示，模式"
                 << w->m_cornealMode;
    }

    w->updateLabelsVisibility(w->m_kerSelected,
                              w->m_alSelected,
                              w->m_refSelected);
}

void MeasurementFlowController::executeMeasurement(int currentCount,
                                                   int maxCount)
{
    MainWindow *w = m_window;
    if (!w) {
        return;
    }

    if (currentCount < 1 || maxCount < 1 || currentCount > maxCount) {
        qWarning() << "无效的测量次数参数：currentCount=" << currentCount
                   << "maxCount=" << maxCount;
        return;
    }

    if (!updateCurrentEyeCounter()) {
        return;
    }

    w->m_currentMeasurementCount = currentCount;
    w->m_maxMeasurementCount = maxCount;

    qDebug() << "开始执行第" << currentCount
             << "次测量，共" << maxCount << "次";

    // 先处理当前图像，得到R1/R2/K1/K2/WTW等图像参数。
    w->continueAfterMeasurementData();

    if (!w->m_serialReceiver) {
        qCritical() << "m_serialReceiver为空，无法执行测量";
        return;
    }

    w->m_serialReceiver->resetMeasurementData();
    qDebug() << "已清除上一轮串口测量数据";

    w->m_measurementStartTime = QDateTime::currentMSecsSinceEpoch();

    if (w->m_currentEyeType == kLeftEye) {
        qDebug() << "发送左眼测量指令";
        w->m_serialReceiver->sendEyeControlCmd(true);
    } else if (w->m_currentEyeType == kRightEye) {
        qDebug() << "发送右眼测量指令";
        w->m_serialReceiver->sendEyeControlCmd(false);
    }

    int rl = 0;
    int rs = 0;
    int angle = 0;

    qDebug() << "等待串口测量数据，超时" << kSerialWaitTimeoutMs << "ms";
    if (w->m_serialReceiver->waitForMeasurementData(
            rl, rs, angle, kSerialWaitTimeoutMs)) {
        qDebug() << "接收到测量数据：rl=" << rl
                 << "rs=" << rs << "angle=" << angle;

        if (w->m_currentEyeType == kLeftEye) {
            w->rawRl_L = rl;
            w->rawRs_L = rs;
            w->rawAngle_L = angle;
        } else if (w->m_currentEyeType == kRightEye) {
            w->rawRl_R = rl;
            w->rawRs_R = rs;
            w->rawAngle_R = angle;
        }
    } else {
        qWarning() << "测量数据接收超时，使用默认测试值";

        if (w->m_currentEyeType == kLeftEye) {
            w->rawRl_L = 208;
            w->rawRs_L = 208;
            w->rawAngle_L = 1;
        } else if (w->m_currentEyeType == kRightEye) {
            w->rawRl_R = 208;
            w->rawRs_R = 208;
            w->rawAngle_R = 1;
        }
    }

    // 串口数据到达后重新计算S/C/A，确保数据库保存的是本次测量结果，
    // 而不是原代码中“测量开始前”的上一轮快照。
    w->recalcAndUpdate();
    w->updateALDataDisplay();

    saveCurrentMeasurementToDatabase();
    appendHistoryAndRefreshMeasureWidget();

    if (currentCount < maxCount) {
        qDebug() << "第" << currentCount
                 << "次测量完成，准备执行第" << currentCount + 1 << "次";

        QTimer::singleShot(
            kNextMeasurementDelayMs,
            w,
            [this, currentCount, maxCount]() {
                executeMeasurement(currentCount + 1, maxCount);
            });
        return;
    }

    handleSequenceFinished(maxCount);
}

bool MeasurementFlowController::saveCurrentMeasurementToDatabase()
{
    MainWindow *w = m_window;
    if (!w) {
        return false;
    }

    const QString dbName = QStringLiteral("nuc_total.db");
    const QString dbFilePath = QFileInfo(dbName).absoluteFilePath();
    const QString connectionName = QStringLiteral("measurement_flow_connection");

    QSqlDatabase db;
    if (QSqlDatabase::contains(connectionName)) {
        db = QSqlDatabase::database(connectionName);
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                       connectionName);
    }

    if (db.databaseName() != dbFilePath) {
        if (db.isOpen()) {
            db.close();
        }
        db.setDatabaseName(dbFilePath);
    }

    if (!db.open()) {
        qCritical() << "数据库打开失败：" << db.lastError().text();
        return false;
    }

    const QString tableName = QStringLiteral("tabel_") + w->s_uuidserial;
    const QString sql = QStringLiteral(
        "INSERT INTO %1 "
        "(l_al, l_cct, l_ad, l_vt, l_r1, l_r2, l_ax, l_avg, "
        "l_s, l_c, l_a, l_k1, l_k2, "
        "r_al, r_cct, r_ad, r_vt, r_r1, r_r2, r_ax, r_avg, "
        "r_s, r_c, r_a, r_k1, r_k2, vd, pd, cyl, times) "
        "VALUES "
        "(:l_al, :l_cct, :l_ad, :l_vt, :l_r1, :l_r2, :l_ax, :l_avg, "
        ":l_s, :l_c, :l_a, :l_k1, :l_k2, "
        ":r_al, :r_cct, :r_ad, :r_vt, :r_r1, :r_r2, :r_ax, :r_avg, "
        ":r_s, :r_c, :r_a, :r_k1, :r_k2, :vd, :pd, :cyl, :times)")
                            .arg(tableName);

    QSqlQuery query(db);
    if (!query.prepare(sql)) {
        qCritical() << "数据库SQL预处理失败：" << query.lastError().text();
        db.close();
        return false;
    }

    const QString dateTimeStr =
        QDateTime::currentDateTime().toString(
            QStringLiteral("yyyy-MM-dd-HH-mm-ss-zzz"));

    query.bindValue(QStringLiteral(":l_al"), w->raw_AL_L);
    query.bindValue(QStringLiteral(":l_cct"), w->raw_wtw_L);
    query.bindValue(QStringLiteral(":l_ad"), w->raw_pupd_L);
    query.bindValue(QStringLiteral(":l_vt"), w->raw_ALCR_L);
    query.bindValue(QStringLiteral(":l_r1"), w->rawR1_L);
    query.bindValue(QStringLiteral(":l_r2"), w->rawR2_L);
    query.bindValue(QStringLiteral(":l_ax"), w->rawAX_L);
    query.bindValue(QStringLiteral(":l_avg"), w->rawAvg_L);
    query.bindValue(QStringLiteral(":l_s"), w->rawS_L);
    query.bindValue(QStringLiteral(":l_c"), w->rawC_L);
    query.bindValue(QStringLiteral(":l_a"), w->rawA_L);
    query.bindValue(QStringLiteral(":l_k1"), w->rawK1_L);
    query.bindValue(QStringLiteral(":l_k2"), w->rawK2_L);

    query.bindValue(QStringLiteral(":r_al"), w->raw_AL_R);
    query.bindValue(QStringLiteral(":r_cct"), w->raw_wtw_R);
    query.bindValue(QStringLiteral(":r_ad"), w->raw_pupd_R);
    query.bindValue(QStringLiteral(":r_vt"), w->raw_ALCR_R);
    query.bindValue(QStringLiteral(":r_r1"), w->rawR1_R);
    query.bindValue(QStringLiteral(":r_r2"), w->rawR2_R);
    query.bindValue(QStringLiteral(":r_ax"), w->rawAX_R);
    query.bindValue(QStringLiteral(":r_avg"), w->rawAvg_R);
    query.bindValue(QStringLiteral(":r_s"), w->rawS_R);
    query.bindValue(QStringLiteral(":r_c"), w->rawC_R);
    query.bindValue(QStringLiteral(":r_a"), w->rawA_R);
    query.bindValue(QStringLiteral(":r_k1"), w->rawK1_R);
    query.bindValue(QStringLiteral(":r_k2"), w->rawK2_R);

    query.bindValue(QStringLiteral(":vd"), w->currentVdIndex);
    query.bindValue(QStringLiteral(":pd"), w->ChuTongJu);
    query.bindValue(QStringLiteral(":cyl"), w->currentcylIndex);
    query.bindValue(QStringLiteral(":times"), dateTimeStr);

    const bool success = query.exec();
    if (!success) {
        qCritical() << "数据库插入失败：" << query.lastError().text();
    } else {
        qDebug() << "数据库插入成功，时间=" << dateTimeStr;
        logCurrentMeasurementSnapshot();
    }

    db.close();
    return success;
}

void MeasurementFlowController::logCurrentMeasurementSnapshot() const
{
    const MainWindow *w = m_window;
    if (!w) {
        return;
    }

    qDebug() << "=== 当前测量数据库参数 ===";
    qDebug() << "s_uuidserial=" << w->s_uuidserial;
    qDebug() << "左眼："
             << "AL=" << w->raw_AL_L
             << "WTW=" << w->raw_wtw_L
             << "PUPD=" << w->raw_pupd_L
             << "AL/CR=" << w->raw_ALCR_L
             << "R1=" << w->rawR1_L
             << "R2=" << w->rawR2_L
             << "AX=" << w->rawAX_L
             << "S=" << w->rawS_L
             << "C=" << w->rawC_L
             << "A=" << w->rawA_L
             << "K1=" << w->rawK1_L
             << "K2=" << w->rawK2_L;
    qDebug() << "右眼："
             << "AL=" << w->raw_AL_R
             << "WTW=" << w->raw_wtw_R
             << "PUPD=" << w->raw_pupd_R
             << "AL/CR=" << w->raw_ALCR_R
             << "R1=" << w->rawR1_R
             << "R2=" << w->rawR2_R
             << "AX=" << w->rawAX_R
             << "S=" << w->rawS_R
             << "C=" << w->rawC_R
             << "A=" << w->rawA_R
             << "K1=" << w->rawK1_R
             << "K2=" << w->rawK2_R;
    qDebug() << "VD=" << w->currentVdIndex
             << "PD=" << w->ChuTongJu
             << "CYL=" << w->currentcylIndex;
}

bool MeasurementFlowController::updateCurrentEyeCounter()
{
    MainWindow *w = m_window;
    if (!w) {
        return false;
    }

    if (w->m_currentEyeType == kUnknownEye) {
        qWarning() << "当前眼别无效，取消测量";
        return false;
    }

    if (w->m_currentEyeType == kLeftEye && w->m_leftCounter < kMaxCounterValue) {
        ++w->m_leftCounter;
        const QString count =
            QStringLiteral("%1").arg(w->m_leftCounter, 2, 10, QChar('0'));
        w->ui->label_11->setText(count);
        w->ui->label_12->setText(QStringLiteral("/ ") + count);
        w->ui->label_13->setText(QStringLiteral("/ ") + count);
        qDebug() << "左眼计数更新为" << count;
    } else if (w->m_currentEyeType == kRightEye &&
               w->m_rightCounter < kMaxCounterValue) {
        ++w->m_rightCounter;
        const QString count =
            QStringLiteral("%1").arg(w->m_rightCounter, 2, 10, QChar('0'));
        w->ui->label_10->setText(count);
        w->ui->label_9->setText(QStringLiteral("/ ") + count);
        w->ui->label_8->setText(QStringLiteral("/ ") + count);
        qDebug() << "右眼计数更新为" << count;
    }

    return true;
}

void MeasurementFlowController::appendHistoryAndRefreshMeasureWidget()
{
    MainWindow *w = m_window;
    if (!w) {
        return;
    }

    MeasurementData newData(
        w->rawK1_L, w->rawK2_L, w->rawAX_L,
        w->rawK1_R, w->rawK2_R, w->rawAX_R,
        w->rawR1_L, w->rawR2_L,
        w->rawR1_R, w->rawR2_R,
        w->rawD_L, w->rawC1_L, w->rawAX_L,
        w->rawD_R, w->rawC1_R, w->rawAX_R,
        w->rawRl_L, w->rawRs_L, w->rawAngle_L,
        w->rawRl_R, w->rawRs_R, w->rawAngle_R,
        w->rawS_L, w->rawC_L, w->rawA_L,
        w->rawS_R, w->rawC_R, w->rawA_R,
        w->raw_AL_L, w->raw_wtw_L, w->raw_pupd_L,
        w->raw_AL_R, w->raw_wtw_R, w->raw_pupd_R,
        w->m_currentEyeType);

    if (w->m_measureHistoryBuffer.size() < kMeasureHistoryLimit) {
        w->m_measureHistoryBuffer.append(newData);
    } else {
        w->m_measureHistoryBuffer.removeFirst();
        w->m_measureHistoryBuffer.append(newData);
    }

    qDebug() << "测量历史缓存行数=" << w->m_measureHistoryBuffer.size();

    if (w->m_measureWidget && w->m_measureWidget->isVisible()) {
        w->m_measureWidget->addMeasurementData(
            w->rawK1_L, w->rawK2_L, w->rawAX_L,
            w->rawK1_R, w->rawK2_R, w->rawAX_R,
            w->rawR1_L, w->rawR2_L,
            w->rawR1_R, w->rawR2_R,
            w->rawD_L, w->rawC1_L, w->rawAX_L,
            w->rawD_R, w->rawC1_R, w->rawAX_R,
            w->rawRl_L, w->rawRs_L, w->rawAngle_L,
            w->rawRl_R, w->rawRs_R, w->rawAngle_R,
            w->raw_AL_L, w->raw_wtw_L, w->raw_pupd_L,
            w->raw_AL_R, w->raw_wtw_R, w->raw_pupd_R,
            w->m_currentEyeType);
    }
}

void MeasurementFlowController::handleSequenceFinished(int maxCount)
{
    MainWindow *w = m_window;
    if (!w) {
        return;
    }

    w->startMeasure = false;
    w->focus_succ = false;

    qDebug() << maxCount << "次测量全部完成";

    w->recalcAndUpdate();
    w->updateALDataDisplay();

    const bool firstEyeFinished =
        w->m_isBothEyeAutoMeasuring &&
        w->m_bothEyeCurrentEye == w->m_bothEyeFirstEye;

    if (firstEyeFinished) {
        qDebug() << "双眼模式：第一只眼" << w->m_bothEyeFirstEye
                 << "测量完成，准备切换对侧眼";

        // 下面命令在用户提供的代码中为空；请替换为设备真实协议字节。
        const QByteArray motorStopCommand = QByteArray::fromHex("");
        w->m_serialReceiver->sendEyeSerialData(motorStopCommand);
        w->m_motorRotationSent = false;
        w->hasExecutedLessThan20 = false;

        const int oppositeEyeType =
            (w->m_bothEyeFirstEye == kLeftEye) ? kRightEye : kLeftEye;

        QByteArray moveCommand;
        if (w->m_bothEyeFirstEye == kLeftEye) {
            // TODO: 填入“左眼位置移动到右眼位置”的真实串口命令。
            moveCommand = QByteArray::fromHex("");
        } else {
            // TODO: 填入“右眼位置移动到左眼位置”的真实串口命令。
            moveCommand = QByteArray::fromHex("");
        }

        if (!moveCommand.isEmpty()) {
            w->m_serialReceiver->sendEyeSerialData(moveCommand);
            w->m_bothEyeMotorMoving = true;
            w->m_waitingForMotorConfirm.store(true);
        } else {
            qWarning() << "双眼移动命令为空：当前仅按固定延时切换眼别";
        }

        QTimer::singleShot(
            kOppositeEyeMoveDelayMs,
            w,
            [this, oppositeEyeType]() {
                MainWindow *window = m_window;
                if (!window) {
                    return;
                }

                window->m_currentEyeType = oppositeEyeType;
                window->m_bothEyeCurrentEye = oppositeEyeType;

                window->startMeasure = false;
                window->focus_succ = false;
                window->hasExecutedLessThan100 = false;
                window->hasExecutedLessThan20 = false;
                window->m_motorRotationSent = false;
                window->AL = 0;

                if (window->m_imageProcessTimer) {
                    window->m_imageProcessTimer->start(kImageTimerIntervalMs);
                }

                qDebug() << "已切换到对侧眼" << oppositeEyeType
                         << "，等待自动找眼或用户再次触发测量";
            });
        return;
    }

    const bool oppositeEyeFinished =
        w->m_isBothEyeAutoMeasuring &&
        w->m_bothEyeCurrentEye != w->m_bothEyeFirstEye;

    if (oppositeEyeFinished) {
        qDebug() << "双眼模式：对侧眼" << w->m_bothEyeCurrentEye
                 << "测量完成，双眼流程结束";
        w->m_isBothEyeAutoMeasuring = false;
        w->m_bothEyeCurrentEye = 0;
        w->m_bothEyeFirstEye = 0;
        w->m_bothEyeMotorMoving = false;
    }

    stopMotorAfterMeasurement();
}

void MeasurementFlowController::stopMotorAfterMeasurement()
{
    MainWindow *w = m_window;
    if (!w) {
        return;
    }

    qDebug() << "测量完成，开始停止电机";

    // 用户提供的停止命令为空；请替换为设备真实协议字节。
    const QByteArray motorStopCommand = QByteArray::fromHex("");
    if (w->m_serialReceiver) {
        w->m_serialReceiver->sendfocusSerialData(motorStopCommand);
    }

    QTimer::singleShot(
        kRestartImageTimerDelayMs,
        w,
        [this]() {
            MainWindow *window = m_window;
            if (!window) {
                return;
            }

            window->startMeasure = false;
            window->focus_succ = false;
            window->m_motorRotationSent = false;
            window->m_preparationDone = false;
            window->AL = 0;

            if (window->m_imageProcessTimer) {
                window->m_imageProcessTimer->start(kImageTimerIntervalMs);
            }

            qDebug() << "测量标志已重置，图像处理定时器已重启";
        });
}
