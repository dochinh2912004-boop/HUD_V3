#include "lib_flash.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "LIB_FLASH";
static nvs_handle_t s_nvs_handle = 0;  // đổi tên tránh xung đột

void Flash_UnLock(void)
{
    if (s_nvs_handle != 0) return;
    esp_err_t err = nvs_open("hud_config", NVS_READWRITE, &s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(RW) THAT BAI: %s", esp_err_to_name(err));
        s_nvs_handle = 0;
    } else {
        ESP_LOGI(TAG, "nvs_open(RW) OK, handle=%lu", (unsigned long)s_nvs_handle);
    }
}

void Flash_Lock(void)
{
    if (s_nvs_handle != 0) {
        esp_err_t err_commit = nvs_commit(s_nvs_handle);
        if (err_commit != ESP_OK) {
            ESP_LOGE(TAG, "nvs_commit THAT BAI: %s", esp_err_to_name(err_commit));
        } else {
            ESP_LOGI(TAG, "nvs_commit OK");
        }
        nvs_close(s_nvs_handle);
        s_nvs_handle = 0;
    }
}

void Flash_Write_uint8_t(const char* key, uint8_t data)
{
    if (s_nvs_handle == 0) {
        Flash_UnLock();
    }
    if (s_nvs_handle != 0 && key != NULL) {
        esp_err_t err_set = nvs_set_u8(s_nvs_handle, key, data);
        if (err_set != ESP_OK) {
            ESP_LOGE(TAG, "nvs_set_u8('%s', %u) THAT BAI: %s", key, data, esp_err_to_name(err_set));
        } else {
            ESP_LOGI(TAG, "nvs_set_u8('%s', %u) OK", key, data);
        }
        Flash_Lock();
    } else {
        ESP_LOGE(TAG, "Flash_Write_uint8_t('%s'): KHONG CO HANDLE HOP LE, bo qua ghi!", key ? key : "(null)");
    }
}

uint8_t Flash_Read_uint8_t(const char* key)
{
    nvs_handle_t handle;
    uint8_t data = 0;
    esp_err_t err = nvs_open("hud_config", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        esp_err_t err_get = nvs_get_u8(handle, key, &data);
        if (err_get != ESP_OK) {
            ESP_LOGW(TAG, "nvs_get_u8('%s') THAT BAI: %s", key, esp_err_to_name(err_get));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Flash_Read_uint8_t('%s'): nvs_open(RO) THAT BAI: %s", key, esp_err_to_name(err));
    }
    return data;
}

void Flash_Erase(uint32_t page_address)
{
    (void)page_address; // Không dùng trên ESP32
}