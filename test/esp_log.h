#pragma once

#include <QDebug>

#define ESP_LOGV(TAG, ...) qDebug(__VA_ARGS__)
#define ESP_LOGD(TAG, ...) qDebug(__VA_ARGS__)
#define ESP_LOGI(TAG, ...) qInfo(__VA_ARGS__)
#define ESP_LOGW(TAG, ...) qWarning(__VA_ARGS__)
#define ESP_LOGE(TAG, ...) qCritical(__VA_ARGS__)
