#ifndef MAINWINDOW_MEASUREMENTFLOW_PATCH_H
#define MAINWINDOW_MEASUREMENTFLOW_PATCH_H

/*
 * 这是 mainwindow.h 的“修改片段”，不是一个独立类。
 * 请按 README_集成说明.txt 把对应内容放入现有 mainwindow.h。
 */

class MeasurementFlowController; // 放在 class MainWindow; 定义之前

/*
在 MainWindow 类的 private: 区域加入：

    friend class MeasurementFlowController;

    MeasurementFlowController *measurementFlowController();
    MeasurementFlowController *m_measurementFlowController = nullptr;

保留或确认已有以下成员函数声明：

private slots:
    void on_start_clicked();
    void on_startMeasureBtn_clicked();

private:
    void executeMeasurement(int currentCount, int maxCount);
    void stopMotorAfterMeasurement();
    void continueAfterMeasurementData();
*/

#endif // MAINWINDOW_MEASUREMENTFLOW_PATCH_H
