/* Unified Provisioning Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_bt.h>

#include <protocomm.h>
#include <protocomm_ble.h>

#include "conn_mgr_prov.h"
#include "conn_mgr_prov_mode_ble.h"

static const char *TAG = "conn_mgr_prov_mode_ble";

extern conn_mgr_prov_t conn_mgr_prov_mode_ble;

static esp_err_t prov_start(protocomm_t *pc, void *config)
{
    if (!pc) {
        ESP_LOGE(TAG, "Protocomm handle cannot be null");
        return ESP_ERR_INVALID_ARG;
    }

    if (!config) {
        ESP_LOGE(TAG, "Cannot start with null configuration");
        return ESP_ERR_INVALID_ARG;
    }

    conn_mgr_prov_mode_ble_config_t *ble_config = (conn_mgr_prov_mode_ble_config_t *) config;

    /* Start protocomm as BLE service */
    if (protocomm_ble_start(pc, ble_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start protocomm BLE service");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void *new_config(void)
{
    conn_mgr_prov_mode_ble_config_t *ble_config = calloc(1, sizeof(conn_mgr_prov_mode_ble_config_t));
    if (!ble_config) {
        ESP_LOGE(TAG, "Error allocating memory for new configuration");
        return NULL;
    }

    uint8_t service_uuid[16] = {
        /* LSB <---------------------------------------
         * ---------------------------------------> MSB */
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
    };

    memcpy(ble_config->service_uuid, service_uuid, sizeof(service_uuid));
    return ble_config;
}

static void delete_config(void *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Cannot delete null configuration");
        return;
    }

    conn_mgr_prov_mode_ble_config_t *ble_config = (conn_mgr_prov_mode_ble_config_t *) config;
    for (unsigned int i = 0; i < ble_config->nu_lookup_count; i++) {
        free((void *)ble_config->nu_lookup[i].name);
    }
    free(ble_config->nu_lookup);
    free(ble_config);
}

static esp_err_t set_config_service(void *config, const char *service_name, const char *service_key)
{
    if (!config) {
        ESP_LOGE(TAG, "Cannot set null configuration");
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_name) {
        ESP_LOGE(TAG, "Service name cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    conn_mgr_prov_mode_ble_config_t *ble_config = (conn_mgr_prov_mode_ble_config_t *) config;
    strlcpy(ble_config->device_name,  service_name, sizeof(ble_config->device_name));
    return ESP_OK;
}

static esp_err_t set_config_endpoint(void *config, const char *endpoint_name, uint16_t uuid)
{
    if (!config) {
        ESP_LOGE(TAG, "Cannot set null configuration");
        return ESP_ERR_INVALID_ARG;
    }

    if (!endpoint_name) {
        ESP_LOGE(TAG, "EP name cannot be null");
        return ESP_ERR_INVALID_ARG;
    }

    conn_mgr_prov_mode_ble_config_t *ble_config = (conn_mgr_prov_mode_ble_config_t *) config;

    char *copy_ep_name = strdup(endpoint_name);
    if (!copy_ep_name) {
        ESP_LOGE(TAG, "Error allocating memory for EP name");
        return ESP_ERR_NO_MEM;
    }

    protocomm_ble_name_uuid_t *lookup_table = (
                realloc(ble_config->nu_lookup, (ble_config->nu_lookup_count + 1) * sizeof(protocomm_ble_name_uuid_t)));
    if (!lookup_table) {
        ESP_LOGE(TAG, "Error allocating memory for EP-UUID lookup table");
        return ESP_ERR_NO_MEM;
    }

    lookup_table[ble_config->nu_lookup_count].name = copy_ep_name;
    lookup_table[ble_config->nu_lookup_count].uuid = uuid;
    ble_config->nu_lookup = lookup_table;
    ble_config->nu_lookup_count += 1;
    return ESP_OK;
}

/* Default event handler for releasing BT/BLE memory during/after provisioning */
static esp_err_t ble_prov_event_handler(void *user_data, conn_mgr_cb_event_t event)
{
    esp_err_t ret = ESP_OK;
    /* The following makes sense only if we are using conn_mgr_prov_mode_ble
     * and user application doesn't require BT/BLE to function once provisioning
     * is complete. If BT or BLE or both are required by the user application then
     * either of the following cases can be commented out depending on the requirements */
    switch (event) {
        case CM_PROV_START:
            /* If provisioning is about to start, release memory used by the unused BT stack.
             * This is to be commented out if user application requires BT to function */
            ret = esp_bt_mem_release(ESP_BT_MODE_CLASSIC_BT);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to release BT memory");
            } else {
                ESP_LOGI(TAG, "Released BT memory");
            }
            break;
        case CM_PROV_END:
            /* If provisioning is about to end, release memory used by the BLE stack
             * which will no longer be needed once provisioning is complete.
             * This is to be commented out if user application requires BLE to function,
             * or modified to ESP_BT_MODE_BLE (along with commenting out classic BT mem release above)
             * if BT is required instead */
            ret = esp_bt_mem_release(ESP_BT_MODE_BTDM);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to release BTDM memory");
            } else {
                ESP_LOGI(TAG, "Released BTDM memory");
            }
            break;
        default:
            break;
    }
    return ret;
}

conn_mgr_prov_t conn_mgr_prov_mode_ble = {
    .prov_start          = prov_start,
    .prov_stop           = protocomm_ble_stop,
    .new_config          = new_config,
    .delete_config       = delete_config,
    .set_config_service  = set_config_service,
    .set_config_endpoint = set_config_endpoint,
    .wifi_mode           = WIFI_MODE_STA,
    .event_cb            = ble_prov_event_handler,
    .cb_user_data        = NULL,
};
