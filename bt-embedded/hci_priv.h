#ifndef BTE_HCI_PRIV_H
#define BTE_HCI_PRIV_H

#ifdef __cplusplus
extern "C" {
#endif

#define HCI_CMD_REPLY_POS_CODE    0
#define HCI_CMD_REPLY_POS_LEN     1
#define HCI_CMD_REPLY_POS_HDR_LEN 2
#define HCI_CMD_REPLY_POS_PACKETS 2
#define HCI_CMD_REPLY_POS_OPCODE  3
#define HCI_CMD_REPLY_POS_STATUS  5
#define HCI_CMD_REPLY_POS_DATA    6

#define HCI_CMD_STATUS_POS_CODE    0
#define HCI_CMD_STATUS_POS_LEN     1
#define HCI_CMD_STATUS_POS_STATUS  2
#define HCI_CMD_STATUS_POS_PACKETS 3
#define HCI_CMD_STATUS_POS_OPCODE  4

#define HCI_CMD_EVENT_POS_CODE 0
#define HCI_CMD_EVENT_POS_LEN  1
#define HCI_CMD_EVENT_POS_DATA 2

/* HCI packet indicators */
#define HCI_COMMAND_DATA_PACKET 0x01
#define HCI_ACL_DATA_PACKET     0x02
#define HCI_SCO_DATA_PACKET     0x03
#define HCI_EVENT_PACKET        0x04
#define HCI_VENDOR_PACKET       0xFF

#define HCI_EVENT_HDR_LEN 2
#define HCI_ACL_HDR_LEN   4
#define HCI_SCO_HDR_LEN   3
#define HCI_CMD_HDR_LEN   3

/* Specification defined parameters */
#define HCI_BD_ADDR_LEN  6
#define HCI_LINK_KEY_LEN 16
#define HCI_LMP_FEAT_LEN 8

/* Opcode Group Field (OGF) values */
#define HCI_LINK_CTRL_OGF    0x01 /* Link Control Commands */
#define HCI_LINK_POLICY_OGF  0x02 /* Link Policy Commands */
#define HCI_HC_BB_OGF        0x03 /* Host Controller & Baseband Commands */
#define HCI_INFO_PARAM_OGF   0x04 /* Informational Parameters */
#define HCI_STATUS_PARAM_OGF 0x05 /* Status Parameters */
#define HCI_TESTING_OGF      0x06 /* Testing Commands */
#define HCI_VENDOR_OGF       0x3F /* Vendor-Specific Commands */

/* Opcode Command Field (OCF) values */

/* Link control commands */
#define HCI_INQUIRY_OCF                 0x01
#define HCI_INQUIRY_CANCEL_OCF          0x02
#define HCI_PERIODIC_INQUIRY_OCF        0x03
#define HCI_EXIT_PERIODIC_INQUIRY_OCF   0x04
#define HCI_CREATE_CONN_OCF             0x05
#define HCI_DISCONN_OCF                 0x06
#define HCI_CREATE_CONN_CANCEL_OCF      0x08
#define HCI_ACCEPT_CONN_REQ_OCF         0x09
#define HCI_REJECT_CONN_REQ_OCF         0x0A
#define HCI_LINK_KEY_REQ_REP_OCF        0x0B
#define HCI_LINK_KEY_REQ_NEG_REP_OCF    0x0C
#define HCI_PIN_CODE_REQ_REP_OCF        0x0D
#define HCI_PIN_CODE_REQ_NEG_REP_OCF    0x0E
#define HCI_CHANGE_CONN_PACKET_TYPE_OCF 0x0F
#define HCI_AUTH_REQUESTED_OCF          0x11
#define HCI_SET_CONN_ENCRYPT_OCF        0x13
#define HCI_R_REMOTE_NAME_OCF           0x19
#define HCI_R_REMOTE_FEATURES_OCF       0x1B
#define HCI_R_REMOTE_VERSION_INFO_OCF   0x1D
#define HCI_R_CLOCK_OFFSET_OCF          0x1F

/* Link Policy commands */
#define HCI_HOLD_MODE_OCF       0x01
#define HCI_SNIFF_MODE_OCF      0x03
#define HCI_EXIT_SNIFF_MODE_OCF 0x04
#define HCI_PARK_MODE_OCF       0x05
#define HCI_EXIT_PARK_MODE_OCF  0x06
#define HCI_R_LINK_POLICY_OCF   0x0C
#define HCI_W_LINK_POLICY_OCF   0x0D

/* Host-Controller and Baseband Commands */
#define HCI_SET_EV_MASK_OCF         0x01
#define HCI_RESET_OCF               0x03
#define HCI_SET_EV_FILTER_OCF       0x05
#define HCI_R_PIN_TYPE_OCF          0x09
#define HCI_W_PIN_TYPE_OCF          0x0A
#define HCI_R_STORED_LINK_KEY_OCF   0x0D
#define HCI_W_STORED_LINK_KEY_OCF   0x11
#define HCI_D_STORED_LINK_KEY_OCF   0x12
#define HCI_W_LOCAL_NAME_OCF        0x13
#define HCI_R_LOCAL_NAME_OCF        0x14
#define HCI_R_PAGE_TIMEOUT_OCF      0x17
#define HCI_W_PAGE_TIMEOUT_OCF      0x18
#define HCI_R_SCAN_EN_OCF           0x19
#define HCI_W_SCAN_EN_OCF           0x1A
#define HCI_R_AUTH_ENABLE_OCF       0x1F
#define HCI_W_AUTH_ENABLE_OCF       0x20
#define HCI_R_COD_OCF               0x23
#define HCI_W_COD_OCF               0x24
#define HCI_R_FLUSHTO_OCF           0x27
#define HCI_W_FLUSHTO_OCF           0x28
#define HCI_SET_HC_TO_H_FC_OCF      0x31
#define HCI_HOST_BUF_SIZE_OCF       0x33
#define HCI_HOST_NUM_COMPL_OCF      0x35
#define HCI_R_LINK_SV_TIMEOUT_OCF   0x36
#define HCI_W_LINK_SV_TIMEOUT_OCF   0x37
#define HCI_R_CUR_IACLAP_OCF        0x39
#define HCI_W_CUR_IACLAP_OCF        0x3A
#define HCI_R_INQUIRY_SCAN_TYPE_OCF 0x42
#define HCI_W_INQUIRY_SCAN_TYPE_OCF 0x43
#define HCI_R_INQUIRY_MODE_OCF      0x44
#define HCI_W_INQUIRY_MODE_OCF      0x45
#define HCI_R_PAGE_SCAN_TYPE_OCF    0x46
#define HCI_W_PAGE_SCAN_TYPE_OCF    0x47

/* Informational Parameters */
#define HCI_R_LOC_VERS_INFO_OCF 0x01
#define HCI_R_LOC_FEAT_OCF      0x03
#define HCI_R_BUF_SIZE_OCF      0x05
#define HCI_R_BD_ADDR_OCF       0x09

/* Status Parameters */
#define HCI_READ_FAILED_CONTACT_COUNTER  0x01
#define HCI_RESET_FAILED_CONTACT_COUNTER 0x02
#define HCI_GET_LINK_QUALITY             0x03
#define HCI_READ_RSSI                    0x05

/* Testing Commands */

/* Vendor-Specific Commands*/
#define HCI_VENDOR_PATCH_START_OCF 0x4F
#define HCI_VENDOR_PATCH_CONT_OCF  0x4C
#define HCI_VENDOR_PATCH_END_OCF   0x4F

/* Command packet length (including ACL header)*/

/* Link Control Commands */
#define HCI_INQUIRY_PLEN               8
#define HCI_INQUIRY_CANCEL_PLEN        3
#define HCI_PERIODIC_INQUIRY_PLEN      12
#define HCI_EXIT_PERIODIC_INQUIRY_PLEN 3
#define HCI_CREATE_CONN_PLEN           16
#define HCI_DISCONN_PLEN               6
#define HCI_CREATE_CONN_CANCEL_PLEN    9
#define HCI_ACCEPT_CONN_REQ_PLEN       10
#define HCI_REJECT_CONN_REQ_PLEN       10
#define HCI_LINK_KEY_REQ_REP_PLEN      25
#define HCI_LINK_KEY_REQ_NEG_REP_PLEN  9
#define HCI_PIN_CODE_REQ_REP_PLEN      26
#define HCI_PIN_CODE_REQ_NEG_REP_PLEN  9
#define HCI_AUTH_REQUESTED_PLEN        5
#define HCI_SET_CONN_ENCRYPT_PLEN      6
#define HCI_R_REMOTE_NAME_PLEN         13
#define HCI_R_REMOTE_FEATURES_PLEN     5
#define HCI_R_REMOTE_VERSION_INFO_PLEN 5
#define HCI_R_CLOCK_OFFSET_PLEN        5

/* Link Policy Commands */
#define HCI_SNIFF_MODE_PLEN    13
#define HCI_R_LINK_POLICY_PLEN 5
#define HCI_W_LINK_POLICY_PLEN 7

/* Host-Controller and Baseband Commands */
#define HCI_SET_EV_MASK_PLEN         11
#define HCI_RESET_PLEN               3
#define HCI_SET_EV_FILTER_PLEN       4
#define HCI_R_PIN_TYPE_PLEN          3
#define HCI_W_PIN_TYPE_PLEN          4
#define HCI_R_STORED_LINK_KEY_PLEN   10
#define HCI_W_STORED_LINK_KEY_PLEN   26
#define HCI_D_STORED_LINK_KEY_PLEN   10
#define HCI_W_LOCAL_NAME_PLEN        251
#define HCI_R_LOCAL_NAME_PLEN        3
#define HCI_R_PAGE_TIMEOUT_PLEN      3
#define HCI_W_PAGE_TIMEOUT_PLEN      5
#define HCI_R_SCAN_EN_PLEN           3
#define HCI_W_SCAN_EN_PLEN           4
#define HCI_R_AUTH_ENABLE_PLEN       3
#define HCI_W_AUTH_ENABLE_PLEN       4
#define HCI_R_COD_PLEN               3
#define HCI_W_COD_PLEN               6
#define HCI_R_FLUSHTO_PLEN           5
#define HCI_W_FLUSHTO_PLEN           7
#define HCI_R_LINK_SV_TIMEOUT_PLEN   5
#define HCI_W_LINK_SV_TIMEOUT_PLEN   7
#define HCI_SET_HC_TO_H_FC_PLEN      4
#define HCI_HOST_BUF_SIZE_PLEN       10
#define HCI_H_NUM_COMPL_PLEN         8
#define HCI_R_CUR_IACLAP_PLEN        3
#define HCI_W_CUR_IACLAP_PLEN        4
#define HCI_W_INQUIRY_SCAN_TYPE_PLEN 4
#define HCI_W_INQUIRY_MODE_PLEN      4
#define HCI_W_PAGE_SCAN_TYPE_PLEN    4

/* Informational Parameters */
#define HCI_R_LOC_VERS_INFO_PLEN 3
#define HCI_R_LOC_FEAT_PLEN      3
#define HCI_R_BUF_SIZE_PLEN      3
#define HCI_R_BD_ADDR_PLEN       3

/* Testing Commands */

/* Vendor-Specific Commands*/
#define HCI_W_VENDOR_CMD_PLEN 3

/* Possible event codes */
#define HCI_INQUIRY_COMPLETE                  0x01
#define HCI_INQUIRY_RESULT                    0x02
#define HCI_CONNECTION_COMPLETE               0x03
#define HCI_CONNECTION_REQUEST                0x04
#define HCI_DISCONNECTION_COMPLETE            0x05
#define HCI_AUTH_COMPLETE                     0x06
#define HCI_REMOTE_NAME_REQ_COMPLETE          0x07
#define HCI_ENCRYPTION_CHANGE                 0x08
#define HCI_CHANGE_CONN_LINK_KEY_COMPLETE     0x09
#define HCI_MASTER_LINK_KEY_COMPLETE          0x0A
#define HCI_READ_REMOTE_FEATURES_COMPLETE     0x0B
#define HCI_READ_REMOTE_VERSION_COMPLETE      0x0C
#define HCI_QOS_SETUP_COMPLETE                0x0D
#define HCI_COMMAND_COMPLETE                  0x0E
#define HCI_COMMAND_STATUS                    0x0F
#define HCI_HARDWARE_ERROR                    0x10
#define HCI_FLUSH_OCCURRED                    0x11
#define HCI_ROLE_CHANGE                       0x12
#define HCI_NBR_OF_COMPLETED_PACKETS          0x13
#define HCI_MODE_CHANGE                       0x14
#define HCI_RETURN_LINK_KEYS                  0x15
#define HCI_PIN_CODE_REQUEST                  0x16
#define HCI_LINK_KEY_REQUEST                  0x17
#define HCI_LINK_KEY_NOTIFICATION             0x18
#define HCI_LOOPBACK_COMMAND                  0x19
#define HCI_DATA_BUFFER_OVERFLOW              0x1A
#define HCI_MAX_SLOTS_CHANGE                  0x1B
#define HCI_READ_CLOCK_OFFSET_COMPLETE        0x1C
#define HCI_CONN_PTYPE_CHANGED                0x1D
#define HCI_QOS_VIOLATION                     0x1E
#define HCI_PSCAN_REP_MODE_CHANGE             0x20
#define HCI_FLOW_SPEC_COMPLETE                0x21
#define HCI_INQUIRY_RESULT_WITH_RSSI          0x22
#define HCI_READ_REMOTE_EXT_FEATURES_COMPLETE 0x23
#define HCI_SYNC_CONN_COMPLETE                0x2C
#define HCI_SYNC_CONN_CHANGED                 0x2D
#define HCI_SNIFF_SUBRATING                   0x2E
#define HCI_EXTENDED_INQUIRY_RESULT           0x2F
#define HCI_ENCRYPTION_KEY_REFRESH_COMPLETE   0x30
#define HCI_IO_CAPABILITY_REQUEST             0x31
#define HCI_IO_CAPABILITY_RESPONSE            0x32
#define HCI_USER_CONFIRM_REQUEST              0x33
#define HCI_USER_PASSKEY_REQUEST              0x34
#define HCI_REMOTE_OOB_DATA_REQUEST           0x35
#define HCI_SIMPLE_PAIRING_COMPLETE           0x36
#define HCI_LINK_SUPERVISION_TIMEOUT_CHANGED  0x38
#define HCI_ENHANCED_FLUSH_COMPLETE           0x39
#define HCI_USER_PASSKEY_NOTIFY               0x3B
#define HCI_KEYPRESS_NOTIFY                   0x3C
#define HCI_REMOTE_HOST_FEATURES_NOTIFY       0x3D
#define HCI_VENDOR_SPECIFIC_EVENT             0xFF

/* Vendor specific event codes */
#define HCI_VENDOR_BEGIN_PAIRING        0x08
#define HCI_VENDOR_CLEAR_PAIRED_DEVICES 0x09

#ifdef __cplusplus
}
#endif

#endif /* BTE_HCI_PRIV_H */
