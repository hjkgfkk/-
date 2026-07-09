void MainWindow::on_start_clicked()
{
    //// ===================================== 数据处理 =====================================
    if (startMeasure){
        // 首先恢复所有数据到原始位置，确保界面布局正确
        resetControlsToOriginalPosition();
        // 根据保存的选择状态更新界面显示
            bool kerOnlyMode = (m_kerSelected && !m_alSelected && !m_refSelected);
            bool alOnlyMode = (!m_kerSelected && m_alSelected && !m_refSelected);
            bool m_refOnlyMode = (!m_kerSelected && !m_alSelected && m_refSelected);
            bool refKerMode = (m_kerSelected && !m_alSelected && m_refSelected);
            bool alRefMode = (!m_kerSelected && m_alSelected && m_refSelected);
            bool alKerMode = (m_kerSelected && m_alSelected && !m_refSelected);
            bool allThreeMode = (m_kerSelected && m_alSelected && m_refSelected);
            bool allnoselectMode = (!m_kerSelected && !m_alSelected && !m_refSelected);

            // 新增：根据选择模式更新界面显示
            if (m_refOnlyMode) {
                // 只选择REF：显示SCA数据,REF移动到AL位置
                qDebug() << "调用moveRefToAlPositions()处理仅REF模式";
                moveRefToAlPositions();
            } else if (kerOnlyMode) {
                // 只选择KER：根据设置界面选择显示对应数据
                moveKerToALPosition();
            } else if (alOnlyMode) {
                // 只选择AL：显示AL、WTW、PUPD、AL/CR数据，并更新Al数据
                updateALDataDisplay();
            } else if (refKerMode) {
                // 同时选择REF和KER：显示SCA + 设置界面选择的数据
                moveRefToAlPositions();//将SCA移动到AL位置
                moveKerParamsToSCAPositions();//根据设置界面选择的KER数据，将其移动到SCA处
            } else if (alRefMode) {
                // 同时选择AL和REF：显示AL、WTW、PUPD、AL/CR + SCA数据，更新AL数据
                updateALDataDisplay();
            }
            else if (alKerMode) {
                // 同时选择AL和KER：显示AL、WTW、PUPD、AL/CR + 设置界面选择的数据
                movekerTorefposition();
                updateALDataDisplay();
            } else if (allThreeMode) {
                // 三个按钮都选择：显示AL、WTW、PUPD、AL/CR + SCA + 设置界面选择的数据
                moveKerControlsToRRAPositions();
                updateALDataDisplay();
            } else if(allnoselectMode){
                // 1201+FYB 新增：当三个按钮都不选时，显示AL + REF + 对应模式的KER数据
                // 设置选择状态：三个都选，显示所有数据
                m_kerSelected = true;
                m_alSelected = true;
                m_refSelected = true;
                // 移动KER控件到RRA原始位置（根据模式显示K1/K2/AX或D/C/A）
                moveKerControlsToRRAPositions();
                // 更新AL数据显示
                updateALDataDisplay();
                qDebug() << "All buttons unselected mode: show AL + REF + KER data (mode" << m_cornealMode << ")";
            }
            updateLabelsVisibility(m_kerSelected, m_alSelected, m_refSelected);



         // 获取当前眼别的数据数量
         int currentEyeDataCount = getCurrentEyeDataCount(m_currentEyeType);
         // 读取配置，判断是否为双眼模式
         QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
         QSettings settings(configPath, QSettings::IniFormat);
         bool isBothEyeMode = settings.value("yanceliangmoshi/shuang", true).toBool(); // 默认true，即双眼模式

         if (currentEyeDataCount == 0) {
             m_imageProcessTimer->stop();//

             // ========== 检查是否是双眼模式的对侧眼测量 ==========
             if (m_isBothEyeAutoMeasuring && m_bothEyeCurrentEye != m_bothEyeFirstEye) {
                 // 对侧眼测量：只执行3次测量，不再进入双眼模式
                 qDebug() << " 双眼模式：对侧眼" << m_currentEyeType
                          << "没有数据，开始3次测量（不进入双眼模式） ===";
                 executeMeasurement(1, 3); // 对侧眼3次测量
             } else if (!isBothEyeMode) {
                 // 当前眼别没有数据：自动执行3次测量（原逻辑不变）
                 qDebug() << " 单眼模式：当前眼别" << m_currentEyeType
                          << "没有数据，开始自动执行3次测量 ===";
                 executeMeasurement(1, 3); // 从第1次开始，总共3次
             }else{ //  双眼模式（第一只眼）
                 qDebug() << "双眼模式：当前眼别" << m_currentEyeType
                          << "没有数据，开始第一只眼3次测量 ===";

                 // ==========设置双眼模式标志位，保存第一只眼信息 ==========
                 m_isBothEyeAutoMeasuring = true;  // 标记正在进行双眼自动测量
                 m_bothEyeFirstEye = m_currentEyeType;  // 保存第一只眼类型
                 m_bothEyeCurrentEye = m_currentEyeType;  // 当前测量的眼别

                 // 执行第一只眼3次测量（测量完成后会在executeMeasurement中处理对侧眼切换）
                 executeMeasurement(1, 3); // 第一只眼3次
             }
         }else {
             m_imageProcessTimer->stop();//
             // 当前眼别已有数据：只执行1次测量
             qDebug() << "=== 当前眼别" << m_currentEyeType << "已有" << currentEyeDataCount << "条数据，开始执行1次测量 ===";
             executeMeasurement(1, 1); // 从第1次开始，总共1次
         }
    }
}

void MainWindow::executeMeasurement(int currentCount, int maxCount) {
    QString dbName="nuc_total.db";
    QFileInfo info(dbName);
    QString dbFilePath = info.absoluteFilePath();

    QSqlDatabase db;
    // 核心：先判断默认连接是否已存在，避免重复添加
    QString defaultConnName = "qt_sql_default_connection";
    if (QSqlDatabase::contains(defaultConnName)) {
        // 连接已存在，直接从连接池中获取该连接
        db = QSqlDatabase::database(defaultConnName);
        // 额外优化：检查获取的连接是否有效，若无效则重新配置
        if (!db.isValid() || db.databaseName() != dbFilePath) {
            db = QSqlDatabase::addDatabase("QSQLITE", defaultConnName);
            db.setDatabaseName(dbFilePath);
        }
    } else {
        // 连接不存在，才添加新连接
        db = QSqlDatabase::addDatabase("QSQLITE", defaultConnName);
        db.setDatabaseName(dbFilePath);
    }


    bool isOpen=db.open();
    if(isOpen)
    {
        QSqlQuery query(db);
        QString sql_insert = QString("INSERT INTO %1 "
                "(l_al, l_cct, l_ad, l_vt, l_r1, l_r2, l_ax, l_avg, l_s, l_c, l_a, l_k1, l_k2, r_al, r_cct, r_ad, r_vt, r_r1, r_r2, r_ax, r_avg, r_s, r_c, r_a, r_k1, r_k2, vd, pd, cyl, times) "
                "VALUES (:l_al, :l_cct, :l_ad, :l_vt, :l_r1, :l_r2, :l_ax, :l_avg, :l_s, :l_c, :l_a, :l_k1, :l_k2, :r_al, :r_cct, :r_ad, :r_vt, :r_r1, :r_r2, :r_ax, :r_avg, :r_s, :r_c, :r_a, :r_k1, :r_k2, :vd, :pd, :cyl,:times)")
                .arg("tabel_" + s_uuidserial);

        // 获取当前时间戳（作为主键，保证唯一性）
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString dateTimeStr = currentDateTime.toString("yyyy-MM-dd-HH-mm-ss-zzz");
        qDebug() << "dateTimeStr :"<< dateTimeStr;

        query.prepare(sql_insert);
        query.bindValue(":l_al", QString::number(raw_AL_L));
        query.bindValue(":l_cct",  QString::number(raw_wtw_L));              // 绑定密码变量
        query.bindValue(":l_ad",QString::number(raw_pupd_L));        // 绑定电话变量
        query.bindValue(":l_vt", QString::number(raw_ALCR_L));         // 绑定生日变量
        query.bindValue(":l_r1",QString::number(rawR1_L));
        query.bindValue(":l_r2",QString::number( rawR2_L));
        query.bindValue(":l_ax", QString::number(rawAX_L));
        query.bindValue(":l_avg", QString::number(rawAvg_L));
        query.bindValue(":l_s", QString::number(rawS_L));
        query.bindValue(":l_c", QString::number(rawC_L));
        query.bindValue(":l_a", QString::number(rawA_L));
        query.bindValue(":l_k1", QString::number(rawK1_L));
        query.bindValue(":l_k2", QString::number(rawK2_L));

        query.bindValue(":r_al", QString::number(raw_AL_R));
        query.bindValue(":r_cct",QString::number( raw_wtw_R));
        query.bindValue(":r_ad", QString::number(raw_pupd_R));
        query.bindValue(":r_vt", QString::number(raw_ALCR_R));
        query.bindValue(":r_r1", QString::number(rawR1_R));
        query.bindValue(":r_r2", QString::number(rawR2_R));
        query.bindValue(":r_ax", QString::number(rawAX_R));
        query.bindValue(":r_avg", QString::number(rawAvg_R));
        query.bindValue(":r_s", QString::number(rawS_R));
        query.bindValue(":r_c", QString::number(rawC_R));
        query.bindValue(":r_a", QString::number(rawA_R));
        query.bindValue(":r_k1", QString::number(rawK1_R));
        query.bindValue(":r_k2", QString::number(rawK2_R));

        query.bindValue(":vd", QString::number(currentVdIndex));
        query.bindValue(":pd", QString::number(ChuTongJu));
        query.bindValue(":cyl", QString::number(currentcylIndex));
        query.bindValue(":times", dateTimeStr);

        if (!query.exec()) {
            qDebug() << "insert error : " << query.lastError().text();
        }else
        {
            qDebug() << "insert success";
            // 打印所有数据库插入参数
            qDebug() << "=== 数据库插入参数列表 ===";
            qDebug() << "s_uuidserial=" << s_uuidserial;
            qDebug() << "左眼参数:";
            qDebug() << "  l_al=" << raw_AL_L << ", l_cct=" << raw_wtw_L << ", l_ad=" << raw_pupd_L << ", l_vt=" << raw_ALCR_L;
            qDebug() << "  l_r1=" << rawR1_L << ", l_r2=" << rawR2_L << ", l_ax=" << rawAX_L << ", l_avg=" << rawAvg_L;
            qDebug() << "  l_s=" << rawS_L << ", l_c=" << rawC_L << ", l_a=" << rawA_L;
            qDebug() << "  l_k1=" << rawK1_L << ", l_k2=" << rawK2_L;
            qDebug() << "右眼参数:";
            qDebug() << "  r_al=" << raw_AL_R << ", r_cct=" << raw_wtw_R << ", r_ad=" << raw_pupd_R << ", r_vt=" << raw_ALCR_R;
            qDebug() << "  r_r1=" << rawR1_R << ", r_r2=" << rawR2_R << ", r_ax=" << rawAX_R << ", r_avg=" << rawAvg_R;
            qDebug() << "  r_s=" << rawS_R << ", r_c=" << rawC_R << ", r_a=" << rawA_R ;
            qDebug() << "  r_k1=" << rawK1_R << ", r_k2=" << rawK2_R ;
            qDebug() << "  currentVdIndex=" << currentVdIndex << ", currentcylIndex=" << ChuTongJu <<" ,currentcylIndex=" << currentcylIndex ;
        }
        db.close();
    }
    else
    {
        qDebug() << "database open error!";
    }
    // 打印所有数据库插入参数
    qDebug() << "=== 数据库插入参数列表 ===";
    qDebug() << "s_uuidserial=" << s_uuidserial;
    qDebug() << "左眼参数:";
    qDebug() << "  l_al=" << raw_AL_L << ", l_cct=" << raw_wtw_L << ", l_ad=" << raw_pupd_L << ", l_vt=" << raw_ALCR_L;
    qDebug() << "  l_r1=" << rawR1_L << ", l_r2=" << rawR2_L << ", l_ax=" << rawAX_L << ", l_avg=" << rawAvg_L;
    qDebug() << "  l_s=" << rawS_L << ", l_c=" << rawC_L << ", l_a=" << rawA_L;
    qDebug() << "右眼参数:";
    qDebug() << "  r_al=" << raw_AL_R << ", r_cct=" << raw_wtw_R << ", r_ad=" << raw_pupd_R << ", r_vt=" << raw_ALCR_R;
    qDebug() << "  r_r1=" << rawR1_R << ", r_r2=" << rawR2_R << ", r_ax=" << rawAX_R << ", r_avg=" << rawAvg_R;
    qDebug() << "  r_s=" << rawS_R << ", r_c=" << rawC_R << ", r_a=" << rawA_R;


    // fyb0123检查当前眼别状态开始测量时立即更新计数
    if (m_currentEyeType == 0) {
        qDebug() << "没有眼睛啊！！！";
        return;
    }else if (m_currentEyeType == 2) {
        // 左眼：更新label_11,label_12,label_13（未到99则递增）
        if (m_leftCounter < 99) {
            m_leftCounter++;
            QString countStr = QString("%1").arg(m_leftCounter, 2, 10, QChar('0')); // 补前导零
            ui->label_11->setText(countStr);
            ui->label_12->setText("/ " + countStr);
            ui->label_13->setText("/ " + countStr);
            qDebug() << "左眼计数更新为:" << countStr;
        }
    } else if (m_currentEyeType == 1) {
        // 右眼：更新label_10,label_9,label_8（未到99则递增）
        if (m_rightCounter < 99) {
            m_rightCounter++;
            QString countStr = QString("%1").arg(m_rightCounter, 2, 10, QChar('0')); // 补前导零
            ui->label_10->setText(countStr);
            ui->label_9->setText("/ " + countStr);
            ui->label_8->setText("/ " + countStr);
            qDebug() << "右眼计数更新为:" << countStr;
        }
    }
    // 保存当前测量计数
    m_currentMeasurementCount = currentCount;
    m_maxMeasurementCount = maxCount;

    qDebug() << "=== 开始执行第" << currentCount << "次测量（共" << maxCount << "次）===";
    // 开始测量的命令，返回瞳距用的

    // 执行图像处理
    continueAfterMeasurementData();

    // 清除之前的测量数据，确保不会使用旧数据
    m_serialReceiver->resetMeasurementData();
    qDebug() << "已清除之前的测量数据，准备接收新数据";

    // 记录测量开始时间，用于验证数据时效性
    m_measurementStartTime = QDateTime::currentMSecsSinceEpoch();

    // 根据当前眼别类型发送相应的测量指令
    if (m_currentEyeType == 2) {

        qDebug() << "左眼=============================";
        m_serialReceiver->sendEyeControlCmd(true);
    } else if (m_currentEyeType == 1) {

        qDebug() << "右眼============================";
        m_serialReceiver->sendEyeControlCmd(false);
    }

    // 同步等待测量数据
    qDebug() << "开始同步等待测量数据...";
    int rl = 0, rs = 0, angle = 0;
    bool hasValidSerialData = false;

    // 等待测量数据返回（5秒超时）
    if (m_serialReceiver->waitForMeasurementData(rl, rs, angle, 5000)) {
        qDebug() << "成功接收到测量数据: rl=" << rl << ", rs=" << rs << ", angle=" << angle;
        hasValidSerialData = true;

        // 根据当前眼别类型更新对应的全局变量
        if (m_currentEyeType == 2) {
            // 左眼数据
            rawRl_L = rl;
            rawRs_L = rs;
            rawAngle_L = angle;
            qDebug() << "更新左眼测量数据";
        } else if (m_currentEyeType == 1) {
            // 右眼数据
            rawRl_R = rl;
            rawRs_R = rs;
            rawAngle_R = angle;
            qDebug() << "更新右眼测量数据";
        }
    } else {
        qDebug() << "测量数据接收超时，使用默认值";
        hasValidSerialData = false;

        // 使用默认值
        if (m_currentEyeType == 2) {
            rawRl_L = 208;
            rawRs_L = 208;
            rawAngle_L = 1;
            qDebug() << "使用左眼默认测试数据";
        } else if (m_currentEyeType == 1) {
            rawRl_R = 208;
            rawRs_R = 208;
            rawAngle_R = 1;
            qDebug() << "使用右眼默认测试数据";
        }
    }

    // 串口数据更新后，重新计算S、C、A值（基于最新的串口数据）
    recalcAndUpdate();
    updateALDataDisplay();
    // 更新到缓冲（顺序填充，满10行后移除第0行再追加到末尾）
    MeasurementData newData(
        /*k1_left*/  rawK1_L, /*k2_left*/ rawK2_L, /*ax_left*/ rawAX_L,
        /*k1_right*/ rawK1_R, /*k2_right*/ rawK2_R, /*ax_right*/ rawAX_R,
        /*r1_left*/  rawR1_L, /*r2_left*/  rawR2_L,
        /*r1_right*/ rawR1_R, /*r2_right*/ rawR2_R,
        /*d_left*/   rawD_L, /*c1_left*/   rawC1_L, /*ax_dca_left*/   rawAX_L,
        /*d_right*/  rawD_R, /*c1_right*/  rawC1_R, /*ax_dca_right*/  rawAX_R,
        /*raw_rl_left*/   rawRl_L, /*raw_rs_left*/   rawRs_L, /*raw_angle_left*/   rawAngle_L,
        /*raw_rl_right*/  rawRl_R, /*raw_rs_right*/  rawRs_R, /*raw_angle_right*/  rawAngle_R,
        /*s_left*/   rawS_L, /*c_left*/   rawC_L, /*a_left*/   rawA_L,
        /*s_right*/  rawS_R, /*c_right*/  rawC_R, /*a_right*/  rawA_R,
        /*al_left*/  raw_AL_L, /*wtw_left*/ raw_wtw_L, /*pupd_left*/ raw_pupd_L,
        /*al_right*/ raw_AL_R, /*wtw_right*/ raw_wtw_R, /*pupd_right*/ raw_pupd_R,
        /*eyeType*/  m_currentEyeType  // 添加眼别信息
    );

    // 更新到缓冲（顺序填充，满10行后移除第0行再追加到末尾）
    if (m_measureHistoryBuffer.size() < MAX_MEASURE_HISTORY) {
        m_measureHistoryBuffer.append(newData);
        qDebug() << "Buffered measurement row" << m_measureHistoryBuffer.size()-1;
    } else {
        m_measureHistoryBuffer.removeFirst();
        m_measureHistoryBuffer.append(newData);
        qDebug() << "Buffer rolled: dropped row 0, appended new row 9";
    }


    // 主界面数据完全更新后，更新测量界面（如果打开的话）
    if (m_measureWidget && m_measureWidget->isVisible()) {
        qDebug() << "=== 主界面数据更新完成，现在更新测量界面 ===";
        qDebug() << "当前眼别:" << m_currentEyeType << "，S、C、A值已完全计算";

        // 使用完全计算好的S、C、A值更新测量界面
        m_measureWidget->addMeasurementData(
            rawK1_L, rawK2_L, rawAX_L,
            rawK1_R, rawK2_R, rawAX_R,
            rawR1_L, rawR2_L,
            rawR1_R, rawR2_R,
            rawD_L, rawC1_L, rawAX_L,
            rawD_R, rawC1_R, rawAX_R,
            rawRl_L, rawRs_L, rawAngle_L,
            rawRl_R, rawRs_R, rawAngle_R,
            raw_AL_L, raw_wtw_L, raw_pupd_L,
            raw_AL_R, raw_wtw_R, raw_pupd_R,
            m_currentEyeType  // 添加眼别参数
        );
    }
    // 注意：测量界面的更新移到continueAfterMeasurementData函数中，
    // 确保在主界面完全更新完成后再更新测量界面，避免S、C、A参数数据错位

    // 检查是否需要继续测量
    if (currentCount < maxCount) {
        // 自动执行下一次测量
        qDebug() << "=== 第" << currentCount << "次测量完成，准备执行第" << (currentCount + 1) << "次测量 ===";

        recalcAndUpdate();
        updateALDataDisplay();

        // 添加短暂延迟后自动执行下一次测量
        QTimer::singleShot(500, this, [this, currentCount, maxCount]() {
            executeMeasurement(currentCount + 1, maxCount);
        });
    } else {
        // 所有测量完成
        startMeasure = false;
        focus_succ = false;
        qDebug() << "=== " << maxCount << "次测量全部完成！===";

        recalcAndUpdate();
        updateALDataDisplay();

        // ==========fyb0122检查是否是双眼模式的第一只眼测量完成 ==========
        if (m_isBothEyeAutoMeasuring && m_bothEyeCurrentEye == m_bothEyeFirstEye) {
            // 第一只眼测量完成，准备切换到对侧眼
            qDebug() << "=== 0121fyb 双眼模式：第一只眼" << m_bothEyeFirstEye
                     << "测量完成，准备切换到对侧眼 ===";

            // ========== 先停止当前测量用的电机，再发送移动指令 ==========
            // 停止电机（停止当前测量用的电机）
            qDebug() << "=== 第一只眼测量完成，开始停止电机 ===";
            m_serialReceiver->sendEyeSerialData(QByteArray::fromHex(""));// 电机停止命令
            m_motorRotationSent = false;
            hasExecutedLessThan20 = false;
            qDebug() << "停喽！！！";

            qDebug() << "=== 电机停止完成，准备发送移动指令 ===";

            // 发送电机移动指令到对侧眼（在停止电机后发送）
            int oppositeEyeType = (m_bothEyeFirstEye == 2) ? 1 : 2;  // 2=左,1=右
            QByteArray moveCmd;
            if (m_bothEyeFirstEye == 2) {
                // 当前为左眼，拉到右眼
                moveCmd = QByteArray::fromHex("");
                qDebug() << "0121fyb 双眼模式：左眼测完3次，发送拉到右眼";
            } else if (m_bothEyeFirstEye == 1) {
                // 当前为右眼，拉到左眼
                moveCmd = QByteArray::fromHex("");
                qDebug() << "0121fyb 双眼模式：右眼测完3次，发送拉到左眼";
            }
            if (!moveCmd.isEmpty()) {
                m_serialReceiver->sendEyeSerialData(moveCmd);
                m_bothEyeMotorMoving = true;//wyy0608
                m_waitingForMotorConfirm.store(true);//wyy0630
            }

            // 等待电机移动完成后切换到对侧眼并开始测量
            QTimer::singleShot(3200, this, [this, oppositeEyeType]() {
                qDebug() << "=== 0121fyb 电机移动完成，切换到对侧眼别" << oppositeEyeType
                         << "开始3次测量 ===";

                // 切换到对侧眼
                m_currentEyeType = oppositeEyeType;
                m_bothEyeCurrentEye = oppositeEyeType;  // 更新当前测量的眼别

                // 重置测量标志位（为对侧眼测量做准备）
                startMeasure = false;
                focus_succ = false;
                hasExecutedLessThan100 = false;
                hasExecutedLessThan20 = false;
                m_motorRotationSent = false;
                AL = 0;
                focus_succ = false;

                // 重新启动图像处理定时器，等待自动找眼成功后通过on_start_clicked触发测量
                // 不立即调用executeMeasurement，避免与自动找眼触发的on_start_clicked冲突
                m_imageProcessTimer->start(20);
                qDebug() << "0121fyb 双眼模式：对侧眼图像处理定时器已启动，等待自动找眼或用户点击start按钮";
            });

        } else if (m_isBothEyeAutoMeasuring && m_bothEyeCurrentEye != m_bothEyeFirstEye) {
            qDebug() << "=== 0121fyb 双眼模式：对侧眼" << m_bothEyeCurrentEye
                     << "测量完成，双眼测量全部结束 ===";
            m_isBothEyeAutoMeasuring = false;
            m_bothEyeCurrentEye = 0;
            m_bothEyeFirstEye = 0;
            m_bothEyeMotorMoving = false;//wyy0608

            // 测量完成后停止电机
            stopMotorAfterMeasurement();
        } else {
            // 单眼模式或已有数据的测量，正常停止电机
            stopMotorAfterMeasurement();
        }
    }

}

void MainWindow::stopMotorAfterMeasurement() {
    qDebug() << "=== 测量完成，开始停止电机 ===";

    // 发送串口数据
    QByteArray motorStopCmd = QByteArray::fromHex("");
    m_serialReceiver->sendfocusSerialData(motorStopCmd);// 电机停止命令wyy0701
//    hasExecutedLessThan20 = false;
    qDebug() << "停喽！！！";
    qDebug() << "=== 电机停止完成 ===";

    // 使用非阻塞延迟重置标志位和重启定时器
    QTimer::singleShot(3000, this, [this]() {
        // 重置所有测量相关标志位
        startMeasure = false;
        focus_succ = false;
        m_motorRotationSent = false;
        m_preparationDone = false;//wyy0701
        AL = 0;
        focus_succ = false;

        // 重新启动图像处理定时器
        m_imageProcessTimer->start(20);
        qDebug() << "=== 标志位重置完成，图像处理定时器已重启 ===";
    });
}


void MainWindow::continueAfterMeasurementData() {
    qDebug() << "=== 继续执行后续处理 ===";

    // 显示当前眼别状态
    QString eyeStatus = (m_currentEyeType == 0) ? "NONE" :
                       (m_currentEyeType == 2) ? "left" : "right";

    QImage eyeImage = image_nuc.copy();//ZQ20260202


    if (!eyeImage.isNull()) {
        // 使用电机判断眼别
        int currentEyeType = getCurrentEyeTypeFromMotor();
        QString eyeType;
        if (currentEyeType == 2) {
            eyeType = "left";
        } else if (currentEyeType == 1) {
            eyeType = "right";
        } else {
            eyeType = "unknown";
        }

        // 检查眼别数据是否有效
        if (eyeType == "unknown") {
            qDebug() << "=== 眼别数据无效，跳过图像处理 ===";

        } else {
            // 只有知道眼别时才进行图像处理
            float frl, frs, fangle, R1, R2, AX, K1, K2, D, C1, inner_radius, outer_radius, center_x, center_y;

            if (calculateEllipseParametersFromQImage(eyeImage, feilingchutongju, frl, frs, fangle, R1, R2, AX, K1, K2, D, C1, inner_radius = 0.0, outer_radius, center_x = 640, center_y, eyeType)) {
                // 打印图片计算的是哪只眼睛
                qDebug() << "=== 图像处理成功 ===";
                qDebug() << "眼别类型:" << eyeType;

                // 打印计算出来的参数
                qDebug() << "=== 椭圆参数计算结果 ===";
                qDebug() << "R1:" << R1 << " R2:" << R2 << " AX:" << AX  << "K1:" << K1 << " K2:" << K2 << " D:" << D << " C:" << C1;
                qDebug() << "内环半径:" << inner_radius << " 外环半径:" << outer_radius;
                qDebug() << "圆心坐标: X=" << center_x << " Y=" << center_y;

                if (eyeType == "left") {
                    rawR1_L = R1; rawR2_L = R2; rawAX_L = AX; rawK1_L = K1; rawK2_L = K2; rawD_L = D; rawC1_L = C1;
                } else {
                    rawR1_R = R1; rawR2_R = R2; rawAX_R = AX; rawK1_R = K1; rawK2_R = K2; rawD_R = D; rawC1_R = C1;
                }
            }

            // 调用WTW计算函数，传入内环半径和圆心X坐标
            WtwResult wtwResult = WtwProcessor::processImage(eyeImage, inner_radius, center_x);
            if (wtwResult.valid) {
                if (eyeType == "left") {
                    raw_wtw_L = wtwResult.wtw; raw_pupd_L = wtwResult.puiLD;
                    raw_AL_L=AL;raw_ALCR_L=AL / (rawR1_L + rawR2_L);
                    qDebug() << "raw_pupd_L:" << wtwResult.puiLD;
                    qDebug() << "raw_wtw_L:" << wtwResult.wtw;
                }else{
                    // 达到99时不再增加
                    raw_wtw_R = wtwResult.wtw; raw_pupd_R = wtwResult.puiLD;
                    raw_AL_R=AL;raw_ALCR_R=AL / (rawR1_R + rawR2_R);
                }
            } else {
                qDebug() << "WTW 计算失败！";
            }

        }
    }

    recalcAndUpdate();
    updateALDataDisplay();

    // 点击测量start时，显示当前瞳距数据
    updatetongjuDataDisplay();
   }

void MainWindow::on_startMeasureBtn_clicked()
{
    // 用户确认，执行原有逻辑
    // ---------------------- 原有cancel函数内容 ----------------------
    // 重置查表数据
    rawRl_R = rawRs_R = rawRl_L = rawRs_L = 208; // 使S、C为0
    rawAngle_R = rawAngle_L = 1;                // 使A为空白
    rawS_R = rawC_R = rawA_R = 0;
    rawS_L = rawC_L = rawA_L = 0;
    // 重置图像处理数据
    rawR1_R = rawR2_R = rawAX_R = rawK1_R = rawK2_R = rawD_R = rawC1_R = 0;
    rawR1_L = rawR2_L = rawAX_L = rawK1_L = rawK2_L = rawD_L = rawC1_L = 0;
    raw_wtw_L = raw_pupd_L = 0;
    raw_wtw_R= raw_pupd_R = 0;
    raw_AL_L=raw_ALCR_L=0;
    raw_AL_R=raw_ALCR_R=0;
    m_currentInterpupillaryDistance=0;

    // 重置全局变量到默认状态
    g_vars.reset();
    // 重新计算并更新显示（S/C/A也会清零）
    recalcAndUpdate();
    updatetongjuDataDisplay();
    // 0830重置左眼计数器和标签
    m_leftCounter = 0; // 假设之前已定义m_leftCount
    ui->label_11->setText("00");
    ui->label_12->setText("/ 00");
    ui->label_13->setText("/ 00");

    // 重置右眼计数器和标签
    m_rightCounter = 0; // 假设之前已定义m_rightCount
    ui->label_10->setText("00");
    ui->label_9->setText("/ 00");
    ui->label_8->setText("/ 00");//0208xin
    m_measureHistoryBuffer.clear();

    ui->stackedWidget_8->setCurrentWidget(ui->page);
    //界面初始化1203fyb
    initalrefrraControlriginalPositions();
    initLabelsVisibility();//1201fyb

    // 创建定时器
    if (!m_imageProcessTimer) {
        m_imageProcessTimer = new QTimer(this);
        connect(m_imageProcessTimer, &QTimer::timeout, this, &MainWindow::onImageProcessTimerTimeout);
    }
    // 初始化图像处理处理标志位
    m_isImageProcessing = false;
    // 初始化距离标志位
    hasExecutedLessThan100 = false;
    hasExecutedLessThan20 = false;
    // 初始化USB状态标志位
    m_isUsbReceiving = false;
    // 初始化电机旋转标志位
    m_motorRotationSent = false;
    // 初始化开始测量标志位
    startMeasure = false;
    focus_succ = false;
    // 初始化激光状态标志位
    m_laserOn = false;
    // 初始化电机移动确认标志位
    m_waitingForMotorConfirm.store(false);//0701fyb
    // wyy0520：0x61 瞳距调整相关标志初始化
    m_waitingForPd61Move = false;
    m_pupilXYAligned = false;
   // resetAutoAlignGateFlags(); // wyy260520
    //0701fyb激光旋转电机总控制
    m_preparationDone=false;
    // 初始化自动测量标志位、
    m_auto = true;
//    zok = false;//wyy0701
    // 设置初始按钮图片（m_auto为true时显示control0.png）
    ui->control1->setStyleSheet("border-image: url(:/img/control0.png); background-color: transparent; border: none;");
    // 启动定时器，每500毫秒执行一次
    m_imageProcessTimer->start(20);
    qDebug() << "图像处理定时器启动";
}


void MainWindow::on_start_clicked()
{
    //// ===================================== 数据处理 =====================================
    if (startMeasure){
        // 首先恢复所有数据到原始位置，确保界面布局正确
        resetControlsToOriginalPosition();
        // 根据保存的选择状态更新界面显示
            bool kerOnlyMode = (m_kerSelected && !m_alSelected && !m_refSelected);
            bool alOnlyMode = (!m_kerSelected && m_alSelected && !m_refSelected);
            bool m_refOnlyMode = (!m_kerSelected && !m_alSelected && m_refSelected);
            bool refKerMode = (m_kerSelected && !m_alSelected && m_refSelected);
            bool alRefMode = (!m_kerSelected && m_alSelected && m_refSelected);
            bool alKerMode = (m_kerSelected && m_alSelected && !m_refSelected);
            bool allThreeMode = (m_kerSelected && m_alSelected && m_refSelected);
            bool allnoselectMode = (!m_kerSelected && !m_alSelected && !m_refSelected);

            // 新增：根据选择模式更新界面显示
            if (m_refOnlyMode) {
                // 只选择REF：显示SCA数据,REF移动到AL位置
                qDebug() << "调用moveRefToAlPositions()处理仅REF模式";
                moveRefToAlPositions();
            } else if (kerOnlyMode) {
                // 只选择KER：根据设置界面选择显示对应数据
                moveKerToALPosition();
            } else if (alOnlyMode) {
                // 只选择AL：显示AL、WTW、PUPD、AL/CR数据，并更新Al数据
                updateALDataDisplay();
            } else if (refKerMode) {
                // 同时选择REF和KER：显示SCA + 设置界面选择的数据
                moveRefToAlPositions();//将SCA移动到AL位置
                moveKerParamsToSCAPositions();//根据设置界面选择的KER数据，将其移动到SCA处
            } else if (alRefMode) {
                // 同时选择AL和REF：显示AL、WTW、PUPD、AL/CR + SCA数据，更新AL数据
                updateALDataDisplay();
            }
            else if (alKerMode) {
                // 同时选择AL和KER：显示AL、WTW、PUPD、AL/CR + 设置界面选择的数据
                movekerTorefposition();
                updateALDataDisplay();
            } else if (allThreeMode) {
                // 三个按钮都选择：显示AL、WTW、PUPD、AL/CR + SCA + 设置界面选择的数据
                moveKerControlsToRRAPositions();
                updateALDataDisplay();
            } else if(allnoselectMode){
                // 1201+FYB 新增：当三个按钮都不选时，显示AL + REF + 对应模式的KER数据
                // 设置选择状态：三个都选，显示所有数据
                m_kerSelected = true;
                m_alSelected = true;
                m_refSelected = true;
                // 移动KER控件到RRA原始位置（根据模式显示K1/K2/AX或D/C/A）
                moveKerControlsToRRAPositions();
                // 更新AL数据显示
                updateALDataDisplay();
                qDebug() << "All buttons unselected mode: show AL + REF + KER data (mode" << m_cornealMode << ")";
            }
            updateLabelsVisibility(m_kerSelected, m_alSelected, m_refSelected);



         // 获取当前眼别的数据数量
         int currentEyeDataCount = getCurrentEyeDataCount(m_currentEyeType);
         // 读取配置，判断是否为双眼模式
         QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
         QSettings settings(configPath, QSettings::IniFormat);
         bool isBothEyeMode = settings.value("yanceliangmoshi/shuang", true).toBool(); // 默认true，即双眼模式

         if (currentEyeDataCount == 0) {
             m_imageProcessTimer->stop();//

             // ========== 检查是否是双眼模式的对侧眼测量 ==========
             if (m_isBothEyeAutoMeasuring && m_bothEyeCurrentEye != m_bothEyeFirstEye) {
                 // 对侧眼测量：只执行3次测量，不再进入双眼模式
                 qDebug() << " 双眼模式：对侧眼" << m_currentEyeType
                          << "没有数据，开始3次测量（不进入双眼模式） ===";
                 executeMeasurement(1, 3); // 对侧眼3次测量
             } else if (!isBothEyeMode) {
                 // 当前眼别没有数据：自动执行3次测量（原逻辑不变）
                 qDebug() << " 单眼模式：当前眼别" << m_currentEyeType
                          << "没有数据，开始自动执行3次测量 ===";
                 executeMeasurement(1, 3); // 从第1次开始，总共3次
             }else{ //  双眼模式（第一只眼）
                 qDebug() << "双眼模式：当前眼别" << m_currentEyeType
                          << "没有数据，开始第一只眼3次测量 ===";

                 // ==========设置双眼模式标志位，保存第一只眼信息 ==========
                 m_isBothEyeAutoMeasuring = true;  // 标记正在进行双眼自动测量
                 m_bothEyeFirstEye = m_currentEyeType;  // 保存第一只眼类型
                 m_bothEyeCurrentEye = m_currentEyeType;  // 当前测量的眼别

                 // 执行第一只眼3次测量（测量完成后会在executeMeasurement中处理对侧眼切换）
                 executeMeasurement(1, 3); // 第一只眼3次
             }
         }else {
             m_imageProcessTimer->stop();//
             // 当前眼别已有数据：只执行1次测量
             qDebug() << "=== 当前眼别" << m_currentEyeType << "已有" << currentEyeDataCount << "条数据，开始执行1次测量 ===";
             executeMeasurement(1, 1); // 从第1次开始，总共1次
         }
    }
}

void MainWindow::executeMeasurement(int currentCount, int maxCount) {
    QString dbName="nuc_total.db";
    QFileInfo info(dbName);
    QString dbFilePath = info.absoluteFilePath();

    QSqlDatabase db;
    // 核心：先判断默认连接是否已存在，避免重复添加
    QString defaultConnName = "qt_sql_default_connection";
    if (QSqlDatabase::contains(defaultConnName)) {
        // 连接已存在，直接从连接池中获取该连接
        db = QSqlDatabase::database(defaultConnName);
        // 额外优化：检查获取的连接是否有效，若无效则重新配置
        if (!db.isValid() || db.databaseName() != dbFilePath) {
            db = QSqlDatabase::addDatabase("QSQLITE", defaultConnName);
            db.setDatabaseName(dbFilePath);
        }
    } else {
        // 连接不存在，才添加新连接
        db = QSqlDatabase::addDatabase("QSQLITE", defaultConnName);
        db.setDatabaseName(dbFilePath);
    }


    bool isOpen=db.open();
    if(isOpen)
    {
        QSqlQuery query(db);
        QString sql_insert = QString("INSERT INTO %1 "
                "(l_al, l_cct, l_ad, l_vt, l_r1, l_r2, l_ax, l_avg, l_s, l_c, l_a, l_k1, l_k2, r_al, r_cct, r_ad, r_vt, r_r1, r_r2, r_ax, r_avg, r_s, r_c, r_a, r_k1, r_k2, vd, pd, cyl, times) "
                "VALUES (:l_al, :l_cct, :l_ad, :l_vt, :l_r1, :l_r2, :l_ax, :l_avg, :l_s, :l_c, :l_a, :l_k1, :l_k2, :r_al, :r_cct, :r_ad, :r_vt, :r_r1, :r_r2, :r_ax, :r_avg, :r_s, :r_c, :r_a, :r_k1, :r_k2, :vd, :pd, :cyl,:times)")
                .arg("tabel_" + s_uuidserial);

        // 获取当前时间戳（作为主键，保证唯一性）
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString dateTimeStr = currentDateTime.toString("yyyy-MM-dd-HH-mm-ss-zzz");
        qDebug() << "dateTimeStr :"<< dateTimeStr;

        query.prepare(sql_insert);
        query.bindValue(":l_al", QString::number(raw_AL_L));
        query.bindValue(":l_cct",  QString::number(raw_wtw_L));              // 绑定密码变量
        query.bindValue(":l_ad",QString::number(raw_pupd_L));        // 绑定电话变量
        query.bindValue(":l_vt", QString::number(raw_ALCR_L));         // 绑定生日变量
        query.bindValue(":l_r1",QString::number(rawR1_L));
        query.bindValue(":l_r2",QString::number( rawR2_L));
        query.bindValue(":l_ax", QString::number(rawAX_L));
        query.bindValue(":l_avg", QString::number(rawAvg_L));
        query.bindValue(":l_s", QString::number(rawS_L));
        query.bindValue(":l_c", QString::number(rawC_L));
        query.bindValue(":l_a", QString::number(rawA_L));
        query.bindValue(":l_k1", QString::number(rawK1_L));
        query.bindValue(":l_k2", QString::number(rawK2_L));

        query.bindValue(":r_al", QString::number(raw_AL_R));
        query.bindValue(":r_cct",QString::number( raw_wtw_R));
        query.bindValue(":r_ad", QString::number(raw_pupd_R));
        query.bindValue(":r_vt", QString::number(raw_ALCR_R));
        query.bindValue(":r_r1", QString::number(rawR1_R));
        query.bindValue(":r_r2", QString::number(rawR2_R));
        query.bindValue(":r_ax", QString::number(rawAX_R));
        query.bindValue(":r_avg", QString::number(rawAvg_R));
        query.bindValue(":r_s", QString::number(rawS_R));
        query.bindValue(":r_c", QString::number(rawC_R));
        query.bindValue(":r_a", QString::number(rawA_R));
        query.bindValue(":r_k1", QString::number(rawK1_R));
        query.bindValue(":r_k2", QString::number(rawK2_R));

        query.bindValue(":vd", QString::number(currentVdIndex));
        query.bindValue(":pd", QString::number(ChuTongJu));
        query.bindValue(":cyl", QString::number(currentcylIndex));
        query.bindValue(":times", dateTimeStr);

        if (!query.exec()) {
            qDebug() << "insert error : " << query.lastError().text();
        }else
        {
            qDebug() << "insert success";
            // 打印所有数据库插入参数
            qDebug() << "=== 数据库插入参数列表 ===";
            qDebug() << "s_uuidserial=" << s_uuidserial;
            qDebug() << "左眼参数:";
            qDebug() << "  l_al=" << raw_AL_L << ", l_cct=" << raw_wtw_L << ", l_ad=" << raw_pupd_L << ", l_vt=" << raw_ALCR_L;
            qDebug() << "  l_r1=" << rawR1_L << ", l_r2=" << rawR2_L << ", l_ax=" << rawAX_L << ", l_avg=" << rawAvg_L;
            qDebug() << "  l_s=" << rawS_L << ", l_c=" << rawC_L << ", l_a=" << rawA_L;
            qDebug() << "  l_k1=" << rawK1_L << ", l_k2=" << rawK2_L;
            qDebug() << "右眼参数:";
            qDebug() << "  r_al=" << raw_AL_R << ", r_cct=" << raw_wtw_R << ", r_ad=" << raw_pupd_R << ", r_vt=" << raw_ALCR_R;
            qDebug() << "  r_r1=" << rawR1_R << ", r_r2=" << rawR2_R << ", r_ax=" << rawAX_R << ", r_avg=" << rawAvg_R;
            qDebug() << "  r_s=" << rawS_R << ", r_c=" << rawC_R << ", r_a=" << rawA_R ;
            qDebug() << "  r_k1=" << rawK1_R << ", r_k2=" << rawK2_R ;
            qDebug() << "  currentVdIndex=" << currentVdIndex << ", currentcylIndex=" << ChuTongJu <<" ,currentcylIndex=" << currentcylIndex ;
        }
        db.close();
    }
    else
    {
        qDebug() << "database open error!";
    }
    // 打印所有数据库插入参数
    qDebug() << "=== 数据库插入参数列表 ===";
    qDebug() << "s_uuidserial=" << s_uuidserial;
    qDebug() << "左眼参数:";
    qDebug() << "  l_al=" << raw_AL_L << ", l_cct=" << raw_wtw_L << ", l_ad=" << raw_pupd_L << ", l_vt=" << raw_ALCR_L;
    qDebug() << "  l_r1=" << rawR1_L << ", l_r2=" << rawR2_L << ", l_ax=" << rawAX_L << ", l_avg=" << rawAvg_L;
    qDebug() << "  l_s=" << rawS_L << ", l_c=" << rawC_L << ", l_a=" << rawA_L;
    qDebug() << "右眼参数:";
    qDebug() << "  r_al=" << raw_AL_R << ", r_cct=" << raw_wtw_R << ", r_ad=" << raw_pupd_R << ", r_vt=" << raw_ALCR_R;
    qDebug() << "  r_r1=" << rawR1_R << ", r_r2=" << rawR2_R << ", r_ax=" << rawAX_R << ", r_avg=" << rawAvg_R;
    qDebug() << "  r_s=" << rawS_R << ", r_c=" << rawC_R << ", r_a=" << rawA_R;


    // fyb0123检查当前眼别状态开始测量时立即更新计数
    if (m_currentEyeType == 0) {
        qDebug() << "没有眼睛啊！！！";
        return;
    }else if (m_currentEyeType == 2) {
        // 左眼：更新label_11,label_12,label_13（未到99则递增）
        if (m_leftCounter < 99) {
            m_leftCounter++;
            QString countStr = QString("%1").arg(m_leftCounter, 2, 10, QChar('0')); // 补前导零
            ui->label_11->setText(countStr);
            ui->label_12->setText("/ " + countStr);
            ui->label_13->setText("/ " + countStr);
            qDebug() << "左眼计数更新为:" << countStr;
        }
    } else if (m_currentEyeType == 1) {
        // 右眼：更新label_10,label_9,label_8（未到99则递增）
        if (m_rightCounter < 99) {
            m_rightCounter++;
            QString countStr = QString("%1").arg(m_rightCounter, 2, 10, QChar('0')); // 补前导零
            ui->label_10->setText(countStr);
            ui->label_9->setText("/ " + countStr);
            ui->label_8->setText("/ " + countStr);
            qDebug() << "右眼计数更新为:" << countStr;
        }
    }
    // 保存当前测量计数
    m_currentMeasurementCount = currentCount;
    m_maxMeasurementCount = maxCount;

    qDebug() << "=== 开始执行第" << currentCount << "次测量（共" << maxCount << "次）===";
    // 开始测量的命令，返回瞳距用的

    // 执行图像处理
    continueAfterMeasurementData();

    // 清除之前的测量数据，确保不会使用旧数据
    m_serialReceiver->resetMeasurementData();
    qDebug() << "已清除之前的测量数据，准备接收新数据";

    // 记录测量开始时间，用于验证数据时效性
    m_measurementStartTime = QDateTime::currentMSecsSinceEpoch();

    // 根据当前眼别类型发送相应的测量指令
    if (m_currentEyeType == 2) {

        qDebug() << "左眼=============================";
        m_serialReceiver->sendEyeControlCmd(true);
    } else if (m_currentEyeType == 1) {

        qDebug() << "右眼============================";
        m_serialReceiver->sendEyeControlCmd(false);
    }

    // 同步等待测量数据
    qDebug() << "开始同步等待测量数据...";
    int rl = 0, rs = 0, angle = 0;
    bool hasValidSerialData = false;

    // 等待测量数据返回（5秒超时）
    if (m_serialReceiver->waitForMeasurementData(rl, rs, angle, 5000)) {
        qDebug() << "成功接收到测量数据: rl=" << rl << ", rs=" << rs << ", angle=" << angle;
        hasValidSerialData = true;

        // 根据当前眼别类型更新对应的全局变量
        if (m_currentEyeType == 2) {
            // 左眼数据
            rawRl_L = rl;
            rawRs_L = rs;
            rawAngle_L = angle;
            qDebug() << "更新左眼测量数据";
        } else if (m_currentEyeType == 1) {
            // 右眼数据
            rawRl_R = rl;
            rawRs_R = rs;
            rawAngle_R = angle;
            qDebug() << "更新右眼测量数据";
        }
    } else {
        qDebug() << "测量数据接收超时，使用默认值";
        hasValidSerialData = false;

        // 使用默认值
        if (m_currentEyeType == 2) {
            rawRl_L = 208;
            rawRs_L = 208;
            rawAngle_L = 1;
            qDebug() << "使用左眼默认测试数据";
        } else if (m_currentEyeType == 1) {
            rawRl_R = 208;
            rawRs_R = 208;
            rawAngle_R = 1;
            qDebug() << "使用右眼默认测试数据";
        }
    }

    // 串口数据更新后，重新计算S、C、A值（基于最新的串口数据）
    recalcAndUpdate();
    updateALDataDisplay();
    // 更新到缓冲（顺序填充，满10行后移除第0行再追加到末尾）
    MeasurementData newData(
        /*k1_left*/  rawK1_L, /*k2_left*/ rawK2_L, /*ax_left*/ rawAX_L,
        /*k1_right*/ rawK1_R, /*k2_right*/ rawK2_R, /*ax_right*/ rawAX_R,
        /*r1_left*/  rawR1_L, /*r2_left*/  rawR2_L,
        /*r1_right*/ rawR1_R, /*r2_right*/ rawR2_R,
        /*d_left*/   rawD_L, /*c1_left*/   rawC1_L, /*ax_dca_left*/   rawAX_L,
        /*d_right*/  rawD_R, /*c1_right*/  rawC1_R, /*ax_dca_right*/  rawAX_R,
        /*raw_rl_left*/   rawRl_L, /*raw_rs_left*/   rawRs_L, /*raw_angle_left*/   rawAngle_L,
        /*raw_rl_right*/  rawRl_R, /*raw_rs_right*/  rawRs_R, /*raw_angle_right*/  rawAngle_R,
        /*s_left*/   rawS_L, /*c_left*/   rawC_L, /*a_left*/   rawA_L,
        /*s_right*/  rawS_R, /*c_right*/  rawC_R, /*a_right*/  rawA_R,
        /*al_left*/  raw_AL_L, /*wtw_left*/ raw_wtw_L, /*pupd_left*/ raw_pupd_L,
        /*al_right*/ raw_AL_R, /*wtw_right*/ raw_wtw_R, /*pupd_right*/ raw_pupd_R,
        /*eyeType*/  m_currentEyeType  // 添加眼别信息
    );

    // 更新到缓冲（顺序填充，满10行后移除第0行再追加到末尾）
    if (m_measureHistoryBuffer.size() < MAX_MEASURE_HISTORY) {
        m_measureHistoryBuffer.append(newData);
        qDebug() << "Buffered measurement row" << m_measureHistoryBuffer.size()-1;
    } else {
        m_measureHistoryBuffer.removeFirst();
        m_measureHistoryBuffer.append(newData);
        qDebug() << "Buffer rolled: dropped row 0, appended new row 9";
    }


    // 主界面数据完全更新后，更新测量界面（如果打开的话）
    if (m_measureWidget && m_measureWidget->isVisible()) {
        qDebug() << "=== 主界面数据更新完成，现在更新测量界面 ===";
        qDebug() << "当前眼别:" << m_currentEyeType << "，S、C、A值已完全计算";

        // 使用完全计算好的S、C、A值更新测量界面
        m_measureWidget->addMeasurementData(
            rawK1_L, rawK2_L, rawAX_L,
            rawK1_R, rawK2_R, rawAX_R,
            rawR1_L, rawR2_L,
            rawR1_R, rawR2_R,
            rawD_L, rawC1_L, rawAX_L,
            rawD_R, rawC1_R, rawAX_R,
            rawRl_L, rawRs_L, rawAngle_L,
            rawRl_R, rawRs_R, rawAngle_R,
            raw_AL_L, raw_wtw_L, raw_pupd_L,
            raw_AL_R, raw_wtw_R, raw_pupd_R,
            m_currentEyeType  // 添加眼别参数
        );
    }
    // 注意：测量界面的更新移到continueAfterMeasurementData函数中，
    // 确保在主界面完全更新完成后再更新测量界面，避免S、C、A参数数据错位

    // 检查是否需要继续测量
    if (currentCount < maxCount) {
        // 自动执行下一次测量
        qDebug() << "=== 第" << currentCount << "次测量完成，准备执行第" << (currentCount + 1) << "次测量 ===";

        recalcAndUpdate();
        updateALDataDisplay();

        // 添加短暂延迟后自动执行下一次测量
        QTimer::singleShot(500, this, [this, currentCount, maxCount]() {
            executeMeasurement(currentCount + 1, maxCount);
        });
    } else {
        // 所有测量完成
        startMeasure = false;
        focus_succ = false;
        qDebug() << "=== " << maxCount << "次测量全部完成！===";

        recalcAndUpdate();
        updateALDataDisplay();

        // ==========fyb0122检查是否是双眼模式的第一只眼测量完成 ==========
        if (m_isBothEyeAutoMeasuring && m_bothEyeCurrentEye == m_bothEyeFirstEye) {
            // 第一只眼测量完成，准备切换到对侧眼
            qDebug() << "=== 0121fyb 双眼模式：第一只眼" << m_bothEyeFirstEye
                     << "测量完成，准备切换到对侧眼 ===";

            // ========== 先停止当前测量用的电机，再发送移动指令 ==========
            // 停止电机（停止当前测量用的电机）
            qDebug() << "=== 第一只眼测量完成，开始停止电机 ===";
            m_serialReceiver->sendEyeSerialData(QByteArray::fromHex(""));// 电机停止命令
            m_motorRotationSent = false;
            hasExecutedLessThan20 = false;
            qDebug() << "停喽！！！";

            qDebug() << "=== 电机停止完成，准备发送移动指令 ===";

            // 发送电机移动指令到对侧眼（在停止电机后发送）
            int oppositeEyeType = (m_bothEyeFirstEye == 2) ? 1 : 2;  // 2=左,1=右
            QByteArray moveCmd;
            if (m_bothEyeFirstEye == 2) {
                // 当前为左眼，拉到右眼
                moveCmd = QByteArray::fromHex("");
                qDebug() << "0121fyb 双眼模式：左眼测完3次，发送拉到右眼";
            } else if (m_bothEyeFirstEye == 1) {
                // 当前为右眼，拉到左眼
                moveCmd = QByteArray::fromHex("");
                qDebug() << "0121fyb 双眼模式：右眼测完3次，发送拉到左眼";
            }
            if (!moveCmd.isEmpty()) {
                m_serialReceiver->sendEyeSerialData(moveCmd);
                m_bothEyeMotorMoving = true;//wyy0608
                m_waitingForMotorConfirm.store(true);//wyy0630
            }

            // 等待电机移动完成后切换到对侧眼并开始测量
            QTimer::singleShot(3200, this, [this, oppositeEyeType]() {
                qDebug() << "=== 0121fyb 电机移动完成，切换到对侧眼别" << oppositeEyeType
                         << "开始3次测量 ===";

                // 切换到对侧眼
                m_currentEyeType = oppositeEyeType;
                m_bothEyeCurrentEye = oppositeEyeType;  // 更新当前测量的眼别

                // 重置测量标志位（为对侧眼测量做准备）
                startMeasure = false;
                focus_succ = false;
                hasExecutedLessThan100 = false;
                hasExecutedLessThan20 = false;
                m_motorRotationSent = false;
                AL = 0;
                focus_succ = false;

                // 重新启动图像处理定时器，等待自动找眼成功后通过on_start_clicked触发测量
                // 不立即调用executeMeasurement，避免与自动找眼触发的on_start_clicked冲突
                m_imageProcessTimer->start(20);
                qDebug() << "0121fyb 双眼模式：对侧眼图像处理定时器已启动，等待自动找眼或用户点击start按钮";
            });

        } else if (m_isBothEyeAutoMeasuring && m_bothEyeCurrentEye != m_bothEyeFirstEye) {
            qDebug() << "=== 0121fyb 双眼模式：对侧眼" << m_bothEyeCurrentEye
                     << "测量完成，双眼测量全部结束 ===";
            m_isBothEyeAutoMeasuring = false;
            m_bothEyeCurrentEye = 0;
            m_bothEyeFirstEye = 0;
            m_bothEyeMotorMoving = false;//wyy0608

            // 测量完成后停止电机
            stopMotorAfterMeasurement();
        } else {
            // 单眼模式或已有数据的测量，正常停止电机
            stopMotorAfterMeasurement();
        }
    }

}

void MainWindow::stopMotorAfterMeasurement() {
    qDebug() << "=== 测量完成，开始停止电机 ===";

    // 发送串口数据
    QByteArray motorStopCmd = QByteArray::fromHex("");
    m_serialReceiver->sendfocusSerialData(motorStopCmd);// 电机停止命令wyy0701
//    hasExecutedLessThan20 = false;
    qDebug() << "停喽！！！";
    qDebug() << "=== 电机停止完成 ===";

    // 使用非阻塞延迟重置标志位和重启定时器
    QTimer::singleShot(3000, this, [this]() {
        // 重置所有测量相关标志位
        startMeasure = false;
        focus_succ = false;
        m_motorRotationSent = false;
        m_preparationDone = false;//wyy0701
        AL = 0;
        focus_succ = false;

        // 重新启动图像处理定时器
        m_imageProcessTimer->start(20);
        qDebug() << "=== 标志位重置完成，图像处理定时器已重启 ===";
    });
}


void MainWindow::continueAfterMeasurementData() {
    qDebug() << "=== 继续执行后续处理 ===";

    // 显示当前眼别状态
    QString eyeStatus = (m_currentEyeType == 0) ? "NONE" :
                       (m_currentEyeType == 2) ? "left" : "right";

    QImage eyeImage = image_nuc.copy();//ZQ20260202


    if (!eyeImage.isNull()) {
        // 使用电机判断眼别
        int currentEyeType = getCurrentEyeTypeFromMotor();
        QString eyeType;
        if (currentEyeType == 2) {
            eyeType = "left";
        } else if (currentEyeType == 1) {
            eyeType = "right";
        } else {
            eyeType = "unknown";
        }

        // 检查眼别数据是否有效
        if (eyeType == "unknown") {
            qDebug() << "=== 眼别数据无效，跳过图像处理 ===";

        } else {
            // 只有知道眼别时才进行图像处理
            float frl, frs, fangle, R1, R2, AX, K1, K2, D, C1, inner_radius, outer_radius, center_x, center_y;

            if (calculateEllipseParametersFromQImage(eyeImage, feilingchutongju, frl, frs, fangle, R1, R2, AX, K1, K2, D, C1, inner_radius = 0.0, outer_radius, center_x = 640, center_y, eyeType)) {
                // 打印图片计算的是哪只眼睛
                qDebug() << "=== 图像处理成功 ===";
                qDebug() << "眼别类型:" << eyeType;

                // 打印计算出来的参数
                qDebug() << "=== 椭圆参数计算结果 ===";
                qDebug() << "R1:" << R1 << " R2:" << R2 << " AX:" << AX  << "K1:" << K1 << " K2:" << K2 << " D:" << D << " C:" << C1;
                qDebug() << "内环半径:" << inner_radius << " 外环半径:" << outer_radius;
                qDebug() << "圆心坐标: X=" << center_x << " Y=" << center_y;

                if (eyeType == "left") {
                    rawR1_L = R1; rawR2_L = R2; rawAX_L = AX; rawK1_L = K1; rawK2_L = K2; rawD_L = D; rawC1_L = C1;
                } else {
                    rawR1_R = R1; rawR2_R = R2; rawAX_R = AX; rawK1_R = K1; rawK2_R = K2; rawD_R = D; rawC1_R = C1;
                }
            }

            // 调用WTW计算函数，传入内环半径和圆心X坐标
            WtwResult wtwResult = WtwProcessor::processImage(eyeImage, inner_radius, center_x);
            if (wtwResult.valid) {
                if (eyeType == "left") {
                    raw_wtw_L = wtwResult.wtw; raw_pupd_L = wtwResult.puiLD;
                    raw_AL_L=AL;raw_ALCR_L=AL / (rawR1_L + rawR2_L);
                    qDebug() << "raw_pupd_L:" << wtwResult.puiLD;
                    qDebug() << "raw_wtw_L:" << wtwResult.wtw;
                }else{
                    // 达到99时不再增加
                    raw_wtw_R = wtwResult.wtw; raw_pupd_R = wtwResult.puiLD;
                    raw_AL_R=AL;raw_ALCR_R=AL / (rawR1_R + rawR2_R);
                }
            } else {
                qDebug() << "WTW 计算失败！";
            }

        }
    }

    recalcAndUpdate();
    updateALDataDisplay();

    // 点击测量start时，显示当前瞳距数据
    updatetongjuDataDisplay();
   }

void MainWindow::on_startMeasureBtn_clicked()
{
    // 用户确认，执行原有逻辑
    // ---------------------- 原有cancel函数内容 ----------------------
    // 重置查表数据
    rawRl_R = rawRs_R = rawRl_L = rawRs_L = 208; // 使S、C为0
    rawAngle_R = rawAngle_L = 1;                // 使A为空白
    rawS_R = rawC_R = rawA_R = 0;
    rawS_L = rawC_L = rawA_L = 0;
    // 重置图像处理数据
    rawR1_R = rawR2_R = rawAX_R = rawK1_R = rawK2_R = rawD_R = rawC1_R = 0;
    rawR1_L = rawR2_L = rawAX_L = rawK1_L = rawK2_L = rawD_L = rawC1_L = 0;
    raw_wtw_L = raw_pupd_L = 0;
    raw_wtw_R= raw_pupd_R = 0;
    raw_AL_L=raw_ALCR_L=0;
    raw_AL_R=raw_ALCR_R=0;
    m_currentInterpupillaryDistance=0;

    // 重置全局变量到默认状态
    g_vars.reset();
    // 重新计算并更新显示（S/C/A也会清零）
    recalcAndUpdate();
    updatetongjuDataDisplay();
    // 0830重置左眼计数器和标签
    m_leftCounter = 0; // 假设之前已定义m_leftCount
    ui->label_11->setText("00");
    ui->label_12->setText("/ 00");
    ui->label_13->setText("/ 00");

    // 重置右眼计数器和标签
    m_rightCounter = 0; // 假设之前已定义m_rightCount
    ui->label_10->setText("00");
    ui->label_9->setText("/ 00");
    ui->label_8->setText("/ 00");//0208xin
    m_measureHistoryBuffer.clear();

    ui->stackedWidget_8->setCurrentWidget(ui->page);
    //界面初始化1203fyb
    initalrefrraControlriginalPositions();
    initLabelsVisibility();//1201fyb

    // 创建定时器
    if (!m_imageProcessTimer) {
        m_imageProcessTimer = new QTimer(this);
        connect(m_imageProcessTimer, &QTimer::timeout, this, &MainWindow::onImageProcessTimerTimeout);
    }
    // 初始化图像处理处理标志位
    m_isImageProcessing = false;
    // 初始化距离标志位
    hasExecutedLessThan100 = false;
    hasExecutedLessThan20 = false;
    // 初始化USB状态标志位
    m_isUsbReceiving = false;
    // 初始化电机旋转标志位
    m_motorRotationSent = false;
    // 初始化开始测量标志位
    startMeasure = false;
    focus_succ = false;
    // 初始化激光状态标志位
    m_laserOn = false;
    // 初始化电机移动确认标志位
    m_waitingForMotorConfirm.store(false);//0701fyb
    // wyy0520：0x61 瞳距调整相关标志初始化
    m_waitingForPd61Move = false;
    m_pupilXYAligned = false;
   // resetAutoAlignGateFlags(); // wyy260520
    //0701fyb激光旋转电机总控制
    m_preparationDone=false;
    // 初始化自动测量标志位、
    m_auto = true;
//    zok = false;//wyy0701
    // 设置初始按钮图片（m_auto为true时显示control0.png）
    ui->control1->setStyleSheet("border-image: url(:/img/control0.png); background-color: transparent; border: none;");
    // 启动定时器，每500毫秒执行一次
    m_imageProcessTimer->start(20);
    qDebug() << "图像处理定时器启动";
}


