#include "mainwindow.h"
#include "measurementflowcontroller.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QImage>
#include <QTimer>

/*
 * 若 WtwResult、WtwProcessor、calculateEllipseParametersFromQImage、g_vars
 * 不在 mainwindow.h 可见，请把原 mainwindow.cpp 中对应的项目头文件 include
 * 复制到本文件顶部。不要重复保留原 mainwindow.cpp 中以下函数的定义。
 */

MeasurementFlowController *MainWindow::measurementFlowController()
{
    if (!m_measurementFlowController) {
        m_measurementFlowController = new MeasurementFlowController(this);
    }
    return m_measurementFlowController;
}

void MainWindow::on_start_clicked()
{
    measurementFlowController()->handleStartClicked();
}

void MainWindow::executeMeasurement(int currentCount, int maxCount)
{
    measurementFlowController()->executeMeasurement(currentCount, maxCount);
}

void MainWindow::stopMotorAfterMeasurement()
{
    measurementFlowController()->stopMotorAfterMeasurement();
}

void MainWindow::continueAfterMeasurementData()
{
    qDebug() << "继续执行图像与参数处理";

    const QImage eyeImage = image_nuc.copy();
    if (eyeImage.isNull()) {
        qWarning() << "当前眼图像为空，跳过图像处理";
        recalcAndUpdate();
        updateALDataDisplay();
        updatetongjuDataDisplay();
        return;
    }

    const int currentEyeType = getCurrentEyeTypeFromMotor();
    QString eyeType;
    if (currentEyeType == 2) {
        eyeType = QStringLiteral("left");
    } else if (currentEyeType == 1) {
        eyeType = QStringLiteral("right");
    } else {
        eyeType = QStringLiteral("unknown");
    }

    if (eyeType == QStringLiteral("unknown")) {
        qWarning() << "眼别数据无效，跳过图像处理";
        recalcAndUpdate();
        updateALDataDisplay();
        updatetongjuDataDisplay();
        return;
    }

    float frl = 0.0f;
    float frs = 0.0f;
    float fangle = 0.0f;
    float R1 = 0.0f;
    float R2 = 0.0f;
    float AX = 0.0f;
    float K1 = 0.0f;
    float K2 = 0.0f;
    float D = 0.0f;
    float C1 = 0.0f;
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;
    float centerX = 640.0f;
    float centerY = 0.0f;

    const bool ellipseOk = calculateEllipseParametersFromQImage(
        eyeImage,
        feilingchutongju,
        frl,
        frs,
        fangle,
        R1,
        R2,
        AX,
        K1,
        K2,
        D,
        C1,
        innerRadius,
        outerRadius,
        centerX,
        centerY,
        eyeType);

    if (ellipseOk) {
        qDebug() << "图像处理成功，眼别=" << eyeType;
        qDebug() << "R1=" << R1 << "R2=" << R2
                 << "AX=" << AX << "K1=" << K1
                 << "K2=" << K2 << "D=" << D << "C=" << C1;
        qDebug() << "内环半径=" << innerRadius
                 << "外环半径=" << outerRadius
                 << "圆心=" << centerX << centerY;

        if (eyeType == QStringLiteral("left")) {
            rawR1_L = R1;
            rawR2_L = R2;
            rawAX_L = AX;
            rawK1_L = K1;
            rawK2_L = K2;
            rawD_L = D;
            rawC1_L = C1;
        } else {
            rawR1_R = R1;
            rawR2_R = R2;
            rawAX_R = AX;
            rawK1_R = K1;
            rawK2_R = K2;
            rawD_R = D;
            rawC1_R = C1;
        }

        const WtwResult wtwResult =
            WtwProcessor::processImage(eyeImage, innerRadius, centerX);

        if (wtwResult.valid) {
            if (eyeType == QStringLiteral("left")) {
                raw_wtw_L = wtwResult.wtw;
                raw_pupd_L = wtwResult.puiLD;
                raw_AL_L = AL;

                const float denominator = rawR1_L + rawR2_L;
                raw_ALCR_L =
                    qFuzzyIsNull(denominator) ? 0.0f : AL / denominator;

                qDebug() << "左眼PUPD=" << raw_pupd_L
                         << "WTW=" << raw_wtw_L;
            } else {
                raw_wtw_R = wtwResult.wtw;
                raw_pupd_R = wtwResult.puiLD;
                raw_AL_R = AL;

                const float denominator = rawR1_R + rawR2_R;
                raw_ALCR_R =
                    qFuzzyIsNull(denominator) ? 0.0f : AL / denominator;
            }
        } else {
            qWarning() << "WTW计算失败";
        }
    } else {
        qWarning() << "椭圆参数计算失败，跳过WTW计算";
    }

    recalcAndUpdate();
    updateALDataDisplay();
    updatetongjuDataDisplay();
}

void MainWindow::on_startMeasureBtn_clicked()
{
    rawRl_R = rawRs_R = rawRl_L = rawRs_L = 208;
    rawAngle_R = rawAngle_L = 1;
    rawS_R = rawC_R = rawA_R = 0;
    rawS_L = rawC_L = rawA_L = 0;

    rawR1_R = rawR2_R = rawAX_R = rawK1_R = rawK2_R = rawD_R = rawC1_R = 0;
    rawR1_L = rawR2_L = rawAX_L = rawK1_L = rawK2_L = rawD_L = rawC1_L = 0;

    raw_wtw_L = raw_pupd_L = 0;
    raw_wtw_R = raw_pupd_R = 0;
    raw_AL_L = raw_ALCR_L = 0;
    raw_AL_R = raw_ALCR_R = 0;
    m_currentInterpupillaryDistance = 0;

    g_vars.reset();
    recalcAndUpdate();
    updatetongjuDataDisplay();

    m_leftCounter = 0;
    ui->label_11->setText(QStringLiteral("00"));
    ui->label_12->setText(QStringLiteral("/ 00"));
    ui->label_13->setText(QStringLiteral("/ 00"));

    m_rightCounter = 0;
    ui->label_10->setText(QStringLiteral("00"));
    ui->label_9->setText(QStringLiteral("/ 00"));
    ui->label_8->setText(QStringLiteral("/ 00"));

    m_measureHistoryBuffer.clear();

    ui->stackedWidget_8->setCurrentWidget(ui->page);
    initalrefrraControlriginalPositions();
    initLabelsVisibility();

    if (!m_imageProcessTimer) {
        m_imageProcessTimer = new QTimer(this);
        connect(m_imageProcessTimer,
                &QTimer::timeout,
                this,
                &MainWindow::onImageProcessTimerTimeout);
    }

    m_isImageProcessing = false;
    hasExecutedLessThan100 = false;
    hasExecutedLessThan20 = false;
    m_isUsbReceiving = false;
    m_motorRotationSent = false;
    startMeasure = false;
    focus_succ = false;
    m_laserOn = false;
    m_waitingForMotorConfirm.store(false);
    m_waitingForPd61Move = false;
    m_pupilXYAligned = false;
    m_preparationDone = false;
    m_auto = true;

    ui->control1->setStyleSheet(
        QStringLiteral("border-image: url(:/img/control0.png); "
                       "background-color: transparent; border: none;"));

    m_imageProcessTimer->start(20);
    qDebug() << "图像处理定时器启动";
}
