#ifndef BTE_HCI_PROTO_H
#define BTE_HCI_PROTO_H

/* HCI packet indicators */
#define HCI_COMMAND_DATA_PACKET 0x01
#define HCI_ACL_DATA_PACKET     0x02
#define HCI_SCO_DATA_PACKET     0x03
#define HCI_EVENT_PACKET        0x04
#define HCI_VENDOR_PACKET       0xFF

/* HCI packet types */
#define HCI_DM1 0x0008
#define HCI_DM3 0x0400
#define HCI_DM5 0x4000
#define HCI_DH1 0x0010
#define HCI_DH3 0x0800
#define HCI_DH5 0x8000

#define HCI_HV1 0x0020
#define HCI_HV2 0x0040
#define HCI_HV3 0x0080

#define SCO_PTYPE_MASK (HCI_HV1 | HCI_HV2 | HCI_HV3)
#define ACL_PTYPE_MASK (~SCO_PTYPE_MASK)

#define HCI_EVENT_HDR_LEN 2
#define HCI_ACL_HDR_LEN   4
#define HCI_SCO_HDR_LEN   3
#define HCI_CMD_HDR_LEN   3

/* Specification defined parameters */
#define HCI_BD_ADDR_LEN  6
#define HCI_LINK_KEY_LEN 16
#define HCI_LMP_FEAT_LEN 8

/* LMP features */
#define LMP_3SLOT     0x01
#define LMP_5SLOT     0x02
#define LMP_ENCRYPT   0x04
#define LMP_SOFFSET   0x08
#define LMP_TACCURACY 0x10
#define LMP_RSWITCH   0x20
#define LMP_HOLD      0x40
#define LMP_SNIFF     0x80

#define LMP_PARK    0x01
#define LMP_RSSI    0x02
#define LMP_QUALITY 0x04
#define LMP_SCO     0x08
#define LMP_HV2     0x10
#define LMP_HV3     0x20
#define LMP_ULAW    0x40
#define LMP_ALAW    0x80

#define LMP_CVSD     0x01
#define LMP_PSCHEME  0x02
#define LMP_PCONTROL 0x04

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
#define HCI_W_LINK_POLICY_OCF   0x0D

/* Host-Controller and Baseband Commands */
#define HCI_SET_EV_MASK_OCF         0x01
#define HCI_RESET_OCF               0x03
#define HCI_SET_EV_FILTER_OCF       0x05
#define HCI_W_PIN_TYPE_OCF          0x0A
#define HCI_R_STORED_LINK_KEY_OCF   0x0D
#define HCI_W_STORED_LINK_KEY_OCF   0x11
#define HCI_D_STORED_LINK_KEY_OCF   0x12
#define HCI_W_LOCAL_NAME_OCF        0x13
#define HCI_R_LOCAL_NAME_OCF        0x14
#define HCI_W_PAGE_TIMEOUT_OCF      0x18
#define HCI_W_SCAN_EN_OCF           0x1A
#define HCI_W_AUTH_ENABLE_OCF       0x20
#define HCI_R_COD_OCF               0x23
#define HCI_W_COD_OCF               0x24
#define HCI_W_FLUSHTO_OCF           0x28
#define HCI_SET_HC_TO_H_FC_OCF      0x31
#define HCI_HOST_BUF_SIZE_OCF       0x33
#define HCI_HOST_NUM_COMPL_OCF      0x35
#define HCI_W_LINK_SUP_TIMEOUT_OCF  0x37
#define HCI_R_CUR_IACLAP_OCF        0x39
#define HCI_W_INQUIRY_SCAN_TYPE_OCF 0x43
#define HCI_W_INQUIRY_MODE_OCF      0x45
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
#define HCI_W_LINK_POLICY_PLEN 7

/* Host-Controller and Baseband Commands */
#define HCI_SET_EV_MASK_PLEN         11
#define HCI_RESET_PLEN               3
#define HCI_SET_EV_FILTER_PLEN       5
#define HCI_W_PIN_TYPE_PLEN          4
#define HCI_R_STORED_LINK_KEY_PLEN   10
#define HCI_W_STORED_LINK_KEY_PLEN   26
#define HCI_D_STORED_LINK_KEY_PLEN   10
#define HCI_W_LOCAL_NAME_PLEN        251
#define HCI_R_LOCAL_NAME_PLEN        3
#define HCI_W_PAGE_TIMEOUT_PLEN      5
#define HCI_W_SCAN_EN_PLEN           4
#define HCI_W_AUTH_ENABLE_PLEN       4
#define HCI_R_COD_PLEN               3
#define HCI_W_COD_PLEN               6
#define HCI_W_FLUSHTO_PLEN           7
#define HCI_SET_HC_TO_H_FC_PLEN      4
#define HCI_HOST_BUF_SIZE_PLEN       10
#define HCI_H_NUM_COMPL_PLEN         8
#define HCI_R_CUR_IACLAP_PLEN        3
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

/* Success code */
#define HCI_SUCCESS 0x00
/* Possible error codes */
#define HCI_UNKNOWN_HCI_COMMAND                                      0x01
#define HCI_NO_CONNECTION                                            0x02
#define HCI_HW_FAILURE                                               0x03
#define HCI_PAGE_TIMEOUT                                             0x04
#define HCI_AUTHENTICATION_FAILURE                                   0x05
#define HCI_KEY_MISSING                                              0x06
#define HCI_MEMORY_FULL                                              0x07
#define HCI_CONN_TIMEOUT                                             0x08
#define HCI_MAX_NUMBER_OF_CONNECTIONS                                0x09
#define HCI_MAX_NUMBER_OF_SCO_CONNECTIONS_TO_DEVICE                  0x0A
#define HCI_ACL_CONNECTION_EXISTS                                    0x0B
#define HCI_COMMAND_DISSALLOWED                                      0x0C
#define HCI_HOST_REJECTED_DUE_TO_LIMITED_RESOURCES                   0x0D
#define HCI_HOST_REJECTED_DUE_TO_SECURITY_REASONS                    0x0E
#define HCI_HOST_REJECTED_DUE_TO_REMOTE_DEVICE_ONLY_PERSONAL_SERVICE 0x0F
#define HCI_HOST_TIMEOUT                                             0x10
#define HCI_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE                   0x11
#define HCI_INVALID_HCI_COMMAND_PARAMETERS                           0x12
#define HCI_OTHER_END_TERMINATED_CONN_USER_ENDED                     0x13
#define HCI_OTHER_END_TERMINATED_CONN_LOW_RESOURCES                  0x14
#define HCI_OTHER_END_TERMINATED_CONN_ABOUT_TO_POWER_OFF             0x15
#define HCI_CONN_TERMINATED_BY_LOCAL_HOST                            0x16
#define HCI_REPEATED_ATTEMPTS                                        0x17
#define HCI_PAIRING_NOT_ALLOWED                                      0x18
#define HCI_UNKNOWN_LMP_PDU                                          0x19
#define HCI_UNSUPPORTED_REMOTE_FEATURE                               0x1A
#define HCI_SCO_OFFSET_REJECTED                                      0x1B
#define HCI_SCO_INTERVAL_REJECTED                                    0x1C
#define HCI_SCO_AIR_MODE_REJECTED                                    0x1D
#define HCI_INVALID_LMP_PARAMETERS                                   0x1E
#define HCI_UNSPECIFIED_ERROR                                        0x1F
#define HCI_UNSUPPORTED_LMP_PARAMETER_VALUE                          0x20
#define HCI_ROLE_CHANGE_NOT_ALLOWED                                  0x21
#define HCI_LMP_RESPONSE_TIMEOUT                                     0x22
#define HCI_LMP_ERROR_TRANSACTION_COLLISION                          0x23
#define HCI_LMP_PDU_NOT_ALLOWED                                      0x24
#define HCI_ENCRYPTION_MODE_NOT_ACCEPTABLE                           0x25
#define HCI_UNIT_KEY_USED                                            0x26
#define HCI_QOS_NOT_SUPPORTED                                        0x27
#define HCI_INSTANT_PASSED                                           0x28
#define HCI_PAIRING_UNIT_KEY_NOT_SUPPORTED                           0x29

/* HCI Link manager feature masks */

#define HCI_FEAT_3_SLOT_PACKETS               ((uint64_t)(1 << 0))
#define HCI_FEAT_5_SLOT_PACKETS               ((uint64_t)(1 << 1))
#define HCI_FEAT_ENCRYPTION                   ((uint64_t)(1 << 2))
#define HCI_FEAT_SLOT_OFFSET                  ((uint64_t)(1 << 3))
#define HCI_FEAT_TIMING_ACCURACY              ((uint64_t)(1 << 4))
#define HCI_FEAT_ROLE_SWITCH                  ((uint64_t)(1 << 5))
#define HCI_FEAT_HOLD_MODE                    ((uint64_t)(1 << 6))
#define HCI_FEAT_SNIFF_MODE                   ((uint64_t)(1 << 7))
#define HCI_FEAT_PARK_STATE                   ((uint64_t)(1 << 8))
#define HCI_FEAT_POWER_CONTROL_REQUESTS       ((uint64_t)(1 << 9))
#define HCI_FEAT_CQDDR                        ((uint64_t)(1 << 10))
#define HCI_FEAT_SCO_LINK                     ((uint64_t)(1 << 11))
#define HCI_FEAT_HV2_PACKETS                  ((uint64_t)(1 << 12))
#define HCI_FEAT_HV3_PACKETS                  ((uint64_t)(1 << 13))
#define HCI_FEAT_Μ_LAW_LOG_SYNCHRONOUS_DATA   ((uint64_t)(1 << 14))
#define HCI_FEAT_A_LAW_LOG_SYNCHRONOUS_DATA   ((uint64_t)(1 << 15))
#define HCI_FEAT_CVSD_SYNCHRONOUS_DATA        ((uint64_t)(1 << 16))
#define HCI_FEAT_PAGING_PARAMETER_NEGOTIATION ((uint64_t)(1 << 17))
#define HCI_FEAT_POWER_CONTROL                ((uint64_t)(1 << 18))
#define HCI_FEAT_TRANSPARENT_SYNCHRONOUS_DATA ((uint64_t)(1 << 19))
#define HCI_FEAT_FLOW_CONTROL_LAG_LSB         ((uint64_t)(1 << 20))
#define HCI_FEAT_FLOW_CONTROL_LAG_MID         ((uint64_t)(1 << 21))
#define HCI_FEAT_FLOW_CONTROL_LAG_MSB         ((uint64_t)(1 << 22))
#define HCI_FEAT_BROADCAST_ENCRYPTION         ((uint64_t)(1 << 23))
#define HCI_FEAT_ENHANCED_INQUIRY_SCAN        ((uint64_t)(1 << 27))
#define HCI_FEAT_INTERLACED_INQUIRY_SCAN      ((uint64_t)(1 << 28))
#define HCI_FEAT_INTERLACED_PAGE_SCAN         ((uint64_t)(1 << 29))
#define HCI_FEAT_RSSI_WITH_INQUIRY_RESULTS    ((uint64_t)(1 << 30))
#define HCI_FEAT_EXTENDED_SCO_LINK            ((uint64_t)(1 << 31))
#define HCI_FEAT_EV4_PACKETS                  ((uint64_t)(1 << 32))
#define HCI_FEAT_EV5_PACKETS                  ((uint64_t)(1 << 33))
#define HCI_FEAT_AFH_CAPABLE_SLAVE            ((uint64_t)(1 << 35))
#define HCI_FEAT_AFH_CLASSIFICATION_SLAVE     ((uint64_t)(1 << 36))
#define HCI_FEAT_AFH_CAPABLE_MASTER           ((uint64_t)(1 << 43))
#define HCI_FEAT_AFH_CLASSIFICATION_MASTER    ((uint64_t)(1 << 44))
#define HCI_FEAT_EXTENDED_FEATURES            ((uint64_t)(1 << 63))

/* Controller event mask */
#define HCI_EVENT_INQUIRY_COMPL                  ((uint64_t)(1 << 0))
#define HCI_EVENT_INQUIRY_RESULT                 ((uint64_t)(1 << 1))
#define HCI_EVENT_CONN_COMPL                     ((uint64_t)(1 << 2))
#define HCI_EVENT_CONN_REQUEST                   ((uint64_t)(1 << 3))
#define HCI_EVENT_DISCONN_COMPL                  ((uint64_t)(1 << 4))
#define HCI_EVENT_AUTHENTICATION_COMPL           ((uint64_t)(1 << 5))
#define HCI_EVENT_REMOTE_NAME_REQUEST_COMPL      ((uint64_t)(1 << 6))
#define HCI_EVENT_ENCRYPTION_CHANGE              ((uint64_t)(1 << 7))
#define HCI_EVENT_CHANGE_CONN_LINK_KEY_COMPL     ((uint64_t)(1 << 8))
#define HCI_EVENT_MASTER_LINK_KEY_COMPL          ((uint64_t)(1 << 9))
#define HCI_EVENT_READ_REMOTE_FEATURES_COMPL     ((uint64_t)(1 << 10))
#define HCI_EVENT_READ_REMOTE_VERS_INFO_COMPL    ((uint64_t)(1 << 11))
#define HCI_EVENT_QOS_SETUP_COMPL                ((uint64_t)(1 << 12))
#define HCI_EVENT_HARDWARE_ERROR                 ((uint64_t)(1 << 15))
#define HCI_EVENT_FLUSH_OCCURRED                 ((uint64_t)(1 << 16))
#define HCI_EVENT_ROLE_CHANGE                    ((uint64_t)(1 << 17))
#define HCI_EVENT_MODE_CHANGE                    ((uint64_t)(1 << 19))
#define HCI_EVENT_RETURN_LINK_KEYS               ((uint64_t)(1 << 20))
#define HCI_EVENT_PIN_CODE_REQUEST               ((uint64_t)(1 << 21))
#define HCI_EVENT_LINK_KEY_REQUEST               ((uint64_t)(1 << 22))
#define HCI_EVENT_LINK_KEY_NOTIFICATION          ((uint64_t)(1 << 23))
#define HCI_EVENT_LOOPBACK_COMMAND               ((uint64_t)(1 << 24))
#define HCI_EVENT_DATA_BUFFER_OVERFLOW           ((uint64_t)(1 << 25))
#define HCI_EVENT_MAX_SLOTS_CHANGE               ((uint64_t)(1 << 26))
#define HCI_EVENT_READ_CLOCK_OFFSET_COMPL        ((uint64_t)(1 << 27))
#define HCI_EVENT_CONN_PACKET_TYPE_CHANGED       ((uint64_t)(1 << 28))
#define HCI_EVENT_QOS_VIOLATION                  ((uint64_t)(1 << 29))
#define HCI_EVENT_PAGE_SCAN_MODE_CHANGE          ((uint64_t)(1 << 30))
#define HCI_EVENT_PAGE_SCAN_REP_MODE_CHANGE      ((uint64_t)(1 << 31))
#define HCI_EVENT_FLOW_SPEC_COMPL                ((uint64_t)(1 << 32))
#define HCI_EVENT_INQUIRY_RESULT_WITH_RSSI       ((uint64_t)(1 << 33))
#define HCI_EVENT_READ_REMOTE_EXT_FEATURES_COMPL ((uint64_t)(1 << 34))
#define HCI_EVENT_SYNC_CONN_COMPL                ((uint64_t)(1 << 43))
#define HCI_EVENT_SYNC_CONN_CHANGED              ((uint64_t)(1 << 44))
#define HCI_EVENT_ALL                            ((uint64_t)0x1fffffffffff)

#define HCI_MAX_NAME_LEN 248

#define BTE_LAP_GIAC 0x009E8B33
#define BTE_LAP_LIAC 0x009E8B00

#endif /* BTE_HCI_PROTO_H */
