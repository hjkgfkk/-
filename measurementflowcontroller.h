#ifndef MEASUREMENTFLOWCONTROLLER_H
#define MEASUREMENTFLOWCONTROLLER_H

#include <QObject>

class MainWindow;

/**
 * @brief 负责测量流程编排。
 *
 * 该类只处理：
 * 1. 测量启动与单眼/双眼次数判断；
 * 2. 串口测量命令与数据等待；
 * 3. 测量计数、历史缓存和数据库保存；
 * 4. 连续测量、双眼切换和测量结束后的电机停止。
 *
 * 图像参数计算、WTW计算和测量页面初始化仍由 MainWindow 负责。
 */
class MeasurementFlowController final : public QObject
{
public:
    explicit MeasurementFlowController(MainWindow *window);

    void handleStartClicked();
    void executeMeasurement(int currentCount, int maxCount);
    void stopMotorAfterMeasurement();

private:
    void applySelectedLayout();
    bool saveCurrentMeasurementToDatabase();
    void logCurrentMeasurementSnapshot() const;
    bool updateCurrentEyeCounter();
    void appendHistoryAndRefreshMeasureWidget();
    void handleSequenceFinished(int maxCount);

private:
    MainWindow *m_window = nullptr;
};

#endif // MEASUREMENTFLOWCONTROLLER_H
