#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lib_led.h"      // HUD_* mask, CarModelNames[], carmodel_count
#include "lib_buzzer.h"   // Buzzer_* volume
#include "web_config.h"
#include "main.h"         // KEY_CAR_MODEL va cac KEY_* khac

static const char LOGO_B64[] =
    "iVBORw0KGgoAAAANSUhEUgAAAQQAAACtBAMAAACkQhyrAAAAHlBMVEUAAAD/AAAAAAD/AAD/AAAAAAAAAAD/AAAAAAAAAAD3L0tF"
    "AAAACHRSTlMA/foImqdlYoPLrbUAAATASURBVHja7Zy/i9xGFMe/0uyqMCQ4ZcCLDwIHASs3WItdGIdbXKT0ceAi4MPLFdc5Vf6J"
    "FIbAdS7CQlI5jVvDHghcOVhGtuwEzuS4cCkdshBIsdnZTbHaPe3qx4w0M1KC51Xa0ZP02ffevNH8koVyQmjJCxAsj7ys0ywseT+v"
    "NEH2g5Niod0ByFsA7U5c1n0EANiN+ciNwfJ2Afk6CMo8n7wLvPgC2gvT/5hu+RbsjTnCkgD47HGCIAYEAMKuXyhrN+w/oPNr6EdZ"
    "126NbJD4eO+8+A0A51yf7cQH7vXj0gT47lZU7D17eeQnQgTAJKEUP9eLjlFBXtyPxBDaydIdYCfxsx274StUkh/cQjO05n/7AJOE"
    "FfDzAYbpyHn5WzUEdjbiI6w/cDLMyAjsZlgNAVMXIRdBRH65gKpytsG3wsl5HfIXtRIA4FxaHLm9QWUEK+SGI0nUodXz49OFH0h1"
    "ArD7VKRS8uQlJOR7oiAW6PRUAqEVKrACuyhjhTGl0ggkCmUQcFOBFVpSBEryApNDiEYKYgG6RNQKrnWq6zk2Gpe6EJixgkEwCAZB"
    "fYJOydVR8X0zej1/b24DmLwI1SDYRxyF7q8Z7fUzAOgpskJng6MwoaFgN78igh3xGm/Pv5g92EEVhaPrcl+THM01wuOrXNGMcMTV"
    "YIya1GQQDIJBMAgGQRqBNY3QwvTzwUrJCeCcD0NiegL032p2xCBVOF77PTDhaBAMgkEwCI2MLzi3C1WsR9oRnEucifDNY92OuMvT"
    "mfR1I/hcJV8zgsA4gKMZYcZXmpm8UEteAEB6eT44qgvhxsM8hX2/Jkfkv6AO3odYMAhCCKSuGpEv/5wYR+TJreYRgjpjIVsKliPU"
    "hTB1SdMIOPuDq9LTjDD94l3TVsBPJi8YBIPQTGNdUg4S1TZsBIF8U192zOt2eI07wh0trU+VIcSr9iPrA5Hb/dV4ODoh01gphTr4"
    "E6oxL4gtBdhjGhHA+gJKTyWy406uwuJM5IuEQlg9HF/dyfvzy5u++pAfCq4EAv+tnFH+svyO5jYies3LDE7EoDMcwa7wAvJLt6IV"
    "LL7SXCXi7NFwDqvNutqpSckMmauwTz8p1Pq4mhFgA9tcpVgjel7EcK/qwn07tS8kw1lxVWDe8828JHnt8rcVjYAWgN/vFcXDzFoO"
    "CAbeM9LdDlMYIR0+IbSiEUSCMSle3t7D9InVLXPkT5ZZjq1RycY6IJ7yAYey7wtM5eCG6UcYBINgEPJSk9MH8HCtV7zSVA90I8wW"
    "m1h3hzk6G6c1OaKd29LZ/ZoQ9vKV/JoQCp7j1IPQlu3SyjbWd8p/B0E1QtBoXpi/d4Hbp9SN8GOuwq52Bm6CfmyaKYPw/0cgJGzc"
    "Cq8btwIdi2oKd+jKfpwn6IWqEcp2JruhciuUnGhoiS9QFEWQ+wiFSU0GwSAYhP8kAjFWwOrXyN5fKxRsT68LgTZvhacKXlnkpGhK"
    "vyYrTGQc0VeCUDSlHzviIO+8mjXx5JCPMNTrhxkNmq4RHYaGa0TxlzuErGBJRkLHlU7QYzmEu8VTyUII21IEVw9d6WZqMWddTfaP"
    "OMtLbFi8Fc1dif1r3rXLD7w1gpXP+1FaQ3XwlAU7qYibHr711s7/C2miFujkMqvSAAAAAElFTkSuQmCC";


static const char *TAG = "hud_web_config";
static httpd_handle_t g_server = NULL;

/* ---------------- Co bat/tat LED tu dong ---------------- */
volatile bool flag_led_auto_brightness = true;
#define LED_AUTO_KEY   "led_auto_en"

void LedAuto_SetEnable(bool enable)
{
    flag_led_auto_brightness = enable;
}

void LedAuto_SaveEnable(bool enable)
{
    LedAuto_SetEnable(enable);
    Flash_Write_uint8_t(LED_AUTO_KEY, enable ? 1 : 0);
}

void LedAuto_LoadFromFlash(void)
{
    nvs_handle_t h;
    bool key_exists = false;
    if (nvs_open("hud_config", NVS_READONLY, &h) == ESP_OK) {
        uint8_t tmp = 0;
        key_exists = (nvs_get_u8(h, LED_AUTO_KEY, &tmp) == ESP_OK);
        nvs_close(h);
    }
    if (key_exists) {
        flag_led_auto_brightness = (Flash_Read_uint8_t(LED_AUTO_KEY) != 0);
    } else {
        flag_led_auto_brightness = true; // mac dinh bat
    }
}

/* ---------------- 3 muc sang co dinh khi TAT LED tu dong ---------------- */
volatile uint8_t led_manual_level = 1; // mac dinh: Vua
#define LED_MANUAL_LEVEL_KEY   "led_man_lvl"

uint8_t LedManualLevel_GetDutyPercent(void)
{
    switch (led_manual_level) {
        case 0:  return 5;   // Thap
        case 2:  return 50;  // Cao
        default: return 20;  // Vua 
        //  case 0:  return 50;   // Thap
        // case 2:  return 80;  // Cao
        // default: return 60;  // Vua 
    }
}

void LedManualLevel_SetLevel(uint8_t level)
{
    if (level > 2) level = 1; // gia tri la -> ep ve Vua cho an toan
    led_manual_level = level;
}

void LedManualLevel_SaveLevel(uint8_t level)
{
    LedManualLevel_SetLevel(level);
    Flash_Write_uint8_t(LED_MANUAL_LEVEL_KEY, led_manual_level);
}

void LedManualLevel_LoadFromFlash(void)
{
    nvs_handle_t h;
    bool key_exists = false;
    if (nvs_open("hud_config", NVS_READONLY, &h) == ESP_OK) {
        uint8_t tmp = 0;
        key_exists = (nvs_get_u8(h, LED_MANUAL_LEVEL_KEY, &tmp) == ESP_OK);
        nvs_close(h);
    }
    if (key_exists) {
        LedManualLevel_SetLevel(Flash_Read_uint8_t(LED_MANUAL_LEVEL_KEY));
    } else {
        led_manual_level = 1; // mac dinh Vua
    }
}

/* ---------------- Danh sach 14 checkbox "VUNG HIEN THI" ---------------- */
/* Thu tu quan trong: dung grid-auto-flow:column 3 cot x 5 hang tren web
 * (5 muc dau -> cot 1, 5 muc tiep -> cot 2, 4 muc cuoi -> cot 3) */
struct RegionItem { const char *field; uint16_t bit; const char *label; };
static const RegionItem REGION_ITEMS[] = {
    // {"speed",     HUD_REGION_SPEED,      "Tốc Độ"},
    {"turn",      HUD_REGION_TURN,       "Xi Nhan"},
    {"gear",      HUD_REGION_GEAR,       "Cần Số"},
    {"drivemode", HUD_REGION_DRIVEMODE,  "Chế Độ Lái"},
    {"handbrake", HUD_REGION_HANDBRAKE,  "Phanh Tay"},
    {"seatbelt",  HUD_REGION_SEATBELT,   "Dây An Toàn"},
    {"battery",   HUD_REGION_BATTERY,    "% Pin"},
    {"range",     HUD_REGION_RANGE,      "Range"},
    {"door",      HUD_REGION_DOOR,       "Cảnh Báo Cửa"},
    {"highbeam",  HUD_REGION_HIGHBEAM,   "Đèn Xa"},
    {"lowbeam",   HUD_REGION_LOWBEAM,    "Đèn Gần"},
    {"speedlimit",HUD_REGION_SPEED_LIMIT,"Tốc Độ Giới Hạn"},
    {"camera",    HUD_REGION_CAMERA,     "Camera"},
    {"noovertake",HUD_REGION_NO_OVERTAKING,"Cấm Vượt"},
};
#define REGION_ITEM_COUNT (sizeof(REGION_ITEMS)/sizeof(REGION_ITEMS[0]))

/* So muc trong tung khung hien thi (3 khung, tong phai = REGION_ITEM_COUNT) */
static const uint8_t REGION_GROUP_SIZES[] = {5, 5, 3};
#define REGION_GROUP_COUNT (sizeof(REGION_GROUP_SIZES)/sizeof(REGION_GROUP_SIZES[0]))

/* ---------------- Trang chinh (GET /) ---------------- */
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");

    char chunk[1024];

    // ---- Head + style ----
    httpd_resp_sendstr_chunk(req,
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>MCD91 Config</title>"
        "<style>"
        "*{box-sizing:border-box}"
        "body{font-family:-apple-system,Segoe UI,Roboto,Arial,sans-serif;margin:0;background:#eef1f6;color:#1f2430}"
        ".topbar{background:#fff;padding:14px 18px;display:flex;align-items:center;justify-content:space-between;"
        "position:relative;"
        "border-bottom:1px solid #e6e9ef;box-shadow:0 2px 10px rgba(15,23,42,.06)}"
        ".brand{display:flex;align-items:center;gap:10px}"
        ".topbar img{max-height:36px;display:block}"
        ".brandTitle{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);"
        "font-size:16px;font-weight:800;color:#1f2430;letter-spacing:.02em;white-space:nowrap}"
        ".verBadge{font-size:12px;font-weight:700;color:#6b7280;background:#f1f3f7;padding:5px 11px;"
        "border-radius:20px;white-space:nowrap;flex-shrink:0}"
        ".verNum{color:#e53935;font-weight:800}"
        ".wrap{max-width:720px;margin:0 auto;padding:22px 16px 48px}"
        ".card{background:#fff;border-radius:16px;box-shadow:0 3px 14px rgba(15,23,42,.08);padding:20px 22px;margin-bottom:20px}"
        ".card h3{margin:0 0 16px 0;font-size:13px;letter-spacing:.08em;text-transform:uppercase;color:#5b6472;"
        "display:flex;align-items:center;gap:8px;font-weight:700}"
        ".card h3:before{content:'';width:5px;height:16px;background:#2f6fed;border-radius:3px;display:inline-block}"
        ".regionCols{display:flex;gap:14px;flex-wrap:wrap;align-items:flex-start}"
        ".regionBox{flex:1 1 0;min-width:150px;border:1.5px solid #cfe0fb;border-radius:12px;"
        "padding:2px 14px;background:#fbfcff}"
        ".regionItem{display:flex;align-items:center;justify-content:space-between;gap:8px;padding:9px 0;"
        "border-bottom:1px solid #f0f1f4;min-width:0}"
        ".regionItem.noBorder{border-bottom:none}"
        ".regionLabel{font-size:13.5px;font-weight:600;color:#2a2f3a}"
        ".fieldRow{display:flex;gap:18px;flex-wrap:wrap}"
        ".field{flex:1;min-width:200px}"
        ".field label{display:block;font-size:12px;font-weight:700;letter-spacing:.03em;color:#6b7280;"
        "text-transform:uppercase;margin-bottom:8px}"
        "select{width:100%;font-size:15px;padding:10px 12px;border-radius:8px;border:1px solid #d7dbe3;"
        "font-weight:600;background:#fafbfc;color:#1f2430}"
        "input[type=range]{width:100%;accent-color:#2f6fed}"
        ".volVal{font-size:13px;color:#6b7280;font-weight:600;margin-top:4px;display:inline-block}"
        ".toggleRow{display:flex;align-items:center;justify-content:space-between;gap:14px}"
        ".toggleRow .ttl{font-size:15px;font-weight:700;color:#1f2430}"
        ".toggleRow .sub{font-size:12.5px;color:#8a93a3;margin-top:3px}"
        ".segWrap{margin-top:16px;padding-top:14px;border-top:1px solid #f0f1f4}"
        ".segLabel{font-size:12px;font-weight:700;color:#6b7280;text-transform:uppercase;margin-bottom:10px}"
        ".seg{display:flex;gap:8px}"
        ".segBtn{flex:1}"
        ".segBtn input{display:none}"
        ".segBtn span{display:block;text-align:center;padding:10px 0;border-radius:8px;border:1.5px solid #d7dbe3;"
        "font-weight:700;font-size:13.5px;color:#4b5563;cursor:pointer;transition:.15s}"
        ".segBtn input:checked + span{background:#2f6fed;border-color:#2f6fed;color:#fff}"
        ".sw{position:relative;display:inline-block;width:48px;height:27px;flex-shrink:0}"
        ".sw input{opacity:0;width:0;height:0}"
        ".slider{position:absolute;cursor:pointer;inset:0;background:#d7dbe3;border-radius:27px;transition:.2s}"
        ".slider:before{position:absolute;content:'';height:21px;width:21px;left:3px;bottom:3px;background:#fff;"
        "border-radius:50%;transition:.2s;box-shadow:0 1px 3px rgba(0,0,0,.25)}"
        "input:checked + .slider{background:#2f6fed}"
        "input:checked + .slider:before{transform:translateX(21px)}"
        ".dropzone{border:2px dashed #c7cddb;border-radius:12px;padding:16px;display:flex;align-items:center;"
        "gap:12px;background:#fafbfd}"
        ".dropzone .fname{flex:1;font-size:13.5px;color:#6b7280;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        "button{font-family:inherit;cursor:pointer;border:none}"
        ".btn-outline{padding:9px 16px;border-radius:8px;border:1.5px solid #2f6fed;background:#fff;"
        "color:#2f6fed;font-weight:700;font-size:13.5px}"
        ".btn-primary{padding:9px 18px;border-radius:8px;background:#2f6fed;color:#fff;font-weight:700;font-size:13.5px}"
        ".btn-save{display:block;width:100%;margin-top:6px;padding:15px;font-size:16px;font-weight:800;"
        "letter-spacing:.04em;color:#fff;border-radius:12px;"
        "background:linear-gradient(135deg,#3fb56f,#2e9c58);box-shadow:0 4px 12px rgba(46,156,88,.35)}"
        ".progress-wrap{width:100%;height:9px;background:#e7eaef;border-radius:6px;overflow:hidden;margin-top:14px;display:none}"
        ".progress-bar{height:100%;width:0%;background:linear-gradient(90deg,#2f6fed,#63a3ff);transition:width .15s}"
        ".ota-status{font-size:13px;margin-top:8px;color:#5b6472;font-weight:600}"
        ".footNote{text-align:center;font-size:11.5px;color:#9aa2b1;margin-top:6px}"
        "</style></head><body>"
        "<div class='topbar'>"
        "<div class='brand'>"
        "<img alt='MCD91' src='data:image/png;base64,"
    );
    httpd_resp_sendstr_chunk(req, LOGO_B64);
    snprintf(chunk, sizeof(chunk),
        "'>"
        "</div>"
        "<span class='brandTitle'>THIẾT LẬP HUD</span>"
        "<div class='verBadge'>Version <span class='verNum'>3.%d</span></div>"
        "</div>",
        (int)Version);
    httpd_resp_sendstr_chunk(req, chunk);
    httpd_resp_sendstr_chunk(req,
        "<div class='wrap'>"
        "<form id='cfgForm' method='POST' action='/save'>"
        "<div class='card'><h3>Vùng hiển thị</h3><div class='regionCols'>"
    );

    // ---- 13 toggle vung hien thi, chia thanh 3 khung vien rieng ----
    size_t item_idx = 0;
    for (size_t g = 0; g < REGION_GROUP_COUNT; g++) {
        httpd_resp_sendstr_chunk(req, "<div class='regionBox'>");
        uint8_t group_size = REGION_GROUP_SIZES[g];
        for (uint8_t j = 0; j < group_size && item_idx < REGION_ITEM_COUNT; j++, item_idx++) {
            bool checked = (hud_display_mask & REGION_ITEMS[item_idx].bit) != 0;
            bool is_last = (j == group_size - 1);
            snprintf(chunk, sizeof(chunk),
                "<div class='regionItem%s'><span class='regionLabel'>%s</span>"
                "<label class='sw'><input type='checkbox' name='%s' value='1' %s><span class='slider'></span></label></div>",
                is_last ? " noBorder" : "",
                REGION_ITEMS[item_idx].label, REGION_ITEMS[item_idx].field, checked ? "checked" : "");
            httpd_resp_sendstr_chunk(req, chunk);
        }
        httpd_resp_sendstr_chunk(req, "</div>");
    }
    httpd_resp_sendstr_chunk(req, "</div></div>");

    // ---- Model xe + Am luong coi ----
    httpd_resp_sendstr_chunk(req,
        "<div class='card'><h3>Cấu hình xe</h3><div class='fieldRow'>"
        "<div class='field'><label>Chọn model xe</label><select name='carmodel'>");
    for (uint8_t i = 0; i < TOTAL_MODEL; i++) {
        snprintf(chunk, sizeof(chunk), "<option value='%u' %s>%s</option>",
                 (unsigned)i, (i == carmodel_count) ? "selected" : "", CarModelNames[i]);
        httpd_resp_sendstr_chunk(req, chunk);
    }
    httpd_resp_sendstr_chunk(req, "</select></div>");

    snprintf(chunk, sizeof(chunk),
        "<div class='field'><label>Âm lượng còi</label>"
        "<input type='range' name='volume' min='0' max='100' value='%u' "
        "oninput='this.nextElementSibling.textContent=this.value+\"%%\"'>"
        "<span class='volVal'>%u%%</span></div></div></div>",
        (unsigned)Buzzer_GetVolume(), (unsigned)Buzzer_GetVolume());
    httpd_resp_sendstr_chunk(req, chunk);

    // ---- Do sang LED tu dong + 3 muc co dinh khi tat ----
    snprintf(chunk, sizeof(chunk),
        "<div class='card'><div class='toggleRow'><div>"
        "<div class='ttl'>Độ sáng LED tự động</div>"
        "<div class='sub'>Tự động chỉnh theo cảm biến ánh sáng</div></div>"
        "<label class='sw'><input type='checkbox' id='ledAutoChk' name='ledauto' value='1' %s "
        "onchange=\"document.getElementById('ledLevelBox').style.display=this.checked?'none':'block'\">"
        "<span class='slider'></span></label>"
        "</div>"
        "<div class='segWrap' id='ledLevelBox' style='display:%s'>"
        "<div class='segLabel'>Chọn mức sáng cố định</div>"
        "<div class='seg'>"
        "<label class='segBtn'><input type='radio' name='ledlevel' value='0' %s><span>Thấp</span></label>"
        "<label class='segBtn'><input type='radio' name='ledlevel' value='1' %s><span>Vừa</span></label>"
        "<label class='segBtn'><input type='radio' name='ledlevel' value='2' %s><span>Cao</span></label>"
        "</div></div></div>",
        flag_led_auto_brightness ? "checked" : "",
        flag_led_auto_brightness ? "none" : "block",
        (led_manual_level == 0) ? "checked" : "",
        (led_manual_level == 1) ? "checked" : "",
        (led_manual_level == 2) ? "checked" : "");
    httpd_resp_sendstr_chunk(req, chunk);

    httpd_resp_sendstr_chunk(req, "<button class='btn-save' type='submit'>LƯU CẤU HÌNH</button></form>");

    // ---- Cap nhat phan mem (OTA) - form rieng, khong nam trong cfgForm ----
    httpd_resp_sendstr_chunk(req,
        "<div class='card'><h3>Cập nhật phần mềm</h3>"
        "<div class='dropzone'>"
        "<span class='fname' id='fileName'>Chưa chọn file firmware (.bin)</span>"
        "<button type='button' class='btn-outline' onclick=\"document.getElementById('fw').click()\">Chọn file</button>"
        "<input type='file' id='fw' style='display:none' accept='.bin'>"
        "</div>"
        "<div style='text-align:right;margin-top:12px'>"
        "<button type='button' class='btn-primary' onclick='doOta()'>Cập nhật ngay</button>"
        "</div>"
        "<div class='progress-wrap' id='otaWrap'><div class='progress-bar' id='otaBar'></div></div>"
        "<div class='ota-status' id='otaStatus'></div>"
        "</div>"
        "<div class='footNote'>MCD91 &middot; HUD Config</div>"
        "</div>" // dong .wrap
    );

    // ---- JS: upload OTA bang XHR de co progress ----
    httpd_resp_sendstr_chunk(req,
        "<script>"
        "document.getElementById('fw').addEventListener('change',function(e){"
        "  var f=e.target.files[0];"
        "  document.getElementById('fileName').textContent = f ? f.name : 'Chưa chọn file firmware (.bin)';"
        "});"
        "function doOta(){"
        "  var f=document.getElementById('fw').files[0];"
        "  if(!f){alert('Hãy chọn file .bin firmware trước');return;}"
        "  var xhr=new XMLHttpRequest();"
        "  xhr.open('POST','/ota',true);"
        "  document.getElementById('otaWrap').style.display='block';"
        "  xhr.upload.onprogress=function(ev){"
        "    if(ev.lengthComputable){"
        "      var pct=Math.round(ev.loaded*100/ev.total);"
        "      document.getElementById('otaBar').style.width=pct+'%';"
        "      document.getElementById('otaStatus').textContent='Đang tải lên: '+pct+'%';"
        "    }"
        "  };"
        "  xhr.onload=function(){"
        "    if(xhr.status==200){"
        "      document.getElementById('otaBar').style.width='100%';"
        "      document.getElementById('otaStatus').textContent='Cập nhật thành công. Thiết bị đang khởi động lại...';"
        "    } else {"
        "      document.getElementById('otaStatus').textContent='Lỗi cập nhật: '+xhr.status+' '+xhr.responseText;"
        "    }"
        "  };"
        "  xhr.onerror=function(){document.getElementById('otaStatus').textContent='Mất kết nối khi tải lên';};"
        "  document.getElementById('otaStatus').textContent='Bắt đầu tải lên...';"
        "  xhr.send(f);"
        "}"
        "</script></body></html>"
    );

    httpd_resp_sendstr_chunk(req, NULL); // ket thuc chunked response
    return ESP_OK;
}

/* ---------------- Luu cau hinh (POST /save) ---------------- */
static esp_err_t save_handler(httpd_req_t *req)
{
    char buf[512];
    int total = 0;
    int to_read = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    while (total < to_read) {
        int r = httpd_req_recv(req, buf + total, to_read - total);
        if (r <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
        total += r;
    }
    buf[total] = '\0';
    ESP_LOGI(TAG, "POST /save body(%d bytes)='%s'", total, buf);

    // ---- 1) Mask 11 vung hien thi ----
    uint16_t mask = 0;
    for (size_t i = 0; i < REGION_ITEM_COUNT; i++) {
        char key[24];
        snprintf(key, sizeof(key), "%s=1", REGION_ITEMS[i].field);
        if (strstr(buf, key) != NULL) {
            mask |= REGION_ITEMS[i].bit;
        }
    }
    HUD_SaveDisplayMask(mask);
    ESP_LOGI(TAG, "Da luu hud mask = 0x%04X", mask);

    // ---- 2) Model xe ----
    char *p = strstr(buf, "carmodel=");
    if (p != NULL) {
        int model = atoi(p + strlen("carmodel="));
        if (model >= 0 && model < TOTAL_MODEL) {
            carmodel_count = (uint8_t)model;
            Flash_Write_uint8_t(KEY_CAR_MODEL, carmodel_count);
            ESP_LOGI(TAG, "Da luu model xe = %d (%s)", model, CarModelNames[model]);
        }
    }

    // ---- 3) Am luong coi ----
    p = strstr(buf, "volume=");
    if (p != NULL) {
        int vol = atoi(p + strlen("volume="));
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        Buzzer_SaveVolume((uint8_t)vol);
        ESP_LOGI(TAG, "Da luu am luong coi = %d%%", vol);
    }

    // ---- 4) LED tu dong (checkbox: co mat trong body neu duoc tick) ----
    bool ledauto = (strstr(buf, "ledauto=1") != NULL);
    LedAuto_SaveEnable(ledauto);
    ESP_LOGI(TAG, "Da luu LED tu dong = %s", ledauto ? "BAT" : "TAT");

    // ---- 5) Muc sang co dinh (Thap/Vua/Cao) khi LED tu dong dang TAT ----
    p = strstr(buf, "ledlevel=");
    if (p != NULL) {
        int lvl = atoi(p + strlen("ledlevel="));
        if (lvl < 0) lvl = 0;
        if (lvl > 2) lvl = 2;
        LedManualLevel_SaveLevel((uint8_t)lvl);
        ESP_LOGI(TAG, "Da luu muc sang thu cong = %d", lvl);
    }

    const char *resp = "<html><body><h3>Da luu cau hinh. Dang khoi dong lai...</h3></body></html>";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* ---------------- OTA: nhan firmware .bin qua HTTP (POST /ota) ---------------- */
static esp_err_t ota_handler(httpd_req_t *req)
{
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Khong tim thay OTA partition - kiem tra partition table (can co ota_0/ota_1)");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition configured");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, req->content_len, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin loi: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;
    int total = 0;
    ESP_LOGI(TAG, "Bat dau nhan firmware qua OTA, kich thuoc: %d bytes", remaining);

    while (remaining > 0) {
        int want = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int received = httpd_req_recv(req, buf, want);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue; // thu lai
        }
        if (received <= 0) {
            ESP_LOGE(TAG, "Loi nhan du lieu OTA (received=%d)", received);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed during OTA");
            return ESP_FAIL;
        }
        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write loi: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }
        remaining -= received;
        total += received;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end loi: %s (file khong hop le / thieu chu ky)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid firmware image");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition loi: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA thanh cong, tong %d bytes. Khoi dong lai...", total);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_sendstr(req, "OK");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return ESP_OK;
}

static void start_http_server(void)
{
    if (g_server != NULL) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.stack_size = 8192;          // esp_ota_write can hop toi ho tro OTA
    config.recv_wait_timeout = 15;     // giay - firmware lon can nhieu thoi gian hon
    config.send_wait_timeout = 15;
    if (httpd_start(&g_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = NULL };
    httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST, .handler = save_handler, .user_ctx = NULL };
    httpd_uri_t ota_uri  = { .uri = "/ota",  .method = HTTP_POST, .handler = ota_handler,  .user_ctx = NULL };
    httpd_register_uri_handler(g_server, &root_uri);
    httpd_register_uri_handler(g_server, &save_uri);
    httpd_register_uri_handler(g_server, &ota_uri);
}

static volatile bool s_wifi_radio_on = false;

static void start_access_point(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    wifi_config_t wifi_config = {0};
    strcpy((char *)wifi_config.ap.ssid, "HUD_Config");
    wifi_config.ap.ssid_len = strlen("HUD_Config");
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    wifi_config.ap.password[0] = '\0';

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_wifi_radio_on = true;

    ESP_LOGI(TAG, "AP started: SSID=HUD_Config, IP=http://192.168.4.1");
}

/* ---------------- Bat/Tat radio WiFi khi dang chay ---------------- */
void WiFi_Enable(void)
{
    if (s_wifi_radio_on) return; // da bat roi, khong lam gi them
    esp_err_t err = esp_wifi_start();
    if (err == ESP_OK) {
        s_wifi_radio_on = true;
        ESP_LOGI(TAG, "WiFi da BAT lai (AP HUD_Config)");
    } else {
        ESP_LOGE(TAG, "esp_wifi_start loi khi bat lai: %s", esp_err_to_name(err));
    }
}

void WiFi_Disable(void)
{
    if (!s_wifi_radio_on) return; // da tat roi
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_OK) {
        s_wifi_radio_on = false;
        ESP_LOGI(TAG, "WiFi da TAT (tiet kiem dien / ngat ket noi web config)");
    } else {
        ESP_LOGE(TAG, "esp_wifi_stop loi: %s", esp_err_to_name(err));
    }
}

static void hud_web_config_task(void *arg)
{
    (void)arg;
    start_access_point();
    start_http_server();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void WebConfig_Start(void)
{
    static bool started = false;
    if (started) return;
    started = true;
    // Uu tien 5 (bang CAN task va main loop) - tranh bi doi CPU tren chip 1 nhan
    // (ESP32-C3), vi CAN task/main loop wake day rat thuong xuyen (1ms/10ms).
    xTaskCreate(hud_web_config_task, "hud_web_cfg", 8192, NULL, 5, NULL);
}