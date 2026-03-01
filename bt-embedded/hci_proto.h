#ifndef BTE_HCI_PROTO_H
#define BTE_HCI_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

/* HCI packet types */
#define BTE_HCI_DM1 0x0008
#define BTE_HCI_DM3 0x0400
#define BTE_HCI_DM5 0x4000
#define BTE_HCI_DH1 0x0010
#define BTE_HCI_DH3 0x0800
#define BTE_HCI_DH5 0x8000

#define BTE_HCI_HV1 0x0020
#define BTE_HCI_HV2 0x0040
#define BTE_HCI_HV3 0x0080

#define SCO_PTYPE_MASK (BTE_HCI_HV1 | BTE_HCI_HV2 | BTE_HCI_HV3)
#define ACL_PTYPE_MASK (~SCO_PTYPE_MASK)

/* Success code */
#define BTE_HCI_SUCCESS 0x00
/* Possible error codes */
#define BTE_HCI_UNKNOWN_BTE_HCI_COMMAND              0x01
#define BTE_HCI_NO_CONNECTION                        0x02
#define BTE_HCI_HW_FAILURE                           0x03
#define BTE_HCI_PAGE_TIMEOUT                         0x04
#define BTE_HCI_AUTHENTICATION_FAILURE               0x05
#define BTE_HCI_KEY_MISSING                          0x06
#define BTE_HCI_MEMORY_FULL                          0x07
#define BTE_HCI_CONN_TIMEOUT                         0x08
#define BTE_HCI_MAX_NUMBER_OF_CONNECTIONS            0x09
#define BTE_HCI_MAX_NUMBER_OF_SCO_CONNECTIONS        0x0A
#define BTE_HCI_ACL_CONNECTION_EXISTS                0x0B
#define BTE_HCI_COMMAND_DISSALLOWED                  0x0C
#define BTE_HCI_HOST_REJECTED_RESOURCES              0x0D
#define BTE_HCI_HOST_REJECTED_SECURITY               0x0E
#define BTE_HCI_HOST_REJECTED_BD_ADDR                0x0F
#define BTE_HCI_HOST_TIMEOUT                         0x10
#define BTE_HCI_UNSUPPORTED_FEAT_OR_PARAM            0x11
#define BTE_HCI_INVALID_HCI_COMMAND_PARAMS           0x12
#define BTE_HCI_OTHER_END_CLOSED_CONN_USER           0x13
#define BTE_HCI_OTHER_END_CLOSED_CONN_RESOURCES      0x14
#define BTE_HCI_OTHER_END_CLOSED_CONN_POWER_OFF      0x15
#define BTE_HCI_CONN_TERMINATED_BY_LOCAL_HOST        0x16
#define BTE_HCI_REPEATED_ATTEMPTS                    0x17
#define BTE_HCI_PAIRING_NOT_ALLOWED                  0x18
#define BTE_HCI_UNKNOWN_LMP_PDU                      0x19
#define BTE_HCI_UNSUPPORTED_REMOTE_FEATURE           0x1A
#define BTE_HCI_SCO_OFFSET_REJECTED                  0x1B
#define BTE_HCI_SCO_INTERVAL_REJECTED                0x1C
#define BTE_HCI_SCO_AIR_MODE_REJECTED                0x1D
#define BTE_HCI_INVALID_LMP_PARAMETERS               0x1E
#define BTE_HCI_UNSPECIFIED_ERROR                    0x1F
#define BTE_HCI_UNSUPPORTED_LMP_PARAM                0x20
#define BTE_HCI_ROLE_CHANGE_NOT_ALLOWED              0x21
#define BTE_HCI_LMP_RESPONSE_TIMEOUT                 0x22
#define BTE_HCI_LMP_ERROR_TRANSACTION_COLLISION      0x23
#define BTE_HCI_LMP_PDU_NOT_ALLOWED                  0x24
#define BTE_HCI_ENCRYPTION_MODE_NOT_ACCEPTABLE       0x25
#define BTE_HCI_UNIT_KEY_USED                        0x26
#define BTE_HCI_QOS_NOT_SUPPORTED                    0x27
#define BTE_HCI_INSTANT_PASSED                       0x28
#define BTE_HCI_PAIRING_UNIT_KEY_NOT_SUPPORTED       0x29

/* HCI Link manager feature masks */

#define BTE_HCI_FEAT_3_SLOT_PACKETS               ((uint64_t)(1 << 0))
#define BTE_HCI_FEAT_5_SLOT_PACKETS               ((uint64_t)(1 << 1))
#define BTE_HCI_FEAT_ENCRYPTION                   ((uint64_t)(1 << 2))
#define BTE_HCI_FEAT_SLOT_OFFSET                  ((uint64_t)(1 << 3))
#define BTE_HCI_FEAT_TIMING_ACCURACY              ((uint64_t)(1 << 4))
#define BTE_HCI_FEAT_ROLE_SWITCH                  ((uint64_t)(1 << 5))
#define BTE_HCI_FEAT_HOLD_MODE                    ((uint64_t)(1 << 6))
#define BTE_HCI_FEAT_SNIFF_MODE                   ((uint64_t)(1 << 7))
#define BTE_HCI_FEAT_PARK_STATE                   ((uint64_t)(1 << 8))
#define BTE_HCI_FEAT_POWER_CONTROL_REQUESTS       ((uint64_t)(1 << 9))
#define BTE_HCI_FEAT_CQDDR                        ((uint64_t)(1 << 10))
#define BTE_HCI_FEAT_SCO_LINK                     ((uint64_t)(1 << 11))
#define BTE_HCI_FEAT_HV2_PACKETS                  ((uint64_t)(1 << 12))
#define BTE_HCI_FEAT_HV3_PACKETS                  ((uint64_t)(1 << 13))
#define BTE_HCI_FEAT_Μ_LAW_LOG_SYNCHRONOUS_DATA   ((uint64_t)(1 << 14))
#define BTE_HCI_FEAT_A_LAW_LOG_SYNCHRONOUS_DATA   ((uint64_t)(1 << 15))
#define BTE_HCI_FEAT_CVSD_SYNCHRONOUS_DATA        ((uint64_t)(1 << 16))
#define BTE_HCI_FEAT_PAGING_PARAMETER_NEGOTIATION ((uint64_t)(1 << 17))
#define BTE_HCI_FEAT_POWER_CONTROL                ((uint64_t)(1 << 18))
#define BTE_HCI_FEAT_TRANSPARENT_SYNCHRONOUS_DATA ((uint64_t)(1 << 19))
#define BTE_HCI_FEAT_FLOW_CONTROL_LAG_LSB         ((uint64_t)(1 << 20))
#define BTE_HCI_FEAT_FLOW_CONTROL_LAG_MID         ((uint64_t)(1 << 21))
#define BTE_HCI_FEAT_FLOW_CONTROL_LAG_MSB         ((uint64_t)(1 << 22))
#define BTE_HCI_FEAT_BROADCAST_ENCRYPTION         ((uint64_t)(1 << 23))
#define BTE_HCI_FEAT_ENHANCED_INQUIRY_SCAN        ((uint64_t)(1 << 27))
#define BTE_HCI_FEAT_INTERLACED_INQUIRY_SCAN      ((uint64_t)(1 << 28))
#define BTE_HCI_FEAT_INTERLACED_PAGE_SCAN         ((uint64_t)(1 << 29))
#define BTE_HCI_FEAT_RSSI_WITH_INQUIRY_RESULTS    ((uint64_t)(1 << 30))
#define BTE_HCI_FEAT_EXTENDED_SCO_LINK            ((uint64_t)(1 << 31))
#define BTE_HCI_FEAT_EV4_PACKETS                  ((uint64_t)(1 << 32))
#define BTE_HCI_FEAT_EV5_PACKETS                  ((uint64_t)(1 << 33))
#define BTE_HCI_FEAT_AFH_CAPABLE_SLAVE            ((uint64_t)(1 << 35))
#define BTE_HCI_FEAT_AFH_CLASSIFICATION_SLAVE     ((uint64_t)(1 << 36))
#define BTE_HCI_FEAT_AFH_CAPABLE_MASTER           ((uint64_t)(1 << 43))
#define BTE_HCI_FEAT_AFH_CLASSIFICATION_MASTER    ((uint64_t)(1 << 44))
#define BTE_HCI_FEAT_EXTENDED_FEATURES            ((uint64_t)(1 << 63))

/* Controller event mask */
#define BTE_HCI_EVENT_INQUIRY_COMPL                  ((uint64_t)(1 << 0))
#define BTE_HCI_EVENT_INQUIRY_RESULT                 ((uint64_t)(1 << 1))
#define BTE_HCI_EVENT_CONN_COMPL                     ((uint64_t)(1 << 2))
#define BTE_HCI_EVENT_CONN_REQUEST                   ((uint64_t)(1 << 3))
#define BTE_HCI_EVENT_DISCONN_COMPL                  ((uint64_t)(1 << 4))
#define BTE_HCI_EVENT_AUTHENTICATION_COMPL           ((uint64_t)(1 << 5))
#define BTE_HCI_EVENT_REMOTE_NAME_REQUEST_COMPL      ((uint64_t)(1 << 6))
#define BTE_HCI_EVENT_ENCRYPTION_CHANGE              ((uint64_t)(1 << 7))
#define BTE_HCI_EVENT_CHANGE_CONN_LINK_KEY_COMPL     ((uint64_t)(1 << 8))
#define BTE_HCI_EVENT_MASTER_LINK_KEY_COMPL          ((uint64_t)(1 << 9))
#define BTE_HCI_EVENT_READ_REMOTE_FEATURES_COMPL     ((uint64_t)(1 << 10))
#define BTE_HCI_EVENT_READ_REMOTE_VERS_INFO_COMPL    ((uint64_t)(1 << 11))
#define BTE_HCI_EVENT_QOS_SETUP_COMPL                ((uint64_t)(1 << 12))
#define BTE_HCI_EVENT_HARDWARE_ERROR                 ((uint64_t)(1 << 15))
#define BTE_HCI_EVENT_FLUSH_OCCURRED                 ((uint64_t)(1 << 16))
#define BTE_HCI_EVENT_ROLE_CHANGE                    ((uint64_t)(1 << 17))
#define BTE_HCI_EVENT_MODE_CHANGE                    ((uint64_t)(1 << 19))
#define BTE_HCI_EVENT_RETURN_LINK_KEYS               ((uint64_t)(1 << 20))
#define BTE_HCI_EVENT_PIN_CODE_REQUEST               ((uint64_t)(1 << 21))
#define BTE_HCI_EVENT_LINK_KEY_REQUEST               ((uint64_t)(1 << 22))
#define BTE_HCI_EVENT_LINK_KEY_NOTIFICATION          ((uint64_t)(1 << 23))
#define BTE_HCI_EVENT_LOOPBACK_COMMAND               ((uint64_t)(1 << 24))
#define BTE_HCI_EVENT_DATA_BUFFER_OVERFLOW           ((uint64_t)(1 << 25))
#define BTE_HCI_EVENT_MAX_SLOTS_CHANGE               ((uint64_t)(1 << 26))
#define BTE_HCI_EVENT_READ_CLOCK_OFFSET_COMPL        ((uint64_t)(1 << 27))
#define BTE_HCI_EVENT_CONN_PACKET_TYPE_CHANGED       ((uint64_t)(1 << 28))
#define BTE_HCI_EVENT_QOS_VIOLATION                  ((uint64_t)(1 << 29))
#define BTE_HCI_EVENT_PAGE_SCAN_MODE_CHANGE          ((uint64_t)(1 << 30))
#define BTE_HCI_EVENT_PAGE_SCAN_REP_MODE_CHANGE      ((uint64_t)(1 << 31))
#define BTE_HCI_EVENT_FLOW_SPEC_COMPL                ((uint64_t)(1 << 32))
#define BTE_HCI_EVENT_INQUIRY_RESULT_WITH_RSSI       ((uint64_t)(1 << 33))
#define BTE_HCI_EVENT_READ_REMOTE_EXT_FEATURES_COMPL ((uint64_t)(1 << 34))
#define BTE_HCI_EVENT_SYNC_CONN_COMPL                ((uint64_t)(1 << 43))
#define BTE_HCI_EVENT_SYNC_CONN_CHANGED              ((uint64_t)(1 << 44))
#define BTE_HCI_EVENT_ALL                            ((uint64_t)0x1fffffffffff)

#define BTE_HCI_MAX_NAME_LEN 248

#define BTE_LAP_GIAC 0x009E8B33
#define BTE_LAP_LIAC 0x009E8B00

#ifdef __cplusplus
}
#endif

#endif /* BTE_HCI_PROTO_H */
