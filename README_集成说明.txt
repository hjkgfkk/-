一、文件划分

1. measurementflowcontroller.h
   测量流程控制器声明。

2. measurementflowcontroller.cpp
   只放测量流程：
   - 开始测量；
   - 单眼/双眼次数判断；
   - 串口测量；
   - 连续测量；
   - 数据库保存；
   - 双眼切换；
   - 测量完成后停止电机。

3. mainwindow_measurement_other.cpp
   放其他逻辑和MainWindow入口：
   - on_start_clicked/executeMeasurement/stopMotorAfterMeasurement的转发；
   - continueAfterMeasurementData图像参数处理；
   - on_startMeasureBtn_clicked测量页面初始化。

4. mainwindow_header_patch.h
   展示需要添加到现有mainwindow.h的声明。
   该文件本身不需要加入工程编译。

二、必须修改现有mainwindow.h

1. 在MainWindow类声明之前加入：

class MeasurementFlowController;

2. 在MainWindow类private区域加入：

friend class MeasurementFlowController;
MeasurementFlowController *measurementFlowController();
MeasurementFlowController *m_measurementFlowController = nullptr;

3. 保留或确认存在以下声明：

void executeMeasurement(int currentCount, int maxCount);
void stopMotorAfterMeasurement();
void continueAfterMeasurementData();

on_start_clicked()和on_startMeasureBtn_clicked()仍放在private slots中。

三、删除原mainwindow.cpp中的重复定义

必须从原mainwindow.cpp删除以下五个函数的全部定义，否则会出现
“multiple definition/重复定义”链接错误：

MainWindow::on_start_clicked
MainWindow::executeMeasurement
MainWindow::stopMotorAfterMeasurement
MainWindow::continueAfterMeasurementData
MainWindow::on_startMeasureBtn_clicked

上传文本中这五个函数整体重复了两次，应全部删除后使用本文件夹中的实现。

四、工程文件加入方式

qmake .pro：

HEADERS += \
    measurementflowcontroller.h

SOURCES += \
    measurementflowcontroller.cpp \
    mainwindow_measurement_other.cpp

CMake：

target_sources(你的目标名 PRIVATE
    measurementflowcontroller.h
    measurementflowcontroller.cpp
    mainwindow_measurement_other.cpp
)

五、项目专用头文件

本次只有函数代码，没有原mainwindow.cpp顶部的include列表。
如果编译提示以下类型不完整或未声明：

WtwResult
WtwProcessor
SerialReceiver对应类型
MeasurementData

请把原mainwindow.cpp中定义这些类型的项目头文件，复制到对应新cpp顶部。
不要猜文件名，以你项目原来的include为准。

六、本次整理同时处理的明显问题

1. 去除了上传文本中重复两遍的函数实现。
2. 数据库保存移动到本次串口数据和S/C/A计算完成之后，避免首条保存零值、后续保存上一轮数据。
3. 使用独立SQLite连接名measurement_flow_connection，避免反复覆盖Qt默认数据库连接。
4. 椭圆识别失败时不再继续使用未初始化参数执行WTW计算。
5. AL/CR增加除零保护。
6. 保留了原同步等待串口数据的行为；该等待最长5秒，仍可能阻塞GUI线程。

七、仍需你补充的设备命令

上传代码中的以下QByteArray::fromHex内容为空：

- 电机停止命令；
- 左眼移动到右眼命令；
- 右眼移动到左眼命令；
- focus电机停止命令。

整理后的代码保留了空命令并标记TODO。必须替换为真实设备协议字节，
否则双眼模式只会按固定3200ms延时切换逻辑眼别，不会真实移动电机。
